// Rejected @(rsp) templates. Each line that must report an error ends with
// `// ERROR: <message prefix>`; test_rsp_templates.py checks that every marked line
// gets its error, on every host target, and that no other line gets one.
package rsp_templates_bad

import "base:intrinsics"

Args :: struct {
	a: i16,
	b: i32,
}

// A pointer field makes the layout the build target's: `a` is at 8 on a 64-bit host
// and at 4 on the N64.
Host_Args :: struct {
	p: rawptr,
	a: i16,
}
Inner :: struct {
	n: [2]int,
}
Outer :: struct {
	inner: Inner,
	a:     i16,
}
HOST_A :: offset_of(Host_Args, a)

global: i32

@(rsp)
ok :: asm(args: ^Args, input: ^u32, count: i32) [args = %s6, input = %s1, count = %s3] {
	lh    %t0, [args + (offset_of(Args, a))]
	addiu input, input, 4
	addiu count, count, -1
}

// A body with no jumps, calls, CP0, DMA or break. rexcode encodes these, and lwv,
// which the RSP does not implement, so each one says why a template cannot use it; an
// instruction rexcode lacks too is unknown.
@(rsp)
profile :: asm() {
	j     .end // ERROR: 'j' jumps out of the template, which an @(rsp) template cannot
	jal   .end // ERROR: 'jal' jumps out of the template
	jr    %t0 // ERROR: 'jr' jumps out of the template
	jalr  %t1, %t0 // ERROR: 'jalr' jumps out of the template
	bltzal %t0, .end // ERROR: 'bltzal' writes %ra, which holds the rspq return address
	bgezal %t0, .end // ERROR: 'bgezal' writes %ra
	break // ERROR: 'break' halts the RSP, which only rspq does
	break 1 // ERROR: 'break' halts the RSP
	mfc0  %t0, %t1 // ERROR: 'mfc0' is CP0 access (DMA, semaphore and RDP registers)
	mtc0  %t0, %t1 // ERROR: 'mtc0' is CP0 access
	lwv   %v01, [%s1] // ERROR: 'lwv' is not implemented by the RSP; LWC2 sub-opcode 0x0a does nothing
	swv   %v01, [%s1]
	mult  %t0, %t1 // ERROR: Unknown mnemonic for this target platform: mult
	bgtz  %t0, 4 // ERROR: 'bgtz' operands
	beq   %t0, %t1, 4 // ERROR: 'beq' operands
	bne   %t0, %t1, .end
.end:
}

// gp and ra hold rspq state; sp is not available at all.
@(rsp)
reserved :: asm() {
	addiu %gp, %gp, 4 // ERROR: 'addiu' writes %gp, which holds the rspq command pointer
	or    %ra, %t0, %t0 // ERROR: 'or' writes %ra, which holds the rspq return address
	addiu %t0, %sp, 4 // ERROR: Register %sp ($29) must not be used in RSP Microcode
	lw    %t0, [%sp + 4] // ERROR: Register %sp ($29) must not be used in RSP Microcode
	lui   %ra, 1 // ERROR: 'lui' writes %ra, which holds the rspq return address
	mfc2  %gp, %v01 // ERROR: 'mfc2' writes %gp, which holds the rspq command pointer
	cfc2  %ra, %vco // ERROR: 'cfc2' writes %ra
	sllv  %gp, %t0, %t1 // ERROR: 'sllv' writes %gp
	or    %t0, %gp, %ra
	mtc2  %gp, %v01
	ctc2  %ra, %vcc
	sw    %ra, [%gp + 4]
	lbu   %ra, [%s1] // ERROR: 'lbu' writes %ra, which holds the rspq return address
	lw    %gp, [%s1 + 4] // ERROR: 'lw' writes %gp, which holds the rspq command pointer
}

@(rsp)
pinned_gp :: asm(p: ^u32) [p = %gp] { // ERROR: 'asm' parameter 'p' cannot be pinned to %gp
	lw    %t0, [p]
}

