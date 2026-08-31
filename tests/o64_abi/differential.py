#!/usr/bin/env python3
"""Execute Odin and GCC O64 leaf probes from identical ABI-visible inputs."""

import importlib.util
import os
import subprocess
import sys
import tempfile
from pathlib import Path


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
WORKSPACE = ROOT.parent
PHASE0 = WORKSPACE / "llvm-project/llvm/utils/o64-abi-differential.py"
GCC = Path(os.environ.get("MIPS_O64_GCC", Path.home() / "n64_toolchain/bin/mips64-elf-gcc"))
ODIN = Path(os.environ.get("ODIN", ROOT / "odin"))


def load_phase0():
	spec = importlib.util.spec_from_file_location("o64_phase0", PHASE0)
	module = importlib.util.module_from_spec(spec)
	spec.loader.exec_module(module)
	return module


def run(command):
	result = subprocess.run(command, cwd=HERE, capture_output=True, text=True)
	if result.returncode:
		sys.stderr.write(result.stdout)
		sys.stderr.write(result.stderr)
		raise SystemExit(f"command failed: {' '.join(map(str, command))}")
	return result.stdout


def main():
	phase0 = load_phase0()
	with tempfile.TemporaryDirectory(prefix="odin-o64-abi-") as directory:
		odin_asm = Path(directory) / "odin.s"
		run([
			ODIN, "build", HERE / "scalars.odin", "-file",
			"-target:freestanding_mips32be", "-build-mode:asm", "-o:speed",
			"-no-entry-point", "-no-thread-local", "-default-to-nil-allocator",
			"-reloc-mode:static", f"-out:{odin_asm}",
		])
		gcc_text = run([
			GCC, "-march=vr4300", "-mabi=o64", "-mno-abicalls", "-fno-pic",
			"-fomit-frame-pointer", "-O2", "-S", "-o", "-", HERE / "scalars.c",
		])
		odin = phase0.functions(odin_asm.read_text())
		gcc = phase0.functions(gcc_text)

	probes = {
		"o_i32_4": ("gpr", False),
		"o_i64_4": ("gpr", False),
		"o_ptr_4": ("gpr", True),
		"o_ptr_add": ("gpr", True),
		"o_f64_2": ("f64", False),
		"o_mixed_4": ("gpr", False),
		"o_s3_c": ("gpr", False),
		"o_s3_stack_c": ("gpr", False),
		"o_s5_e": ("gpr", False),
		"o_s12_c": ("gpr", False),
		"o_u8_w0": ("gpr", False),
		"o_sf_b": ("f32", False),
		"o_big_f": ("gpr", False),
		"o_big_after": ("gpr32", False),
		"o_ret_s3": ("sret", False),
		"o_ret_s12": ("sret", False),
		"o_ret_big": ("sret", False),
		"o_ret_u8": ("sret", False),
		"o_ret_sf": ("sret", False),
	}
	failures = []
	for name, (result_kind, pointer) in probes.items():
		try:
			matches = True
			for seed in range(16):
				is_sret = result_kind == "sret"
				gcc_machine = phase0.run_leaf(gcc[name], seed, sret=is_sret, pointer=pointer)
				odin_machine = phase0.run_leaf(odin[name], seed, sret=is_sret, pointer=pointer)
				if is_sret:
					gcc_result = (gcc_machine.gpr[2], bytes(gcc_machine.mem[0x200000+i] for i in range(32)))
					odin_result = (odin_machine.gpr[2], bytes(odin_machine.mem[0x200000+i] for i in range(32)))
				elif result_kind == "f64":
					gcc_result = gcc_machine.fpr[0]
					odin_result = odin_machine.fpr[0]
				elif result_kind == "f32":
					gcc_result = gcc_machine.fpr[0] & 0xffffffff
					odin_result = odin_machine.fpr[0] & 0xffffffff
				elif result_kind == "gpr32":
					# The incoming signed i32 is valid only when its GPR is
					# sign-extended. Compare its payload here; scalar return
					# extension is covered independently above.
					gcc_result = gcc_machine.gpr[2] & 0xffffffff
					odin_result = odin_machine.gpr[2] & 0xffffffff
				else:
					gcc_result = gcc_machine.gpr[2]
					odin_result = odin_machine.gpr[2]
				if gcc_result != odin_result:
					matches = False
					break
		except (KeyError, ValueError) as error:
			matches = False
			failures.append(f"{name}: {error}")
		else:
			if not matches:
				failures.append(f"{name}: ABI-visible result differs")
		print(f"  {'ok  ' if matches else 'DIFF'} {name}")

	print(f"\n{len(probes) - len(failures)}/{len(probes)} Odin/GCC differentials match")
	if failures:
		for failure in failures:
			print(f"  {failure}")
			return 1

	call_stack_bytes = {
		"o_call_named": 0,
		"o_call_var": 8,
		"o_call_var_s3": 0,
		"o_call_var_big": 8,
		"o_call_var_u8": 0,
		"o_call_i32_cast": 0,
		"o_call_u32_cast": 0,
	}

	def call_view(name, state):
		gprs, fprs, stack = state
		if name == "o_call_var_s3":
			# The three aggregate bytes are left-justified in a1. Its lower
			# five bytes and unused a3 are unspecified slot padding.
			return (gprs[0], gprs[1] >> 40, gprs[2]), fprs, stack
		if name == "o_call_var_big":
			# The trailing i32 occupies the low four bytes of its stack slot.
			return gprs, fprs, stack[4:]
		return state

	for name, stack_bytes in call_stack_bytes.items():
		try:
			matches = all(
				call_view(name, phase0.run_to_call(gcc[name], seed, stack_bytes)) ==
				call_view(name, phase0.run_to_call(odin[name], seed, stack_bytes))
				for seed in range(16)
			)
		except (KeyError, ValueError) as error:
			matches = False
			failures.append(f"{name}: {error}")
		else:
			if not matches:
				failures.append(f"{name}: call-state differs")
		print(f"  {'ok  ' if matches else 'DIFF'} {name}")

	if failures:
		print("\nCall-state failures:")
		for failure in failures:
			print(f"  {failure}")
		return 1
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
