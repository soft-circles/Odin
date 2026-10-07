package n64_asm_bad_backend

// Pins LLVM has no constraint for, and non-GPR literals in a GPR slot. The
// checker rejects these through the table's pin_reject_reason and
// literal_register_reject_reason hooks; the LLVM generator keeps the same
// checks as a backstop. One template per line.

pin_to_cop0      :: asm(x: u32) [x = %c0_status] { mtc0 x, %c0_status }
pin_to_fcc0      :: asm(x: u32) -> (r: u32) [x = %fcc0] { move r, x }
cop0_in_gpr_slot :: asm(x: u32) -> (r: u32) { addu r, x, %c0_status }
hi_in_gpr_slot   :: asm(x: u32) -> (r: u32) { mult x, x; addu r, x, %hi }
fcc0_in_gpr_slot :: asm(a: f32, x: u32) -> (r: u32) { c_eq_s a, a; addu r, x, %fcc0 }
fcr_in_gpr_slot  :: asm(x: u32) -> (r: u32) { addu r, x, %fcr31 }

@(export)
use :: proc "contextless" (x: u32, a: f32) -> u32 {
	pin_to_cop0(x)
	return pin_to_fcc0(x) + cop0_in_gpr_slot(x) + hi_in_gpr_slot(x) + fcc0_in_gpr_slot(a, x) + fcr_in_gpr_slot(x)
}
