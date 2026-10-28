// rexcode  ·  Brendan Punsky (dotbmp@github), original author

package rexcode_rsp_tests

// Printer spot checks: the text must assemble with GNU as + libdragon's
// `rsp.inc` to the same word `encode` produces.
//
// Each case pairs the printed text with the word libdragon's toolchain
// assembles it to,
//   mips64-elf-gcc -march=vr4300 -mabi=32 -x assembler-with-cpp \
//       -I$N64_INST/mips64-elf/include -c printed.S
// (`#include <rsp.inc>` at the top), and checks `encode` produces that word.
// The test itself does not need the toolchain.

import "core:fmt"
import "core:os"
import rsp "../"

@(private="file") ppasses   := 0
@(private="file") pfailures := 0

@(private="file")
encode_one :: proc(inst: rsp.Instruction) -> (word: u32, ok: bool) {
	code: [4]u8
	relocs: [dynamic]rsp.Relocation
	errors: [dynamic]rsp.Error
	defer delete(relocs)
	defer delete(errors)
	insts := []rsp.Instruction{inst}
	if _, ok = rsp.encode(insts, nil, code[:], &relocs, &errors); !ok {
		return
	}
	return u32(code[0]) << 24 | u32(code[1]) << 16 | u32(code[2]) << 8 | u32(code[3]), true
}

// `want_word` is what GNU as + rsp.inc assembles `want` to.
@(private="file")
check_print :: proc(inst: rsp.Instruction, want: string, want_word: u32) {
	insts := []rsp.Instruction{inst}
	got := rsp.tprint(insts, nil, nil)
	want_line := fmt.tprintf("    %s\n", want)
	word, encoded := encode_one(inst)
	switch {
	case got != want_line:
		fmt.printfln("  [FAIL] got %q want %q", got, want_line)
		pfailures += 1
	case !encoded || word != want_word:
		fmt.printfln("  [FAIL] %s: encode gave %08x (ok=%v), as gives %08x", want, word, encoded, want_word)
		pfailures += 1
	case:
		fmt.printfln("  [ok]   %-40s %08x", want, word)
		ppasses += 1
	}
}

// Text-only check, for a case `encode` doesn't produce yet.
@(private="file")
check_text :: proc(inst: rsp.Instruction, want: string) {
	insts := []rsp.Instruction{inst}
	got := rsp.tprint(insts, nil, nil)
	want_line := fmt.tprintf("    %s\n", want)
	if got == want_line {
		fmt.printfln("  [ok]   %s", want)
		ppasses += 1
	} else {
		fmt.printfln("  [FAIL] got %q want %q", got, want_line)
		pfailures += 1
	}
}

@(private="file")
check_text_eq :: proc(name, got, want: string) {
	if got == want {
		fmt.printfln("  [ok]   %s", name)
		ppasses += 1
	} else {
		fmt.printfln("  [FAIL] %s: got %q want %q", name, got, want)
		pfailures += 1
	}
}

