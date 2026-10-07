#!/usr/bin/env python3
"""Check that the libdragon SDK has the files and tools needed by the O64 fixtures."""

import argparse
import os
import sys
from pathlib import Path


REQUIRED_FILES = (
	"include/n64.mk",
	"mips64-elf/lib/libdragon.a",
	"mips64-elf/lib/libdragonsys.a",
	"mips64-elf/lib/n64.ld",
)

REQUIRED_TOOLS = (
	"bin/ed64romconfig",
	"bin/mips64-elf-g++",
	"bin/mips64-elf-gcc",
	"bin/mips64-elf-objdump",
	"bin/mips64-elf-size",
	"bin/mips64-elf-strip",
	"bin/n64elfcompress",
	"bin/n64sym",
	"bin/n64tool",
)


class ValidationError(Exception):
	pass


def validate_sdk(root: Path) -> None:
	missing_files = [relative for relative in REQUIRED_FILES if not (root / relative).is_file()]
	missing_tools = [
		relative for relative in REQUIRED_TOOLS
		if not (root / relative).is_file() or not os.access(root / relative, os.X_OK)
	]
	if missing_files or missing_tools:
		problems = [*(f"missing SDK file: {path}" for path in missing_files)]
		problems.extend(f"missing or non-executable SDK tool: {path}" for path in missing_tools)
		raise ValidationError("\n".join(problems))


def main(argv: list[str]) -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("sdk", type=Path, help="installed libdragon SDK root")
	args = parser.parse_args(argv)
	try:
		validate_sdk(args.sdk.resolve())
	except ValidationError as error:
		parser.exit(1, f"error: {error}\n")
	print(f"N64 SDK files and tools available: {args.sdk.resolve()}")
	return 0


if __name__ == "__main__":
	raise SystemExit(main(sys.argv[1:]))
