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

// A body with no jumps, calls, CP0, DMA or break. rexcode encodes these, so each one
// says why a template cannot use it; an instruction the RSP lacks is unknown.
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
	ltv   %v01.b2, [%s1] // ERROR: '.b2' on %v01 is not a transposed load/store element accessor
	stv   %v01.h0, [%s1] // ERROR: '.h0' on %v01 is not a transposed load/store element accessor
	lbv   %v01.q1, [%s1] // ERROR: '.q1' on %v01 is not a load/store element accessor
	vrcp  %v01.q0, %v02.e0 // ERROR: '.q0' on %v01 is not a destination lane
	vrsqh %v01.e8, %v02.e0 // ERROR: '.e8' on %v01 is not a destination lane
	vrsq  %v01.e0, %v02.h1 // ERROR: '.h1' on %v02 is not a source lane
	vrcpl %v01.e7, %v02.b0 // ERROR: '.b0' on %v02 is not a source lane
	vmov  %v01.e1, %v02.e0 // ERROR: '.e1' on %v01 is not vmov's destination lane
	vmov  %v01.e0, %v02.q1 // ERROR: '.q1' on %v02 is not a source lane
	vsar  %v01.e0, %v02, %v03.e0 // ERROR: '.e0' on %v01 takes no element accessor
	cfc2  %t0, %vco.e0 // ERROR: Register flags can only be called on %flags
	mtc2  %t0, %v01.e7
	lpv   %v01.e7, [%s1]
	ltv   %v01.e7, [%s1]
	vrcp  %v01.e7, %v02.e7
	vmov  %v01.e0, %v02.e7
}

// A single-lane instruction names both lanes, as rsp.inc requires. Only the encoder
// sees a missing one, so each is its own template.
@(rsp)
no_destination_lane :: asm() {
	vrcp  %v01, %v02.e0 // ERROR: 'vrcp' operand-0 %v01 needs a destination lane, .e0-.e7
}

@(rsp)
no_source_lane :: asm() {
	vrcph %v01.e0, %v02 // ERROR: 'vrcph' operand-1 %v02 needs a source lane, .e0-.e7
}

@(rsp)
no_vmov_lane :: asm() {
	vmov  %v01, %v02.e3 // ERROR: 'vmov' operand-0 %v01 needs its destination lane, .e0
}

// cfc2 and ctc2 take a COP2 control register by name, and nothing else does.
@(rsp)
control :: asm(p: ^u32) [p = %s1] {
	cfc2  %t0, %t1 // ERROR: 'cfc2' operand-1 must be a named COP2 control register, got %t1
	ctc2  %t0, %v01 // ERROR: 'ctc2' operand-1 must be a named COP2 control register, got %v01
	ctc2  %t0, p // ERROR: 'ctc2' operand-1 must be a named COP2 control register, got the parameter 'p'
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
	ltv   %v01, [p + 8] // ERROR: 'ltv' operand-1 memory displacement 8 is not a multiple of the 16-byte access size
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
