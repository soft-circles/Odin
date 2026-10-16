package n64_large_aggregate

// About 64 KB, like core:container/handle_map's Static_Handle_Map(513, ...) that hung LLVM's MIPS instruction
// selection when it was passed as a first-class aggregate of 8,000+ i64s.
Big :: struct {
	words: [8192]u64,
}

// Three 8-byte slots: still fits O64's four argument registers, so it stays in $4-$6.
Small :: struct {
	a, b, c: u64,
}

// Six 8-byte slots: the first four go in $4-$7 and the rest on the stack, as GCC passes it.
Spill :: struct {
	a, b, c, d, e, f: u64,
}

foreign {
	take_big_c   :: proc "c" (big: Big) ---
	take_spill_c :: proc "c" (spill: Spill, tail: i32) ---
}

@(export)
sum_big :: proc "contextless" (big: Big) -> u64 {
	return big.words[0] + big.words[8191]
}

@(export)
sum_small :: proc "contextless" (small: Small) -> u64 {
	return small.a + small.c
}

@(export)
call_sum_big :: proc "contextless" (big: ^Big) -> u64 {
	return #force_no_inline sum_big(big^)
}

@(export)
call_sum_small :: proc "contextless" (small: ^Small) -> u64 {
	return #force_no_inline sum_small(small^)
}

@(export)
call_take_big_c :: proc "c" (big: ^Big) {
	take_big_c(big^)
}

@(export)
call_take_spill_c :: proc "c" (spill: ^Spill) {
	take_spill_c(spill^, 7)
}
