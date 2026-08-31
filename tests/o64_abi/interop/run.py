#!/usr/bin/env python3
"""Execute linked GCC/Odin call pairs from identical O64 machine states."""

import importlib.util
import os
import re
import struct
import subprocess
import sys
from pathlib import Path


HERE = Path(__file__).resolve().parent
ODIN_ROOT = HERE.parents[2]
WORKSPACE = ODIN_ROOT.parent
PHASE0 = WORKSPACE / "llvm-project/llvm/utils/o64-abi-differential.py"
N64_INST = Path(os.environ.get("N64_INST", Path.home() / "n64_toolchain"))
OBJDUMP = Path(os.environ.get("MIPS_O64_OBJDUMP", N64_INST / "bin/mips64-elf-objdump"))
STOP = 0xDEAD0000


def load_phase0():
	spec = importlib.util.spec_from_file_location("o64_phase0", PHASE0)
	module = importlib.util.module_from_spec(spec)
	spec.loader.exec_module(module)
	return module


def disassemble(elf):
	result = subprocess.run(
		[OBJDUMP, "-d", elf], capture_output=True, text=True, check=True
	)
	symbols = {}
	code = {}
	for line in result.stdout.splitlines():
		match = re.fullmatch(r"([0-9a-f]+) <([^>]+)>:", line.strip())
		if match:
			symbols[match.group(2)] = int(match.group(1), 16)
			continue
		match = re.match(r"\s*([0-9a-f]+):\s+[0-9a-f]{8}\s+(.+?)\s*$", line)
		if match:
			code[int(match.group(1), 16)] = match.group(2)
	return symbols, code


def control_target(instruction):
	return int(instruction.split()[1].split(",")[0], 16)


def machine_instruction(instruction):
	return re.sub(r"\(([a-z][a-z0-9]*)\)", r"($\1)", instruction)


def execute_machine(machine, instruction):
	normalized = machine_instruction(instruction)
	fields = normalized.split(None, 1)
	if fields[0] in {"sdl", "sdr"} and fields[1].split(",", 1)[0] == "zero":
		# Odin clears its fully-overwritten sret temporary with unaligned stores.
		return
	machine.execute(normalized)


def execute_delay(machine, code, pc):
	instruction = code.get(pc)
	if instruction is None:
		raise ValueError(f"missing delay-slot instruction at {pc:08x}")
	if instruction.split(None, 1)[0] in {"j", "jal", "jr", "b", "bal"}:
		raise ValueError(f"control instruction in delay slot: {instruction}")
	execute_machine(machine, instruction)


def execute(phase0, symbols, code, name, machine):
	pc = symbols[name]
	machine.gpr[31] = STOP
	for _ in range(1000):
		instruction = code.get(pc)
		if instruction is None:
			raise ValueError(f"{name}: no instruction at {pc:08x}")
		op = instruction.split(None, 1)[0]
		if op in {"jal", "bal"}:
			machine.gpr[31] = (pc + 8) & phase0.MASK64
			execute_delay(machine, code, pc + 4)
			pc = control_target(instruction)
			continue
		if op in {"j", "b"}:
			execute_delay(machine, code, pc + 4)
			pc = control_target(instruction)
			continue
		if op == "jr":
			target = machine.r(instruction.split(None, 1)[1]) & 0xFFFFFFFF
			execute_delay(machine, code, pc + 4)
			if target == STOP:
				return machine
			pc = target
			continue
		execute_machine(machine, instruction)
		pc += 4
	raise ValueError(f"{name}: instruction limit exceeded")


VALUES32 = [0x81234567, 0x7EDCBA98, 0xFFFFFFFF, 0x80000005]
VALUES64 = [
	0x8123456789ABCDEF,
	0x7EDCBA9876543210,
	0xFFFFFFFF00000001,
	0x8000000000000005,
]


def bits32(value):
	return int.from_bytes(struct.pack(">f", value), "big")


def bits64(value):
	return int.from_bytes(struct.pack(">d", value), "big")


