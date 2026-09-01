"""Read the checked-in N64 compiler constants used by test helpers."""

from __future__ import annotations

import re
from pathlib import Path


ODIN_ROOT = Path(__file__).resolve().parents[1]
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
