package n64_console

import ld "vendor:libdragon"

READY_SENTINEL :: "ODIN_N64_CONSOLE_READY:v1\n"
PASS_SENTINEL  :: "ODIN_N64_CONSOLE_PASS:v1\n"
FAIL_SENTINEL  :: "ODIN_N64_CONSOLE_FAIL:v1\n"

foreign {
	puts :: proc "c" (text: cstring) -> i32 ---
}

@(private="file")
draw_post_console_frame :: proc "contextless" (frame: ^ld.surface_t) {
	background := ld.graphics_make_color(24, 12, 32, 255)
	accent := ld.graphics_make_color(80, 208, 224, 255)
	text := ld.graphics_make_color(255, 255, 255, 255)

	ld.graphics_fill_screen(frame, background)
	ld.graphics_draw_box(frame, 32, 40, 256, 160, accent)
	ld.graphics_draw_box(frame, 36, 44, 248, 152, background)
	ld.graphics_set_default_font()
	ld.graphics_set_color(text, background)
	ld.graphics_draw_text(frame, 64, 88, "CONSOLE LIFECYCLE")
	ld.graphics_draw_text(frame, 64, 112, "INIT RENDER CLOSE")
	ld.graphics_draw_text(frame, 64, 136, "DISPLAY REACQUIRED")
}

@(private="file")
run_console_lifecycle :: proc "contextless" () {
	_ = ld.debug_init_emulog()
	ld.debugf(READY_SENTINEL)

	ld.console_init()
	ld.console_set_render_mode(ld.RENDER_MANUAL)
	ld.console_clear()
	_ = puts("ODIN CONSOLE BINDING")
	ld.console_render()
	ld.console_close()

	ld.display_init(ld.RESOLUTION_320x240, .DEPTH_16_BPP, 2, .GAMMA_NONE, .FILTERS_DISABLED)
	frame := ld.display_get()
	if frame == nil {
		ld.debugf(FAIL_SENTINEL)
		return
	}
	draw_post_console_frame(frame)
	ld.display_show(frame)
	ld.debugf(PASS_SENTINEL)
}

@(export)
odin_n64_console :: proc "c" () {
	run_console_lifecycle()
}