@(rsp)
unpinned :: asm(p: ^u32) { // ERROR: 'asm' parameter 'p' is not pinned to a register
	lw    %t0, [p]
}

// Element accessors: VU fields 0, 2-15; load/store e0-e7 and b0-b15.
@(rsp)
elements :: asm() {
	vmudn %v01, %v02, %v03.b1 // ERROR: '.b1' on %v03 is not a VU element accessor
	vmudn %v01, %v02, %v03.e8 // ERROR: '.e8' on %v03 is not a VU element accessor
	vmudn %v01, %v02, %v03.q2 // ERROR: '.q2' on %v03 is not a VU element accessor
	vmudn %v01, %v02, %v03.x // ERROR: '.x' on %v03 is not a VU element accessor
	vmudn %v01.e0, %v02, %v03 // ERROR: '.e0' on %v01 takes no element accessor
	vmadh %v01, %v02.q0, %v03 // ERROR: '.q0' on %v02 takes no element accessor
	lqv   %v01.q0, [%s1] // ERROR: '.q0' on %v01 is not a load/store element accessor
	sqv   %v01.e8, [%s1] // ERROR: '.e8' on %v01 is not a load/store element accessor
	lqv   %v01.b16, [%s1] // ERROR: '.b16' on %v01 is not a load/store element accessor
	or    %t0.e0, %t1, %t2 // ERROR: Register flags can only be called on %flags
	mtc2  %t0, %v01.q0 // ERROR: '.q0' on %v01 is not an mtc2/mfc2 element accessor
	mfc2  %t0, %v01.b16 // ERROR: '.b16' on %v01 is not an mtc2/mfc2 element accessor
	lpv   %v01.b1, [%s1] // ERROR: '.b1' on %v01 is not a packed load/store element accessor
	suv   %v01.e8, [%s1] // ERROR: '.e8' on %v01 is not a packed load/store element accessor
	ltv   %v08.b2, [%s1] // ERROR: '.b2' on %v08 is not a transposed load/store element accessor
	stv   %v16.h0, [%s1] // ERROR: '.h0' on %v16 is not a transposed load/store element accessor
	lbv   %v01.q1, [%s1] // ERROR: '.q1' on %v01 is not a load/store element accessor
	vrcp  %v01.q0, %v02.e0 // ERROR: '.q0' on %v01 is not a destination lane
	vrsqh %v01.e8, %v02.e0 // ERROR: '.e8' on %v01 is not a destination lane
	vrsq  %v01.e0, %v02.h1 // ERROR: '.h1' on %v02 is not a source lane
	vrcpl %v01.e7, %v02.b0 // ERROR: '.b0' on %v02 is not a source lane
	vmov  %v01.e8, %v02.e0 // ERROR: '.e8' on %v01 is not a destination lane
	vmov  %v01.e0, %v02.q1 // ERROR: '.q1' on %v02 is not a source lane
	vsar  %v01.e0, %acc_hi // ERROR: '.e0' on %v01 takes no element accessor
	vsar  %v01, %acc_md.e0 // ERROR: '.e0' on %acc_md takes no element accessor
	cfc2  %t0, %vco.e0 // ERROR: '.e0' on %vco takes no element accessor
	cfc2  %t0, %vce.b0 // ERROR: '.b0' on %vce takes no element accessor
	ctc2  %t0.b0, %vcc // ERROR: Register flags can only be called on %flags
	mtc2  %t0, %v01.e7
	lpv   %v01.e7, [%s1]
	ltv   %v24.e7, [%s1]
	vrcp  %v01.e7, %v02.e7
	vmov  %v01.e0, %v02.e7
	vmov  %v01.e5, %v02.e7
	vsar  %v01, %acc_lo
}

