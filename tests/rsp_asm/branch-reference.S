/* Independent branch oracle: unequal inputs yield 30+1, equal inputs 40+1.
 * The BEQ slot defines the +1 on both paths; the J slot adds it on the
 * unequal path. The DMA tail slot supplies pitch before entering the helper.
 * Fixed words are reviewed ISA bitfields, not generated compiler output.
 */
#include <rsp_queue.inc>
.data
RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand reference_command, 16
RSPQ_EndOverlayHeader
RSPQ_EmptySavedState
.bss
.balign 16
rspq_scratch:
    .space 16
.text
.set noreorder
.set noat
.set nomacro
reference_command:
    .word 0x2402001e /* addiu r2,zero,30 */
    .word 0x10a60003 /* beq a1,a2, PC+4+3*4 */
    .word 0x24030001 /* slot: addiu r3,zero,1 */
    j .Lstore
    .word 0x00431021 /* slot: addu r2,r2,r3 */
    .word 0x24020028 /* taken: addiu r2,zero,40 */
    .word 0x00431021 /* addu r2,r2,r3 */
.Lstore:
    ori $20, $0, %lo(rspq_scratch)
    .word 0xae820000 /* sw r2,0(s4) */
    .word 0xae800004 /* sw zero,4(s4) */
    .word 0xae800008 /* sw zero,8(s4) */
    .word 0xae80000c /* sw zero,12(s4) */
    .word 0x00e08021 /* addu s0,a3,zero */
    .word 0x2408000f /* addiu t0,zero,15 */
    j DMAOut
    .word 0x24090000 /* slot: addiu t1,zero,0 */
