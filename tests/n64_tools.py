"""Locate the SDK and shared LLVM differential helper for N64 ABI tests."""

from __future__ import annotations

import importlib.util
import os
import sys
from pathlib import Path


ODIN_ROOT = Path(__file__).resolve().parents[1]
N64_INST = Path(os.environ.get("N64_INST", Path.home() / "n64_toolchain")).expanduser()
PHASE0 = Path(os.environ.get(
	"O64_PHASE0", ODIN_ROOT.parent / "llvm-project/llvm/utils/o64-abi-differential.py"))


def load_phase0():
	if not PHASE0.is_file():
		sys.exit(f"O64 phase-0 differential script not found: {PHASE0} (set O64_PHASE0)")
	spec = importlib.util.spec_from_file_location("o64_phase0", PHASE0)
	module = importlib.util.module_from_spec(spec)
	spec.loader.exec_module(module)
	return module