// A single-lane instruction names both lanes, as rsp.inc requires. The check reports
// every missing one.
@(rsp)
missing_lanes :: asm() {
	vrcp  %v01, %v02.e0 // ERROR: 'vrcp' operand-0 %v01 needs a destination lane, .e0-.e7
	vrcph %v01.e0, %v02 // ERROR: 'vrcph' operand-1 %v02 needs a source lane, .e0-.e7
	vmov  %v01, %v02.e3 // ERROR: 'vmov' operand-0 %v01 needs a destination lane, .e0-.e7
	vrsql %v01, %v02.e3 // ERROR: 'vrsql' operand-0 %v01 needs a destination lane
	vmov  %v01, %v02 // ERROR: 'vmov' operand-1 %v02 needs a source lane
	vrsq  %v01.e1, %v02.e3
}

// vsar names the accumulator slice it reads; vs and vt are not operands.
@(rsp)
accumulator :: asm() {
	vsar  %v01, %v02 // ERROR: 'vsar' operand-1 must be a named accumulator slice register, got %v02
	vsar  %v01, %v02, %v03.e0 // ERROR: The asm instruction 'vsar' expects 2 operands, got 3
	vsar  %v01, %vcc // ERROR: 'vsar' operand-1 must be a named accumulator slice register, got %vcc
	vsar  %acc_hi, %acc_hi // ERROR: 'vsar' operand-0 cannot be %acc_hi, it must be a vector register
	or    %t0, %acc_lo, %t1 // ERROR: 'or' operand-1 cannot be %acc_lo, it must be a general-purpose register
	vsar  %v01, %acc_hi
	vsar  %v02, %acc_md
}

// ltv and stv move one lane of each of a group of 8 registers, which the RSP
// addresses by vt & ~7.
@(rsp)
transposed :: asm() {
	ltv   %v12.e5, [%s1] // ERROR: 'ltv' operand-0 cannot be %v12, it must be %v00, %v08, %v16 or %v24: ltv and stv address a group of 8 registers
	stv   %v01, [%s1] // ERROR: 'stv' operand-0 cannot be %v01, it must be %v00, %v08, %v16 or %v24
	stv   %v31.e7, [%s1 + 16] // ERROR: 'stv' operand-0 cannot be %v31
	ltv   %v00, [%s1]
	ltv   %v08.e7, [%s1]
	stv   %v16.e2, [%s1]
	stv   %v24, [%s1 - 16]
}

// cfc2 and ctc2 take a COP2 control register by name, and nothing else does.
@(rsp)
control :: asm(p: ^u32) [p = %s1] {
	cfc2  %t0, %t1 // ERROR: 'cfc2' operand-1 must be a named COP2 control register, got %t1
	ctc2  %t0, %v01 // ERROR: 'ctc2' operand-1 must be a named COP2 control register, got %v01
	ctc2  %t0, p // ERROR: 'ctc2' operand-1 must be a named COP2 control register, got the parameter 'p', whose pinned register cannot be one
	lw    %t0, [%vcc + 0] // ERROR: A memory operand's base cannot be %vcc: it is a COP2 control register; a base is a general-purpose register
	sw    %t0, [%v01] // ERROR: A memory operand's base cannot be %v01: it is a vector register
	lqv   %v01, [%acc_hi + 16] // ERROR: A memory operand's base cannot be %acc_hi: it is an accumulator slice
	or    %t0, %vco, %t1 // ERROR: 'or' operand-1 cannot be %vco, it must be a general-purpose register
	vmudn %v01, %v02, %vcc // ERROR: 'vmudn' operand-2 cannot be %vcc, it must be a vector register
	cfc2  %t0, %vco
	ctc2  %t1, %vce
}

@(rsp)
pinned_control :: asm(p: ^u32) [p = %vcc] { // ERROR: 'asm' parameter 'p' cannot be pinned to %vcc
	lw    %t0, [p]
}

