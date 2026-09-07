#!/usr/bin/env python3
"""Execute matrix return regressions and inspect their retained unoptimized IR."""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
NAMES = "test_4553_matrix_align,test_matrix_integer_return_align"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifacts", type=Path, required=True)
    args = parser.parse_args()
    artifacts = args.artifacts.resolve()
    artifacts.mkdir(parents=True, exist_ok=True)
    for mode in ("default", "none", "speed"):
        output = artifacts / mode
        output.mkdir(exist_ok=True)
        command = [str(ROOT / "odin"), "test", "tests/internal",
                   f"-define:ODIN_TEST_NAMES={NAMES}",
                   "-keep-temp-files", f"-out:{output / 'matrix-tests'}"]
        if mode != "default":
            command.append(f"-o:{mode}")
        with (output / "run.log").open("w") as log:
            log.write(f"command: {command!r}\n")
            log.flush()
            result = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        (output / "exit.txt").write_text(f"{result.returncode}\n")
        if result.returncode:
            raise SystemExit(f"Matrix return tests failed ({mode}); inspect {output / 'run.log'}")
        if mode == "none":
            ir = (output / "matrix-tests-test_internal.ll").read_text()
            # These return coercions exercise nonstandard integer widths. A test
            # that ceases to generate them must not silently lose coverage.
            for width in (24, 40, 48, 56):
                if not re.search(rf"\bcall i{width}\b", ir):
                    raise SystemExit(f"Missing i{width} return coercion in matrix IR")
            alignments = [int(n) for n in re.findall(r"\balign (\d+)", ir)]
            if not alignments or any(n < 1 or n & (n - 1) for n in alignments):
                raise SystemExit("Invalid or missing power-of-two matrix IR alignments")
        print(f"PASS: matrix return values and alignment ({mode})", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
