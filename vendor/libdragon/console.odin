package libdragon

import "core:c"

foreign import lib "system:dragon"

RENDER_MANUAL    :: c.int(0)
RENDER_AUTOMATIC :: c.int(1)

@(default_calling_convention="c")
foreign lib {
	console_init            :: proc() ---
	console_close           :: proc() ---
	console_set_debug       :: proc(debug: c.bool) ---
	console_set_render_mode :: proc(mode: c.int) ---
	console_clear           :: proc() ---
	console_render          :: proc() ---
}
