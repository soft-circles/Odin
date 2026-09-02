#!/usr/bin/env python3
"""Guard standalone compiler validation ownership and failure behavior."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("compiler_validation", ROOT / "tests/n64_validate.py")
driver = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = driver
spec.loader.exec_module(driver)

class ValidationContract(unittest.TestCase):
    def test_quick_is_sdk_free_and_binding_independent(self):
        stages = driver.quick_stages()
        text = repr(stages)
        for forbidden in ("vendor:libdragon", "odin64:", "n64_pong", "n64_tracer", "libdragon_bindings"):
            self.assertNotIn(forbidden, text)
        self.assertIn("tests/n64_runtime", text)
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

if __name__ == "__main__":
    unittest.main(verbosity=2)
