# GNU as's words for golden.odin's cross_overlay, which reaches whole Microcode
# valid.whole_kernel by its labels and start, and constant addresses. As libdragon
# builds an overlay, the kernel and the overlay are one file linked at 0xA4001000
# (rsp.ld's .text): the kernel first, the overlay at 0x100. test_rsp_templates.py
# assembles it with -march=mips1 -mabi=32 (N64_RSPASFLAGS) when mips64-elf-as is found.
# CROSS_ABS, a constant address, is an absolute symbol the link defines (--defsym
# CROSS_ABS=0xA4001008): a branch to an absolute address is resolved by ld.
	.set noreorder
	.set noat
	.text
whole_kernel:
	nop
loop:
	nop
	nop
tail:
	jr    $ra
	nop

	.org 0x100
cross_overlay:
	beq   $at, $v0, loop
	nop
	bne   $zero, $v1, tail
	nop
	bgez  $a0, loop
	nop
	blez  $a1, tail
	nop
	bgtz  $a3, whole_kernel
	nop
	bltz  $a2, CROSS_ABS
	nop
	jal   tail
	nop
	jal   0xA4001004
	nop
	j     0x1040
	nop
back:
	j     whole_kernel
	nop
last:
	j     loop
	nop
