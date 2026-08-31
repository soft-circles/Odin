#include <stdarg.h>
#include <stdint.h>
#include <libdragon.h>

#define NOINLINE __attribute__((noinline))

struct S3 { uint8_t a, b, c; };
struct S12 { uint32_t a, b, c; };
struct Big { uint32_t a, b, c, d, e, f; };
union U8 { uint64_t q; uint32_t w[2]; uint8_t b[8]; };

#define DECLARE_SIDE(prefix) \
	int32_t prefix##_i32(int32_t); \
	uint32_t prefix##_u32(uint32_t); \
	int64_t prefix##_i64(int64_t); \
	uint8_t *prefix##_ptr_add(uint8_t *, uint32_t); \
	float prefix##_f32(float); \
	double prefix##_f64(double); \
	uint8_t prefix##_s3(struct S3); \
	uint32_t prefix##_s12(struct S12); \
	uint32_t prefix##_u8(union U8); \
	int32_t prefix##_stack(int32_t, int32_t, int32_t, int32_t, int32_t); \
	struct Big prefix##_ret_big(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)

DECLARE_SIDE(odin);

int32_t odin_to_c_i32(int32_t);
uint32_t odin_to_c_u32(uint32_t);
int64_t odin_to_c_i64(int64_t);
uint8_t *odin_to_c_ptr_add(uint8_t *, uint32_t);
float odin_to_c_f32(float);
double odin_to_c_f64(double);
uint8_t odin_to_c_s3(struct S3);
uint32_t odin_to_c_s12(struct S12);
uint32_t odin_to_c_u8(union U8);
int32_t odin_to_c_stack(int32_t, int32_t, int32_t, int32_t, int32_t);
uint32_t odin_to_c_ret_big(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int64_t odin_to_c_var_i32(int32_t, int32_t);

#define DEFINE_SIDE(prefix) \
	NOINLINE int32_t prefix##_i32(int32_t x) { return x; } \
	NOINLINE uint32_t prefix##_u32(uint32_t x) { return x; } \
	NOINLINE int64_t prefix##_i64(int64_t x) { return x; } \
	NOINLINE uint8_t *prefix##_ptr_add(uint8_t *p, uint32_t n) { return p + n; } \
	NOINLINE float prefix##_f32(float x) { return x; } \
	NOINLINE double prefix##_f64(double x) { return x; } \
	NOINLINE uint8_t prefix##_s3(struct S3 s) { return s.c; } \
	NOINLINE uint32_t prefix##_s12(struct S12 s) { return s.c; } \
	NOINLINE uint32_t prefix##_u8(union U8 u) { return u.w[0]; } \
	NOINLINE int32_t prefix##_stack(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) \
		{ (void)a; (void)b; (void)c; (void)d; return e; } \
	NOINLINE struct Big prefix##_ret_big( \
		uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f) \
		{ struct Big r = {a, b, c, d, e, f}; return r; }

DEFINE_SIDE(c)
DEFINE_SIDE(gcc)

NOINLINE int64_t c_var_i32(int32_t tag, ...) {
	va_list ap;
	va_start(ap, tag);
	uint32_t x = (uint32_t)va_arg(ap, int32_t);
	va_end(ap);
	return (int64_t)(((uint64_t)(uint32_t)tag << 32) | x);
}

#define FORWARD_PAIR(ret, name, params, args) \
	NOINLINE ret c_to_odin_##name params { return odin_##name args; } \
	NOINLINE ret c_to_gcc_##name params { return gcc_##name args; } \
	NOINLINE ret gcc_to_c_##name params { return c_##name args; }

FORWARD_PAIR(int32_t, i32, (int32_t x), (x))
FORWARD_PAIR(uint32_t, u32, (uint32_t x), (x))
FORWARD_PAIR(int64_t, i64, (int64_t x), (x))
FORWARD_PAIR(uint8_t *, ptr_add, (uint8_t *p, uint32_t n), (p, n))
FORWARD_PAIR(float, f32, (float x), (x))
FORWARD_PAIR(double, f64, (double x), (x))
FORWARD_PAIR(uint8_t, s3, (struct S3 s), (s))
FORWARD_PAIR(uint32_t, s12, (struct S12 s), (s))
FORWARD_PAIR(uint32_t, u8, (union U8 u), (u))
FORWARD_PAIR(int32_t, stack,
	(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e),
	(a, b, c, d, e))

NOINLINE uint32_t c_to_odin_ret_big(
	uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f) {
	return odin_ret_big(a, b, c, d, e, f).f;
}
NOINLINE uint32_t c_to_gcc_ret_big(
	uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f) {
	return gcc_ret_big(a, b, c, d, e, f).f;
}
NOINLINE uint32_t gcc_to_c_ret_big(
	uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f) {
	return c_ret_big(a, b, c, d, e, f).f;
}
NOINLINE int64_t gcc_to_c_var_i32(int32_t tag, int32_t x) { return c_var_i32(tag, x); }

/* Keep the GCC reference entry points live for run.py's linked differential. */
static volatile uintptr_t reference_sink;