run_rsp_printer_tests :: proc() {
	fmt.println()
	fmt.println("=== N64 RSP printer (libdragon rsp.inc syntax) ===")

	// Scalar: GPRs are rsp.inc's bare ABI names; $1 stays `$at` because
	// regdef.h has no `at` define.
	check_print(rsp.inst_r_r_r(.ADD,   rsp.T0, rsp.T1, rsp.T2),      "add t0, t1, t2",     0x012A4020)
	check_print(rsp.inst_r_r_i(.ADDIU, rsp.AT, rsp.AT, -1),          "addiu $at, $at, -1", 0x2421FFFF)
	check_print(rsp.inst_r_m  (.LW,    rsp.RA, rsp.mem(rsp.SP, 16)), "lw ra, 16(sp)",      0x8FBF0010)
	check_print(rsp.inst_r_m  (.SW,    rsp.FP, rsp.mem(rsp.GP, -8)), "sw fp, -8(gp)",      0xAF9EFFF8)
	check_print(rsp.inst_r_i  (.LUI,   rsp.T0, 0x1234),              "lui t0, 4660",       0x3C081234)
	check_print(rsp.inst_r    (.JR,    rsp.RA),                      "jr ra",              0x03E00008)
	// BREAK's 20-bit code spans bits 25-6; a lone `break N` is bits 25-16.
	check_print(rsp.Instruction{mnemonic = .BREAK, operand_count = 1, length = 4,
		ops = {rsp.op_imm(1 << 10, 4), {}, {}, {}}}, "break 1",    0x0001000D)
	check_print(rsp.Instruction{mnemonic = .BREAK, operand_count = 1, length = 4,
		ops = {rsp.op_imm(0x403, 4), {}, {}, {}}},   "break 1, 3", 0x000100CD)

	// COP0 / COP2 moves.
	check_print(rsp.Instruction{mnemonic = .MTC0, operand_count = 2, length = 4,
		ops = {rsp.op_reg(rsp.T0), rsp.op_reg(rsp.Register(rsp.REG_CP0 | 4)), {}, {}}},
		"mtc0 t0, COP0_SP_STATUS", 0x40882000)
	check_print(rsp.Instruction{mnemonic = .CFC2, operand_count = 2, length = 4,
		ops = {rsp.op_reg(rsp.T0), rsp.op_reg(rsp.VCC), {}, {}}},
		"cfc2 t0, COP2_CTRL_VCC", 0x48480800)
	check_print(rsp.Instruction{mnemonic = .MFC2, operand_count = 2, length = 4,
		ops = {rsp.op_reg(rsp.AT), rsp.op_vr(rsp.VR31), {}, {}}},
		"mfc2 at, $v31", 0x4801F800)
	// mtc2/mfc2's element is the byte offset (bits 10-7); rsp.inc looks the
	// GPR up as `hex.<name>`, so $1 has to be spelled `at` there.
	check_print(rsp.Instruction{mnemonic = .MTC2, operand_count = 2, length = 4,
		ops = {rsp.op_reg(rsp.T0), rsp.op_vr(rsp.VR1, 4), {}, {}}},
		"mtc2 t0, $v01, 4", 0x48880A00)

	// Vector ALU: the element is spelled with rsp.inc's accessor names;
	// field 1 has none, so it uses the 4-operand form.
	check_print(rsp.inst_v_v_v(.VMULF, rsp.VR1, rsp.VR2, rsp.VR3),     "vmulf $v01, $v02, $v03",    0x4A031040)
	check_print(rsp.inst_v_v_v(.VMULF, rsp.VR1, rsp.VR2, rsp.VR3, 3),  "vmulf $v01, $v02, $v03.q1", 0x4A631040)
	check_print(rsp.inst_v_v_v(.VMUDH, rsp.VR4, rsp.VR5, rsp.VR6, 4),  "vmudh $v04, $v05, $v06.h0", 0x4A862907)
	check_print(rsp.inst_v_v_v(.VMACF, rsp.VR7, rsp.VR8, rsp.VR9, 15), "vmacf $v07, $v08, $v09.e7", 0x4BE941C8)
	check_print(rsp.inst_v_v_v(.VADD,  rsp.VR1, rsp.VR2, rsp.VR3, 1),  "vadd $v01, $v02, $v03, 1",  0x4A231050)
	// Only vt has an element field, so one on vd is not printed; encode
	// rejects it.
	check_text(rsp.Instruction{mnemonic = .VADD, operand_count = 3, length = 4,
		ops = {rsp.op_vr(rsp.VR1, 3), rsp.op_vr(rsp.VR2), rsp.op_vr(rsp.VR3), {}}},
		"vadd $v01, $v02, $v03")

	// Single-lane ops: `op vd, de, vt, element` (vs holds the destination lane).
	check_print(rsp.inst_v_v_v(.VRCP, rsp.VR1, rsp.VR3, rsp.VR2, 13), "vrcp $v01, 3, $v02, 13", 0x4BA21870)
	check_print(rsp.Instruction{mnemonic = .VMOV, operand_count = 2, length = 4,
		ops = {rsp.op_vr(rsp.VR1), rsp.op_vr(rsp.VR2, 9), {}, {}}},
		"vmov $v01, 0, $v02, 9", 0x4B220073)
	// vsar: rsp.inc only takes `vsar vd, element` (vs = vt = 0, element 8-10);
	// anything else goes through its generic `vectorOp`.
	check_print(rsp.inst_v_v_v(.VSAR, rsp.VR1, rsp.VR0, rsp.VR0, 8), "vsar $v01, 8", 0x4B00005D)
	check_print(rsp.inst_vsar_acc(rsp.VR1, .HI), "vsar $v01, 8", 0x4B00005D)
	check_print(rsp.inst_vsar_acc(rsp.VR31, .LO), "vsar $v31, 10", 0x4B4007DD)
	check_print(rsp.inst_v_v_v(.VSAR, rsp.VR1, rsp.VR2, rsp.VR3, 9), "vectorOp 0b011101, $v01, $v02, $v03, 9", 0x4B23105D)
	check_print(rsp.inst_none(.VNOP), "vnop", 0x4A000037)

	// Vector load/store: `op vt, element, byte_offset, base`. The element is
	// always printed; the offset field is scaled by the access size.
	check_print(rsp.inst_v_vmem(.LQV, rsp.VR4, rsp.vmem(rsp.T0, 0, 1)),    "lqv $v04, 0, 16, t0",    0xC9042001)
	check_print(rsp.inst_v_vmem(.LSV, rsp.VR1, rsp.vmem(rsp.T0, 4, 1)),    "lsv $v01, 4, 2, t0",     0xC9010A01)
	check_print(rsp.inst_v_vmem(.LBV, rsp.VR2, rsp.vmem(rsp.S0, 15, -64)), "lbv $v02, 15, -64, s0",  0xCA0207C0)
	check_print(rsp.inst_v_vmem(.SDV, rsp.VR3, rsp.vmem(rsp.SP, 8, -64)),  "sdv $v03, 8, -512, sp",  0xEBA31C40)
	check_print(rsp.inst_v_vmem(.LLV, rsp.VR3, rsp.vmem(rsp.A1, 12, 2)),   "llv $v03, 12, 8, a1",    0xC8A31602)
	check_print(rsp.inst_v_vmem(.LTV, rsp.VR5, rsp.vmem(rsp.A0, 2, 63)),   "ltv $v05, 2, 1008, a0",  0xC885593F)
	check_print(rsp.inst_v_vmem(.SWV, rsp.VR6, rsp.vmem(rsp.T1, 0, -1)),   "swv $v06, 0, -16, t1",   0xE926507F)
	// rsp.inc has no `lwv`; its `loadVector` takes the raw 7-bit offset field.
	check_print(rsp.inst_v_vmem(.LWV, rsp.VR6, rsp.vmem(rsp.T1, 3, -1)),   "loadVector 0b01010, $v06, 3, 127, t1", 0xC92651FF)

	// Print options: `uppercase` touches mnemonics only (rsp.inc's names are
	// case-sensitive); `space_after_comma = false` drops the spaces.
	{
		opts := rsp.DEFAULT_PRINT_OPTIONS
		opts.uppercase = true
		insts := []rsp.Instruction{rsp.inst_v_v_v(.VMULF, rsp.VR1, rsp.VR2, rsp.VR3, 3)}
		check_text_eq("uppercase mnemonic only", rsp.tprint(insts, nil, nil, options = &opts),
			"    VMULF $v01, $v02, $v03.q1\n")

		opts = rsp.DEFAULT_PRINT_OPTIONS
		opts.space_after_comma = false
		insts = []rsp.Instruction{rsp.inst_v_vmem(.LQV, rsp.VR4, rsp.vmem(rsp.T0, 0, 1))}
		check_text_eq("no space after comma", rsp.tprint(insts, nil, nil, options = &opts),
			"    lqv $v04,0,16,t0\n")
	}

	// Decoded words print back to text that assembles to the same words.
	{
		words := []u32{
			0x4A4C5A91,   // vsub $v10, $v11, $v12.q0
			0x4AEF7368,   // vand $v13, $v14, $v15.h3
			0x4B128C2A,   // vor $v16, $v17, $v18.e0
			0x4B23105D,   // vectorOp 0b011101, $v01, $v02, $v03, 9
			0xC92651FF,   // loadVector 0b01010, $v06, 3, 127, t1
			0x48C01000,   // ctc2 zero, COP2_CTRL_VCE
			0x40048800,   // mfc0 a0, $17
			0xC9010A01,   // lsv $v01, 4, 2, t0
			0xC885593F,   // ltv $v05, 2, 1008, a0
		}
		code := make([]u8, len(words) * 4, context.temp_allocator)
		for w, i in words {
			code[i*4+0] = u8(w >> 24); code[i*4+1] = u8(w >> 16)
			code[i*4+2] = u8(w >>  8); code[i*4+3] = u8(w)
		}
		errors:   [dynamic]rsp.Error
		d_insts:  [dynamic]rsp.Instruction
		d_info:   [dynamic]rsp.Instruction_Info
		d_labels: [dynamic]rsp.Label_Definition
		defer delete(errors)
		defer delete(d_insts)
		defer delete(d_info)
		defer delete(d_labels)
		rsp.decode(code, nil, &d_insts, &d_info, &d_labels, &errors)
		check_text_eq("decoded words", rsp.tprint(d_insts[:], d_info[:], d_labels[:]),
			"    vsub $v10, $v11, $v12.q0\n" +
			"    vand $v13, $v14, $v15.h3\n" +
			"    vor $v16, $v17, $v18.e0\n" +
			"    vectorOp 0b011101, $v01, $v02, $v03, 9\n" +
			"    loadVector 0b01010, $v06, 3, 127, t1\n" +
			"    ctc2 zero, COP2_CTRL_VCE\n" +
			"    mfc0 a0, $17\n" +
			"    lsv $v01, 4, 2, t0\n" +
			"    ltv $v05, 2, 1008, a0\n")
	}

	fmt.println()
	fmt.printfln("==> printer: %d passed, %d failed", ppasses, pfailures)
	if pfailures > 0 { os.exit(1) }
}
