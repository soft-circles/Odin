#!/usr/bin/env python3
"""Black-box tests for Odin's integrated N64 ROM build."""

from __future__ import annotations

import json
import os
import re
import shlex
import shutil
import signal
import subprocess
import sys
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
	"mips64-elf/lib/libc.a",
	"mips64-elf/lib/libm.a",
	"mips64-elf/lib/libstdc++.a",
)
GCC_RUNTIME = "lib/gcc/mips64-elf"
REQUIRED_SDK_TOOLS = (
	"bin/ed64romconfig",
	"bin/n64elfcompress",
	"bin/n64sym",
	"bin/n64tool",
)


ROM_LLVM_TOOLS = ("ld.lld", "llvm-strip", "llvm-objdump", "llvm-addr2line")
CLANG_TARGET_FLAGS = ("--target=mips64-elf", "-march=mips3", "-mabi=o64", "-mno-abicalls", "-fno-pic", "-G0")


def llvm_bindir() -> Path | None:
	configured = os.environ.get("ODIN_N64_LLVM_BIN")
	if configured:
		return Path(configured).resolve()
	llvm_config = os.environ.get("LLVM_CONFIG")
	if not llvm_config:
		return None
	result = subprocess.run([llvm_config, "--bindir"], text=True, stdout=subprocess.PIPE, check=False)
	return Path(result.stdout.strip()).resolve() if result.returncode == 0 else None


def fake_llvm_bindir(parent: Path, name: str, tools: tuple[str, ...]) -> Path:
	directory = parent / name
	directory.mkdir()
	for tool in tools:
		(directory / tool).write_text("#!/bin/sh\nexit 3\n")
		(directory / tool).chmod(0o755)
	return directory


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


