#+build n64
#+private
package runtime

import "base:intrinsics"

foreign import libc "system:c"

@(default_calling_convention="c")
foreign libc {
	@(link_name="malloc")  _n64_malloc  :: proc(size: int) -> rawptr ---
	@(link_name="realloc") _n64_realloc :: proc(ptr: rawptr, size: int) -> rawptr ---
	@(link_name="free")    _n64_free    :: proc(ptr: rawptr) ---
}

_heap_alloc :: proc "contextless" (size: int, zero_memory := true) -> rawptr {
	if size <= 0 {
		return nil
	}

	ptr := _n64_malloc(size)
	if ptr != nil && zero_memory {
		intrinsics.mem_zero(ptr, size)
	}
	return ptr
}

_heap_resize :: proc "contextless" (ptr: rawptr, new_size: int) -> rawptr {
	if new_size <= 0 {
		_n64_free(ptr)
		return nil
	}
	return _n64_realloc(ptr, new_size)
}

_heap_free :: proc "contextless" (ptr: rawptr) {
	_n64_free(ptr)
}
