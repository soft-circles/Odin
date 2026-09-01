#+feature global-context

package n64_dfs

import "core:c"
import ld "vendor:libdragon"

#assert(ODIN_OS == .N64)

MOUNT_SENTINEL  :: "ODIN_N64_DFS_CHECK:v1:MOUNT:PASS\n"
OPEN_SENTINEL   :: "ODIN_N64_DFS_CHECK:v1:OPEN:PASS\n"
SIZE_SENTINEL   :: "ODIN_N64_DFS_CHECK:v1:SIZE:PASS\n"
READ_SENTINEL   :: "ODIN_N64_DFS_CHECK:v1:READ:PASS\n"
CONTENT_SENTINEL :: "ODIN_N64_DFS_CHECK:v1:CONTENT:PASS\n"
CLOSE_SENTINEL  :: "ODIN_N64_DFS_CHECK:v1:CLOSE:PASS\n"
STATE_SENTINEL  :: "ODIN_N64_DFS_STATE:v1:FRAME_SUBMITTED\n"
READY_SENTINEL  :: "ODIN_N64_DFS_READY:v1\n"
PASS_SENTINEL   :: "ODIN_N64_DFS_PASS:v1\n"

ASSET_PATH    :: "message.txt"
ASSET_CONTENT :: "ODIN N64 DFS ASSET v1\n"
BUFFER_SIZE   :: 64

Asset_Load_Error :: enum {
	None,
	Mount,
	Open,
	Size,
	Read,
	Content,
	Close,
}

Asset_Load_Result :: struct {
	bytes: [BUFFER_SIZE]byte,
	count: int,
}

@(private="file")
bytes_match_string :: proc "contextless" (bytes: []byte, expected: string) -> bool {
	if len(bytes) != len(expected) {
		return false
	}
	for index in 0..<len(expected) {
		if bytes[index] != expected[index] {
			return false
		}
	}
	return true
}

@(private="file", require_results)
load_asset :: proc "contextless" () -> (Asset_Load_Result, Asset_Load_Error) {
	result: Asset_Load_Result
	if ld.dfs_init(ld.DFS_DEFAULT_LOCATION) != ld.DFS_ESUCCESS {
		return result, .Mount
	}

	handle := ld.dfs_open(ASSET_PATH)
	if handle < 0 {
		return result, .Open
	}

	size := ld.dfs_size(u32(handle))
	if size != c.int(len(ASSET_CONTENT)) || size > BUFFER_SIZE {
		_ = ld.dfs_close(u32(handle))
		return result, .Size
	}

	read_count := ld.dfs_read(rawptr(&result.bytes[0]), 1, size, u32(handle))
	if read_count != size {
		_ = ld.dfs_close(u32(handle))
		return result, .Read
	}
	result.count = int(read_count)
	if !bytes_match_string(result.bytes[:result.count], ASSET_CONTENT) {
		_ = ld.dfs_close(u32(handle))
		return result, .Content
	}

	if ld.dfs_close(u32(handle)) != ld.DFS_ESUCCESS {
		return result, .Close
	}
	return result, .None
}

@(private="file")
failure_sentinel :: proc "contextless" (err: Asset_Load_Error) -> cstring {
	switch err {
	case .Mount:
		return "ODIN_N64_DFS_FAIL:v1:MOUNT\n"
	case .Open:
		return "ODIN_N64_DFS_FAIL:v1:OPEN\n"
	case .Size:
		return "ODIN_N64_DFS_FAIL:v1:SIZE\n"
	case .Read:
		return "ODIN_N64_DFS_FAIL:v1:READ\n"
	case .Content:
		return "ODIN_N64_DFS_FAIL:v1:CONTENT\n"
	case .Close:
		return "ODIN_N64_DFS_FAIL:v1:CLOSE\n"
	case .None:
		return "ODIN_N64_DFS_FAIL:v1:INTERNAL\n"
	}
	return "ODIN_N64_DFS_FAIL:v1:INTERNAL\n"
}

@(private="file")
draw_success_frame :: proc "contextless" (frame: ^ld.surface_t) {
	background := ld.graphics_make_color(8, 16, 32, 255)
	panel := ld.graphics_make_color(20, 42, 68, 255)
	accent := ld.graphics_make_color(72, 224, 160, 255)
	text := ld.graphics_make_color(238, 244, 248, 255)

	ld.graphics_fill_screen(frame, background)
	ld.graphics_draw_box(frame, 14, 18, 292, 204, panel)
	ld.graphics_draw_box(frame, 14, 18, 292, 8, accent)
	ld.graphics_draw_box(frame, 26, 64, 12, 12, accent)
	ld.graphics_draw_box(frame, 26, 94, 12, 12, accent)
	ld.graphics_draw_box(frame, 26, 124, 12, 12, accent)
	ld.graphics_set_default_font()
	ld.graphics_set_color(text, panel)
	ld.graphics_draw_text(frame, 28, 36, "ODIN N64 DFS + METADATA")
	ld.graphics_draw_text(frame, 48, 64, "DRAGONFS MOUNTED")
	ld.graphics_draw_text(frame, 48, 94, "READ message.txt: 22 BYTES")
	ld.graphics_draw_text(frame, 48, 124, "ODIN N64 DFS ASSET v1")
	ld.graphics_draw_text(frame, 28, 174, "EXTENDED METADATA INI EMBEDDED")
}

@(private="file")
present_success_frame :: proc "contextless" () -> bool {
	ld.display_init(ld.RESOLUTION_320x240, .DEPTH_16_BPP, 2, .GAMMA_NONE, .FILTERS_DISABLED)
	frame := ld.display_get()
	if frame == nil {
		ld.debugf("ODIN_N64_DFS_FAIL:v1:DISPLAY_GET\n")
		return false
	}
	draw_success_frame(frame)
	ld.display_show(frame)
	return true
}

@(private="file")
run_sample :: proc "contextless" () {
	_, load_error := load_asset()
	if load_error != .None {
		ld.debugf(failure_sentinel(load_error))
		return
	}

	ld.debugf(MOUNT_SENTINEL)
	ld.debugf(OPEN_SENTINEL)
	ld.debugf(SIZE_SENTINEL)
	ld.debugf(READ_SENTINEL)
	ld.debugf(CONTENT_SENTINEL)
	ld.debugf(CLOSE_SENTINEL)
	if !present_success_frame() {
		return
	}
	ld.debugf(STATE_SENTINEL)
	ld.debugf(READY_SENTINEL)
	ld.debugf(PASS_SENTINEL)
}

main :: proc() {
	_ = ld.debug_init_emulog()
	run_sample()
	for {}
}
