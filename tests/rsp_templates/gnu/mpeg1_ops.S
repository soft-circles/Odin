# GNU as's words for valid.mpeg1_ops: MPEG-1's VU ops through libdragon's rsp.inc,
# whose vrndp16 and vrndn16 set vs to 1 and whose vmacq takes only vd. The rounding
# ops name their element as `vt, e(N)`, as rsp_mpeg1.S does: rsp.inc's `vt.eN` form
# of them encodes element 0 whatever N is.
# test_rsp_templates.py assembles it with -march=mips1 -mabi=32 (N64_RSPASFLAGS)
# and -I $N64_INST/mips64-elf/include (where libdragon installs rsp.inc) when libdragon's toolchain is found.
#include <rsp.inc>
	.set noreorder
	.text
mpeg1_ops:
	vmulq   $v00, $v00, $v01
	vmulq   $v07, $v18, $v30.e5
	vmacq   $v00
	vmacq   $v31
	vrndp16 $v00, $v29, e(0)
	vrndp   $v12, $v03, e(1q)
	vrndp   $v14, $v27, e(1h)
	vrndn16 $v00, $v29, e(1)
	vrndn16 $v00, $v29, e(7)
	vrndn   $v25, $v14
