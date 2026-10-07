#+build riscv64
package asm_check_valid

// Pseudo-instructions count their operands against the alias, not the target form.
copy_mv    :: asm(a: u64) -> (r: u64) { mv r, a }
invert_not :: asm(a: u64) -> (r: u64) { not r, a }
do_nothing :: asm() { nop }

// `#clobber` of an untracked register reaches LLVM as ~{t0} and nothing else (no "<reg>").
copy_via_t0 :: asm(a: u64) -> (r: u64) [#clobber %t0] { addi %t0, a, 0; addi r, %t0, 0 }

// Writing a tied input, or a scratch copy, is fine.
tied_add :: asm(a: u64) -> (r: u64) [a -> r] { addi a, a, 1 }
scratch  :: asm(a: u64) -> (r: u64) [s: u64] { addi s, a, 1; mv r, s }

@(export) riscv64_copy_mv     :: proc "contextless" (a: u64) -> u64 { return copy_mv(a) }
@(export) riscv64_invert_not  :: proc "contextless" (a: u64) -> u64 { return invert_not(a) }
@(export) riscv64_do_nothing  :: proc "contextless" () { do_nothing() }
@(export) riscv64_copy_via_t0 :: proc "contextless" (a: u64) -> u64 { return copy_via_t0(a) }
@(export) riscv64_inputs      :: proc "contextless" (a: u64) -> u64 { return tied_add(a) + scratch(a) }
