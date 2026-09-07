/* Minimal overlay using the pinned SDK's documented queue wrapper.
 * Independent MIPS encoding: JR has opcode=0, rs=31, function=8;
 * SLL zero,zero,0 is the no-op delay slot. No compiler emitter is used.
 */
#include <rsp_queue.inc>
.data
RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand reference_command, REFERENCE_WORDS * 4
RSPQ_EndOverlayHeader
RSPQ_EmptySavedState
.text
reference_command:
    .word 0x03e00008
    .word 0x00000000
