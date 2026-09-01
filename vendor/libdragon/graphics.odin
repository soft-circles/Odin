package libdragon

import "core:c"

foreign import lib "system:dragon"

@(default_calling_convention="c")
foreign lib {
	graphics_make_color       :: proc(r, g, b, a: c.int) -> u32 ---
	graphics_fill_screen      :: proc(surf: ^surface_t, color: u32) ---
	graphics_draw_box         :: proc(surf: ^surface_t, x, y, width, height: c.int, color: u32) ---
	graphics_set_default_font :: proc() ---
	graphics_set_color        :: proc(forecolor, backcolor: u32) ---
	graphics_draw_text        :: proc(surf: ^surface_t, x, y: c.int, msg: cstring) ---
}
