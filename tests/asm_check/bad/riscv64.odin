#+build riscv64
package asm_check_bad

// Checker rejections, one per template and one template per line: the driver matches
// each expected message to the line its template is declared on.

write_input         :: asm(a: u64) -> (r: u64) { addi a, a, 1; mv r, a }
write_input_literal :: asm(a: u64) -> (r: u64) [a = %t1] { addi %t1, %t1, 1; mv r, %t1 }
mv_three_operands   :: asm(a: u64) -> (r: u64) { mv r, a, a }