// Vector registers have two digits; %v0 and %v1 are GPRs.
@(rsp)
vector_names :: asm() {
	vmudn %v1, %v02, %v03 // ERROR: 'vmudn' operands
	lqv   %v0, [%s1] // ERROR: 'lqv' operands
	vmudn %v2, %v02, %v03 // ERROR: Register %v2 is not a register; vector registers are written with two digits
	or    %v0, %v1, %v1
	addu  %t0, %v01, %t1 // ERROR: 'addu' operand-1 cannot be %v01, it must be a general-purpose register
	lw    %v02, [%s1] // ERROR: 'lw' operand-0 cannot be %v02, it must be a general-purpose register
	mtc2  %v03, %v04 // ERROR: 'mtc2' operand-0 cannot be %v03, it must be a general-purpose register
	mfc2  %t0, %t1 // ERROR: 'mfc2' operand-1 cannot be %t1, it must be a vector register
	vrcp  %v01, %v02, %v03.e0 // ERROR: The asm instruction 'vrcp' expects 2 operands, got 3
	vmov  %v01, %v02, %v03.e0 // ERROR: The asm instruction 'vmov' expects 2 operands, got 3
}

// Offsets: scalar bytes in a signed 16-bit field; vector bytes, a multiple of the
// access size, within -64..63 units of it.
@(rsp)
offsets :: asm(p: ^[64]u8) [p = %s1] {
	lh    %t0, [p + 32768] // ERROR: 'lh' operand-1 memory displacement 32768 does not fit
	sw    %t0, [p - 32769] // ERROR: 'sw' operand-1 memory displacement -32769 does not fit
	lqv   %v01, [p + 8] // ERROR: 'lqv' operand-1 memory displacement 8 is not a multiple of the 16-byte access size
	sqv   %v01, [p + 1024] // ERROR: 'sqv' operand-1 memory displacement 1024 does not fit
	lqv   %v01, [p - 1040] // ERROR: 'lqv' operand-1 memory displacement -1040 does not fit
	lqv   %v01, [p + 1008]
	sqv   %v01, [p - 1024]
	lbv   %v01, [p + 64] // ERROR: 'lbv' operand-1 memory displacement 64 does not fit
	sbv   %v01, [p - 65] // ERROR: 'sbv' operand-1 memory displacement -65 does not fit
	lsv   %v01, [p + 1] // ERROR: 'lsv' operand-1 memory displacement 1 is not a multiple of the 2-byte access size
	ssv   %v01, [p + 128] // ERROR: 'ssv' operand-1 memory displacement 128 does not fit
	llv   %v01, [p + 2] // ERROR: 'llv' operand-1 memory displacement 2 is not a multiple of the 4-byte access size
	slv   %v01, [p - 260] // ERROR: 'slv' operand-1 memory displacement -260 does not fit
	ldv   %v01, [p + 512] // ERROR: 'ldv' operand-1 memory displacement 512 does not fit
	lpv   %v01, [p + 4] // ERROR: 'lpv' operand-1 memory displacement 4 is not a multiple of the 8-byte access size
	suv   %v01, [p - 520] // ERROR: 'suv' operand-1 memory displacement -520 does not fit
	ltv   %v08, [p + 8] // ERROR: 'ltv' operand-1 memory displacement 8 is not a multiple of the 16-byte access size
	shv   %v01, [p + 1024] // ERROR: 'shv' operand-1 memory displacement 1024 does not fit
	lfv   %v01, [p + 24] // ERROR: 'lfv' operand-1 memory displacement 24 is not a multiple of the 16-byte access size
	lbv   %v01, [p + 63]
	sbv   %v01, [p - 64]
	ssv   %v01, [p + 126]
	llv   %v01, [p - 256]
	sdv   %v01, [p + 504]
	lb    %t0, [p + 32768] // ERROR: 'lb' operand-1 memory displacement 32768 does not fit
	sh    %t0, [p - 32768]
}

