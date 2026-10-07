#+build amd64
#+vet unused-variables
package test_internal

import "base:intrinsics"
import "core:testing"

// `#clobber` of a register outside the table's tracked set (amd64 %r12 is callee-saved and
// untracked) once added a "<reg>" placeholder next to it in the LLVM clobber list. The
// clobber itself must still reach LLVM, so values live across the template are kept out of
// %r12 and the caller's %r12 is saved. Enough values stay live across each call that the
// allocator would reach for %r12 if it were free.
@(test)
asm_clobber_of_an_untracked_register :: proc(t: ^testing.T) {
	bump :: asm(a: u64) -> (r: u64) [#clobber %r12] {
		mov %r12, a
		add %r12, 1
		mov r, %r12
	}

	seed := [12]u64{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}
	v: [12]u64
	for i in 0..<len(v) {
		v[i] = intrinsics.volatile_load(&seed[i])
	}
	x0, x1, x2,  x3  := v[0], v[1], v[2],  v[3]
	x4, x5, x6,  x7  := v[4], v[5], v[6],  v[7]
	x8, x9, x10, x11 := v[8], v[9], v[10], v[11]

	total: u64
	for i in u64(0)..<8 {
		total += bump(i)
		x0 += 1; x1 += 2; x2 += 3;  x3  += 4
		x4 += 5; x5 += 6; x6 += 7;  x7  += 8
		x8 += 9; x9 += 10; x10 += 11; x11 += 12
	}

	testing.expect_value(t, total, u64(36)) // (0+1) + (1+1) + ... + (7+1)
	testing.expect_value(t, x0 + x1 + x2 + x3 + x4 + x5 + x6 + x7 + x8 + x9 + x10 + x11, u64(78 + 8*78))
}

// A literal write to the register an output is pinned to assigns that output, so the
// template below passes the definite-assignment check and returns what it wrote.
@(test)
asm_literal_write_to_a_pinned_output_assigns_it :: proc(t: ^testing.T) {
	via_r12 :: asm(a: u64) -> (r: u64) [r = %r12] { mov %r12, a }
	via_rdx :: asm(a: u64) -> (r: u64) [r = %rdx] { mov %rdx, a }

	testing.expect_value(t, via_r12(0x1234_5678_9abc_def0), u64(0x1234_5678_9abc_def0))
	testing.expect_value(t, via_rdx(42), u64(42))
}

// `cqo` reads its input through the register it is pinned to (%rax) and never names it.
// Under -vet (this file opts in with `#+vet unused-variables`), that implicit read counts as
// a use; it used to report "'asm' input parameter 'a' is declared but never used".
@(test)
asm_implicit_read_of_a_pinned_input_is_a_use :: proc(t: ^testing.T) {
	sign_hi :: asm(a: i64) -> (hi: i64) [a = %rax, hi = %rdx] { cqo }

	testing.expect_value(t, sign_hi(-5), i64(-1))
	testing.expect_value(t, sign_hi(5),  i64(0))
}
