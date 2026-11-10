"""Skip machine-code checks that only the MIPS O64 LLVM fork can pass.

Stock LLVM lowers -target:n64 with the n64 ABI's 64-bit pointers and writes ELF64 objects, so the assembly it
generates for the fork's O64 IR (32-bit pointers, aggregates in 8-byte slots) says nothing about the N64 build.
CI's stock-LLVM quick job skips these checks. Full mode, an SDK and CI's LLVM-fork job (N64_REQUIRE_O64_FORK=1) all
come with the fork, so stock output fails instead.
"""
import functools
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

NOT_O64_FORK = "compiler emits ELF64: not the MIPS O64 LLVM fork; checked in full mode / with the fork"


def o64_fork_expected():
    return (os.environ.get("N64_VALIDATION_MODE") == "full"
            or bool(os.environ.get("N64_INST")) or bool(os.environ.get("MIPS_O64_OBJDUMP"))
            or os.environ.get("N64_REQUIRE_O64_FORK") == "1")


@functools.lru_cache(maxsize=None)
def emits_elf32_objects(odin):
    with tempfile.TemporaryDirectory(prefix="odin-n64-elf-class-") as directory:
        package = Path(directory) / "probe"
        package.mkdir()
        (package / "probe.odin").write_text('package probe\n\n@(export) probe :: proc "contextless" () {}\n')
        out = Path(directory) / "probe.o"
        subprocess.run([str(odin), "build", str(package), "-target:n64", "-build-mode:obj", "-no-entry-point",
                        "-use-single-module", f"-out:{out}"], check=True)
        return out.read_bytes()[4] == 1  # EI_CLASS: 1 = ELF32, 2 = ELF64


def require_o64_fork(odin):
    """Raise SkipTest unless `odin` uses the O64 fork; fail instead where the fork is expected."""
    if emits_elf32_objects(odin):
        return
    if o64_fork_expected():
        raise AssertionError(f"{NOT_O64_FORK}; N64_VALIDATION_MODE=full, N64_INST, MIPS_O64_OBJDUMP or "
                             "N64_REQUIRE_O64_FORK=1 requires the fork")
    raise unittest.SkipTest(NOT_O64_FORK)