@(rsp)
immediates :: asm() {
	addiu %t0, %t0, 32768 // ERROR: 'addiu' operand-2 immediate 32768 does not fit
	andi  %t0, %t0, -1 // ERROR: 'andi' operand-2 immediate -1 does not fit
	sll   %t0, %t0, 32 // ERROR: 'sll' operand-2 immediate 32 does not fit
	sra   %t0, %t0, -1 // ERROR: 'sra' operand-2 immediate -1 does not fit
	srl   %t0, %t0, 32 // ERROR: 'srl' operand-2 immediate 32 does not fit
	addi  %t0, %t0, -32769 // ERROR: 'addi' operand-2 immediate -32769 does not fit
	slti  %t0, %t0, 32768 // ERROR: 'slti' operand-2 immediate 32768 does not fit
	sltiu %t0, %t0, -32769 // ERROR: 'sltiu' operand-2 immediate -32769 does not fit
	ori   %t0, %t0, 65536 // ERROR: 'ori' operand-2 immediate 65536 does not fit
	xori  %t0, %t0, -1 // ERROR: 'xori' operand-2 immediate -1 does not fit
	lui   %t0, 65536 // ERROR: 'lui' operand-1 immediate 65536 does not fit
	lui   %t0, -1 // ERROR: 'lui' operand-1 immediate -1 does not fit
	sllv  %t0, %t1, 3 // ERROR: 'sllv' operands
	lui   %t0, 0xffff
	sltiu %t0, %t0, -1
}

// Rules of the host's own architecture do not apply to an @(rsp) template,
// so these stay errors when the host is arm64.
@(rsp)
host_rules :: asm(p: ^u32) [p = %s1] {
	lw    %t0, #post [p + 4] // ERROR: #pre/#post asm memory operands are not supported by the target platform
	lw    %t0, [p + %t1 >> 2] // ERROR: The target platform does not support '>>' for shifting scale parameters in memory operands
	vmudn %v01, %v02, %v03[3] // ERROR: The target platform does not support vector element extraction syntax
}

@(rsp=true) // ERROR: Expected no value for 'rsp'
with_value :: asm() {
	nop
}

// What only the encoder rejects is still reported by `odin check`, though nothing
// takes these templates' words. Each is its own template: a template is encoded only
// when the rest of its check reported nothing.
@(rsp)
directive :: asm() {
	nop
	#byte 1 // ERROR: Asm directives are not supported in an @(rsp) template
}

@(rsp)
no_base :: asm() {
	lw    %t0, [8] // ERROR: An @(rsp) memory operand is a base register plus a constant displacement
}

@(rsp)
no_base_hex :: asm() {
	sw    %t0, [0x100] // ERROR: An @(rsp) memory operand is a base register plus a constant displacement
}

@(rsp)
no_base_vector :: asm() {
	lqv   %v01, [16] // ERROR: An @(rsp) memory operand is a base register plus a constant displacement
}

@(rsp)
not_constant :: asm() {
	addiu %t0, %t0, global // ERROR: Only asm parameters or constants are allowed to be used within an 'asm' template
}

@(rsp)
no_label :: asm() {
	bgtz  %t0, .nowhere // ERROR: Undeclared asm label '.nowhere'
	nop
}

// Sizes are the RSP's, whatever the host: no type whose size is the build target's.
@(rsp)
host_int :: asm(count: int, p: uintptr) [count = %s3, p = %s4] { // ERROR: 'count' has type 'int', whose size is the build target's
	addiu count, count, -4
}

@(rsp)
host_uint :: asm() -> (n: uint) [n = %s3] { // ERROR: 'n' has type 'uint', whose size is the build target's
	addiu n, %zero, 4
}

