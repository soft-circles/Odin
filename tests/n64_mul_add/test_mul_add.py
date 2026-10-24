#!/usr/bin/env python3
"""SDK-free check that N64 matrix multiply-adds do not call software fma/fmaf."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
ODIN = Path(os.environ.get("ODIN", ROOT / "odin")).resolve()
FIXTURE = Path(__file__).parent / "fixture"
MATRIX_PROCS = ("mat3_mul", "mat4_mul", "mat3_mul_vec", "vec_mul_mat3", "mat2_mul_f64")
FMA_CALL = re.compile(r"\bjal\s+fmaf?\b")


def function_body(assembly, name):
    body = re.search(r"^" + name + r":\n.*?^\s*\.end\s+" + name + r"\b", assembly, re.M | re.S)
    if body is None:
        raise AssertionError(f"{name} not found in assembly")
    return body[0]


class MulAdd(unittest.TestCase):
    def test_n64_assembly(self):
        for optimization in ("none", "speed"):
            with self.subTest(optimization=optimization), tempfile.TemporaryDirectory(prefix="odin-mul-add-") as directory:
                # -o:none writes one .S per module; read them all.
                subprocess.run([str(ODIN), "build", str(FIXTURE), "-target:n64", "-no-entry-point",
                                f"-o:{optimization}", "-build-mode:asm", f"-out:{directory}/probe.S"], check=True)
                assembly = "\n".join(path.read_text() for path in Path(directory).glob("*.S"))
                for name in MATRIX_PROCS:
                    with self.subTest(optimization=optimization, function=name):
                        body = function_body(assembly, name)
                        self.assertNotRegex(body, FMA_CALL)
                        self.assertRegex(body, r"\bmul\.[sd]\b")
                with self.subTest(optimization=optimization, function="explicit_fma"):
                    self.assertRegex(function_body(assembly, "explicit_fma"), FMA_CALL)


if __name__ == "__main__":
    unittest.main(verbosity=2)
