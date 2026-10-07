#+build arm64
package asm_check_bad

// Checker rejections, one per template and one template per line: the driver matches
// each expected message to the line its template is declared on.

write_input         :: asm(a: u64) -> (r: u64) { add a, a, 1; mov r, a }
write_input_literal :: asm(a: u64) -> (r: u64) [a = %x9] { add %x9, %x9, 1; mov r, %x9 }
post_index_input    :: asm(p: ^u64) -> (r: u64) { ldr r, #post [p + 8] }
failed_operand      :: asm(a: u64) -> (r: u64) { add r, a, %bogus }
output_pin_sp       :: asm(a: u64) -> (r: u64) [r = %sp] { mov %sp, a }
