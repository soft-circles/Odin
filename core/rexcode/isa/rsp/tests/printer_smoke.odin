// rexcode  ·  Brendan Punsky (dotbmp@github), original author

package rexcode_rsp_tests

// Printer spot checks: the text must assemble with GNU as + libdragon's
// `rsp.inc` to the same word `encode` produces.
//
// Every expected line below was assembled with libdragon's toolchain
//   mips64-elf-gcc -march=vr4300 -mabi=32 -x assembler-with-cpp \
//       -I$N64_INST/mips64-elf/include -c printed.S
// (`#include <rsp.inc>` at the top) and its word compared with `rsp.encode`.
// The test itself does not need the toolchain.

import "core:fmt"
import "core:os"
import rsp "../"

@(private="file") ppasses   := 0
@(private="file") pfailures := 0

@(private="file")
check_print :: proc(inst: rsp.Instruction, want: string) {
	insts := []rsp.Instruction{inst}
	got := rsp.tprint(insts, nil, nil, nil, nil, nil)
	want_line := fmt.tprintf("    %s\n", want)
	if got == want_line {
		fmt.printfln("  [ok]   %s", want)
		ppasses += 1
	} else {
		fmt.printfln("  [FAIL] got %q want %q", got, want_line)
		pfailures += 1
	}
}

