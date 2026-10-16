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
        # A C callee owns its copy: the caller copies the struct into the outgoing argument area.
        body = function_body(self.asm, "call_take_big_c")
        self.assertRegex(body, r"\bjal\s+memcpy\b")
        self.assertRegex(body, r"\bld\s+\$7, 32\(\$sp\)")
        self.assertRegex(body, r"\bjal\s+take_big_c\b")

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