// ... and no layout measure of a type whose layout is the build target's.
@(rsp)
host_layout :: asm(args: ^Host_Args) [args = %s6] {
	lh    %t0, [args + (offset_of(Host_Args, a))] // ERROR: 'offset_of(Host_Args, a)' measures 'Host_Args', whose layout is the build target's because it holds 'rawptr'
	lh    %t0, [args + HOST_A] // ERROR: 'offset_of(Host_Args, a)' measures 'Host_Args'
	lh    %t0, [args + (offset_of(Outer, a))] // ERROR: 'offset_of(Outer, a)' measures 'Outer', whose layout is the build target's because it holds 'int'
	addiu %t0, %t0, (size_of(int)) // ERROR: 'size_of(int)' measures 'int', whose layout is the build target's
	addiu %t0, %t0, (align_of(uintptr)) // ERROR: 'align_of(uintptr)' measures 'uintptr'
	addiu %t0, %t0, (size_of(Args) + size_of(i32))
	lh    %t0, [args + (offset_of(Args, b))]
}

// A template cannot take its own words.
@(rsp)
self_reference :: asm(input: ^u32) [input = %s1] {
	addiu input, input, (len(intrinsics.rsp_template_code(self_reference))) // ERROR: 'intrinsics.rsp_template_code(self_reference)' refers to the template being checked
}

// A template with errors still gives an `[]Rsp_Code_Line` (an empty one), so taking
// its words adds no error, and the encoder repeats none of the check's.
@(rsp)
indexed :: asm(input: ^u32) [input = %s1] {
	lw    %t0, [input + %t1] // ERROR: The target platform does not support memory indexing within memory operands
}
INDEXED :: intrinsics.rsp_template_code(indexed)

indexed_words :: proc() -> int {
	return len(INDEXED)
}

// An @(rsp) template is Microcode, never a CPU callee.
group :: asm { ok } // ERROR: An @(rsp) template is RSP Microcode and cannot be in an asm template group

call_it :: proc() {
	ok(nil, nil, 0) // ERROR: 'ok' is an @(rsp) template: RSP Microcode, which CPU code cannot call
}

not_a_template :: proc() {}
NOT_A_TEMPLATE :: intrinsics.rsp_template_code(not_a_template) // ERROR: 'rsp_template_code' expects a declared @(rsp) asm template

cpu_template_location :: proc() {
	_ = intrinsics.rsp_template_location(not_a_template) // ERROR: 'rsp_template_location' expects a declared @(rsp) asm template
}

// `#unroll for`: an error inside an unrolled body is reported at the body's line, for
// the iteration that has it.
@(rsp)
unrolled_errors :: asm(p: ^[64]u8) [p = %s1] {
	#unroll for i in 0..<3 {
		addiu %t0, %t0, (i * 16384) // ERROR: 'addiu' operand-2 immediate 32768 does not fit
		lqv   %v01, [p + (8*i)] // ERROR: 'lqv' operand-1 memory displacement 8 is not a multiple of the 16-byte access size
		sw    %t0, [p + (4*i)]
	}
	#unroll for vr, i in {%v30..=%v31} {
		#unroll for lane in 0..<2 {
			vmadh %vr, %vr, %v00.e7
			addiu %t0, %t0, (65536*lane + i) // ERROR: 'addiu' operand-2 immediate 65536 does not fit
		}
	}
}

@(rsp)
unroll_misuse :: asm(p: ^u32, count: i32) [p = %s1, count = %s3] {
	#unroll for i in 0..<2 {
.again: // ERROR: An asm label can't be declared inside an '#unroll for' body
		addiu %t0, %t0, i
	}
	#unroll for x in global { // ERROR: An '#unroll for' in an @(rsp) template iterates over a constant range, a constant fixed array of integers or a register group
		sw    %t0, [p + x]
	}
	#unroll for i in 0..<count { // ERROR: 'count' is a parameter of the template, a register at run time
		nop
	}
	#unroll for count in 0..<2 { // ERROR: '#unroll for' variable 'count' is a parameter of the template
		nop
	}
	#unroll for t0 in 0..<2 { // ERROR: '#unroll for' variable 't0' is an RSP register's name
		nop
	}
	#unroll for i in 0..<2 {
		#unroll for i in 0..<2 { // ERROR: '#unroll for' variable 'i' is the variable of an enclosing '#unroll for'
			nop
		}
	}
	#unroll for r in {%v10..=%t3} { // ERROR: An '#unroll for' register range steps the number at the end of one register name
		nop
	}
	#unroll for r in {%v30..=%v32} { // ERROR: '%v32' in this '#unroll for' register group is not an RSP register
		nop
	}
	#unroll for r in {%v10.e0, %v11} { // ERROR: An '#unroll for' register group holds registers without accessors
		nop
	}
	#unroll for r in {%v10, %v11} {
		addiu %t0, %t0, (r) // ERROR: 'r' is the register variable of an '#unroll for', named as an operand
	}
}

