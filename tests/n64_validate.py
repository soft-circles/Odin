#!/usr/bin/env python3
"""Run the portable quick or authoritative full Odin N64 validation layer."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

try:
	import tomllib
except ModuleNotFoundError:  # pragma: no cover - Python 3.11+ is required in CI
	tomllib = None


ODIN_ROOT = Path(__file__).resolve().parents[1]
PYTHON = sys.executable
SHA256_PATTERN = re.compile(r"[0-9a-f]{64}")
GIT_COMMIT_PATTERN = re.compile(r"[0-9a-f]{40}")


@dataclass(frozen=True)
class Stage:
	name: str
	command: tuple[str, ...]
	cwd: Path = ODIN_ROOT
	environment: dict[str, str] = field(default_factory=dict)


@dataclass(frozen=True)
class FullDependencies:
	root: Path
	sdk: Path
	runner: Path
	runner_source: Path
	container_identity: str
	lock: dict


@dataclass(frozen=True)
class CandidateManifest:
	path: Path
	sha256: str
	odin_commit: str
	rom_hashes: dict[str, str]


@dataclass(frozen=True)
class RomIdentities:
	label: str
	hashes: dict[str, str]
	candidate_manifest: CandidateManifest | None = None


@dataclass(frozen=True)
class RetainedCandidateManifest:
	path: Path
	sha256: str


@dataclass(frozen=True)
class FullValidation:
	dependencies: FullDependencies
	artifacts: Path
	fixtures: dict[str, Path]
	rom_identities: RomIdentities


@dataclass(frozen=True)
class IdentityInputs:
	root: Path | None
	sdk: Path | None = None
	runner: Path | None = None
	runner_source: Path | None = None
	container_identity: str | None = None
	rom_identity_source: str | None = None
	candidate_manifest: RetainedCandidateManifest | None = None


class CandidateManifestError(ValueError):
	pass


def workspace_root() -> Path | None:
	configured = os.environ.get("ODIN_N64_WORKSPACE")
	if configured:
		return Path(configured).expanduser().resolve()
	candidate = ODIN_ROOT.parent
	if (candidate / "toolchain.lock.toml").is_file() and (candidate / "llvm-project").is_dir():
		return candidate
	return None


def quick_stages(lock_path: Path | None = None) -> list[Stage]:
	quick_environment = {
		"N64_VALIDATION_MODE": "quick",
		"ODIN": str(ODIN_ROOT / "odin"),
	}
	pin_environment = {}
	if lock_path is not None:
		pin_environment["ODIN_N64_TOOLCHAIN_LOCK"] = str(lock_path)
	return [
		Stage(
			"active pin drift",
			(PYTHON, "tests/n64_validation/check_active_pins.py"),
			environment=pin_environment,
		),
		Stage("documentation links", (PYTHON, "tests/n64_validation/check_documentation_links.py")),
		Stage("N64 build-module boundary", (PYTHON, "tests/n64_build/test_n64_module.py")),
		Stage("N64 public options and failure paths", (PYTHON, "tests/n64_build/test_n64_build.py"), environment=quick_environment),
		Stage("SDK validator unit tests", (PYTHON, "tests/o64_abi/test_validate_sdk.py")),
		Stage(
			"canonical Pong quickstart",
			("./odin", "check", "tests/n64_pong", "-target:n64", "-vet", "-warnings-as-errors"),
		),
	]


def load_lock(root: Path) -> dict:
	path = root / "toolchain.lock.toml"
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


def require_manifest_value(manifest: dict, name: str, expected: object) -> None:
	if name not in manifest:
		raise CandidateManifestError(f"candidate manifest is missing {name}")
	if type(manifest[name]) is not type(expected) or manifest[name] != expected:
		raise CandidateManifestError(f"candidate manifest {name} must be {expected!r}")


def require_lowercase_hash(values: dict, name: str, location: str) -> str:
	if name not in values:
		raise CandidateManifestError(f"candidate manifest is missing {location}.{name}")
	value = values[name]
	if not isinstance(value, str) or SHA256_PATTERN.fullmatch(value) is None:
		raise CandidateManifestError(
			f"candidate manifest {location}.{name} must be 64 lowercase hexadecimal characters"
		)
	return value


def parse_candidate_manifest(contents: bytes) -> dict:
	try:
		text = contents.decode("utf-8")
	except UnicodeDecodeError as error:
		raise CandidateManifestError(f"candidate manifest is not UTF-8: {error}") from error
	if tomllib is not None:
		try:
			return tomllib.loads(text)
		except tomllib.TOMLDecodeError as error:
			raise CandidateManifestError(f"invalid TOML: {error}") from error

	manifest: dict[str, object] = {}
	current = manifest
	for line_number, raw_line in enumerate(text.splitlines(), 1):
		line = raw_line.strip()
		if not line or line.startswith("#"):
			continue
		if line == "[roms]":
			if "roms" in manifest:
				raise CandidateManifestError("candidate manifest repeats [roms]")
			current = {}
			manifest["roms"] = current
			continue
		integer = re.fullmatch(r"([A-Za-z0-9_]+)\s*=\s*([0-9]+)", line)
		string = re.fullmatch(r'([A-Za-z0-9_]+)\s*=\s*"([^"\\]*)"', line)
		if integer:
			name, value = integer.groups()
			parsed: object = int(value)
		elif string:
			name, parsed = string.groups()
		else:
			raise CandidateManifestError(f"unsupported TOML on line {line_number}: {raw_line}")
		if name in current:
			raise CandidateManifestError(f"candidate manifest repeats {name}")
		current[name] = parsed
	return manifest


def load_candidate_manifest(path: Path, checked_out_odin_commit: str) -> CandidateManifest:
	if not path.is_absolute():
		raise CandidateManifestError(f"candidate manifest path must be absolute: {path}")
	path = path.resolve()
	try:
		contents = path.read_bytes()
	except OSError as error:
		raise CandidateManifestError(f"cannot read candidate manifest {path}: {error}") from error
	try:
		manifest = parse_candidate_manifest(contents)
	except CandidateManifestError as error:
		raise CandidateManifestError(f"cannot parse candidate manifest {path}: {error}") from error

	require_manifest_value(manifest, "schema", 1)
	require_manifest_value(manifest, "release", "0.2.1")
	if "odin_commit" not in manifest:
		raise CandidateManifestError("candidate manifest is missing odin_commit")
	odin_commit = manifest["odin_commit"]
	if not isinstance(odin_commit, str) or GIT_COMMIT_PATTERN.fullmatch(odin_commit) is None:
		raise CandidateManifestError("candidate manifest odin_commit must be a 40-character lowercase Git commit")
	if odin_commit != checked_out_odin_commit:
		raise CandidateManifestError(
			f"candidate manifest Odin commit is {odin_commit}, but the checked-out Odin HEAD is {checked_out_odin_commit}"
		)

	roms = manifest.get("roms")
	if not isinstance(roms, dict):
		raise CandidateManifestError("candidate manifest is missing [roms]")
	rom_hashes = {
		"tracer_rom_sha256": require_lowercase_hash(roms, "tracer_sha256", "roms"),
		"pong_rom_sha256": require_lowercase_hash(roms, "pong_sha256", "roms"),
		"dfs_rom_sha256": require_lowercase_hash(roms, "dfs_sha256", "roms"),
	}
	return CandidateManifest(path, hashlib.sha256(contents).hexdigest(), odin_commit, rom_hashes)


def accepted_rom_identities(lock: dict) -> RomIdentities:
	return RomIdentities("accepted", lock["accepted_artifacts"])


def candidate_rom_identities(manifest: CandidateManifest) -> RomIdentities:
	return RomIdentities("candidate", manifest.rom_hashes, manifest)


def retain_candidate_manifest(manifest: CandidateManifest, artifacts: Path) -> RetainedCandidateManifest:
	destination = artifacts / "candidate-manifest.toml"
	shutil.copy2(manifest.path, destination)
	retained_sha256 = hashlib.sha256(destination.read_bytes()).hexdigest()
	if retained_sha256 != manifest.sha256:
		raise CandidateManifestError("retained candidate manifest differs from the validated input")
	return RetainedCandidateManifest(destination, retained_sha256)


def resolve_executable(value: str) -> Path | None:
	candidate = Path(value).expanduser()
	if candidate.parent != Path("."):
		resolved = candidate.resolve()
		return resolved if resolved.is_file() and os.access(resolved, os.X_OK) else None
	found = shutil.which(value)
	return Path(found).resolve() if found else None


def git_output(repository: Path, *arguments: str) -> str:
	return subprocess.run(
		["git", "-C", str(repository), *arguments],
		text=True,
		stdout=subprocess.PIPE,
		stderr=subprocess.PIPE,
		check=True,
	).stdout.strip()


def full_preflight() -> FullDependencies:
	errors: list[str] = []
	root = workspace_root()
	if root is None:
		errors.append("ODIN_N64_WORKSPACE must name the coordination workspace containing toolchain.lock.toml and llvm-project")
		root = ODIN_ROOT.parent

	sdk_value = os.environ.get("N64_INST")
	sdk = Path(sdk_value).expanduser().resolve() if sdk_value else None
	if sdk is None:
		errors.append("N64_INST must explicitly name the pinned installed SDK")
	elif not sdk.is_dir():
		errors.append(f"N64_INST is not a directory: {sdk}")

	runner_value = os.environ.get("ARES_TEST")
	runner = resolve_executable(runner_value) if runner_value else None
	if not runner_value:
		errors.append("ARES_TEST must explicitly name the pinned headless runner executable")
	elif runner is None:
		errors.append(f"ARES_TEST is not executable or on PATH: {runner_value}")

	runner_source_value = os.environ.get("ARES_TEST_SOURCE")
	runner_source = Path(runner_source_value).expanduser().resolve() if runner_source_value else None
	if not runner_source_value:
		errors.append("ARES_TEST_SOURCE must explicitly name the pinned ares-test Git checkout")
	elif not (runner_source / ".git").exists():
		errors.append(f"ARES_TEST_SOURCE is not a Git checkout: {runner_source}")

	container_identity = os.environ.get("N64_VALIDATION_CONTAINER")
	if not container_identity:
		errors.append("N64_VALIDATION_CONTAINER must name the immutable Ubuntu/AMD64 job image or digest")

	if platform.system() != "Linux" or platform.machine().lower() not in {"x86_64", "amd64"}:
		errors.append(
			f"authoritative full validation requires Linux/AMD64; current host is {platform.system()}/{platform.machine()}"
		)

	lock: dict = {}
	if (root / "toolchain.lock.toml").is_file():
		try:
			lock = load_lock(root)
		except (OSError, RuntimeError, ValueError) as error:
			errors.append(f"cannot read authoritative toolchain lock: {error}")
	else:
		errors.append(f"missing authoritative toolchain lock: {root / 'toolchain.lock.toml'}")

	for relative in ("llvm-project/llvm/utils/o64-abi-differential.py", "odin/tests/o64_abi/differential.py"):
		if not (root / relative).is_file():
			errors.append(f"missing full-validation source: {root / relative}")
	if (root / "odin").resolve() != ODIN_ROOT:
		errors.append(f"coordination workspace Odin checkout is not this checkout: {root / 'odin'}")

	if lock and runner_source is not None and (runner_source / ".git").exists():
		try:
			head = git_output(runner_source, "rev-parse", "HEAD")
			dirty = git_output(runner_source, "status", "--porcelain", "--untracked-files=no")
		except subprocess.CalledProcessError as error:
			errors.append(f"cannot inspect ARES_TEST_SOURCE: {error.stderr.strip()}")
		else:
			expected = lock["ares_test"]["commit"]
			if head != expected:
				errors.append(f"ARES_TEST_SOURCE is {head}, expected {expected}")
			if dirty:
				errors.append("ARES_TEST_SOURCE has tracked modifications; use a clean pinned checkout")

	if errors:
		print("full validation preflight failed:", file=sys.stderr)
		for error in errors:
			print(f"  - {error}", file=sys.stderr)
		print("full mode never skips required SDK, runner, source-provenance, workspace, or host gates", file=sys.stderr)
		raise SystemExit(2)

	assert sdk is not None and runner is not None and runner_source is not None
	assert container_identity is not None
	return FullDependencies(root, sdk, runner, runner_source, container_identity, lock)


def copy_fixture(source: Path, destination: Path) -> None:
	destination.mkdir(parents=True)
	prefix = source.relative_to(ODIN_ROOT)
	tracked = git_output(ODIN_ROOT, "ls-files", "--", str(prefix)).splitlines()
	for relative in tracked:
		path = ODIN_ROOT / relative
		target = destination / path.relative_to(source)
		target.parent.mkdir(parents=True, exist_ok=True)
		shutil.copy2(path, target)


def prepare_full_fixtures(artifacts: Path) -> dict[str, Path]:
	fixtures = {
		"tracer": artifacts / "fixtures/tracer",
		"pong": artifacts / "fixtures/pong",
		"dfs": artifacts / "fixtures/dfs",
	}
	for name, destination in fixtures.items():
		copy_fixture(ODIN_ROOT / f"tests/n64_{name}", destination)
	return fixtures


def full_stages(validation: FullValidation) -> list[Stage]:
	root = validation.dependencies.root
	sdk = validation.dependencies.sdk
	runner = validation.dependencies.runner
	llvm = root / "llvm-project"
	common_environment = {
		"N64_INST": str(sdk),
		"ARES_TEST": str(runner),
		"ODIN": str(ODIN_ROOT / "odin"),
		"N64_VALIDATION_MODE": "full",
		"MIPS_O64_GCC": str(sdk / "bin/mips64-elf-gcc"),
		"MIPS_O64_OBJDUMP": str(sdk / "bin/mips64-elf-objdump"),
	}
	clang = Path(os.environ.get("LLVM_CLANG", root / "build/bin/clang")).expanduser().resolve()

	tracer = validation.fixtures["tracer"]
	pong = validation.fixtures["pong"]
	dfs = validation.fixtures["dfs"]
	expected_hashes = validation.rom_identities.hashes
	stages = [Stage("build compiler from tested tree", ("./build_odin.sh", "release"))]
	stages.extend(quick_stages(root / "toolchain.lock.toml"))
	stages.extend([
		Stage("validate pinned SDK", (PYTHON, "tests/o64_abi/validate_sdk.py", str(sdk)), environment=common_environment),
		Stage("N64 public build suite", (PYTHON, "tests/n64_build/test_n64_build.py"), environment=common_environment),
		Stage(
			"LLVM O64 ABI differential",
			(PYTHON, str(llvm / "llvm/utils/o64-abi-differential.py"), "--gcc", str(sdk / "bin/mips64-elf-gcc"), "--clang", str(clang)),
			cwd=root,
			environment=common_environment,
		),
		Stage("Odin O64 ABI differential", (PYTHON, "tests/o64_abi/differential.py"), environment=common_environment),
		Stage(
			"linked O64 ABI ROM",
			("make", "-C", "tests/o64_abi/interop", "clean", "all", "check"),
			environment=common_environment,
		),
		Stage(
			"libdragon binding layout and runtime",
			("make", "-C", "tests/o64_abi/libdragon_bindings", "clean", "all", "check-layout", "check"),
			environment=common_environment,
		),
		Stage(
			"console lifecycle",
			("make", "-C", "tests/n64_console", "clean", "all", "check"),
			environment=common_environment,
		),
		Stage(
			"build tracer from clean sample copy",
			(
				str(ODIN_ROOT / "odin"), "build", ".", "-target:n64", "-out:n64_tracer.z64",
				"-source-code-locations:filename", "-keep-temp-files",
			),
			cwd=tracer,
			environment=common_environment,
		),
		Stage(
			"tracer ROM identity",
			(PYTHON, str(ODIN_ROOT / "tests/n64_validation/check_sha256.py"), "n64_tracer.z64", expected_hashes["tracer_rom_sha256"]),
			cwd=tracer,
		),
		Stage("tracer golden", (str(runner), "tracer.test.js", "n64_tracer.z64", "--timeout", "30"), cwd=tracer),
		Stage(
			"build Pong from clean sample copy",
			(
				str(ODIN_ROOT / "odin"), "build", ".", "-target:n64", "-out:n64_pong.z64",
				"-source-code-locations:filename", "-keep-temp-files",
			),
			cwd=pong,
			environment=common_environment,
		),
		Stage(
			"Pong ROM identity",
			(PYTHON, str(ODIN_ROOT / "tests/n64_validation/check_sha256.py"), "n64_pong.z64", expected_hashes["pong_rom_sha256"]),
			cwd=pong,
		),
		Stage("Pong golden", (str(runner), "pong.test.js", "n64_pong.z64", "--timeout", "30"), cwd=pong),
		Stage(
			"build DFS from clean sample copy",
			(
				str(ODIN_ROOT / "odin"), "build", ".", "-target:n64", "-out:n64_dfs.z64",
				"-n64-title:Odin DFS v0.2", "-n64-region:E", "-n64-save-type:none",
				"-n64-controllers:n64;none;none;none", "-n64-assets:assets",
				"-n64-metadata:metadata.ini", "-source-code-locations:filename", "-keep-temp-files",
			),
			cwd=dfs,
			environment=common_environment,
		),
		Stage(
			"DFS ROM identity",
			(PYTHON, str(ODIN_ROOT / "tests/n64_validation/check_sha256.py"), "n64_dfs.z64", expected_hashes["dfs_rom_sha256"]),
			cwd=dfs,
		),
		Stage("DFS golden", (str(runner), "dfs.test.js", "n64_dfs.z64", "--timeout", "30"), cwd=dfs),
	])
	return stages


def sdk_identity(sdk: Path) -> dict[str, object]:
	identity: dict[str, object] = {"path": str(sdk)}
	for name, relative in {
		"libdragon": "mips64-elf/include/libdragon.version",
		"toolchain": "mips64-elf/include/toolchain.version",
	}.items():
		path = sdk / relative
		if path.is_file():
			try:
				identity[name] = json.loads(path.read_text(encoding="utf-8"))
			except (OSError, json.JSONDecodeError):
				identity[name] = {"unreadable": str(path)}
	makefile = sdk / "include/n64.mk"
	if makefile.is_file():
		identity["n64_make_sha256"] = hashlib.sha256(makefile.read_bytes()).hexdigest()
	return identity


def write_identity_manifest(path: Path, mode: str, inputs: IdentityInputs) -> None:
	manifest: dict[str, object] = {
		"mode": mode,
		"host": {"system": platform.system(), "machine": platform.machine(), "platform": platform.platform()},
		"python": sys.version,
		"repositories": {},
	}
	for name, repository in (
		("odin", ODIN_ROOT),
		("workspace", inputs.root),
		("llvm", inputs.root / "llvm-project" if inputs.root else None),
		("ares_test_source", inputs.runner_source),
	):
		if repository is None or not (repository / ".git").exists():
			continue
		try:
			manifest["repositories"][name] = {
				"head": git_output(repository, "rev-parse", "HEAD"),
				"branch": git_output(repository, "branch", "--show-current"),
				"status": git_output(repository, "status", "--short", "--untracked-files=all"),
			}
		except subprocess.CalledProcessError:
			pass
	if inputs.sdk is not None:
		manifest["sdk"] = sdk_identity(inputs.sdk)
	if inputs.runner is not None:
		manifest["ares_test"] = {
			"path": str(inputs.runner),
			"sha256": hashlib.sha256(inputs.runner.read_bytes()).hexdigest(),
		}
	if inputs.container_identity is not None:
		manifest["container_identity"] = inputs.container_identity
	if inputs.rom_identity_source is not None:
		manifest["rom_identity_source"] = inputs.rom_identity_source
	if inputs.candidate_manifest is not None:
		manifest["candidate_manifest"] = {
			"path": str(inputs.candidate_manifest.path),
			"sha256": inputs.candidate_manifest.sha256,
		}
	path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def run_stages(stages: list[Stage], artifacts: Path) -> int:
	artifacts.mkdir(parents=True, exist_ok=True)
	for index, stage in enumerate(stages, 1):
		label = f"[{index}/{len(stages)}] {stage.name}"
		print(f"\n==> {label}", flush=True)
		log_name = re.sub(r"[^a-z0-9]+", "-", stage.name.lower()).strip("-") + ".log"
		log_path = artifacts / log_name
		environment = os.environ.copy()
		environment.update(stage.environment)
		with log_path.open("w", encoding="utf-8") as log:
			process = subprocess.Popen(
				stage.command,
				cwd=stage.cwd,
				env=environment,
				text=True,
				stdout=subprocess.PIPE,
				stderr=subprocess.STDOUT,
			)
			assert process.stdout is not None
			for line in process.stdout:
				print(line, end="", flush=True)
				log.write(line)
			returncode = process.wait()
		if returncode != 0:
			print(f"\nFAILED: {stage.name} (exit {returncode})", file=sys.stderr)
			print(f"log retained at {log_path}", file=sys.stderr)
			print(f"all validation artifacts retained at {artifacts}", file=sys.stderr)
			return returncode or 1
		print(f"PASS: {stage.name}", flush=True)
	return 0


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("mode", choices=("quick", "full"))
	parser.add_argument("--list", action="store_true", help="list stages without running preflight or commands")
	parser.add_argument("--artifacts", type=Path, help="directory for stage logs and full fixture copies")
	parser.add_argument(
		"--candidate-manifest",
		type=Path,
		help="absolute path to proposed full-mode ROM identities; omit to use accepted_artifacts",
	)
	args = parser.parse_args()
	if args.candidate_manifest is not None and args.mode != "full":
		parser.error("--candidate-manifest is only valid with full mode")
	if args.candidate_manifest is not None and not args.candidate_manifest.is_absolute():
		parser.error("--candidate-manifest must be an absolute path")

	root: Path | None = workspace_root()
	sdk: Path | None = None
	runner: Path | None = None
	runner_source: Path | None = None
	container_identity: str | None = None
	artifacts: Path | None = None
	rom_identities: RomIdentities | None = None
	retained_candidate_manifest: RetainedCandidateManifest | None = None
	if not args.list:
		artifact_base = args.artifacts or Path(
			os.environ.get("N64_VALIDATION_ARTIFACTS", ODIN_ROOT / ".n64-validation-artifacts")
		)
		run_name = f"{time.strftime('%Y%m%d-%H%M%S')}-{os.getpid()}"
		artifacts = artifact_base.expanduser().resolve() / run_name
	if args.mode == "quick":
		stages = quick_stages()
	elif args.list:
		# Full stage names do not depend on a live release environment.
		stages = [
			Stage("build compiler from tested tree", ()),
			*quick_stages(),
			Stage("validate pinned SDK", ()),
			Stage("N64 public build suite", ()),
			Stage("LLVM O64 ABI differential", ()),
			Stage("Odin O64 ABI differential", ()),
			Stage("linked O64 ABI ROM", ()),
			Stage("libdragon binding layout and runtime", ()),
			Stage("console lifecycle", ()),
			Stage("build tracer from clean sample copy", ()),
			Stage("tracer ROM identity", ()),
			Stage("tracer golden", ()),
			Stage("build Pong from clean sample copy", ()),
			Stage("Pong ROM identity", ()),
			Stage("Pong golden", ()),
			Stage("build DFS from clean sample copy", ()),
			Stage("DFS ROM identity", ()),
			Stage("DFS golden", ()),
		]
	else:
		if args.candidate_manifest is not None:
			try:
				odin_commit = git_output(ODIN_ROOT, "rev-parse", "HEAD")
				candidate_manifest = load_candidate_manifest(args.candidate_manifest, odin_commit)
			except (CandidateManifestError, subprocess.CalledProcessError) as error:
				parser.error(str(error))
			rom_identities = candidate_rom_identities(candidate_manifest)
		dependencies = full_preflight()
		root = dependencies.root
		sdk = dependencies.sdk
		runner = dependencies.runner
		runner_source = dependencies.runner_source
		container_identity = dependencies.container_identity
		assert artifacts is not None
		if rom_identities is None:
			rom_identities = accepted_rom_identities(dependencies.lock)
		else:
			assert rom_identities.candidate_manifest is not None
			artifacts.mkdir(parents=True, exist_ok=True)
			try:
				retained_candidate_manifest = retain_candidate_manifest(
					rom_identities.candidate_manifest, artifacts
				)
			except (CandidateManifestError, OSError) as error:
				parser.error(f"cannot retain candidate manifest: {error}")
		validation = FullValidation(
			dependencies, artifacts, prepare_full_fixtures(artifacts), rom_identities
		)
		stages = full_stages(validation)

	if args.list:
		for stage in stages:
			print(stage.name)
		return 0

	if args.mode == "quick" and not (ODIN_ROOT / "odin").is_file():
		parser.error("build the Odin compiler first; expected ./odin")

	assert artifacts is not None
	artifacts.mkdir(parents=True, exist_ok=True)
	identity = IdentityInputs(
		root, sdk, runner, runner_source, container_identity,
		rom_identities.label if rom_identities is not None else None,
		retained_candidate_manifest,
	)
	write_identity_manifest(artifacts / "identity.json", args.mode, identity)
	if rom_identities is not None:
		print(f"N64 full validation ROM identities: {rom_identities.label}")
	print(f"N64 {args.mode} validation artifacts: {artifacts}")
	result = run_stages(stages, artifacts)
	if result == 0:
		print(f"\nN64 {args.mode} validation passed; logs retained at {artifacts}")
	return result


if __name__ == "__main__":
	raise SystemExit(main())
