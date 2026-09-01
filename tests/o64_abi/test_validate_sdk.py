#!/usr/bin/env python3

import json
import tempfile
import unittest
from pathlib import Path

import validate_sdk


class ValidateSdkTests(unittest.TestCase):
	def create_sdk(self, libdragon_commit=validate_sdk.LIBDRAGON_COMMIT, dirty=False):
		temporary = tempfile.TemporaryDirectory()
		root = Path(temporary.name)
		for relative in validate_sdk.REQUIRED_FILES:
			path = root / relative
			path.parent.mkdir(parents=True, exist_ok=True)
			path.touch()
		for relative in validate_sdk.REQUIRED_TOOLS:
			path = root / relative
			path.parent.mkdir(parents=True, exist_ok=True)
			path.touch(mode=0o755)
		(root / "mips64-elf/include/libdragon.version").write_text(json.dumps({
			"branch": "preview",
			"hash": libdragon_commit,
			"commit-date": "2026-08-18",
			"dirty": dirty,
		}))
		(root / "mips64-elf/include/toolchain.version").write_text(json.dumps({
			"host": "aarch64-apple-darwin25.5.0",
			"binutils": "2.47",
			"gcc": "16.2.0",
			"newlib": "4.4.0.20231231",
		}))
		return temporary, root

	def test_accepts_the_clean_pinned_sdk(self):
		temporary, root = self.create_sdk()
		self.addCleanup(temporary.cleanup)
		provenance = validate_sdk.validate_sdk(root)
		self.assertEqual(provenance.libdragon["hash"], validate_sdk.LIBDRAGON_COMMIT)

	def test_rejects_a_different_libdragon_commit(self):
		temporary, root = self.create_sdk(libdragon_commit="0" * 40)
		self.addCleanup(temporary.cleanup)
		with self.assertRaisesRegex(validate_sdk.ValidationError, "libdragon SDK mismatch"):
			validate_sdk.validate_sdk(root)

	def test_rejects_dirty_libdragon_provenance(self):
		temporary, root = self.create_sdk(dirty=True)
		self.addCleanup(temporary.cleanup)
		with self.assertRaisesRegex(validate_sdk.ValidationError, "must be clean"):
			validate_sdk.validate_sdk(root)


if __name__ == "__main__":
	unittest.main()
