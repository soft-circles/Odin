package n64_bounds_check

// Each exported procedure below indexes past what the compiler can prove, so upstream's bounds-check elision must
// keep its `runtime.bounds_check_error*` call; `fixed_array_constant_in_range` is the one it may (and should) drop.

Holder :: struct {
	items: []u32,
	count: int,
}

// len(s) >= 4 does not put index 4 in range.
@(export)
slice_index_past_checked_len :: proc "contextless" (s: []u32) -> u32 {
	if len(s) < 4 {
		return 0
	}
	return s[4]
}

// `i & 7` ranges over 0..7, past the array's four elements.
@(export)
fixed_array_index_past_len :: proc "contextless" (a: ^[4]u32, i: int) -> u32 {
	return a[i & 7]
}

// The guard is on a different field than the slice's length.
@(export)
struct_field_slice_dynamic_index :: proc "contextless" (h: ^Holder, i: int) -> u32 {
	if i >= 0 && i < h.count {
		return h.items[i]
	}
	return 0
}

// The loop runs one past the end.
@(export)
loop_bound_past_len :: proc "contextless" (s: []u32) -> (sum: u32) {
	for i in 0 ..= len(s) {
		sum += s[i]
	}
	return
}

@(export)
fixed_array_constant_in_range :: proc "contextless" (a: ^[4]u32) -> u32 {
	return a[3]
}

// Upstream's elision proves `i < len(s)` for a loop over the slice's own length.
@(export)
loop_bound_is_len :: proc "contextless" (s: []u32) -> (sum: u32) {
	for i in 0 ..< len(s) {
		sum += s[i]
	}
	return
}
