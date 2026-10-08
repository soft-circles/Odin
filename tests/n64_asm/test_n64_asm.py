#!/usr/bin/env python3
"""SDK-free checks for CPU asm templates on the N64 (MIPS III, o64) targets.

Packages under tests/n64_asm/:
  fixture/       the MIPS_ASM.md examples (cache loops, COP0 counters, pins)
  coverage/      one exported proc per VR4300 mnemonic, checked word by word
  regress/       templates for the MIPS-specific traps (named in each test)
  bad/           the original rejected templates
  bad_check/     every checker rejection, one template per line
  bad_features/  every mnemonic the VR4300 lacks
  bad_backend/   pins and wrong-class literals the LLVM generator also rejects

Every object and IR build runs once per package and target and is cached for
the whole run. Instruction words come from parsing the ELF directly. When
MIPS_O64_OBJDUMP or N64_INST is set, the coverage object is also disassembled
with the SDK's objdump as a cross-check; otherwise that test is skipped.

Instruction words are read from ELF32 objects, which only the MIPS O64 LLVM fork
writes; stock LLVM writes an ELF64 n64-ABI object for -target:n64. With a stock
compiler (quick mode in CI) the object checks skip and the IR, constraint,
checker and feature checks still run. In full mode, or when N64_INST or
MIPS_O64_OBJDUMP is set, the fork is expected and an ELF64 object fails instead.
"""
import functools
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
ODIN = Path(os.environ.get("ODIN", ROOT / "odin")).resolve()
TABLE_SOURCE = ROOT / "src" / "asm_tables_mips.cpp"
TARGETS = ("n64", "freestanding_mips32be")
OBJ_FLAGS = ("-o:speed", "-use-single-module", "-no-entry-point")


def objdump_path():
    explicit = os.environ.get("MIPS_O64_OBJDUMP")
    if explicit:
        return Path(explicit)
    sdk = os.environ.get("N64_INST")
    if sdk and (Path(sdk) / "bin" / "mips64-elf-objdump").exists():
        return Path(sdk) / "bin" / "mips64-elf-objdump"
    return None


# --------------------------------------------------------------------------
# Building
# --------------------------------------------------------------------------

_scratch = None


def scratch_dir():
    global _scratch
    if _scratch is None:
        _scratch = tempfile.TemporaryDirectory(prefix="odin-n64-asm-")
    return Path(_scratch.name)


def tearDownModule():
    if _scratch is not None:
        _scratch.cleanup()


def run_odin(*args):
    """Run the compiler; return (returncode, output with ANSI colours removed)."""
    result = subprocess.run([str(ODIN), *map(str, args)], capture_output=True, text=True)
    return result.returncode, re.sub(r"\x1b\[[0-9;]*m", "", result.stdout + result.stderr)


def unique_dir(stem):
    return Path(tempfile.mkdtemp(prefix=f"{stem}-", dir=scratch_dir()))


@functools.lru_cache(maxsize=None)
def build_object(package, target):
    out = unique_dir(f"{package}-{target}") / "out.o"
    code, output = run_odin("build", HERE / package, f"-target:{target}", "-build-mode:obj",
                            *OBJ_FLAGS, f"-out:{out}")
    if code != 0:
        raise AssertionError(f"building {package} for {target} failed:\n{output}")
    return out


@functools.lru_cache(maxsize=None)
def object_functions(package, target):
    return elf32be_functions(build_object(package, target))


@functools.lru_cache(maxsize=None)
def build_ir(package, target="n64", expect_success=True):
    """(returncode, LLVM IR text, output) for a package; the backend writes `<dir>/<package>.ll`.

    The runtime ROM keeps its entry point: without `main` everything in it is dead.
    """
    directory = unique_dir(f"{package}-{target}-ir")
    flags = [f for f in OBJ_FLAGS if package != "rom" or f != "-no-entry-point"]
    code, output = run_odin("build", HERE / package, f"-target:{target}", "-build-mode:llvm-ir",
                            *flags, f"-out:{directory}")
    if expect_success and code != 0:
        raise AssertionError(f"IR for {package} ({target}) failed:\n{output}")
    ir = "\n".join(p.read_text() for p in directory.glob("*.ll"))
    return code, ir, output


@functools.lru_cache(maxsize=None)
def check_package(package):
    return run_odin("check", HERE / package, "-target:n64", "-no-entry-point", "-max-error-count:1000")


def snippet(source, name):
    """Write a one-file package for a bug reproduction; return its directory."""
    directory = unique_dir(name)
    (directory / "snippet.odin").write_text("package snippet\n\n" + source)
    return directory


NOT_O64_FORK = ("compiler emits ELF64: not the MIPS O64 LLVM fork; "
                "object encodings are checked in full mode / with the fork")


def o64_fork_expected():
    """Full mode and an SDK both come with the O64 fork, so ELF64 output is a failure there."""
    return (os.environ.get("N64_VALIDATION_MODE") == "full"
            or bool(os.environ.get("N64_INST")) or bool(os.environ.get("MIPS_O64_OBJDUMP")))


@functools.lru_cache(maxsize=None)
def emits_elf32_objects():
    """True when the compiler writes ELF32 (O64) objects for -target:n64, as the fork does."""
    directory = snippet('@(export) probe :: proc "contextless" () {}\n', "elf-class-probe")
    out = directory / "out.o"
    code, output = run_odin("build", directory, "-target:n64", "-build-mode:obj", *OBJ_FLAGS, f"-out:{out}")
    if code != 0:
        raise AssertionError(f"building the ELF class probe failed:\n{output}")
    return out.read_bytes()[4] == 1  # EI_CLASS: 1 = ELF32, 2 = ELF64


def require_o64_objects(test):
    """Skip the rest of `test` unless the compiler writes ELF32 objects; fail if the fork is expected.

    Call it after a test's IR assertions, so those still run on stock LLVM.
    """
    if emits_elf32_objects():
        return
    if o64_fork_expected():
        test.fail(f"{NOT_O64_FORK}; N64_VALIDATION_MODE=full, N64_INST or MIPS_O64_OBJDUMP requires the fork")
    test.skipTest(NOT_O64_FORK)


# --------------------------------------------------------------------------
# Parsing
# --------------------------------------------------------------------------

