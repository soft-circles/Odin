// Operand widths at the asm boundary, and the HI/LO hazard padding, on the N64.
// Each exported proc is run by CodegenWidths in test_n64_asm.py (a small MIPS
// III interpreter) and its IR is checked; keep the two in sync.
package widths

// ---------------------------------------------------------------------------
// Inputs narrower than 32 bits reach the template extended (200 + 200 = 144 in
// a u8, -56 in an i8), pinned, unpinned or tied.
// ---------------------------------------------------------------------------

@(export) in_u8 :: proc "contextless" (a, b: u8) -> u32 {
	t :: asm(x: u8) -> (r: u32) { sltiu r, x, 200 }
	return t(a + b)
}
@(export) in_u8_pinned :: proc "contextless" (a, b: u8) -> u32 {
	t :: asm(x: u8) -> (r: u32) [x = %a2] { sltiu r, x, 200 }
	return t(a + b)
}
@(export) in_u8_tied :: proc "contextless" (a, b: u8) -> u32 {
	t :: asm(x: u8) -> (r: u32) [x -> r] { addiu r, r, 0 }
	return t(a + b)
}
@(export) in_u8_tied_pinned :: proc "contextless" (a, b: u8) -> u32 {
	t :: asm(x: u8) -> (r: u32) [x -> r = %a1] { addiu r, r, 0 }
	return t(a + b)
}
@(export) in_i8 :: proc "contextless" (a, b: i8) -> i32 {
	t :: asm(x: i8) -> (r: i32) { addiu r, x, 0 }
	return t(a + b)
}
@(export) in_i8_pinned :: proc "contextless" (a, b: i8) -> i32 {
	t :: asm(x: i8) -> (r: i32) [x = %a3] { addiu r, x, 0 }
	return t(a + b)
}
@(export) in_i16 :: proc "contextless" (a, b: i16) -> i32 {
	t :: asm(x: i16) -> (r: i32) { addiu r, x, 0 }
	return t(a + b)
}
@(export) in_bool :: proc "contextless" (a, b: u32) -> u32 {
	t :: asm(x: bool) -> (r: u32) { addiu r, x, 0 }
	return t(a == b)
}

// ---------------------------------------------------------------------------
// Narrow outputs come back truncated.
// ---------------------------------------------------------------------------

@(export) out_u8 :: proc "contextless" (a: u32) -> u32 {
	t :: asm(x: u32) -> (r: u8) { addiu r, x, 1 }
	return u32(t(a))
}
@(export) out_i16 :: proc "contextless" (a: u32) -> i32 {
	t :: asm(x: u32) -> (r: i16) { addiu r, x, 0 }
	return i32(t(a))
}
@(export) out_bool :: proc "contextless" (a: u32) -> u32 {
	t :: asm(x: u32) -> (r: bool) { sltiu r, x, 1 }
	return 7 if t(a) else 3
}

// ---------------------------------------------------------------------------
// Outputs narrower than 64 bits that an instruction may leave with an arbitrary
// upper half compare as their declared type.
// ---------------------------------------------------------------------------

