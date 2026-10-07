#!/usr/bin/env python3

import re
import tempfile
import unittest
from pathlib import Path

import validate_sdk


class ValidateSdkTests(unittest.TestCase):
	def test_rejects_missing_required_files_and_non_executable_tools(self):
		for relative in (*validate_sdk.REQUIRED_FILES, *validate_sdk.REQUIRED_TOOLS):
			with self.subTest(relative=relative):
				temporary, root = self.create_sdk()
				self.addCleanup(temporary.cleanup)
				path = root / relative
				if relative in validate_sdk.REQUIRED_TOOLS:
					path.chmod(0o644)
				else:
					path.unlink()
				with self.assertRaisesRegex(validate_sdk.ValidationError, re.escape(relative)):
					validate_sdk.validate_sdk(root)

	def create_sdk(self):
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
		return temporary, root

	def test_accepts_sdk_without_matching_recipe_or_version_metadata(self):
		for metadata in (None, "not JSON", '{"hash":"local-development","dirty":true}'):
			with self.subTest(metadata=metadata):
				temporary, root = self.create_sdk()
				self.addCleanup(temporary.cleanup)
				(root / "include/n64.mk").write_text("# locally customized SDK recipe\n")
				for name in ("libdragon.version", "toolchain.version"):
					path = root / "mips64-elf/include" / name
					path.parent.mkdir(parents=True, exist_ok=True)
					path.unlink(missing_ok=True)
					if metadata is not None:
						path.write_text(metadata)
				self.assertIsNone(validate_sdk.validate_sdk(root))


if __name__ == "__main__":
	unittest.main()