run_rsp_printer_tests :: proc() {
	fmt.println()
	fmt.println("=== N64 RSP printer (libdragon rsp.inc syntax) ===")

	// Scalar: GPRs are rsp.inc's bare ABI names; $1 stays `$at` because
	// regdef.h has no `at` define.
	check_print(rsp.inst_r_r_r(.ADD,   rsp.T0, rsp.T1, rsp.T2),  "add t0, t1, t2")
	check_print(rsp.inst_r_r_i(.ADDIU, rsp.AT, rsp.AT, -1),      "addiu $at, $at, -1")
	check_print(rsp.inst_r_m  (.LW,    rsp.RA, rsp.mem(rsp.SP, 16)), "lw ra, 16(sp)")
	check_print(rsp.inst_r_m  (.SW,    rsp.FP, rsp.mem(rsp.GP, -8)), "sw fp, -8(gp)")
	check_print(rsp.inst_r_i  (.LUI,   rsp.T0, 0x1234),          "lui t0, 4660")
	check_print(rsp.inst_r    (.JR,    rsp.RA),                  "jr ra")
	// BREAK's 20-bit code spans bits 25-6; a lone `break N` is bits 25-16.
	check_print(rsp.Instruction{mnemonic = .BREAK, operand_count = 1, length = 4,
		ops = {rsp.op_imm(1 << 10, 4), {}, {}, {}}}, "break 1")
	check_print(rsp.Instruction{mnemonic = .BREAK, operand_count = 1, length = 4,
		ops = {rsp.op_imm(0x403, 4), {}, {}, {}}}, "break 1, 3")

	// COP0 / COP2 moves.
	check_print(rsp.Instruction{mnemonic = .MTC0, operand_count = 2, length = 4,
		ops = {rsp.op_reg(rsp.T0), rsp.op_reg(rsp.Register(rsp.REG_CP0 | 4)), {}, {}}},
		"mtc0 t0, COP0_SP_STATUS")
	check_print(rsp.Instruction{mnemonic = .CFC2, operand_count = 2, length = 4,
		ops = {rsp.op_reg(rsp.T0), rsp.op_reg(rsp.VCC), {}, {}}},
		"cfc2 t0, $1")
	// mtc2/mfc2's element is the byte offset (bits 10-7). rsp.inc looks the
	// GPR up as `hex.<name>`, so $1 has to be spelled `at` here.
	check_print(rsp.Instruction{mnemonic = .MTC2, operand_count = 2, length = 4,
		ops = {rsp.op_reg(rsp.T0), rsp.op_vr(rsp.VR1, 4), {}, {}}},
		"mtc2 t0, $v01, 4")
	check_print(rsp.Instruction{mnemonic = .MFC2, operand_count = 2, length = 4,
		ops = {rsp.op_reg(rsp.AT), rsp.op_vr(rsp.VR31), {}, {}}},
		"mfc2 at, $v31")

	// Vector ALU: the element is spelled with rsp.inc's accessor names;
	// field 1 has none, so it uses the 4-operand form.
	check_print(rsp.inst_v_v_v(.VMULF, rsp.VR1, rsp.VR2, rsp.VR3),      "vmulf $v01, $v02, $v03")
	check_print(rsp.inst_v_v_v(.VMULF, rsp.VR1, rsp.VR2, rsp.VR3, 3),   "vmulf $v01, $v02, $v03.q1")
	check_print(rsp.inst_v_v_v(.VMUDH, rsp.VR4, rsp.VR5, rsp.VR6, 4),   "vmudh $v04, $v05, $v06.h0")
	check_print(rsp.inst_v_v_v(.VMACF, rsp.VR7, rsp.VR8, rsp.VR9, 15),  "vmacf $v07, $v08, $v09.e7")
	check_print(rsp.inst_v_v_v(.VADD,  rsp.VR1, rsp.VR2, rsp.VR3, 1),   "vadd $v01, $v02, $v03, 1")

	// Single-lane ops: `op vd, de, vt, element` (vs holds the destination lane).
	check_print(rsp.inst_v_v_v(.VRCP,  rsp.VR1, rsp.VR3, rsp.VR2, 13),  "vrcp $v01, 3, $v02, 13")
	check_print(rsp.Instruction{mnemonic = .VMOV, operand_count = 2, length = 4,
		ops = {rsp.op_vr(rsp.VR1), rsp.op_vr(rsp.VR2, 9), {}, {}}},
		"vmov $v01, 0, $v02, 9")
	// vsar: rsp.inc only takes `vsar vd, element` (vs = vt = 0, element 8-10);
	// anything else goes through its generic `vectorOp`.
	check_print(rsp.inst_v_v_v(.VSAR,  rsp.VR1, rsp.VR0, rsp.VR0, 8),   "vsar $v01, 8")
	check_print(rsp.inst_v_v_v(.VSAR,  rsp.VR1, rsp.VR2, rsp.VR3, 9),   "vectorOp 0b011101, $v01, $v02, $v03, 9")
	check_print(rsp.inst_none(.VNOP), "vnop")

	// Vector load/store: `op vt, element, byte_offset, base`. The element is
	// always printed; the offset field is scaled by the access size.
	check_print(rsp.inst_v_vmem(.LQV, rsp.VR4, rsp.vmem(rsp.T0, 0, 1)),     "lqv $v04, 0, 16, t0")
	check_print(rsp.inst_v_vmem(.LSV, rsp.VR1, rsp.vmem(rsp.T0, 4, 1)),     "lsv $v01, 4, 2, t0")
	check_print(rsp.inst_v_vmem(.LBV, rsp.VR2, rsp.vmem(rsp.S0, 15, -64)),  "lbv $v02, 15, -64, s0")
	check_print(rsp.inst_v_vmem(.SDV, rsp.VR3, rsp.vmem(rsp.SP, 8, -64)),   "sdv $v03, 8, -512, sp")
	check_print(rsp.inst_v_vmem(.LLV, rsp.VR3, rsp.vmem(rsp.A1, 12, 2)),    "llv $v03, 12, 8, a1")
	check_print(rsp.inst_v_vmem(.LTV, rsp.VR5, rsp.vmem(rsp.A0, 2, 63)),    "ltv $v05, 2, 1008, a0")
	check_print(rsp.inst_v_vmem(.SWV, rsp.VR6, rsp.vmem(rsp.T1, 0, -1)),    "swv $v06, 0, -16, t1")
	// rsp.inc has no `lwv`; its `loadVector` takes the raw 7-bit offset field.
	check_print(rsp.inst_v_vmem(.LWV, rsp.VR6, rsp.vmem(rsp.T1, 3, -1)),    "loadVector 0b01010, $v06, 3, 127, t1")

	// Decoded words keep their element and byte offset through the printer.
	{
		code: [8]u8
		relocs: [dynamic]rsp.Relocation
		errors: [dynamic]rsp.Error
		defer delete(relocs)
		defer delete(errors)
		insts := []rsp.Instruction{
			{mnemonic = .LSV, operand_count = 2, length = 4,   // 0xC9010A01
				ops = {rsp.op_vr(rsp.VR1), rsp.op_vmem(rsp.vmem(rsp.T0, 4, 1), 2), {}, {}}},
			{mnemonic = .SQV, operand_count = 2, length = 4,   // 0xEA22207E
				ops = {rsp.op_vr(rsp.VR2), rsp.op_vmem(rsp.vmem(rsp.S1, 0, -2), 16), {}, {}}},
		}
		byte_count, encoded := rsp.encode(insts, nil, code[:], &relocs, &errors)
		if !encoded {
			fmt.printfln("  [FAIL] decoded vector load/store: encode failed %v", errors[:])
			pfailures += 1
		}

		d_insts:  [dynamic]rsp.Instruction
		d_info:   [dynamic]rsp.Instruction_Info
		d_labels: [dynamic]rsp.Label_Definition
		defer delete(d_insts)
		defer delete(d_info)
		defer delete(d_labels)
		rsp.decode(code[:byte_count], nil, &d_insts, &d_info, &d_labels, &errors)
		got  := rsp.tprint(d_insts[:], d_info[:], d_labels[:])
		want := "    lsv $v01, 4, 2, t0\n    sqv $v02, 0, -32, s1\n"
		if got == want {
			fmt.println("  [ok]   decoded vector load/store")
			ppasses += 1
		} else {
			fmt.printfln("  [FAIL] decoded vector load/store: got %q want %q", got, want)
			pfailures += 1
		}
	}

	fmt.println()
	fmt.printfln("==> printer: %d passed, %d failed", ppasses, pfailures)
	if pfailures > 0 { os.exit(1) }
}