def elf32be_symbols(path):
    """Map each defined FUNC symbol in a big-endian ELF32 relocatable to (offset, words)."""
    data = Path(path).read_bytes()
    assert data[:4] == b"\x7fELF" and data[4] == 1 and data[5] == 2, "expected ELF32 big-endian"
    shoff, = struct.unpack_from(">I", data, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from(">HHH", data, 0x2E)
    sections = [struct.unpack_from(">IIIIIIIIII", data, shoff + i*shentsize) for i in range(shnum)]
    def name(strtab, offset):
        start = sections[strtab][4] + offset
        return data[start:data.index(b"\0", start)].decode()
    functions = {}
    for sh in sections:
        if sh[1] != 2:  # SHT_SYMTAB
            continue
        for i in range(sh[5] // 16):
            st_name, value, size, info, _, shndx = struct.unpack_from(">IIIBBH", data, sh[4] + i*16)
            if info & 0xF != 2 or shndx == 0 or shndx >= shnum:  # STT_FUNC, defined
                continue
            text = sections[shndx][4] + value
            functions[name(sh[6], st_name)] = (value, struct.unpack_from(f">{size // 4}I", data, text))
    return functions


def elf32be_functions(path):
    """Map each FUNC symbol in a big-endian ELF32 relocatable to its .text words."""
    return {name: words for name, (_, words) in elf32be_symbols(path).items()}


ASM_CALL = re.compile(r'asm (sideeffect )?"((?:[^"\\]|\\.)*)", "([^"]*)"')


def asm_calls(ir):
    """[(function, sideeffect, text, constraints)] for every inline-asm call in `ir`."""
    calls, function = [], None
    for line in ir.splitlines():
        m = re.match(r'^define [^@]*@"?([^"(]+)"?\(', line)
        if m:
            function = m.group(1)
            continue
        for m in ASM_CALL.finditer(line):
            calls.append((function, bool(m.group(1)), m.group(2), m.group(3)))
    return calls


def calls_in(ir, function):
    return [(side, text, constraints) for fn, side, text, constraints in asm_calls(ir) if fn == function]


DIAGNOSTIC = re.compile(r"^(.*)\((\d+):(\d+)\) (Error|Warning): (.*?)\s*$")


def diagnostics(output):
    """[(line, message, detail lines)] for every error/warning in compiler output."""
    found = []
    for raw in output.splitlines():
        m = DIAGNOSTIC.match(raw)
        if m:
            found.append((int(m.group(2)), m.group(5), []))
        elif found:
            found[-1][2].append(raw.strip())
    return found


def template_lines(path):
    """{template name: line} for `name :: asm(...)` declarations in a fixture file."""
    lines = {}
    for number, text in enumerate(Path(path).read_text().splitlines(), 1):
        m = re.match(r"^(\w+)\s*::\s*asm\b", text)
        if m:
            lines[m.group(1)] = number
    return lines


def table_mnemonics():
    """(VR4300 mnemonics, {gated mnemonic: LLVM feature}) from the hand-written MIPS table."""
    source = TABLE_SOURCE.read_text()
    block = source[source.index("#define ASM_MIPS_MNEMONICS"):source.index("#define ASM_MIPS_REGISTERS")]
    vr4300_part, gated_part = block.split("NOT on the VR4300")
    vr4300 = re.findall(r'X\(\w+, "(\w+)"\)', vr4300_part)
    gated = re.findall(r'X\(\w+, "(\w+)"\)', gated_part)
    features = {}
    for m in re.finditer(r"^\s*R\((\w+),\s*SH_\w+,.*?\b(MIPS4|MIPS32R2|MIPS32|MIPS64R2|MIPS64)\s*,", source, re.M):
        features.setdefault(m.group(1).lower(), m.group(2).lower())
    return vr4300, {m: features[m] for m in gated}


# --------------------------------------------------------------------------
# A tiny MIPS III interpreter: just enough to run the branch fixtures
# --------------------------------------------------------------------------

MASK64 = (1 << 64) - 1
RETURN_SENTINEL = 0xFFFFFFF0


def sext(value, bits):
    value &= (1 << bits) - 1
    return (value - (1 << bits) if value >> (bits - 1) else value) & MASK64


def signed64(value):
    return value - (1 << 64) if value >> 63 else value


def run_mips(offset, words, args, max_steps=10_000):
    """Execute a function's words (delay slots included) and return v0 as u32.

    `offset` is the function's section offset, which is what `j`/`jal` fields hold
    in a relocatable object. Unknown opcodes raise, so a test cannot pass on code
    the interpreter does not understand.
    """
    regs = [0] * 32
    for i, value in enumerate(args):
        regs[4 + i] = sext(value, 32)
    regs[29], regs[31] = 0x8000, RETURN_SENTINEL
    memory = {}
    index_of = lambda address: (address - offset) // 4
    pc, pending = 0, None
    for _ in range(max_steps):
        word = words[pc]
        op, rs, rt, rd = word >> 26, (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
        sa, fn, imm = (word >> 6) & 31, word & 63, sext(word & 0xFFFF, 16)
        s, t = regs[rs], regs[rt]
        branch, likely = None, False
        if op == 0:
            if fn == 0x00:   regs[rd] = sext(t << sa, 32)                       # sll / nop
            elif fn == 0x21: regs[rd] = sext(s + t, 32)                          # addu
            elif fn == 0x25: regs[rd] = s | t                                    # or / move
            elif fn == 0x2D: regs[rd] = (s + t) & MASK64                         # daddu
            elif fn == 0x08: branch = s                                          # jr
            else: raise NotImplementedError(f"SPECIAL funct {fn:#x} in {word:08x}")
        elif op == 0x01 and rt in (0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13):
            # bltz/bgez, +2: likely, +0x10: and link
            taken = signed64(s) >= 0 if rt & 1 else signed64(s) < 0
            likely = bool(rt & 0x02)
            if rt & 0x10:
                regs[31] = offset + (pc + 2) * 4
            branch = offset + (pc + 1 + signed64(imm)) * 4 if taken else None
        elif op & ~0x10 in (0x04, 0x05, 0x06, 0x07):
            # beq, bne, blez, bgtz, +0x10: likely
            taken = {0x04: s == t, 0x05: s != t, 0x06: signed64(s) <= 0, 0x07: signed64(s) > 0}[op & ~0x10]
            likely = bool(op & 0x10)
            branch = offset + (pc + 1 + signed64(imm)) * 4 if taken else None
        elif op in (0x02, 0x03):                                                 # j, jal
            if op == 0x03:
                regs[31] = offset + (pc + 2) * 4
            branch = (word & 0x03FFFFFF) << 2
        elif op == 0x09: regs[rt] = sext(s + imm, 32)                            # addiu
        elif op == 0x19: regs[rt] = (s + imm) & MASK64                           # daddiu
        elif op == 0x0D: regs[rt] = s | (word & 0xFFFF)                          # ori
        elif op == 0x0F: regs[rt] = sext((word & 0xFFFF) << 16, 32)              # lui
        elif op == 0x3F: memory[(s + imm) & MASK64] = t                          # sd
        elif op == 0x37: regs[rt] = memory[(s + imm) & MASK64]                   # ld
        else:
            raise NotImplementedError(f"opcode {op:#x} in {word:08x}")
        regs[0] = 0
        if pending is not None:
            if branch is not None:
                raise AssertionError(f"branch in a delay slot: {word:08x}")
            if pending == RETURN_SENTINEL:
                return regs[2] & 0xFFFFFFFF
            pc, pending = index_of(pending), None
        elif branch is not None:
            pending, pc = branch, pc + 1
        elif likely:
            pc += 2                                                              # not taken: slot annulled
        else:
            pc += 1
    raise AssertionError("function did not return")


# The words each enc_* proc in coverage/coverage.odin must contain; keep the two in
# sync. An int is an exact word; a (mask, bits) pair checks only the fixed fields
# (FPR operands are not pinned, and j/jal targets are relocations).
ENCODINGS = {
    "enc_add": [0x00851020],  # add v0, a0, a1
    "enc_addu": [0x00851021],  # addu v0, a0, a1
    "enc_sub": [0x00851022],  # sub v0, a0, a1
    "enc_subu": [0x00851023],  # subu v0, a0, a1
    "enc_and": [0x00851024],  # and v0, a0, a1
    "enc_or": [0x00851025],  # or v0, a0, a1
    "enc_xor": [0x00851026],  # xor v0, a0, a1
    "enc_nor": [0x00851027],  # nor v0, a0, a1
    "enc_slt": [0x0085102A],  # slt v0, a0, a1
    "enc_sltu": [0x0085102B],  # sltu v0, a0, a1
    "enc_dadd": [0x0085102C],  # dadd v0, a0, a1
    "enc_daddu": [0x0085102D],  # daddu v0, a0, a1
    "enc_dsub": [0x0085102E],  # dsub v0, a0, a1
    "enc_dsubu": [0x0085102F],  # dsubu v0, a0, a1
    "enc_addi": [0x2082FFFD],  # addi v0, a0, -3
    "enc_addiu": [0x248204D2],  # addiu v0, a0, 1234
    "enc_slti": [0x2882FFFB],  # slti v0, a0, -5
    "enc_sltiu": [0x2C82004D],  # sltiu v0, a0, 77
    "enc_andi": [0x3082F0F0],  # andi v0, a0, 61680
    "enc_ori": [0x34828001],  # ori v0, a0, 32769
    "enc_xori": [0x3882FFFF],  # xori v0, a0, 65535
    "enc_daddi": [0x6082FFF9],  # daddi v0, a0, -7
    "enc_daddiu": [0x64827FFF],  # daddiu v0, a0, 32767
    "enc_lui": [0x3C021234],  # lui v0, 0x1234
    "enc_sll": [0x000410C0],  # sll v0, a0, 3
    "enc_srl": [0x00041142],  # srl v0, a0, 5
    "enc_sra": [0x000411C3],  # sra v0, a0, 7
    "enc_dsll": [0x00041278],  # dsll v0, a0, 9
    "enc_dsrl": [0x000412FA],  # dsrl v0, a0, 11
    "enc_dsra": [0x0004137B],  # dsra v0, a0, 13
    "enc_dsll32": [0x0004107C],  # dsll32 v0, a0, 1
    "enc_dsrl32": [0x000410BE],  # dsrl32 v0, a0, 2
    "enc_dsra32": [0x000417FF],  # dsra32 v0, a0, 31
    "enc_sllv": [0x00A41004],  # sllv v0, a0, a1
    "enc_srlv": [0x00A41006],  # srlv v0, a0, a1
    "enc_srav": [0x00A41007],  # srav v0, a0, a1
    "enc_dsllv": [0x00A41014],  # dsllv v0, a0, a1
    "enc_dsrlv": [0x00A41016],  # dsrlv v0, a0, a1
    "enc_dsrav": [0x00A41017],  # dsrav v0, a0, a1
    "enc_mult": [0x00850018, 0x00001012],  # mult a0, a1 (bare, rd = $zero); mflo v0
    "enc_multu": [0x00850019, 0x00001012],  # multu a0, a1 (bare, rd = $zero); mflo v0
    "enc_div": [0x0085001A, 0x00001012],  # div a0, a1 (bare, rd = $zero); mflo v0
    "enc_divu": [0x0085001B, 0x00001012],  # divu a0, a1 (bare, rd = $zero); mflo v0
    "enc_dmult": [0x0085001C, 0x00001012],  # dmult a0, a1 (bare, rd = $zero); mflo v0
    "enc_dmultu": [0x0085001D, 0x00001012],  # dmultu a0, a1 (bare, rd = $zero); mflo v0
    "enc_ddiv": [0x0085001E, 0x00001012],  # ddiv a0, a1 (bare, rd = $zero); mflo v0
    "enc_ddivu": [0x0085001F, 0x00001012],  # ddivu a0, a1 (bare, rd = $zero); mflo v0
    "enc_mfhi": [0x00001010],  # mfhi v0
    "enc_mflo": [0x00001012],  # mflo v0
    "enc_mthi": [0x00800011, 0x00001010],  # mthi a0; mfhi v0
    "enc_mtlo": [0x00800013, 0x00001012],  # mtlo a0; mflo v0
    "enc_move": [0x00801025],  # or v0, a0, $zero
    "enc_not": [0x00801027],  # nor v0, a0, $zero
    "enc_negu": [0x00041023],  # subu v0, $zero, a0
    "enc_dnegu": [0x0004102F],  # dsubu v0, $zero, a0
    "enc_li": [0x3C021234, 0x34425678],  # lui v0, 0x1234; ori v0, v0, 0x5678
    "enc_li_small": [0x2402FFFB],  # addiu v0, $zero, -5
    "enc_li_u16": [0x3402FFFF],  # ori v0, $zero, 0xffff
    "enc_li_hi": [0x3C028000],  # lui v0, 0x8000
    "enc_lb": [0x80820008],  # lb v0, 8(a0)
    "enc_lbu": [0x90820008],  # lbu v0, 8(a0)
    "enc_lh": [0x84820008],  # lh v0, 8(a0)
    "enc_lhu": [0x94820008],  # lhu v0, 8(a0)
    "enc_lw": [0x8C820008],  # lw v0, 8(a0)
    "enc_lwu": [0x9C820008],  # lwu v0, 8(a0)
    "enc_ld": [0xDC820008],  # ld v0, 8(a0)
    "enc_lwl": [0x88820008],  # lwl v0, 8(a0)
    "enc_lwr": [0x98820008],  # lwr v0, 8(a0)
    "enc_ldl": [0x68820008],  # ldl v0, 8(a0)
    "enc_ldr": [0x6C820008],  # ldr v0, 8(a0)
    "enc_ll": [0xC0820008],  # ll v0, 8(a0)
    "enc_lld": [0xD0820008],  # lld v0, 8(a0)
    "enc_sb": [0xA085FFF8],  # sb a1, -8(a0)
    "enc_sh": [0xA485FFF8],  # sh a1, -8(a0)
    "enc_sw": [0xAC85FFF8],  # sw a1, -8(a0)
    "enc_sd": [0xFC85FFF8],  # sd a1, -8(a0)
    "enc_swl": [0xA885FFF8],  # swl a1, -8(a0)
    "enc_swr": [0xB885FFF8],  # swr a1, -8(a0)
    "enc_sdl": [0xB085FFF8],  # sdl a1, -8(a0)
    "enc_sdr": [0xB485FFF8],  # sdr a1, -8(a0)
    "enc_sc": [0xE0820008],  # sc v0, 8(a0)
    "enc_scd": [0xF0820008],  # scd v0, 8(a0)
    "enc_beq": [0x10850002],  # beq a0, a1, +2
    "enc_bne": [0x14850002],  # bne a0, a1, +2
    "enc_beql": [0x50850002],  # beql a0, a1, +2
    "enc_bnel": [0x54850002],  # bnel a0, a1, +2
    "enc_blez": [0x18800002],  # blez a0, +2
    "enc_bgtz": [0x1C800002],  # bgtz a0, +2
    "enc_blezl": [0x58800002],  # blezl a0, +2
    "enc_bgtzl": [0x5C800002],  # bgtzl a0, +2
    "enc_bltz": [0x04800002],  # bltz a0, +2
    "enc_bgez": [0x04810002],  # bgez a0, +2
    "enc_bltzl": [0x04820002],  # bltzl a0, +2
    "enc_bgezl": [0x04830002],  # bgezl a0, +2
    "enc_bltzal": [0x04900002],  # bltzal a0, +2
    "enc_bgezal": [0x04910002],  # bgezal a0, +2
    "enc_bltzall": [0x04920002],  # bltzall a0, +2
    "enc_bgezall": [0x04930002],  # bgezall a0, +2
    "enc_beqz": [0x10800002],  # beqz a0, +2
    "enc_bnez": [0x14800002],  # bnez a0, +2
    "enc_beqzl": [0x50800002],  # beqzl a0, +2
    "enc_bnezl": [0x54800002],  # bnezl a0, +2
    "enc_bal": [0x04110002],  # bgezal $zero, +2
    "enc_b": [0x10000002, 0x10800003],  # beq $zero, $zero, +2; beqz a0, +3
    "enc_j": [(0xFC000000, 0x08000000)],  # j (target is a relocation)
    "enc_jal": [(0xFC000000, 0x0C000000)],  # jal (target is a relocation)
    "enc_jr": [0x00800008],  # jr a0
    "enc_jalr": [0x0080F809],  # jalr ra, a0
    "enc_jalr_rd": [0x00801009],  # jalr v0, a0
    "enc_teq": [0x00850034],  # teq a0, a1
    "enc_teq_code": [0x008501F4],  # teq a0, a1, 7
    "enc_tne": [0x00850036],  # tne a0, a1
    "enc_tne_code": [0x008501F6],  # tne a0, a1, 7
    "enc_tge": [0x00850030],  # tge a0, a1
    "enc_tge_code": [0x008501F0],  # tge a0, a1, 7
    "enc_tgeu": [0x00850031],  # tgeu a0, a1
    "enc_tgeu_code": [0x008501F1],  # tgeu a0, a1, 7
    "enc_tlt": [0x00850032],  # tlt a0, a1
    "enc_tlt_code": [0x008501F2],  # tlt a0, a1, 7
    "enc_tltu": [0x00850033],  # tltu a0, a1
    "enc_tltu_code": [0x008501F3],  # tltu a0, a1, 7
    "enc_teqi": [0x048CFFF7],  # teqi a0, -9
    "enc_tnei": [0x048E0009],  # tnei a0, 9
    "enc_tgei": [0x04888000],  # tgei a0, -32768
    "enc_tgeiu": [0x04897FFF],  # tgeiu a0, 32767
    "enc_tlti": [0x048A0064],  # tlti a0, 100
    "enc_tltiu": [0x048BFFFF],  # tltiu a0, -1
    "enc_nop": [0x00000000],  # sll $0, $0, 0
    "enc_ssnop": [0x00000040],  # sll $0, $0, 1
    "enc_sync": [0x0000000F],  # sync
    "enc_syscall": [0x0000000C],  # syscall
    "enc_syscall_code": [0x00048D0C],  # syscall 0x1234
    "enc_break": [0x0000000D],  # break
    "enc_break_code": [0x0007000D],  # break 7
    "enc_break_code2": [0x000700CD],  # break 7, 3
    "enc_cache": [
        0xBC800010,  # cache 0x00, 16(a0)
        0xBC840010,  # cache 0x04, 16(a0)
        0xBC880010,  # cache 0x08, 16(a0)
        0xBC900010,  # cache 0x10, 16(a0)
        0xBC940010,  # cache 0x14, 16(a0)
        0xBC980010,  # cache 0x18, 16(a0)
        0xBC810010,  # cache 0x01, 16(a0)
        0xBC850010,  # cache 0x05, 16(a0)
        0xBC890010,  # cache 0x09, 16(a0)
        0xBC8D0010,  # cache 0x0d, 16(a0)
        0xBC910010,  # cache 0x11, 16(a0)
        0xBC950010,  # cache 0x15, 16(a0)
        0xBC990010,  # cache 0x19, 16(a0)
    ],
    "enc_mfc0": [
        0x40020000,  # mfc0 v0, $0 (index)
        0x40020800,  # mfc0 v0, $1 (random)
        0x40021000,  # mfc0 v0, $2 (entrylo0)
        0x40021800,  # mfc0 v0, $3 (entrylo1)
        0x40022000,  # mfc0 v0, $4 (context)
        0x40022800,  # mfc0 v0, $5 (pagemask)
        0x40023000,  # mfc0 v0, $6 (wired)
        0x40024000,  # mfc0 v0, $8 (badvaddr)
        0x40024800,  # mfc0 v0, $9 (count)
        0x40025000,  # mfc0 v0, $10 (entryhi)
        0x40025800,  # mfc0 v0, $11 (compare)
        0x40026000,  # mfc0 v0, $12 (status)
        0x40026800,  # mfc0 v0, $13 (cause)
        0x40027000,  # mfc0 v0, $14 (epc)
        0x40027800,  # mfc0 v0, $15 (prid)
        0x40028000,  # mfc0 v0, $16 (config)
        0x40028800,  # mfc0 v0, $17 (lladdr)
        0x40029000,  # mfc0 v0, $18 (watchlo)
        0x40029800,  # mfc0 v0, $19 (watchhi)
        0x4002A000,  # mfc0 v0, $20 (xcontext)
        0x4002D000,  # mfc0 v0, $26 (perr)
        0x4002D800,  # mfc0 v0, $27 (cacheerr)
        0x4002E000,  # mfc0 v0, $28 (taglo)
        0x4002E800,  # mfc0 v0, $29 (taghi)
        0x4002F000,  # mfc0 v0, $30 (errorepc)
    ],
    "enc_mtc0": [
        0x40840000,  # mtc0 a0, $0 (index)
        0x40840800,  # mtc0 a0, $1 (random)
        0x40841000,  # mtc0 a0, $2 (entrylo0)
        0x40841800,  # mtc0 a0, $3 (entrylo1)
        0x40842000,  # mtc0 a0, $4 (context)
        0x40842800,  # mtc0 a0, $5 (pagemask)
        0x40843000,  # mtc0 a0, $6 (wired)
        0x40844000,  # mtc0 a0, $8 (badvaddr)
        0x40844800,  # mtc0 a0, $9 (count)
        0x40845000,  # mtc0 a0, $10 (entryhi)
        0x40845800,  # mtc0 a0, $11 (compare)
        0x40846000,  # mtc0 a0, $12 (status)
        0x40846800,  # mtc0 a0, $13 (cause)
        0x40847000,  # mtc0 a0, $14 (epc)
        0x40847800,  # mtc0 a0, $15 (prid)
        0x40848000,  # mtc0 a0, $16 (config)
        0x40848800,  # mtc0 a0, $17 (lladdr)
        0x40849000,  # mtc0 a0, $18 (watchlo)
        0x40849800,  # mtc0 a0, $19 (watchhi)
        0x4084A000,  # mtc0 a0, $20 (xcontext)
        0x4084D000,  # mtc0 a0, $26 (perr)
        0x4084D800,  # mtc0 a0, $27 (cacheerr)
        0x4084E000,  # mtc0 a0, $28 (taglo)
        0x4084E800,  # mtc0 a0, $29 (taghi)
        0x4084F000,  # mtc0 a0, $30 (errorepc)
    ],
    "enc_dmfc0": [
        0x40221000,  # dmfc0 v0, $2 (entrylo0)
        0x40222000,  # dmfc0 v0, $4 (context)
        0x40224000,  # dmfc0 v0, $8 (badvaddr)
        0x40225000,  # dmfc0 v0, $10 (entryhi)
        0x40227000,  # dmfc0 v0, $14 (epc)
        0x4022A000,  # dmfc0 v0, $20 (xcontext)
        0x4022F000,  # dmfc0 v0, $30 (errorepc)
    ],
    "enc_dmtc0": [
        0x40A41000,  # dmtc0 a0, $2 (entrylo0)
        0x40A42000,  # dmtc0 a0, $4 (context)
        0x40A44000,  # dmtc0 a0, $8 (badvaddr)
        0x40A45000,  # dmtc0 a0, $10 (entryhi)
        0x40A47000,  # dmtc0 a0, $14 (epc)
        0x40A4A000,  # dmtc0 a0, $20 (xcontext)
        0x40A4F000,  # dmtc0 a0, $30 (errorepc)
    ],
    "enc_tlbr": [0x42000001],  # tlbr
    "enc_tlbwi": [0x42000002],  # tlbwi
    "enc_tlbwr": [0x42000006],  # tlbwr
    "enc_tlbp": [0x42000008],  # tlbp
    "enc_eret": [0x42000018],  # eret
    "enc_mfc1": [(0xFFFF07FF, 0x44020000)],  # mfc1 v0, fs
    "enc_mtc1": [(0xFFFF07FF, 0x44840000)],  # mtc1 a0, fs
    "enc_dmfc1": [(0xFFFF07FF, 0x44220000)],  # dmfc1 v0, fs
    "enc_dmtc1": [(0xFFFF07FF, 0x44A40000)],  # dmtc1 a0, fs
    "enc_cfc1": [0x44420000, 0x4442F800],  # cfc1 v0, $0; cfc1 v0, $31
    "enc_ctc1": [0x44C4F800],  # ctc1 a0, $31
    "enc_lwc1": [(0xFFE0FFFF, 0xC4800008)],  # lwc1 ft, 8(a0)
    "enc_ldc1": [(0xFFE0FFFF, 0xD4800008)],  # ldc1 ft, 8(a0)
    "enc_swc1": [(0xFFE0FFFF, 0xE480FFF8)],  # swc1 ft, -8(a0)
    "enc_sdc1": [(0xFFE0FFFF, 0xF480FFF8)],  # sdc1 ft, -8(a0)
    "enc_add_s": [(0xFFE0003F, 0x46000000)],  # add.s
    "enc_add_d": [(0xFFE0003F, 0x46200000)],  # add.d
    "enc_sub_s": [(0xFFE0003F, 0x46000001)],  # sub.s
    "enc_sub_d": [(0xFFE0003F, 0x46200001)],  # sub.d
    "enc_mul_s": [(0xFFE0003F, 0x46000002)],  # mul.s
    "enc_mul_d": [(0xFFE0003F, 0x46200002)],  # mul.d
    "enc_div_s": [(0xFFE0003F, 0x46000003)],  # div.s
    "enc_div_d": [(0xFFE0003F, 0x46200003)],  # div.d
    "enc_sqrt_s": [(0xFFFF003F, 0x46000004)],  # sqrt.s
    "enc_sqrt_d": [(0xFFFF003F, 0x46200004)],  # sqrt.d
    "enc_abs_s": [(0xFFFF003F, 0x46000005)],  # abs.s
    "enc_abs_d": [(0xFFFF003F, 0x46200005)],  # abs.d
    "enc_mov_s": [(0xFFFF003F, 0x46000006)],  # mov.s
    "enc_mov_d": [(0xFFFF003F, 0x46200006)],  # mov.d
    "enc_neg_s": [(0xFFFF003F, 0x46000007)],  # neg.s
    "enc_neg_d": [(0xFFFF003F, 0x46200007)],  # neg.d
    "enc_cvt_s_d": [(0xFFFF003F, 0x46200020)],  # cvt.s.d
    "enc_cvt_s_w": [(0xFFFF003F, 0x46800020)],  # cvt.s.w
    "enc_cvt_s_l": [(0xFFFF003F, 0x46A00020)],  # cvt.s.l
    "enc_cvt_d_s": [(0xFFFF003F, 0x46000021)],  # cvt.d.s
    "enc_cvt_d_w": [(0xFFFF003F, 0x46800021)],  # cvt.d.w
    "enc_cvt_d_l": [(0xFFFF003F, 0x46A00021)],  # cvt.d.l
    "enc_cvt_w_s": [(0xFFFF003F, 0x46000024)],  # cvt.w.s
    "enc_cvt_w_d": [(0xFFFF003F, 0x46200024)],  # cvt.w.d
    "enc_cvt_l_s": [(0xFFFF003F, 0x46000025)],  # cvt.l.s
    "enc_cvt_l_d": [(0xFFFF003F, 0x46200025)],  # cvt.l.d
    "enc_round_l_s": [(0xFFFF003F, 0x46000008)],  # round.l.s
    "enc_round_l_d": [(0xFFFF003F, 0x46200008)],  # round.l.d
    "enc_round_w_s": [(0xFFFF003F, 0x4600000C)],  # round.w.s
    "enc_round_w_d": [(0xFFFF003F, 0x4620000C)],  # round.w.d
    "enc_trunc_l_s": [(0xFFFF003F, 0x46000009)],  # trunc.l.s
    "enc_trunc_l_d": [(0xFFFF003F, 0x46200009)],  # trunc.l.d
    "enc_trunc_w_s": [(0xFFFF003F, 0x4600000D)],  # trunc.w.s
    "enc_trunc_w_d": [(0xFFFF003F, 0x4620000D)],  # trunc.w.d
    "enc_ceil_l_s": [(0xFFFF003F, 0x4600000A)],  # ceil.l.s
    "enc_ceil_l_d": [(0xFFFF003F, 0x4620000A)],  # ceil.l.d
    "enc_ceil_w_s": [(0xFFFF003F, 0x4600000E)],  # ceil.w.s
    "enc_ceil_w_d": [(0xFFFF003F, 0x4620000E)],  # ceil.w.d
    "enc_floor_l_s": [(0xFFFF003F, 0x4600000B)],  # floor.l.s
    "enc_floor_l_d": [(0xFFFF003F, 0x4620000B)],  # floor.l.d
    "enc_floor_w_s": [(0xFFFF003F, 0x4600000F)],  # floor.w.s
    "enc_floor_w_d": [(0xFFFF003F, 0x4620000F)],  # floor.w.d
    "enc_c_f_s": [(0xFFE007FF, 0x46000030)],  # c.f.s
    "enc_c_un_s": [(0xFFE007FF, 0x46000031)],  # c.un.s
    "enc_c_eq_s": [(0xFFE007FF, 0x46000032)],  # c.eq.s
    "enc_c_ueq_s": [(0xFFE007FF, 0x46000033)],  # c.ueq.s
    "enc_c_olt_s": [(0xFFE007FF, 0x46000034)],  # c.olt.s
    "enc_c_ult_s": [(0xFFE007FF, 0x46000035)],  # c.ult.s
    "enc_c_ole_s": [(0xFFE007FF, 0x46000036)],  # c.ole.s
    "enc_c_ule_s": [(0xFFE007FF, 0x46000037)],  # c.ule.s
    "enc_c_sf_s": [(0xFFE007FF, 0x46000038)],  # c.sf.s
    "enc_c_ngle_s": [(0xFFE007FF, 0x46000039)],  # c.ngle.s
    "enc_c_seq_s": [(0xFFE007FF, 0x4600003A)],  # c.seq.s
    "enc_c_ngl_s": [(0xFFE007FF, 0x4600003B)],  # c.ngl.s
    "enc_c_lt_s": [(0xFFE007FF, 0x4600003C)],  # c.lt.s
    "enc_c_nge_s": [(0xFFE007FF, 0x4600003D)],  # c.nge.s
    "enc_c_le_s": [(0xFFE007FF, 0x4600003E)],  # c.le.s
    "enc_c_ngt_s": [(0xFFE007FF, 0x4600003F)],  # c.ngt.s
    "enc_c_f_d": [(0xFFE007FF, 0x46200030)],  # c.f.d
    "enc_c_un_d": [(0xFFE007FF, 0x46200031)],  # c.un.d
    "enc_c_eq_d": [(0xFFE007FF, 0x46200032)],  # c.eq.d
    "enc_c_ueq_d": [(0xFFE007FF, 0x46200033)],  # c.ueq.d
    "enc_c_olt_d": [(0xFFE007FF, 0x46200034)],  # c.olt.d
    "enc_c_ult_d": [(0xFFE007FF, 0x46200035)],  # c.ult.d
    "enc_c_ole_d": [(0xFFE007FF, 0x46200036)],  # c.ole.d
    "enc_c_ule_d": [(0xFFE007FF, 0x46200037)],  # c.ule.d
    "enc_c_sf_d": [(0xFFE007FF, 0x46200038)],  # c.sf.d
    "enc_c_ngle_d": [(0xFFE007FF, 0x46200039)],  # c.ngle.d
    "enc_c_seq_d": [(0xFFE007FF, 0x4620003A)],  # c.seq.d
    "enc_c_ngl_d": [(0xFFE007FF, 0x4620003B)],  # c.ngl.d
    "enc_c_lt_d": [(0xFFE007FF, 0x4620003C)],  # c.lt.d
    "enc_c_nge_d": [(0xFFE007FF, 0x4620003D)],  # c.nge.d
    "enc_c_le_d": [(0xFFE007FF, 0x4620003E)],  # c.le.d
    "enc_c_ngt_d": [(0xFFE007FF, 0x4620003F)],  # c.ngt.d
    "enc_bc1f": [0x45000002, (0xFFE007FF, 0x46000034)],  # bc1f +2; c.olt.s
    "enc_bc1t": [0x45010002, (0xFFE007FF, 0x46000034)],  # bc1t +2; c.olt.s
    "enc_bc1fl": [0x45020002, (0xFFE007FF, 0x46000034)],  # bc1fl +2; c.olt.s
    "enc_bc1tl": [0x45030002, (0xFFE007FF, 0x46000034)],  # bc1tl +2; c.olt.s
}


# Branch functions in coverage/: `move r, a; BR .d; addiu r, r, 1; .d:` returns a
# when the branch is taken and a + 1 when it falls through. `b`/`j` use
# `beqz a, .s; b .d; .s: addiu ...`, which adds 1 only when a == 0.
def _s32(v):
    return v - (1 << 32) if v & 0x80000000 else v

BRANCH_TAKEN = {
    "enc_beq":  lambda a, b: a == b,  "enc_beql":  lambda a, b: a == b,
    "enc_bne":  lambda a, b: a != b,  "enc_bnel":  lambda a, b: a != b,
    "enc_blez": lambda a, b: _s32(a) <= 0, "enc_blezl": lambda a, b: _s32(a) <= 0,
    "enc_bgtz": lambda a, b: _s32(a) > 0,  "enc_bgtzl": lambda a, b: _s32(a) > 0,
    "enc_bltz": lambda a, b: _s32(a) < 0,  "enc_bltzl": lambda a, b: _s32(a) < 0,
    "enc_bgez": lambda a, b: _s32(a) >= 0, "enc_bgezl": lambda a, b: _s32(a) >= 0,
    "enc_bltzal": lambda a, b: _s32(a) < 0,  "enc_bltzall": lambda a, b: _s32(a) < 0,
    "enc_bgezal": lambda a, b: _s32(a) >= 0, "enc_bgezall": lambda a, b: _s32(a) >= 0,
    "enc_beqz": lambda a, b: a == 0,  "enc_beqzl": lambda a, b: a == 0,
    "enc_bnez": lambda a, b: a != 0,  "enc_bnezl": lambda a, b: a != 0,
    "enc_bal":  lambda a, b: True,    "enc_jal":   lambda a, b: True,
    "enc_b":    lambda a, b: a != 0,  "enc_j":     lambda a, b: a != 0,
}
SAMPLE_VALUES = (0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF)

# Mnemonic objdump prints for a coverage function, where it differs from the name.
OBJDUMP_NAMES = {"enc_not": "nor", "enc_li": "lui", "enc_li_hi": "lui", "enc_li_small": "li",
                 "enc_li_u16": "li", "enc_jalr_rd": "jalr"}


def expected_words(spec):
    return [(0xFFFFFFFF, item) if isinstance(item, int) else item for item in spec]


def find_sequence(words, sequence):
    n = len(sequence)
    return any(list(words[i:i + n]) == sequence for i in range(len(words) - n + 1))


class Encodings(unittest.TestCase):
    """Instruction words in the object file, for both MIPS targets."""

    def test_every_vr4300_mnemonic_is_covered(self):
        vr4300, gated = table_mnemonics()
        self.assertGreater(len(vr4300), 200)
        missing = [m for m in vr4300 if f"enc_{m}" not in ENCODINGS]
        self.assertEqual(missing, [], "add a coverage/ proc and an ENCODINGS row for each")
        templates = template_lines(HERE / "bad_features" / "bad_features.odin")
        self.assertEqual([m for m in gated if f"g_{m}" not in templates], [],
                         "add a bad_features/ template for each gated mnemonic")
        self.assertEqual([k for k in ENCODINGS if k[4:] in gated], [])

    def test_coverage_encodings(self):
        require_o64_objects(self)
        for target in TARGETS:
            functions = object_functions("coverage", target)
            for function, spec in ENCODINGS.items():
                with self.subTest(target=target, function=function):
                    self.assertIn(function, functions)
                    words = functions[function]
                    for mask, bits in expected_words(spec):
                        self.assertTrue(any(w & mask == bits for w in words),
                                        f"no {bits:08x}/{mask:08x} in {' '.join(f'{w:08x}' for w in words)}")

    def test_fixture_encodings(self):
        # Fixed fields only: the allocator picks the GPRs.
        expect = {
            "data_cache_hit_writeback_invalidate": (0xFC1FFFFF, 0xBC150000),  # cache 0x15, 0(base)
            "inst_cache_hit_invalidate":           (0xFC1FFFFF, 0xBC100000),  # cache 0x10, 0(base)
            "read_cp0_count":                      (0xFFE0FFFF, 0x40004800),  # mfc0 rt, $9
            "write_cp0_compare":                   (0xFFE0FFFF, 0x40805800),  # mtc0 rt, $11
            "load_word_at_offset":                 (0xFC00FFFF, 0x8C000008),  # lw rt, 8(base)
            "add_u64":                             (0xFC0007FF, 0x0000002D),  # daddu
        }
        require_o64_objects(self)
        for target in TARGETS:
            functions = object_functions("fixture", target)
            for function, (mask, bits) in expect.items():
                with self.subTest(target=target, function=function):
                    words = functions[function]
                    self.assertTrue(any(w & mask == bits for w in words),
                                    f"{function}: no {bits:08x}/{mask:08x} in {[f'{w:08x}' for w in words]}")
            # pinned_double reads the pinned input $5 (a1) and writes $8 (t0), then $3 (v1)
            self.assertIn(0x00A54021, functions["pinned_double"])  # addu t0, a1, a1

    def test_objdump_agrees_with_function_names(self):
        objdump = objdump_path()
        if objdump is None:
            self.skipTest("set MIPS_O64_OBJDUMP or N64_INST to cross-check with objdump")
        require_o64_objects(self)
        listing = subprocess.run([str(objdump), "-d", str(build_object("coverage", "n64"))],
                                 capture_output=True, text=True, check=True).stdout
        mnemonics, current = {}, None
        for line in listing.splitlines():
            m = re.match(r"^[0-9a-f]+ <(\w+)>:", line)
            if m:
                current = m.group(1)
                mnemonics[current] = []
                continue
            m = re.match(r"^\s+[0-9a-f]+:\s+[0-9a-f]{8}\s+(\S+)", line)
            if current and m:
                mnemonics[current].append(m.group(1))
        for function in ENCODINGS:
            name = OBJDUMP_NAMES.get(function) or re.sub(r"_(code2?)$", "", function[4:]).replace("_", ".")
            with self.subTest(function=function):
                self.assertIn(name, mnemonics.get(function, []))


class Constraints(unittest.TestCase):
    """The LLVM inline-asm strings and constraints the backend writes."""

    def test_every_asm_call_clobbers_at(self):
        # $at (~{$1}) never holds an operand: assembler macros expand through it.
        builds = [(p, t) for p in ("fixture", "coverage", "regress") for t in TARGETS] + [("rom", "n64")]
        for package, target in builds:
            with self.subTest(package=package, target=target):
                calls = asm_calls(build_ir(package, target)[1])
                self.assertTrue(calls)
                for function, _, text, constraints in calls:
                    self.assertTrue(constraints.endswith("~{$1}"), f"{function}: {text!r} {constraints!r}")
        coverage = {fn for fn, *_ in asm_calls(build_ir("coverage")[1])}
        self.assertEqual(sorted(set(ENCODINGS) - coverage), [])

    def test_fixture_constraints(self):
        ir = build_ir("fixture")[1]
        by_text = {text: (side, constraints) for _, side, text, constraints in asm_calls(ir)}
        cache = [v for k, v in by_text.items() if "cache 21" in k]
        self.assertEqual(cache[0], (True, "r,~{memory},~{$1}"))
        self.assertIn((True, "=r,~{memory},~{$1}"), [v for k, v in by_text.items() if "mfc0 $0, $$9" in k])
        self.assertIn((False, "=&{$3},{$5},~{$8},~{$1}"), [v for k, v in by_text.items() if "addu $$8" in k])
        self.assertTrue(any(re.search(r"1:.*bne \$0, \$\$0, 1b", k, re.S) for k in by_text))

    def test_pins_and_clobbers_are_numeric(self):
        # LLVM's Mips backend asserts on named GPRs ({$a1}): only {$N}, {$fN}, {$fccN}.
        for package in ("fixture", "coverage", "regress"):
            ir = build_ir(package)[1]
            for _, _, _, constraints in asm_calls(ir):
                for name in re.findall(r"\{\$([^}]*)\}", constraints):
                    self.assertRegex(name, r"^(\d+|f\d+|fcc\d+)$", f"{package}: {constraints}")
        coverage = build_ir("coverage")[1]
        self.assertEqual(calls_in(coverage, "enc_addu"), [(False, r"\09addu $0, $1, $2", "={$2},{$4},{$5},~{$1}")])

    def test_hi_lo_are_clobbered_without_a_declaration(self):
        ir = build_ir("coverage")[1]
        for function in ("enc_mult", "enc_multu", "enc_div", "enc_divu", "enc_dmult", "enc_dmultu",
                         "enc_ddiv", "enc_ddivu", "enc_mfhi", "enc_mflo"):
            with self.subTest(function=function):
                (_, _, constraints), = calls_in(ir, function)
                self.assertIn("~{hi},~{lo}", constraints)
        self.assertIn("~{hi},", calls_in(ir, "enc_mthi")[0][2])
        self.assertNotIn("~{lo}", calls_in(ir, "enc_mthi")[0][2])
        self.assertIn("~{lo},", calls_in(ir, "enc_mtlo")[0][2])
        self.assertNotIn("~{hi}", calls_in(ir, "enc_mtlo")[0][2])

    def test_two_operand_divide_is_the_bare_instruction(self):
        # LLVM's `div rs, rt` is a macro (zero trap + mflo into rs); the backend writes `div $zero, rs, rt`.
        ir = build_ir("coverage")[1]
        for m in ("div", "divu", "ddiv", "ddivu"):
            # div writes HI/LO in the body's first two instructions, so the HI/LO
            # hazard padding puts two nops in front of it.
            self.assertTrue(calls_in(ir, f"enc_{m}")[0][1].startswith(rf"\09nop\0A\09nop\0A\09{m} $$0, $1, $2"))

    def test_fcc0_is_clobbered_by_compares_and_ctc1(self):
        ir = build_ir("coverage")[1]
        functions = [f for f in ENCODINGS if re.match(r"enc_(c_\w+_[sd]|bc1\w+|ctc1)$", f)]
        self.assertEqual(len(functions), 32 + 4 + 1)
        for function in functions:
            with self.subTest(function=function):
                self.assertIn("~{$fcc0}", calls_in(ir, function)[0][2])

    def test_ra_is_clobbered_by_link_forms(self):
        ir = build_ir("coverage")[1]
        for function in ("enc_jal", "enc_bal", "enc_bltzal", "enc_bgezal", "enc_bltzall", "enc_bgezall", "enc_jalr"):
            with self.subTest(function=function):
                self.assertIn("~{$31}", calls_in(ir, function)[0][2])
        self.assertNotIn("~{$31}", calls_in(ir, "enc_jalr_rd")[0][2])

    def test_volatile_and_memory_inference(self):
        ir = build_ir("coverage")[1]
        volatile_memory = ["enc_cache", "enc_mfc0", "enc_mtc0", "enc_dmfc0", "enc_dmtc0", "enc_tlbr", "enc_tlbwi",
                           "enc_tlbwr", "enc_tlbp", "enc_eret", "enc_sync", "enc_ll", "enc_lld", "enc_sc", "enc_scd",
                           "enc_jalr", "enc_jalr_rd"]
        volatile_only = ["enc_syscall", "enc_break", "enc_teq", "enc_teqi", "enc_jr", "enc_ctc1", "enc_cfc1"]
        memory_only = ["enc_lw", "enc_sw", "enc_ld", "enc_sd", "enc_lwl", "enc_swr", "enc_lwc1", "enc_sdc1"]
        neither = ["enc_addu", "enc_daddu", "enc_li", "enc_mult", "enc_add_s", "enc_c_eq_s", "enc_beq"]
        for group, side, memory in ((volatile_memory, True, True), (volatile_only, True, False),
                                    (memory_only, False, True), (neither, False, False)):
            for function in group:
                with self.subTest(function=function):
                    (got_side, _, constraints), = calls_in(ir, function)
                    self.assertEqual((got_side, "~{memory}" in constraints), (side, memory), constraints)

    def test_fpu_mnemonics_are_written_with_dots(self):
        ir = build_ir("coverage")[1]
        fpu = [f for f in ENCODINGS if re.match(r"enc_(\w+_[sdwl]|c_\w+)$", f)]
        self.assertEqual(len(fpu), 16 + 26 + 32)  # arithmetic, conversions, compares
        for function in fpu:
            with self.subTest(function=function):
                text = calls_in(ir, function)[0][1]
                self.assertIn(function[4:].replace("_", "."), text)
                self.assertNotIn(function[4:], text)


class Traps(unittest.TestCase):
    """One test per MIPS-specific trap the asm backend must not fall into."""

    def test_at_li_expands_on_its_destination(self):
        # Trap: $at hazard. `li` with a 32-bit constant is an assembler macro; it must
        # expand to lui+ori on the destination, and the template must still clobber $1.
        self.assertEqual(calls_in(build_ir("regress")[1], "load_wide_constant"),
                         [(False, r"\09li $0, 305419896", "=r,~{$1}")])
        require_o64_objects(self)
        for target in TARGETS:
            words = object_functions("regress", target)["load_wide_constant"]
            self.assertTrue(any(list(words[i:i + 2]) == [0x3C000000 | (r << 16) | 0x1234, 0x34000000 | (r << 21) | (r << 16) | 0x5678]
                                for i in range(len(words) - 1) for r in range(32)), [f"{w:08x}" for w in words])

    def test_at_largest_displacements_encode_directly(self):
        # Trap: $at hazard. 32767 and -32768 fit the offset field; no lui/addu via $at.
        require_o64_objects(self)
        for target in TARGETS:
            functions = object_functions("regress", target)
            for function, word in (("store_max_displacement", 0xAC857FFF), ("store_min_displacement", 0xAC858000)):
                with self.subTest(target=target, function=function):
                    self.assertIn(word, functions[function])  # sw a1, +-disp(a0)
                    self.assertFalse(any(w >> 16 == 0x3C01 for w in functions[function]), "lui $at")

    def test_at_large_displacement_is_rejected(self):
        # Trap: $at hazard. `sw v, [p + 40000]` used to store the address instead of
        # the value (LLVM allocated $at to v, then expanded the offset through $at).
        _, output = check_package("bad_check")
        line = template_lines(HERE / "bad_check" / "bad_check.odin")["sw_disp_wide"]
        self.assertIn((line, "'sw' operand-1 memory displacement 40000 does not fit in a signed 16-bit offset (-32768..=32767)"),
                      [(l, m) for l, m, _ in diagnostics(output)])

    def test_named_pins_become_numeric_constraints(self):
        # Trap: LLVM's Mips backend asserts on {$a1}-style named constraints.
        ir = build_ir("regress")[1]
        self.assertNotRegex(ir, r"\{\$(?!(\d+|f\d+|fcc\d+)\})[^}]*\}")
        self.assertEqual(calls_in(ir, "literal_read_of_pinned"), [(False, r"\09addu $0, $$5, $$5", "=r,{$5},~{$1}")])
        # mflo/mfhi as the last instruction gets the HI/LO hazard padding after it.
        self.assertEqual(calls_in(ir, "read_pinned_lo"), [(False, r"\09mflo $0\0A\09nop\0A\09nop", "=r,{lo},~{$1}")])
        self.assertEqual(calls_in(ir, "read_pinned_hi"), [(False, r"\09mfhi $0\0A\09nop\0A\09nop", "=r,{hi},~{$1}")])

    def test_literal_read_of_a_pinned_register_is_the_parameter(self):
        # Trap: `addu r, %a1, %a1` with v pinned to %a1 must read v, i.e. LLVM moves v into $5 first.
        require_o64_objects(self)
        for target in TARGETS:
            words = object_functions("regress", target)["literal_read_of_pinned"]
            self.assertTrue(find_sequence(words, [0x00802825, 0x00A51021]),  # move a1, a0; addu v0, a1, a1
                            [f"{w:08x}" for w in words])

    def test_delay_slots_hold_a_nop(self):
        # Trap: delay slots. `.set reorder` puts a nop after every branch; the
        # instruction after the branch in the template runs on fall-through only.
        expect = {
            "enc_beq":  [0x00801025, 0x10850002, 0x00000000, 0x24420001],  # move; beq a0,a1,+2; nop; addiu
            "enc_bnel": [0x00801025, 0x54850002, 0x00000000, 0x24420001],  # likely: same nop
            "enc_b":    [0x00801025, 0x10800003, 0x00000000, 0x10000002, 0x00000000, 0x24420001],
            "enc_bc1t": [0x45010002, 0x00000000, 0x24020000],              # bc1t +2; nop; li v0, 0
            "enc_jr":   [0x00800008, 0x00000000],                          # jr a0; nop
        }
        require_o64_objects(self)
        for target in TARGETS:
            functions = object_functions("coverage", target)
            for function, sequence in expect.items():
                with self.subTest(target=target, function=function):
                    self.assertTrue(find_sequence(functions[function], sequence),
                                    [f"{w:08x}" for w in functions[function]])
            hazard = object_functions("regress", target)["write_entryhi"]
            self.assertTrue(find_sequence(hazard, [0x40845000, 0, 0]), "explicit hazard nops dropped")

    def test_branches_compute_the_right_value(self):
        # Trap: delay slots and branch offsets. Run every integer branch fixture
        # through an interpreter (delay slots, likely annulment, link) for sample inputs.
        require_o64_objects(self)
        for target in TARGETS:
            symbols = elf32be_symbols(build_object("coverage", target))
            for function, taken in BRANCH_TAKEN.items():
                offset, words = symbols[function]
                for a in SAMPLE_VALUES:
                    for b in (a, (a + 1) & 0xFFFFFFFF):
                        with self.subTest(target=target, function=function, a=a, b=b):
                            want = a if taken(a, b) else (a + 1) & 0xFFFFFFFF
                            self.assertEqual(run_mips(offset, words, (a, b)), want)

    def test_labels_forward_backward_and_numeric(self):
        # Trap: labels. Template labels become numeric locals (`1:`, `1f`, `2b`), so the
        # same template can be inlined twice without duplicate-symbol errors.
        text = calls_in(build_ir("regress")[1], "two_labels")[0][1]
        self.assertEqual(text, r"\09move $0, $1\0A\09beqz $1, 1f\0A2:\0A\09addiu $0, $0, -1\0A\09bnez $0, 2b\0A1:")
        require_o64_objects(self)
        for target in TARGETS:
            offset, words = elf32be_symbols(build_object("regress", target))["two_labels"]
            self.assertTrue(find_sequence(words, [0x00801025, 0x10800004, 0, 0x2442FFFF, 0x1440FFFE, 0]))
            for n in (0, 1, 5):
                self.assertEqual(run_mips(offset, words, (n,)), 0)
            offset, words = elf32be_symbols(build_object("fixture", target))["count_down"]
            self.assertEqual(run_mips(offset, words, (7,)), 0)

    def test_hi_lo_clobbered_after_mult(self):
        # Trap: HI/LO. multu + mflo with no #clobber must still tell LLVM HI and LO change.
        # The body starts with a HI/LO write and ends with a HI/LO read, so the
        # VR4300 mfhi/mflo-then-mult hazard padding adds two nops at each end.
        self.assertEqual(calls_in(build_ir("regress")[1], "mul_lo"),
                         [(False, r"\09nop\0A\09nop\0A\09multu $1, $2\0A\09mflo $0\0A\09nop\0A\09nop",
                           "=&r,r,r,~{hi},~{lo},~{$1}")])

    def test_fcc0_clobbered_by_compare(self):
        # Trap: %fcc0. c.olt.d writes the FP condition bit; bc1f reads it.
        side, text, constraints = calls_in(build_ir("regress")[1], "min_f64")[0]
        self.assertEqual(constraints, "=&f,f,f,~{$fcc0},~{$1}")
        self.assertIn(r"c.olt.d $2, $1\0A\09bc1f 1f", text)

    def test_cop0_clobber_lowers_to_memory(self):
        # Trap: LLVM has no constraint for COP0 registers; `#clobber %c0_status` is a memory clobber.
        self.assertEqual(calls_in(build_ir("regress")[1], "cop0_clobber_only"),
                         [(False, r"\09addu $0, $1, $1", "=r,r,~{memory},~{$1}")])
        self.assertEqual(calls_in(build_ir("regress")[1], "plain_alu"),
                         [(False, r"\09addu $0, $1, $1", "=r,r,~{$1}")])

    def test_jalr_is_early_clobber_and_clobbers_memory(self):
        # Trap: `jalr rd, rs` with rd == rs is UNPREDICTABLE; the callee may touch any memory.
        self.assertEqual(calls_in(build_ir("regress")[1], "call_through_register"),
                         [(True, r"\09jalr $0, $1", "=&r,r,~{memory},~{$1}")])
        require_o64_objects(self)
        for target in TARGETS:
            words = object_functions("regress", target)["call_through_register"]
            jalr = [w for w in words if w & 0xFC1F07FF == 0x00000009]
            self.assertEqual(len(jalr), 1)
            self.assertNotEqual((jalr[0] >> 21) & 31, (jalr[0] >> 11) & 31, "jalr rd == rs")


# Checker rejections in bad_check/bad_check.odin: template name -> message (verbatim).
CHECK_ERRORS = {
    "addiu_imm_wide":      "'addiu' operand-2 immediate 40000 does not fit in a signed 16-bit immediate (-32768..=32767)",
    "addiu_imm_max_plus1": "'addiu' operand-2 immediate 32768 does not fit in a signed 16-bit immediate (-32768..=32767)",
    "addiu_imm_min_less1": "'addiu' operand-2 immediate -32769 does not fit in a signed 16-bit immediate (-32768..=32767)",
    "slti_imm_unsigned":   "'slti' operand-2 immediate 65535 does not fit in a signed 16-bit immediate (-32768..=32767)",
    "andi_imm_negative":   "'andi' operand-2 immediate -1 does not fit in an unsigned 16-bit immediate (0..=65535)",
    "andi_imm_max_plus1":  "'andi' operand-2 immediate 65536 does not fit in an unsigned 16-bit immediate (0..=65535)",
    "lui_imm_negative":    "'lui' operand-1 immediate -1 does not fit in an unsigned 16-bit immediate (0..=65535)",
    "lui_imm_max_plus1":   "'lui' operand-1 immediate 65536 does not fit in an unsigned 16-bit immediate (0..=65535)",
    "sll_amount_32":       "'sll' operand-2 immediate 32 does not fit in an unsigned 5-bit immediate (0..=31)",
    "sll_amount_negative": "'sll' operand-2 immediate -1 does not fit in an unsigned 5-bit immediate (0..=31)",
    "cache_op_wide":       "'cache' operand-0 immediate 32 does not fit in an unsigned 5-bit immediate (0..=31)",
    "cache_op_l2":         "'cache' operand-0 immediate 2 is not a VR4300 cache operation (valid: 0x00 0x04 0x08 0x10 0x14 0x18 for the I-cache, 0x01 0x05 0x09 0x0D 0x11 0x15 0x19 for the D-cache)",
    "cache_op_icache_cde": "'cache' operand-0 immediate 12 is not a VR4300 cache operation (valid: 0x00 0x04 0x08 0x10 0x14 0x18 for the I-cache, 0x01 0x05 0x09 0x0D 0x11 0x15 0x19 for the D-cache)",
    "syscall_code_wide":   "'syscall' operand-0 immediate 1048576 does not fit in an unsigned 20-bit immediate (0..=1048575)",
    "break_code_wide":     "'break' operand-0 immediate 1024 does not fit in an unsigned 10-bit immediate (0..=1023)",
    "teq_code_wide":       "'teq' operand-2 immediate 1024 does not fit in an unsigned 10-bit immediate (0..=1023)",
    "li_too_large":        "'li' operand-1 immediate 4294967296 does not fit in the immediate range -2147483648..=4294967295",
    "li_too_small":        "'li' operand-1 immediate -2147483649 does not fit in the immediate range -2147483648..=4294967295",
    "lw_disp_wide":        "'lw' operand-1 memory displacement 40000 does not fit in a signed 16-bit offset (-32768..=32767)",
    "sw_disp_wide":        "'sw' operand-1 memory displacement 40000 does not fit in a signed 16-bit offset (-32768..=32767)",
    "lw_disp_negative":    "'lw' operand-1 memory displacement -32769 does not fit in a signed 16-bit offset (-32768..=32767)",
    "lw_disp_max_plus1":   "'lw' operand-1 memory displacement 32768 does not fit in a signed 16-bit offset (-32768..=32767)",
    "sw_disp_max_plus1":   "'sw' operand-1 memory displacement 32768 does not fit in a signed 16-bit offset (-32768..=32767)",
    "sw_disp_min_less1":   "'sw' operand-1 memory displacement -32769 does not fit in a signed 16-bit offset (-32768..=32767)",
    "gpr_in_cp0_slot":     "'mtc0' operand-1 must be a named COP0 register, got %t0",
    "param_in_cp0_slot":   "'mfc0' operand-1 must be a named COP0 register, got the parameter 'v', which the compiler allocates",
    "f32_to_mtc0":         "'mtc0' operand-0 is in the wrong register class, expected 32-bit integer register, got 32-bit float register",
    "unknown_mnemonic":    "Unknown mnemonic for this target platform: frobnicate",
    "mnemonic_typo":       "Unknown mnemonic for this target platform: addui",
    "unknown_cp0":         "Unknown register for this target platform: %c0_bogus",
    "cp0_typo":            "Unknown register for this target platform: %c0_cnt",
    "at_operand":          "Register %at ($1) is reserved for the assembler, which uses it to expand macros",
    "at_pin":              "Register %at ($1) is reserved for the assembler, which uses it to expand macros",
    "at_clobber":          "Register %at ($1) is reserved for the assembler, which uses it to expand macros",
    "absolute_j":          "'j' operand-0 has an invalid kind, expected a label operand",
    "absolute_jal":        "'jal' operand-0 has an invalid kind, expected a label operand",
    "numeric_branch":      "'beq' operand-2 has an invalid kind, expected a label operand",
    "undeclared_label":    "Undeclared asm label '.nowhere'",
    "duplicate_label":     "Redeclaration of the label 'l' in this scope",
    "jr_not_diverging":    "This 'asm' template has no reachable path that returns or falls through the end; if this is intended, declare it diverging (-> !)",
    "u64_in_gpr32_slot":   "'addu' operand-1 has the wrong size: expected a 32-bit integer operand, got 64-bit",
    "f32_in_gpr_slot":     "'addu' operand-2 is in the wrong register class, expected 32-bit integer register, got 32-bit float register",
    "gpr_in_fpr_slot":     "'add_s' operand-2 is in the wrong register class, expected 32-bit float register, got 32-bit integer register",
    "f64_in_single_slot":  "'add_s' operand-1 has the wrong size: expected a 32-bit float operand, got 64-bit",
    "f32_in_double_slot":  "'add_d' operand-1 has the wrong size: expected a 64-bit float operand, got 32-bit",
    "f64_in_word_slot":    "'cvt_d_w' operand-1 has the wrong size: expected a 32-bit float operand, got 64-bit",
    "too_many_operands":   "The asm instruction 'addu' expects 3 operands, got 4",
    "too_few_operands":    "The asm instruction 'addu' expects 3 operands, got 2",
    "literal_write":       "'addu' writes %t0, which is not pinned to an output or scratch parameter; add '#clobber %t0' so the compiler does not keep a live value in it",
    "write_untied_input":  "'addu' writes %a0, which holds the input 'a'; the compiler assumes an input keeps its value, so tie 'a' to an output or copy it into a scratch register before writing",
    "mflo_unproduced":     "'mflo' implicitly reads %lo, but nothing in this template produces a value for it on all paths reaching here; pin an input to %lo, or write %lo first",
    "mfhi_after_mtlo":     "'mfhi' implicitly reads %hi, but nothing in this template produces a value for it on all paths reaching here; pin an input to %hi, or write %hi first",
    # No hint to pin an input: %fcc0 cannot be pinned.
    "bc1t_unproduced":     "'bc1t' implicitly reads %fcc0, but nothing in this template produces a value for it on all paths reaching here; write %fcc0 first",
    "output_pin_zero":     "'asm' output 'r' is pinned to a register '%zero' which cannot be an output; copy it into the output in the body instead",
    "output_pin_k0":       "'asm' output 'r' is pinned to a register '%k0' which cannot be an output; copy it into the output in the body instead",
    "output_pin_gp":       "'asm' output 'r' is pinned to a register '%gp' which cannot be an output; copy it into the output in the body instead",
    "output_pin_sp":       "'asm' output 'r' is pinned to a register '%sp' which cannot be an output; copy it into the output in the body instead",
    "f32_pinned_to_gpr":   "Parameter 'a' is pinned to %a0, but its type is in the wrong register class for that register",
}

# Wrong-class literals and pins in bad_backend/bad_backend.odin. `odin check` reports
# them; the generator keeps the same checks as a backstop.
BACKEND_ERRORS = {
    "pin_to_cop0":      "'asm' parameter 'x' cannot be pinned to %c0_status: LLVM has no register constraint for it; move the value with 'mtc0'/'mfc0' or 'ctc1'/'cfc1' in the template",
    "pin_to_fcc0":      "'asm' parameter 'x' cannot be pinned to %fcc0: LLVM cannot copy a value into or out of the FP condition bit; use a 'c_*' compare or 'bc1t'/'bc1f' in the template",
    "cop0_in_gpr_slot": "'addu' operand-2 cannot be %c0_status, it must be a general-purpose register",
    "hi_in_gpr_slot":   "'addu' operand-2 cannot be %hi, it must be a general-purpose register",
    "fcc0_in_gpr_slot": "'addu' operand-2 cannot be %fcc0, it must be a general-purpose register",
    "fcr_in_gpr_slot":  "'addu' operand-2 cannot be %fcr31, it must be a general-purpose register",
}


class Diagnostics(unittest.TestCase):
    """Rejected templates, with the message on the line of the template that caused it."""

    def assert_errors_by_template(self, path, output, expected):
        lines = template_lines(path)
        found = diagnostics(output)
        for template, message in expected.items():
            with self.subTest(template=template):
                at_line = [m for line, m, _ in found if line == lines[template]]
                self.assertTrue(any(m.startswith(message) for m in at_line), f"got {at_line}")
        # every error belongs to an expected template: nothing else in the package fails
        expected_lines = {lines[t] for t in expected}
        self.assertEqual([(l, m) for l, m, _ in found if l not in expected_lines], [])

    def test_original_bad_package(self):
        code, output = run_odin("check", HERE / "bad", "-target:n64", "-no-entry-point")
        self.assertNotEqual(code, 0)
        for message in ("'cache' operand-0 immediate 32 does not fit in an unsigned 5-bit immediate (0..=31)",
                        "Unknown register for this target platform: %c0_bogus",
                        "'mtc0' operand-1 must be a named COP0 register",
                        "Unknown mnemonic for this target platform: frobnicate",
                        "'mtc0' operand-0 is in the wrong register class"):
            self.assertIn(message, output)

    def test_checker_rejections(self):
        code, output = check_package("bad_check")
        self.assertNotEqual(code, 0)
        self.assert_errors_by_template(HERE / "bad_check" / "bad_check.odin", output, CHECK_ERRORS)

    def test_range_edges_are_accepted(self):
        # Every bound is inclusive: regress/ uses each kind's exact edges, and
        # bad_check/ rejects one past each, so a bound off by one either way fails.
        ir = build_ir("regress")[1]
        self.assertEqual(calls_in(ir, "range_edges"), [(False,
            r"\09addiu $0, $1, 32767\0A\09addiu $0, $0, -32768\0A\09andi $0, $0, 65535\0A\09andi $0, $0, 0"
            r"\0A\09sll $0, $0, 31\0A\09sll $0, $0, 0\0A\09lw $0, 32767($2)\0A\09lw $0, -32768($2)"
            r"\0A\09lui $0, 65535", "=&r,r,r,~{memory},~{$1}")])
        self.assertEqual(calls_in(ir, "trap_code_edges"),
                         [(True, r"\09break 1023\0A\09teq $0, $$0, 1023\0A\09syscall 1048575", "r,~{$1}")])
        require_o64_objects(self)
        fields = {
            "range_edges": [(0xFC00FFFF, 0x24007FFF), (0xFC00FFFF, 0x24008000),  # addiu +32767, -32768
                            (0xFC00FFFF, 0x3000FFFF), (0xFC00FFFF, 0x30000000),  # andi 65535, 0
                            (0xFFE007FF, 0x000007C0),                            # sll 31
                            (0xFC00FFFF, 0x8C007FFF), (0xFC00FFFF, 0x8C008000),  # lw +32767, -32768
                            (0xFFE0FFFF, 0x3C00FFFF)],                           # lui 65535
            "trap_code_edges": [(0xFFFFFFFF, 0x03FF000D), (0xFC00FFFF, 0x0000FFF4),  # break, teq 1023
                                (0xFFFFFFFF, 0x03FFFFCC)],                       # syscall 1048575
        }
        for target in TARGETS:
            functions = object_functions("regress", target)
            for function, expected in fields.items():
                for mask, bits in expected:
                    with self.subTest(target=target, function=function, bits=f"{bits:08x}"):
                        self.assertTrue(any(w & mask == bits for w in functions[function]),
                                        [f"{w:08x}" for w in functions[function]])

    def test_did_you_mean_suggestions(self):
        _, output = check_package("bad_check")
        lines = template_lines(HERE / "bad_check" / "bad_check.odin")
        details = {line: detail for line, _, detail in diagnostics(output)}
        for template, suggestion in (("mnemonic_typo", "addiu"), ("cp0_typo", "%c0_count")):
            with self.subTest(template=template):
                detail = details[lines[template]]
                self.assertIn("Suggestion: Did you mean?", detail)
                self.assertIn(suggestion, detail)

    def test_feature_gated_mnemonics_are_rejected(self):
        code, output = check_package("bad_features")
        self.assertNotEqual(code, 0)
        _, gated = table_mnemonics()
        gated = dict(gated, mfc0_select="mips32", mtc0_select="mips32")
        expected = {}
        for mnemonic, feature in gated.items():
            name = mnemonic.replace("_select", "")
            expected[f"g_{mnemonic}"] = (
                f"'{name}' requires the target feature '{feature}', which is not enabled; enable it on this "
                f"'asm' template (e.g. @(enable_target_feature=\"{feature}\")), or globally via "
                f"'-target-features:\"{feature}\"' or a matching micro-architecture")
        self.assert_errors_by_template(HERE / "bad_features" / "bad_features.odin", output, expected)

    def test_backend_rejections(self):
        # These used to pass `odin check` and fail only in the LLVM generator. The table's
        # literal_register_reject_reason / pin_reject_reason hooks now report them at check
        # time, with the same wording the generator keeps as a backstop.
        code, output = check_package("bad_backend")
        self.assertNotEqual(code, 0)
        self.assert_errors_by_template(HERE / "bad_backend" / "bad_backend.odin", output, BACKEND_ERRORS)


EXT_TEMPLATE = '''
ext4 :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] { ext r, a, 0, 4 }
@(export) use :: proc "contextless" (a: u32) -> u32 { return ext4(a) }
'''
EXT_WORD = 0x7C821800  # ext v0, a0, 0, 4


class TargetFeatures(unittest.TestCase):
    """Opting in to an instruction the VR4300 lacks."""

    def test_global_target_feature_enables_the_mnemonic(self):
        directory = snippet(EXT_TEMPLATE, "global-feature")
        out = directory / "out.o"
        code, output = run_odin("build", directory, "-target:n64", "-build-mode:obj", *OBJ_FLAGS,
                                "-target-features:mips32r2", f"-out:{out}")
        self.assertEqual(code, 0, output)
        require_o64_objects(self)
        self.assertIn(EXT_WORD, elf32be_functions(out)["use"])

    def test_caller_feature_does_not_cover_the_template(self):
        directory = snippet('''
ext4 :: asm(a: u32) -> (r: u32) { ext r, a, 0, 4 }
@(export, enable_target_feature="mips32r2") use :: proc "contextless" (a: u32) -> u32 { return ext4(a) }
''', "caller-feature")
        code, output = run_odin("check", directory, "-target:n64", "-no-entry-point")
        self.assertNotEqual(code, 0)
        self.assertIn("'ext' requires the target feature 'mips32r2', which is not enabled", output)

    def test_template_feature_reaches_llvm(self):
        # @(enable_target_feature="mips32r2") on the template satisfies the checker, but
        # the asm is assembled inside the caller, and LLVM's assembler checks `ext`
        # against the caller's features. A caller without the feature is now rejected at
        # the call site; it used to reach LLVM, which dropped the template.
        directory = snippet("@(enable_target_feature=\"mips32r2\")" + EXT_TEMPLATE, "template-feature")
        out = directory / "out.o"
        code, output = run_odin("build", directory, "-target:n64", "-build-mode:obj", *OBJ_FLAGS, f"-out:{out}")
        checker_rejected = code != 0 and "LLVM Error" not in output
        if code == 0:
            require_o64_objects(self)
        encoded = code == 0 and EXT_WORD in elf32be_functions(out)["use"]
        self.assertTrue(checker_rejected or encoded, output)
        self.assertIn("'asm' template 'ext4' enables target feature 'mips32r2', but the calling procedure "
                      "does not", output)

    def test_template_and_caller_feature_reach_llvm(self):
        directory = snippet('''
@(enable_target_feature="mips32r2")
ext4 :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] { ext r, a, 0, 4 }
@(export, enable_target_feature="mips32r2") use :: proc "contextless" (a: u32) -> u32 { return ext4(a) }
''', "template-and-caller-feature")
        out = directory / "out.o"
        code, output = run_odin("build", directory, "-target:n64", "-build-mode:obj", *OBJ_FLAGS, f"-out:{out}")
        self.assertEqual(code, 0, output)
        require_o64_objects(self)
        self.assertIn(EXT_WORD, elf32be_functions(out)["use"])

    def test_llvm_asm_error_fails_build(self):
        # An error from LLVM's inline-asm parser is printed ("LLVM Error: <inline
        # asm>...") and recorded by lb_llvm_diagnostic_handler. The build must fail
        # rather than write an object without the template, as it used to. Reproduced
        # with a MIPS IV `movz` the build enables but the caller turns off, which the
        # checker allows.
        directory = snippet('''
pick :: asm(a: u64, b: u64, c: u64) -> (r: u64) { move r, a; movz r, b, c }
@(export, enable_target_feature="-mips4") use :: proc "contextless" (a, b, c: u64) -> u64 { return pick(a, b, c) }
''', "llvm-asm-error")
        code, output = run_odin("build", directory, "-target:n64", "-build-mode:obj", *OBJ_FLAGS,
                                "-target-features:mips4", f"-out:{directory / 'out.o'}")
        self.assertIn("LLVM Error", output)
        self.assertNotEqual(code, 0)


class KnownBugs(unittest.TestCase):
    """Compiler bugs found by this suite. Each test asserts the correct behaviour."""

    def test_literal_fpr_is_a_float_register(self):
        # BUG: check_register types a literal register by width only, so %f4 (64-bit)
        # becomes a u64: "'add_d' operand-0 is in the wrong register class, expected
        # 64-bit float register, got 64-bit integer register". No literal FPR can be
        # written in any FPU slot, with or without #clobber.
        directory = snippet('''
t :: asm(a: f64, b: f64) -> (r: f64) [#clobber %f4] { add_d %f4, a, b; mov_d r, %f4 }
@(export) use :: proc "contextless" (a, b: f64) -> f64 { return t(a, b) }
''', "literal-fpr")
        code, output = run_odin("check", directory, "-target:n64", "-no-entry-point")
        self.assertEqual(code, 0, output)

    def test_literal_fpr_write_needs_a_clobber(self):
        # BUG (same root cause): an undeclared literal FPR write should get the
        # register_write_needs_clobber diagnostic, not a register-class error.
        directory = snippet('''
t :: asm(a: f32, b: f32) { add_s %f4, a, b }
@(export) use :: proc "contextless" (a, b: f32) { t(a, b) }
''', "literal-fpr-clobber")
        _, output = run_odin("check", directory, "-target:n64", "-no-entry-point")
        self.assertIn("'add_s' writes %f4, which is not pinned to an output or scratch parameter; "
                      "add '#clobber %f4' so the compiler does not keep a live value in it", output)

    def test_fpr_pins_become_numeric_constraints(self):
        # BUG (same root cause): pinning an f64 to %f12 fails with "Parameter 'a' is
        # pinned to %f12, but its type is in the wrong register class for that register".
        directory = snippet('''
t :: asm(a: f64) -> (r: f64) [a = %f12, r = %f0] { mov_d r, a }
@(export) use :: proc "contextless" (a: f64) -> f64 { return t(a) }
''', "fpr-pin")
        code, output = run_odin("build", directory, "-target:n64", "-build-mode:llvm-ir", *OBJ_FLAGS,
                                f"-out:{directory}")
        self.assertEqual(code, 0, output)
        ir = "\n".join(p.read_text() for p in directory.glob("*.ll"))
        self.assertIn("={$f0},{$f12},~{$1}", ir)

    def test_narrow_pinned_operand_does_not_abort_llvm(self):
        # A u8/u16 parameter pinned to a GPR used to become an i8 `{$4}` operand, and
        # LLVM aborted: "Assertion failed: (RC && \"This value type is not natively
        # supported!\"), function getRegClassFor". A pinned operand narrower than 32
        # bits now crosses the asm boundary as an i32. Unpinned u8 operands always worked.
        directory = snippet('''
t :: asm(x: u8) -> (r: u8) [x = %a0] { move r, x }
@(export) use :: proc "contextless" (x: u8) -> u8 { return t(x) }
''', "narrow-pin")
        code, output = run_odin("build", directory, "-target:n64", "-build-mode:obj", *OBJ_FLAGS,
                                f"-out:{directory / 'out.o'}")
        self.assertEqual(code, 0, output)
        self.assertNotIn("Assertion failed", output)

    def test_hi_lo_pin_counts_as_a_use_under_vet(self):
        # MIPS_ASM.md says "pin an input to %hi or %lo to supply a value from outside".
        # -vet used to report "'asm' input parameter 'v' is declared but never used"
        # because the implicit read by mflo was not counted as a use.
        directory = snippet('''
t :: asm(v: u32) -> (r: u32) [v = %lo] { mflo r }
@(export) use :: proc "contextless" (v: u32) -> u32 { return t(v) }
''', "lo-pin-vet")
        code, output = run_odin("check", directory, "-target:n64", "-no-entry-point", "-vet")
        self.assertEqual(code, 0, output)


# --------------------------------------------------------------------------
# Operand widths at the asm boundary (widths/): a 64-bit MIPS III interpreter
# --------------------------------------------------------------------------

MASK32 = (1 << 32) - 1
HILO_READ_FUNCTS = {0x10, 0x12}                                        # mfhi, mflo
HILO_WRITE_FUNCTS = {0x11, 0x13, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F}  # mthi, mtlo, (d)mult(u), (d)div(u)


def run_mips64(offset, words, args, memory=None, max_steps=10_000):
    """Execute a function's words and return v0 as all 64 bits.

    Unlike run_mips, arguments are 64-bit register images (the caller applies the
    o64 sign/zero extension), loads and stores use a byte-addressed big-endian
    `memory` dict, and HI/LO, doubleword shifts and multiplies are modelled, so a
    value whose upper half disagrees with bit 31 is visible.
    """
    regs = [0] * 32
    for i, value in enumerate(args):
        regs[4 + i] = value & MASK64
    regs[29], regs[31] = 0x8000, RETURN_SENTINEL
    memory = dict(memory or {})
    hi = lo = 0
    index_of = lambda address: (address - offset) // 4

    def load(address, size, signed):
        value = 0
        for k in range(size):
            if address + k not in memory:
                raise AssertionError(f"read of unset memory at {address + k:#x}")
            value = value << 8 | memory[address + k]
        return sext(value, size * 8) if signed else value

    def store(address, size, value):
        for k in range(size):
            memory[address + k] = (value >> (8 * (size - 1 - k))) & 0xFF

    pc, pending = 0, None
    for _ in range(max_steps):
        word = words[pc]
        op, rs, rt, rd = word >> 26, (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
        sa, fn, imm = (word >> 6) & 31, word & 63, sext(word & 0xFFFF, 16)
        s, t = regs[rs], regs[rt]
        address = (s + imm) & MASK64
        branch, likely = None, False
        if op == 0:
            if   fn == 0x00: regs[rd] = sext(t << sa, 32)                                    # sll / nop
            elif fn == 0x02: regs[rd] = sext((t & MASK32) >> sa, 32)                         # srl
            elif fn == 0x03: regs[rd] = sext(signed64(sext(t, 32)) >> sa, 32)                # sra
            elif fn == 0x08: branch = s                                                      # jr
            elif fn == 0x10: regs[rd] = hi                                                   # mfhi
            elif fn == 0x11: hi = s                                                          # mthi
            elif fn == 0x12: regs[rd] = lo                                                   # mflo
            elif fn == 0x13: lo = s                                                          # mtlo
            elif fn in (0x18, 0x19):                                                         # mult, multu
                a, b = (signed64(sext(s, 32)), signed64(sext(t, 32))) if fn == 0x18 else (s & MASK32, t & MASK32)
                product = a * b
                lo, hi = sext(product, 32), sext(product >> 32, 32)
            elif fn in (0x1C, 0x1D):                                                         # dmult, dmultu
                product = signed64(s) * signed64(t) if fn == 0x1C else s * t
                lo, hi = product & MASK64, (product >> 64) & MASK64
            elif fn == 0x21: regs[rd] = sext(s + t, 32)                                      # addu
            elif fn == 0x23: regs[rd] = sext(s - t, 32)                                      # subu
            elif fn == 0x24: regs[rd] = s & t                                                # and
            elif fn == 0x25: regs[rd] = s | t                                                # or / move
            elif fn == 0x26: regs[rd] = s ^ t                                                # xor
            elif fn == 0x27: regs[rd] = ~(s | t) & MASK64                                    # nor
            elif fn == 0x2A: regs[rd] = int(signed64(s) < signed64(t))                       # slt
            elif fn == 0x2B: regs[rd] = int(s < t)                                           # sltu
            elif fn == 0x2D: regs[rd] = (s + t) & MASK64                                     # daddu
            elif fn == 0x2F: regs[rd] = (s - t) & MASK64                                     # dsubu
            elif fn == 0x38: regs[rd] = (t << sa) & MASK64                                   # dsll
            elif fn == 0x3A: regs[rd] = t >> sa                                              # dsrl
            elif fn == 0x3B: regs[rd] = (signed64(t) >> sa) & MASK64                         # dsra
            elif fn == 0x3C: regs[rd] = (t << (sa + 32)) & MASK64                            # dsll32
            elif fn == 0x3E: regs[rd] = t >> (sa + 32)                                       # dsrl32
            elif fn == 0x3F: regs[rd] = (signed64(t) >> (sa + 32)) & MASK64                  # dsra32
            else: raise NotImplementedError(f"SPECIAL funct {fn:#x} in {word:08x}")
        elif op & ~0x10 in (0x04, 0x05, 0x06, 0x07):
            taken = {0x04: s == t, 0x05: s != t, 0x06: signed64(s) <= 0, 0x07: signed64(s) > 0}[op & ~0x10]
            likely = bool(op & 0x10)
            branch = offset + (pc + 1 + signed64(imm)) * 4 if taken else None
        elif op == 0x02: branch = (word & 0x03FFFFFF) << 2                                   # j
        elif op == 0x09: regs[rt] = sext(s + imm, 32)                                        # addiu
        elif op == 0x0A: regs[rt] = int(signed64(s) < signed64(imm))                         # slti
        elif op == 0x0B: regs[rt] = int(s < imm)                                             # sltiu
        elif op == 0x0C: regs[rt] = s & (word & 0xFFFF)                                      # andi
        elif op == 0x0D: regs[rt] = s | (word & 0xFFFF)                                      # ori
        elif op == 0x0E: regs[rt] = s ^ (word & 0xFFFF)                                      # xori
        elif op == 0x0F: regs[rt] = sext((word & 0xFFFF) << 16, 32)                          # lui
        elif op == 0x19: regs[rt] = (s + imm) & MASK64                                       # daddiu
        elif op in (0x20, 0x21, 0x23, 0x24, 0x25, 0x27, 0x37):                               # lb lh lw lbu lhu lwu ld
            size, signed = {0x20: (1, True), 0x21: (2, True), 0x23: (4, True), 0x24: (1, False),
                            0x25: (2, False), 0x27: (4, False), 0x37: (8, False)}[op]
            regs[rt] = load(address, size, signed)
        elif op in (0x28, 0x29, 0x2B, 0x3F):                                                 # sb sh sw sd
            store(address, {0x28: 1, 0x29: 2, 0x2B: 4, 0x3F: 8}[op], t)
        else:
            raise NotImplementedError(f"opcode {op:#x} in {word:08x}")
        regs[0] = 0
        if pending is not None:
            if branch is not None:
                raise AssertionError(f"branch in a delay slot: {word:08x}")
            if pending == RETURN_SENTINEL:
                return regs[2]
            pc, pending = index_of(pending), None
        elif branch is not None:
            pending, pc = branch, pc + 1
        elif likely:
            pc += 2
        else:
            pc += 1
    raise AssertionError("function did not return")


def big_endian_bytes(address, value, size):
    return {address + k: (value >> (8 * (size - 1 - k))) & 0xFF for k in range(size)}


ASM_SIGNATURE = re.compile(r'call (\{[^}]*\}|\w+) asm (?:sideeffect )?"((?:[^"\\]|\\.)*)", "([^"]*)"\(([^)]*)\)')


def function_ir(ir, function):
    """The body of `define ... @function(` in `ir`."""
    m = re.search(rf'^define [^@]*@"?{re.escape(function)}"?\(.*?^}}', ir, re.M | re.S)
    if m is None:
        raise AssertionError(f"no IR for {function}")
    return m.group(0)


def asm_signatures(ir, function):
    """[(result type, text, constraints, [argument types])] for the asm calls in `function`."""
    return [(m.group(1), m.group(2), m.group(3), [a.split()[0] for a in m.group(4).split(",") if a.strip()])
            for m in ASM_SIGNATURE.finditer(function_ir(ir, function))]


def hilo_hazards(words):
    """Indices of mfhi/mflo words followed within two words by a HI/LO write."""
    special = lambda w, functs: w >> 26 == 0 and (w & 63) in functs and w != 0
    return [i for i, w in enumerate(words) if special(w, HILO_READ_FUNCTS)
            and any(special(n, HILO_WRITE_FUNCTS) for n in words[i + 1:i + 3])]


class CodegenWidths(unittest.TestCase):
    """Operand widths at the asm boundary and HI/LO hazard padding (widths/)."""

    @classmethod
    def setUpClass(cls):
        cls.ir = build_ir("widths")[1]
        # Object words need the O64 fork (ELF32); the IR assertions run on any compiler.
        cls.symbols = elf32be_symbols(build_object("widths", "n64")) if emits_elf32_objects() else None

    def run_fn(self, name, *args, memory=None):
        require_o64_objects(self)
        offset, words = self.symbols[name]
        return run_mips64(offset, words, args, memory)

    def test_narrow_inputs_cross_as_i32(self):
        for fn in ("in_u8", "in_u8_pinned", "in_u8_tied", "in_u8_tied_pinned",
                   "in_i8", "in_i8_pinned", "in_i16", "in_bool"):
            with self.subTest(fn):
                (result, _, _, arguments), = asm_signatures(self.ir, fn)
                self.assertEqual((result, arguments), ("i32", ["i32"]))
        self.assertRegex(function_ir(self.ir, "in_u8"), r"zext i8 %\d+ to i32")
        self.assertRegex(function_ir(self.ir, "in_i8"), r"sext i8 %\d+ to i32")
        self.assertRegex(function_ir(self.ir, "in_i16"), r"sext i16 %\d+ to i32")
        # mips3 has no seb/seh: the sign extension is sll + sra.
        require_o64_objects(self)
        for fn in ("in_i8", "in_i8_pinned", "in_i16"):
            self.assertFalse(any(w & 0xFC0007FF in (0x7C000420, 0x7C000620) for w in self.symbols[fn][1]), fn)

    def test_narrow_inputs_are_extended(self):
        # 200 + 200 wraps to 144 in a u8; 100 + 100 to -56 in an i8; 20000 + 20000 to -25536 in an i16.
        cases = {
            ("in_u8", 200, 200): 1, ("in_u8_pinned", 200, 200): 1,
            ("in_u8_tied", 200, 200): 144, ("in_u8_tied_pinned", 200, 200): 144,
            ("in_i8", 100, 100): sext(-56, 64), ("in_i8_pinned", 100, 100): sext(-56, 64),
            ("in_i16", 20000, 20000): sext(-25536, 64),
            ("in_bool", 5, 5): 1, ("in_bool", 5, 6): 0,
        }
        for (fn, *args), want in cases.items():
            with self.subTest(fn, args=args):
                self.assertEqual(self.run_fn(fn, *args), want)

    def test_narrow_outputs_are_truncated(self):
        # The truncation itself is folded by the optimiser (and/shl+ashr); the runs prove it.
        for fn in ("out_u8", "out_i16", "out_bool"):
            with self.subTest(fn):
                (result, _, _, _), = asm_signatures(self.ir, fn)
                self.assertEqual(result, "i32")
        self.assertEqual(self.run_fn("out_u8", 255), 0)
        self.assertEqual(self.run_fn("out_i16", 0x18000), sext(-32768, 64))
        self.assertEqual(self.run_fn("out_bool", 0), 7)
        self.assertEqual(self.run_fn("out_bool", 1), 3)

    def test_doubleword_writes_cross_as_i64(self):
        for fn in ("lwu_eq", "ld_eq0", "ld_u8_eq0", "ld_ptr_nil", "move_u64_eq0", "dsll_eq", "dsll_tied_eq",
                   "dmfc0_eq0", "mflo_after_dmultu_eq0", "lo_pin_after_dmultu_eq0"):
            with self.subTest(fn):
                (result, _, _, _), = asm_signatures(self.ir, fn)
                self.assertEqual(result, "i64")
        (result, _, _, _), = asm_signatures(self.ir, "or_after_dsll_eq")
        self.assertEqual(result, "{ i64, i64 }")
        # A tied u32 input reaches a 64-bit output sign-extended, as an untied i32 would.
        (_, _, constraints, arguments), = asm_signatures(self.ir, "dsll_tied_eq")
        self.assertEqual((constraints, arguments), ("=r,0,~{$1}", ["i64"]))
        self.assertRegex(function_ir(self.ir, "dsll_tied_eq"), r"sext i32 %\d+ to i64")

    def test_doubleword_writes_compare_as_words(self):
        p = 0x1000
        self.assertEqual(self.run_fn("lwu_eq", p, memory=big_endian_bytes(p, 0x80000000, 4)), 1)
        self.assertEqual(self.run_fn("ld_eq0", p, memory=big_endian_bytes(p, 1 << 32, 8)), 1)
        self.assertEqual(self.run_fn("ld_u8_eq0", p, memory=big_endian_bytes(p, 0xFFFFFFFF_FFFFFF00, 8)), 1)
        self.assertEqual(self.run_fn("ld_ptr_nil", p, memory=big_endian_bytes(p, 1 << 32, 8)), 1)
        self.assertEqual(self.run_fn("move_u64_eq0", 1 << 32), 1)
        self.assertEqual(self.run_fn("move_u64_eq0", (1 << 32) | 5), 0)
        for fn in ("dsll_eq", "dsll_tied_eq", "or_after_dsll_eq"):
            self.assertEqual(self.run_fn(fn, 0x8000), 1, fn)
        self.assertEqual(self.run_fn("mflo_after_dmultu_eq0", 1 << 32, 1), 1)
        self.assertEqual(self.run_fn("lo_pin_after_dmultu_eq0", 1 << 32, 1), 1)

    def test_word_writes_keep_i32(self):
        # lw, move from a u32, mult + mflo, andi + ori, mfc0: no i64 and no extra sll.
        signatures = asm_signatures(self.ir, "word_writes")
        self.assertEqual(len(signatures), 5)
        self.assertEqual({result for result, _, _, _ in signatures}, {"i32"})
        self.assertNotIn("trunc i64", function_ir(self.ir, "word_writes"))

    def test_hilo_pins_of_64_bit_values_go_through_a_gpr(self):
        # LLVM takes '{hi}'/'{lo}' as 32-bit registers whatever the type.
        (_, text, constraints, _), = asm_signatures(self.ir, "lo_u64_high_half")
        self.assertTrue(constraints.startswith("=r,r,r,"), constraints)
        self.assertTrue(text.endswith(r"\09mflo $0\0A\09nop\0A\09nop"), text)
        (_, text, constraints, _), = asm_signatures(self.ir, "lo_in_u64")
        self.assertEqual(constraints, "=r,r,~{$1},~{lo}")
        self.assertEqual(text, r"\09nop\0A\09nop\0A\09mtlo $1\0A\09mflo $0\0A\09nop\0A\09nop")
        self.assertEqual(self.run_fn("lo_u64_high_half", (1 << 32) | 1, 3), 3)
        self.assertEqual(self.run_fn("lo_in_u64", 0x12345678_9ABCDEF0), 0x12345678_9ABCDEF0)

    def test_hilo_hazard_padding(self):
        nops = r"\09nop\0A\09nop"
        for fn in ("mflo_then_tmpl_multu", "tmpl_mflo_then_mult"):
            (_, text, _, _), = asm_signatures(self.ir, fn)
            self.assertEqual(text, rf"{nops}\0A\09multu $1, $2\0A\09mflo $0\0A{nops}", fn)
        (_, text, _, _), = asm_signatures(self.ir, "hilo_far_from_edges")
        self.assertNotIn("nop", text)
        # bnez + its delay slot already precede the multu; one nop follows mflo + addiu.
        (_, text, _, _), = asm_signatures(self.ir, "hilo_after_branch")
        self.assertTrue(text.startswith(r"\09bnez"), text)
        self.assertTrue(text.endswith(r"\09mflo $0\0A\09addiu $0, $0, 0\0A\09nop"), text)
        require_o64_objects(self)
        for name, (_, words) in self.symbols.items():
            with self.subTest(name):
                self.assertEqual(hilo_hazards(words), [])
        self.assertEqual(self.run_fn("mflo_then_tmpl_multu", 3, 5, 7), 105)
        self.assertEqual(self.run_fn("tmpl_mflo_then_mult", 3, 5, 7), 105)
        self.assertEqual(self.run_fn("hilo_far_from_edges", 2, 3), 26)
        self.assertEqual(self.run_fn("hilo_after_branch", 4, 5), 20)


if __name__ == "__main__":
    unittest.main(verbosity=2)
