package overlay

@(rspq_command_words=4, rspq_scratch_bytes=16)
command :: asm() {
	addu %r2, %a1, %a2
	vxor %v01, %v00, %v00
	vxor %v02, %v00, %v00
	nop
	nop
	mtc2 %a1, %v01.e0
	mtc2 %a2, %v02.e0
	nop
	nop
	vxor %v03, %v01, %v02
	nop
	nop
	mfc2 %r3, %v03.e0
	nop
	nop
	andi %r3, %r3, 0xffff
	la %s4, rspq_scratch
	sw %r2, [%s4]
	sw %r3, [%s4 + 4]
	sw %zero, [%s4 + 8]
	sw %zero, [%s4 + 12]
	move %s0, %a3
	li %t0, 15
	li %t1, 0
	j DMAOut
	nop
}
