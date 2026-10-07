package n64_asm_coverage

// One exported proc per VR4300 mnemonic (or form), each wrapping a single asm
// template. GPR operands are pinned (inputs %a0, %a1, output %v0) so the test can
// check whole instruction words; FPR operands are left to the allocator.
// The ENCODINGS table in test_n64_asm.py lists the words each proc must
// contain: keep the two in sync.

@(export) enc_add :: proc "contextless" (a: u32, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) [a = %a0, b = %a1, r = %v0] { add r, a, b }
	return t(a, b)
}

@(export) enc_addu :: proc "contextless" (a: u32, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) [a = %a0, b = %a1, r = %v0] { addu r, a, b }
	return t(a, b)
}

@(export) enc_sub :: proc "contextless" (a: u32, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) [a = %a0, b = %a1, r = %v0] { sub r, a, b }
	return t(a, b)
}

@(export) enc_subu :: proc "contextless" (a: u32, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) [a = %a0, b = %a1, r = %v0] { subu r, a, b }
	return t(a, b)
}

@(export) enc_and :: proc "contextless" (a: u64, b: u64) -> u64 {
	t :: asm(a: u64, b: u64) -> (r: u64) [a = %a0, b = %a1, r = %v0] { and r, a, b }
	return t(a, b)
}

@(export) enc_or :: proc "contextless" (a: u64, b: u64) -> u64 {
	t :: asm(a: u64, b: u64) -> (r: u64) [a = %a0, b = %a1, r = %v0] { or r, a, b }
	return t(a, b)
}

@(export) enc_xor :: proc "contextless" (a: u64, b: u64) -> u64 {
	t :: asm(a: u64, b: u64) -> (r: u64) [a = %a0, b = %a1, r = %v0] { xor r, a, b }
	return t(a, b)
}

@(export) enc_nor :: proc "contextless" (a: u64, b: u64) -> u64 {
	t :: asm(a: u64, b: u64) -> (r: u64) [a = %a0, b = %a1, r = %v0] { nor r, a, b }
	return t(a, b)
}

@(export) enc_slt :: proc "contextless" (a: u64, b: u64) -> u64 {
	t :: asm(a: u64, b: u64) -> (r: u64) [a = %a0, b = %a1, r = %v0] { slt r, a, b }
	return t(a, b)
}

@(export) enc_sltu :: proc "contextless" (a: u64, b: u64) -> u64 {
	t :: asm(a: u64, b: u64) -> (r: u64) [a = %a0, b = %a1, r = %v0] { sltu r, a, b }
	return t(a, b)
}

@(export) enc_dadd :: proc "contextless" (a: u64, b: u64) -> u64 {
	t :: asm(a: u64, b: u64) -> (r: u64) [a = %a0, b = %a1, r = %v0] { dadd r, a, b }
	return t(a, b)
}

@(export) enc_daddu :: proc "contextless" (a: u64, b: u64) -> u64 {
	t :: asm(a: u64, b: u64) -> (r: u64) [a = %a0, b = %a1, r = %v0] { daddu r, a, b }
	return t(a, b)
}

@(export) enc_dsub :: proc "contextless" (a: u64, b: u64) -> u64 {
	t :: asm(a: u64, b: u64) -> (r: u64) [a = %a0, b = %a1, r = %v0] { dsub r, a, b }
	return t(a, b)
}

@(export) enc_dsubu :: proc "contextless" (a: u64, b: u64) -> u64 {
	t :: asm(a: u64, b: u64) -> (r: u64) [a = %a0, b = %a1, r = %v0] { dsubu r, a, b }
	return t(a, b)
}

@(export) enc_addi :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] { addi r, a, -3 }
	return t(a)
}

@(export) enc_addiu :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] { addiu r, a, 1234 }
	return t(a)
}

@(export) enc_slti :: proc "contextless" (a: u64) -> u64 {
	t :: asm(a: u64) -> (r: u64) [a = %a0, r = %v0] { slti r, a, -5 }
	return t(a)
}

@(export) enc_sltiu :: proc "contextless" (a: u64) -> u64 {
	t :: asm(a: u64) -> (r: u64) [a = %a0, r = %v0] { sltiu r, a, 77 }
	return t(a)
}

@(export) enc_andi :: proc "contextless" (a: u64) -> u64 {
	t :: asm(a: u64) -> (r: u64) [a = %a0, r = %v0] { andi r, a, 61680 }
	return t(a)
}

@(export) enc_ori :: proc "contextless" (a: u64) -> u64 {
	t :: asm(a: u64) -> (r: u64) [a = %a0, r = %v0] { ori r, a, 32769 }
	return t(a)
}

@(export) enc_xori :: proc "contextless" (a: u64) -> u64 {
	t :: asm(a: u64) -> (r: u64) [a = %a0, r = %v0] { xori r, a, 65535 }
	return t(a)
}

@(export) enc_daddi :: proc "contextless" (a: u64) -> u64 {
	t :: asm(a: u64) -> (r: u64) [a = %a0, r = %v0] { daddi r, a, -7 }
	return t(a)
}

@(export) enc_daddiu :: proc "contextless" (a: u64) -> u64 {
	t :: asm(a: u64) -> (r: u64) [a = %a0, r = %v0] { daddiu r, a, 32767 }
	return t(a)
}

@(export) enc_lui :: proc "contextless" () -> u32 {
	t :: asm() -> (r: u32) [r = %v0] { lui r, 0x1234 }
	return t()
}

@(export) enc_sll :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] { sll r, a, 3 }
	return t(a)
}

@(export) enc_srl :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] { srl r, a, 5 }
	return t(a)
}

@(export) enc_sra :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] { sra r, a, 7 }
	return t(a)
}