def run_check(app: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
	return subprocess.run(
		[str(ODIN), "check", ".", *arguments],
		cwd=app,
		text=True,
		stdout=subprocess.PIPE,
		stderr=subprocess.STDOUT,
		check=False,
	)


def create_foreign_source_app(parent: Path, name: str) -> Path:
	"""A package that foreign-imports a C source, a VR4300 .S source and an RSP .S source."""
	app = parent / name
	(app / "other").mkdir(parents=True)
	(app / "helper.c").write_text(
		"#include <rsp.h>\n"
		"#include <inttypes.h>\n"
		"#include <stdio.h>\n"
		"_Static_assert(INT_FAST16_MAX == 0x7fffffff, \"GCC's int_fast16_t is int\");\n"
		"int helper_format(char *text) { return snprintf(text, 32, \"%\" PRId32 \" %ld\", INT32_C(1), INT32_MAX); }\n"
		"#ifndef FROM_CFLAGS\n"
		"#error FROM_CFLAGS must come from -n64-cflags\n"
		"#endif\n"
		"DEFINE_RSP_UCODE(rsp_vector);\n"
		"int helper_value(void) { return FROM_CFLAGS; }\n"
		"void *helper_ucode(void) { return &rsp_vector; }\n",
		encoding="utf-8",
	)
	# A second probe.c in another directory must not collide with the first in the intermediates.
	(app / "probe.c").write_text("int first_probe(void) { return 1; }\n", encoding="utf-8")
	(app / "other/probe.c").write_text("int second_probe(void) { return 2; }\n", encoding="utf-8")
	(app / "vr4300.S").write_text(
		"\t.set noreorder\n\t.text\n\t.globl asm_value\nasm_value:\n\tjr $ra\n\tli $v0, 7\n",
		encoding="utf-8",
	)
	shutil.copy2(ODIN_ROOT / "tests/rsp_asm/vector-reference.S", app / "rsp_vector.S")
	(app / "main.odin").write_text(
		"package foreign_sources\n\n"
		"foreign import lib {\"helper.c\", \"vr4300.S\", \"rsp_vector.S\"}\n"
		"foreign import probes {\"probe.c\", \"other/probe.c\"}\n\n"
		"@(default_calling_convention=\"c\")\n"
		"foreign lib {\n"
		"\thelper_value :: proc() -> i32 ---\n"
		"\thelper_ucode :: proc() -> rawptr ---\n"
		"\tasm_value :: proc() -> i32 ---\n"
		"}\n"
		"@(default_calling_convention=\"c\")\n"
		"foreign probes {\n"
		"\tfirst_probe :: proc() -> i32 ---\n"
		"\tsecond_probe :: proc() -> i32 ---\n"
		"}\n\n"
		"main :: proc() {\n"
		"\tif helper_value() + asm_value() + first_probe() + second_probe() == 0 || helper_ucode() == nil {\n"
		"\t\tunreachable()\n"
		"\t}\n"
		"}\n",
		encoding="utf-8",
	)
	return app


def make_sdk_facade(parent: Path, source: Path) -> Path:
	"""Link the required SDK inputs without requiring version metadata."""
	root = parent / "sdk facade"
	for relative in (*REQUIRED_SDK_FILES, *REQUIRED_SDK_TOOLS, GCC_RUNTIME):
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
		for option in ("-n64-title:Host", "-n64-cflags:-DHOST"):
			with self.subTest(option=option):
				result = run_host_build(self.app, option)
				self.assertNotEqual(result.returncode, 0, result.stdout)
				self.assertIn("may only be used with -target:n64", result.stdout)

	def test_foreign_c_sources_are_checked_only_for_n64(self):
		app = create_foreign_source_app(self.root, "foreign sources")

		n64 = run_check(app, "-target:n64")
		host = run_check(app)

		self.assertEqual(n64.returncode, 0, n64.stdout)
		self.assertNotEqual(host.returncode, 0, host.stdout)
		self.assertIn("you cannot import a .c file", host.stdout)


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
				runtime = tuple(f"{GCC_RUNTIME}/13.2.0/{name}" for name in ("crti.o", "crtbegin.o", "crtend.o", "crtn.o", "libgcc.a"))
				for relative in (*REQUIRED_SDK_FILES, *REQUIRED_SDK_TOOLS, *runtime):
					path = sdk / relative
					path.parent.mkdir(parents=True, exist_ok=True)
					path.touch(mode=0o755 if relative in REQUIRED_SDK_TOOLS else 0o644)
				llvm = fake_llvm_bindir(self.root, f"custom llvm {index}", ROM_LLVM_TOOLS)
				(llvm / "ld.lld").write_text("#!/bin/sh\necho custom-sdk-packaging-reached\nexit 37\n")
				if metadata is not None:
					for name in ("libdragon.version", "toolchain.version"):
						path = sdk / "mips64-elf/include" / name
						path.parent.mkdir(parents=True, exist_ok=True)
						path.write_text(metadata)

				result = run_build(self.app, f"-n64-inst:{sdk}", extra_env={"ODIN_N64_LLVM_BIN": str(llvm)})

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

	def test_a_missing_or_ambiguous_gcc_runtime_is_named(self):
		source = configured_sdk()
		if source is None or not all((source / path).exists() for path in (*REQUIRED_SDK_FILES, *REQUIRED_SDK_TOOLS)):
			self.skipTest("a complete N64 SDK is required for the SDK fixture")
		versions = [entry for entry in (source / GCC_RUNTIME).iterdir() if (entry / "crtbegin.o").is_file()]
		for runtimes in ([], [versions[0], versions[0]]):
			with self.subTest(runtimes=len(runtimes)):
				sdk = make_sdk_facade(self.root / f"{len(runtimes)} runtimes", source)
				(sdk / GCC_RUNTIME).unlink()
				(sdk / GCC_RUNTIME).mkdir()
				for index, runtime in enumerate(runtimes):
					(sdk / GCC_RUNTIME / f"{runtime.name}.{index}").symlink_to(runtime, target_is_directory=True)

				result = run_build(self.app, f"-n64-inst:{sdk}")

				self.assertNotEqual(result.returncode, 0, result.stdout)
				self.assertIn("needs exactly one GCC runtime", result.stdout)

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


# Each stub logs one JSON line per call and writes the files its real tool would, so the packaging pipeline runs to a
# ROM without an SDK or the LLVM fork. A behaviour of "exit:N" or "signal:N" for a tool makes it fail after its outputs.
STUB_TOOL = r'''
import json, os, signal, sys

log, behaviours, program, *argv = sys.argv[1:]
tool = os.path.basename(program)
if argv[:1] == ["--n64-tool"]:
	tool = argv[1]
	arguments = argv[2:]
else:
	arguments = argv
record = {"program": program, "tool": tool, "argv": argv, "cwd": os.getcwd(), "env": dict(os.environ)}

def write(path, data):
	with open(path, "wb") as file:
		file.write(data)

def value_after(flag):
	return arguments[arguments.index(flag) + 1]

if tool in ("ld.lld", "clang"):
	write(value_after("-o"), b"stub object")
	for argument in arguments:
		if argument.startswith("-Map="):
			write(argument[len("-Map="):], b"stub map")
elif tool == "n64sym":
	write(arguments[-1], b"stub symbols")
	sym_bin = os.path.join(os.environ.get("N64_INST", ""), "bin")
	record["sym_tools"] = {name: os.path.realpath(os.path.join(sym_bin, name)) for name in ("mips64-elf-objdump", "mips64-elf-addr2line")}
elif tool == "mkdfs":
	write(arguments[0], b"stub dfs")
elif tool == "n64tool":
	write(value_after("--output"), b"stub rom")
elif tool == "ed64romconfig":
	with open(arguments[-1], "ab") as file:
		file.write(b" configured")
print(f"stub {tool} ran", flush=True)
with open(log, "a", encoding="utf-8") as file:
	file.write(json.dumps(record) + "\n")
with open(behaviours, encoding="utf-8") as file:
	behaviour = json.load(file).get(tool, "ok")
if behaviour.startswith("exit:"):
	sys.exit(int(behaviour[len("exit:"):]))
if behaviour.startswith("signal:"):
	os.kill(os.getpid(), int(behaviour[len("signal:"):]))
'''
STUB_LLVM_TOOLS = (*ROM_LLVM_TOOLS, "clang")
STUB_SDK_TOOLS = (*REQUIRED_SDK_TOOLS, "bin/mkdfs")
SANITIZED_PATH = "/usr/bin:/bin:/usr/sbin:/sbin"


class N64StubPackagingTests(unittest.TestCase):
	"""The packaging pipeline against stub tools, so quick mode covers it without an SDK or the LLVM fork."""

	def setUp(self):
		if not ODIN.is_file():
			self.skipTest(f"Odin compiler not found: {ODIN}")
		self.temporary = tempfile.TemporaryDirectory(prefix="odin n64 stub test ")
		self.addCleanup(self.temporary.cleanup)
		self.root = Path(self.temporary.name).resolve()
		self.log = self.root / "stub calls.jsonl"
		self.behaviours = self.root / "stub behaviours.json"
		self.set_behaviours({})
		script = self.root / "stub tool.py"
		script.write_text(STUB_TOOL, encoding="utf-8")
		command = " ".join(shlex.quote(str(part)) for part in (sys.executable, "-I", script, self.log, self.behaviours))
		self.wrapper = f"#!/bin/sh\nexec {command} \"$0\" \"$@\"\n"
		self.sdk = self.root / "stub sdk"
		self.runtime = self.sdk / GCC_RUNTIME / "13.2.0"
		for relative in REQUIRED_SDK_FILES:
			(self.sdk / relative).parent.mkdir(parents=True, exist_ok=True)
			(self.sdk / relative).touch()
		self.runtime.mkdir(parents=True)
		for name in ("crti.o", "crtbegin.o", "crtend.o", "crtn.o", "libgcc.a"):
			(self.runtime / name).touch()
		for relative in STUB_SDK_TOOLS:
			self.make_stub(self.sdk / relative)
		self.include = self.sdk / "mips64-elf/include"
		self.include.mkdir()
		for name in ("toolchain.version", "libdragon.version"):
			(self.include / name).write_text("{}", encoding="utf-8")
		self.llvm = self.root / "stub llvm"
		for name in STUB_LLVM_TOOLS:
			self.make_stub(self.llvm / name)
		self.pack = self.root / "stub pack"
		self.make_stub(self.pack)
		self.inherited = {
			"N64_GCCPREFIX": str(self.root / "unvalidated toolchain"),
			"N64_INST": str(self.root / "unvalidated sdk"),
			"CPATH": str(self.root / "unvalidated include"),
			"LIBRARY_PATH": str(self.root / "unvalidated lib"),
			"GCC_EXEC_PREFIX": str(self.root / "unvalidated gcc"),
			"CCC_OVERRIDE_OPTIONS": "+-DODIN_UNEXPECTED",
			"ODIN_N64_STUB_INHERITED": "kept",
			# Nothing reads these now that no make or shell runs; the exact link argv shows they add nothing.
			"LDFLAGS": f"-L{self.root / 'missing libs'} -lno-such-library",
			"CFLAGS": "-DODIN_UNEXPECTED",
			"ASFLAGS": "--no-such-assembler-flag",
		}

	def make_stub(self, path: Path):
		path.parent.mkdir(parents=True, exist_ok=True)
		path.write_text(self.wrapper, encoding="utf-8")
		path.chmod(0o755)

	def set_behaviours(self, behaviours: dict[str, str]):
		self.behaviours.write_text(json.dumps(behaviours), encoding="utf-8")

	def build(self, app: Path, output: Path, *arguments: str, extra_env: dict[str, str] | None = None):
		environment = {"ODIN_N64_LLVM_BIN": str(self.llvm), **(extra_env or {})}
		return run_build(app, f"-n64-inst:{self.sdk}", f"-out:{output}", *arguments, extra_env=environment)

	def calls(self) -> list[dict]:
		if not self.log.exists():
			return []
		return [json.loads(line) for line in self.log.read_text(encoding="utf-8").splitlines()]

	def intermediates(self, output: Path) -> Path:
		return output.parent / f"{output.stem}.n64-build"

	def assert_sanitized_environment(self, call: dict):
		environment = call["env"]
		self.assertEqual(environment.get("PATH"), SANITIZED_PATH)
		self.assertEqual(environment.get("SHELL"), "/bin/sh")
		self.assertEqual(environment.get("ODIN_N64_STUB_INHERITED"), "kept")
		for key in ("CPATH", "LIBRARY_PATH", "GCC_EXEC_PREFIX", "N64_GCCPREFIX", "CCC_OVERRIDE_OPTIONS"):
			self.assertFalse(key in environment, key)
		if call["tool"] != "n64sym":
			self.assertFalse("N64_INST" in environment)

	def test_each_step_gets_its_argv_and_a_sanitized_environment_in_order(self):
		app = create_app(self.root)
		output = app / "stub.z64"
		build = self.intermediates(output)

		result = self.build(app, output, extra_env=self.inherited)

		self.assertEqual(result.returncode, 0, result.stdout)
		calls = self.calls()
		self.assertEqual(
			[(call["program"], call["tool"]) for call in calls],
			[
				(str(self.llvm / "ld.lld"), "ld.lld"),
				(str(self.sdk / "bin/n64sym"), "n64sym"),
				(str(self.llvm / "llvm-strip"), "llvm-strip"),
				(str(self.sdk / "bin/n64elfcompress"), "n64elfcompress"),
				(str(self.sdk / "bin/n64tool"), "n64tool"),
				(str(self.sdk / "bin/ed64romconfig"), "ed64romconfig"),
			],
		)
		link, sym, strip, compress, n64tool, romconfig = calls
		elf, stripped, rom_tmp = build / "odin-n64.elf", build / "odin-n64.elf.stripped", build / "odin-n64.z64.tmp"
		self.assertEqual(
			link["argv"][:7],
			["-EB", "-o", str(elf), str(self.runtime / "crti.o"), str(self.runtime / "crtbegin.o"),
			 f"-L{self.runtime}", f"-L{self.sdk / 'mips64-elf/lib'}"],
		)
		objects = link["argv"][7:link["argv"].index("-ldragon")]
		self.assertTrue(objects and all(path.endswith(".o") for path in objects), link["argv"])
		self.assertEqual(
			link["argv"][7 + len(objects):],
			["-ldragon", "-lm", "-ldragonsys", "-Tn64.ld", "--gc-sections", "--wrap", "__do_global_ctors",
			 "-z", "max-page-size=4096", f"-Map={build / 'odin-n64.map'}", "--cref",
			 "-lstdc++", "-lm", "-lc", "-lgcc", str(self.runtime / "crtend.o"), str(self.runtime / "crtn.o")],
		)
		self.assertEqual(sym["argv"], ["--all", "odin-n64.elf", "odin-n64.elf.sym"])
		self.assertEqual(sym["cwd"], str(build))
		self.assertEqual(sym["env"].get("N64_INST"), "n64sym")
		self.assertEqual(
			sym["sym_tools"],
			{"mips64-elf-objdump": str(self.llvm / "llvm-objdump"), "mips64-elf-addr2line": str(self.llvm / "llvm-addr2line")},
		)
		self.assertEqual(strip["argv"], ["-s", str(stripped)])
		self.assertEqual(compress["argv"], ["-o", f"{build}/", "-c", "1", str(stripped)])
		self.assertEqual(
			n64tool["argv"],
			["--toc", "--title", "app", "--category", "N", "--output", str(rom_tmp), "--align", "256",
			 str(stripped), str(build / "odin-n64.elf.sym"),
			 str(self.include / "libdragon.version"), str(self.include / "toolchain.version")],
		)
		self.assertEqual(romconfig["argv"], ["--regionfree", str(rom_tmp)])
		for call in calls:
			with self.subTest(tool=call["tool"]):
				self.assert_sanitized_environment(call)

	def test_assets_reach_the_sdk_mkdfs_between_compression_and_n64tool(self):
		app = create_app(self.root)
		assets = app / "spaced assets"
		assets.mkdir()
		(assets / "message.txt").write_text("asset", encoding="utf-8")
		output = app / "assets.z64"
		build = self.intermediates(output)

		result = self.build(app, output, f"-n64-assets:{assets}", extra_env=self.inherited)

		self.assertEqual(result.returncode, 0, result.stdout)
		calls = self.calls()
		self.assertEqual(
			[call["tool"] for call in calls],
			["ld.lld", "n64sym", "llvm-strip", "n64elfcompress", "mkdfs", "n64tool", "ed64romconfig"],
		)
		mkdfs = calls[4]
		self.assertEqual(mkdfs["program"], str(self.sdk / "bin/mkdfs"))
		self.assertEqual(mkdfs["argv"], [str(build / "odin-n64.dfs"), str(assets)])
		self.assert_sanitized_environment(mkdfs)
		self.assertEqual(calls[5]["argv"][-3], str(build / "odin-n64.dfs"))
		self.assertNotIn("stub mkdfs ran", result.stdout)

	def test_a_failing_step_keeps_the_intermediates_and_the_previous_rom(self):
		app = create_app(self.root)
		output = app / "stub.z64"
		output.write_bytes(b"previous rom")
		build = self.intermediates(output)
		self.set_behaviours({"n64tool": "exit:7"})

		result = self.build(app, output)

		self.assertEqual(result.returncode, 7, result.stdout)
		self.assertIn("N64 packaging step 'n64tool' failed with exit code 7", result.stdout)
		self.assertIn(f"N64 build failed; intermediates were retained at {build}\n", result.stdout)
		self.assertEqual(output.read_bytes(), b"previous rom")
		for name in ("odin-n64.elf", "odin-n64.elf.sym", "odin-n64.elf.stripped", "odin-n64.z64.tmp"):
			self.assertTrue((build / name).is_file(), name)
		self.assertNotIn("ed64romconfig", [call["tool"] for call in self.calls()])

	def test_success_places_the_rom_and_removes_the_intermediates(self):
		app = create_app(self.root)
		output = app / "stub.z64"
		output.write_bytes(b"previous rom")

		result = self.build(app, output)

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertEqual(output.read_bytes(), b"stub rom configured")
		self.assertEqual(list(app.glob("*.n64-build")), [])

	def test_keep_temp_files_retains_the_intermediates_after_success(self):
		app = create_app(self.root)
		output = app / "stub.z64"
		build = self.intermediates(output)

		result = self.build(app, output, "-keep-temp-files")

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertIn(f"Retained N64 build intermediates: {build}\n", result.stdout)
		self.assertTrue((build / "odin-n64.elf").is_file())
		self.assertFalse((build / "odin-n64.z64.tmp").exists())
		self.assertEqual(output.read_bytes(), b"stub rom configured")

	def test_source_and_output_paths_with_spaces(self):
		app = self.root / "spaced source dir" / "spaced app"
		app.mkdir(parents=True)
		(app / "helper source.c").write_text("int helper(void) { return 1; }\n", encoding="utf-8")
		(app / "main.odin").write_text(
			"package spaced_app\n\n"
			"foreign import lib \"helper source.c\"\n"
			"@(default_calling_convention=\"c\")\n"
			"foreign lib {\n\thelper :: proc() -> i32 ---\n}\n\n"
			"main :: proc() { helper() }\n",
			encoding="utf-8",
		)
		output = self.root / "rom output dir" / "spaced rom.z64"
		output.parent.mkdir()
		build = self.intermediates(output)

		result = self.build(app, output, extra_env=self.inherited)

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertEqual(output.read_bytes(), b"stub rom configured")
		self.assertFalse(build.exists())
		self.assertEqual(
			[call["tool"] for call in self.calls()],
			["clang", "ld.lld", "n64sym", "llvm-strip", "n64elfcompress", "n64tool", "ed64romconfig"],
		)
		calls = {call["tool"]: call for call in self.calls()}
		self.assert_sanitized_environment(calls["clang"])
		compile_ = calls["clang"]
		self.assertEqual(compile_["cwd"], str(app))
		self.assertEqual(compile_["argv"][-3:], ["-o", str(build / "foreign/0-helper source.o"), "helper source.c"])
		self.assertIn(f"-ffile-prefix-map={app}=", compile_["argv"])
		link = calls["ld.lld"]["argv"]
		self.assertEqual(link[1:3], ["-o", str(build / "odin-n64.elf")])
		self.assertIn(str(build / "foreign/0-helper source.o"), link)
		self.assertEqual(calls["n64tool"]["argv"][1:3], ["--title", "spaced app"])
		self.assertEqual(calls["ed64romconfig"]["argv"], ["--regionfree", str(build / "odin-n64.z64.tmp")])

	def test_a_tool_killed_by_a_signal_maps_to_128_plus_the_signal(self):
		app = create_app(self.root)
		output = app / "stub.z64"
		output.write_bytes(b"previous rom")
		self.set_behaviours({"n64elfcompress": f"signal:{int(signal.SIGKILL)}"})

		result = self.build(app, output)

		code = 128 + int(signal.SIGKILL)
		self.assertEqual(result.returncode, code, result.stdout)
		self.assertIn(f"N64 packaging subprocess terminated by signal {int(signal.SIGKILL)}", result.stdout)
		self.assertIn(f"N64 packaging step 'elfcompress' failed with exit code {code}", result.stdout)
		self.assertIn(f"intermediates were retained at {self.intermediates(output)}\n", result.stdout)
		self.assertEqual(output.read_bytes(), b"previous rom")

	def test_n64_pack_runs_each_sdk_packaging_tool_as_n64_tool(self):
		for tool in ("ed64romconfig", "n64elfcompress", "n64tool", "mkdfs"):
			(self.sdk / "bin" / tool).unlink()
		app = create_app(self.root)
		assets = app / "spaced assets"
		assets.mkdir()
		(assets / "message.txt").write_text("asset", encoding="utf-8")
		output = app / "packed.z64"
		build = self.intermediates(output)
		stripped, rom_tmp, dfs = build / "odin-n64.elf.stripped", build / "odin-n64.z64.tmp", build / "odin-n64.dfs"

		result = self.build(app, output, f"-n64-assets:{assets}", f"-n64-pack:{self.pack}")

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertEqual(output.read_bytes(), b"stub rom configured")
		calls = self.calls()
		self.assertEqual(
			[(Path(call["program"]).name, call["tool"]) for call in calls],
			[("ld.lld", "ld.lld"), ("n64sym", "n64sym"), ("llvm-strip", "llvm-strip"),
			 ("stub pack", "n64elfcompress"), ("stub pack", "mkdfs"), ("stub pack", "n64tool"), ("stub pack", "ed64romconfig")],
		)
		packed = {call["tool"]: call["argv"] for call in calls if call["program"] == str(self.pack)}
		self.assertEqual(packed["n64elfcompress"], ["--n64-tool", "n64elfcompress", "-o", f"{build}/", "-c", "1", str(stripped)])
		self.assertEqual(packed["mkdfs"], ["--n64-tool", "mkdfs", str(dfs), str(assets)])
		self.assertEqual(
			packed["n64tool"],
			["--n64-tool", "n64tool", "--toc", "--title", "app", "--category", "N", "--output", str(rom_tmp),
			 "--align", "256", str(stripped), str(build / "odin-n64.elf.sym"), str(dfs),
			 str(self.include / "libdragon.version"), str(self.include / "toolchain.version")],
		)
		self.assertEqual(packed["ed64romconfig"], ["--n64-tool", "ed64romconfig", "--regionfree", str(rom_tmp)])
		# mkdfs's stdout is discarded as n64.mk's `>/dev/null` does; the other tools' output reaches the user.
		self.assertNotIn("stub mkdfs ran", result.stdout)
		self.assertIn("stub n64tool ran", result.stdout)

	def test_inherited_clang_driver_overrides_do_not_reach_the_tools(self):
		app = self.root / "override app"
		app.mkdir()
		(app / "helper.c").write_text("int helper(void) { return 1; }\n", encoding="utf-8")
		(app / "main.odin").write_text(
			"package override_app\n\n"
			"foreign import lib \"helper.c\"\n"
			"@(default_calling_convention=\"c\")\n"
			"foreign lib {\n\thelper :: proc() -> i32 ---\n}\n\n"
			"main :: proc() { helper() }\n",
			encoding="utf-8",
		)

		result = self.build(app, app / "override.z64", extra_env={"CCC_OVERRIDE_OPTIONS": "+-DODIN_UNEXPECTED"})

		self.assertEqual(result.returncode, 0, result.stdout)
		calls = self.calls()
		self.assertIn("clang", [call["tool"] for call in calls])
		for call in calls:
			with self.subTest(tool=call["tool"]):
				self.assertFalse("CCC_OVERRIDE_OPTIONS" in call["env"])


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
		llvm = llvm_bindir()
		if llvm is None:
			self.skipTest("set ODIN_N64_LLVM_BIN or LLVM_CONFIG to build the foreign inputs with the fork's clang")
		compiler = llvm / "clang"
		archiver = llvm / "llvm-ar"
		common = (*CLANG_TARGET_FLAGS, "-c")
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

	def test_foreign_c_and_assembly_sources_are_compiled_with_n64_cflags(self):
		app = create_foreign_source_app(self.root, "foreign sources")
		output = app / "sources.z64"

		result = run_build(
			app,
			f"-n64-inst:{self.sdk}",
			f"-out:{output}",
			"-n64-cflags:-DFROM_CFLAGS=3  -Wextra",
			"-keep-temp-files",
			"-show-system-calls",
		)

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertEqual(output.read_bytes()[:4], bytes.fromhex("80371240"))
		self.assertIn("[SYSTEM CALL] n64-cc", result.stdout)
		self.assertIn("[SYSTEM CALL] n64-rsp-as", result.stdout)
		self.assertIn("-std=gnu17 -DFROM_CFLAGS=3 -Wextra -o", result.stdout)
		foreign = app / "sources.n64-build" / "foreign"
		self.assertEqual(
			{path.name for path in foreign.glob("*.o") if "." not in path.stem},
			{"0-helper.o", "1-vr4300.o", "2-rsp_vector.o", "3-probe.o", "4-probe.o"},
		)
		symbols = subprocess.run(
			[str(self.sdk / "bin/mips64-elf-nm"), str(app / "sources.n64-build" / "odin-n64.elf")],
			text=True,
			stdout=subprocess.PIPE,
			stderr=subprocess.STDOUT,
			check=True,
		).stdout
		for symbol in ("helper_value", "asm_value", "first_probe", "second_probe", "rsp_vector_text_start", "rsp_vector_meta_end"):
			self.assertRegex(symbols, rf" {symbol}\n")
		self.assertNotIn("-I binary", result.stdout)
		rsp_commands = re.findall(r"\[SYSTEM CALL\] n64-rsp-[a-z]+\n(.*)\n", result.stdout)
		self.assertEqual(len(rsp_commands), 6, result.stdout)
		for command in rsp_commands:
			self.assertNotIn("bin/mips64-elf-", command)
		arch_mach_abi = lambda name: int.from_bytes((foreign / name).read_bytes()[36:40], "big") & 0xF0FFF000
		self.assertEqual(arch_mach_abi("2-rsp_vector.o"), arch_mach_abi("1-vr4300.o"))
		wrap = re.search(r"\[SYSTEM CALL\] n64-rsp-wrap\n.*\n", result.stdout)
		self.assertIsNotNone(wrap, result.stdout)
		self.assertNotIn("FROM_CFLAGS", wrap.group(0))
		self.assert_wraps_rsp_sections(foreign, "2-rsp_vector", "rsp_vector")

	def test_rsp_source_names_outside_c_identifiers_are_wrapped(self):
		app = self.root / "rsp names"
		app.mkdir()
		shutil.copy2(ODIN_ROOT / "tests/rsp_asm/vector-reference.S", app / "rsp-café.S")
		(app / "helper.c").write_text(
			"void *keep_lib_linked(void) { return keep_lib_linked; }\n",
			encoding="utf-8",
		)
		(app / "main.odin").write_text(
			"package rsp_names\n\n"
			"foreign import lib {\"helper.c\", \"rsp-café.S\"}\n\n"
			"@(default_calling_convention=\"c\")\n"
			"foreign lib { keep_lib_linked :: proc() -> rawptr --- }\n\n"
			"main :: proc() {\n"
			"\tif keep_lib_linked() == nil {\n"
			"\t\tunreachable()\n"
			"\t}\n"
			"}\n",
			encoding="utf-8",
		)

		result = run_build(app, f"-n64-inst:{self.sdk}", f"-out:{app / 'names.z64'}", "-keep-temp-files")

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assert_wraps_rsp_sections(app / "names.n64-build" / "foreign", "1-rsp-café", "rsp-café")

	def assert_wraps_rsp_sections(self, foreign: Path, job: str, stem: str):
		run = lambda tool, *arguments: subprocess.run(
			[str(self.sdk / "bin" / tool), *arguments],
			stdout=subprocess.PIPE,
			stderr=subprocess.STDOUT,
			check=True,
		).stdout
		values = {}
		for line in run("mips64-elf-nm", str(foreign / f"{job}.o")).decode().splitlines():
			value, kind, name = line.split(" ", 2)
			values[name] = (int(value, 16), kind)
		run("mips64-elf-objcopy", "-O", "binary", "-j", ".data", str(foreign / f"{job}.o"), str(foreign / f"{job}.wrapped.bin"))
		wrapped = (foreign / f"{job}.wrapped.bin").read_bytes()
		for section in ("text", "data", "meta"):
			expected = (foreign / f"{job}.{section}.bin").read_bytes()
			start, start_kind = values[f"{stem}_{section}_start"]
			end, end_kind = values[f"{stem}_{section}_end"]
			self.assertEqual((start % 16, start_kind, end_kind), (0, "D", "D"))
			self.assertEqual(wrapped[start:end], expected)
			self.assertEqual(values[f"{stem}_{section}_size"], (len(expected), "A"))

	def test_inherited_gcc_environment_does_not_reach_foreign_source_compiles(self):
		app = create_foreign_source_app(self.root, "gcc environment")
		output = app / "environment.z64"
		dependencies = self.root / "inherited dependencies.d"

		result = run_build(
			app,
			f"-n64-inst:{self.sdk}",
			f"-out:{output}",
			"-n64-cflags:-DFROM_CFLAGS=1",
			extra_env={"DEPENDENCIES_OUTPUT": str(dependencies), "GCC_EXEC_PREFIX": str(self.root / "missing gcc") + "/"},
		)

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertTrue(output.is_file(), result.stdout)
		self.assertFalse(dependencies.exists(), result.stdout)

	def test_foreign_source_compile_failure_fails_the_build_and_keeps_intermediates(self):
		app = create_foreign_source_app(self.root, "failing foreign sources")
		output = app / "must-not-exist.z64"

		result = run_build(app, f"-n64-inst:{self.sdk}", f"-out:{output}")

		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertFalse(output.exists(), result.stdout)
		self.assertIn("FROM_CFLAGS must come from -n64-cflags", result.stdout)
		self.assertIn("N64 packaging step 'cc' failed", result.stdout)
		self.assertIn("intermediates were retained", result.stdout)

	def test_foreign_sources_leave_no_intermediates_without_keep_temp_files(self):
		app = create_foreign_source_app(self.root, "clean foreign sources")
		output = app / "clean.z64"

		result = run_build(app, f"-n64-inst:{self.sdk}", f"-out:{output}", "-n64-cflags:-DFROM_CFLAGS=1")

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertTrue(output.is_file(), result.stdout)
		self.assertFalse((app / "clean.n64-build").exists(), result.stdout)

	def test_rom_link_requires_the_llvm_tools(self):
		app = create_app(self.root, "rom without llvm")
		empty = self.root / "no llvm"
		empty.mkdir()

		result = run_build(app, f"-n64-inst:{self.sdk}", f"-out:{app / 'must-not-exist.z64'}",
		                   extra_env={"ODIN_N64_LLVM_BIN": str(empty)})

		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertIn("N64 ROM builds require the LLVM fork's executable ld.lld in", result.stdout)
		self.assertFalse((app / "must-not-exist.n64-build").exists(), result.stdout)

	def test_foreign_source_requires_the_llvm_clang(self):
		app = create_foreign_source_app(self.root, "foreign sources without clang")
		llvm = fake_llvm_bindir(self.root, "rom tools only", ROM_LLVM_TOOLS)

		result = run_build(app, f"-n64-inst:{self.sdk}", f"-out:{app / 'must-not-exist.z64'}", "-n64-cflags:-DFROM_CFLAGS=1",
		                   extra_env={"ODIN_N64_LLVM_BIN": str(llvm)})

		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertIn("foreign import source", result.stdout)
		self.assertIn("requires the LLVM fork's executable clang in", result.stdout)
		self.assertFalse((app / "must-not-exist.n64-build").exists(), result.stdout)

	def test_rsp_source_requires_the_llvm_tools(self):
		app = create_foreign_source_app(self.root, "rsp without llvm-objcopy")
		llvm = fake_llvm_bindir(self.root, "no llvm-objcopy", (*ROM_LLVM_TOOLS, "clang"))

		result = run_build(app, f"-n64-inst:{self.sdk}", f"-out:{app / 'must-not-exist.z64'}", "-n64-cflags:-DFROM_CFLAGS=1",
		                   extra_env={"ODIN_N64_LLVM_BIN": str(llvm)})

		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertIn("N64 RSP microcode", result.stdout)
		self.assertIn("requires the LLVM fork's executable llvm-objcopy in", result.stdout)
		self.assertFalse((app / "must-not-exist.n64-build").exists(), result.stdout)

	def test_microcode_project_builds_without_mips64_elf_tools_or_n64_inst(self):
		odin_root = self.root / "odin root"
		odin_root.mkdir()
		for name in ("base", "core", "shared", "vendor", "misc"):
			(odin_root / name).symlink_to(ODIN_ROOT / name, target_is_directory=True)
		bundled = make_sdk_facade(self.root, self.sdk)
		(bundled / "mips64-elf/include").symlink_to(self.sdk / "mips64-elf/include", target_is_directory=True)
		bundled.rename(odin_root / "n64")
		self.assertEqual(list((odin_root / "n64/bin").glob("mips64-elf-*")), [])
		app = create_foreign_source_app(self.root, "microcode without gcc")
		output = app / "microcode.z64"

		result = run_build(app, f"-out:{output}", "-n64-cflags:-DFROM_CFLAGS=1", extra_env={"ODIN_ROOT": str(odin_root)})

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertEqual(output.read_bytes()[:4], bytes.fromhex("80371240"))

	def test_failing_llvm_tools_stop_the_build_before_a_rom(self):
		app = create_foreign_source_app(self.root, "rsp with failing llvm")
		failing = self.root / "failing llvm"
		failing.mkdir()
		for name in ("clang", "ld.lld", "llvm-objcopy"):
			(failing / name).write_text("#!/bin/sh\nexit 3\n")
			(failing / name).chmod(0o755)
		output = app / "must-not-exist.z64"

		result = run_build(app, f"-n64-inst:{self.sdk}", f"-out:{output}", extra_env={"ODIN_N64_LLVM_BIN": str(failing)})

		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertFalse(output.exists(), result.stdout)

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

	def test_n64_pack_replaces_the_sdk_packaging_tools(self):
		sdk = make_sdk_facade(self.root, self.sdk)
		for tool in ("ed64romconfig", "n64elfcompress", "n64tool"):
			(sdk / "bin" / tool).unlink()
		log = self.root / "pack calls.txt"
		pack = self.root / "pack shim"
		pack.write_text(
			"#!/bin/sh\n"
			f"printf '%s\\n' \"$*\" >> '{log}'\n"
			"[ \"$1\" = --n64-tool ] || exit 64\n"
			"tool=$2\nshift 2\n"
			f"exec '{self.sdk}/bin/'\"$tool\" \"$@\"\n",
			encoding="utf-8",
		)
		pack.chmod(0o755)
		app = create_app(self.root, "pack app")
		assets = app / "assets"
		assets.mkdir()
		(assets / "message.txt").write_bytes(b"ODIN_N64_PACK_ASSET")
		output = app / "packed.z64"

		result = run_build(app, f"-n64-inst:{sdk}", f"-n64-assets:{assets}", f"-n64-pack:{pack}", f"-out:{output}")

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertIn(b"ODIN_N64_PACK_ASSET", output.read_bytes())
		tools = [line.split()[1] for line in log.read_text(encoding="utf-8").splitlines()]
		self.assertEqual(tools, ["n64elfcompress", "mkdfs", "n64tool", "ed64romconfig"])

	def test_n64_pack_must_be_executable(self):
		pack = self.root / "not executable"
		pack.write_text("#!/bin/sh\n", encoding="utf-8")
		pack.chmod(0o644)

		result = run_build(create_app(self.root, "pack app"), f"-n64-inst:{self.sdk}", f"-n64-pack:{pack}")

		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertIn("-n64-pack must name an executable", result.stdout)

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
		                 {"odin-n64.elf", "odin-n64.map", "odin-n64.elf.sym", "odin-n64.elf.stripped", "n64sym"})
		llvm = llvm_bindir()
		if llvm is not None:
			nm = subprocess.run([str(llvm / "llvm-nm"), "--defined-only", str(intermediates / "odin-n64.elf")],
			                    text=True, stdout=subprocess.PIPE, check=True).stdout
			functions = sum(1 for line in nm.splitlines() if line.split()[1:2] in (["t"], ["T"]))
			symbols = int.from_bytes((intermediates / "odin-n64.elf.sym").read_bytes()[8:12], "big")
			# n64sym records every call site it finds in llvm-objdump's disassembly, so it holds far more entries than functions.
			self.assertGreater(symbols, 2 * functions)

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
		                 {"odin-n64.elf", "odin-n64.map", "odin-n64.elf.sym", "odin-n64.elf.stripped", "odin-n64.dfs", "n64sym"})
		self.assertIn(asset_payload, (intermediates / "odin-n64.dfs").read_bytes())

	def test_sdk_directory_with_spaces_reaches_n64sym_through_a_relative_facade(self):
		# n64sym runs objdump through a shell, unquoted. Link each top-level
		# SDK entry so the spaced SDK root itself is real, not resolved away.
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
		link = output.parent / "rom.n64-build" / "n64sym/bin/mips64-elf-objdump"
		self.assertTrue(link.is_symlink(), result.stdout)
		llvm = llvm_bindir()
		if llvm is not None:
			self.assertEqual(link.resolve(), (llvm / "llvm-objdump").resolve())
		self.assertEqual(output.read_bytes(), reference.read_bytes())

	def test_a_dangling_n64sym_facade_link_from_an_earlier_build_is_replaced(self):
		app = create_app(self.root, "dangling facade app")
		output = app / "rom.z64"
		stale = app / "rom.n64-build/n64sym/bin/mips64-elf-objdump"
		stale.parent.mkdir(parents=True)
		stale.symlink_to(self.root / "removed llvm/llvm-objdump")

		result = run_build(app, f"-n64-inst:{self.sdk}", f"-out:{output}")

		self.assertEqual(result.returncode, 0, result.stdout)
		self.assertFalse((app / "rom.n64-build").exists(), result.stdout)

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
		self.assertIn("--defsym=odin_n64_extra_flag_marker=0x1234 --wrap=odin_n64_unused_symbol", result.stdout)
		llvm = llvm_bindir()
		if llvm is None:
			self.skipTest("set ODIN_N64_LLVM_BIN or LLVM_CONFIG to read the ELF with llvm-nm")
		symbols = subprocess.run(
			[str(llvm / "llvm-nm"), str(app / "flags.n64-build" / "odin-n64.elf")],
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
