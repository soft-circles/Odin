#!/usr/bin/env python3
"""Validate and report the pinned libdragon SDK used by the O64 fixtures."""

import argparse
import json
import os
import sys
from dataclasses import dataclass
from pathlib import Path


LIBDRAGON_COMMIT = "c79a52b42ac790e06e797aede43914dd8754cd5f"

REQUIRED_FILES = (
	"include/n64.mk",
	"mips64-elf/include/libdragon.version",
	"mips64-elf/include/toolchain.version",
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


@dataclass(frozen=True)
class SdkProvenance:
	libdragon: dict
	toolchain: dict


def load_json(path: Path) -> dict:
	try:
		return json.loads(path.read_text())
	except (OSError, json.JSONDecodeError) as error:
		raise ValidationError(f"cannot read {path}: {error}") from error


def validate_sdk(root: Path) -> SdkProvenance:
	missing_files = [relative for relative in REQUIRED_FILES if not (root / relative).is_file()]
	missing_tools = [
		relative for relative in REQUIRED_TOOLS
		if not (root / relative).is_file() or not os.access(root / relative, os.X_OK)
	]
	if missing_files or missing_tools:
		problems = [*(f"missing SDK file: {path}" for path in missing_files)]
		problems.extend(f"missing or non-executable SDK tool: {path}" for path in missing_tools)
		raise ValidationError("\n".join(problems))

	libdragon = load_json(root / "mips64-elf/include/libdragon.version")
	toolchain = load_json(root / "mips64-elf/include/toolchain.version")
	actual_commit = libdragon.get("hash")
	if actual_commit != LIBDRAGON_COMMIT:
		raise ValidationError(
			"libdragon SDK mismatch: "
			f"expected {LIBDRAGON_COMMIT}, found {actual_commit or '<missing hash>'}"
		)
	if libdragon.get("dirty") is not False:
		raise ValidationError("libdragon SDK mismatch: pinned SDK provenance must be clean")
	return SdkProvenance(libdragon=libdragon, toolchain=toolchain)


def format_provenance(provenance: SdkProvenance) -> str:
	libdragon = provenance.libdragon
	toolchain = provenance.toolchain
	return "\n".join((
		"validated N64 SDK:",
		f"  libdragon={libdragon['hash']} branch={libdragon.get('branch', '<unknown>')} "
		f"commit-date={libdragon.get('commit-date', '<unknown>')} dirty={libdragon['dirty']}",
		f"  host={toolchain.get('host', '<unknown>')} binutils={toolchain.get('binutils', '<unknown>')} "
		f"gcc={toolchain.get('gcc', '<unknown>')} newlib={toolchain.get('newlib', '<unknown>')}",
	))


def main(argv: list[str]) -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("sdk", type=Path, help="installed libdragon SDK root")
	args = parser.parse_args(argv)
	try:
		provenance = validate_sdk(args.sdk.resolve())
	except ValidationError as error:
		parser.exit(1, f"error: {error}\n")
	print(format_provenance(provenance))
	return 0


if __name__ == "__main__":
	raise SystemExit(main(sys.argv[1:]))