@(export) enc_dsll :: proc "contextless" (a: u64) -> u64 {
	t :: asm(a: u64) -> (r: u64) [a = %a0, r = %v0] { dsll r, a, 9 }
	return t(a)
}

@(export) enc_dsrl :: proc "contextless" (a: u64) -> u64 {
	t :: asm(a: u64) -> (r: u64) [a = %a0, r = %v0] { dsrl r, a, 11 }
	return t(a)
}

@(export) enc_dsra :: proc "contextless" (a: u64) -> u64 {
	t :: asm(a: u64) -> (r: u64) [a = %a0, r = %v0] { dsra r, a, 13 }
	return t(a)
}

@(export) enc_dsll32 :: proc "contextless" (a: u64) -> u64 {
	t :: asm(a: u64) -> (r: u64) [a = %a0, r = %v0] { dsll32 r, a, 1 }
	return t(a)
}

@(export) enc_dsrl32 :: proc "contextless" (a: u64) -> u64 {
	t :: asm(a: u64) -> (r: u64) [a = %a0, r = %v0] { dsrl32 r, a, 2 }
	return t(a)
}

@(export) enc_dsra32 :: proc "contextless" (a: u64) -> u64 {
	t :: asm(a: u64) -> (r: u64) [a = %a0, r = %v0] { dsra32 r, a, 31 }
	return t(a)
}

@(export) enc_sllv :: proc "contextless" (a: u32, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) [a = %a0, b = %a1, r = %v0] { sllv r, a, b }
	return t(a, b)
}

@(export) enc_srlv :: proc "contextless" (a: u32, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) [a = %a0, b = %a1, r = %v0] { srlv r, a, b }
	return t(a, b)
}

@(export) enc_srav :: proc "contextless" (a: u32, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) [a = %a0, b = %a1, r = %v0] { srav r, a, b }
	return t(a, b)
}

@(export) enc_dsllv :: proc "contextless" (a: u64, b: u32) -> u64 {
	t :: asm(a: u64, b: u32) -> (r: u64) [a = %a0, b = %a1, r = %v0] { dsllv r, a, b }
	return t(a, b)
}

@(export) enc_dsrlv :: proc "contextless" (a: u64, b: u32) -> u64 {
	t :: asm(a: u64, b: u32) -> (r: u64) [a = %a0, b = %a1, r = %v0] { dsrlv r, a, b }
	return t(a, b)
}

@(export) enc_dsrav :: proc "contextless" (a: u64, b: u32) -> u64 {
	t :: asm(a: u64, b: u32) -> (r: u64) [a = %a0, b = %a1, r = %v0] { dsrav r, a, b }
	return t(a, b)
}

@(export) enc_mult :: proc "contextless" (a: u32, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) [a = %a0, b = %a1, r = %v0] {
		mult a, b
		mflo r
	}
	return t(a, b)
}

@(export) enc_multu :: proc "contextless" (a: u32, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) [a = %a0, b = %a1, r = %v0] {
		multu a, b
		mflo r
	}
	return t(a, b)
}

@(export) enc_div :: proc "contextless" (a: u32, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) [a = %a0, b = %a1, r = %v0] {
		div a, b
		mflo r
	}
	return t(a, b)
}

@(export) enc_divu :: proc "contextless" (a: u32, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) [a = %a0, b = %a1, r = %v0] {
		divu a, b
		mflo r
	}
	return t(a, b)
}

@(export) enc_dmult :: proc "contextless" (a: u64, b: u64) -> u64 {
	t :: asm(a: u64, b: u64) -> (r: u64) [a = %a0, b = %a1, r = %v0] {
		dmult a, b
		mflo r
	}
	return t(a, b)
}

@(export) enc_dmultu :: proc "contextless" (a: u64, b: u64) -> u64 {
	t :: asm(a: u64, b: u64) -> (r: u64) [a = %a0, b = %a1, r = %v0] {
		dmultu a, b
		mflo r
	}
	return t(a, b)
}

@(export) enc_ddiv :: proc "contextless" (a: u64, b: u64) -> u64 {
	t :: asm(a: u64, b: u64) -> (r: u64) [a = %a0, b = %a1, r = %v0] {
		ddiv a, b
		mflo r
	}
	return t(a, b)
}

@(export) enc_ddivu :: proc "contextless" (a: u64, b: u64) -> u64 {
	t :: asm(a: u64, b: u64) -> (r: u64) [a = %a0, b = %a1, r = %v0] {
		ddivu a, b
		mflo r
	}
	return t(a, b)
}

@(export) enc_mfhi :: proc "contextless" (a: u32, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) [a = %a0, b = %a1, r = %v0] {
		multu a, b
		mfhi r
	}
	return t(a, b)
}

@(export) enc_mflo :: proc "contextless" (a: u32, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) [a = %a0, b = %a1, r = %v0] {
		multu a, b
		mflo r
	}
	return t(a, b)
}

@(export) enc_mthi :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		mthi a
		mfhi r
	}
	return t(a)
}

@(export) enc_mtlo :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		mtlo a
		mflo r
	}
	return t(a)
}

@(export) enc_move :: proc "contextless" (a: u64) -> u64 {
	t :: asm(a: u64) -> (r: u64) [a = %a0, r = %v0] { move r, a }
	return t(a)
}

@(export) enc_not :: proc "contextless" (a: u64) -> u64 {
	t :: asm(a: u64) -> (r: u64) [a = %a0, r = %v0] { not r, a }
	return t(a)
}

@(export) enc_negu :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] { negu r, a }
	return t(a)
}

@(export) enc_dnegu :: proc "contextless" (a: u64) -> u64 {
	t :: asm(a: u64) -> (r: u64) [a = %a0, r = %v0] { dnegu r, a }
	return t(a)
}

@(export) enc_li :: proc "contextless" () -> u32 {
	t :: asm() -> (r: u32) [r = %v0] { li r, 0x12345678 }
	return t()
}

