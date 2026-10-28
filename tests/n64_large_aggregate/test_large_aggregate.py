#!/usr/bin/env python3
"""SDK-free check of how the N64 (MIPS O64) ABI passes aggregates wider than the argument registers."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
ODIN = Path(os.environ.get("ODIN", ROOT / "odin")).resolve()
FIXTURE = Path(__file__).parent / "fixture"
# The fixture builds in about a second; before the fix LLVM's MIPS instruction selection ran for many minutes.
BUILD_SECONDS = 120


def function_body(asm, name):
    body = re.search(rf"^{re.escape(name)}:.*?^\s*\.end\s+{re.escape(name)}$", asm, re.M | re.S)
    if body is None:
        raise AssertionError(f"{name} not found in the assembly")
    return body[0]


# O64 passes a struct by value in consecutive 8-byte slots: the first four in $4-$7, the rest in the caller's outgoing
# argument area from 32($sp). For a C callee the caller copies the struct's tail there with memcpy.
BIG_BYTES = 65536
REGISTER_BYTES = 32
CALLEE_SAVED = {f"${n}" for n in (*range(16, 24), 30)} | {"$fp"}
NOT_WRITING = {"sb", "sh", "sw", "sd", "swl", "swr", "sdl", "sdr", "swc1", "sdc1", "j", "jr", "jal", "jalr", "nop"}


def instructions(body):
    result = []
    for line in body.splitlines():
        line = line.split("#")[0].strip()
        if not line or line.startswith(".") or line.endswith(":"):
            continue
        op, _, operands = line.partition("\t") if "\t" in line else line.partition(" ")
        result.append((op, [operand.strip() for operand in operands.split(",")] if operands.strip() else []))
    return result


def written_register(instruction):
    op, operands = instruction
    if op in NOT_WRITING or op.startswith("b") or not operands:
        return None
    return operands[0]


def last_write(code, start, stop, register):
    for index in range(stop - 1, start - 1, -1):
        if written_register(code[index]) == register:
            return index, code[index]
    return None, None


def large_byval_call_problems(body, callee, size=BIG_BYTES):
    """What keeps the call to `callee` in `body` from passing a `size`-byte struct by value as O64 lays it out.

    $4-$7 must be loaded from offsets 0, 8, 16 and 24 of one base register B, and the memcpy just before the call must
    copy `size - 32` bytes from B+32 to 32($sp). B may be $sp only at an offset past the outgoing argument area. A
    branch delay slot counts as part of the call before it.
    """
    code = instructions(body)
    calls = [index for index, (op, _) in enumerate(code) if op in ("jal", "jalr")]
    target = [index for index in calls if code[index][1] == [callee]]
    if len(target) != 1:
        return [f"expected one jal {callee}, found {len(target)}"]
    call = target[0]
    earlier = [index for index in calls if index < call]
    if not earlier or code[earlier[-1]] != ("jal", ["memcpy"]):
        return [f"the call before jal {callee} is not jal memcpy"]
    copy = earlier[-1]
    setup_start = earlier[-2] + 2 if len(earlier) > 1 else 0
    problems = []

    def setup(register, expected, start, stop):
        index, instruction = last_write(code, start, stop, register)
        if instruction is None:
            problems.append(f"{register} is not set before jal {code[stop - 2][1][0]}")
            return None, None
        match = re.fullmatch(expected, "\t".join((instruction[0], ", ".join(instruction[1]))))
        if match is None:
            problems.append(f"{register} is set by `{instruction[0]} {', '.join(instruction[1])}`")
        return index, match

    destination, _ = setup("$4", r"d?addiu\t\$4, \$sp, 32", setup_start, copy + 2)
    setup("$6", rf"(ori|d?addiu)\t\$6, \$zero, {size - REGISTER_BYTES}", setup_start, copy + 2)
    source, source_match = setup("$5", r"d?addiu\t\$5, (\$\w+), (-?\d+)", setup_start, copy + 2)

    loads = {}
    for slot, register in enumerate(("$4", "$5", "$6", "$7")):
        index, match = setup(register, r"ld\t\$\d+, (-?\d+)\((\$\w+)\)", copy + 2, call + 2)
        if match:
            loads[register] = (index, match[2], int(match[1]) - 8 * slot)
    if len(loads) < 4:
        return problems
    bases = {(base, offset) for _, base, offset in loads.values()}
    if len(bases) != 1:
        return problems + [f"$4-$7 are not loaded from consecutive doublewords of one base: {sorted(bases)}"]
    ((base, offset),) = bases
    if base == "$sp":
        if offset < size:
            problems.append(f"$4-$7 are loaded from {offset}($sp), inside the outgoing argument area")
    elif offset != 0:
        problems.append(f"$4-$7 are loaded from offsets {offset}..{offset + 24} of {base}, not 0..24")
    if source_match and (source_match[1], int(source_match[2])) != (base, offset + REGISTER_BYTES):
        problems.append(f"memcpy copies from {source_match[2]}({source_match[1]}), "
                        f"not {offset + REGISTER_BYTES}({base}) where $4-$7 come from")
    if source is not None and base != "$sp":
        if base not in CALLEE_SAVED:
            problems.append(f"{base} does not survive jal memcpy")
        if last_write(code, source + 1, call + 2, base)[1] is not None:
            problems.append(f"{base} changes between the memcpy source and the loads")
    if destination is not None and last_write(code, destination + 1, call + 2, "$sp")[1] is not None:
        problems.append("$sp changes between the memcpy destination and the call")
    return problems


# call_take_big_c and call_take_local_big_c as the O64 LLVM builds them at -o:speed (directives dropped). The first is
# right; the second is soft-circles/Odin64#1071, the same wrong sequence the old pin produced for call_take_big_c.
THROUGH_POINTER = """
	addiu	$sp, $sp, -16
	sd	$ra, 8($sp)
	sd	$16, 0($sp)
	move	$16, $4
	lui	$1, 1
	subu	$sp, $sp, $1
	addiu	$4, $sp, 32
	addiu	$5, $16, 32
	jal	memcpy
	ori	$6, $zero, 65504
	ld	$7, 24($16)
	ld	$6, 16($16)
	ld	$5, 8($16)
	ld	$4, 0($16)
	jal	take_big_c
	nop
