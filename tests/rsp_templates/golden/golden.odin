// Every form of the @(rsp) table (src/asm_tables_rsp.cpp), encoded twice: by the
// compiler through intrinsics.rsp_template_code, and by core:rexcode/isa/rsp from the
// same instructions written as rexcode values. The words must agree, and every rexcode
// RSP mnemonic must be diffed here or be one the table rejects. Run on the host:
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

// The rest of the scalar core: every other R-type, shift, immediate, branch and
// scalar load/store, at the edges of each immediate.
@(rsp)
scalar_rest :: asm(p: ^u32) [p = %s1] {
.top:
	add   %t0, %t1, %t2
	addu  %t3, %t4, %t5
	sub   %t6, %t7, %s0
	subu  %s2, %s3, %s4
	and   %s5, %s6, %s7
	xor   %t8, %t9, %k0
	nor   %k1, %fp, %at
	slt   %v0, %v1, %a0
	sltu  %a1, %a2, %a3
	srl   %t0, %t1, 0
	srl   %t0, %t1, 31
	sllv  %t2, %t3, %t4
	srlv  %t5, %t6, %t7
	srav  %s0, %s1, %s2
	addi  %t0, %t1, -32768
	addi  %t0, %t1, 32767
	slti  %t2, %t3, -1
	sltiu %t4, %t5, 32767
	ori   %t6, %t7, 0
	ori   %t6, %t7, 0xffff
	xori  %s0, %s1, 0x8000
	lui   %s2, 0
	lui   %s3, 0xffff
	lb    %t0, [p - 32768]
	lbu   %t1, [p + 32767]
	lhu   %t2, [p + 2]
	sb    %t3, [p + 1]
	sh    %t4, [%zero + 6]
	beq   %t0, %t1, .top
	bne   %t2, %zero, .end
	blez  %t3, .top
	bltz  %t4, .mid
.mid:
	bgez  %t5, .end
	nop
.end:
}

// mtc2/mfc2 with each kind of byte element, and cfc2/ctc2 with each control register.
@(rsp)
cop2_forms :: asm() {
	mtc2  %t0, %v01
	mtc2  %t1, %v02.e0
	mtc2  %t2, %v03.e7
	mtc2  %t3, %v04.b1
	mtc2  %at, %v31.b15
	mfc2  %t4, %v05
	mfc2  %t5, %v06.e3
	mfc2  %t6, %v07.b9
	cfc2  %t7, %vco
	cfc2  %s0, %vcc
	cfc2  %s1, %vce
	ctc2  %s2, %vco
	ctc2  %s3, %vcc
	ctc2  %zero, %vce
}

// Every VU instruction: the three-operand ones through every element accessor, vsar
// with each accumulator slice, the single-lane ones and vmov with two lane pairs
// each, and vnop.
@(rsp)
vu_forms :: asm() {
	vmulf %v00, %v11, %v23
	vmulu %v07, %v18, %v30.q0
	vmudl %v14, %v25, %v05.q1
	vmudm %v21, %v00, %v12.h0
	vmudn %v28, %v07, %v19.h1
	vmudh %v03, %v14, %v26.h2
	vmacf %v10, %v21, %v01.h3
	vmacu %v17, %v28, %v08.e0
	vmadl %v24, %v03, %v15.e1
	vmadm %v31, %v10, %v22.e2
	vmadn %v06, %v17, %v29.e3
	vmadh %v13, %v24, %v04.e4
	vadd  %v20, %v31, %v11.e5
	vsub  %v27, %v06, %v18.e6
	vabs  %v02, %v13, %v25.e7
	vaddc %v09, %v20, %v00
	vsubc %v16, %v27, %v07.q0
	vsar  %v23, %acc_hi
	vsar  %v00, %acc_md
	vsar  %v31, %acc_lo
	vlt   %v30, %v09, %v21.h0
	veq   %v05, %v16, %v28.h1
	vne   %v12, %v23, %v03.h2
	vge   %v19, %v30, %v10.h3
	vcl   %v26, %v05, %v17.e0
	vch   %v01, %v12, %v24.e1
	vcr   %v08, %v19, %v31.e2
	vmrg  %v15, %v26, %v06.e3
	vand  %v22, %v01, %v13.e4
	vnand %v29, %v08, %v20.e5
	vor   %v04, %v15, %v27.e6
	vnor  %v11, %v22, %v02.e7
	vxor  %v18, %v29, %v09
	vnxor %v25, %v04, %v16.q0
	vrcp  %v01.e0, %v17.e7
	vrcp  %v01.e3, %v17.e5
	vrcpl %v06.e1, %v22.e6
	vrcpl %v06.e4, %v22.e6
	vrcph %v11.e2, %v27.e5
	vrcph %v11.e5, %v27.e7
	vrsq  %v16.e3, %v00.e4
	vrsq  %v16.e6, %v00.e0
	vrsql %v21.e4, %v05.e3
	vrsql %v21.e7, %v05.e1
	vrsqh %v26.e5, %v10.e2
	vrsqh %v26.e0, %v10.e2
	vmov  %v00.e0, %v31.e0
	vmov  %v30.e0, %v01.e7
	vmov  %v12.e0, %v12.e4
	vmov  %v01.e3, %v02.e0
	vmov  %v31.e7, %v15.e2
	vnop
}

