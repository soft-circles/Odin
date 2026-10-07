package n64_asm_bad_features

// Every mnemonic (and the mfc0/mtc0 select form) that the VR4300 lacks, one
// template per line named g_<mnemonic>. Each must be rejected with the LLVM
// feature that would enable it. HI/LO accumulators are fed first so the
// feature check, not the HI/LO read check, is what fires.

g_ext     :: asm(a: u32) -> (r: u32) { ext r, a, 0, 4 }
g_ins     :: asm(a: u32) -> (r: u32) { move r, a; ins r, a, 4, 4 }
g_dext    :: asm(a: u64) -> (r: u64) { dext r, a, 0, 4 }
g_dins    :: asm(a: u64) -> (r: u64) { move r, a; dins r, a, 4, 4 }
g_rotr    :: asm(a: u32) -> (r: u32) { rotr r, a, 3 }
g_rotrv   :: asm(a: u32, b: u32) -> (r: u32) { rotrv r, a, b }
g_drotr   :: asm(a: u64) -> (r: u64) { drotr r, a, 3 }
g_drotr32 :: asm(a: u64) -> (r: u64) { drotr32 r, a, 3 }
g_drotrv  :: asm(a: u64, b: u64) -> (r: u64) { drotrv r, a, b }
g_seb     :: asm(a: u32) -> (r: u32) { seb r, a }
g_seh     :: asm(a: u32) -> (r: u32) { seh r, a }
g_wsbh    :: asm(a: u32) -> (r: u32) { wsbh r, a }
g_dsbh    :: asm(a: u64) -> (r: u64) { dsbh r, a }
g_dshd    :: asm(a: u64) -> (r: u64) { dshd r, a }
g_clz     :: asm(a: u32) -> (r: u32) { clz r, a }
g_clo     :: asm(a: u32) -> (r: u32) { clo r, a }
g_dclz    :: asm(a: u64) -> (r: u64) { dclz r, a }
g_dclo    :: asm(a: u64) -> (r: u64) { dclo r, a }
g_mul     :: asm(a: u32, b: u32) -> (r: u32) { mul r, a, b }
g_madd    :: asm(a: u32, b: u32) { mthi a; mtlo b; madd a, b }
g_maddu   :: asm(a: u32, b: u32) { mthi a; mtlo b; maddu a, b }
g_msub    :: asm(a: u32, b: u32) { mthi a; mtlo b; msub a, b }
g_msubu   :: asm(a: u32, b: u32) { mthi a; mtlo b; msubu a, b }
g_movz    :: asm(a: u64, c: u64) -> (r: u64) { move r, a; movz r, a, c }
g_movn    :: asm(a: u64, c: u64) -> (r: u64) { move r, a; movn r, a, c }
g_movz_s  :: asm(a: f32, c: u64) -> (r: f32) { mov_s r, a; movz_s r, a, c }
g_movz_d  :: asm(a: f64, c: u64) -> (r: f64) { mov_d r, a; movz_d r, a, c }
g_movn_s  :: asm(a: f32, c: u64) -> (r: f32) { mov_s r, a; movn_s r, a, c }
g_movn_d  :: asm(a: f64, c: u64) -> (r: f64) { mov_d r, a; movn_d r, a, c }
g_pref    :: asm(p: rawptr) { pref 0, [p] }
g_synci   :: asm(p: rawptr) { synci [p] }
g_ehb     :: asm() { ehb }
g_wait    :: asm() { wait }
g_di      :: asm() { di }
g_ei      :: asm() { ei }
g_rdhwr   :: asm() -> (r: u64) { rdhwr r, 29 }
g_jr_hb   :: asm(a: rawptr) -> ! { jr_hb a }
g_jalr_hb :: asm(a: rawptr) { jalr_hb a }
g_mfhc1   :: asm(d: f64) -> (r: u32) { mfhc1 r, d }
g_mthc1   :: asm(a: u32, d: f64) -> (r: f64) { mov_d r, d; mthc1 a, r }
g_madd_s  :: asm(a: f32, b: f32, c: f32) -> (r: f32) { madd_s r, a, b, c }
g_madd_d  :: asm(a: f64, b: f64, c: f64) -> (r: f64) { madd_d r, a, b, c }
g_msub_s  :: asm(a: f32, b: f32, c: f32) -> (r: f32) { msub_s r, a, b, c }
g_msub_d  :: asm(a: f64, b: f64, c: f64) -> (r: f64) { msub_d r, a, b, c }
g_nmadd_s :: asm(a: f32, b: f32, c: f32) -> (r: f32) { nmadd_s r, a, b, c }
g_nmadd_d :: asm(a: f64, b: f64, c: f64) -> (r: f64) { nmadd_d r, a, b, c }
g_nmsub_s :: asm(a: f32, b: f32, c: f32) -> (r: f32) { nmsub_s r, a, b, c }
g_nmsub_d :: asm(a: f64, b: f64, c: f64) -> (r: f64) { nmsub_d r, a, b, c }
g_recip_s :: asm(a: f32) -> (r: f32) { recip_s r, a }
g_recip_d :: asm(a: f64) -> (r: f64) { recip_d r, a }
g_rsqrt_s :: asm(a: f32) -> (r: f32) { rsqrt_s r, a }
g_rsqrt_d :: asm(a: f64) -> (r: f64) { rsqrt_d r, a }
g_mfc0_select :: asm() -> (r: u32) { mfc0 r, %c0_status, 1 }
g_mtc0_select :: asm(a: u32) { mtc0 a, %c0_status, 1 }

@(export)
use :: proc "contextless" (x: u32, w: u64, f: f32, d: f64, p: rawptr) {
	_ = g_ext(x); _ = g_ins(x); _ = g_dext(w); _ = g_dins(w); _ = g_rotr(x); _ = g_rotrv(x, x)
	_ = g_drotr(w); _ = g_drotr32(w); _ = g_drotrv(w, w); _ = g_seb(x); _ = g_seh(x); _ = g_wsbh(x)
	_ = g_dsbh(w); _ = g_dshd(w); _ = g_clz(x); _ = g_clo(x); _ = g_dclz(w); _ = g_dclo(w); _ = g_mul(x, x)
	g_madd(x, x); g_maddu(x, x); g_msub(x, x); g_msubu(x, x)
	_ = g_movz(w, w); _ = g_movn(w, w); _ = g_movz_s(f, w); _ = g_movz_d(d, w); _ = g_movn_s(f, w); _ = g_movn_d(d, w)
	g_pref(p); g_synci(p); g_ehb(); g_wait(); g_di(); g_ei(); _ = g_rdhwr(); g_jalr_hb(p)
	_ = g_mfhc1(d); _ = g_mthc1(x, d)
	_ = g_madd_s(f, f, f); _ = g_madd_d(d, d, d); _ = g_msub_s(f, f, f); _ = g_msub_d(d, d, d)
	_ = g_nmadd_s(f, f, f); _ = g_nmadd_d(d, d, d); _ = g_nmsub_s(f, f, f); _ = g_nmsub_d(d, d, d)
	_ = g_recip_s(f); _ = g_recip_d(d); _ = g_rsqrt_s(f); _ = g_rsqrt_d(d)
	_ = g_mfc0_select(); g_mtc0_select(x)
	g_jr_hb(p)
}