"""
FROM_LOCAL = """
	addiu	$16, $sp, 8
	addiu	$sp, $sp, -32
	jal	fill_big
	move	$4, $16
	addiu	$sp, $sp, 32
	addiu	$5, $16, 32
	lui	$1, 1
	subu	$sp, $sp, $1
	ori	$6, $zero, 65504
	jal	memcpy
	addiu	$4, $sp, 32
	ld	$7, 32($sp)
	ld	$6, 24($sp)
	ld	$5, 16($sp)
	jal	take_big_c
	ld	$4, 8($sp)
"""


def replaced(text, *pairs):
    for old, new in pairs:
        assert old in text, old
        text = text.replace(old, new)
    return text


class LargeByvalCallMatcher(unittest.TestCase):
    """The call check rejects wrong code that still looks like a large byval call."""

    def assertRejected(self, body):
        self.assertNotEqual(large_byval_call_problems(body, "take_big_c"), [])

    def test_accepts_the_struct_read_through_its_pointer(self):
        self.assertEqual(large_byval_call_problems(THROUGH_POINTER, "take_big_c"), [])

    def test_accepts_the_registers_loaded_from_a_local_through_a_saved_register(self):
        fixed = replaced(FROM_LOCAL, ("32($sp)\n", "24($16)\n"), ("24($sp)", "16($16)"), ("16($sp)", "8($16)"),
                         ("8($sp)\n", "0($16)\n"))
        self.assertEqual(large_byval_call_problems(fixed, "take_big_c"), [])

    def test_rejects_registers_loaded_from_the_unadjusted_frame(self):
        self.assertRejected(FROM_LOCAL)

    def test_rejects_registers_loaded_from_the_outgoing_argument_area(self):
        self.assertRejected(THROUGH_POINTER.replace("($16)", "($sp)"))

    def test_rejects_registers_loaded_past_the_first_doubleword(self):
        self.assertRejected(replaced(THROUGH_POINTER, ("24($16)", "32($16)"), ("16($16)", "24($16)"),
                                     ("8($16)", "16($16)"), ("0($16)", "8($16)")))

    def test_rejects_registers_loaded_out_of_order(self):
        self.assertRejected(replaced(THROUGH_POINTER, ("$5, 8($16)", "$5, X($16)"), ("$4, 0($16)", "$4, 8($16)"),
                                     ("X(", "0(")))

    def test_rejects_registers_loaded_from_two_bases(self):
        self.assertRejected(replaced(THROUGH_POINTER, ("$7, 24($16)", "$7, 24($17)"), ("$6, 16($16)", "$6, 16($17)")))

    def test_rejects_a_copy_from_another_base(self):
        self.assertRejected(replaced(THROUGH_POINTER, ("$5, $16, 32", "$5, $sp, 32")))

    def test_rejects_a_copy_from_another_offset(self):
        self.assertRejected(replaced(THROUGH_POINTER, ("$5, $16, 32", "$5, $16, 40")))

    def test_rejects_a_copy_to_another_offset(self):
        self.assertRejected(replaced(THROUGH_POINTER, ("$4, $sp, 32", "$4, $sp, 40")))

    def test_rejects_a_short_copy(self):
        self.assertRejected(replaced(THROUGH_POINTER, ("65504", "65496")))

    def test_rejects_a_base_that_memcpy_may_clobber(self):
        self.assertRejected(THROUGH_POINTER.replace("$16", "$8"))

    def test_rejects_registers_set_before_the_copy_only(self):
        early = replaced(THROUGH_POINTER, ("\tld\t$4, 0($16)\n", ""), ("move\t$16, $4", "move\t$16, $4\n\tld\t$4, 0($16)"))
        self.assertRejected(early)


class LargeAggregateArguments(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory(prefix="odin-n64-large-aggregate-") as directory:
            out = Path(directory) / "fixture.S"
            subprocess.run([str(ODIN), "build", str(FIXTURE), "-target:n64", "-o:speed",
                            "-build-mode:asm", "-no-entry-point", f"-out:{out}"],
                           check=True, timeout=BUILD_SECONDS)
            cls.asm = out.read_text()

    def test_odin_procedure_takes_a_pointer_to_a_large_struct(self):
        # The first word is loaded through the pointer in $4 and the last one 65528 bytes past it. LLVM may first
        # re-sign-extend the pointer with `sll`.
        body = function_body(self.asm, "sum_big")
        self.assertRegex(body, r"\bld\s+\$\d+, 0\(\$4\)|\bsll\s+\$\d+, \$4, 0\b")
        self.assertRegex(body, r"\bori\s+\$\d+, \$zero, 65528\b")

    def test_odin_procedure_keeps_a_small_struct_in_registers(self):
        self.assertRegex(function_body(self.asm, "sum_small"), r"\bdaddu\s+\$2, \$4, \$6\b")

    def test_c_procedure_gets_a_large_struct_by_value(self):
        # Which register the four doublewords are loaded through is LLVM's choice: a local temporary before upstream
        # bf9d09619, the caller's own pointer since.
        self.assertEqual(large_byval_call_problems(function_body(self.asm, "call_take_big_c"), "take_big_c"), [])

    # soft-circles/Odin64#1071: LLVM's MIPS backend loads $4-$7 for a byval argument wider than about 32 KB from
    # offsets of $sp it computed before it reserved the outgoing argument area, so the callee gets 32 bytes of that
    # area instead of the struct's first 32. A struct read through a pointer (above) doesn't hit it; one in the
    # caller's frame does. These flip to unexpected successes once LLVM is fixed.
    @unittest.expectedFailure
    def test_c_procedure_gets_a_local_large_struct_by_value(self):
        body = function_body(self.asm, "call_take_local_big_c")
        self.assertEqual(large_byval_call_problems(body, "take_big_c"), [])

    @unittest.expectedFailure
    def test_c_procedure_gets_a_packed_field_large_struct_by_value(self):
        body = function_body(self.asm, "call_take_packed_big_c")
        self.assertEqual(large_byval_call_problems(body, "take_big_c"), [])

    def test_c_struct_spills_past_the_registers_as_gcc_lays_it_out(self):
        # Slots 0-3 in $4-$7, slots 4-5 at 32($sp) and 40($sp), then the i32 right-justified in its slot at 48.
        body = function_body(self.asm, "call_take_spill_c")
        for register in (4, 5, 6, 7):
            self.assertRegex(body, rf"\bld\s+\${register}, ")
        self.assertRegex(body, r"\bsd\s+\$\d+, 32\(\$sp\)")
        self.assertRegex(body, r"\bsd\s+\$\d+, 40\(\$sp\)")
        self.assertRegex(body, r"\bsw\s+\$\d+, 52\(\$sp\)")


if __name__ == "__main__":
    unittest.main(verbosity=2)