// Every vector load and store: no element, a lane or byte element, and each access
// size's lowest and highest byte offset.
@(rsp)
vls_forms :: asm() {
	lbv   %v00, [%s1]
	lbv   %v11.e1, [%s2 - 64]
	lbv   %v22.b0, [%t0 + 63]
	lsv   %v03, [%s2]
	lsv   %v14.e2, [%t0 - 128]
	lsv   %v25.b3, [%zero + 126]
	llv   %v06, [%t0]
	llv   %v17.e3, [%zero - 256]
	llv   %v28.b6, [%s1 + 252]
	ldv   %v09, [%zero]
	ldv   %v20.e4, [%s1 - 512]
	ldv   %v31.b9, [%s2 + 504]
	lqv   %v12, [%s1]
	lqv   %v23.e5, [%s2 - 1024]
	lqv   %v02.b12, [%t0 + 1008]
	lrv   %v15, [%s2]
	lrv   %v26.e6, [%t0 - 1024]
	lrv   %v05.b15, [%zero + 1008]
	lpv   %v18, [%t0]
	lpv   %v29.e7, [%zero - 512]
	lpv   %v08.e7, [%s1 + 504]
	luv   %v21, [%zero]
	luv   %v00.e1, [%s1 - 512]
	luv   %v11.e7, [%s2 + 504]
	lhv   %v24, [%s1]
	lhv   %v03.e2, [%s2 - 1024]
	lhv   %v14.b8, [%t0 + 1008]
	lfv   %v27, [%s2]
	lfv   %v06.e3, [%t0 - 1024]
	lfv   %v17.b11, [%zero + 1008]
	ltv   %v16, [%zero]
	ltv   %v08.e5, [%s1 - 1024]
	ltv   %v24.e7, [%s2 + 1008]
	sbv   %v04, [%s1]
	sbv   %v15.e6, [%s2 - 64]
	sbv   %v26.b4, [%t0 + 63]
	ssv   %v07, [%s2]
	ssv   %v18.e7, [%t0 - 128]
	ssv   %v29.b7, [%zero + 126]
	slv   %v10, [%t0]
	slv   %v21.e1, [%zero - 256]
	slv   %v00.b10, [%s1 + 252]
	sdv   %v13, [%zero]
	sdv   %v24.e2, [%s1 - 512]
	sdv   %v03.b13, [%s2 + 504]
	sqv   %v16, [%s1]
	sqv   %v27.e3, [%s2 - 1024]
	sqv   %v06.b0, [%t0 + 1008]
	srv   %v19, [%s2]
	srv   %v30.e4, [%t0 - 1024]
	srv   %v09.b3, [%zero + 1008]
	spv   %v22, [%t0]
	spv   %v01.e5, [%zero - 512]
	spv   %v12.e7, [%s1 + 504]
	suv   %v25, [%zero]
	suv   %v04.e6, [%s1 - 512]
	suv   %v15.e7, [%s2 + 504]
	shv   %v28, [%s1]
	shv   %v07.e7, [%s2 - 1024]
	shv   %v18.b12, [%t0 + 1008]
	sfv   %v31, [%s2]
	sfv   %v10.e1, [%t0 - 1024]
	sfv   %v21.b15, [%zero + 1008]
	swv   %v02, [%t0]
	swv   %v13.e2, [%zero - 1024]
	swv   %v24.b2, [%s1 + 1008]
	stv   %v00, [%zero]
	stv   %v16.e3, [%s1 - 1024]
	stv   %v24.e7, [%s2 + 1008]
}

