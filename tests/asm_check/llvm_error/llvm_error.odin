#+build amd64
package asm_check_llvm_error

// Every allocatable general-purpose register is clobbered, so LLVM has none left for the
// output and reports "inline assembly requires more registers than available" while it
// emits the object. The checker has nothing to object to, so only the backend can fail
// the build. With -use-single-module the object is emitted on the non-threaded path,
// which used to print the error and still exit 0.
spill :: asm(a: u64) -> (r: u64) [#clobber %rax, #clobber %rbx, #clobber %rcx, #clobber %rdx, #clobber %rsi, #clobber %rdi, #clobber %r8, #clobber %r9, #clobber %r10, #clobber %r11, #clobber %r12, #clobber %r13, #clobber %r14, #clobber %r15, #clobber %rbp] { mov r, a }

@(export) llvm_error_spill :: proc "contextless" (a: u64) -> u64 { return spill(a) }
