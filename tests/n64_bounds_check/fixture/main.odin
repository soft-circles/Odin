package n64_bounds_check

// Most exported procedures below index past what the compiler can prove, so upstream's bounds-check elision must keep
// their `runtime.bounds_check_error*` or `runtime.slice_expr_error*` calls. `fixed_array_constant_in_range`,
// `loop_bound_is_len` and `slice_expression_constant_in_range` are provably in range and must have none.

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

// Upstream 2804ac31c: `k` changes inside the loop, so the `s[k]` in the loop's bound is not covered by the `s[k]` check
// before it. Each keeps its own check.
@(export)
loop_interval_end_index_prechecked :: proc "contextless" (s: []u32) -> (sum: u32) {
	k := 0
	sum = s[k]
	for i in 0 ..< int(s[k]) {
		sum += u32(i)
		k += 5
	}
	return
}

Iterator :: struct {
	n: int,
}

next_value :: proc "contextless" (v: u32, it: ^Iterator) -> (u32, bool) {
	it.n += 1
	return v, it.n < 10
}

// Upstream 2804ac31c: the same for an index in an iterator call's arguments, which runs once per iteration.
@(export)
loop_iterator_argument_index_prechecked :: proc "contextless" (s: []u32) -> (sum: u32) {
	it: Iterator
	k := 0
	sum = s[k]
	for v in next_value(s[k], &it) {
		sum += v
		k += 5
	}
	return
}

// `lo` and `hi` are only known to be in order, not to be within `len(s)`.
@(export)
slice_expression_past_len :: proc "contextless" (s: []u32, lo, hi: int) -> []u32 {
	if lo < 0 || lo > hi {
		return nil
	}
	return s[lo:hi]
}

// Constant ends inside the array's four elements.
@(export)
slice_expression_constant_in_range :: proc "contextless" (a: ^[4]u32) -> []u32 {
	return a[1:3]
}