@(export) enc_li_small :: proc "contextless" () -> u32 {
	t :: asm() -> (r: u32) [r = %v0] { li r, -5 }
	return t()
}

@(export) enc_li_u16 :: proc "contextless" () -> u32 {
	t :: asm() -> (r: u32) [r = %v0] { li r, 0xFFFF }
	return t()
}

@(export) enc_li_hi :: proc "contextless" () -> u32 {
	t :: asm() -> (r: u32) [r = %v0] { li r, 0x80000000 }
	return t()
}

@(export) enc_lb :: proc "contextless" (p: rawptr) -> i32 {
	t :: asm(p: rawptr) -> (r: i32) [p = %a0, r = %v0] { lb r, [p + 8] }
	return t(p)
}

@(export) enc_lbu :: proc "contextless" (p: rawptr) -> u32 {
	t :: asm(p: rawptr) -> (r: u32) [p = %a0, r = %v0] { lbu r, [p + 8] }
	return t(p)
}

@(export) enc_lh :: proc "contextless" (p: rawptr) -> i32 {
	t :: asm(p: rawptr) -> (r: i32) [p = %a0, r = %v0] { lh r, [p + 8] }
	return t(p)
}

@(export) enc_lhu :: proc "contextless" (p: rawptr) -> u32 {
	t :: asm(p: rawptr) -> (r: u32) [p = %a0, r = %v0] { lhu r, [p + 8] }
	return t(p)
}

@(export) enc_lw :: proc "contextless" (p: rawptr) -> u32 {
	t :: asm(p: rawptr) -> (r: u32) [p = %a0, r = %v0] { lw r, [p + 8] }
	return t(p)
}

@(export) enc_lwu :: proc "contextless" (p: rawptr) -> u64 {
	t :: asm(p: rawptr) -> (r: u64) [p = %a0, r = %v0] { lwu r, [p + 8] }
	return t(p)
}

@(export) enc_ld :: proc "contextless" (p: rawptr) -> u64 {
	t :: asm(p: rawptr) -> (r: u64) [p = %a0, r = %v0] { ld r, [p + 8] }
	return t(p)
}

@(export) enc_lwl :: proc "contextless" (p: rawptr) -> u32 {
	t :: asm(p: rawptr) -> (r: u32) [p = %a0, r = %v0] { lwl r, [p + 8] }
	return t(p)
}

@(export) enc_lwr :: proc "contextless" (p: rawptr) -> u32 {
	t :: asm(p: rawptr) -> (r: u32) [p = %a0, r = %v0] { lwr r, [p + 8] }
	return t(p)
}

@(export) enc_ldl :: proc "contextless" (p: rawptr) -> u64 {
	t :: asm(p: rawptr) -> (r: u64) [p = %a0, r = %v0] { ldl r, [p + 8] }
	return t(p)
}

@(export) enc_ldr :: proc "contextless" (p: rawptr) -> u64 {
	t :: asm(p: rawptr) -> (r: u64) [p = %a0, r = %v0] { ldr r, [p + 8] }
	return t(p)
}

@(export) enc_ll :: proc "contextless" (p: rawptr) -> u32 {
	t :: asm(p: rawptr) -> (r: u32) [p = %a0, r = %v0] { ll r, [p + 8] }
	return t(p)
}

@(export) enc_lld :: proc "contextless" (p: rawptr) -> u64 {
	t :: asm(p: rawptr) -> (r: u64) [p = %a0, r = %v0] { lld r, [p + 8] }
	return t(p)
}

@(export) enc_sb :: proc "contextless" (p: rawptr, v: u32) {
	t :: asm(p: rawptr, v: u32) [p = %a0, v = %a1] { sb v, [p - 8] }
	t(p, v)
}

@(export) enc_sh :: proc "contextless" (p: rawptr, v: u32) {
	t :: asm(p: rawptr, v: u32) [p = %a0, v = %a1] { sh v, [p - 8] }
	t(p, v)
}

@(export) enc_sw :: proc "contextless" (p: rawptr, v: u32) {
	t :: asm(p: rawptr, v: u32) [p = %a0, v = %a1] { sw v, [p - 8] }
	t(p, v)
}

@(export) enc_sd :: proc "contextless" (p: rawptr, v: u64) {
	t :: asm(p: rawptr, v: u64) [p = %a0, v = %a1] { sd v, [p - 8] }
	t(p, v)
}

@(export) enc_swl :: proc "contextless" (p: rawptr, v: u32) {
	t :: asm(p: rawptr, v: u32) [p = %a0, v = %a1] { swl v, [p - 8] }
	t(p, v)
}

@(export) enc_swr :: proc "contextless" (p: rawptr, v: u32) {
	t :: asm(p: rawptr, v: u32) [p = %a0, v = %a1] { swr v, [p - 8] }
	t(p, v)
}

@(export) enc_sdl :: proc "contextless" (p: rawptr, v: u64) {
	t :: asm(p: rawptr, v: u64) [p = %a0, v = %a1] { sdl v, [p - 8] }
	t(p, v)
}

@(export) enc_sdr :: proc "contextless" (p: rawptr, v: u64) {
	t :: asm(p: rawptr, v: u64) [p = %a0, v = %a1] { sdr v, [p - 8] }
	t(p, v)
}

@(export) enc_sc :: proc "contextless" (p: rawptr, v: u32) -> u32 {
	t :: asm(p: rawptr, v: u32) -> (r: u32) [p = %a0, v = %a1, r = %v0] {
		move r, v
		sc r, [p + 8]
	}
	return t(p, v)
}

@(export) enc_scd :: proc "contextless" (p: rawptr, v: u64) -> u64 {
	t :: asm(p: rawptr, v: u64) -> (r: u64) [p = %a0, v = %a1, r = %v0] {
		move r, v
		scd r, [p + 8]
	}
	return t(p, v)
}

