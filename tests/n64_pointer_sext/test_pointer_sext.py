#!/usr/bin/env python3
"""SDK-free check that -o:speed passes 32-bit N64 pointers sign-extended in 64-bit registers."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
ODIN = Path(os.environ.get("ODIN", ROOT / "odin")).resolve()
FIXTURE = Path(__file__).parent / "fixture"


def function_body(asm, name):
    body = re.search(rf"^{re.escape(name)}:.*?^\s*\.end\s+{re.escape(name)}$", asm, re.M | re.S)
    if body is None:
        raise AssertionError(f"{name} not found in the assembly")
    return body[0]


class PointerSignExtension(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory(prefix="odin-n64-ptr-sext-") as directory:
            out = Path(directory) / "fixture.S"
            subprocess.run([str(ODIN), "build", str(FIXTURE), "-target:n64", "-o:speed",
                            "-build-mode:asm", "-no-entry-point", f"-out:{out}"], check=True)
            cls.asm = out.read_text()

    def test_pointer_from_upper_half_of_struct_slot(self):
        # `file` sits in the upper half of $4; a logical shift would zero-extend 0x80xxxxxx.
        body = function_body(self.asm, "open_scene")
        self.assertRegex(body, r"\bdsra\s+\$4, \$4, 32\b")
        self.assertNotRegex(body, r"\bdsrl\s+\$4,")

    def test_kseg0_constant_pointer(self):
        # 0x80001000 is built with lui/ori, which sign-extend, not as 0x0000000080001000.
        body = function_body(self.asm, "write_back_kseg0")
        self.assertRegex(body, r"\blui\s+\$(\d+), 32768\b[\s\S]*\bori\s+\$4, \$\1, 4096\b")
        self.assertNotRegex(body, r"\bdsll\s+\$\d+, \$\d+, 31\b")


if __name__ == "__main__":
    unittest.main(verbosity=2)
