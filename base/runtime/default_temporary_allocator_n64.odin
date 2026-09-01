#+build n64
package runtime

import "base:intrinsics"

N64_TEMP_ARENA_SIZE: int : #config(N64_TEMP_ARENA_SIZE, 256 * Kilobyte)
#assert(N64_TEMP_ARENA_SIZE > 0, "N64_TEMP_ARENA_SIZE must be greater than zero")

// The generic growing-arena declarations are part of base:runtime on every
// target. Keep their compile-time constant available even though the N64
// default allocator uses only the fixed storage below.
DEFAULT_TEMP_ALLOCATOR_BACKING_SIZE :: N64_TEMP_ARENA_SIZE

NO_DEFAULT_TEMP_ALLOCATOR :: false

Default_Temp_Allocator :: struct {
	buffer: [N64_TEMP_ARENA_SIZE]byte,
	used:   int,
}

N64_Temp_Arena_Mark :: struct {
	arena: ^Default_Temp_Allocator,
	used:  int,
}

@(private="file", require_results)
n64_temp_allocation_offset :: proc "contextless" (
	arena: ^Default_Temp_Allocator,
	size, alignment: int,
) -> (offset: int, ok: bool) {
	if arena == nil || size < 0 || alignment <= 0 || !is_power_of_two_int(alignment) {
		return 0, false
	}

	base := uintptr(raw_data(arena.buffer[:]))
	address := base + uintptr(arena.used)
	mask := uintptr(alignment - 1)
	misalignment := address & mask
	padding := 0
	if misalignment != 0 {
		padding = int(uintptr(alignment) - misalignment)
	}

	remaining := N64_TEMP_ARENA_SIZE - arena.used
	if padding > remaining || size > remaining-padding {
		return 0, false
	}
	return arena.used + padding, true
}

@(private="file", require_results)
n64_temp_alloc :: proc "contextless" (
	arena: ^Default_Temp_Allocator,
	size, alignment: int,
	zero_memory: bool,
) -> (data: []byte, err: Allocator_Error) {
	if arena == nil {
		return nil, .Invalid_Pointer
	}
	if size < 0 || alignment <= 0 || !is_power_of_two_int(alignment) {
		return nil, .Invalid_Argument
	}
	if size == 0 {
		return nil, nil
	}

	offset, ok := n64_temp_allocation_offset(arena, size, alignment)
	if !ok {
		return nil, .Out_Of_Memory
	}

	arena.used = offset + size
	data = arena.buffer[offset:arena.used]
	if zero_memory {
		intrinsics.mem_zero(raw_data(data), len(data))
	}
	return data, nil
}

@(private="file", require_results)
n64_temp_pointer_offset :: proc "contextless" (
	arena: ^Default_Temp_Allocator,
	ptr: rawptr,
	size: int,
) -> (offset: int, ok: bool) {
	if arena == nil || ptr == nil || size < 0 {
		return 0, false
	}

	base := uintptr(raw_data(arena.buffer[:]))
	address := uintptr(ptr)
	if address < base {
		return 0, false
	}

	distance := address - base
	if distance > uintptr(N64_TEMP_ARENA_SIZE) {
		return 0, false
	}

	offset = int(distance)
	if size > N64_TEMP_ARENA_SIZE-offset {
		return 0, false
	}
	return offset, true
}

@(private="file", require_results)
n64_temp_resize :: proc "contextless" (
	arena: ^Default_Temp_Allocator,
	old_memory: rawptr,
	old_size, new_size, alignment: int,
	zero_memory: bool,
) -> (data: []byte, err: Allocator_Error) {
	if new_size < 0 || alignment <= 0 || !is_power_of_two_int(alignment) {
		return nil, .Invalid_Argument
	}
	if old_memory == nil {
		return n64_temp_alloc(arena, new_size, alignment, zero_memory)
	}
	if new_size == 0 {
		return nil, .Mode_Not_Implemented
	}

	offset, valid := n64_temp_pointer_offset(arena, old_memory, old_size)
	if !valid {
		return nil, .Invalid_Pointer
	}
	is_aligned := uintptr(old_memory) & uintptr(alignment-1) == 0
	if is_aligned && new_size <= old_size {
		if offset+old_size == arena.used {
			arena.used = offset + new_size
		}
		return ([^]byte)(old_memory)[:new_size], nil
	}

	if is_aligned && offset+old_size == arena.used && new_size <= N64_TEMP_ARENA_SIZE-offset {
		arena.used = offset + new_size
		data = ([^]byte)(old_memory)[:new_size]
		if zero_memory && new_size > old_size {
			intrinsics.mem_zero(raw_data(data[old_size:]), new_size-old_size)
		}
		return data, nil
	}

	data, err = n64_temp_alloc(arena, new_size, alignment, zero_memory)
	if err != nil {
		return nil, err
	}
	copy(data, ([^]byte)(old_memory)[:min(old_size, new_size)])
	return data, nil
}

