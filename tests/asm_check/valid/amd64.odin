#+build amd64
package asm_check_valid

// `#clobber` of an untracked register reaches LLVM as ~{r12} and nothing else (no "<reg>").
copy_via_r12 :: asm(a: u64) -> (r: u64) [#clobber %r12] { mov %r12, a; mov r, %r12 }

// cqo reads the input through its pinned register; under -vet that is a use.
sign_hi :: asm(a: i64) -> (hi: i64) [a = %rax, hi = %rdx] { cqo }

// A literal write to an output's pinned register assigns the output.
pinned_out :: asm(a: u64) -> (r: u64) [r = %r12] { mov %r12, a }

// Writing a tied input, or a scratch copy, is fine.
tied_add  :: asm(a: u64) -> (r: u64) [a -> r] { add a, 1 }
tied_out  :: asm(a: u64) -> (r: u64) [a -> r] { add r, 1 }
scratch   :: asm(a: u64) -> (r: u64) [s: u64] { mov s, a; add s, 1; mov r, s }
tied_pin  :: asm(a: u64) -> (r: u64) [a -> r = %rcx] { add %rcx, 1 }

@(export) amd64_copy_via_r12 :: proc "contextless" (a: u64) -> u64 { return copy_via_r12(a) }
@(export) amd64_sign_hi      :: proc "contextless" (a: i64) -> i64 { return sign_hi(a) }
@(export) amd64_pinned_out   :: proc "contextless" (a: u64) -> u64 { return pinned_out(a) }
@(export) amd64_inputs       :: proc "contextless" (a: u64) -> u64 { return tied_add(a) + tied_out(a) + scratch(a) + tied_pin(a) }
