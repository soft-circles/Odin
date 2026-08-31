package o64_interop

S3 :: struct {a, b, c: u8}
S12 :: struct {a, b, c: u32}
Big :: struct {a, b, c, d, e, f: u32}
U8 :: struct #raw_union {
	q: u64,
	w: [2]u32,
	b: [8]u8,
}

foreign {
	c_i32 :: proc "c" (x: i32) -> i32 ---
	c_u32 :: proc "c" (x: u32) -> u32 ---
	c_i64 :: proc "c" (x: i64) -> i64 ---
	c_ptr_add :: proc "c" (p: [^]u8, n: u32) -> [^]u8 ---
	c_f32 :: proc "c" (x: f32) -> f32 ---
	c_f64 :: proc "c" (x: f64) -> f64 ---
	c_s3 :: proc "c" (s: S3) -> u8 ---
	c_s12 :: proc "c" (s: S12) -> u32 ---
	c_u8 :: proc "c" (u: U8) -> u32 ---
	c_stack :: proc "c" (a, b, c, d, e: i32) -> i32 ---
	c_ret_big :: proc "c" (a, b, c, d, e, f: u32) -> Big ---
	c_var_i32 :: proc "c" (tag: i32, #c_vararg args: ..any) -> i64 ---
}

@(export) odin_i32 :: proc "c" (x: i32) -> i32 { return x }
@(export) odin_u32 :: proc "c" (x: u32) -> u32 { return x }
@(export) odin_i64 :: proc "c" (x: i64) -> i64 { return x }
@(export) odin_ptr_add :: proc "c" (p: [^]u8, n: u32) -> [^]u8 {
	return ([^]u8)(uintptr(p) + uintptr(n))
}
@(export) odin_f32 :: proc "c" (x: f32) -> f32 { return x }
@(export) odin_f64 :: proc "c" (x: f64) -> f64 { return x }
@(export) odin_s3 :: proc "c" (s: S3) -> u8 { return s.c }
@(export) odin_s12 :: proc "c" (s: S12) -> u32 { return s.c }
@(export) odin_u8 :: proc "c" (u: U8) -> u32 { return u.w[0] }
@(export) odin_stack :: proc "c" (a, b, c, d, e: i32) -> i32 { return e }
// Distinct caller-supplied fields keep this probe about sret transport only.
@(export) odin_ret_big :: proc "c" (a, b, c, d, e, f: u32) -> Big {
	return {a, b, c, d, e, f}
}

@(export) odin_to_c_i32 :: proc "c" (x: i32) -> i32 { return c_i32(x) }
@(export) odin_to_c_u32 :: proc "c" (x: u32) -> u32 { return c_u32(x) }
@(export) odin_to_c_i64 :: proc "c" (x: i64) -> i64 { return c_i64(x) }
@(export) odin_to_c_ptr_add :: proc "c" (p: [^]u8, n: u32) -> [^]u8 {
	return c_ptr_add(p, n)
}
@(export) odin_to_c_f32 :: proc "c" (x: f32) -> f32 { return c_f32(x) }
@(export) odin_to_c_f64 :: proc "c" (x: f64) -> f64 { return c_f64(x) }
@(export) odin_to_c_s3 :: proc "c" (s: S3) -> u8 { return c_s3(s) }
@(export) odin_to_c_s12 :: proc "c" (s: S12) -> u32 { return c_s12(s) }
@(export) odin_to_c_u8 :: proc "c" (u: U8) -> u32 { return c_u8(u) }
@(export) odin_to_c_stack :: proc "c" (a, b, c, d, e: i32) -> i32 {
	return c_stack(a, b, c, d, e)
}
@(export) odin_to_c_ret_big :: proc "c" (a, b, c, d, e, f: u32) -> u32 {
	return c_ret_big(a, b, c, d, e, f).f
}
@(export) odin_to_c_var_i32 :: proc "c" (tag, x: i32) -> i64 {
	return c_var_i32(tag, x)
}