SCALAR    :: intrinsics.rsp_template_code(scalar_forms)
EDGE      :: intrinsics.rsp_template_code(edge_forms)
FILL      :: intrinsics.rsp_template_code(valid.fill)
TRANSFORM :: intrinsics.rsp_template_code(transform.transform)
VECTOR    :: intrinsics.rsp_template_code(vector_forms)
REST      :: intrinsics.rsp_template_code(scalar_rest)
COP2      :: intrinsics.rsp_template_code(cop2_forms)
VU        :: intrinsics.rsp_template_code(vu_forms)
VLS       :: intrinsics.rsp_template_code(vls_forms)

// rexcode's RSP mnemonics an @(rsp) template may not use (asm_tables_rsp.cpp's
// mnemonic_unavailable_reason; bad.odin's `profile` checks each one's error). LWV is
// one rexcode encodes but the RSP does not implement.
REJECTED :: bit_set[rsp.Mnemonic]{.J, .JAL, .JR, .JALR, .BLTZAL, .BGEZAL, .MFC0, .MTC0, .BREAK, .LWV}

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

// A VU instruction from rexcode's three-operand builder, with its mnemonic swapped in
vu :: proc(m: rsp.Mnemonic, d, s, t: int, element: u8) -> rsp.Instruction {
	inst := rsp.inst_vmudn(vr(d), vr(s), vr(t), element)
	inst.mnemonic = m
	return inst
}

// rexcode's single-lane form: vd, the destination lane as a register number (`de`),
// vt, and the element field 8 + the source lane
single :: proc(m: rsp.Mnemonic, d, de, t, lane: int) -> rsp.Instruction {
	inst := rsp.inst_vrcp(vr(d), vr(de), vr(t), u8(8 + lane))
	inst.mnemonic = m
	return inst
}

// A vector load/store; rexcode's offset is in units of the access size
vls :: proc(m: rsp.Mnemonic, t: int, base: rsp.Register, element: u8, units: i32) -> rsp.Instruction {
	inst := rsp.inst_lqv(vr(t), rsp.vmem(base, element, units))
	inst.mnemonic = m
	return inst
}

expected_rest :: proc() -> (code: [dynamic]rsp.Instruction, labels: [dynamic]rsp.Label_Definition) {
	TOP :: 0; MID :: 1; END :: 2
	append(&labels, rsp.Label_Definition(0), rsp.Label_Definition(32), rsp.Label_Definition(34))
	append(&code,
		rsp.inst_add(rsp.T0, rsp.T1, rsp.T2),
		rsp.inst_addu(rsp.T3, rsp.T4, rsp.T5),
		rsp.inst_sub(rsp.T6, rsp.T7, rsp.S0),
		rsp.inst_subu(rsp.S2, rsp.S3, rsp.S4),
		rsp.inst_and(rsp.S5, rsp.S6, rsp.S7),
		rsp.inst_xor(rsp.T8, rsp.T9, rsp.K0),
		rsp.inst_nor(rsp.K1, rsp.FP, rsp.AT),
		rsp.inst_slt(rsp.V0, rsp.V1, rsp.A0),
		rsp.inst_sltu(rsp.A1, rsp.A2, rsp.A3),
		rsp.inst_srl(rsp.T0, rsp.T1, 0),
		rsp.inst_srl(rsp.T0, rsp.T1, 31),
		rsp.inst_sllv(rsp.T2, rsp.T3, rsp.T4),
		rsp.inst_srlv(rsp.T5, rsp.T6, rsp.T7),
		rsp.inst_srav(rsp.S0, rsp.S1, rsp.S2),
		rsp.inst_addi(rsp.T0, rsp.T1, -32768),
		rsp.inst_addi(rsp.T0, rsp.T1, 32767),
		rsp.inst_slti(rsp.T2, rsp.T3, -1),
		rsp.inst_sltiu(rsp.T4, rsp.T5, 32767),
		rsp.inst_ori(rsp.T6, rsp.T7, 0),
		rsp.inst_ori(rsp.T6, rsp.T7, 0xffff),
		rsp.inst_xori(rsp.S0, rsp.S1, 0x8000),
		rsp.inst_lui(rsp.S2, 0),
		rsp.inst_lui(rsp.S3, 0xffff),
		rsp.inst_lb(rsp.T0, rsp.mem(rsp.S1, -32768)),
		rsp.inst_lbu(rsp.T1, rsp.mem(rsp.S1, 32767)),
		rsp.inst_lhu(rsp.T2, rsp.mem(rsp.S1, 2)),
		rsp.inst_sb(rsp.T3, rsp.mem(rsp.S1, 1)),
		rsp.inst_sh(rsp.T4, rsp.mem(rsp.ZERO, 6)),
		rsp.inst_beq(rsp.T0, rsp.T1, TOP),
		rsp.inst_bne(rsp.T2, rsp.ZERO, END),
		rsp.inst_blez(rsp.T3, TOP),
		rsp.inst_bltz(rsp.T4, MID),
		rsp.inst_bgez(rsp.T5, END),
		rsp.inst_nop(),
	)
	return
}

