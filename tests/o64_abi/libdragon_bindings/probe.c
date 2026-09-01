#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <libdragon.h>

#define NOINLINE __attribute__((noinline))

#define RESOLUTION_DIGEST UINT32_C(0x5245534f)
#define BUTTONS_DIGEST UINT32_C(0x42544e53)
#define INPUTS_DIGEST UINT32_C(0x494e5054)
#define BOOL_FALSE_DIGEST UINT32_C(0x46414c53)
#define BOOL_TRUE_DIGEST UINT32_C(0x54525545)

_Static_assert(sizeof(resolution_t) == 20, "resolution_t size changed");
_Static_assert(_Alignof(resolution_t) == 4, "resolution_t alignment changed");
_Static_assert(offsetof(resolution_t, width) == 0, "resolution_t.width moved");
_Static_assert(offsetof(resolution_t, height) == 4, "resolution_t.height moved");
_Static_assert(offsetof(resolution_t, interlaced) == 8, "resolution_t.interlaced moved");
_Static_assert(offsetof(resolution_t, aspect_ratio) == 12, "resolution_t.aspect_ratio moved");
_Static_assert(offsetof(resolution_t, overscan_margin) == 16, "resolution_t.overscan_margin moved");

_Static_assert(sizeof(joypad_buttons_t) == 2, "joypad_buttons_t size changed");
_Static_assert(_Alignof(joypad_buttons_t) == 2, "joypad_buttons_t alignment changed");
_Static_assert(offsetof(joypad_buttons_t, raw) == 0, "joypad_buttons_t.raw moved");

_Static_assert(sizeof(joypad_inputs_t) == 8, "joypad_inputs_t size changed");
_Static_assert(_Alignof(joypad_inputs_t) == 1, "joypad_inputs_t alignment changed");
_Static_assert(offsetof(joypad_inputs_t, btn) == 0, "joypad_inputs_t.btn moved");
_Static_assert(offsetof(joypad_inputs_t, stick_x) == 2, "joypad_inputs_t.stick_x moved");
_Static_assert(offsetof(joypad_inputs_t, stick_y) == 3, "joypad_inputs_t.stick_y moved");
_Static_assert(offsetof(joypad_inputs_t, cstick_x) == 4, "joypad_inputs_t.cstick_x moved");
_Static_assert(offsetof(joypad_inputs_t, cstick_y) == 5, "joypad_inputs_t.cstick_y moved");
_Static_assert(offsetof(joypad_inputs_t, analog_l) == 6, "joypad_inputs_t.analog_l moved");
_Static_assert(offsetof(joypad_inputs_t, analog_r) == 7, "joypad_inputs_t.analog_r moved");

_Static_assert(sizeof(bool) == 1, "libdragon C _Bool size changed");
_Static_assert(_Alignof(bool) == 1, "libdragon C _Bool alignment changed");

uint32_t odin_resolution_digest(resolution_t value);
resolution_t odin_make_resolution(void);
uint32_t odin_buttons_digest(joypad_buttons_t value);
joypad_buttons_t odin_make_buttons(void);
uint32_t odin_inputs_digest(joypad_inputs_t value);
joypad_inputs_t odin_make_inputs(void);
uint32_t odin_bool_parameter(bool value);
bool odin_bool_result(uint32_t token);

uint32_t odin_to_c_resolution_parameter(void);
uint32_t odin_to_c_resolution_result(void);
uint32_t odin_to_c_buttons_parameter(void);
uint32_t odin_to_c_buttons_result(void);
uint32_t odin_to_c_inputs_parameter(void);
uint32_t odin_to_c_inputs_result(void);
uint32_t odin_to_c_bool_false_parameter(void);
uint32_t odin_to_c_bool_true_parameter(void);
bool odin_to_c_bool_false_result(void);
bool odin_to_c_bool_true_result(void);

bool odin_libdragon_debug_result(void);
bool odin_libdragon_console_bool_parameter(bool value);

static NOINLINE resolution_t reference_resolution(void) {
	resolution_t value = {
		.width = 319,
		.height = 241,
		.interlaced = INTERLACE_FULL,
		.aspect_ratio = -13.25f,
		.overscan_margin = 0.125f,
	};
	return value;
}

static bool resolution_matches(resolution_t value) {
	return value.width == 319 &&
		value.height == 241 &&
		value.interlaced == INTERLACE_FULL &&
		value.aspect_ratio == -13.25f &&
		value.overscan_margin == 0.125f;
}

NOINLINE uint32_t c_resolution_digest(resolution_t value) {
	return resolution_matches(value) ? RESOLUTION_DIGEST : 0;
}

NOINLINE resolution_t c_make_resolution(void) {
	return reference_resolution();
}

static NOINLINE joypad_buttons_t reference_buttons(void) {
	joypad_buttons_t value = {.raw = UINT16_C(0xa59c)};
	return value;
}

static bool buttons_match(joypad_buttons_t value) {
	return value.raw == UINT16_C(0xa59c);
}

NOINLINE uint32_t c_buttons_digest(joypad_buttons_t value) {
	return buttons_match(value) ? BUTTONS_DIGEST : 0;
}

NOINLINE joypad_buttons_t c_make_buttons(void) {
	return reference_buttons();
}

static NOINLINE joypad_inputs_t reference_inputs(void) {
	joypad_inputs_t value = {
		.btn = {.raw = UINT16_C(0x936a)},
		.stick_x = -101,
		.stick_y = 87,
		.cstick_x = -76,
		.cstick_y = 63,
		.analog_l = 201,
		.analog_r = 254,
	};
	return value;
}

