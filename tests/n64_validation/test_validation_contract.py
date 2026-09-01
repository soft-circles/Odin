#!/usr/bin/env python3
"""Public-command contract tests for the N64 validation driver."""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ODIN_ROOT = Path(__file__).resolve().parents[2]
DRIVER = ODIN_ROOT / "tests/n64_validate.py"


class N64ValidationContractTests(unittest.TestCase):
	def test_full_mode_reports_every_missing_required_dependency(self):
		with tempfile.TemporaryDirectory(prefix="odin-n64-validation-home-") as home:
			environment = os.environ.copy()
			environment["HOME"] = home
			for name in ("N64_INST", "ARES_TEST", "ARES_TEST_SOURCE", "N64_VALIDATION_CONTAINER"):
				environment.pop(name, None)
			result = subprocess.run(
				[sys.executable, str(DRIVER), "full"],
				cwd=ODIN_ROOT,
				env=environment,
				text=True,
				stdout=subprocess.PIPE,
				stderr=subprocess.STDOUT,
				check=False,
			)

		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertIn("full validation preflight", result.stdout)
		self.assertIn("N64_INST", result.stdout)
		self.assertIn("ARES_TEST", result.stdout)
		self.assertIn("ARES_TEST_SOURCE", result.stdout)
		self.assertIn("N64_VALIDATION_CONTAINER", result.stdout)
		self.assertNotIn("SKIP", result.stdout)

	def test_list_mode_exposes_focused_quick_and_full_stages(self):
		for mode, expected in {
			"quick": ("active pin drift", "documentation links", "canonical Pong quickstart"),
			"full": ("O64 ABI", "libdragon binding", "tracer golden", "Pong golden", "DFS golden"),
		}.items():
			with self.subTest(mode=mode):
				result = subprocess.run(
					[sys.executable, str(DRIVER), mode, "--list"],
					cwd=ODIN_ROOT,
					text=True,
					stdout=subprocess.PIPE,
					stderr=subprocess.STDOUT,
					check=False,
				)
				self.assertEqual(result.returncode, 0, result.stdout)
				for stage in expected:
					self.assertIn(stage, result.stdout)


if __name__ == "__main__":
	unittest.main(verbosity=2)
