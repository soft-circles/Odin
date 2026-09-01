package libdragon_bindings_probe

import ld "vendor:libdragon"

RESOLUTION_DIGEST :: u32(0x5245534f)
BUTTONS_DIGEST    :: u32(0x42544e53)
INPUTS_DIGEST     :: u32(0x494e5054)
BOOL_FALSE_DIGEST :: u32(0x46414c53)
BOOL_TRUE_DIGEST  :: u32(0x54525545)

#assert(size_of(ld.resolution_t) == 20)
#assert(align_of(ld.resolution_t) == 4)
#assert(offset_of(ld.resolution_t, width) == 0)
#assert(offset_of(ld.resolution_t, height) == 4)
#assert(offset_of(ld.resolution_t, interlaced) == 8)
#assert(offset_of(ld.resolution_t, aspect_ratio) == 12)
#assert(offset_of(ld.resolution_t, overscan_margin) == 16)

#assert(size_of(ld.joypad_buttons_t) == 2)
#assert(align_of(ld.joypad_buttons_t) == 2)
#assert(offset_of(ld.joypad_buttons_t, raw) == 0)

#assert(size_of(ld.joypad_inputs_t) == 8)
#assert(align_of(ld.joypad_inputs_t) == 1)
#assert(offset_of(ld.joypad_inputs_t, btn) == 0)
#assert(offset_of(ld.joypad_inputs_t, stick_x) == 2)
#assert(offset_of(ld.joypad_inputs_t, stick_y) == 3)
#assert(offset_of(ld.joypad_inputs_t, cstick_x) == 4)
#assert(offset_of(ld.joypad_inputs_t, cstick_y) == 5)
#assert(offset_of(ld.joypad_inputs_t, analog_l) == 6)
#assert(offset_of(ld.joypad_inputs_t, analog_r) == 7)

#assert(size_of(bool) == 1)
#assert(align_of(bool) == 1)

foreign {
	c_resolution_digest :: proc "c" (value: ld.resolution_t) -> u32 ---
	c_make_resolution   :: proc "c" () -> ld.resolution_t ---
	c_buttons_digest    :: proc "c" (value: ld.joypad_buttons_t) -> u32 ---
	c_make_buttons      :: proc "c" () -> ld.joypad_buttons_t ---
	c_inputs_digest     :: proc "c" (value: ld.joypad_inputs_t) -> u32 ---
	c_make_inputs       :: proc "c" () -> ld.joypad_inputs_t ---
	c_bool_parameter    :: proc "c" (value: bool) -> u32 ---
	c_bool_result       :: proc "c" (token: u32) -> bool ---
}

reference_resolution :: proc "contextless" () -> ld.resolution_t {
	return {
		width           = 319,
		height          = 241,
		interlaced      = ld.interlace_mode_t(2),
		aspect_ratio    = -13.25,
		overscan_margin = 0.125,
	}
}

resolution_matches :: proc "contextless" (value: ld.resolution_t) -> bool {
	return value.width == 319 &&
		value.height == 241 &&
		i32(value.interlaced) == 2 &&
		value.aspect_ratio == -13.25 &&
		value.overscan_margin == 0.125
}

reference_buttons :: proc "contextless" () -> ld.joypad_buttons_t {
	return {raw = 0xa59c}
}

buttons_match :: proc "contextless" (value: ld.joypad_buttons_t) -> bool {
	return value.raw == 0xa59c
}

reference_inputs :: proc "contextless" () -> ld.joypad_inputs_t {
	return {
		btn      = {raw = 0x936a},
		stick_x  = -101,
		stick_y  = 87,
		cstick_x = -76,
		cstick_y = 63,
		analog_l = 201,
		analog_r = 254,
	}
}

inputs_match :: proc "contextless" (value: ld.joypad_inputs_t) -> bool {
	return value.btn.raw == 0x936a &&
		value.stick_x == -101 &&
		value.stick_y == 87 &&
		value.cstick_x == -76 &&
		value.cstick_y == 63 &&
		value.analog_l == 201 &&
		value.analog_r == 254
}

@(export)
odin_resolution_digest :: proc "c" (value: ld.resolution_t) -> u32 {
	return RESOLUTION_DIGEST if resolution_matches(value) else 0
}

@(export)
odin_make_resolution :: proc "c" () -> ld.resolution_t {
	return reference_resolution()
}

@(export)
odin_buttons_digest :: proc "c" (value: ld.joypad_buttons_t) -> u32 {
	return BUTTONS_DIGEST if buttons_match(value) else 0
}

@(export)
odin_make_buttons :: proc "c" () -> ld.joypad_buttons_t {
	return reference_buttons()
}

@(export)
odin_inputs_digest :: proc "c" (value: ld.joypad_inputs_t) -> u32 {
	return INPUTS_DIGEST if inputs_match(value) else 0
}

@(export)
odin_make_inputs :: proc "c" () -> ld.joypad_inputs_t {
	return reference_inputs()
}

@(export)
odin_bool_parameter :: proc "c" (value: bool) -> u32 {
	return BOOL_TRUE_DIGEST if value else BOOL_FALSE_DIGEST
}

@(export)
odin_bool_result :: proc "c" (token: u32) -> bool {
	return token == BOOL_TRUE_DIGEST
}

@(export)
odin_to_c_resolution_parameter :: proc "c" () -> u32 {
	return c_resolution_digest(reference_resolution())
}

@(export)
odin_to_c_resolution_result :: proc "c" () -> u32 {
	return RESOLUTION_DIGEST if resolution_matches(c_make_resolution()) else 0
}

@(export)
odin_to_c_buttons_parameter :: proc "c" () -> u32 {
	return c_buttons_digest(reference_buttons())
}

@(export)
odin_to_c_buttons_result :: proc "c" () -> u32 {
	return BUTTONS_DIGEST if buttons_match(c_make_buttons()) else 0
}

@(export)
odin_to_c_inputs_parameter :: proc "c" () -> u32 {
	return c_inputs_digest(reference_inputs())
}

@(export)
odin_to_c_inputs_result :: proc "c" () -> u32 {
	return INPUTS_DIGEST if inputs_match(c_make_inputs()) else 0
}

@(export)
odin_to_c_bool_false_parameter :: proc "c" () -> u32 {
	return c_bool_parameter(false)
}

@(export)
odin_to_c_bool_true_parameter :: proc "c" () -> u32 {
	return c_bool_parameter(true)
}

@(export)
odin_to_c_bool_false_result :: proc "c" () -> bool {
	return c_bool_result(BOOL_FALSE_DIGEST)
}

@(export)
odin_to_c_bool_true_result :: proc "c" () -> bool {
	return c_bool_result(BOOL_TRUE_DIGEST)
}

@(export)
odin_libdragon_debug_result :: proc "c" () -> bool {
	return ld.debug_init_emulog()
}

@(export)
odin_libdragon_console_bool_parameter :: proc "c" (value: bool) -> bool {
	ld.console_set_debug(value)
	return true
}
