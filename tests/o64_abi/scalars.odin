package o64_abi

S3 :: struct {a, b, c: u8}
S5 :: struct {a, b, c, d, e: u8}
S12 :: struct {a, b, c: u32}
Big :: struct {a, b, c, d, e, f: u32}
SF :: struct {a, b: f32}
U8 :: struct #raw_union {
	q: u64,
	w: [2]u32,
	b: [8]u8,
}

foreign {
	sink :: proc "c" (n: i32, #c_vararg args: ..any) -> i64 ---
	sink_named :: proc "c" (a: i32, b: f64, #c_vararg args: ..any) -> i64 ---
	sink_agg :: proc "c" (n: i32, #c_vararg args: ..any) -> i64 ---
	sink_i32 :: proc "c" (x: i32) -> i64 ---
	sink_u32 :: proc "c" (x: u32) -> i64 ---
}

@(export)
o_i32_4 :: proc "c" (a, b, c, d, e: i32) -> i32 {
	return e
}

@(export)
o_i64_4 :: proc "c" (a, b, c, d, e: i64) -> i64 {
	return e
}

@(export)
o_ptr_4 :: proc "c" (a, b, c, d, e: rawptr) -> rawptr {
	return e
}

@(export)
o_ptr_add :: proc "c" (p: [^]u8, n: u32) -> [^]u8 {
	return ([^]u8)(uintptr(p) + uintptr(n))
}

@(export)
o_f64_2 :: proc "c" (a, b, c: f64) -> f64 {
	return c
}

@(export)
o_mixed_4 :: proc "c" (a: i32, b: f32, c: i32, d: f64, e: i32) -> i32 {
	return e
}

@(export)
o_s3_c :: proc "c" (s: S3) -> u8 {
	return s.c
}

@(export)
o_s3_stack_c :: proc "c" (a, b, c, d: i32, s: S3) -> u8 {
	return s.c
}

@(export)
o_s5_e :: proc "c" (s: S5) -> u8 {
	return s.e
}

@(export)
o_s12_c :: proc "c" (s: S12) -> u32 {
	return s.c
}

@(export)
o_u8_w0 :: proc "c" (u: U8) -> u32 {
	return u.w[0]
}

@(export)
o_sf_b :: proc "c" (s: SF) -> f32 {
	return s.b
}

@(export)
o_big_f :: proc "c" (s: Big) -> u32 {
	return s.f
}

@(export)
o_big_after :: proc "c" (s: Big, next: i32) -> i32 {
	return next
}

@(export)
o_ret_s3 :: proc "c" (x: u32) -> S3 {
	return {u8(x), u8(x), u8(x)}
}

@(export)
o_ret_s12 :: proc "c" (x: u32) -> S12 {
	return {x, x, x}
}

@(export)
o_ret_big :: proc "c" (x: u32) -> Big {
	return {x, x, x, x, x, x}
}

@(export)
o_ret_u8 :: proc "c" (x: u64) -> U8 {
	return U8{q = x}
}

@(export)
o_ret_sf :: proc "c" (x: f32) -> SF {
	return {x, x}
}

@(export)
o_call_var :: proc "c" (n: i32, a: f64, b: i32, c: f64, d: i64) -> i64 {
	return sink(n, a, b, c, d)
}

@(export)
o_call_named :: proc "c" (a: i32, b, c: f64, d: i32) -> i64 {
	return sink_named(a, b, c, d)
}

@(export)
o_call_var_s3 :: proc "c" (n: i32, s: S3, next: i32) -> i64 {
	return sink_agg(n, s, next)
}

@(export)
o_call_var_big :: proc "c" (n: i32, s: Big, next: i32) -> i64 {
	return sink_agg(n, s, next)
}

@(export)
o_call_var_u8 :: proc "c" (n: i32, u: U8, next: i32) -> i64 {
	return sink_agg(n, u, next)
}

@(export)
o_call_i32_cast :: proc "c" (x: i64) -> i64 {
	return sink_i32(i32(x))
}

@(export)
o_call_u32_cast :: proc "c" (x: u64) -> i64 {
	return sink_u32(u32(x))
}