UNROLL_SLICE :: []i32{1, 2}

@(rsp)
unroll_review :: asm(p: ^u32, count: i32) [p = %s1, count = %s3, word: u32 = %t2] {
	#unroll for word in 0..<2 { // ERROR: '#unroll for' variable 'word' is a scratch register of the template
		nop
	}
	#unroll for i in u64(0)..<u64(0x8000_0000_0000_0002) { // ERROR: An '#unroll for' range bound must fit in an i64
		nop
	}
	#unroll for i in 0..<((1<<64)+2) { // ERROR: An '#unroll for' range bound must fit in an i64
		nop
	}
	#unroll for i in UNROLL_SLICE { // ERROR: Cannot iterate over 'UNROLL_SLICE' of type '[]i32' in an '#unroll for' statement
		nop
	}
	#unroll for i in 0..<(count + 1) { // ERROR: 'count' is a parameter of the template, a register at run time
		nop
	}
	#unroll for r in {%t0, %t1} {
		#unroll for j in 0..<r { // ERROR: 'r' is the register variable of an '#unroll for', named as an operand
			nop
		}
	}
	#unroll for r in {%v14..=%v10} { // ERROR: An '#unroll for' register range steps upward
		nop
	}
	#unroll for r in {%t5..=%t3} { // ERROR: An '#unroll for' register range steps upward
		nop
	}
	#unroll for i, i in 0..<2 { // ERROR: '#unroll for' variable 'i' is declared twice in this '#unroll for'
		nop
	}
	#unroll for i in 0..<1025 { // ERROR: Too many iterations for an '#unroll for' in an @(rsp) template, expected <= 1024
		nop
	}
}

// The template's `#unroll for`s expand to at most 1024 instructions in all, reported at
// the outermost `#unroll for`
@(rsp)
unroll_nested_total :: asm() {
	#unroll for i in 0..<1024 { // ERROR: '#unroll for' expands past 1024 instructions in this @(rsp) template
		#unroll for j in 0..<1024 {
			nop
		}
	}
}

@(rsp)
unroll_nested_empty :: asm() {
	#unroll for i in 0..<1024 { // ERROR: '#unroll for' expands past 1024 instructions in this @(rsp) template
		#unroll for j in 0..<1024 {
			#unroll for k in 0..<1024 {
			}
		}
	}
}

// `#unroll for` over the operand kinds the full table added: a lane, group or
// control-register error in an unrolled body is reported at the body's line.
@(rsp)
unrolled_operand_errors :: asm(p: ^u32) [p = %s1] {
	#unroll for r in {%v01..=%v02} {
		vrcp  %r, %v17.e5 // ERROR: 'vrcp' operand-0 %v01 needs a destination lane, .e0-.e7
		vsar  %r, %v03 // ERROR: 'vsar' operand-1 must be a named accumulator slice register, got %v03
	}
	#unroll for g in {%v00, %v04} {
		ltv   %g, [p] // ERROR: 'ltv' operand-0 cannot be %v04, it must be %v00, %v08, %v16 or %v24
	}
	#unroll for g, i in {%v08, %v16} {
		stv   %g.e2, [p + (8*i)] // ERROR: 'stv' operand-1 memory displacement 8 is not a multiple of the 16-byte access size
	}
	#unroll for c in {%vco, %v01} {
		cfc2  %t0, %c // ERROR: 'cfc2' operand-1 must be a named COP2 control register, got %v01
	}
}
