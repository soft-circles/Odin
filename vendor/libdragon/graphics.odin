package libdragon

import "core:c"

foreign import lib "system:dragon"

@(default_calling_convention="c")
foreign lib {
	// Create a packed libdragon color for the CPU drawing operations below.
	// Draw only to a display_get surface and submit it with display_show.
	graphics_make_color       :: proc(r, g, b, a: c.int) -> u32 ---
	// Replace every pixel in the acquired surface.
	graphics_fill_screen      :: proc(surf: ^surface_t, color: u32) ---
	// Draw a filled rectangle in surface coordinates.
	graphics_draw_box         :: proc(surf: ^surface_t, x, y, width, height: c.int, color: u32) ---
	// Select libdragon's built-in font before drawing text.
	graphics_set_default_font :: proc() ---
	// Set the foreground and background colors used by subsequent text calls.
	graphics_set_color        :: proc(forecolor, backcolor: u32) ---
	// Draw a NUL-terminated string with the currently selected font and colors.
	graphics_draw_text        :: proc(surf: ^surface_t, x, y: c.int, msg: cstring) ---
}
