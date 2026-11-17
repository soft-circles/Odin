# GNU as's words for valid.single_lane: each single-lane op through libdragon's
# rsp.inc, whose two-operand form writes the destination lane in vs as 8|lane.
# test_rsp_templates.py assembles it with -march=mips1 -mabi=32 (N64_RSPASFLAGS)
# and -I $N64_INST/mips64-elf/include (where libdragon installs rsp.inc) when libdragon's toolchain is found.
#include <rsp.inc>
	.set noreorder
	.text
single_lane:
	vrcp  $v01.e0, $v17.e7
	vrcp  $v01.e3, $v17.e5
	vrcpl $v06.e1, $v22.e6
	vrcph $v11.e7, $v27.e5
	vrsq  $v16.e3, $v00.e4
	vrsql $v21.e4, $v05.e3
	vrsqh $v26.e5, $v10.e2
	vmov  $v30.e0, $v01.e7
	vmov  $v31.e7, $v15.e2
