// Every form of the @(rsp) table (src/asm_tables_rsp.cpp), encoded twice: by the
// compiler through intrinsics.rsp_template_code, and by core:rexcode/isa/rsp from the
// same instructions written as rexcode values. The words must agree. Run on the host:
//
//     odin run tests/rsp_templates/golden
//
// Prints each template's words as `<template> <index> <word> <line>` for
// test_rsp_templates.py, then `ok` or the mismatches, and exits non-zero on one.
package rsp_templates_golden

import "base:intrinsics"
import "base:runtime"
import "core:fmt"
import "core:os"
import rsp "core:rexcode/isa/rsp"

import valid "../valid"
import "../valid/transform"

Args :: struct {
	a, b, c, d: i16,
	tx:         i32,
	pad:        [4]i32,
}

@(rsp)
scalar_forms :: asm(args: ^Args, input: ^u32, count: i32) [args = %s6, input = %s1, count = %s3] {
.top:
	nop
	or    %t0, %t1, %t2
	or    %at, %v0, %v1
	or    %k0, %k1, %fp
	or    %s8, %t9, %zero
	or    count, count, %t8
	addiu %t0, %t1, -32768
	addiu %t0, %t1, 32767
	addiu count, count, -4
	addiu input, input, 16
	andi  %t3, %t4, 0
	andi  %t3, %t4, 0xffff
	sll   %t5, %t6, 0
	sll   %t5, %t6, 31
	sra   %t7, %s0, 0
	sra   %t7, %s0, 31
	lh    %t0, [args + (offset_of(Args, a))]
	lh    %t1, [args + (offset_of(Args, c))]
	lh    %a0, [%s6 - 32768]
	lw    %a1, [args + (offset_of(Args, tx))]
	lw    %a2, [%s7 + 32767]
	lw    %a3, [input]
	sw    %t0, [input + 4]
	sw    %s4, [%s5 - 4]
	bgtz  count, .mid
	nop
	bgtz  %t0, .top
.mid:
	addiu %s2, %s2, 1
	bgtz  %t1, .end
	addiu %s2, %s2, 2
.end:
}

@(rsp)
vector_forms :: asm(input: ^[8]i16, output: ^[8]i16) [input = %s1, output = %s2] {
	vmudn %v00, %v01, %v02
	vmudn %v03, %v04, %v05.q0
	vmudn %v06, %v07, %v08.q1
	vmudn %v09, %v10, %v11.h0
	vmadn %v12, %v13, %v14.h1
	vmadn %v15, %v16, %v17.h2
	vmadn %v18, %v19, %v20.h3
	vmadh %v21, %v22, %v23.e0
	vmadh %v24, %v25, %v26.e1
	vmadh %v27, %v28, %v29.e2
	vmadh %v30, %v31, %v00.e3
	vmudn %v31, %v30, %v29.e4
	vmadn %v28, %v27, %v26.e5
	vmadh %v25, %v24, %v23.e6
	vmadh %v02, %v14, %v30.e7
	lqv   %v01, [input]
	lqv   %v01.e0, [input + 0]
	lqv   %v02.e1, [input + 16]
	lqv   %v03.e7, [input - 1024]
	lqv   %v04.b0, [input + 1008]
	lqv   %v05.b1, [%s7 + 64]
	lqv   %v06.b15, [%t0 - 16]
	sqv   %v07, [output]
	sqv   %v08.e3, [output + 32]
	sqv   %v09.b7, [output - 1024]
	sqv   %v31.e7, [%s2 + 1008]
}

// Bases and branches at their edges: %zero as a base, a displacement of several terms,
// a branch to itself and to the next instruction, and two labels at one point.
@(rsp)
edge_forms :: asm(p: ^u32) [p = %s1] {
	lw    %t0, [%zero + 8]
	sw    %t1, [%zero - 4]
	lw    %t2, [p + 4 - 8]
	lw    %t3, [p + 16 - 4 + 2 - 2]
.self:
	bgtz  %t0, .self
	bgtz  %t1, .next
.next:
	nop
.first:
.second:
	bgtz  %t2, .first
	bgtz  %t3, .second
	nop
}

SCALAR    :: intrinsics.rsp_template_code(scalar_forms)
EDGE      :: intrinsics.rsp_template_code(edge_forms)
FILL      :: intrinsics.rsp_template_code(valid.fill)
TRANSFORM :: intrinsics.rsp_template_code(transform.transform)
VECTOR :: intrinsics.rsp_template_code(vector_forms)

vr :: proc(n: int) -> rsp.Register {
	return rsp.Register(rsp.REG_VR | u16(n))
}

