package n64_pointer_sext

foreign {
	dfs_open :: proc "c" (path: cstring) -> i32 ---
	data_cache_hit_writeback :: proc "c" (addr: rawptr, length: u32) ---
}

// Passed by value, Scene fills one 8-byte O64 slot. `file` is at offset 0, so on big-endian MIPS it sits in the
// upper 32 bits of the argument register, and -o:speed extracts it with a 32-bit shift.
Scene :: struct {
	file: cstring,
	size: i32,
}

@(export)
open_scene :: proc "c" (scene: Scene) -> i32 {
	return dfs_open(scene.file)
}

// A KSEG0 address constant: 0x80001000 must reach the callee as 0xffffffff80001000.
@(export)
write_back_kseg0 :: proc "c" () {
	data_cache_hit_writeback(rawptr(uintptr(0x80001000)), 16)
}
