#!/usr/bin/env python3
"""Check active N64 pin statements against compiler constants and the hub lock."""

from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path

try:
	import tomllib
except ModuleNotFoundError:  # Python 3.9 is still the macOS system default.
	tomllib = None


ODIN_ROOT = Path(__file__).resolve().parents[2]
CONSTANT_PATTERN = re.compile(
	r'^gb_global char const \*(N64_[A-Z0-9_]+) = "([^"]+)";$',
	re.MULTILINE,
)
LIBDRAGON_DOCUMENTS = (
	"N64_BUILD.md",
	"vendor/libdragon/README.md",
	"vendor/libdragon/libdragon.odin",
	"tests/n64_dfs/README.md",
	"tests/n64_pong/README.md",
	"tests/o64_abi/README.md",
	"tests/o64_abi/libdragon_bindings/README.md",
)
ARES_DOCUMENTS = (
	"tests/n64_dfs/README.md",
	"tests/n64_pong/README.md",
	"tests/n64_tracer/README.md",
)


def discover_lock() -> Path | None:
	configured = os.environ.get("ODIN_N64_TOOLCHAIN_LOCK")
	if configured:
		return Path(configured).expanduser().resolve()
	candidate = ODIN_ROOT.parent / "toolchain.lock.toml"
	return candidate if candidate.is_file() else None


def load_lock(path: Path) -> dict[str, dict[str, str]]:
	if tomllib is not None:
		with path.open("rb") as file:
			return tomllib.load(file)
	sections: dict[str, dict[str, str]] = {}
	current: dict[str, str] | None = None
	for line in path.read_text(encoding="utf-8").splitlines():
		line = line.strip()
		section = re.fullmatch(r"\[([A-Za-z0-9_]+)\]", line)
		if section:
			current = sections.setdefault(section.group(1), {})
			continue
		value = re.fullmatch(r'([A-Za-z0-9_]+)\s*=\s*"([^"]*)"', line)
		if current is not None and value:
			current[value.group(1)] = value.group(2)
	return sections


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--lock", type=Path, help="authoritative coordination lock")
	args = parser.parse_args()

	constants_path = ODIN_ROOT / "src/n64_toolchain_pins.hpp"
	constants = dict(CONSTANT_PATTERN.findall(constants_path.read_text(encoding="utf-8")))
	libdragon = constants.get("N64_PINNED_LIBDRAGON_COMMIT")
	if not libdragon:
		parser.error(f"missing N64_PINNED_LIBDRAGON_COMMIT in {constants_path}")

	errors: list[str] = []
	for relative in LIBDRAGON_DOCUMENTS:
		if libdragon not in (ODIN_ROOT / relative).read_text(encoding="utf-8"):
			errors.append(f"{relative}: does not name compiler libdragon pin {libdragon}")

	lock_path = args.lock.resolve() if args.lock else discover_lock()
	if lock_path is not None:
		lock = load_lock(lock_path)
		expected = {
			"N64_PINNED_LIBDRAGON_COMMIT": lock["libdragon"]["commit"],
			"N64_PINNED_MAKEFILE_SHA256": lock["libdragon"]["n64_make_sha256"],
			"N64_EXPECTED_TOOLCHAIN_HOST": lock["libdragon_toolchain"]["validated_host"],
			"N64_EXPECTED_BINUTILS_VERSION": lock["libdragon_toolchain"]["binutils"],
			"N64_EXPECTED_GCC_VERSION": lock["libdragon_toolchain"]["gcc"],
			"N64_EXPECTED_NEWLIB_VERSION": lock["libdragon_toolchain"]["newlib"],
		}
		if constants != expected:
			errors.append(f"{constants_path}: compiler constants differ from {lock_path}")
		ares_test = lock["ares_test"]["commit"]
		for relative in ARES_DOCUMENTS:
			if ares_test not in (ODIN_ROOT / relative).read_text(encoding="utf-8"):
				errors.append(f"{relative}: does not name ares-test pin {ares_test}")
		print(f"authoritative lock: {lock_path}")
	else:
		print("authoritative coordination lock not present; checked Odin-local pin statements")

	if errors:
		for error in errors:
			print(f"error: {error}", file=sys.stderr)
		return 1
	print("active N64 pin statements match")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
