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

// #noreorder: the author fills every delay slot. A plain branch's slot runs on
// both edges: r = a when a == b, else a + 1.
@(export) noreorder_filled :: proc "contextless" (a, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) [#noreorder] {
		beq a, b, .done
		move r, a
		addiu r, a, 1
	.done:
	}
	return t(a, b)
}

// A branch-likely's slot runs on the taken path only: 8 * (n - 1) for n >= 1, else 0.
@(export) noreorder_likely :: proc "contextless" (n: i32) -> i32 {
	t :: asm(n: i32) -> (r: i32, c: i32) [n -> c, #noreorder] {
		move r, %zero
	.loop:
		addiu c, c, -1
		bgtzl c, .loop
		addiu r, r, 8
	}
	r, _ := t(n)
	return r
}

// The slot of an unconditional `b` is reachable and runs: 3 when a == 0, else a + 4.
@(export) noreorder_jump_slot :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [#noreorder] {
		bnez a, .skip
		move r, a
		b .end
		addiu r, a, 3
	.skip:
		addiu r, a, 4
	.end:
	}
	return t(a)
}

// Under `.set noat` a #noreorder body may use $1 as a scratch.
@(export) noreorder_at_scratch :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [#noreorder] {
		addiu %at, a, 5
		addu r, %at, %at
	}
	return t(a)
}

// The HI/LO boundary padding counts a #noreorder branch as one word: the multu in
// its slot is the template's second instruction, so one nop goes before it.
@(export) noreorder_mult_in_slot :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [#noreorder] {
		bnez a, .x
		multu a, a
	.x:
		mflo r
	}
	return t(a)
}

// An mflo in a delay slot: the taken edge leaves right after it, so two nops
// follow the body although the program-order tail is two instructions long.
@(export) noreorder_mflo_in_slot :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [#noreorder] {
		multu a, a
		nop
		nop
		bnez a, .end
		mflo r
		addiu r, r, 1
		addiu r, r, 1
	.end:
	}
	return t(a) * a
}

// li of 0xFFFFFFFF is one word (addiu r, $0, -1), so it may fill a slot.
@(export) noreorder_li_in_slot :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [#noreorder] {
		move r, a
		bnez a, .x
		li r, 0xFFFFFFFF
		move r, %zero
	.x:
	}
	return t(a)
}

// 0xFFFF8000 is the lowest 32-bit value one addiu holds (-32768, sign-extended).
@(export) noreorder_li_min_in_slot :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [#noreorder] {
		move r, a
		bnez a, .x
		li r, 0xFFFF8000
		move r, %zero
	.x:
	}
	return t(a)
}