@(private="file")
n64_temp_reset_to :: proc "contextless" (arena: ^Default_Temp_Allocator, used: int) {
	if arena == nil || used < 0 || used > arena.used {
		return
	}
	if arena.used > used {
		intrinsics.mem_zero(raw_data(arena.buffer[used:arena.used]), arena.used-used)
	}
	arena.used = used
}

default_temp_allocator_init :: proc "contextless" (
	s: ^Default_Temp_Allocator,
	size: int,
	backing_allocator: Allocator,
) {
	if s != nil {
		n64_temp_reset_to(s, 0)
	}
}

default_temp_allocator_destroy :: proc "contextless" (s: ^Default_Temp_Allocator) {
	if s != nil {
		n64_temp_reset_to(s, 0)
	}
}

default_temp_allocator_proc :: proc(
	allocator_data: rawptr,
	mode: Allocator_Mode,
	size, alignment: int,
	old_memory: rawptr,
	old_size: int,
	loc := #caller_location,
) -> (data: []byte, err: Allocator_Error) {
	arena := (^Default_Temp_Allocator)(allocator_data)

	switch mode {
	case .Alloc, .Alloc_Non_Zeroed:
		return n64_temp_alloc(arena, size, alignment, mode == .Alloc)
	case .Resize, .Resize_Non_Zeroed:
		return n64_temp_resize(arena, old_memory, old_size, size, alignment, mode == .Resize)
	case .Free:
		if old_memory != nil {
			return nil, .Mode_Not_Implemented
		}
	case .Free_All:
		n64_temp_reset_to(arena, 0)
	case .Query_Features:
		set := (^Allocator_Mode_Set)(old_memory)
		if set != nil {
			set^ = {
				.Alloc,
				.Alloc_Non_Zeroed,
				.Free_All,
				.Resize,
				.Resize_Non_Zeroed,
				.Query_Features,
			}
		}
	case .Query_Info:
		return nil, .Mode_Not_Implemented
	}
	return nil, nil
}

@(require_results)
default_temp_allocator_temp_begin :: proc(loc := #caller_location) -> (mark: N64_Temp_Arena_Mark) {
	if context.temp_allocator.data == &global_default_temp_allocator_data {
		mark.arena = &global_default_temp_allocator_data
		mark.used = global_default_temp_allocator_data.used
	}
	return
}

default_temp_allocator_temp_end :: proc(mark: N64_Temp_Arena_Mark, loc := #caller_location) {
	if mark.arena != nil {
		n64_temp_reset_to(mark.arena, mark.used)
	}
}

@(deferred_out=default_temp_allocator_temp_end)
DEFAULT_TEMP_ALLOCATOR_TEMP_GUARD :: #force_inline proc(
	ignore := false,
	loc := #caller_location,
) -> (N64_Temp_Arena_Mark, Source_Code_Location) {
	if ignore {
		return {}, loc
	}
	return default_temp_allocator_temp_begin(loc), loc
}

@(require_results)
default_temp_allocator :: proc(allocator: ^Default_Temp_Allocator) -> Allocator {
	return Allocator{
		procedure = default_temp_allocator_proc,
		data      = allocator,
	}
}

@(fini, private)
_destroy_n64_temp_allocator_fini :: proc "contextless" () {
	default_temp_allocator_destroy(&global_default_temp_allocator_data)
}