@(export) enc_beq :: proc "contextless" (a: u32, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) [a = %a0, b = %a1, r = %v0] {
		move r, a
		beq a, b, .d
		addiu r, r, 1
	.d:
	}
	return t(a, b)
}

@(export) enc_bne :: proc "contextless" (a: u32, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) [a = %a0, b = %a1, r = %v0] {
		move r, a
		bne a, b, .d
		addiu r, r, 1
	.d:
	}
	return t(a, b)
}

@(export) enc_beql :: proc "contextless" (a: u32, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) [a = %a0, b = %a1, r = %v0] {
		move r, a
		beql a, b, .d
		addiu r, r, 1
	.d:
	}
	return t(a, b)
}

@(export) enc_bnel :: proc "contextless" (a: u32, b: u32) -> u32 {
	t :: asm(a: u32, b: u32) -> (r: u32) [a = %a0, b = %a1, r = %v0] {
		move r, a
		bnel a, b, .d
		addiu r, r, 1
	.d:
	}
	return t(a, b)
}

@(export) enc_blez :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		blez a, .d
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_bgtz :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		bgtz a, .d
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_blezl :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		blezl a, .d
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_bgtzl :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		bgtzl a, .d
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_bltz :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		bltz a, .d
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_bgez :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		bgez a, .d
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_bltzl :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		bltzl a, .d
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_bgezl :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		bgezl a, .d
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_bltzal :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		bltzal a, .d
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_bgezal :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		bgezal a, .d
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_bltzall :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		bltzall a, .d
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_bgezall :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		bgezall a, .d
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_beqz :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		beqz a, .d
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_bnez :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		bnez a, .d
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_beqzl :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		beqzl a, .d
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_bnezl :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		bnezl a, .d
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_bal :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		bal .d
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_b :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		beqz a, .s
		b .d
	.s:
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_j :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		beqz a, .s
		j .d
	.s:
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_jal :: proc "contextless" (a: u32) -> u32 {
	t :: asm(a: u32) -> (r: u32) [a = %a0, r = %v0] {
		move r, a
		jal .d
		addiu r, r, 1
	.d:
	}
	return t(a)
}

@(export) enc_jr :: proc "contextless" (a: u32) -> ! {
	t :: asm(a: u32) -> ! [a = %a0] { jr a }
	t(a)
}

@(export) enc_jalr :: proc "contextless" (a: rawptr) {
	t :: asm(a: rawptr) [a = %a0] { jalr a }
	t(a)
}

@(export) enc_jalr_rd :: proc "contextless" (a: rawptr) -> u32 {
	t :: asm(a: rawptr) -> (r: u32) [a = %a0, r = %v0] { jalr r, a }
	return t(a)
}

@(export) enc_teq :: proc "contextless" (a: u32, b: u32) {
	t :: asm(a: u32, b: u32) [a = %a0, b = %a1] { teq a, b }
	t(a, b)
}

@(export) enc_teq_code :: proc "contextless" (a: u32, b: u32) {
	t :: asm(a: u32, b: u32) [a = %a0, b = %a1] { teq a, b, 7 }
	t(a, b)
}

@(export) enc_tne :: proc "contextless" (a: u32, b: u32) {
	t :: asm(a: u32, b: u32) [a = %a0, b = %a1] { tne a, b }
	t(a, b)
}

@(export) enc_tne_code :: proc "contextless" (a: u32, b: u32) {
	t :: asm(a: u32, b: u32) [a = %a0, b = %a1] { tne a, b, 7 }
	t(a, b)
}

@(export) enc_tge :: proc "contextless" (a: u32, b: u32) {
	t :: asm(a: u32, b: u32) [a = %a0, b = %a1] { tge a, b }
	t(a, b)
}

@(export) enc_tge_code :: proc "contextless" (a: u32, b: u32) {
	t :: asm(a: u32, b: u32) [a = %a0, b = %a1] { tge a, b, 7 }
	t(a, b)
}

@(export) enc_tgeu :: proc "contextless" (a: u32, b: u32) {
	t :: asm(a: u32, b: u32) [a = %a0, b = %a1] { tgeu a, b }
	t(a, b)
}

@(export) enc_tgeu_code :: proc "contextless" (a: u32, b: u32) {
	t :: asm(a: u32, b: u32) [a = %a0, b = %a1] { tgeu a, b, 7 }
	t(a, b)
}

@(export) enc_tlt :: proc "contextless" (a: u32, b: u32) {
	t :: asm(a: u32, b: u32) [a = %a0, b = %a1] { tlt a, b }
	t(a, b)
}

@(export) enc_tlt_code :: proc "contextless" (a: u32, b: u32) {
	t :: asm(a: u32, b: u32) [a = %a0, b = %a1] { tlt a, b, 7 }
	t(a, b)
}

@(export) enc_tltu :: proc "contextless" (a: u32, b: u32) {
	t :: asm(a: u32, b: u32) [a = %a0, b = %a1] { tltu a, b }
	t(a, b)
}

@(export) enc_tltu_code :: proc "contextless" (a: u32, b: u32) {
	t :: asm(a: u32, b: u32) [a = %a0, b = %a1] { tltu a, b, 7 }
	t(a, b)
}

@(export) enc_teqi :: proc "contextless" (a: u32) {
	t :: asm(a: u32) [a = %a0] { teqi a, -9 }
	t(a)
}

@(export) enc_tnei :: proc "contextless" (a: u32) {
	t :: asm(a: u32) [a = %a0] { tnei a, 9 }
	t(a)
}

@(export) enc_tgei :: proc "contextless" (a: u32) {
	t :: asm(a: u32) [a = %a0] { tgei a, -32768 }
	t(a)
}