@(export) lwu_eq :: proc "contextless" (p: rawptr) -> bool {
	t :: asm(p: rawptr) -> (r: u32) { lwu r, [p] }
	return t(p) == 0x80000000
}
@(export) ld_eq0 :: proc "contextless" (p: rawptr) -> bool {
	t :: asm(p: rawptr) -> (r: u32) { ld r, [p] }
	return t(p) == 0
}
@(export) ld_u8_eq0 :: proc "contextless" (p: rawptr) -> bool {
	t :: asm(p: rawptr) -> (r: u8) { ld r, [p] }
	return t(p) == 0
}
@(export) ld_ptr_nil :: proc "contextless" (p: rawptr) -> bool {
	t :: asm(p: rawptr) -> (r: rawptr) { ld r, [p] }
	return t(p) == nil
}
@(export) move_u64_eq0 :: proc "contextless" (x: u64) -> bool {
	t :: asm(x: u64) -> (r: u32) { move r, x }
	return t(x) == 0
}
@(export) dsll_eq :: proc "contextless" (a: u32) -> bool {
	t :: asm(x: u32) -> (r: u32) { dsll r, x, 16 }
	return t(a) == 0x80000000
}
@(export) dsll_tied_eq :: proc "contextless" (a: u32) -> bool {
	t :: asm(x: u32) -> (r: u32) [x -> r] { dsll r, r, 16 }
	return t(a) == 0x80000000
}
@(export) or_after_dsll_eq :: proc "contextless" (a: u32) -> bool {
	t :: asm(x: u32) -> (r: u32, s: u32) { dsll s, x, 16; or r, s, %zero }
	r, _ := t(a)
	return r == 0x80000000
}
@(export) dmfc0_eq0 :: proc "contextless" () -> bool {
	t :: asm() -> (r: u32) { dmfc0 r, %c0_epc }
	return t() == 0
}
@(export) mflo_after_dmultu_eq0 :: proc "contextless" (a, b: u64) -> bool {
	t :: asm(x: u64, y: u64) -> (r: u32) { dmultu x, y; mflo r }
	return t(a, b) == 0
}
@(export) lo_pin_after_dmultu_eq0 :: proc "contextless" (a, b: u64) -> bool {
	t :: asm(x: u64, y: u64) -> (r: u32) [r = %lo] { dmultu x, y }
	return t(a, b) == 0
}

// LLVM takes '{hi}'/'{lo}' as 32-bit registers, so 64-bit %hi/%lo operands are
// moved through a GPR inside the template (mtlo before the body, mflo after).
@(export) lo_u64_high_half :: proc "contextless" (a, b: u64) -> u64 {
	t :: asm(x: u64, y: u64) -> (r: u64) [r = %lo] { dmultu x, y }
	return t(a, b) >> 32
}
@(export) lo_in_u64 :: proc "contextless" (a: u64) -> u64 {
	t :: asm(x: u64) -> (r: u64) [x = %lo] { mflo r }
	return t(a)
}

// Writes that always leave a sign-extended word keep the plain i32.
@(export) word_writes :: proc "contextless" (p: rawptr, a, b: u32) -> u32 {
	t1 :: asm(p: rawptr) -> (r: u32) { lw r, [p] }
	t2 :: asm(x: u32) -> (r: u32) { move r, x }
	t3 :: asm(x: u32, y: u32) -> (r: u32) { mult x, y; mflo r }
	t4 :: asm(x: u32) -> (r: u32) { andi r, x, 0xFF; ori r, r, 1 }
	t5 :: asm() -> (r: u32) { mfc0 r, %c0_count }
	return t1(p) + t2(a) + t3(a, b) + t4(b) + t5()
}

// ---------------------------------------------------------------------------
// HI/LO hazard: two instructions between a compiler mflo and a template mult,
// and between a template mflo and a compiler mult.
// ---------------------------------------------------------------------------

@(export) mflo_then_tmpl_multu :: proc "contextless" (a, b, c: u32) -> u32 {
	t :: asm(x: u32, y: u32) -> (r: u32) { multu x, y; mflo r }
	x := a * b
	return t(x, c)
}
@(export) tmpl_mflo_then_mult :: proc "contextless" (a, b, c: u32) -> u32 {
	t :: asm(x: u32, y: u32) -> (r: u32) { multu x, y; mflo r }
	r := t(a, b)
	return r * c
}
@(export) hilo_far_from_edges :: proc "contextless" (a, b: u32) -> u32 {
	t :: asm(x: u32, y: u32) -> (r: u32) {
		addu r, x, y
		addu r, r, y
		multu r, y
		mflo r
		addiu r, r, 1
		addiu r, r, 1
	}
	return t(a, b)
}
@(export) hilo_after_branch :: proc "contextless" (a, b: u32) -> u32 {
	t :: asm(x: u32, y: u32) -> (r: u32) {
		bnez x, .l
	.l:
		multu x, y
		mflo r
		addiu r, r, 0
	}
	return t(a, b)
}
