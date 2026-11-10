#!/usr/bin/env python3
"""Guard standalone compiler validation ownership and failure behavior."""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("compiler_validation", ROOT / "tests/n64_validate.py")
driver = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = driver
spec.loader.exec_module(driver)

class ValidationContract(unittest.TestCase):
    def test_source_snapshot_runs_checks_without_git_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            checks = root / "tests/n64_validation"
            checks.mkdir(parents=True)
            (root / "odin").touch()
            shutil.copy2(ROOT / "tests/n64_validate.py", root / "tests/n64_validate.py")
            (checks / "check_documentation_links.py").write_text(
                "print('snapshot check reached'); raise SystemExit(7)\n")
            artifacts = root / "artifacts"

            result = subprocess.run(
                (sys.executable, str(root / "tests/n64_validate.py"), "quick", "--artifacts", str(artifacts)),
                cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

            self.assertEqual(result.returncode, 7, result.stdout)
            run = next(artifacts.iterdir())
            self.assertIn("snapshot check reached", (run / "documentation-links.log").read_text())
            self.assertFalse((run / "identity.json").exists())

    def test_quick_does_not_require_matching_toolchain_pins(self):
        stages = driver.quick_stages()
        self.assertNotIn("active pin drift", [stage.name for stage in stages])

    def test_quick_is_sdk_free_and_binding_independent(self):
        stages = driver.quick_stages()
        text = repr(stages)
        for forbidden in ("vendor:libdragon", "odin64:", "n64_pong", "n64_tracer", "libdragon_bindings"):
            self.assertNotIn(forbidden, text)
        self.assertIn("tests/n64_runtime", text)
        self.assertIn("-build-mode:obj", text)
        self.assertIn("tests/n64_core_mem", text)
        self.assertIn("N64_VALIDATION_MODE", text)
        self.assertIn("quick", text)

    def test_full_retains_compiler_runtime_and_abi_only(self):
        stages = driver.full_stages(Path("/sdk"), "/runner", Path("/artifacts"))
        names = [stage.name for stage in stages]
        self.assertIn("N64 public build suite", names)
        self.assertIn("Odin O64 ABI differential", names)
        self.assertIn("standalone runtime lifecycle", names)
        self.assertNotIn("cmake", repr(stages))
        self.assertNotIn("build_odin", repr(stages))

    def test_first_failure_stops_and_retains_log(self):
        with tempfile.TemporaryDirectory() as directory:
            stages = [
                driver.Stage("fail here", (sys.executable, "-c", "print('failure evidence'); raise SystemExit(7)")),
                driver.Stage("must not run", (sys.executable, "-c", "raise AssertionError")),
            ]
            self.assertEqual(driver.run_stages(stages, Path(directory)), 7)
            self.assertIn("failure evidence", (Path(directory) / "fail-here.log").read_text())
            self.assertFalse((Path(directory) / "must-not-run.log").exists())

    def test_runtime_uses_only_local_logging_declarations(self):
        contents = (ROOT / "tests/n64_runtime/runtime.odin").read_text()
        self.assertNotIn("vendor:", contents)
        self.assertNotIn("odin64:", contents)
        for marker in ("GENERAL_ALLOCATOR", "TEMP_ALLOCATOR", "ALLOCATOR_REPLACEABILITY", "CLEANUP"):
            self.assertIn(marker, contents)

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

FORK_ENVIRONMENT = ("N64_VALIDATION_MODE", "N64_INST", "MIPS_O64_OBJDUMP", "N64_REQUIRE_O64_FORK")

class O64ForkRequirement(unittest.TestCase):
    """CI's fork job sets N64_REQUIRE_O64_FORK=1 so ELF64 output fails the O64-only checks instead of skipping them."""

    def setUp(self):
        self.stage = load("o64_fork", "tests/n64_validation/o64_fork.py")
        self.asm = load("n64_asm", "tests/n64_asm/test_n64_asm.py")
        environment = {key: value for key, value in os.environ.items() if key not in FORK_ENVIRONMENT}
        patcher = mock.patch.dict(os.environ, environment, clear=True)
        patcher.start()
        self.addCleanup(patcher.stop)

    def require(self, elf32):
        """Run both fork gates against a compiler that writes ELF32 (`elf32`) or ELF64 objects."""
        outcomes = []
        with mock.patch.object(self.stage, "emits_elf32_objects", return_value=elf32):
            try:
                self.stage.require_o64_fork("odin")
                outcomes.append("ran")
            except unittest.SkipTest:
                outcomes.append("skipped")
            except AssertionError:
                outcomes.append("failed")
        test = mock.Mock(spec=unittest.TestCase)
        test.fail.side_effect = AssertionError
        test.skipTest.side_effect = unittest.SkipTest
        with mock.patch.object(self.asm, "emits_elf32_objects", return_value=elf32):
            try:
                self.asm.require_o64_objects(test)
                outcomes.append("ran")
            except unittest.SkipTest:
                outcomes.append("skipped")
            except AssertionError:
                outcomes.append("failed")
        return outcomes

    def test_elf64_skips_without_a_fork_expectation(self):
        self.assertEqual(self.require(elf32=False), ["skipped", "skipped"])

    def test_elf64_skips_when_the_switch_is_not_1(self):
        for setting in ({"N64_VALIDATION_MODE": "quick"}, {"N64_REQUIRE_O64_FORK": "0"},
                        {"N64_REQUIRE_O64_FORK": ""}, {"N64_REQUIRE_O64_FORK": "true"}):
            with self.subTest(setting), mock.patch.dict(os.environ, setting):
                self.assertEqual(self.require(elf32=False), ["skipped", "skipped"])

    def test_each_fork_expectation_turns_elf64_skips_into_failures(self):
        for setting in ({"N64_REQUIRE_O64_FORK": "1"}, {"N64_VALIDATION_MODE": "full"},
                        {"N64_INST": "/sdk"}, {"MIPS_O64_OBJDUMP": "/sdk/bin/mips64-elf-objdump"}):
            with self.subTest(setting), mock.patch.dict(os.environ, setting):
                self.assertEqual(self.require(elf32=False), ["failed", "failed"])

    def test_required_fork_runs_checks_on_elf32(self):
        os.environ["N64_REQUIRE_O64_FORK"] = "1"
        self.assertEqual(self.require(elf32=True), ["ran", "ran"])

if __name__ == "__main__":
    unittest.main(verbosity=2)
