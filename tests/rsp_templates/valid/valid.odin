// @(rsp) templates every build target must accept under -vet -strict-style:
// test_rsp_templates.py checks this package for several hosts, the n64 and
// targets with no CPU asm table at all.
package rsp_templates_valid

import "base:intrinsics"
import "base:runtime"

import "transform"

// Through a package selector, at file scope
TRANSFORM :: intrinsics.rsp_template_code(transform.transform)

// The output pinned as the exit contract; a scratch register by name.
@(rsp)
fill :: asm(output: ^[4]u32, value: u32) -> (end: u32) [output = %s2, value = %a0, end = %s3, word: u32 = %t2] {
	or    word, value, %zero
	sw    word, [output + 0]
	sw    word, [output + 4]
	sw    word, [output + 8]
	sw    word, [output + 12]
	addiu end, output, 16
}

// A call to another template, here and through a package selector
@(rsp)
fill_twice :: asm() {
	jal   (fill)
	ori   %a0, %zero, 1
	jal   (transform.transform)
	nop
}

FILL_ADDRESS :: intrinsics.rsp_template_address(fill)

// Vector parameters: named bare as a whole register, or spelled as a register to take
// a lane or a VU element (`%colour.e2`), like any other pinned name.
@(rsp)
shade :: asm(colour: #simd[8]i16, scale: #simd[8]i16, output: ^[8]i16) -> (lane: u32) [colour = %v05, scale = %v06, output = %s4, lane = %t1] {
	vmudn colour, colour, %scale.e3
	vaddc %colour, colour, scale
	sqv   colour, [%output]
	ssv   %colour.e2, [output + 16]
	mfc2  lane, %colour.e1
	mtc2  %lane, %scale.e4
	lqv   scale, [output + 16]
}

@(export)
fill_twice_code :: proc "contextless" () -> (code: []intrinsics.Rsp_Code_Line, address: u32, pins: []intrinsics.Rsp_Pin) {
	return intrinsics.rsp_template_code(fill_twice), FILL_ADDRESS, intrinsics.rsp_template_pins(fill)
}

@(export)
fill_code :: proc "contextless" () -> []intrinsics.Rsp_Code_Line {
	return intrinsics.rsp_template_code(fill)
}

// Where the template is declared, for the generator's errors
@(export)
fill_location :: proc "contextless" () -> runtime.Source_Code_Location {
	return intrinsics.rsp_template_location(fill)
}

@(export)
transform_words :: proc "contextless" () -> (count: int, first: u32, line: i32) {
	code := TRANSFORM
	return len(code), code[0].word, code[0].line
}

// Whole Microcode another one reaches by its labels, as an rdpq overlay reaches rspq's
// kernel: cross_overlay below, whose words gnu/cross_template.S gives
@(rsp, rsp_origin = 0xA4001000)
whole_kernel :: asm() -> ! {
	nop
.loop:
	nop
	nop
.tail:
	jr    %ra
	nop
}

// Reaches whole_kernel by its labels as '(t).label', its start as '(t)', and constant
// addresses; gnu/cross_template.S is the same code for GNU as (golden.odin's CROSS_GNU)
CROSS_ABS :: 0xA4001008

@(rsp, rsp_origin = 0xA4001100)
cross_overlay :: asm() -> ! {
	beq   %at, %v0, (whole_kernel).loop
	nop
	bne   %zero, %v1, (whole_kernel).tail
	nop
	bgez  %a0, (whole_kernel).loop
	nop
	blez  %a1, (whole_kernel).tail
	nop
	bgtz  %a3, (whole_kernel)
	nop
	bltz  %a2, CROSS_ABS
	nop
	jal   (whole_kernel).tail
	nop
	jal   0xA4001004
	nop
	j     0x1040
	nop
.back:
	j     (whole_kernel)
	nop
.last:
	j     (whole_kernel).loop
	nop
}

@(export)
cross_overlay_code :: proc "contextless" () -> []intrinsics.Rsp_Code_Line {
	return intrinsics.rsp_template_code(cross_overlay)
}

// Each single-lane op, whose destination lane is 8|lane in vs, as rsp.inc writes it;
// gnu/single_lane.S is the same code for GNU as (golden.odin's SINGLE_LANE_GNU)
@(rsp)
single_lane :: asm() {
	vrcp  %v01.e0, %v17.e7
	vrcp  %v01.e3, %v17.e5
	vrcpl %v06.e1, %v22.e6
	vrcph %v11.e7, %v27.e5
	vrsq  %v16.e3, %v00.e4
	vrsql %v21.e4, %v05.e3
	vrsqh %v26.e5, %v10.e2
	vmov  %v30.e0, %v01.e7
	vmov  %v31.e7, %v15.e2
}

@(export)
single_lane_code :: proc "contextless" () -> []intrinsics.Rsp_Code_Line {
	return intrinsics.rsp_template_code(single_lane)
}

// MPEG-1's VU ops: vmulq, vmacq, which takes only vd, and the rounding ops, whose 16
// forms set vs to 1; gnu/mpeg1_ops.S is the same code for GNU as (golden.odin's
// MPEG1_OPS_GNU)
@(rsp)
mpeg1_ops :: asm() {
	vmulq   %v00, %v00, %v01
	vmulq   %v07, %v18, %v30.e5
	vmacq   %v00
	vmacq   %v31
	vrndp16 %v00, %v29.e0
	vrndp   %v12, %v03.q1
	vrndp   %v14, %v27.h1
	vrndn16 %v00, %v29.e1
	vrndn16 %v00, %v29.e7
	vrndn   %v25, %v14
}

@(export)
mpeg1_ops_code :: proc "contextless" () -> []intrinsics.Rsp_Code_Line {
	return intrinsics.rsp_template_code(mpeg1_ops)
}