static NOINLINE void keep_reference_entries(void) {
	struct S3 s3 = {0x81, 0xa5, 0xfe};
	struct S12 s12 = {0x11223344, 0x89abcdef, 0xfedcba98};
	union U8 u8 = {.q = 0x8123456789abcdefULL};
	uint32_t x = 0x81234567;
	uintptr_t sum = 0;

#define KEEP_REFERENCE(name, value) \
	sum += (uintptr_t)c_to_gcc_##name(value); \
	sum += (uintptr_t)gcc_to_c_##name(value)
	KEEP_REFERENCE(i32, (int32_t)x);
	KEEP_REFERENCE(u32, x);
	KEEP_REFERENCE(i64, (int64_t)0x8123456789abcdefULL);
	KEEP_REFERENCE(f32, -13.25f);
	KEEP_REFERENCE(f64, 9876.5);
	KEEP_REFERENCE(s3, s3);
	KEEP_REFERENCE(s12, s12);
	KEEP_REFERENCE(u8, u8);
#undef KEEP_REFERENCE

	sum += (uintptr_t)c_to_gcc_ptr_add((uint8_t *)0x80102030, 0x1234);
	sum += (uintptr_t)gcc_to_c_ptr_add((uint8_t *)0x80102030, 0x1234);
	sum += (uintptr_t)c_to_gcc_stack(1, 2, 3, 4, (int32_t)x);
	sum += (uintptr_t)gcc_to_c_stack(1, 2, 3, 4, (int32_t)x);
	sum += c_to_gcc_ret_big(0x11223344, 0x89abcdef, 0xfedcba98, 4, 5, x);
	sum += gcc_to_c_ret_big(0x11223344, 0x89abcdef, 0xfedcba98, 4, 5, x);
	sum += (uintptr_t)gcc_to_c_var_i32(0x13579bdf, (int32_t)x);
	reference_sink = sum;
}

int main(void) {
	struct S3 s3 = {0x81, 0xa5, 0xfe};
	struct S12 s12 = {0x11223344, 0x89abcdef, 0xfedcba98};
	union U8 u8 = {.q = 0x8123456789abcdefULL};
	uint32_t x = 0x81234567;
	int failures = 0;

	debug_init_emulog();

#define CHECK(label, condition) do { \
	if (!(condition)) { \
		debugf("FAIL: " label "\n"); \
		failures++; \
	} \
} while (0)

	CHECK("C -> Odin i32", c_to_odin_i32((int32_t)x) == (int32_t)x);
	CHECK("C -> Odin u32", c_to_odin_u32(x) == x);
	CHECK("C -> Odin i64", c_to_odin_i64((int64_t)0x8123456789abcdefULL) ==
		(int64_t)0x8123456789abcdefULL);
	CHECK("C -> Odin pointer", c_to_odin_ptr_add((uint8_t *)0x80102030, 0x1234) ==
		(uint8_t *)0x80103264);
	CHECK("C -> Odin f32", c_to_odin_f32(-13.25f) == -13.25f);
	CHECK("C -> Odin f64", c_to_odin_f64(9876.5) == 9876.5);
	CHECK("C -> Odin small struct", c_to_odin_s3(s3) == 0xfe);
	CHECK("C -> Odin multi-slot struct", c_to_odin_s12(s12) == 0xfedcba98);
	CHECK("C -> Odin raw union", c_to_odin_u8(u8) == 0x81234567);
	CHECK("C -> Odin stack arg", c_to_odin_stack(1, 2, 3, 4, (int32_t)x) ==
		(int32_t)x);
	CHECK("C -> Odin indirect return", c_to_odin_ret_big(
		0x11223344, 0x89abcdef, 0xfedcba98, 4, 5, x) == x);

	CHECK("Odin -> C i32", odin_to_c_i32((int32_t)x) == (int32_t)x);
	CHECK("Odin -> C u32", odin_to_c_u32(x) == x);
	CHECK("Odin -> C i64", odin_to_c_i64((int64_t)0x8123456789abcdefULL) ==
		(int64_t)0x8123456789abcdefULL);
	CHECK("Odin -> C pointer", odin_to_c_ptr_add((uint8_t *)0x80102030, 0x1234) ==
		(uint8_t *)0x80103264);
	CHECK("Odin -> C f32", odin_to_c_f32(-13.25f) == -13.25f);
	CHECK("Odin -> C f64", odin_to_c_f64(9876.5) == 9876.5);
	CHECK("Odin -> C small struct", odin_to_c_s3(s3) == 0xfe);
	CHECK("Odin -> C multi-slot struct", odin_to_c_s12(s12) == 0xfedcba98);
	CHECK("Odin -> C raw union", odin_to_c_u8(u8) == 0x81234567);
	CHECK("Odin -> C stack arg", odin_to_c_stack(1, 2, 3, 4, (int32_t)x) ==
		(int32_t)x);
	CHECK("Odin -> C indirect return", odin_to_c_ret_big(
		0x11223344, 0x89abcdef, 0xfedcba98, 4, 5, x) == x);
	CHECK("Odin -> variadic C", odin_to_c_var_i32(0x13579bdf, (int32_t)x) ==
		(int64_t)0x13579bdf81234567ULL);

#undef CHECK

	if (failures == 0)
		debugf("PASS: Odin O64 ABI 23/23\n");
	else
		debugf("FAIL: Odin O64 ABI\n");

	keep_reference_entries();
	for (;;) {}
}
