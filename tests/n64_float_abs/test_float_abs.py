#!/usr/bin/env python3
"""SDK-free native/endian float abs regressions, including big-endian N64 IR."""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
ODIN = Path(os.environ.get("ODIN", ROOT / "odin")).resolve()
HOST_ODIN = Path(os.environ.get("HOST_ODIN", ODIN)).resolve()
FIXTURE = Path(__file__).parent / "fixture"


class FloatAbs(unittest.TestCase):
    def test_host_values(self):
        with tempfile.TemporaryDirectory(prefix="odin-abs-host-") as directory:
            subprocess.run([str(HOST_ODIN), "run", str(FIXTURE), "-o:none",
                            f"-out:{directory}/probe"], check=True)

    def test_host_and_n64_sign_masks(self):
        for target, byteorder in ((None, sys.byteorder), ("n64", "big")):
            with self.subTest(target=target), tempfile.TemporaryDirectory(prefix="odin-abs-ir-") as directory:
                compiler = ODIN if target else HOST_ODIN
                command = [str(compiler), "build", str(FIXTURE), "-o:none",
                           "-build-mode:llvm-ir", f"-out:{directory}"]
                if target:
                    command.append(f"-target:{target}")
                subprocess.run(command, check=True)
                ir = "\n".join(path.read_text() for path in Path(directory).glob("*.ll"))
                for width in (16, 32, 64):
                    for suffix, storage in (("", byteorder), ("le", "little"), ("be", "big")):
                        name = f"abs_f{width}{suffix}"
                        with self.subTest(target=target, function=name):
                            body = re.search(r"define[^\n]* @" + name + r"\([^\n]*\).*?^}", ir, re.M | re.S)
                            self.assertIsNotNone(body, name)
                            sign = width - 1 if storage == byteorder else 7
                            mask = ((1 << width) - 1) ^ (1 << sign)
                            signed = mask if mask < (1 << (width-1)) else mask - (1 << width)
                            self.assertRegex(body[0], rf"and i{width} [^,\n]+, {signed}\b")


if __name__ == "__main__":
    unittest.main(verbosity=2)
