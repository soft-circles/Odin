package overlay

@(rspq_command_words=4, rspq_scratch_bytes=16)
command :: asm() {
	addu %t3, %a1, %a2
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
	mfc2 %t4, %v03.e0
	nop
	nop
	andi %t4, %t4, 0xffff
	la %s4, rspq_scratch
	sw %t3, [%s4]
	sw %t4, [%s4 + 4]
	sw %zero, [%s4 + 8]
	sw %zero, [%s4 + 12]
	addu %s0, %a3, %zero
	ori %t1, %zero, 0
	j DMAOut
	ori %t0, %zero, 15
}
