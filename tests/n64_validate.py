#!/usr/bin/env python3
"""Run compiler-owned N64 checks; cross-repository integration lives in Odin64."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field

ODIN_ROOT = Path(__file__).resolve().parents[1]
PYTHON = sys.executable

@dataclass(frozen=True)
class Stage:
    name: str
    command: tuple[str, ...]
    cwd: Path = ODIN_ROOT
    environment: dict[str, str] = field(default_factory=dict)

def quick_stages(lock_path: Path | None = None) -> list[Stage]:
    environment = {"N64_VALIDATION_MODE": "quick", "ODIN": str(ODIN_ROOT / "odin")}
    pins = {"ODIN_N64_TOOLCHAIN_LOCK": str(lock_path)} if lock_path else {}
    return [
        Stage("active pin drift", (PYTHON, "tests/n64_validation/check_active_pins.py"), environment=pins),
        Stage("documentation links", (PYTHON, "tests/n64_validation/check_documentation_links.py")),
        Stage("validation contract", (PYTHON, "tests/n64_validation/test_validation_contract.py")),
        Stage("N64 build-module boundary", (PYTHON, "tests/n64_build/test_n64_module.py")),
        Stage("N64 public options and failure paths", (PYTHON, "tests/n64_build/test_n64_build.py"), environment=environment),
        Stage("SDK validator unit tests", (PYTHON, "tests/o64_abi/test_validate_sdk.py")),
        Stage("standalone runtime probe", ("./odin", "check", "tests/n64_runtime", "-target:n64", "-vet", "-warnings-as-errors")),
    ]

def full_stages(sdk: Path, runner: str, artifacts: Path) -> list[Stage]:
    environment = {
        "N64_INST": str(sdk), "ARES_TEST": runner, "ODIN": str(ODIN_ROOT / "odin"),
        "N64_VALIDATION_MODE": "full",
        "MIPS_O64_GCC": str(sdk / "bin/mips64-elf-gcc"),
        "MIPS_O64_OBJDUMP": str(sdk / "bin/mips64-elf-objdump"),
    }
    rom = str(artifacts / "runtime.z64")
    return [
        Stage("validate pinned SDK", (PYTHON, "tests/o64_abi/validate_sdk.py", str(sdk)), environment=environment),
        Stage("N64 public build suite", (PYTHON, "tests/n64_build/test_n64_build.py"), environment=environment),
        Stage("Odin O64 ABI differential", (PYTHON, "tests/o64_abi/differential.py"), environment=environment),
        Stage("linked O64 ABI ROM", ("make", "-C", "tests/o64_abi/interop", "clean", "all", "check"), environment=environment),
        Stage("build standalone runtime ROM", ("./odin", "build", "tests/n64_runtime", "-target:n64", f"-out:{rom}"), environment=environment),
        Stage("standalone runtime lifecycle", (runner, "tests/n64_runtime/runtime.test.js", rom, "--timeout", "30"), environment=environment),
    ]

def run_stages(stages: list[Stage], artifacts: Path) -> int:
    artifacts.mkdir(parents=True, exist_ok=True)
    for index, stage in enumerate(stages, 1):
        print(f"\n==> [{index}/{len(stages)}] {stage.name}", flush=True)
        name = re.sub(r"[^a-z0-9]+", "-", stage.name.lower()).strip("-") + ".log"
        log_path = artifacts / name
        with log_path.open("w", encoding="utf-8") as log:
            try:
                process = subprocess.Popen(stage.command, cwd=stage.cwd,
                    env={**os.environ, **stage.environment}, text=True,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                assert process.stdout is not None
                for line in process.stdout:
                    print(line, end="", flush=True)
                    log.write(line)
                process.stdout.close()
                result = process.wait()
            except OSError as error:
                print(str(error), file=log)
                print(f"Cannot start {stage.name}: {error}", file=sys.stderr)
                result = 1
        if result:
            print(f"FAILED: {stage.name}; retained log: {log_path}", file=sys.stderr)
            return result
        print(f"PASS: {stage.name}", flush=True)
    return 0

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("quick", "full"))
    parser.add_argument("--list", action="store_true", help="list stages without running checks")
    parser.add_argument("--artifacts", type=Path, help="parent directory for unique retained logs")
    args = parser.parse_args()
    stages = quick_stages()
    sdk = Path(os.environ.get("N64_INST", "/missing-sdk")).expanduser().resolve()
    runner = os.environ.get("ARES_TEST", "ares-test")
    if args.list:
        if args.mode == "full":
            stages += full_stages(sdk, runner, Path("/artifacts"))
        for stage in stages:
            print(stage.name)
        return 0
    if not (ODIN_ROOT / "odin").is_file():
        parser.error("build Odin first using an explicit compatible LLVM_CONFIG")
    if args.mode == "full":
        if not os.environ.get("N64_INST") or not sdk.is_dir():
            parser.error("full mode requires explicit N64_INST; see N64_BUILD.md")
        if not os.environ.get("ARES_TEST") or not shutil.which(runner):
            parser.error("full mode requires an executable ARES_TEST; use Odin64 for pinned cross-repository qualification")
    base = (args.artifacts or ODIN_ROOT / ".n64-validation-artifacts").expanduser().resolve()
    base.mkdir(parents=True, exist_ok=True)
    artifacts = Path(tempfile.mkdtemp(prefix=f"{args.mode}-", dir=base))
    if args.mode == "full":
        stages += full_stages(sdk, runner, artifacts)
    identity = {"mode": args.mode, "scope": "compiler-only", "release_qualified": False,
        "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ODIN_ROOT, text=True).strip(),
        "status": subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=all"], cwd=ODIN_ROOT, text=True)}
    (artifacts / "identity.json").write_text(json.dumps(identity, indent=2) + "\n", encoding="utf-8")
    print(f"Compiler validation artifacts: {artifacts}")
    return run_stages(stages, artifacts)

if __name__ == "__main__":
    raise SystemExit(main())
