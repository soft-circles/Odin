package n64_asm_bad

cache_op_too_wide :: asm(p: rawptr) { cache 0x20, [p] }
unknown_cp0       :: asm() -> (r: u32) { mfc0 r, %c0_bogus }
gpr_as_cp0        :: asm(v: u32) { mtc0 v, %t0 }
unknown_mnemonic  :: asm(p: rawptr) { frobnicate [p] }
float_to_cp0      :: asm(v: f32) { mtc0 v, %c0_status }

@(export)
use :: proc "contextless" (p: rawptr) {
	cache_op_too_wide(p)
	_ = unknown_cp0()
	gpr_as_cp0(1)
	unknown_mnemonic(p)
	float_to_cp0(1)
}