@(export) enc_tgeiu :: proc "contextless" (a: u32) {
	t :: asm(a: u32) [a = %a0] { tgeiu a, 32767 }
	t(a)
}

@(export) enc_tlti :: proc "contextless" (a: u32) {
	t :: asm(a: u32) [a = %a0] { tlti a, 100 }
	t(a)
}

@(export) enc_tltiu :: proc "contextless" (a: u32) {
	t :: asm(a: u32) [a = %a0] { tltiu a, -1 }
	t(a)
}

@(export) enc_nop :: proc "contextless" () {
	t :: asm() { nop }
	t()
}

@(export) enc_ssnop :: proc "contextless" () {
	t :: asm() { ssnop }
	t()
}

@(export) enc_sync :: proc "contextless" () {
	t :: asm() { sync }
	t()
}

@(export) enc_syscall :: proc "contextless" () {
	t :: asm() { syscall }
	t()
}

@(export) enc_syscall_code :: proc "contextless" () {
	t :: asm() { syscall 0x1234 }
	t()
}

@(export) enc_break :: proc "contextless" () {
	t :: asm() { break }
	t()
}

@(export) enc_break_code :: proc "contextless" () {
	t :: asm() { break 7 }
	t()
}

@(export) enc_break_code2 :: proc "contextless" () {
	t :: asm() { break 7, 3 }
	t()
}

@(export) enc_cache :: proc "contextless" (p: rawptr) {
	t :: asm(p: rawptr) [p = %a0] {
		cache 0x00, [p + 16]
		cache 0x04, [p + 16]
		cache 0x08, [p + 16]
		cache 0x10, [p + 16]
		cache 0x14, [p + 16]
		cache 0x18, [p + 16]
		cache 0x01, [p + 16]
		cache 0x05, [p + 16]
		cache 0x09, [p + 16]
		cache 0x0D, [p + 16]
		cache 0x11, [p + 16]
		cache 0x15, [p + 16]
		cache 0x19, [p + 16]
	}
	t(p)
}

@(export) enc_mfc0 :: proc "contextless" () -> u32 {
	t :: asm() -> (r: u32) [r = %v0] {
		mfc0 r, %c0_index
		mfc0 r, %c0_random
		mfc0 r, %c0_entrylo0
		mfc0 r, %c0_entrylo1
		mfc0 r, %c0_context
		mfc0 r, %c0_pagemask
		mfc0 r, %c0_wired
		mfc0 r, %c0_badvaddr
		mfc0 r, %c0_count
		mfc0 r, %c0_entryhi
		mfc0 r, %c0_compare
		mfc0 r, %c0_status
		mfc0 r, %c0_cause
		mfc0 r, %c0_epc
		mfc0 r, %c0_prid
		mfc0 r, %c0_config
		mfc0 r, %c0_lladdr
		mfc0 r, %c0_watchlo
		mfc0 r, %c0_watchhi
		mfc0 r, %c0_xcontext
		mfc0 r, %c0_perr
		mfc0 r, %c0_cacheerr
		mfc0 r, %c0_taglo
		mfc0 r, %c0_taghi
		mfc0 r, %c0_errorepc
	}
	return t()
}

@(export) enc_mtc0 :: proc "contextless" (a: u32) {
	t :: asm(a: u32) [a = %a0] {
		mtc0 a, %c0_index
		mtc0 a, %c0_random
		mtc0 a, %c0_entrylo0
		mtc0 a, %c0_entrylo1
		mtc0 a, %c0_context
		mtc0 a, %c0_pagemask
		mtc0 a, %c0_wired
		mtc0 a, %c0_badvaddr
		mtc0 a, %c0_count
		mtc0 a, %c0_entryhi
		mtc0 a, %c0_compare
		mtc0 a, %c0_status
		mtc0 a, %c0_cause
		mtc0 a, %c0_epc
		mtc0 a, %c0_prid
		mtc0 a, %c0_config
		mtc0 a, %c0_lladdr
		mtc0 a, %c0_watchlo
		mtc0 a, %c0_watchhi
		mtc0 a, %c0_xcontext
		mtc0 a, %c0_perr
		mtc0 a, %c0_cacheerr
		mtc0 a, %c0_taglo
		mtc0 a, %c0_taghi
		mtc0 a, %c0_errorepc
	}
	t(a)
}

@(export) enc_dmfc0 :: proc "contextless" () -> u64 {
	t :: asm() -> (r: u64) [r = %v0] {
		dmfc0 r, %c0_entrylo0
		dmfc0 r, %c0_context
		dmfc0 r, %c0_badvaddr
		dmfc0 r, %c0_entryhi
		dmfc0 r, %c0_epc
		dmfc0 r, %c0_xcontext
		dmfc0 r, %c0_errorepc
	}
	return t()
}

@(export) enc_dmtc0 :: proc "contextless" (a: u64) {
	t :: asm(a: u64) [a = %a0] {
		dmtc0 a, %c0_entrylo0
		dmtc0 a, %c0_context
		dmtc0 a, %c0_badvaddr
		dmtc0 a, %c0_entryhi
		dmtc0 a, %c0_epc
		dmtc0 a, %c0_xcontext
		dmtc0 a, %c0_errorepc
	}
	t(a)
}

@(export) enc_tlbr :: proc "contextless" () {
	t :: asm() { tlbr }
	t()
}

@(export) enc_tlbwi :: proc "contextless" () {
	t :: asm() { tlbwi }
	t()
}

@(export) enc_tlbwr :: proc "contextless" () {
	t :: asm() { tlbwr }
	t()
}

@(export) enc_tlbp :: proc "contextless" () {
	t :: asm() { tlbp }
	t()
}

@(export) enc_eret :: proc "contextless" () -> ! {
	t :: asm() -> ! { eret }
	t()
}

