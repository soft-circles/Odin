#+build arm64
package asm_check_valid

// `#clobber` of an untracked register reaches LLVM as ~{x9} and nothing else (no "<reg>").
copy_via_x9 :: asm(a: u64) -> (r: u64) [#clobber %x9] { mov %x9, a; mov r, %x9 }

// %daif's register code needs more than 16 bits. Truncated, it matched no literal in
// the body and -vet reported 'v' as unused.
read_daif :: asm(v: u64) -> (r: u64) [v = %daif] { mrs r, %daif }

// A literal write to an output's pinned register assigns the output.
pinned_out :: asm(a: u64) -> (r: u64) [r = %x9] { mov %x9, a }

// Writing a tied input, or a scratch copy, is fine; so is a tied post-index base.
tied_add  :: asm(a: u64) -> (r: u64) [a -> r] { add a, a, 1 }
scratch   :: asm(a: u64) -> (r: u64) [s: u64] { add s, a, 1; mov r, s }
tied_post :: asm(p: ^u64) -> (r: u64, q: ^u64) [p -> q] { ldr r, #post [p + 8] }

@(export) arm64_copy_via_x9 :: proc "contextless" (a: u64) -> u64 { return copy_via_x9(a) }
@(export) arm64_read_daif   :: proc "contextless" (v: u64) -> u64 { return read_daif(v) }
@(export) arm64_pinned_out  :: proc "contextless" (a: u64) -> u64 { return pinned_out(a) }
@(export) arm64_inputs      :: proc "contextless" (a: u64, p: ^u64) -> u64 {
	r, _ := tied_post(p)
	return tied_add(a) + scratch(a) + r
}
