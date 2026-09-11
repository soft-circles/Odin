"""Read the checked-in N64 compiler constants used by test helpers."""

from __future__ import annotations

import importlib.util
import os
import re
import sys
from pathlib import Path


ODIN_ROOT = Path(__file__).resolve().parents[1]
N64_INST = Path(os.environ.get("N64_INST", Path.home() / "n64_toolchain")).expanduser()
PHASE0 = Path(os.environ.get(
	"O64_PHASE0", ODIN_ROOT.parent / "llvm-project/llvm/utils/o64-abi-differential.py"))
CONSTANTS_PATH = ODIN_ROOT / "src/n64_toolchain_pins.hpp"
_CONSTANT_PATTERN = re.compile(
	r'^gb_global char const \*(N64_[A-Z0-9_]+) = "([^"]+)";$',
	re.MULTILINE,
)


def load_compiler_constants(path: Path = CONSTANTS_PATH) -> dict[str, str]:
	return dict(_CONSTANT_PATTERN.findall(path.read_text(encoding="utf-8")))


COMPILER_CONSTANTS = load_compiler_constants()
LIBDRAGON_COMMIT = COMPILER_CONSTANTS["N64_PINNED_LIBDRAGON_COMMIT"]
N64_MAKEFILE_SHA256 = COMPILER_CONSTANTS["N64_PINNED_MAKEFILE_SHA256"]
EXPECTED_TOOLCHAIN = {
	"host": COMPILER_CONSTANTS["N64_EXPECTED_TOOLCHAIN_HOST"],
	"binutils": COMPILER_CONSTANTS["N64_EXPECTED_BINUTILS_VERSION"],
	"gcc": COMPILER_CONSTANTS["N64_EXPECTED_GCC_VERSION"],
	"newlib": COMPILER_CONSTANTS["N64_EXPECTED_NEWLIB_VERSION"],
}


def load_phase0():
	if not PHASE0.is_file():
		sys.exit(f"O64 phase-0 differential script not found: {PHASE0} (set O64_PHASE0)")
	spec = importlib.util.spec_from_file_location("o64_phase0", PHASE0)
	module = importlib.util.module_from_spec(spec)
	spec.loader.exec_module(module)
	return module
