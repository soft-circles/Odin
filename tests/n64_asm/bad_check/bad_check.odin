package n64_asm_bad_check

// Checker rejections, one per template and one template per line: the test
// matches each expected message to the line its template is declared on.

// immediate ranges
addiu_imm_wide      :: asm(a: u32) -> (r: u32) { addiu r, a, 40000 }
addiu_imm_max_plus1 :: asm(a: u32) -> (r: u32) { addiu r, a, 32768 }
addiu_imm_min_less1 :: asm(a: u32) -> (r: u32) { addiu r, a, -32769 }
slti_imm_unsigned   :: asm(a: u32) -> (r: u32) { slti r, a, 65535 }
andi_imm_negative   :: asm(a: u32) -> (r: u32) { andi r, a, -1 }
andi_imm_max_plus1  :: asm(a: u32) -> (r: u32) { andi r, a, 65536 }
lui_imm_negative    :: asm() -> (r: u32) { lui r, -1 }
lui_imm_max_plus1   :: asm() -> (r: u32) { lui r, 65536 }
sll_amount_32       :: asm(a: u32) -> (r: u32) { sll r, a, 32 }
sll_amount_negative :: asm(a: u32) -> (r: u32) { sll r, a, -1 }
cache_op_wide       :: asm(p: rawptr) { cache 0x20, [p] }
cache_op_l2         :: asm(p: rawptr) { cache 2, [p] }
cache_op_icache_cde :: asm(p: rawptr) { cache 0x0C, [p] }
syscall_code_wide   :: asm() { syscall 1048576 }
break_code_wide     :: asm() { break 1024 }
teq_code_wide       :: asm(a: u32) { teq a, a, 1024 }
li_too_large        :: asm() -> (r: u32) { li r, 4294967296 }
li_too_small        :: asm() -> (r: u32) { li r, -2147483649 }

// memory displacements: no silent expansion through $at
lw_disp_wide        :: asm(p: rawptr) -> (r: u32) { lw r, [p + 40000] }
sw_disp_wide        :: asm(p: rawptr, v: u32) { sw v, [p + 40000] }
lw_disp_negative    :: asm(p: rawptr) -> (r: u32) { lw r, [p - 32769] }
lw_disp_max_plus1   :: asm(p: rawptr) -> (r: u32) { lw r, [p + 32768] }
sw_disp_max_plus1   :: asm(p: rawptr, v: u32) { sw v, [p + 32768] }
sw_disp_min_less1   :: asm(p: rawptr, v: u32) { sw v, [p - 32769] }

// COP0 slots
gpr_in_cp0_slot     :: asm(v: u32) { mtc0 v, %t0 }
param_in_cp0_slot   :: asm(v: u32) -> (r: u32) { mfc0 r, v }
f32_to_mtc0         :: asm(v: f32) { mtc0 v, %c0_status }

// unknown names, with did-you-mean suggestions
unknown_mnemonic    :: asm(p: rawptr) { frobnicate [p] }
mnemonic_typo       :: asm(a: u32) -> (r: u32) { addui r, a, 1 }
unknown_cp0         :: asm() -> (r: u32) { mfc0 r, %c0_bogus }
cp0_typo            :: asm() -> (r: u32) { mfc0 r, %c0_cnt }