expected_scalar :: proc() -> (code: [dynamic]rsp.Instruction, labels: [dynamic]rsp.Label_Definition) {
	TOP :: 0; MID :: 1; END :: 2
	append(&labels, rsp.Label_Definition(0), rsp.Label_Definition(27), rsp.Label_Definition(30))
	append(&code,
		rsp.inst_nop(),
		rsp.inst_or(rsp.T0, rsp.T1, rsp.T2),
		rsp.inst_or(rsp.AT, rsp.V0, rsp.V1),
		rsp.inst_or(rsp.K0, rsp.K1, rsp.FP),
		rsp.inst_or(rsp.FP, rsp.T9, rsp.ZERO),
		rsp.inst_or(rsp.S3, rsp.S3, rsp.T8),
		rsp.inst_addiu(rsp.T0, rsp.T1, -32768),
		rsp.inst_addiu(rsp.T0, rsp.T1, 32767),
		rsp.inst_addiu(rsp.S3, rsp.S3, -4),
		rsp.inst_addiu(rsp.S1, rsp.S1, 16),
		rsp.inst_andi(rsp.T3, rsp.T4, 0),
		rsp.inst_andi(rsp.T3, rsp.T4, 0xffff),
		rsp.inst_sll(rsp.T5, rsp.T6, 0),
		rsp.inst_sll(rsp.T5, rsp.T6, 31),
		rsp.inst_sra(rsp.T7, rsp.S0, 0),
		rsp.inst_sra(rsp.T7, rsp.S0, 31),
		rsp.inst_lh(rsp.T0, rsp.mem(rsp.S6, i32(offset_of(Args, a)))),
		rsp.inst_lh(rsp.T1, rsp.mem(rsp.S6, i32(offset_of(Args, c)))),
		rsp.inst_lh(rsp.A0, rsp.mem(rsp.S6, -32768)),
		rsp.inst_lw(rsp.A1, rsp.mem(rsp.S6, i32(offset_of(Args, tx)))),
		rsp.inst_lw(rsp.A2, rsp.mem(rsp.S7, 32767)),
		rsp.inst_lw(rsp.A3, rsp.mem(rsp.S1, 0)),
		rsp.inst_sw(rsp.T0, rsp.mem(rsp.S1, 4)),
		rsp.inst_sw(rsp.S4, rsp.mem(rsp.S5, -4)),
		rsp.inst_bgtz(rsp.S3, MID),
		rsp.inst_nop(),
		rsp.inst_bgtz(rsp.T0, TOP),
		rsp.inst_addiu(rsp.S2, rsp.S2, 1),
		rsp.inst_bgtz(rsp.T1, END),
		rsp.inst_addiu(rsp.S2, rsp.S2, 2),
	)
	return
}

expected_edge :: proc() -> (code: [dynamic]rsp.Instruction, labels: [dynamic]rsp.Label_Definition) {
	SELF :: 0; NEXT :: 1; FIRST :: 2; SECOND :: 3
	append(&labels, rsp.Label_Definition(4), rsp.Label_Definition(6), rsp.Label_Definition(7), rsp.Label_Definition(7))
	append(&code,
		rsp.inst_lw(rsp.T0, rsp.mem(rsp.ZERO, 8)),
		rsp.inst_sw(rsp.T1, rsp.mem(rsp.ZERO, -4)),
		rsp.inst_lw(rsp.T2, rsp.mem(rsp.S1, -4)),
		rsp.inst_lw(rsp.T3, rsp.mem(rsp.S1, 12)),
		rsp.inst_bgtz(rsp.T0, SELF),
		rsp.inst_bgtz(rsp.T1, NEXT),
		rsp.inst_nop(),
		rsp.inst_bgtz(rsp.T2, FIRST),
		rsp.inst_bgtz(rsp.T3, SECOND),
		rsp.inst_nop(),
	)
	return
}

// valid.fill: the output pinned as the exit contract, a scratch register by name
expected_fill :: proc() -> (code: [dynamic]rsp.Instruction) {
	append(&code,
		rsp.inst_or(rsp.T2, rsp.A0, rsp.ZERO),
		rsp.inst_sw(rsp.T2, rsp.mem(rsp.S2, 0)),
		rsp.inst_sw(rsp.T2, rsp.mem(rsp.S2, 4)),
		rsp.inst_sw(rsp.T2, rsp.mem(rsp.S2, 8)),
		rsp.inst_sw(rsp.T2, rsp.mem(rsp.S2, 12)),
		rsp.inst_addiu(rsp.S3, rsp.S2, 16),
	)
	return
}

