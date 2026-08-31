#include <stdint.h>

struct S3 { uint8_t a, b, c; };
struct S5 { uint8_t a, b, c, d, e; };
struct S12 { uint32_t a, b, c; };
struct Big { uint32_t a, b, c, d, e, f; };
struct SF { float a, b; };
union U8 { uint64_t q; uint32_t w[2]; uint8_t b[8]; };

int32_t o_i32_4(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
	return e;
}

int64_t o_i64_4(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e) {
	return e;
}

void *o_ptr_4(void *a, void *b, void *c, void *d, void *e) {
	return e;
}

uint8_t *o_ptr_add(uint8_t *p, uint32_t n) { return p + n; }

double o_f64_2(double a, double b, double c) {
	return c;
}

int32_t o_mixed_4(int32_t a, float b, int32_t c, double d, int32_t e) {
	return e;
}

uint8_t o_s3_c(struct S3 s) { return s.c; }
uint8_t o_s3_stack_c(int32_t a, int32_t b, int32_t c, int32_t d, struct S3 s) { return s.c; }
uint8_t o_s5_e(struct S5 s) { return s.e; }
uint32_t o_s12_c(struct S12 s) { return s.c; }
uint32_t o_u8_w0(union U8 u) { return u.w[0]; }
float o_sf_b(struct SF s) { return s.b; }
uint32_t o_big_f(struct Big s) { return s.f; }
int32_t o_big_after(struct Big s, int32_t next) { return next; }

struct S3 o_ret_s3(uint32_t x) {
	struct S3 r = {(uint8_t)x, (uint8_t)x, (uint8_t)x};
	return r;
}

struct S12 o_ret_s12(uint32_t x) {
	struct S12 r = {x, x, x};
	return r;
}

struct Big o_ret_big(uint32_t x) {
	struct Big r = {x, x, x, x, x, x};
	return r;
}

union U8 o_ret_u8(uint64_t x) {
	union U8 r;
	r.q = x;
	return r;
}

struct SF o_ret_sf(float x) {
	struct SF r = {x, x};
	return r;
}

extern int64_t sink(int32_t, ...);
extern int64_t sink_named(int32_t, double, ...);
extern int64_t sink_agg(int32_t, ...);
extern int64_t sink_i32(int32_t);
extern int64_t sink_u32(uint32_t);

int64_t o_call_var(int32_t n, double a, int32_t b, double c, int64_t d) {
	return sink(n, a, b, c, d);
}

int64_t o_call_named(int32_t a, double b, double c, int32_t d) {
	return sink_named(a, b, c, d);
}

int64_t o_call_var_s3(int32_t n, struct S3 s, int32_t next) {
	return sink_agg(n, s, next);
}

int64_t o_call_var_big(int32_t n, struct Big s, int32_t next) {
	return sink_agg(n, s, next);
}

int64_t o_call_var_u8(int32_t n, union U8 u, int32_t next) {
	return sink_agg(n, u, next);
}

int64_t o_call_i32_cast(int64_t x) { return sink_i32((int32_t)x); }
int64_t o_call_u32_cast(uint64_t x) { return sink_u32((uint32_t)x); }