def initial_machine(phase0, kind, seed):
	machine = phase0.Machine(seed)
	v32 = VALUES32[seed]
	v64 = VALUES64[seed]
	signed32 = phase0.sx(v32, 32) & phase0.MASK64

	if kind in {"i32", "u32"}:
		machine.gpr[4] = signed32
	elif kind == "var_i32":
		machine.gpr[4] = phase0.sx(0x13579BDF ^ seed, 32) & phase0.MASK64
		machine.gpr[5] = signed32
	elif kind == "i64":
		machine.gpr[4] = v64
	elif kind == "ptr_add":
		machine.gpr[4] = phase0.sx(0x80102030 + seed * 0x100, 32) & phase0.MASK64
		machine.gpr[5] = 0x1234 + seed
	elif kind == "f32":
		machine.fpr[12] = bits32((-13.25, 0.125, 65504.0, -0.0)[seed])
	elif kind == "f64":
		machine.fpr[12] = bits64((9876.5, -0.03125, 1.0e100, -0.0)[seed])
	elif kind == "s3":
		machine.gpr[4] = (0x81 << 56) | (0xA5 << 48) | ((v32 & 0xFF) << 40) | (v64 & ((1 << 40) - 1))
	elif kind == "s12":
		machine.gpr[4] = (0x11223344 << 32) | 0x89ABCDEF
		machine.gpr[5] = (v32 << 32) | (v64 & 0xFFFFFFFF)
	elif kind == "u8":
		machine.gpr[4] = v64
	elif kind == "stack":
		machine.gpr[4:8] = [1, 2, 3, 4]
		machine.store(machine.gpr[29] + 32, 4, 0xA5A5A5A5)
		machine.store(machine.gpr[29] + 36, 4, v32)
	elif kind == "ret_big":
		fields = [0x11223344, 0x89ABCDEF, 0xFEDCBA98, 0x80000004, 0x7FFFFFF5, v32]
		machine.gpr[4:8] = [phase0.sx(value, 32) & phase0.MASK64 for value in fields[:4]]
		machine.store(machine.gpr[29] + 32, 4, 0xA5A5A5A5)
		machine.store(machine.gpr[29] + 36, 4, fields[4])
		machine.store(machine.gpr[29] + 40, 4, 0x5A5A5A5A)
		machine.store(machine.gpr[29] + 44, 4, fields[5])
	else:
		raise AssertionError(kind)
	return machine


def output(machine, kind):
	if kind == "f32":
		return machine.fpr[0] & 0xFFFFFFFF
	if kind == "f64":
		return machine.fpr[0]
	return machine.gpr[2]


PROBES = [
	("C -> Odin i32", "i32", "c_to_odin_i32", "c_to_gcc_i32"),
	("C -> Odin u32", "u32", "c_to_odin_u32", "c_to_gcc_u32"),
	("C -> Odin i64", "i64", "c_to_odin_i64", "c_to_gcc_i64"),
	("C -> Odin pointer", "ptr_add", "c_to_odin_ptr_add", "c_to_gcc_ptr_add"),
	("C -> Odin f32", "f32", "c_to_odin_f32", "c_to_gcc_f32"),
	("C -> Odin f64", "f64", "c_to_odin_f64", "c_to_gcc_f64"),
	("C -> Odin small struct", "s3", "c_to_odin_s3", "c_to_gcc_s3"),
	("C -> Odin multi-slot struct", "s12", "c_to_odin_s12", "c_to_gcc_s12"),
	("C -> Odin raw union", "u8", "c_to_odin_u8", "c_to_gcc_u8"),
	("C -> Odin stack arg", "stack", "c_to_odin_stack", "c_to_gcc_stack"),
	("C -> Odin indirect return", "ret_big", "c_to_odin_ret_big", "c_to_gcc_ret_big"),
	("Odin -> C i32", "i32", "odin_to_c_i32", "gcc_to_c_i32"),
	("Odin -> C u32", "u32", "odin_to_c_u32", "gcc_to_c_u32"),
	("Odin -> C i64", "i64", "odin_to_c_i64", "gcc_to_c_i64"),
	("Odin -> C pointer", "ptr_add", "odin_to_c_ptr_add", "gcc_to_c_ptr_add"),
	("Odin -> C f32", "f32", "odin_to_c_f32", "gcc_to_c_f32"),
	("Odin -> C f64", "f64", "odin_to_c_f64", "gcc_to_c_f64"),
	("Odin -> C small struct", "s3", "odin_to_c_s3", "gcc_to_c_s3"),
	("Odin -> C multi-slot struct", "s12", "odin_to_c_s12", "gcc_to_c_s12"),
	("Odin -> C raw union", "u8", "odin_to_c_u8", "gcc_to_c_u8"),
	("Odin -> C stack arg", "stack", "odin_to_c_stack", "gcc_to_c_stack"),
	("Odin -> C indirect return", "ret_big", "odin_to_c_ret_big", "gcc_to_c_ret_big"),
	("Odin -> variadic C", "var_i32", "odin_to_c_var_i32", "gcc_to_c_var_i32"),
]


def main():
	elf = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE / "build/interop.elf"
	phase0 = load_phase0()
	symbols, code = disassemble(elf)
	failures = []
	for label, kind, mixed, reference in PROBES:
		try:
			matches = True
			for seed in range(len(VALUES32)):
				mixed_result = output(
					execute(phase0, symbols, code, mixed, initial_machine(phase0, kind, seed)), kind
				)
				reference_result = output(
					execute(phase0, symbols, code, reference, initial_machine(phase0, kind, seed)), kind
				)
				if mixed_result != reference_result:
					matches = False
					failures.append(
						f"{label} seed {seed}: mixed={mixed_result:#x}, gcc={reference_result:#x}"
					)
					break
		except (KeyError, ValueError) as error:
			matches = False
			failures.append(f"{label}: {error}")
		print(f"  {'ok  ' if matches else 'DIFF'} {label}")

	print(f"\n{len(PROBES) - len(failures)}/{len(PROBES)} linked Odin/GCC interop probes match")
	if failures:
		for failure in failures:
			print(f"  {failure}")
		return 1
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
