#!/usr/bin/env python3
"""Run the linked binding probe with the pinned ares-test interface."""

import argparse
import os
import shutil
import subprocess
from pathlib import Path


HERE = Path(__file__).resolve().parent
PASS_SENTINEL = "PASS: Odin libdragon binding ABI 23/23"
FAIL_SENTINEL = "FAIL: Odin libdragon binding ABI"


def find_runner(requested: str) -> str:
	runner = shutil.which(requested)
	if runner is None:
		raise FileNotFoundError(
			f"cannot find ares-test runner {requested!r}; "
			"pass --ares-test or set ARES_TEST"
		)
	return runner


def run_probe(runner: str, rom: Path, wall_timeout: int) -> str:
	command = [runner, str(HERE / "runtime.test.js"), str(rom), "--timeout", "30"]
	result = subprocess.run(
		command,
		cwd=HERE,
		capture_output=True,
		text=True,
		timeout=wall_timeout,
	)
	output = result.stdout + result.stderr
	if result.returncode != 0:
		raise RuntimeError(f"ares-test exited {result.returncode}:\n{output}")
	if PASS_SENTINEL not in output or FAIL_SENTINEL in output:
		raise RuntimeError(f"ares-test did not report the unique pass sentinel:\n{output}")
	return output


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("rom", nargs="?", type=Path, default=HERE / "libdragon_bindings.z64")
	parser.add_argument(
		"--ares-test",
		default=os.environ.get("ARES_TEST", "ares-test"),
		help="pinned ares-test executable (default: ARES_TEST or PATH)",
	)
	parser.add_argument("--wall-timeout", type=int, default=45)
	args = parser.parse_args()
	rom = args.rom.resolve()
	if not rom.is_file():
		parser.error(f"ROM does not exist: {rom}")
	try:
		runner = find_runner(args.ares_test)
		output = run_probe(runner, rom, args.wall_timeout)
	except (FileNotFoundError, RuntimeError, subprocess.TimeoutExpired) as error:
		parser.exit(1, f"error: {error}\n")
	print(output, end="" if output.endswith("\n") else "\n")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
