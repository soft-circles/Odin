#!/usr/bin/env python3
"""SDK-free check that N64 float min, max and clamp do not call software fmin/fmax."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
ODIN = Path(os.environ.get("ODIN", ROOT / "odin")).resolve()
FIXTURE = Path(__file__).parent / "fixture"
PROCS = ("min_f32", "max_f32", "min_f64", "max_f64", "min3_f32", "clamp_f32")
MIN_MAX_CALL = re.compile(r"\b(jal|j)\s+(__\$fast_isel\$)?(fmin|fmax|llvm\.minnum|llvm\.maxnum)")


def function_body(assembly, name):
    body = re.search(r"^" + name + r":\n.*?^\s*\.end\s+" + name + r"\b", assembly, re.M | re.S)
    if body is None:
        raise AssertionError(f"{name} not found in assembly")
    return body[0]


class FloatMinMax(unittest.TestCase):
    def test_n64_assembly(self):
        for optimization in ("none", "speed"):
            with self.subTest(optimization=optimization), tempfile.TemporaryDirectory(prefix="odin-min-max-") as directory:
                # -o:none writes one .S per module; read them all.
                subprocess.run([str(ODIN), "build", str(FIXTURE), "-target:n64", "-no-entry-point",
                                f"-o:{optimization}", "-build-mode:asm", f"-out:{directory}/probe.S"], check=True)
                assembly = "\n".join(path.read_text() for path in Path(directory).glob("*.S"))
                for name in PROCS:
                    with self.subTest(optimization=optimization, function=name):
                        body = function_body(assembly, name)
                        self.assertNotRegex(body, MIN_MAX_CALL)
                        self.assertRegex(body, r"\bc\.(olt|ule|un)\.[sd]\b")


if __name__ == "__main__":
    unittest.main(verbosity=2)
