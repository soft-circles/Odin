package overlay

@(rspq_command_words=4, rspq_scratch_bytes=16)
command :: asm() {
	addu %r2, %a1, %a2
	xor %r3, %a1, %a2
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
