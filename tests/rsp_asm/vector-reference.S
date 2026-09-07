/* Independent scalar-add/vector-XOR command oracle. Real MIPS words are derived from the
 * ISA bitfields; only the local scratch and DMA jump need SDK relocations.
 * No output from the Odin emitter is included here.
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
reference_command:
    .word 0x00a61021 /* addu r2,a1,a2 */
    .word 0x4a00006c /* vxor v01,v00,v00: COP2=0x12, vector=1, vt=0, vs=0, vd=1, fn=0x2c */
    .word 0x4a0000ac /* vxor v02,v00,v00: vd=2 */
    .word 0x00000000
    .word 0x00000000
    .word 0x48850800 /* mtc2 a1,v01.e0: COP2=0x12, rs=4, rt=5, vs=1, byte element=0 */
    .word 0x48861000 /* mtc2 a2,v02.e0: rt=6, vs=2 */
    .word 0x00000000
    .word 0x00000000
    .word 0x4a0208ec /* vxor v03,v01,v02: vt=2, vs=1, vd=3 */
    .word 0x00000000
    .word 0x00000000
    .word 0x48031800 /* mfc2 r3,v03.e0: rs=0, rt=3, vs=3, byte element=0 */
    .word 0x00000000
    .word 0x00000000
    .word 0x3063ffff /* andi r3,r3,65535 */
    ori $20, $0, %lo(rspq_scratch)
    .word 0xae820000 /* sw r2,0(s4) */
    .word 0xae830004 /* sw r3,4(s4) */
    .word 0xae800008 /* sw zero,8(s4) */
    .word 0xae80000c /* sw zero,12(s4) */
    .word 0x00e08021 /* addu s0,a3,zero */
    .word 0x2408000f /* addiu t0,zero,15 */
    .word 0x24090000 /* addiu t1,zero,0 */
    j DMAOut
    .word 0x00000000
