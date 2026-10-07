package n64_asm_regress

// Templates for the MIPS-specific traps the asm backend has to get right.
// test_n64_asm.py names the trap each one guards.

// $at: the largest displacements still encode directly (no lui/addu through $at).
@(export) store_max_displacement :: proc "contextless" (p: rawptr, v: u32) {
	t :: asm(p: rawptr, v: u32) [p = %a0, v = %a1] { sw v, [p + 32767] }
	t(p, v)
}

@(export) store_min_displacement :: proc "contextless" (p: rawptr, v: u32) {
	t :: asm(p: rawptr, v: u32) [p = %a0, v = %a1] { sw v, [p - 32768] }
	t(p, v)
}

// Range edges: every bound is inclusive. bad_check/ rejects one past each edge.
@(export) range_edges :: proc "contextless" (a: u32, p: rawptr) -> u32 {
	t :: asm(a: u32, p: rawptr) -> (r: u32) {
		addiu r, a, 32767
		addiu r, r, -32768
		andi  r, r, 65535
		andi  r, r, 0
		sll   r, r, 31
		sll   r, r, 0
		lw    r, [p + 32767]
		lw    r, [p - 32768]
		lui   r, 65535
	}
	return t(a, p)
}

@(export) trap_code_edges :: proc "contextless" (a: u32) {
	t :: asm(a: u32) { break 1023; teq a, %zero, 1023; syscall 1048575 }
	t(a)
}

// $at: `li` with a 32-bit constant is an assembler macro.
@(export) load_wide_constant :: proc "contextless" () -> u32 {
	t :: asm() -> (r: u32) { li r, 0x12345678 }
	return t()
}

// A literal use of a pinned register is a use of its parameter.
@(export) literal_read_of_pinned :: proc "contextless" (v: u32) -> u32 {
	t :: asm(v: u32) -> (r: u32) [v = %a1] { addu r, %a1, %a1 }
	return t(v)
}

// %hi / %lo can be pinned to supply a value from outside the template.
@(export) read_pinned_lo :: proc "contextless" (v: u32) -> u32 {
	t :: asm(v: u32) -> (r: u32) [v = %lo] { mflo r }
	return t(v)
}

@(export) read_pinned_hi :: proc "contextless" (v: u32) -> u32 {
	t :: asm(v: u32) -> (r: u32) [v = %hi] { mfhi r }
	return t(v)
}

// HI/LO written by mult and read back with no #clobber: auto-clobbered.
@(export) mul_lo :: proc "contextless" (a, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) { multu a, b; mflo r }
	return t(a, b)
}

// A COP0 clobber has no LLVM constraint: it lowers to a memory clobber.
@(export) cop0_clobber_only :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [#clobber %c0_status] { addu r, a, a }
	return t(a)
}

// A plain ALU template is neither volatile nor a memory clobber.
@(export) plain_alu :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) { addu r, a, a }
	return t(a)
}

// jalr rd, rs: rd must not share a register with rs (early clobber), and the
// callee may touch any memory.
@(export) call_through_register :: proc "contextless" (f: rawptr) -> rawptr {
	t :: asm(f: rawptr) -> (r: rawptr) { jalr r, f }
	return t(f)
}

// Two labels in one template, one forward and one backward reference each.
@(export) two_labels :: proc "contextless" (n: u32) -> u32 {
	t :: asm(n: u32) -> (r: u32) [n = %a0, r = %v0] {
		move r, n
		beqz n, .done
	.top:
		addiu r, r, -1
		bnez r, .top
	.done:
	}
	return t(n)
}

// The FPU example from MIPS_ASM.md: c.olt.d sets %fcc0, bc1f reads it.
@(export) min_f64 :: proc "contextless" (a, b: f64) -> f64 {
	t :: asm(a: f64, b: f64) -> (r: f64) {
		mov_d r, a
		c_olt_d b, a
		bc1f .done
		mov_d r, b
	.done:
	}
	return t(a, b)
}

// The libdragon C0_WRITE_ENTRYHI pattern: explicit hazard nops survive.
@(export) write_entryhi :: proc "contextless" (v: u32) {
	t :: asm(v: u32) [v = %a0] {
		mtc0 v, %c0_entryhi
		nop
		nop
	}
	t(v)
}