@(export) enc_mfc1 :: proc "contextless" (f: f32) -> u32 {
	t :: asm(f: f32) -> (r: u32) [r = %v0] { mfc1 r, f }
	return t(f)
}

@(export) enc_mtc1 :: proc "contextless" (a: u32) -> f32 {
	t :: asm(a: u32) -> (r: f32) [a = %a0] { mtc1 a, r }
	return t(a)
}

@(export) enc_dmfc1 :: proc "contextless" (f: f64) -> u64 {
	t :: asm(f: f64) -> (r: u64) [r = %v0] { dmfc1 r, f }
	return t(f)
}

@(export) enc_dmtc1 :: proc "contextless" (a: u64) -> f64 {
	t :: asm(a: u64) -> (r: f64) [a = %a0] { dmtc1 a, r }
	return t(a)
}

@(export) enc_cfc1 :: proc "contextless" () -> u32 {
	t :: asm() -> (r: u32) [r = %v0] {
		cfc1 r, %fcr0
		cfc1 r, %fcr31
	}
	return t()
}

@(export) enc_ctc1 :: proc "contextless" (a: u32) {
	t :: asm(a: u32) [a = %a0] { ctc1 a, %fcr31 }
	t(a)
}

@(export) enc_lwc1 :: proc "contextless" (p: rawptr) -> f32 {
	t :: asm(p: rawptr) -> (r: f32) [p = %a0] { lwc1 r, [p + 8] }
	return t(p)
}

@(export) enc_ldc1 :: proc "contextless" (p: rawptr) -> f64 {
	t :: asm(p: rawptr) -> (r: f64) [p = %a0] { ldc1 r, [p + 8] }
	return t(p)
}

@(export) enc_swc1 :: proc "contextless" (p: rawptr, v: f32) {
	t :: asm(p: rawptr, v: f32) [p = %a0] { swc1 v, [p - 8] }
	t(p, v)
}

@(export) enc_sdc1 :: proc "contextless" (p: rawptr, v: f64) {
	t :: asm(p: rawptr, v: f64) [p = %a0] { sdc1 v, [p - 8] }
	t(p, v)
}

@(export) enc_add_s :: proc "contextless" (a: f32, b: f32) -> f32 {
	t :: asm(a: f32, b: f32) -> (r: f32) { add_s r, a, b }
	return t(a, b)
}

@(export) enc_add_d :: proc "contextless" (a: f64, b: f64) -> f64 {
	t :: asm(a: f64, b: f64) -> (r: f64) { add_d r, a, b }
	return t(a, b)
}

@(export) enc_sub_s :: proc "contextless" (a: f32, b: f32) -> f32 {
	t :: asm(a: f32, b: f32) -> (r: f32) { sub_s r, a, b }
	return t(a, b)
}

@(export) enc_sub_d :: proc "contextless" (a: f64, b: f64) -> f64 {
	t :: asm(a: f64, b: f64) -> (r: f64) { sub_d r, a, b }
	return t(a, b)
}

@(export) enc_mul_s :: proc "contextless" (a: f32, b: f32) -> f32 {
	t :: asm(a: f32, b: f32) -> (r: f32) { mul_s r, a, b }
	return t(a, b)
}

@(export) enc_mul_d :: proc "contextless" (a: f64, b: f64) -> f64 {
	t :: asm(a: f64, b: f64) -> (r: f64) { mul_d r, a, b }
	return t(a, b)
}

@(export) enc_div_s :: proc "contextless" (a: f32, b: f32) -> f32 {
	t :: asm(a: f32, b: f32) -> (r: f32) { div_s r, a, b }
	return t(a, b)
}

@(export) enc_div_d :: proc "contextless" (a: f64, b: f64) -> f64 {
	t :: asm(a: f64, b: f64) -> (r: f64) { div_d r, a, b }
	return t(a, b)
}

@(export) enc_sqrt_s :: proc "contextless" (a: f32) -> f32 {
	t :: asm(a: f32) -> (r: f32) { sqrt_s r, a }
	return t(a)
}

@(export) enc_sqrt_d :: proc "contextless" (a: f64) -> f64 {
	t :: asm(a: f64) -> (r: f64) { sqrt_d r, a }
	return t(a)
}

@(export) enc_abs_s :: proc "contextless" (a: f32) -> f32 {
	t :: asm(a: f32) -> (r: f32) { abs_s r, a }
	return t(a)
}

@(export) enc_abs_d :: proc "contextless" (a: f64) -> f64 {
	t :: asm(a: f64) -> (r: f64) { abs_d r, a }
	return t(a)
}

@(export) enc_mov_s :: proc "contextless" (a: f32) -> f32 {
	t :: asm(a: f32) -> (r: f32) { mov_s r, a }
	return t(a)
}

@(export) enc_mov_d :: proc "contextless" (a: f64) -> f64 {
	t :: asm(a: f64) -> (r: f64) { mov_d r, a }
	return t(a)
}

@(export) enc_neg_s :: proc "contextless" (a: f32) -> f32 {
	t :: asm(a: f32) -> (r: f32) { neg_s r, a }
	return t(a)
}

@(export) enc_neg_d :: proc "contextless" (a: f64) -> f64 {
	t :: asm(a: f64) -> (r: f64) { neg_d r, a }
	return t(a)
}

@(export) enc_cvt_s_d :: proc "contextless" (a: f64) -> f32 {
	t :: asm(a: f64) -> (r: f32) { cvt_s_d r, a }
	return t(a)
}

@(export) enc_cvt_s_w :: proc "contextless" (a: f32) -> f32 {
	t :: asm(a: f32) -> (r: f32) { cvt_s_w r, a }
	return t(a)
}

@(export) enc_cvt_s_l :: proc "contextless" (a: f64) -> f32 {
	t :: asm(a: f64) -> (r: f32) { cvt_s_l r, a }
	return t(a)
}

