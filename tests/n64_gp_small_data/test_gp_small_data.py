#!/usr/bin/env python3
"""SDK-free check that -target:n64 reaches globals of 8 bytes or less through $gp, as GCC's -G 8 does."""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/n64_validation"))
from o64_fork import require_o64_fork

ODIN = Path(os.environ.get("ODIN", ROOT / "odin")).resolve()
FIXTURE = Path(__file__).parent / "fixture"


def build_asm(target):
    with tempfile.TemporaryDirectory(prefix="odin-n64-gp-") as directory:
        out = Path(directory) / "fixture.S"
        subprocess.run([str(ODIN), "build", str(FIXTURE), f"-target:{target}", "-o:speed",
                        "-build-mode:asm", "-no-entry-point", "-use-single-module", f"-out:{out}"], check=True)
        return out.read_text()


def function_body(asm, name):
    body = re.search(rf"^{re.escape(name)}:.*?^\s*\.end\s+{re.escape(name)}$", asm, re.M | re.S)
    if body is None:
        raise AssertionError(f"{name} not found in the assembly")
    return body[0]


def section_of(asm, symbol):
    section = None
    for line in asm.splitlines():
        directive = re.match(r"\s*\.(section\s+([^,\s]+)|text|data|bss)\b", line)
        if directive:
            section = directive[2] or "." + directive[1]
        if re.match(rf'^"?{re.escape(symbol)}"?:', line):
            return section
    raise AssertionError(f"{symbol} is not defined in the assembly")


class GpSmallData(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        require_o64_fork(ODIN)
        cls.asm = build_asm("n64")

    def test_word_global_in_one_instruction(self):
        body = function_body(self.asm, "bump_counter")
        self.assertRegex(body, r"\blw\s+\$\d+, %gp_rel\(\"n64_gp_small_data::counter\"\)\(\$gp\)")
        self.assertRegex(body, r"\bsw\s+\$\d+, %gp_rel\(\"n64_gp_small_data::counter\"\)\(\$gp\)")
        self.assertNotIn("%hi(", body)

    def test_doubleword_global_in_one_instruction(self):
        body = function_body(self.asm, "bump_ticks")
        self.assertRegex(body, r"\bld\s+\$\d+, %gp_rel\(\"n64_gp_small_data::ticks\"\)\(\$gp\)")
        self.assertRegex(body, r"\bsd\s+\$\d+, %gp_rel\(\"n64_gp_small_data::ticks\"\)\(\$gp\)")
        self.assertNotIn("%hi(", body)

    def test_small_globals_live_in_small_sections(self):
        self.assertEqual(section_of(self.asm, "n64_gp_small_data::counter"), ".sbss")
        self.assertEqual(section_of(self.asm, "n64_gp_small_data::ticks"), ".sbss")

    def test_extern_word_through_gp(self):
        self.assertRegex(function_body(self.asm, "read_c_small"), r"\blw\s+\$2, %gp_rel\(c_small\)\(\$gp\)")

    def test_float_constant_through_gp(self):
        self.assertRegex(function_body(self.asm, "scale"), r"\blwc1\s+\$f\d+, %gp_rel\(\$CPI\d+_\d+\)\(\$gp\)")

    def test_nine_bytes_stay_absolute(self):
        body = function_body(self.asm, "bump_nine")
        self.assertIn('%hi("n64_gp_small_data::nine', body)
        self.assertNotIn("%gp_rel", body)

    def test_explicit_section_stays_absolute(self):
        body = function_body(self.asm, "bump_placed")
        self.assertIn('%hi("n64_gp_small_data::placed")', body)
        self.assertNotIn("%gp_rel", body)

    def test_zero_sized_extern_stays_absolute(self):
        body = function_body(self.asm, "address_of_c_zero")
        self.assertIn("%hi(c_zero)", body)
        self.assertNotIn("%gp_rel", body)


class FreestandingKeepsAbsolute(unittest.TestCase):
    def test_freestanding_mips32be_does_not_use_gp(self):
        require_o64_fork(ODIN)
        self.assertNotIn("%gp_rel", build_asm("freestanding_mips32be"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
