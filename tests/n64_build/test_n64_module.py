#!/usr/bin/env python3
"""Architecture checks for the localized N64 build implementation."""

from __future__ import annotations

import unittest
from pathlib import Path


ODIN_ROOT = Path(__file__).resolve().parents[2]
MODULE = ODIN_ROOT / "src/n64_build.cpp"
LINKER = ODIN_ROOT / "src/linker.cpp"


class N64BuildModuleTests(unittest.TestCase):
	def test_module_exposes_prepare_and_package_request_boundaries(self):
		contents = MODULE.read_text(encoding="utf-8")
		self.assertIn("n64_prepare_build(N64PrepareBuildRequest const &request)", contents)
		self.assertIn("n64_package_rom(N64BuildRequest request)", contents)

	def test_module_does_not_reach_into_compiler_or_linker_state(self):
		contents = MODULE.read_text(encoding="utf-8")
		for forbidden in ("build_context", "LinkerData", "Entity"):
			self.assertNotIn(forbidden, contents)

	def test_general_linker_uses_the_n64_module_through_adapters(self):
		contents = LINKER.read_text(encoding="utf-8")
		self.assertIn('#include "n64_build.cpp"', contents)
		self.assertIn("n64_prepare_build_from_context", contents)
		self.assertIn("n64_package_rom_from_linker", contents)


if __name__ == "__main__":
	unittest.main(verbosity=2)