static bool inputs_match(joypad_inputs_t value) {
	return value.btn.raw == UINT16_C(0x936a) &&
		value.stick_x == -101 &&
		value.stick_y == 87 &&
		value.cstick_x == -76 &&
		value.cstick_y == 63 &&
		value.analog_l == 201 &&
		value.analog_r == 254;
}

NOINLINE uint32_t c_inputs_digest(joypad_inputs_t value) {
	return inputs_match(value) ? INPUTS_DIGEST : 0;
}

NOINLINE joypad_inputs_t c_make_inputs(void) {
	return reference_inputs();
}

NOINLINE uint32_t c_bool_parameter(bool value) {
	return value ? BOOL_TRUE_DIGEST : BOOL_FALSE_DIGEST;
}

NOINLINE bool c_bool_result(uint32_t token) {
	return token == BOOL_TRUE_DIGEST;
}

static void record_check(FILE *debug_log, FILE *console, const char *label,
		bool passed, int *failures) {
	if (passed)
		return;
	fprintf(debug_log, "FAIL: Odin libdragon binding ABI: %s\n", label);
	fprintf(console, "FAIL: %s\n", label);
	*failures += 1;
}

int main(void) {
	bool emulator_log_ready = debug_init_emulog();
	console_init();
	int failures = 0;

	fprintf(stdout, "Odin pinned-libdragon binding ABI test\n\n");

	record_check(stderr, stdout, "C -> Odin resolution_t parameter",
		odin_resolution_digest(reference_resolution()) == RESOLUTION_DIGEST, &failures);
	record_check(stderr, stdout, "C -> Odin resolution_t result",
		resolution_matches(odin_make_resolution()), &failures);
	record_check(stderr, stdout, "Odin -> C resolution_t parameter",
		odin_to_c_resolution_parameter() == RESOLUTION_DIGEST, &failures);
	record_check(stderr, stdout, "Odin -> C resolution_t result",
		odin_to_c_resolution_result() == RESOLUTION_DIGEST, &failures);

	record_check(stderr, stdout, "C -> Odin joypad_buttons_t parameter",
		odin_buttons_digest(reference_buttons()) == BUTTONS_DIGEST, &failures);
	record_check(stderr, stdout, "C -> Odin joypad_buttons_t result",
		buttons_match(odin_make_buttons()), &failures);
	record_check(stderr, stdout, "Odin -> C joypad_buttons_t parameter",
		odin_to_c_buttons_parameter() == BUTTONS_DIGEST, &failures);
	record_check(stderr, stdout, "Odin -> C joypad_buttons_t result",
		odin_to_c_buttons_result() == BUTTONS_DIGEST, &failures);

	record_check(stderr, stdout, "C -> Odin joypad_inputs_t parameter",
		odin_inputs_digest(reference_inputs()) == INPUTS_DIGEST, &failures);
	record_check(stderr, stdout, "C -> Odin joypad_inputs_t result",
		inputs_match(odin_make_inputs()), &failures);
	record_check(stderr, stdout, "Odin -> C joypad_inputs_t parameter",
		odin_to_c_inputs_parameter() == INPUTS_DIGEST, &failures);
	record_check(stderr, stdout, "Odin -> C joypad_inputs_t result",
		odin_to_c_inputs_result() == INPUTS_DIGEST, &failures);

	record_check(stderr, stdout, "C -> Odin false _Bool parameter",
		odin_bool_parameter(false) == BOOL_FALSE_DIGEST, &failures);
	record_check(stderr, stdout, "C -> Odin true _Bool parameter",
		odin_bool_parameter(true) == BOOL_TRUE_DIGEST, &failures);
	record_check(stderr, stdout, "C -> Odin false _Bool result",
		!odin_bool_result(BOOL_FALSE_DIGEST), &failures);
	record_check(stderr, stdout, "C -> Odin true _Bool result",
		odin_bool_result(BOOL_TRUE_DIGEST), &failures);
	record_check(stderr, stdout, "Odin -> C false _Bool parameter",
		odin_to_c_bool_false_parameter() == BOOL_FALSE_DIGEST, &failures);
	record_check(stderr, stdout, "Odin -> C true _Bool parameter",
		odin_to_c_bool_true_parameter() == BOOL_TRUE_DIGEST, &failures);
	record_check(stderr, stdout, "Odin -> C false _Bool result",
		!odin_to_c_bool_false_result(), &failures);
	record_check(stderr, stdout, "Odin -> C true _Bool result",
		odin_to_c_bool_true_result(), &failures);

	record_check(stderr, stdout, "libdragon debug_init_emulog _Bool result",
		emulator_log_ready && odin_libdragon_debug_result(), &failures);
	record_check(stderr, stdout, "libdragon console_set_debug false _Bool parameter",
		odin_libdragon_console_bool_parameter(false), &failures);
	record_check(stderr, stdout, "libdragon console_set_debug true _Bool parameter",
		odin_libdragon_console_bool_parameter(true), &failures);

	if (failures == 0) {
		fprintf(stderr, "PASS: Odin libdragon binding ABI 23/23\n");
		fprintf(stdout, "PASS: Odin libdragon binding ABI 23/23\n");
	} else {
		fprintf(stderr, "FAIL: Odin libdragon binding ABI (%d/23 failed)\n", failures);
		fprintf(stdout, "FAIL: Odin libdragon binding ABI (%d/23 failed)\n", failures);
	}

	for (;;) {}
}
