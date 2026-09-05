package overlay

@(rspq_command_words=4, rspq_scratch_bytes=16)
command :: asm() {
	addu %t3, %a1, %a2
	la %s4, rspq_scratch
	sw %t3, [%s4]
	sw %zero, [%s4 + 4]
	sw %zero, [%s4 + 8]
	sw %zero, [%s4 + 12]
	addu %s0, %a3, %zero
	ori %t1, %zero, 0
	j DMAOut
	ori %t0, %zero, 15
}