@(export) enc_cvt_d_s :: proc "contextless" (a: f32) -> f64 {
	t :: asm(a: f32) -> (r: f64) { cvt_d_s r, a }
	return t(a)
}

@(export) enc_cvt_d_w :: proc "contextless" (a: f32) -> f64 {
	t :: asm(a: f32) -> (r: f64) { cvt_d_w r, a }
	return t(a)
}

@(export) enc_cvt_d_l :: proc "contextless" (a: f64) -> f64 {
	t :: asm(a: f64) -> (r: f64) { cvt_d_l r, a }
	return t(a)
}

@(export) enc_cvt_w_s :: proc "contextless" (a: f32) -> f32 {
	t :: asm(a: f32) -> (r: f32) { cvt_w_s r, a }
	return t(a)
}

@(export) enc_cvt_w_d :: proc "contextless" (a: f64) -> f32 {
	t :: asm(a: f64) -> (r: f32) { cvt_w_d r, a }
	return t(a)
}

@(export) enc_cvt_l_s :: proc "contextless" (a: f32) -> f64 {
	t :: asm(a: f32) -> (r: f64) { cvt_l_s r, a }
	return t(a)
}

@(export) enc_cvt_l_d :: proc "contextless" (a: f64) -> f64 {
	t :: asm(a: f64) -> (r: f64) { cvt_l_d r, a }
	return t(a)
}

@(export) enc_round_l_s :: proc "contextless" (a: f32) -> f64 {
	t :: asm(a: f32) -> (r: f64) { round_l_s r, a }
	return t(a)
}

@(export) enc_round_l_d :: proc "contextless" (a: f64) -> f64 {
	t :: asm(a: f64) -> (r: f64) { round_l_d r, a }
	return t(a)
}

@(export) enc_round_w_s :: proc "contextless" (a: f32) -> f32 {
	t :: asm(a: f32) -> (r: f32) { round_w_s r, a }
	return t(a)
}

@(export) enc_round_w_d :: proc "contextless" (a: f64) -> f32 {
	t :: asm(a: f64) -> (r: f32) { round_w_d r, a }
	return t(a)
}

@(export) enc_trunc_l_s :: proc "contextless" (a: f32) -> f64 {
	t :: asm(a: f32) -> (r: f64) { trunc_l_s r, a }
	return t(a)
}

@(export) enc_trunc_l_d :: proc "contextless" (a: f64) -> f64 {
	t :: asm(a: f64) -> (r: f64) { trunc_l_d r, a }
	return t(a)
}

@(export) enc_trunc_w_s :: proc "contextless" (a: f32) -> f32 {
	t :: asm(a: f32) -> (r: f32) { trunc_w_s r, a }
	return t(a)
}

@(export) enc_trunc_w_d :: proc "contextless" (a: f64) -> f32 {
	t :: asm(a: f64) -> (r: f32) { trunc_w_d r, a }
	return t(a)
}

@(export) enc_ceil_l_s :: proc "contextless" (a: f32) -> f64 {
	t :: asm(a: f32) -> (r: f64) { ceil_l_s r, a }
	return t(a)
}

@(export) enc_ceil_l_d :: proc "contextless" (a: f64) -> f64 {
	t :: asm(a: f64) -> (r: f64) { ceil_l_d r, a }
	return t(a)
}

@(export) enc_ceil_w_s :: proc "contextless" (a: f32) -> f32 {
	t :: asm(a: f32) -> (r: f32) { ceil_w_s r, a }
	return t(a)
}

@(export) enc_ceil_w_d :: proc "contextless" (a: f64) -> f32 {
	t :: asm(a: f64) -> (r: f32) { ceil_w_d r, a }
	return t(a)
}

@(export) enc_floor_l_s :: proc "contextless" (a: f32) -> f64 {
	t :: asm(a: f32) -> (r: f64) { floor_l_s r, a }
	return t(a)
}

@(export) enc_floor_l_d :: proc "contextless" (a: f64) -> f64 {
	t :: asm(a: f64) -> (r: f64) { floor_l_d r, a }
	return t(a)
}

@(export) enc_floor_w_s :: proc "contextless" (a: f32) -> f32 {
	t :: asm(a: f32) -> (r: f32) { floor_w_s r, a }
	return t(a)
}

@(export) enc_floor_w_d :: proc "contextless" (a: f64) -> f32 {
	t :: asm(a: f64) -> (r: f32) { floor_w_d r, a }
	return t(a)
}

@(export) enc_c_f_s :: proc "contextless" (a: f32, b: f32) {
	t :: asm(a: f32, b: f32) { c_f_s a, b }
	t(a, b)
}

@(export) enc_c_un_s :: proc "contextless" (a: f32, b: f32) {
	t :: asm(a: f32, b: f32) { c_un_s a, b }
	t(a, b)
}

@(export) enc_c_eq_s :: proc "contextless" (a: f32, b: f32) {
	t :: asm(a: f32, b: f32) { c_eq_s a, b }
	t(a, b)
}

@(export) enc_c_ueq_s :: proc "contextless" (a: f32, b: f32) {
	t :: asm(a: f32, b: f32) { c_ueq_s a, b }
	t(a, b)
}

@(export) enc_c_olt_s :: proc "contextless" (a: f32, b: f32) {
	t :: asm(a: f32, b: f32) { c_olt_s a, b }
	t(a, b)
}

@(export) enc_c_ult_s :: proc "contextless" (a: f32, b: f32) {
	t :: asm(a: f32, b: f32) { c_ult_s a, b }
	t(a, b)
}

@(export) enc_c_ole_s :: proc "contextless" (a: f32, b: f32) {
	t :: asm(a: f32, b: f32) { c_ole_s a, b }
	t(a, b)
}

@(export) enc_c_ule_s :: proc "contextless" (a: f32, b: f32) {
	t :: asm(a: f32, b: f32) { c_ule_s a, b }
	t(a, b)
}

