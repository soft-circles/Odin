package overlay

@(rspq_command_words=4, rspq_scratch_bytes=16)
command :: asm() {
	li %r2, 30
	beq %a1, %a2, .equal
	li %r3, 1
	j .store
	addu %r2, %r2, %r3
.equal:
	li %r2, 40
	addu %r2, %r2, %r3
.store:
	la %s4, rspq_scratch
	sw %r2, [%s4]
	sw %zero, [%s4 + 4]
	sw %zero, [%s4 + 8]
	sw %zero, [%s4 + 12]
	move %s0, %a3
	li %t0, 15
	j DMAOut
	li %t1, 0
}
