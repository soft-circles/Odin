#!/usr/bin/env python3
"""Black-box tests for Odin's integrated N64 ROM build."""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


HERE = Path(__file__).resolve().parent
ODIN_ROOT = HERE.parents[1]
ODIN = Path(os.environ.get("ODIN", ODIN_ROOT / "odin")).resolve()
REQUIRED_SDK_FILES = (
	"mips64-elf/lib/libdragon.a",
	"mips64-elf/lib/libdragonsys.a",
	"mips64-elf/lib/n64.ld",
)
REQUIRED_SDK_TOOLS = (
	"bin/ed64romconfig",
	"bin/mips64-elf-g++",
	"bin/mips64-elf-objdump",
	"bin/mips64-elf-strip",
	"bin/n64elfcompress",
	"bin/n64sym",
	"bin/n64tool",
)


def configured_sdk() -> Path | None:
	configured = os.environ.get("N64_INST")
	if configured:
		return Path(configured).expanduser().resolve()
	default = Path.home() / "n64_toolchain"
	return default.resolve() if default.is_dir() else None


def create_app(parent: Path, name: str = "app") -> Path:
	app = parent / name
	app.mkdir(parents=True)
	(app / "main.odin").write_text(
		"package n64_build_fixture\n\n"
		"main :: proc() {}\n",
		encoding="utf-8",
	)
	return app


