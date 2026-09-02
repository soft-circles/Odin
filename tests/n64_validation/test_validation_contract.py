#!/usr/bin/env python3
"""Public-command contract tests for the N64 validation driver."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ODIN_ROOT = Path(__file__).resolve().parents[2]
DRIVER = ODIN_ROOT / "tests/n64_validate.py"
SPEC = importlib.util.spec_from_file_location("n64_validate", DRIVER)
assert SPEC is not None and SPEC.loader is not None
VALIDATION = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = VALIDATION
SPEC.loader.exec_module(VALIDATION)


def candidate_manifest_text(
	odin_commit: str,
	tracer_hash: str = "a" * 64,
	pong_hash: str = "b" * 64,
	dfs_hash: str = "c" * 64,
) -> str:
	return f'''schema = 1
release = "0.2.1"
odin_commit = "{odin_commit}"

[roms]
tracer_sha256 = "{tracer_hash}"
pong_sha256 = "{pong_hash}"
dfs_sha256 = "{dfs_hash}"
'''


class N64ValidationContractTests(unittest.TestCase):
	def full_stages(self, identities):
		root = ODIN_ROOT.parent
		dependencies = VALIDATION.FullDependencies(
			root, Path("/sdk"), Path("/runner"), Path("/runner-source"), "container@sha256:test", {}
		)
		fixtures = {name: Path(f"/{name}") for name in ("tracer", "pong", "dfs")}
		validation = VALIDATION.FullValidation(dependencies, Path("/artifacts"), fixtures, identities)
		return VALIDATION.full_stages(validation)

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

	def test_all_release_fixture_builds_normalize_source_locations(self):
		identities = VALIDATION.RomIdentities(
			"accepted",
			{f"{name}_rom_sha256": name[0] * 64 for name in ("tracer", "pong", "dfs")},
		)
		stages_by_name = {stage.name: stage for stage in self.full_stages(identities)}
		for fixture in ("tracer", "Pong", "DFS"):
			with self.subTest(fixture=fixture):
				command = stages_by_name[f"build {fixture} from clean sample copy"].command
				self.assertEqual(command.count("-source-code-locations:filename"), 1, command)

	def test_accepted_mode_uses_accepted_artifacts_unchanged(self):
		accepted = {
			"tracer_rom_sha256": "1" * 64,
			"pong_rom_sha256": "2" * 64,
			"dfs_rom_sha256": "3" * 64,
		}
		identities = VALIDATION.accepted_rom_identities({"accepted_artifacts": accepted})
		self.assertEqual(identities.label, "accepted")
		self.assertIs(identities.hashes, accepted)
		self.assertIsNone(identities.candidate_manifest)
		identity_commands = {
			stage.name: stage.command for stage in self.full_stages(identities) if "ROM identity" in stage.name
		}
		self.assertEqual(identity_commands["tracer ROM identity"][-1], accepted["tracer_rom_sha256"])
		self.assertEqual(identity_commands["Pong ROM identity"][-1], accepted["pong_rom_sha256"])
		self.assertEqual(identity_commands["DFS ROM identity"][-1], accepted["dfs_rom_sha256"])
		with tempfile.TemporaryDirectory(prefix="odin-n64-accepted-") as directory:
			identity_path = Path(directory) / "identity.json"
			VALIDATION.write_identity_manifest(
				identity_path,
				"full",
				VALIDATION.IdentityInputs(root=None, rom_identity_source=identities.label),
			)
			identity = json.loads(identity_path.read_text(encoding="utf-8"))
			self.assertEqual(identity["rom_identity_source"], "accepted")
			self.assertNotIn("candidate_manifest", identity)

	def test_candidate_manifest_selects_hashes_and_is_retained_in_identity(self):
		odin_commit = "d" * 40
		with tempfile.TemporaryDirectory(prefix="odin-n64-candidate-") as directory:
			root = Path(directory)
			manifest_path = root / "candidate.toml"
			manifest_path.write_text(candidate_manifest_text(odin_commit), encoding="utf-8")

			manifest = VALIDATION.load_candidate_manifest(manifest_path, odin_commit)
			identities = VALIDATION.candidate_rom_identities(manifest)
			self.assertEqual(identities.label, "candidate")
			identity_commands = {
				stage.name: stage.command for stage in self.full_stages(identities) if "ROM identity" in stage.name
			}
			self.assertEqual(identity_commands["tracer ROM identity"][-1], "a" * 64)
			self.assertEqual(identity_commands["Pong ROM identity"][-1], "b" * 64)
			self.assertEqual(identity_commands["DFS ROM identity"][-1], "c" * 64)

			artifacts = root / "artifacts"
			artifacts.mkdir()
			retained = VALIDATION.retain_candidate_manifest(manifest, artifacts)
			identity_path = artifacts / "identity.json"
			VALIDATION.write_identity_manifest(
				identity_path,
				"full",
				VALIDATION.IdentityInputs(
					root=None,
					rom_identity_source="candidate",
					candidate_manifest=retained,
				),
			)
			identity = json.loads(identity_path.read_text(encoding="utf-8"))
			self.assertEqual(identity["rom_identity_source"], "candidate")
			self.assertEqual(identity["candidate_manifest"]["path"], str(retained.path))
			self.assertEqual(
				identity["candidate_manifest"]["sha256"],
				hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
			)
			self.assertEqual(retained.path.read_bytes(), manifest_path.read_bytes())

	def test_candidate_manifest_requires_an_absolute_path(self):
		with self.assertRaisesRegex(VALIDATION.CandidateManifestError, "must be absolute"):
			VALIDATION.load_candidate_manifest(Path("candidate.toml"), "d" * 40)

	def test_candidate_manifest_rejects_missing_required_fields(self):
		odin_commit = "d" * 40
		valid = candidate_manifest_text(odin_commit)
		missing_lines = {
			"schema": "schema = 1\n",
			"release": 'release = "0.2.1"\n',
			"odin_commit": f'odin_commit = "{odin_commit}"\n',
			"roms": "[roms]\n",
			"tracer": f'tracer_sha256 = "{"a" * 64}"\n',
			"pong": f'pong_sha256 = "{"b" * 64}"\n',
			"dfs": f'dfs_sha256 = "{"c" * 64}"\n',
		}
		with tempfile.TemporaryDirectory(prefix="odin-n64-candidate-missing-") as directory:
			path = Path(directory) / "candidate.toml"
			for field, line in missing_lines.items():
				with self.subTest(field=field):
					path.write_text(valid.replace(line, ""), encoding="utf-8")
					with self.assertRaisesRegex(VALIDATION.CandidateManifestError, "missing"):
						VALIDATION.load_candidate_manifest(path, odin_commit)

	def test_candidate_manifest_rejects_invalid_values_and_commit_mismatch(self):
		odin_commit = "d" * 40
		cases = {
			"schema": (candidate_manifest_text(odin_commit).replace("schema = 1", "schema = 2"), "schema"),
			"release": (candidate_manifest_text(odin_commit).replace("0.2.1", "0.2.2"), "release"),
			"malformed commit": (candidate_manifest_text("D" * 40), "odin_commit"),
			"commit mismatch": (candidate_manifest_text("e" * 40), "checked-out Odin HEAD"),
			"uppercase hash": (candidate_manifest_text(odin_commit, tracer_hash="A" * 64), "lowercase"),
			"short hash": (candidate_manifest_text(odin_commit, pong_hash="b" * 63), "lowercase"),
			"non-hex hash": (candidate_manifest_text(odin_commit, dfs_hash="g" * 64), "lowercase"),
		}
		with tempfile.TemporaryDirectory(prefix="odin-n64-candidate-invalid-") as directory:
			path = Path(directory) / "candidate.toml"
			for case, (contents, error) in cases.items():
				with self.subTest(case=case):
					path.write_text(contents, encoding="utf-8")
					with self.assertRaisesRegex(VALIDATION.CandidateManifestError, error):
						VALIDATION.load_candidate_manifest(path, odin_commit)

	def test_candidate_manifest_option_is_full_mode_only(self):
		result = subprocess.run(
			[sys.executable, str(DRIVER), "quick", "--candidate-manifest", "/tmp/candidate.toml"],
			cwd=ODIN_ROOT,
			text=True,
			stdout=subprocess.PIPE,
			stderr=subprocess.STDOUT,
			check=False,
		)
		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertIn("only valid with full mode", result.stdout)

		result = subprocess.run(
			[sys.executable, str(DRIVER), "full", "--list", "--candidate-manifest", "candidate.toml"],
			cwd=ODIN_ROOT,
			text=True,
			stdout=subprocess.PIPE,
			stderr=subprocess.STDOUT,
			check=False,
		)
		self.assertNotEqual(result.returncode, 0, result.stdout)
		self.assertIn("must be an absolute path", result.stdout)


if __name__ == "__main__":
	unittest.main(verbosity=2)