expected_cop2 :: proc() -> (code: [dynamic]rsp.Instruction) {
	append(&code,
		rsp.inst_mtc2(rsp.T0, vr(1), 0),
		rsp.inst_mtc2(rsp.T1, vr(2), 0),
		rsp.inst_mtc2(rsp.T2, vr(3), 14),
		rsp.inst_mtc2(rsp.T3, vr(4), 1),
		rsp.inst_mtc2(rsp.AT, vr(31), 15),
		rsp.inst_mfc2(rsp.T4, vr(5), 0),
		rsp.inst_mfc2(rsp.T5, vr(6), 6),
		rsp.inst_mfc2(rsp.T6, vr(7), 9),
		rsp.inst_cfc2(rsp.T7, rsp.VCO),
		rsp.inst_cfc2(rsp.S0, rsp.VCC),
		rsp.inst_cfc2(rsp.S1, rsp.VCE),
		rsp.inst_ctc2(rsp.S2, rsp.VCO),
		rsp.inst_ctc2(rsp.S3, rsp.VCC),
		rsp.inst_ctc2(rsp.ZERO, rsp.VCE),
	)
	return
}

expected_vu :: proc() -> (code: [dynamic]rsp.Instruction) {
	append(&code,
		vu(.VMULF, 0, 11, 23, 0),
		vu(.VMULU, 7, 18, 30, 2),
		vu(.VMUDL, 14, 25, 5, 3),
		vu(.VMUDM, 21, 0, 12, 4),
		vu(.VMUDN, 28, 7, 19, 5),
		vu(.VMUDH, 3, 14, 26, 6),
		vu(.VMACF, 10, 21, 1, 7),
		vu(.VMACU, 17, 28, 8, 8),
		vu(.VMADL, 24, 3, 15, 9),
		vu(.VMADM, 31, 10, 22, 10),
		vu(.VMADN, 6, 17, 29, 11),
		vu(.VMADH, 13, 24, 4, 12),
		vu(.VADD, 20, 31, 11, 13),
		vu(.VSUB, 27, 6, 18, 14),
		vu(.VABS, 2, 13, 25, 15),
		vu(.VADDC, 9, 20, 0, 0),
		vu(.VSUBC, 16, 27, 7, 2),
		rsp.inst_vsar_acc(vr(23), .HI),
		rsp.inst_vsar_acc(vr(0), .MD),
		rsp.inst_vsar_acc(vr(31), .LO),
		vu(.VLT, 30, 9, 21, 4),
		vu(.VEQ, 5, 16, 28, 5),
		vu(.VNE, 12, 23, 3, 6),
		vu(.VGE, 19, 30, 10, 7),
		vu(.VCL, 26, 5, 17, 8),
		vu(.VCH, 1, 12, 24, 9),
		vu(.VCR, 8, 19, 31, 10),
		vu(.VMRG, 15, 26, 6, 11),
		vu(.VAND, 22, 1, 13, 12),
		vu(.VNAND, 29, 8, 20, 13),
		vu(.VOR, 4, 15, 27, 14),
		vu(.VNOR, 11, 22, 2, 15),
		vu(.VXOR, 18, 29, 9, 0),
		vu(.VNXOR, 25, 4, 16, 2),
		single(.VRCP, 1, 0, 17, 7),
		single(.VRCP, 1, 3, 17, 5),
		single(.VRCPL, 6, 1, 22, 6),
		single(.VRCPL, 6, 4, 22, 6),
		single(.VRCPH, 11, 2, 27, 5),
		single(.VRCPH, 11, 5, 27, 7),
		single(.VRSQ, 16, 3, 0, 4),
		single(.VRSQ, 16, 6, 0, 0),
		single(.VRSQL, 21, 4, 5, 3),
		single(.VRSQL, 21, 7, 5, 1),
		single(.VRSQH, 26, 5, 10, 2),
		single(.VRSQH, 26, 0, 10, 2),
		rsp.inst_vmov(vr(0), vr(31), 8),
		rsp.inst_vmov(vr(30), vr(1), 15),
		rsp.inst_vmov(vr(12), vr(12), 12),
		rsp.inst_vmov(vr(1), vr(2), 8),
		rsp.inst_vmov(vr(31), vr(15), 10),
		rsp.inst_vnop(),
	)
	return
}

