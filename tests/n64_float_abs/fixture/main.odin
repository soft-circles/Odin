package main

@(export) abs_f16   :: proc "c" (x: f16)   -> f16   { return abs(x) }
@(export) abs_f16le :: proc "c" (x: f16le) -> f16le { return abs(x) }
@(export) abs_f16be :: proc "c" (x: f16be) -> f16be { return abs(x) }
@(export) abs_f32   :: proc "c" (x: f32)   -> f32   { return abs(x) }
@(export) abs_f32le :: proc "c" (x: f32le) -> f32le { return abs(x) }
@(export) abs_f32be :: proc "c" (x: f32be) -> f32be { return abs(x) }
@(export) abs_f64   :: proc "c" (x: f64)   -> f64   { return abs(x) }
@(export) abs_f64le :: proc "c" (x: f64le) -> f64le { return abs(x) }
@(export) abs_f64be :: proc "c" (x: f64be) -> f64be { return abs(x) }

check_abs :: proc($F: typeid, $U: typeid, $T: typeid, patterns: []U) {
	for bits in patterns {
		value := T(transmute(F)bits)
		actual := F(abs(value))
		mask := ~(U(1) << (size_of(U)*8-1))
		assert(transmute(U)actual == bits & mask)
	}
}

main :: proc() {
	// Negative values, signed zero, positive mantissa bit 7, and infinities.
	p16 := [?]u16{0xc000,0xb800,0x8000,0,0x3800,0x3cff,0x7c00,0xfc00}
	p32 := [?]u32{0xc0000000,0xbf000000,0x80000000,0,0x3f000000,0x3f8000ff,0x7f800000,0xff800000}
	p64 := [?]u64{0xc000000000000000,0xbfe0000000000000,0x8000000000000000,0,0x3fe0000000000000,0x3ff00000000000ff,0x7ff0000000000000,0xfff0000000000000}
	check_abs(f16,u16,f16,p16[:]); check_abs(f16,u16,f16le,p16[:]); check_abs(f16,u16,f16be,p16[:])
	check_abs(f32,u32,f32,p32[:]); check_abs(f32,u32,f32le,p32[:]); check_abs(f32,u32,f32be,p32[:])
	check_abs(f64,u64,f64,p64[:]); check_abs(f64,u64,f64le,p64[:]); check_abs(f64,u64,f64be,p64[:])
}