expected_vector :: proc() -> (code: [dynamic]rsp.Instruction) {
	// VU element fields in accessor order: none, q0 q1, h0-h3, e0-e7
	vu := [15]u8{0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}
	regs := [15][3]int{
		{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {9, 10, 11}, {12, 13, 14}, {15, 16, 17}, {18, 19, 20},
		{21, 22, 23}, {24, 25, 26}, {27, 28, 29}, {30, 31, 0}, {31, 30, 29}, {28, 27, 26}, {25, 24, 23}, {2, 14, 30},
	}
	ops := [15]rsp.Mnemonic{.VMUDN, .VMUDN, .VMUDN, .VMUDN, .VMADN, .VMADN, .VMADN, .VMADH, .VMADH, .VMADH, .VMADH, .VMUDN, .VMADN, .VMADH, .VMADH}
	for i in 0..<15 {
		inst := rsp.inst_vmudn(vr(regs[i][0]), vr(regs[i][1]), vr(regs[i][2]), vu[i])
		inst.mnemonic = ops[i]
		append(&code, inst)
	}
	// rexcode's vector offsets are in units of the 16-byte access; the template's are bytes
	append(&code,
		rsp.inst_lqv(vr(1), rsp.vmem(rsp.S1, 0, 0)),
		rsp.inst_lqv(vr(1), rsp.vmem(rsp.S1, 0, 0)),
		rsp.inst_lqv(vr(2), rsp.vmem(rsp.S1, 2, 1)),
		rsp.inst_lqv(vr(3), rsp.vmem(rsp.S1, 14, -64)),
		rsp.inst_lqv(vr(4), rsp.vmem(rsp.S1, 0, 63)),
		rsp.inst_lqv(vr(5), rsp.vmem(rsp.S7, 1, 4)),
		rsp.inst_lqv(vr(6), rsp.vmem(rsp.T0, 15, -1)),
		rsp.inst_sqv(vr(7), rsp.vmem(rsp.S2, 0, 0)),
		rsp.inst_sqv(vr(8), rsp.vmem(rsp.S2, 6, 2)),
		rsp.inst_sqv(vr(9), rsp.vmem(rsp.S2, 7, -64)),
		rsp.inst_sqv(vr(31), rsp.vmem(rsp.S2, 14, 63)),
	)
	return
}

// Odin64's rexcode builder body for rsp_jobs/transform (body.odin there), with its
// register convention: the template in ../valid/transform must encode to the same words.
expected_transform :: proc() -> (code: [dynamic]rsp.Instruction, labels: [dynamic]rsp.Label_Definition) {
	Args :: transform.Args
	ARGS :: rsp.S6; SCRATCH :: rsp.S7; INPUT :: rsp.S1; OUTPUT :: rsp.S2; COUNT :: rsp.S3
	VSHIFT :: rsp.VR30
	Q0 :: 2; Q1 :: 3; E0 :: 8

	splat :: proc(code: ^[dynamic]rsp.Instruction, word: rsp.Register, offset: i32) {
		for lane in i32(0)..<4 { append(code, rsp.inst_sw(word, rsp.mem(SCRATCH, offset + lane * 4))) }
	}
	coefficients :: proc(code: ^[dynamic]rsp.Instruction, offset: i32) {
		append(code, rsp.inst_sra(rsp.T4, rsp.T0, 8))
		append(code, rsp.inst_sll(rsp.T4, rsp.T4, 16))
		append(code, rsp.inst_sra(rsp.T5, rsp.T1, 8))
		append(code, rsp.inst_andi(rsp.T5, rsp.T5, 0xffff))
		append(code, rsp.inst_or(rsp.T4, rsp.T4, rsp.T5))
		splat(code, rsp.T4, offset)
		append(code, rsp.inst_andi(rsp.T4, rsp.T0, 0xff))
		append(code, rsp.inst_sll(rsp.T4, rsp.T4, 24))
		append(code, rsp.inst_andi(rsp.T5, rsp.T1, 0xff))
		append(code, rsp.inst_sll(rsp.T5, rsp.T5, 8))
		append(code, rsp.inst_or(rsp.T4, rsp.T4, rsp.T5))
		splat(code, rsp.T4, offset + 16)
	}

	append(&code, rsp.inst_lh(rsp.T0, rsp.mem(ARGS, i32(offset_of(Args, a)))))
	append(&code, rsp.inst_lh(rsp.T1, rsp.mem(ARGS, i32(offset_of(Args, c)))))
	coefficients(&code, 0)
	append(&code, rsp.inst_lh(rsp.T0, rsp.mem(ARGS, i32(offset_of(Args, b)))))
	append(&code, rsp.inst_lh(rsp.T1, rsp.mem(ARGS, i32(offset_of(Args, d)))))
	coefficients(&code, 32)
	append(&code, rsp.inst_lw(rsp.T4, rsp.mem(ARGS, i32(offset_of(Args, tx)))))
	splat(&code, rsp.T4, 64)
	for register, i in ([]rsp.Register{rsp.VR10, rsp.VR11, rsp.VR12, rsp.VR13, rsp.VR14}) {
		append(&code, rsp.inst_lqv(register, rsp.vmem(SCRATCH, 0, i32(i))))
	}
	append(&labels, rsp.Label_Definition(len(code)))
	append(&code, rsp.inst_lqv(rsp.VR1, rsp.vmem(INPUT, 0, 0)))
	append(&code, rsp.inst_vmudn(rsp.VR2, rsp.VR11, rsp.VR1, Q0))
	append(&code, rsp.inst_vmadn(rsp.VR2, rsp.VR13, rsp.VR1, Q1))
	append(&code, rsp.inst_vmadh(rsp.VR2, rsp.VR10, rsp.VR1, Q0))
	append(&code, rsp.inst_vmadh(rsp.VR2, rsp.VR12, rsp.VR1, Q1))
	append(&code, rsp.inst_vmadh(rsp.VR2, rsp.VR14, VSHIFT, E0 + 7))
	append(&code, rsp.inst_sqv(rsp.VR2, rsp.vmem(OUTPUT, 0, 0)))
	append(&code, rsp.inst_addiu(COUNT, COUNT, -4))
	append(&code, rsp.inst_addiu(INPUT, INPUT, 16))
	append(&code, rsp.inst_bgtz(COUNT, 0))
	append(&code, rsp.inst_addiu(OUTPUT, OUTPUT, 16))
	return
}

failures := 0

compare :: proc(name: string, got: []intrinsics.Rsp_Code_Line, want: []rsp.Instruction, labels: []rsp.Label_Definition) {
	code := make([]u8, len(want)*4)
	relocs: [dynamic]rsp.Relocation
	errors: [dynamic]rsp.Error
	_, ok := rsp.encode(want, labels, code, &relocs, &errors)
	if !ok || len(relocs) != 0 {
		fmt.printfln("%s: rexcode failed to encode: %v %v", name, errors, relocs)
		failures += 1
		return
	}
	if len(got) != len(want) {
		fmt.printfln("%s: %d words from the compiler, %d from rexcode", name, len(got), len(want))
		failures += 1
	}
	for line, i in got {
		fmt.printfln("%s %d %08x %d", name, i, line.word, line.line)
		if i >= len(want) {
			continue
		}
		w := u32(code[i*4])<<24 | u32(code[i*4+1])<<16 | u32(code[i*4+2])<<8 | u32(code[i*4+3])
		if w != line.word {
			fmt.printfln("MISMATCH %s[%d] (line %d): compiler %08x, rexcode %08x", name, i, line.line, line.word, w)
			failures += 1
		}
	}
}

main :: proc() {
	scalar, labels := expected_scalar()
	compare("scalar_forms", SCALAR, scalar[:], labels[:])
	edge, edge_labels := expected_edge()
	compare("edge_forms", EDGE, edge[:], edge_labels[:])
	fill := expected_fill()
	compare("fill", FILL, fill[:], nil)
	vector := expected_vector()
	compare("vector_forms", VECTOR, vector[:], nil)
	body, body_labels := expected_transform()
	compare("transform", TRANSFORM, body[:], body_labels[:])
	if len(TRANSFORM) != 61 {
		fmt.printfln("transform: %d words, want the builder body's 61", len(TRANSFORM))
		failures += 1
	}

	// Where each template is declared, for test_rsp_templates.py to check
	for loc in ([]runtime.Source_Code_Location{intrinsics.rsp_template_location(transform.transform),
	                                           intrinsics.rsp_template_location(valid.fill)}) {
		fmt.printfln("location %s %d %d %s", loc.procedure, loc.line, loc.column, loc.file_path)
	}

	// The constant is usable at run time like any #load slice.
	runtime_copy := SCALAR
	if runtime_copy[0].word != 0 || runtime_copy[1].word != SCALAR[1].word {
		fmt.println("a runtime copy of the constant differs")
		failures += 1
	}

	if failures != 0 {
		fmt.printfln("%d failures", failures)
		os.exit(1)
	}
	fmt.println("ok")
}