// rexcode's vmov form has no destination lane field (`de`, the vs field), which the
// RSP honours (ares: vd.u16(de) = vt(e).u16(de)), so the vmov words with a lane other
// than 0 are rexcode's with `de << 11` added: expected_vu's index and lane.
VMOV_DE :: [?][2]int{{49, 3}, {50, 7}}

vu_extra :: proc() -> (extra: []u32) {
	extra = make([]u32, len(VU))
	for d in VMOV_DE {
		extra[d[0]] = u32(d[1]) << 11
	}
	return
}

expected_vls :: proc() -> (code: [dynamic]rsp.Instruction) {
	append(&code,
		vls(.LBV, 0, rsp.S1, 0, 0),
		vls(.LBV, 11, rsp.S2, 2, -64),
		vls(.LBV, 22, rsp.T0, 0, 63),
		vls(.LSV, 3, rsp.S2, 0, 0),
		vls(.LSV, 14, rsp.T0, 4, -64),
		vls(.LSV, 25, rsp.ZERO, 3, 63),
		vls(.LLV, 6, rsp.T0, 0, 0),
		vls(.LLV, 17, rsp.ZERO, 6, -64),
		vls(.LLV, 28, rsp.S1, 6, 63),
		vls(.LDV, 9, rsp.ZERO, 0, 0),
		vls(.LDV, 20, rsp.S1, 8, -64),
		vls(.LDV, 31, rsp.S2, 9, 63),
		vls(.LQV, 12, rsp.S1, 0, 0),
		vls(.LQV, 23, rsp.S2, 10, -64),
		vls(.LQV, 2, rsp.T0, 12, 63),
		vls(.LRV, 15, rsp.S2, 0, 0),
		vls(.LRV, 26, rsp.T0, 12, -64),
		vls(.LRV, 5, rsp.ZERO, 15, 63),
		vls(.LPV, 18, rsp.T0, 0, 0),
		vls(.LPV, 29, rsp.ZERO, 7, -64),
		vls(.LPV, 8, rsp.S1, 7, 63),
		vls(.LUV, 21, rsp.ZERO, 0, 0),
		vls(.LUV, 0, rsp.S1, 1, -64),
		vls(.LUV, 11, rsp.S2, 7, 63),
		vls(.LHV, 24, rsp.S1, 0, 0),
		vls(.LHV, 3, rsp.S2, 4, -64),
		vls(.LHV, 14, rsp.T0, 8, 63),
		vls(.LFV, 27, rsp.S2, 0, 0),
		vls(.LFV, 6, rsp.T0, 6, -64),
		vls(.LFV, 17, rsp.ZERO, 11, 63),
		vls(.LTV, 16, rsp.ZERO, 0, 0),
		vls(.LTV, 8, rsp.S1, 10, -64),
		vls(.LTV, 24, rsp.S2, 14, 63),
		vls(.SBV, 4, rsp.S1, 0, 0),
		vls(.SBV, 15, rsp.S2, 12, -64),
		vls(.SBV, 26, rsp.T0, 4, 63),
		vls(.SSV, 7, rsp.S2, 0, 0),
		vls(.SSV, 18, rsp.T0, 14, -64),
		vls(.SSV, 29, rsp.ZERO, 7, 63),
		vls(.SLV, 10, rsp.T0, 0, 0),
		vls(.SLV, 21, rsp.ZERO, 2, -64),
		vls(.SLV, 0, rsp.S1, 10, 63),
		vls(.SDV, 13, rsp.ZERO, 0, 0),
		vls(.SDV, 24, rsp.S1, 4, -64),
		vls(.SDV, 3, rsp.S2, 13, 63),
		vls(.SQV, 16, rsp.S1, 0, 0),
		vls(.SQV, 27, rsp.S2, 6, -64),
		vls(.SQV, 6, rsp.T0, 0, 63),
		vls(.SRV, 19, rsp.S2, 0, 0),
		vls(.SRV, 30, rsp.T0, 8, -64),
		vls(.SRV, 9, rsp.ZERO, 3, 63),
		vls(.SPV, 22, rsp.T0, 0, 0),
		vls(.SPV, 1, rsp.ZERO, 5, -64),
		vls(.SPV, 12, rsp.S1, 7, 63),
		vls(.SUV, 25, rsp.ZERO, 0, 0),
		vls(.SUV, 4, rsp.S1, 6, -64),
		vls(.SUV, 15, rsp.S2, 7, 63),
		vls(.SHV, 28, rsp.S1, 0, 0),
		vls(.SHV, 7, rsp.S2, 14, -64),
		vls(.SHV, 18, rsp.T0, 12, 63),
		vls(.SFV, 31, rsp.S2, 0, 0),
		vls(.SFV, 10, rsp.T0, 2, -64),
		vls(.SFV, 21, rsp.ZERO, 15, 63),
		vls(.SWV, 2, rsp.T0, 0, 0),
		vls(.SWV, 13, rsp.ZERO, 4, -64),
		vls(.SWV, 24, rsp.S1, 2, 63),
		vls(.STV, 0, rsp.ZERO, 0, 0),
		vls(.STV, 16, rsp.S1, 6, -64),
		vls(.STV, 24, rsp.S2, 14, 63),
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

// `extra`, when given, holds bits to add to each of rexcode's words: fields its forms lack.
compare :: proc(name: string, got: []intrinsics.Rsp_Code_Line, want: []rsp.Instruction, labels: []rsp.Label_Definition, extra: []u32 = nil) {
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
		if i < len(extra) {
			w |= extra[i]
		}
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
	rest, rest_labels := expected_rest()
	compare("scalar_rest", REST, rest[:], rest_labels[:])
	cop2 := expected_cop2()
	compare("cop2_forms", COP2, cop2[:], nil)
	vu_code := expected_vu()
	compare("vu_forms", VU, vu_code[:], nil, vu_extra())
	vls_code := expected_vls()
	compare("vls_forms", VLS, vls_code[:], nil)

	// Every rexcode RSP mnemonic (one form each) is diffed above, or is one the
	// table rejects and none of the templates holds.
	covered: bit_set[rsp.Mnemonic]
	for list in ([][]rsp.Instruction{scalar[:], edge[:], fill[:], vector[:], rest[:], cop2[:], vu_code[:], vls_code[:]}) {
		for inst in list {
			covered += {inst.mnemonic}
		}
	}
	for m in rsp.Mnemonic {
		if m == .INVALID {
			continue
		}
		if n := rsp.ENCODE_RUNS[u16(m)].count; n != 1 {
			fmt.printfln("rexcode's %v has %d forms; the golden templates cover one", m, n)
			failures += 1
		}
		if m in REJECTED {
			if m in covered {
				fmt.printfln("%v is rejected by the table but in a golden template", m)
				failures += 1
			}
		} else if m not_in covered {
			fmt.printfln("%v: no golden template encodes it", m)
			failures += 1
		}
	}
	fmt.printfln("mnemonics: %d diffed, %d rejected", card(covered), card(REJECTED))

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
