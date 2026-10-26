// Odin64's rsp_jobs/transform Microcode body (tests/n64/rsp_jobs/project/microcode/transform/body.odin)
// written out flat as an @(rsp) template. The pins are the job register convention.
package transform

Args :: struct {
	a, c: i16,
	b, d: i16,
	tx, ty: i16,
}

Point :: struct {
	x, y: i16,
}

@(rsp)
transform :: asm(args: ^Args, input: ^Point, output: ^Point, count: i32, scratch: ^[20]u32) [args = %s6, input = %s1, output = %s2, count = %s3, scratch = %s7] {
	lh    %t0, [args + (offset_of(Args, a))]
	lh    %t1, [args + (offset_of(Args, c))]
	sra   %t4, %t0, 8
	sll   %t4, %t4, 16
	sra   %t5, %t1, 8
	andi  %t5, %t5, 0xffff
	or    %t4, %t4, %t5
	sw    %t4, [scratch + 0]
	sw    %t4, [scratch + 4]
	sw    %t4, [scratch + 8]
	sw    %t4, [scratch + 12]
	andi  %t4, %t0, 0xff
	sll   %t4, %t4, 24
	andi  %t5, %t1, 0xff
	sll   %t5, %t5, 8
	or    %t4, %t4, %t5
	sw    %t4, [scratch + 16]
	sw    %t4, [scratch + 20]
	sw    %t4, [scratch + 24]
	sw    %t4, [scratch + 28]
	lh    %t0, [args + (offset_of(Args, b))]
	lh    %t1, [args + (offset_of(Args, d))]
	sra   %t4, %t0, 8
	sll   %t4, %t4, 16
	sra   %t5, %t1, 8
	andi  %t5, %t5, 0xffff
	or    %t4, %t4, %t5
	sw    %t4, [scratch + 32]
	sw    %t4, [scratch + 36]
	sw    %t4, [scratch + 40]
	sw    %t4, [scratch + 44]
	andi  %t4, %t0, 0xff
	sll   %t4, %t4, 24
	andi  %t5, %t1, 0xff
	sll   %t5, %t5, 8
	or    %t4, %t4, %t5
	sw    %t4, [scratch + 48]
	sw    %t4, [scratch + 52]
	sw    %t4, [scratch + 56]
	sw    %t4, [scratch + 60]
	lw    %t4, [args + (offset_of(Args, tx))]
	sw    %t4, [scratch + 64]
	sw    %t4, [scratch + 68]
	sw    %t4, [scratch + 72]
	sw    %t4, [scratch + 76]
	lqv   %v10.e0, [scratch + 0]
	lqv   %v11.e0, [scratch + 16]
	lqv   %v12.e0, [scratch + 32]
	lqv   %v13.e0, [scratch + 48]
	lqv   %v14.e0, [scratch + 64]
.loop:
	lqv   %v01.e0, [input + 0]
	vmudn %v02, %v11, %v01.q0
	vmadn %v02, %v13, %v01.q1
	vmadh %v02, %v10, %v01.q0
	vmadh %v02, %v12, %v01.q1
	vmadh %v02, %v14, %v30.e7
	sqv   %v02.e0, [output + 0]
	addiu count, count, -4
	addiu input, input, 16
	bgtz  count, .loop
	addiu output, output, 16
}
