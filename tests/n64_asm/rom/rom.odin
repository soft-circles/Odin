package n64_asm_rom

// Runtime probe for CPU asm templates on the N64 (MIPS III / VR4300).
//
// It writes hand-assembled MIPS leaf functions into cached RAM, makes them
// visible to instruction fetch with Odin reimplementations of libdragon's
// data_cache_hit_writeback_invalidate and inst_cache_hit_invalidate (built
// from `cache` templates), calls them through a `proc "c"` pointer, and times
// the run with an `mfc0` template reading CP0 Count. One buffer is rewritten
// after it has run, so its I-cache line is stale and the result depends on
// the I-cache invalidate as well as the D-cache writeback.

import "base:runtime"

// Fixture-local logging only: the SDK log is the test oracle.
foreign import dragon "system:dragon"
@(default_calling_convention="c")
foreign dragon {
	debugf :: proc(msg: cstring, #c_vararg args: ..any) ---
}

#assert(ODIN_OS == .N64)

PASS_SENTINEL :: "ODIN_N64_ASM_PASS:v1\n"
FAIL_SENTINEL :: "ODIN_N64_ASM_FAIL:v1\n"

DCACHE_LINE :: 16
ICACHE_LINE :: 32

dcache_hit_wb_inv_line :: asm(p: rawptr) { cache 0x15, [p] }
icache_hit_inv_line    :: asm(p: rawptr) { cache 0x10, [p] }
read_cp0_count         :: asm() -> (r: u32) { mfc0 r, %c0_count }

// Not exported: libdragon already defines C symbols with libdragon's names.
@(private="file")
dcache_writeback_invalidate :: proc "contextless" (addr: rawptr, length: int) {
	if length <= 0 { return }
	cur  := uintptr(addr) &~ (DCACHE_LINE-1)
	stop := uintptr(addr) + uintptr(length)
	for ; cur < stop; cur += DCACHE_LINE {
		dcache_hit_wb_inv_line(rawptr(cur))
	}
}

@(private="file")
icache_invalidate :: proc "contextless" (addr: rawptr, length: int) {
	if length <= 0 { return }
	cur  := uintptr(addr) &~ (ICACHE_LINE-1)
	stop := uintptr(addr) + uintptr(length)
	for ; cur < stop; cur += ICACHE_LINE {
		icache_hit_inv_line(rawptr(cur))
	}
}

// One I-cache line, so each buffer's lines belong to it alone.
@(private="file")
Code_Buffer :: struct #align(ICACHE_LINE) {
	words: [ICACHE_LINE/4]u32,
}

@(private="file")
Leaf :: #type proc "c" (x: u32) -> u32

// Big-endian MIPS words for: addiu $v0, $a0, imm; jr $ra; nop.
ADDIU_V0_A0 :: u32(0x2482_0000) // opcode 9 | rs=$a0(4) | rt=$v0(2)
JR_RA       :: u32(0x03E0_0008) // SPECIAL | rs=$ra(31) | funct jr(8)
NOP         :: u32(0x0000_0000)

ADDEND           :: 42
REFRESHED_ADDEND :: ADDEND + 99
OTHER_ADDEND     :: 7
ARGUMENT         :: u32(1000)

@(private="file")
primary_code: Code_Buffer

@(private="file")
control_code: Code_Buffer

@(private="file")
all_passed := true

@(private="file")
check :: proc "contextless" (name: cstring, ok: bool) {
	debugf("ODIN_N64_ASM_CHECK:v1:%s:%s\n", name, cstring("PASS") if ok else cstring("FAIL"))
	all_passed = all_passed && ok
}

@(private="file")
emit_add_immediate :: proc "contextless" (code: ^Code_Buffer, addend: u16) {
	code.words = {}
	code.words[0] = ADDIU_V0_A0 | u32(addend)
	code.words[1] = JR_RA
	code.words[2] = NOP
	dcache_writeback_invalidate(code, size_of(Code_Buffer))
	icache_invalidate(code, size_of(Code_Buffer))
}

// The harness's oracle for a generated leaf: call it and compare.
@(private="file")
leaf_returns :: proc "contextless" (code: ^Code_Buffer, arg, want: u32) -> bool {
	leaf := Leaf(rawptr(code))
	got := leaf(arg)
	if got != want {
		debugf("ODIN_N64_ASM_INFO:v1:leaf(%u) returned %u, expected %u\n", arg, got, want)
	}
	return got == want
}

main :: proc() {
	// Through the runtime's stderr path: it must open the emulator log itself.
	runtime.print_string("ODIN_N64_ASM_CHECK:v1:MAIN_REACHED:PASS\n")
	before := read_cp0_count()
	check("COUNT_READ", true)

	emit_add_immediate(&primary_code, ADDEND)
	ok := leaf_returns(&primary_code, ARGUMENT, ARGUMENT + ADDEND)

	// The call above left primary_code's line in the I-cache. Rewrite the
	// same buffer before logging, so no other code can evict that line:
	// without the I-cache invalidate the CPU runs the stale ADDEND.
	emit_add_immediate(&primary_code, REFRESHED_ADDEND)
	refreshed := leaf_returns(&primary_code, ARGUMENT, ARGUMENT + REFRESHED_ADDEND)
	check("CALL_GENERATED_CODE", ok)
	check("ICACHE_REFRESH", refreshed)

	// Negative control: a buffer adding a different constant must be
	// rejected by the same oracle, and accepted with its own constant.
	emit_add_immediate(&control_code, OTHER_ADDEND)
	rejected := !leaf_returns(&control_code, ARGUMENT, ARGUMENT + ADDEND)
	accepted := leaf_returns(&control_code, ARGUMENT, ARGUMENT + OTHER_ADDEND)
	check("NEGATIVE_CONTROL", rejected && accepted)

	after := read_cp0_count()
	delta := after - before // wraps correctly across a Count overflow
	if delta == 0 || delta >= 1 << 31 {
		debugf("ODIN_N64_ASM_INFO:v1:count before %u after %u\n", before, after)
	}
	check("COUNT_ADVANCED", delta != 0 && delta < 1 << 31)

	debugf(PASS_SENTINEL if all_passed else FAIL_SENTINEL)
}
