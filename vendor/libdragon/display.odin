package libdragon

import "core:c"

foreign import lib "system:dragon"

interlace_mode_t :: enum c.int {
	INTERLACE_OFF,
	INTERLACE_HALF,
	INTERLACE_FULL,
}

resolution_t :: struct {
	width:           i32,
	height:          i32,
	interlaced:      interlace_mode_t,
	aspect_ratio:    f32,
	overscan_margin: f32,
}

VI_CRT_MARGIN :: f32(0.05)

RESOLUTION_256x240 :: resolution_t{width = 256, height = 240, interlaced = .INTERLACE_OFF}
RESOLUTION_320x240 :: resolution_t{width = 320, height = 240, interlaced = .INTERLACE_OFF}
RESOLUTION_512x240 :: resolution_t{width = 512, height = 240, interlaced = .INTERLACE_OFF}
RESOLUTION_640x240 :: resolution_t{width = 640, height = 240, interlaced = .INTERLACE_OFF}
RESOLUTION_512x480 :: resolution_t{width = 512, height = 480, interlaced = .INTERLACE_HALF}
RESOLUTION_640x480 :: resolution_t{width = 640, height = 480, interlaced = .INTERLACE_HALF}

bitdepth_t :: enum c.int {
	DEPTH_16_BPP,
	DEPTH_32_BPP,
}

gamma_t :: enum c.int {
	GAMMA_NONE           = 0,
	GAMMA_CORRECT        = 1 << 2,
	GAMMA_CORRECT_DITHER = (1 << 2) | (1 << 3),
}

filter_options_t :: enum c.int {
	FILTERS_DISABLED,
	FILTERS_RESAMPLE,
	FILTERS_DEDITHER,
	FILTERS_RESAMPLE_ANTIALIAS,
	FILTERS_RESAMPLE_ANTIALIAS_DEDITHER,
}

// The pinned SDK defines surface_t concretely in surface.h. This narrow binding
// intentionally keeps it opaque because the exposed calls only pass pointers
// and do not access its fields. Never allocate or inspect surface_t in Odin.
surface_t :: struct {}

@(default_calling_convention="c")
foreign lib {
	// Initialize direct display once before acquiring surfaces. Do not call this
	// while the console owns display state.
	display_init :: proc(
		res: resolution_t,
		bit: bitdepth_t,
		num_buffers: u32,
		gamma: gamma_t,
		filters: filter_options_t,
	) ---
	// Acquire one libdragon-owned surface for the next frame. A nil result means
	// no surface is currently available.
	display_get  :: proc() -> ^surface_t ---
	// Submit the same surface returned by display_get after drawing is complete.
	display_show :: proc(surf: ^surface_t) ---
}

#assert(size_of(interlace_mode_t) == size_of(c.int))
#assert(size_of(resolution_t) == 20)
#assert(align_of(resolution_t) == 4)
#assert(offset_of(resolution_t, width) == 0)
#assert(offset_of(resolution_t, height) == 4)
#assert(offset_of(resolution_t, interlaced) == 8)
#assert(offset_of(resolution_t, aspect_ratio) == 12)
#assert(offset_of(resolution_t, overscan_margin) == 16)
