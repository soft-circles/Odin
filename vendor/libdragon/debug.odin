package libdragon

import "core:c"

foreign import lib "system:dragon"

@(default_calling_convention="c")
foreign lib {
	// Initialize the EMUX debug channel before calling debugf.
	debug_init_emulog :: proc() -> c.bool ---
	// Write an EMUX log message. This is C variadic: every argument must match
	// its C format specifier.
	debugf            :: proc(msg: cstring, #c_vararg args: ..any) ---
}