@(export) enc_c_sf_s :: proc "contextless" (a: f32, b: f32) {
	t :: asm(a: f32, b: f32) { c_sf_s a, b }
	t(a, b)
}

@(export) enc_c_ngle_s :: proc "contextless" (a: f32, b: f32) {
	t :: asm(a: f32, b: f32) { c_ngle_s a, b }
	t(a, b)
}

@(export) enc_c_seq_s :: proc "contextless" (a: f32, b: f32) {
	t :: asm(a: f32, b: f32) { c_seq_s a, b }
	t(a, b)
}

@(export) enc_c_ngl_s :: proc "contextless" (a: f32, b: f32) {
	t :: asm(a: f32, b: f32) { c_ngl_s a, b }
	t(a, b)
}

@(export) enc_c_lt_s :: proc "contextless" (a: f32, b: f32) {
	t :: asm(a: f32, b: f32) { c_lt_s a, b }
	t(a, b)
}

@(export) enc_c_nge_s :: proc "contextless" (a: f32, b: f32) {
	t :: asm(a: f32, b: f32) { c_nge_s a, b }
	t(a, b)
}

@(export) enc_c_le_s :: proc "contextless" (a: f32, b: f32) {
	t :: asm(a: f32, b: f32) { c_le_s a, b }
	t(a, b)
}

@(export) enc_c_ngt_s :: proc "contextless" (a: f32, b: f32) {
	t :: asm(a: f32, b: f32) { c_ngt_s a, b }
	t(a, b)
}

@(export) enc_c_f_d :: proc "contextless" (a: f64, b: f64) {
	t :: asm(a: f64, b: f64) { c_f_d a, b }
	t(a, b)
}

@(export) enc_c_un_d :: proc "contextless" (a: f64, b: f64) {
	t :: asm(a: f64, b: f64) { c_un_d a, b }
	t(a, b)
}

@(export) enc_c_eq_d :: proc "contextless" (a: f64, b: f64) {
	t :: asm(a: f64, b: f64) { c_eq_d a, b }
	t(a, b)
}

@(export) enc_c_ueq_d :: proc "contextless" (a: f64, b: f64) {
	t :: asm(a: f64, b: f64) { c_ueq_d a, b }
	t(a, b)
}

@(export) enc_c_olt_d :: proc "contextless" (a: f64, b: f64) {
	t :: asm(a: f64, b: f64) { c_olt_d a, b }
	t(a, b)
}

@(export) enc_c_ult_d :: proc "contextless" (a: f64, b: f64) {
	t :: asm(a: f64, b: f64) { c_ult_d a, b }
	t(a, b)
}

@(export) enc_c_ole_d :: proc "contextless" (a: f64, b: f64) {
	t :: asm(a: f64, b: f64) { c_ole_d a, b }
	t(a, b)
}

@(export) enc_c_ule_d :: proc "contextless" (a: f64, b: f64) {
	t :: asm(a: f64, b: f64) { c_ule_d a, b }
	t(a, b)
}

@(export) enc_c_sf_d :: proc "contextless" (a: f64, b: f64) {
	t :: asm(a: f64, b: f64) { c_sf_d a, b }
	t(a, b)
}

@(export) enc_c_ngle_d :: proc "contextless" (a: f64, b: f64) {
	t :: asm(a: f64, b: f64) { c_ngle_d a, b }
	t(a, b)
}

@(export) enc_c_seq_d :: proc "contextless" (a: f64, b: f64) {
	t :: asm(a: f64, b: f64) { c_seq_d a, b }
	t(a, b)
}

@(export) enc_c_ngl_d :: proc "contextless" (a: f64, b: f64) {
	t :: asm(a: f64, b: f64) { c_ngl_d a, b }
	t(a, b)
}

@(export) enc_c_lt_d :: proc "contextless" (a: f64, b: f64) {
	t :: asm(a: f64, b: f64) { c_lt_d a, b }
	t(a, b)
}

@(export) enc_c_nge_d :: proc "contextless" (a: f64, b: f64) {
	t :: asm(a: f64, b: f64) { c_nge_d a, b }
	t(a, b)
}

@(export) enc_c_le_d :: proc "contextless" (a: f64, b: f64) {
	t :: asm(a: f64, b: f64) { c_le_d a, b }
	t(a, b)
}

@(export) enc_c_ngt_d :: proc "contextless" (a: f64, b: f64) {
	t :: asm(a: f64, b: f64) { c_ngt_d a, b }
	t(a, b)
}

@(export) enc_bc1f :: proc "contextless" (a: f32, b: f32) -> u32 {
	t :: asm(a: f32, b: f32) -> (r: u32) [r = %v0] {
		li r, 1
		c_olt_s a, b
		bc1f .d
		li r, 0
	.d:
	}
	return t(a, b)
}

@(export) enc_bc1t :: proc "contextless" (a: f32, b: f32) -> u32 {
	t :: asm(a: f32, b: f32) -> (r: u32) [r = %v0] {
		li r, 1
		c_olt_s a, b
		bc1t .d
		li r, 0
	.d:
	}
	return t(a, b)
}

@(export) enc_bc1fl :: proc "contextless" (a: f32, b: f32) -> u32 {
	t :: asm(a: f32, b: f32) -> (r: u32) [r = %v0] {
		li r, 1
		c_olt_s a, b
		bc1fl .d
		li r, 0
	.d:
	}
	return t(a, b)
}

@(export) enc_bc1tl :: proc "contextless" (a: f32, b: f32) -> u32 {
	t :: asm(a: f32, b: f32) -> (r: u32) [r = %v0] {
		li r, 1
		c_olt_s a, b
		bc1tl .d
		li r, 0
	.d:
	}
	return t(a, b)
}

