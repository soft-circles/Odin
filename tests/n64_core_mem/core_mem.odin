package n64_core_mem

// core:mem must type-check on N64 without its core:sync-backed allocators:
// N64 has no futex or thread-identity primitives. Checked, not run.
import "core:mem"

#assert(ODIN_OS == .N64)

arena_round_trip :: proc(storage: []byte) -> bool {
	arena: mem.Arena
	mem.arena_init(&arena, storage)
	allocator := mem.arena_allocator(&arena)
	data, err := mem.alloc_bytes(16, 8, allocator)
	if err != nil || len(data) != 16 { return false }
	mem.arena_free_all(&arena)
	return arena.offset == 0
}
