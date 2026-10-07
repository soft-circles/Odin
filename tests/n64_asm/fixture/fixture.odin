package n64_asm_fixture

// CPU asm templates on the N64's MIPS III (VR4300) target.

DCACHE_LINE :: 16
ICACHE_LINE :: 32

dcache_hit_wb_inv_line :: asm(p: rawptr) { cache 0x15, [p] }
icache_hit_inv_line    :: asm(p: rawptr) { cache 0x10, [p] }

@(export)
data_cache_hit_writeback_invalidate :: proc "contextless" (addr: rawptr, length: int) {
	if length <= 0 { return }
	cur  := uintptr(addr) &~ (DCACHE_LINE-1)
	stop := uintptr(addr) + uintptr(length)
	for ; cur < stop; cur += DCACHE_LINE {
		dcache_hit_wb_inv_line(rawptr(cur))
	}
}

@(export)
inst_cache_hit_invalidate :: proc "contextless" (addr: rawptr, length: int) {
	if length <= 0 { return }
	cur  := uintptr(addr) &~ (ICACHE_LINE-1)
	stop := uintptr(addr) + uintptr(length)
	for ; cur < stop; cur += ICACHE_LINE {
		icache_hit_inv_line(rawptr(cur))
	}
}

@(export)
read_cp0_count :: proc "contextless" () -> u32 {
	read :: asm() -> (r: u32) { mfc0 r, %c0_count }
	return read()
}

@(export)
write_cp0_compare :: proc "contextless" (v: u32) {
	write :: asm(v: u32) { mtc0 v, %c0_compare }
	write(v)
}

@(export)
load_word_at_offset :: proc "contextless" (p: ^u32) -> u32 {
	ld :: asm(p: ^u32) -> (r: u32) { lw r, [p + 8] }
	return ld(p)
}

@(export)
count_down :: proc "contextless" (n: u32) -> u32 {
	loop :: asm(n: u32) -> (r: u32) {
		move r, n
	.top:
		addiu r, r, -1
		bne r, %zero, .top
	}
	return loop(n)
}

@(export)
pinned_double :: proc "contextless" (x: u32) -> u32 {
	double :: asm(v: u32) -> (r: u32) [v = %a1, r = %v1, #clobber %t0] {
		addu %t0, v, v
		addu r, %t0, %zero
	}
	return double(x)
}

@(export)
add_u64 :: proc "contextless" (a, b: u64) -> u64 {
	add :: asm(a: u64, b: u64) -> (r: u64) { daddu r, a, b }
	return add(a, b)
}
