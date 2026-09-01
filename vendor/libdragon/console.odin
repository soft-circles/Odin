package libdragon

import "core:c"

foreign import lib "system:dragon"

RENDER_MANUAL    :: c.int(0)
RENDER_AUTOMATIC :: c.int(1)

@(default_calling_convention="c")
foreign lib {
	// Initialize the console and give it display ownership. Do not mix this
	// lifecycle with direct display_get/display_show operations.
	console_init            :: proc() ---
	// Close the console before reinitializing direct display.
	console_close           :: proc() ---
	// Mirror console output to the configured debug channel.
	console_set_debug       :: proc(debug: c.bool) ---
	// Select manual or automatic console rendering.
	console_set_render_mode :: proc(mode: c.int) ---
	// Clear the console's text state.
	console_clear           :: proc() ---
	// Present the console explicitly when using manual rendering.
	console_render          :: proc() ---
}
