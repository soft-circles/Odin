// Rejected @(rsp) templates. Each line that must report an error ends with
// `// ERROR: <message prefix>`; test_rsp_templates.py checks that every marked line
// gets its error, on every host target, and that no other line gets one.
package rsp_templates_bad

import "base:intrinsics"

Args :: struct {
	a: i16,
	b: i32,
}

@(rsp)
ok :: asm(args: ^Args, input: ^u32, count: i32) [args = %s6, input = %s1, count = %s3] {
	lh    %t0, [args + (offset_of(Args, a))]
	addiu input, input, 4
	addiu count, count, -1
}

// A body with no jumps, calls, CP0, DMA or break: the table has no such mnemonic.
@(rsp)
profile :: asm() {
	j     .end // ERROR: Unknown mnemonic for this target platform: j
	jr    %t0 // ERROR: Unknown mnemonic for this target platform: jr
	jal   .end // ERROR: Unknown mnemonic for this target platform: jal
	break // ERROR: Unknown mnemonic/prefix for this target platform: break
	mtc0  %t0, %t1 // ERROR: Unknown mnemonic for this target platform: mtc0
	addu  %t0, %t1, %t2 // ERROR: Unknown mnemonic for this target platform: addu
	bgtz  %t0, 4 // ERROR: 'bgtz' operands
.end:
}

// gp and ra hold rspq state; sp is not available at all.
@(rsp)
reserved :: asm() {
	addiu %gp, %gp, 4 // ERROR: 'addiu' writes %gp, which holds the rspq command pointer
	or    %ra, %t0, %t0 // ERROR: 'or' writes %ra, which holds the rspq return address
	addiu %t0, %sp, 4 // ERROR: Register %sp ($29) must not be used in RSP Microcode
	lw    %t0, [%sp + 4] // ERROR: Register %sp ($29) must not be used in RSP Microcode
	or    %t0, %gp, %ra
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
}

// Vector registers have two digits; %v0 and %v1 are GPRs.
@(rsp)
vector_names :: asm() {
	vmudn %v1, %v02, %v03 // ERROR: 'vmudn' operands
	lqv   %v0, [%s1] // ERROR: 'lqv' operands
	vmudn %v2, %v02, %v03 // ERROR: Register %v2 is not a register; vector registers are written with two digits
	or    %v0, %v1, %v1
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
}

@(rsp)
immediates :: asm() {
	addiu %t0, %t0, 32768 // ERROR: 'addiu' operand-2 immediate 32768 does not fit
	andi  %t0, %t0, -1 // ERROR: 'andi' operand-2 immediate -1 does not fit
	sll   %t0, %t0, 32 // ERROR: 'sll' operand-2 immediate 32 does not fit
	sra   %t0, %t0, -1 // ERROR: 'sra' operand-2 immediate -1 does not fit
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

@(rsp)
directive :: asm() {
	nop
	#byte 1 // ERROR: Asm directives are not supported in an @(rsp) template
}
DIRECTIVE :: intrinsics.rsp_template_code(directive) // ERROR: Invalid declaration value

// An @(rsp) template is Microcode, never a CPU callee.
group :: asm { ok } // ERROR: An @(rsp) template is RSP Microcode and cannot be in an asm template group

call_it :: proc() {
	ok(nil, nil, 0) // ERROR: 'ok' is an @(rsp) template: RSP Microcode, which CPU code cannot call
}

not_a_template :: proc() {}
NOT_A_TEMPLATE :: intrinsics.rsp_template_code(not_a_template) // ERROR: 'rsp_template_code' expects a declared @(rsp) asm template
