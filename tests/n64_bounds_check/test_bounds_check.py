#!/usr/bin/env python3
"""SDK-free check that upstream's bounds-check elision keeps every N64 check it cannot prove away."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
ODIN = Path(os.environ.get("ODIN", ROOT / "odin")).resolve()
FIXTURE = Path(__file__).parent / "fixture"
# At -o:speed the call goes to the handler LLVM outlines from runtime.bounds_check_error_contextless.
BOUNDS_CHECK_CALL = re.compile(r'\bjal\s+"?runtime(::|\.)bounds_check_error\w*')
CHECKED = (
    "slice_index_past_checked_len",
    "fixed_array_index_past_len",
    "struct_field_slice_dynamic_index",
    "loop_bound_past_len",
)
ELIDED = ("fixed_array_constant_in_range", "loop_bound_is_len")


def function_body(assembly, name):
    body = re.search(rf"^{re.escape(name)}:.*?^\s*\.end\s+{re.escape(name)}$", assembly, re.M | re.S)
    if body is None:
        raise AssertionError(f"{name} not found in the assembly")
    return body[0]


class BoundsCheckElision(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory(prefix="odin-n64-bounds-check-") as directory:
            out = Path(directory) / "fixture.S"
            subprocess.run([str(ODIN), "build", str(FIXTURE), "-target:n64", "-o:speed",
                            "-build-mode:asm", "-no-entry-point", f"-out:{out}"], check=True)
            cls.assembly = out.read_text()

    def test_unprovable_indices_keep_their_check(self):
        for name in CHECKED:
            with self.subTest(function=name):
                self.assertRegex(function_body(self.assembly, name), BOUNDS_CHECK_CALL)

    def test_provably_in_range_indices_lose_their_check(self):
        for name in ELIDED:
            with self.subTest(function=name):
                self.assertNotRegex(function_body(self.assembly, name), BOUNDS_CHECK_CALL)


if __name__ == "__main__":
    unittest.main(verbosity=2)
