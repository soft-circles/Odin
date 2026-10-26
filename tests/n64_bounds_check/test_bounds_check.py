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
# Any reference counts, not only `jal`: LLVM may tail-call the handler with `j` or call it through a register. At
# -o:speed the reference goes to a handler LLVM outlines, such as runtime::bounds_check_error_contextless.handle_error-*
# or runtime::slice_handle_error_contextless.
CHECK_REFERENCE = re.compile(r'"?runtime(?:::|\.)(?:bounds_check_error|slice_expr_error|slice_handle_error)\w*')
# How many checks the frontend emits for each procedure at -o:none.
CHECKED = {
    "slice_index_past_checked_len": 1,
    "fixed_array_index_past_len": 1,
    "struct_field_slice_dynamic_index": 1,
    "loop_bound_past_len": 1,
    # Upstream 2804ac31c: the `s[k]` before the loop does not cover the one in the loop's bound or iterator call.
    "loop_interval_end_index_prechecked": 2,
    "loop_iterator_argument_index_prechecked": 2,
    "slice_expression_past_len": 1,
}
ELIDED = ("fixed_array_constant_in_range", "loop_bound_is_len", "slice_expression_constant_in_range")


def function_body(assembly, name):
    body = re.search(rf"^{re.escape(name)}:.*?^\s*\.end\s+{re.escape(name)}$", assembly, re.M | re.S)
    if body is None:
        raise AssertionError(f"{name} not found in the assembly")
    return body[0]


def check_references(assembly, name):
    return len(CHECK_REFERENCE.findall(function_body(assembly, name)))


def build(directory, *options):
    out = Path(directory) / "fixture.S"
    subprocess.run([str(ODIN), "build", str(FIXTURE), "-target:n64", *options, "-build-mode:asm", "-no-entry-point",
                    f"-out:{out}"], check=True)
    return out.read_text()


class BoundsCheckElision(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory(prefix="odin-n64-bounds-check-") as directory:
            # Without optimization every check left in the assembly is one the frontend emitted, so the counts say
            # what upstream's elision dropped. At -o:speed LLVM removes the provably redundant checks itself.
            cls.unoptimized = build(directory, "-o:none", "-use-single-module")
            cls.optimized = build(directory, "-o:speed")

    def test_unprovable_indices_keep_their_check(self):
        for name, count in CHECKED.items():
            with self.subTest(function=name):
                self.assertEqual(check_references(self.unoptimized, name), count)

    def test_provably_in_range_indices_lose_their_check(self):
        for name in ELIDED:
            with self.subTest(function=name):
                self.assertEqual(check_references(self.unoptimized, name), 0)

    def test_optimization_keeps_the_unprovable_checks(self):
        for name in CHECKED:
            with self.subTest(function=name):
                self.assertGreater(check_references(self.optimized, name), 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
