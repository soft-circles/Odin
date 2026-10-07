#!/usr/bin/env python3
"""SDK-free checks for CPU asm templates on the N64 (MIPS III, o64) targets."""
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
ODIN = Path(os.environ.get("ODIN", ROOT / "odin")).resolve()
FIXTURE = Path(__file__).parent / "fixture"
BAD = Path(__file__).parent / "bad"
TARGETS = ("n64", "freestanding_mips32be")


def elf32be_functions(path):
    """Map each FUNC symbol in a big-endian ELF32 relocatable to its .text words."""
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
            words = struct.unpack_from(f">{size // 4}I", data, text)
            functions[name(sh[6], st_name)] = words
    return functions


def build(target, mode, directory):
    out = Path(directory) / ("fixture.o" if mode == "obj" else "")
    subprocess.run([str(ODIN), "build", str(FIXTURE), f"-target:{target}", f"-build-mode:{mode}",
                    "-o:speed", "-use-single-module", "-no-entry-point", f"-out:{out}"], check=True)
    return out


class N64AsmTemplates(unittest.TestCase):
    def test_encodings(self):
        # Fixed fields only: the allocator picks the GPRs.
        expect = {
            "data_cache_hit_writeback_invalidate": (0xFC1FFFFF, 0xBC150000),  # cache 0x15, 0(base)
            "inst_cache_hit_invalidate":           (0xFC1FFFFF, 0xBC100000),  # cache 0x10, 0(base)
            "read_cp0_count":                      (0xFFE0FFFF, 0x40004800),  # mfc0 rt, $9
            "write_cp0_compare":                   (0xFFE0FFFF, 0x40805800),  # mtc0 rt, $11
            "load_word_at_offset":                 (0xFC00FFFF, 0x8C000008),  # lw rt, 8(base)
            "add_u64":                             (0xFC0007FF, 0x0000002D),  # daddu
        }
        for target in TARGETS:
            with self.subTest(target=target), tempfile.TemporaryDirectory(prefix="odin-n64-asm-") as directory:
                functions = elf32be_functions(build(target, "obj", directory))
                for function, (mask, bits) in expect.items():
                    with self.subTest(function=function):
                        words = functions[function]
                        self.assertTrue(any(w & mask == bits for w in words),
                                        f"{function}: no {bits:08x}/{mask:08x} in {[f'{w:08x}' for w in words]}")
                # pinned_double reads the pinned input $5 (a1) and writes $8 (t0), then $3 (v1)
                self.assertIn(0x00A54021, functions["pinned_double"])  # addu t0, a1, a1

    def test_constraints(self):
        with tempfile.TemporaryDirectory(prefix="odin-n64-asm-ir-") as directory:
            build("n64", "llvm-ir", directory)
            ir = "\n".join(p.read_text() for p in Path(directory).glob("*.ll"))
            calls = re.findall(r'call [^\n]*asm (sideeffect )?"([^"]*)", "([^"]*)"', ir)
            self.assertTrue(calls)
            for _, _, constraints in calls:
                self.assertIn("~{$1}", constraints)  # $at never holds an operand
            by_text = {text: (side, constraints) for side, text, constraints in calls}
            cache = [v for k, v in by_text.items() if "cache 21" in k]
            self.assertEqual(cache[0], ("sideeffect ", "r,~{memory},~{$1}"))
            self.assertIn(("sideeffect ", "=r,~{memory},~{$1}"), [v for k, v in by_text.items() if "mfc0 $0, $$9" in k])
            self.assertIn(("", "=&{$3},{$5},~{$8},~{$1}"), [v for k, v in by_text.items() if "addu $$8" in k])
            self.assertTrue(any(re.search(r"1:.*bne \$0, \$\$0, 1b", k, re.S) for k in by_text))

    def test_diagnostics(self):
        result = subprocess.run([str(ODIN), "check", str(BAD), "-target:n64", "-no-entry-point"],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        output = re.sub(r"\x1b\[[0-9;]*m", "", result.stdout + result.stderr)
        for message in ("value 32 does not fit in the 5-bit immediate",
                        "Unknown register for this target platform: %c0_bogus",
                        "'mtc0' operand-1 must be a named COP0 register",
                        "Unknown mnemonic for this target platform: frobnicate",
                        "'mtc0' operand-0 is in the wrong register class"):
            self.assertIn(message, output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