def run_build(
	app: Path,
	*arguments: str,
	sdk_env: Path | None = None,
	extra_env: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
	environment = os.environ.copy()
	if sdk_env is None:
		environment.pop("N64_INST", None)
	else:
		environment["N64_INST"] = str(sdk_env)
	if extra_env:
		environment.update(extra_env)
	return subprocess.run(
		[str(ODIN), "build", ".", "-target:n64", *arguments],
		cwd=app,
		env=environment,
		text=True,
		stdout=subprocess.PIPE,
		stderr=subprocess.STDOUT,
		check=False,
	)


def run_host_build(app: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
	return subprocess.run(
		[str(ODIN), "build", ".", *arguments],
		cwd=app,
		text=True,
		stdout=subprocess.PIPE,
		stderr=subprocess.STDOUT,
		check=False,
	)


def make_sdk_facade(parent: Path, source: Path) -> Path:
	"""Link the required SDK inputs without requiring version metadata."""
	root = parent / "sdk facade"
	for relative in (*REQUIRED_SDK_FILES, *REQUIRED_SDK_TOOLS):
		destination = root / relative
		destination.parent.mkdir(parents=True, exist_ok=True)
		destination.symlink_to(source / relative)
	return root


class N64OptionValidationTests(unittest.TestCase):
	def setUp(self):
		if not ODIN.is_file():
			self.skipTest(f"Odin compiler not found: {ODIN}")
		self.temporary = tempfile.TemporaryDirectory(prefix="odin-n64-option-test-")
		self.addCleanup(self.temporary.cleanup)
		self.root = Path(self.temporary.name)
		self.app = create_app(self.root)

	def assert_invalid_option(self, option: str, diagnostic: str):
		result = run_build(self.app, option)
		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertIn(diagnostic, result.stdout)

	def test_title_is_bounded_and_makefile_safe(self):
		for value in ("", "x" * 21, "unsafe$title", "unsafe#title"):
			with self.subTest(value=value):
				self.assert_invalid_option(f"-n64-title:{value}", "n64-title")

	def test_region_is_exactly_one_ascii_letter(self):
		self.assert_invalid_option("-n64-region:", "n64-region")
		for value in ("EU", "7"):
			with self.subTest(value=value):
				self.assert_invalid_option(f"-n64-region:{value}", "one ASCII region letter")

	def test_save_type_is_validated_before_sdk_discovery(self):
		self.assert_invalid_option("-n64-save-type:battery", "eeprom4k")

	def test_rtc_is_rejected_with_eeprom_save_types(self):
		for save_type in ("eeprom4k", "eeprom16k"):
			with self.subTest(save_type=save_type):
				result = run_build(self.app, f"-n64-save-type:{save_type}", "-n64-rtc")
				self.assertNotEqual(result.returncode, 0, result.stdout)
				self.assertIn("cannot use RTC with EEPROM", result.stdout)

	def test_controller_list_uses_unambiguous_semicolon_separators(self):
		for value in (
			"n64,pak=rumble;invalid",
			"n64;none;none;none;none",
			"n64;;none",
		):
			with self.subTest(value=value):
				self.assert_invalid_option(f"-n64-controllers:{value}", "semicolon-separated")

		valid = run_build(self.app, "-n64-controllers:n64,pak=rumble;none")
		self.assertNotEqual(valid.returncode, 0, valid.stdout)
		self.assertIn("N64 SDK is not configured", valid.stdout)
		self.assertNotIn("Invalid -n64-controllers", valid.stdout)

	def test_asset_and_metadata_inputs_must_exist_with_the_expected_kind(self):
		asset_file = self.root / "not assets"
		asset_file.write_text("x", encoding="utf-8")
		metadata_directory = self.root / "not metadata"
		metadata_directory.mkdir()

		cases = (
			(f"-n64-assets:{asset_file}", "existing directory"),
			(f"-n64-metadata:{metadata_directory}", "existing file"),
			(f"-n64-assets:{self.root / 'missing assets'}", "existing directory"),
			(f"-n64-metadata:{self.root / 'missing.ini'}", "existing file"),
		)
		for option, diagnostic in cases:
			with self.subTest(option=option):
				self.assert_invalid_option(option, diagnostic)

	def test_n64_options_are_rejected_for_host_builds(self):
		result = run_host_build(self.app, "-n64-title:Host")
		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertIn("may only be used with -target:n64", result.stdout)


class N64SdkDiscoveryTests(unittest.TestCase):
	def setUp(self):
		if not ODIN.is_file():
			self.skipTest(f"Odin compiler not found: {ODIN}")
		self.temporary = tempfile.TemporaryDirectory(prefix="odin-n64-build-test-")
		self.addCleanup(self.temporary.cleanup)
		self.root = Path(self.temporary.name)
		self.app = create_app(self.root)

	def test_missing_sdk_configuration_explains_both_supported_sources(self):
		result = run_build(self.app)

		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertIn("N64_INST", result.stdout)
		self.assertIn("-n64-inst", result.stdout)
		self.assertIn("<odin root>/n64", result.stdout)

	def test_explicit_sdk_option_takes_precedence_over_environment(self):
		explicit = self.root / "explicit sdk"
		environment = self.root / "environment sdk"
		explicit.mkdir()
		environment.mkdir()

		result = run_build(
			self.app,
			f"-n64-inst:{explicit}",
			sdk_env=environment,
		)

		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertIn(str(explicit), result.stdout)
		self.assertNotIn(str(environment), result.stdout)

	def test_environment_sdk_is_validated_when_no_option_is_given(self):
		environment = self.root / "environment sdk"
		environment.mkdir()

		result = run_build(self.app, sdk_env=environment)

		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertIn(str(environment), result.stdout)
		self.assertIn("mips64-elf/lib/n64.ld", result.stdout)

	def test_sdk_falls_back_to_n64_in_the_odin_root(self):
		odin_root = self.root / "odin root"
		odin_root.mkdir()
		for name in ("base", "core", "shared", "vendor"):
			(odin_root / name).symlink_to(ODIN_ROOT / name, target_is_directory=True)
		bundled = odin_root / "n64"
		bundled.mkdir()

		result = run_build(self.app, extra_env={"ODIN_ROOT": str(odin_root)})

		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertNotIn("N64 SDK is not configured", result.stdout)
		self.assertIn(f"N64 SDK {bundled.resolve()} is missing required file", result.stdout)

	def test_sdk_metadata_does_not_block_real_packaging_failures(self):
		for index, metadata in enumerate((None, "not JSON", '{"hash":"local-development","dirty":true}')):
			with self.subTest(metadata=metadata):
				sdk = self.root / f"custom sdk {index}"
				for relative in (*REQUIRED_SDK_FILES, *REQUIRED_SDK_TOOLS):
					path = sdk / relative
					path.parent.mkdir(parents=True, exist_ok=True)
					path.touch(mode=0o755 if relative in REQUIRED_SDK_TOOLS else 0o644)
				# The link is the first packaging step.
				(sdk / "bin/mips64-elf-g++").write_text(
					"#!/bin/sh\necho custom-sdk-packaging-reached\nexit 37\n")
				if metadata is not None:
					for name in ("libdragon.version", "toolchain.version"):
						path = sdk / "mips64-elf/include" / name
						path.parent.mkdir(parents=True, exist_ok=True)
						path.write_text(metadata)

				result = run_build(self.app, f"-n64-inst:{sdk}")

				self.assertNotEqual(result.returncode, 0, result.stdout)
				self.assertIn("custom-sdk-packaging-reached", result.stdout)
				self.assertIn("intermediates were retained", result.stdout)
				self.assertFalse((self.app / "app.z64").exists())

	def test_missing_required_sdk_file_is_named(self):
		source = configured_sdk()
		if source is None or not all((source / path).exists() for path in (*REQUIRED_SDK_FILES, *REQUIRED_SDK_TOOLS)):
			self.skipTest("a complete N64 SDK is required for the SDK fixture")
		sdk = make_sdk_facade(self.root, source)
		(sdk / "mips64-elf/lib/n64.ld").unlink()

		result = run_build(self.app, f"-n64-inst:{sdk}")

		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertIn("missing required file", result.stdout)
		self.assertIn("mips64-elf/lib/n64.ld", result.stdout)

	def test_non_executable_sdk_tool_is_named(self):
		source = configured_sdk()
		if source is None or not all((source / path).exists() for path in (*REQUIRED_SDK_FILES, *REQUIRED_SDK_TOOLS)):
			self.skipTest("a complete N64 SDK is required for the SDK fixture")
		sdk = make_sdk_facade(self.root, source)
		tool = sdk / "bin/n64sym"
		tool.unlink()
		tool.write_text("not executable\n", encoding="utf-8")
		tool.chmod(0o644)

		result = run_build(self.app, f"-n64-inst:{sdk}")

		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertIn("missing required executable tool", result.stdout)
		self.assertIn("bin/n64sym", result.stdout)

	def test_mkdfs_is_required_only_when_assets_are_requested(self):
		source = configured_sdk()
		if source is None or not all((source / path).exists() for path in (*REQUIRED_SDK_FILES, *REQUIRED_SDK_TOOLS)):
			self.skipTest("a complete N64 SDK is required for the SDK fixture")
		sdk = make_sdk_facade(self.root, source)
		assets = self.root / "assets with spaces"
		assets.mkdir()
		(assets / "message.txt").write_text("asset payload", encoding="utf-8")

		result = run_build(self.app, f"-n64-inst:{sdk}", f"-n64-assets:{assets}")

		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertIn("-n64-assets requires", result.stdout)
		self.assertIn("bin/mkdfs", result.stdout)

	def test_n64metadata_is_required_only_when_metadata_is_requested(self):
		source = configured_sdk()
		if source is None or not all((source / path).exists() for path in (*REQUIRED_SDK_FILES, *REQUIRED_SDK_TOOLS)):
			self.skipTest("a complete N64 SDK is required for the SDK fixture")
		sdk = make_sdk_facade(self.root, source)
		metadata = self.root / "metadata with spaces.ini"
		metadata.write_text("[meta]\nname = Tool Test\n", encoding="utf-8")

		result = run_build(self.app, f"-n64-inst:{sdk}", f"-n64-metadata:{metadata}")

		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertIn("-n64-metadata requires", result.stdout)
		self.assertIn("bin/n64metadata", result.stdout)


class N64EndToEndBuildTests(unittest.TestCase):
	@classmethod
	def setUpClass(cls):
		if not ODIN.is_file():
			raise unittest.SkipTest(f"Odin compiler not found: {ODIN}")
		if os.environ.get("N64_VALIDATION_MODE") == "quick":
			raise unittest.SkipTest("end-to-end SDK builds are disabled in quick validation")
		cls.sdk = configured_sdk()
		if cls.sdk is None or not (cls.sdk / "mips64-elf/lib/n64.ld").is_file():
			raise unittest.SkipTest("set N64_INST to run N64 end-to-end build tests")

	def setUp(self):
		self.temporary = tempfile.TemporaryDirectory(prefix="odin n64 build test ")
		self.addCleanup(self.temporary.cleanup)
		self.root = Path(self.temporary.name)

	def test_foreign_objects_and_archives_are_preserved_for_libdragon_linking(self):
		app = self.root / "foreign inputs"
		app.mkdir()
		(app / "object.c").write_text("int object_data = 20; int object_value(int *p) { return *p; }\n", encoding="utf-8")
		(app / "archive.c").write_text("int archive_value(void) { return 22; }\n", encoding="utf-8")
		compiler = self.sdk / "bin/mips64-elf-gcc"
		archiver = self.sdk / "bin/mips64-elf-ar"
		common = ("-march=vr4300", "-mtune=vr4300", "-mabi=o64", "-c")
		subprocess.run(
			[str(compiler), *common, "object.c", "-o", "object.o"],
			cwd=app,
			check=True,
			stdout=subprocess.PIPE,
			stderr=subprocess.STDOUT,
		)
		subprocess.run(
			[str(compiler), *common, "archive.c", "-o", "archive-member.o"],
			cwd=app,
			check=True,
			stdout=subprocess.PIPE,
			stderr=subprocess.STDOUT,
		)
		subprocess.run(
			[str(archiver), "rcs", "helpers.a", "archive-member.o"],
			cwd=app,
			check=True,
			stdout=subprocess.PIPE,
			stderr=subprocess.STDOUT,
		)
		(app / "main.odin").write_text(
			"package foreign_inputs\n\n"
			"foreign import object_lib \"object.o\"\n"
			"foreign import archive_lib \"helpers.a\"\n\n"
			"foreign object_lib { object_data: i32; object_value :: proc \"c\" (p: ^i32) -> i32 --- }\n"
			"foreign archive_lib { archive_value :: proc \"c\" () -> i32 --- }\n\n"
			"main :: proc() {\n"
			"\tif object_value(&object_data) + archive_value() == -1 {\n"
			"\t\tunreachable()\n"
			"\t}\n"
			"}\n",
			encoding="utf-8",
		)

		output = app / "foreign-inputs.z64"
		result = run_build(app, "-o:speed", f"-n64-inst:{self.sdk}", f"-out:{output}")

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertEqual(output.read_bytes()[:4], bytes.fromhex("80371240"))

	def test_inherited_n64_toolchain_override_does_not_escape_validated_sdk(self):
		app = create_app(self.root, "sanitized environment app")
		output = app / "sanitized.z64"

		result = run_build(
			app,
			f"-n64-inst:{self.sdk}",
			f"-out:{output}",
			extra_env={
				"N64_GCCPREFIX": str(self.root / "unvalidated toolchain"),
				"N64_INST": str(self.root / "unvalidated sdk"),
			},
		)

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertTrue(output.is_file(), result.stdout)

	def test_inherited_compiler_and_linker_flags_do_not_reach_the_sdk_link(self):
		app = create_app(self.root, "inherited flags app")
		output = app / "flags.z64"

		result = run_build(
			app,
			f"-n64-inst:{self.sdk}",
			f"-out:{output}",
			extra_env={
				"LDFLAGS": f"-L{self.root / 'missing libs'} -lno-such-library",
				"CFLAGS": "-DODIN_UNEXPECTED",
				"CXXFLAGS": "-DODIN_UNEXPECTED",
				"ASFLAGS": "--no-such-assembler-flag",
			},
		)

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertTrue(output.is_file(), result.stdout)

	def test_failing_packaging_tool_fails_the_build_and_keeps_intermediates(self):
		sdk = make_sdk_facade(self.root, self.sdk)
		tool = sdk / "bin/n64sym"
		tool.unlink()
		tool.write_text("#!/bin/sh\nexit 19\n", encoding="utf-8")
		tool.chmod(0o755)
		app = create_app(self.root, "failing packaging app")
		output = app / "must-not-exist.z64"

		result = run_build(app, f"-n64-inst:{sdk}", f"-out:{output}")

		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertFalse(output.exists(), result.stdout)
		self.assertIn("N64 packaging step 'sym' failed with exit code 19", result.stdout)
		self.assertIn("intermediates were retained", result.stdout)

	def test_incompatible_generic_link_options_are_rejected(self):
		cases = {
			"position-independent relocation": ("-reloc-mode:pic", "require -reloc-mode:static"),
			"custom CRT policy": ("-no-crt", "do not support -no-crt"),
			"linker-only output": ("-print-linker-flags", "use -show-system-calls"),
		}
		for name, (option, diagnostic) in cases.items():
			with self.subTest(name=name):
				app = create_app(self.root, name)
				result = run_build(app, f"-n64-inst:{self.sdk}", option)
				self.assertNotEqual(result.returncode, 0, result.stdout)
				self.assertIn(diagnostic, result.stdout)

	def test_runtime_build_handles_spaced_paths_and_retains_the_intermediates(self):
		app = self.root / "odin runtime source"
		app.mkdir()
		shutil.copy2(ODIN_ROOT / "tests/n64_runtime/runtime.odin", app / "runtime.odin")
		sdk_alias = self.root / "libdragon sdk"
		sdk_alias.symlink_to(self.sdk, target_is_directory=True)
		output = self.root / "rom output" / "runtime result.z64"
		output.parent.mkdir()

		result = run_build(
			app,
			f"-n64-inst:{sdk_alias}",
			f"-out:{output}",
			"-keep-temp-files",
			"-show-system-calls",
		)

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertTrue(output.is_file(), result.stdout)
		self.assertEqual(output.read_bytes()[:4], bytes.fromhex("80371240"))
		self.assertIn("[SYSTEM CALL] n64-link", result.stdout)

		retention = re.search(r"Retained N64 build intermediates: (.+)", result.stdout)
		self.assertIsNotNone(retention, result.stdout)
		intermediates = Path(retention.group(1).strip())
		self.assertEqual(intermediates.parent, output.parent)
		self.assertEqual(intermediates.name, "runtime result.n64-build")
		self.assertEqual({path.name for path in intermediates.iterdir()},
		                 {"odin-n64.elf", "odin-n64.map", "odin-n64.elf.sym", "odin-n64.elf.stripped"})

		headless_runner = os.environ.get("ARES_TEST")
		if headless_runner:
			headless = subprocess.run(
				[headless_runner, str(ODIN_ROOT / "tests/n64_runtime/runtime.test.js"), str(output), "--timeout", "30"],
				text=True,
				stdout=subprocess.PIPE,
				stderr=subprocess.STDOUT,
				check=False,
			)
			self.assertEqual(headless.returncode, 0, headless.stdout)
			self.assertIn("runtime: ordered startup", headless.stdout)
			self.assertIn("ODIN_N64_RUNTIME_CLEANUP:v2", headless.stdout)

	def test_metadata_and_raw_assets_are_packaged_with_header_options(self):
		app = create_app(self.root, "metadata and dfs app")
		assets = app / "raw assets with spaces"
		assets.mkdir()
		asset_payload = b"ODIN_N64_DFS_ASSET_PAYLOAD_V02"
		(assets / "message with spaces.txt").write_bytes(asset_payload)
		metadata_source = app / "metadata source with spaces"
		metadata_source.mkdir()
		companion_directory = metadata_source / "localized copy"
		companion_directory.mkdir()
		companion = companion_directory / "description with spaces.txt"
		companion_payload = "Relative metadata companion was packaged."
		companion.write_text(companion_payload, encoding="utf-8")
		metadata = metadata_source / "extended metadata with spaces.ini"
		metadata.write_text(
			"[meta]\n"
			"name = Odin N64 Metadata Test\n"
			"author = Odin\n"
			"release-date = 2026-08-31\n"
			"num-players = 1\n"
			"long-desc = localized copy/description with spaces.txt\n",
			encoding="utf-8",
		)
		output = metadata_source / "configured output.z64"

		result = run_build(
			app,
			f"-n64-inst:{self.sdk}",
			f"-out:{output}",
			"-n64-title:Odin DFS 0.2!",
			"-n64-region:j",
			"-n64-save-type:sram256k",
			"-n64-rtc",
			"-n64-controllers:n64,pak=rumble;none;mouse;gamecube",
			f"-n64-assets:{assets}",
			f"-n64-metadata:{metadata}",
			"-keep-temp-files",
		)

		self.assertEqual(result.returncode, 0, result.stdout)
		rom = output.read_bytes()
		self.assertEqual(rom[:4], bytes.fromhex("80371240"))
		self.assertEqual(rom[0x20:0x34].rstrip(b" \0"), b"Odin DFS 0.2!")
		self.assertEqual(rom[0x3E:0x3F], b"J")
		self.assertIn(b"metadata.ini", rom)
		self.assertIn(b"Odin N64 Metadata Test", rom)
		self.assertIn(companion_payload.encode(), rom)

		retention = re.search(r"Retained N64 build intermediates: (.+)", result.stdout)
		self.assertIsNotNone(retention, result.stdout)
		intermediates = Path(retention.group(1).strip())
		# The INI and assets are passed in place, so only the tool outputs are
		# left beside the ROM.
		self.assertEqual(intermediates.name, "configured output.n64-build")
		self.assertEqual({path.name for path in intermediates.iterdir()},
		                 {"odin-n64.elf", "odin-n64.map", "odin-n64.elf.sym", "odin-n64.elf.stripped", "odin-n64.dfs"})
		self.assertIn(asset_payload, (intermediates / "odin-n64.dfs").read_bytes())

	def test_sdk_directory_with_spaces_reaches_n64sym_through_a_relative_link(self):
		# A symlinked alias is resolved to the real SDK path; a real directory
		# whose own name has a space is not, and n64sym may run objdump through
		# a shell. Link each top-level SDK entry so the root itself is real.
		sdk = self.root / "real sdk with spaces"
		sdk.mkdir()
		for entry in self.sdk.iterdir():
			(sdk / entry.name).symlink_to(entry, target_is_directory=entry.is_dir())
		app = create_app(self.root, "spaced sdk app")
		# Same file name in both builds: the output stem names the LLVM module.
		output = self.root / "spaced sdk output" / "rom.z64"
		reference = self.root / "reference output" / "rom.z64"
		output.parent.mkdir()
		reference.parent.mkdir()

		result = run_build(app, f"-n64-inst:{sdk}", f"-out:{output}", "-keep-temp-files")
		baseline = run_build(app, f"-n64-inst:{self.sdk}", f"-out:{reference}")

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertEqual(baseline.returncode, 0, baseline.stdout)
		link = output.parent / "rom.n64-build" / "sdk"
		self.assertTrue(link.is_symlink(), result.stdout)
		self.assertEqual(link.resolve(), sdk.resolve())
		self.assertEqual(output.read_bytes(), reference.read_bytes())

	def test_extra_linker_flags_reach_the_sdk_link(self):
		app = create_app(self.root, "extra linker flags app")
		output = app / "flags.z64"

		result = run_build(
			app,
			f"-n64-inst:{self.sdk}",
			f"-out:{output}",
			"-extra-linker-flags:-Wl,--defsym=odin_n64_extra_flag_marker=0x1234  -Wl,--wrap=odin_n64_unused_symbol",
			"-keep-temp-files",
			"-show-system-calls",
		)

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertIn("-Wl,--defsym=odin_n64_extra_flag_marker=0x1234 -Wl,--wrap=odin_n64_unused_symbol", result.stdout)
		symbols = subprocess.run(
			[str(self.sdk / "bin/mips64-elf-nm"), str(app / "flags.n64-build" / "odin-n64.elf")],
			text=True,
			stdout=subprocess.PIPE,
			stderr=subprocess.STDOUT,
			check=True,
		).stdout
		self.assertRegex(symbols, r"00001234 A odin_n64_extra_flag_marker")

	def test_runtime_builds_from_a_clean_odin_only_directory(self):
		app = self.root / "clean runtime sample"
		app.mkdir()
		shutil.copy2(ODIN_ROOT / "tests/n64_runtime/runtime.odin", app / "runtime.odin")
		output = app / "runtime.z64"

		result = run_build(app, f"-n64-inst:{self.sdk}", f"-out:{output}")

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertEqual(output.read_bytes()[:4], bytes.fromhex("80371240"))
		first_rom = output.read_bytes()
		repeated = run_build(app, f"-n64-inst:{self.sdk}", f"-out:{output}")
		self.assertEqual(repeated.returncode, 0, repeated.stdout)
		self.assertEqual(output.read_bytes(), first_rom)
		application_files = {
			path.name for path in app.iterdir()
			if path.is_file() and path != output
		}
		self.assertEqual(application_files, {"runtime.odin"})

	def test_extensionless_explicit_output_gains_z64_extension(self):
		app = create_app(self.root, "extensionless output app")
		requested = app / "named-rom"
		expected = app / "named-rom.z64"

		result = run_build(app, f"-n64-inst:{self.sdk}", f"-out:{requested}")

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertFalse(requested.exists())
		self.assertTrue(expected.is_file(), result.stdout)
		self.assertEqual(expected.read_bytes()[:4], bytes.fromhex("80371240"))

	def test_default_output_is_named_after_the_package_directory_with_z64_extension(self):
		app = create_app(self.root, "default output app")
		expected = app / "default output app.z64"

		result = run_build(app, sdk_env=self.sdk)

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertTrue(expected.is_file(), result.stdout)
		self.assertFalse((app / "default output app.bin").exists())
		leftovers = {
			path.name for path in app.rglob("*")
			if path.is_file() and path not in {app / "main.odin", expected}
		}
		self.assertFalse(leftovers, f"unexpected intermediates without -keep-temp-files: {leftovers}")

	def test_title_defaults_to_the_package_directory_not_the_output_name(self):
		app = create_app(self.root, "title default app")
		output = self.root / "renamed output" / "rom.z64"
		output.parent.mkdir()

		result = run_build(app, f"-n64-inst:{self.sdk}", f"-out:{output}")

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertEqual(output.read_bytes()[0x20:0x34].rstrip(b" \0"), b"title default app")


if __name__ == "__main__":
	unittest.main(verbosity=2)