// %at is reserved for assembler macros
at_operand          :: asm(a: u32) -> (r: u32) { addu r, a, %at }
at_pin              :: asm(a: u32) -> (r: u32) [a = %at] { addu r, a, a }
at_clobber          :: asm(a: u32) -> (r: u32) [#clobber %at] { addu r, a, a }

// branch and jump targets are template labels only
absolute_j          :: asm() { j 0x80001000 }
absolute_jal        :: asm() { jal 0x80000400 }
numeric_branch      :: asm(a: u32, b: u32) -> (r: u32) { move r, a; beq a, b, 8 }
undeclared_label    :: asm(a: u32) -> ! { b .nowhere }
duplicate_label     :: asm(a: u32) -> (r: u32) { move r, a; .l: addiu r, r, 1; .l: }
jr_not_diverging    :: asm(a: u32) { jr a }

// operand classes, widths and counts
u64_in_gpr32_slot   :: asm(a: u64, b: u32) -> (r: u32) { addu r, a, b }
f32_in_gpr_slot     :: asm(a: f32, b: u32) -> (r: u32) { addu r, b, a }
gpr_in_fpr_slot     :: asm(a: u32, b: f32) -> (r: f32) { add_s r, b, a }
f64_in_single_slot  :: asm(a: f64, b: f32) -> (r: f32) { add_s r, a, b }
f32_in_double_slot  :: asm(a: f32, b: f64) -> (r: f64) { add_d r, a, b }
f64_in_word_slot    :: asm(a: f64) -> (r: f64) { cvt_d_w r, a }
too_many_operands   :: asm(a: u32, b: u32) -> (r: u32) { addu r, a, b, a }
too_few_operands    :: asm(a: u32) -> (r: u32) { addu r, a }

// literal register writes and tracked reads
literal_write       :: asm(a: u32) -> (r: u32) { addu %t0, a, a; move r, a }
write_untied_input  :: asm(a: u32) -> (r: u32) [a = %a0] { addu %a0, a, a; move r, a }
mflo_unproduced     :: asm() -> (r: u32) { mflo r }
mfhi_after_mtlo     :: asm(a: u32) -> (r: u32) { mtlo a; mfhi r }
bc1t_unproduced     :: asm(a: u32) -> (r: u32) { move r, a; bc1t .l; addiu r, r, 1; .l: }

// pins
output_pin_zero     :: asm(a: u32) -> (r: u32) [r = %zero] { move r, a }
output_pin_k0       :: asm(a: u32) -> (r: u32) [r = %k0] { move r, a }
output_pin_gp       :: asm(a: u32) -> (r: u32) [r = %gp] { move r, a }
output_pin_sp       :: asm(a: u32) -> (r: u32) [r = %sp] { move r, a }
f32_pinned_to_gpr   :: asm(a: f32) -> (r: f32) [a = %a0] { mov_s r, a }

@(export)
use :: proc "contextless" (x: u32, w: u64, f: f32, d: f64, p: rawptr) {
	_ = addiu_imm_wide(x); _ = slti_imm_unsigned(x); _ = andi_imm_negative(x); _ = lui_imm_negative()
	_ = addiu_imm_max_plus1(x); _ = addiu_imm_min_less1(x); _ = andi_imm_max_plus1(x); _ = lui_imm_max_plus1()
	_ = sll_amount_32(x); _ = sll_amount_negative(x); cache_op_wide(p); cache_op_l2(p); cache_op_icache_cde(p)
	syscall_code_wide(); break_code_wide(); teq_code_wide(x); _ = li_too_large(); _ = li_too_small()
	_ = lw_disp_wide(p); sw_disp_wide(p, x); _ = lw_disp_negative(p)
	_ = lw_disp_max_plus1(p); sw_disp_max_plus1(p, x); sw_disp_min_less1(p, x)
	gpr_in_cp0_slot(x); _ = param_in_cp0_slot(x); f32_to_mtc0(f)
	unknown_mnemonic(p); _ = mnemonic_typo(x); _ = unknown_cp0(); _ = cp0_typo()
	_ = at_operand(x); _ = at_pin(x); _ = at_clobber(x)
	absolute_j(); absolute_jal(); _ = numeric_branch(x, x); _ = duplicate_label(x); jr_not_diverging(x)
	_ = u64_in_gpr32_slot(w, x); _ = f32_in_gpr_slot(f, x); _ = gpr_in_fpr_slot(x, f); _ = f64_in_single_slot(d, f)
	_ = f32_in_double_slot(f, d); _ = f64_in_word_slot(d); _ = too_many_operands(x, x); _ = too_few_operands(x)
	_ = literal_write(x); _ = write_untied_input(x); _ = mflo_unproduced(); _ = mfhi_after_mtlo(x); _ = bc1t_unproduced(x)
	_ = output_pin_zero(x); _ = output_pin_k0(x); _ = output_pin_gp(x); _ = output_pin_sp(x); _ = f32_pinned_to_gpr(f)
	undeclared_label(x)
}
