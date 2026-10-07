#+build amd64
package asm_check_bad

// Checker rejections, one per template and one template per line: the driver matches
// each expected message to the line its template is declared on.

write_input          :: asm(a: u64) -> (r: u64) { add a, 1; mov r, a }
write_pinned_input   :: asm(a: u64) -> (r: u64) [a = %rcx] { add a, 1; mov r, a }
write_input_literal  :: asm(a: u64) -> (r: u64) [a = %r12] { add %r12, 1; mov r, %r12 }
write_input_view     :: asm(a: u64) -> (r: u64) [ab: u32 = a] { mov ab, 1; mov r, a }
zero_input           :: asm(a: u64) -> (r: u64) { xor a, a; mov r, a }
write_input_no_out   :: asm(a: u64) { add a, 1 }
register_typo        :: asm(a: u64) -> (r: u64) { mov r, %rbxx }
