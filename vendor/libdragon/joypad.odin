package libdragon

import "core:c"

foreign import lib "system:dragon"

joypad_port_t :: enum c.int {
	JOYPAD_PORT_1 = 0,
	JOYPAD_PORT_2 = 1,
	JOYPAD_PORT_3 = 2,
	JOYPAD_PORT_4 = 3,
}

JOYPAD_PORT_COUNT :: 4

// The C union also provides bitfield members. They are intentionally omitted:
// C bitfield layout is implementation-defined, while raw is ABI-stable.
joypad_buttons_t :: struct #raw_union {
	raw: u16,
}

// Masks for joypad_buttons_t.raw, derived from the pinned big-endian C layout.
JOYPAD_BUTTON_A       :: u16(0x8000)
JOYPAD_BUTTON_B       :: u16(0x4000)
JOYPAD_BUTTON_Z       :: u16(0x2000)
JOYPAD_BUTTON_START   :: u16(0x1000)
JOYPAD_BUTTON_D_UP    :: u16(0x0800)
JOYPAD_BUTTON_D_DOWN  :: u16(0x0400)
JOYPAD_BUTTON_D_LEFT  :: u16(0x0200)
JOYPAD_BUTTON_D_RIGHT :: u16(0x0100)
JOYPAD_BUTTON_Y       :: u16(0x0080)
JOYPAD_BUTTON_X       :: u16(0x0040)
JOYPAD_BUTTON_L       :: u16(0x0020)
JOYPAD_BUTTON_R       :: u16(0x0010)
JOYPAD_BUTTON_C_UP    :: u16(0x0008)
JOYPAD_BUTTON_C_DOWN  :: u16(0x0004)
JOYPAD_BUTTON_C_LEFT  :: u16(0x0002)
JOYPAD_BUTTON_C_RIGHT :: u16(0x0001)

joypad_inputs_t :: struct #packed {
	btn:      joypad_buttons_t,
	stick_x:  i8,
	stick_y:  i8,
	cstick_x: i8,
	cstick_y: i8,
	analog_l: u8,
	analog_r: u8,
}

@(default_calling_convention="c")
foreign lib {
	// Initialize the joypad subsystem once before polling.
	joypad_init       :: proc() ---
	// Refresh current inputs once per input cycle before reading ports.
	joypad_poll       :: proc() ---
	// Read a port only after the current cycle's joypad_poll.
	joypad_get_inputs :: proc(port: joypad_port_t) -> joypad_inputs_t ---
}

#assert(size_of(joypad_buttons_t) == 2)
#assert(align_of(joypad_buttons_t) == 2)
#assert(offset_of(joypad_buttons_t, raw) == 0)

#assert(size_of(joypad_inputs_t) == 8)
#assert(align_of(joypad_inputs_t) == 1)
#assert(offset_of(joypad_inputs_t, btn) == 0)
#assert(offset_of(joypad_inputs_t, stick_x) == 2)
#assert(offset_of(joypad_inputs_t, stick_y) == 3)
#assert(offset_of(joypad_inputs_t, cstick_x) == 4)
#assert(offset_of(joypad_inputs_t, cstick_y) == 5)
#assert(offset_of(joypad_inputs_t, analog_l) == 6)
#assert(offset_of(joypad_inputs_t, analog_r) == 7)
