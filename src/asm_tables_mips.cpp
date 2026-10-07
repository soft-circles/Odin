// =============================================================================
// HAND-WRITTEN TABLE - MIPS III (NEC VR4300, Nintendo 64) asm templates
// =============================================================================
//
// Unlike asm_tables_{amd64,riscv,arm64}.cpp this table is not generated from
// core:rexcode. Upstream Odin has no MIPS target and core:rexcode's MIPS ISA
// has no clobber semantics, so the fork keeps a hand-written table. It is laid
// out like the generated riscv table (same sections, enums, Encoding/Clobber
// records and AsmCtx interface) so it can be swapped for a generated one.
//
// Scope: the instruction set of the VR4300, i.e. MIPS I/II/III integer, COP0
// (system control, TLB, CACHE) and the COP1 FPU in FR=1 mode (32 64-bit FPRs,
// which LLVM uses for mips3 under the o64 ABI). MIPS IV / MIPS32 / MIPS32r2 /
// MIPS64r2-only instructions are present but gated behind their LLVM feature
// name, so the checker rejects them unless the target enables that feature.
// LLVM's assembler would otherwise accept some of them for mips3 (pref, wait).
//
// Spelling: operand order is the assembler's (destination first). FPU
// mnemonics use '_' for '.', e.g. `add_s`, `cvt_d_w`, `c_olt_d`; the backend
// writes the '.' form. Labels are template labels only (`.name`).
//
// Things every MIPS template inherits from LLVM, and how the table and the
// backend (lbAsmGenerate_mips) deal with them:
//
//   * `.set push; .set at; .set macro; .set reorder` wraps every asm block.
//
//   * $at is allocatable in LLVM, but assembler macros (large memory offsets,
//     `li`, out-of-range immediates) expand through $at. Every template
//     clobbers `~{$1}`, and `%at` is not a register name here at all, so it can
//     neither be pinned nor written in a template.
//
//   * Delay slots: `.set reorder` makes the assembler put a `nop` in every
//     branch/jump delay slot. Write branches without a delay-slot instruction;
//     the instruction after a branch runs after it, on the fall-through path
//     only. Branch-likely forms (`beql`, `bc1tl`, ...) are accepted but get the
//     same nop, so they behave like the plain branch.
//
//   * The HI/LO hazard (an `mfhi`/`mflo` followed within two instructions by
//     anything that writes HI or LO: `mult`/`div` and their unsigned and
//     doubleword forms, `mthi`/`mtlo`) is filled at the template boundary only,
//     where the author cannot see the compiler's code. A template whose first
//     two instructions include a HI/LO write starts with `nop`s so at least two
//     instructions precede the write, and a template whose last two
//     instructions include a HI/LO read ends with `nop`s so at least two
//     instructions follow the read. Branch and jump delay slots count. Writes
//     and reads come from each form's Clobber record (implicit_wr/implicit_rd).
//     Inside a template the hazard remains the author's job.
//
//   * COP0 hazards (e.g. after `mtc0` to Status or EntryHi, around
//     `tlbwi`/`eret`) are NOT filled: they remain the template author's job, as
//     in libdragon's C0_WRITE_* macros.
//
//   * Register operands narrower than 64 bits keep LLVM's invariant that a
//     32-bit value sits sign-extended in its 64-bit GPR. Integer and boolean
//     inputs narrower than 32 bits are passed as i32 (sign-extended for signed
//     types, zero-extended otherwise), tied or not, and narrow outputs are
//     requested as i32 and truncated. An output narrower than 64 bits that an
//     instruction may leave with an arbitrary upper half (`ld`, `lwu`, `d*`
//     arithmetic and shifts, `dmfc0`, `dmfc1`, `mfhi`/`mflo` after a doubleword
//     multiply or divide, `move` from a 64-bit value, ...) is requested as i64
//     and truncated, which costs one `sll rd, rs, 0`. See gpr_word_write.
//
//   * LLVM parses the `{hi}`/`{lo}` constraints as the 32-bit HI/LO whatever
//     the value type, so it truncates a 64-bit input pinned to %hi/%lo and
//     assumes a 64-bit result from them has a zero upper half. An operand
//     pinned to %hi or %lo that crosses as i64 (a 64-bit type, or a narrower
//     output HI/LO may hold a doubleword in) is therefore passed in a GPR
//     instead: the backend adds `mthi`/`mtlo` before the body for inputs (and
//     clobbers HI/LO) and `mfhi`/`mflo` after it for results. Those moves count
//     for the HI/LO hazard padding above.
//
//   * Two-operand `div`/`divu`/`ddiv`/`ddivu` are assembler MACROS in LLVM
//     (divide-by-zero trap plus `mflo rs`, which overwrites rs). The backend
//     always spells them `div $zero, rs, rt`, which is the bare instruction.
//
// Jump policy:
//
//   * The only branch/jump targets are template labels. A numeric or absolute
//     target cannot be written, so `j`/`jal` to an absolute address is rejected
//     by the operand-kind check ("expected label operand").
//   * `j .label` is an unconditional jump inside the template (same as `b`).
//   * `jal`/`bal`/`bltzal`/`bgezal` link through %ra and are modelled as calls
//     that return: they write %ra (so it is clobbered) and fall through.
//   * `jr rs` leaves the template: it is terminal, so a template using it must
//     be declared diverging (`-> !`) unless every path still reaches the end.
//   * `jalr rs` / `jalr rd, rs` call through a register and return. They write
//     %ra (or rd), make the template volatile and clobber memory (the callee
//     may touch any of it; amd64 `call` gets the same from its stack push). The
//     callee's register clobbers are the author's responsibility (`#clobber`
//     every caller-saved register it may touch, and %hi/%lo if it may use them).
//
// Register model (ClobberRegs): %ra, %hi, %lo and %fcc0 (the MIPS I-III FP
// condition bit, FCSR bit 23) are tracked registers. `mult`/`div`/`mthi`/`mtlo`
// write HI/LO, `mfhi`/`mflo` read them; `c_cond_fmt` and `ctc1` write %fcc0,
// `bc1t`/`bc1f` read it. The checker auto-clobbers tracked registers a template
// writes and reports reads of them that nothing in the template produced.
// Other GPRs and the FPRs are not tracked (the checker's masks are u16), so a
// template that writes a literal %t0 must pin it to an output or scratch, or
// `#clobber %t0`; the checker reports the write otherwise
// (register_write_needs_clobber).

#define ASM_MIPS_MNEMONICS(X) \
	/* system, cache, COP0 */ \
	X(NOP, "nop") X(SSNOP, "ssnop") X(SYNC, "sync") X(SYSCALL, "syscall") X(BREAK, "break") \
	X(CACHE, "cache") X(MFC0, "mfc0") X(MTC0, "mtc0") X(DMFC0, "dmfc0") X(DMTC0, "dmtc0") \
	X(TLBR, "tlbr") X(TLBWI, "tlbwi") X(TLBWR, "tlbwr") X(TLBP, "tlbp") X(ERET, "eret") \
	/* integer ALU */ \
	X(ADD, "add") X(ADDU, "addu") X(SUB, "sub") X(SUBU, "subu") \
	X(AND, "and") X(OR, "or") X(XOR, "xor") X(NOR, "nor") X(SLT, "slt") X(SLTU, "sltu") \
	X(DADD, "dadd") X(DADDU, "daddu") X(DSUB, "dsub") X(DSUBU, "dsubu") \
	X(ADDI, "addi") X(ADDIU, "addiu") X(SLTI, "slti") X(SLTIU, "sltiu") \
	X(ANDI, "andi") X(ORI, "ori") X(XORI, "xori") X(LUI, "lui") X(DADDI, "daddi") X(DADDIU, "daddiu") \
	X(SLL, "sll") X(SRL, "srl") X(SRA, "sra") X(SLLV, "sllv") X(SRLV, "srlv") X(SRAV, "srav") \
	X(DSLL, "dsll") X(DSRL, "dsrl") X(DSRA, "dsra") X(DSLL32, "dsll32") X(DSRL32, "dsrl32") X(DSRA32, "dsra32") \
	X(DSLLV, "dsllv") X(DSRLV, "dsrlv") X(DSRAV, "dsrav") \
	/* multiply / divide and HI/LO */ \
	X(MULT, "mult") X(MULTU, "multu") X(DIV, "div") X(DIVU, "divu") \
	X(DMULT, "dmult") X(DMULTU, "dmultu") X(DDIV, "ddiv") X(DDIVU, "ddivu") \
	X(MFHI, "mfhi") X(MFLO, "mflo") X(MTHI, "mthi") X(MTLO, "mtlo") \
	/* assembler aliases */ \
	X(MOVE, "move") X(NOT, "not") X(NEGU, "negu") X(DNEGU, "dnegu") X(LI, "li") \
	/* loads / stores */ \
	X(LB, "lb") X(LBU, "lbu") X(LH, "lh") X(LHU, "lhu") X(LW, "lw") X(LWU, "lwu") X(LD, "ld") \
	X(SB, "sb") X(SH, "sh") X(SW, "sw") X(SD, "sd") \
	X(LWL, "lwl") X(LWR, "lwr") X(LDL, "ldl") X(LDR, "ldr") \
	X(SWL, "swl") X(SWR, "swr") X(SDL, "sdl") X(SDR, "sdr") \
	X(LL, "ll") X(LLD, "lld") X(SC, "sc") X(SCD, "scd") \
	/* branches (delay slot filled by the assembler) */ \
	X(BEQ, "beq") X(BNE, "bne") X(BLEZ, "blez") X(BGTZ, "bgtz") X(BLTZ, "bltz") X(BGEZ, "bgez") \
	X(BLTZAL, "bltzal") X(BGEZAL, "bgezal") \
	X(BEQL, "beql") X(BNEL, "bnel") X(BLEZL, "blezl") X(BGTZL, "bgtzl") X(BLTZL, "bltzl") X(BGEZL, "bgezl") \
	X(BLTZALL, "bltzall") X(BGEZALL, "bgezall") \
	X(B, "b") X(BAL, "bal") X(BEQZ, "beqz") X(BNEZ, "bnez") X(BEQZL, "beqzl") X(BNEZL, "bnezl") \
	/* jumps */ \
	X(J, "j") X(JAL, "jal") X(JR, "jr") X(JALR, "jalr") \
	/* traps */ \
	X(TEQ, "teq") X(TNE, "tne") X(TGE, "tge") X(TGEU, "tgeu") X(TLT, "tlt") X(TLTU, "tltu") \
	X(TEQI, "teqi") X(TNEI, "tnei") X(TGEI, "tgei") X(TGEIU, "tgeiu") X(TLTI, "tlti") X(TLTIU, "tltiu") \
	/* COP1 moves, loads and stores */ \
	X(MFC1, "mfc1") X(MTC1, "mtc1") X(DMFC1, "dmfc1") X(DMTC1, "dmtc1") X(CFC1, "cfc1") X(CTC1, "ctc1") \
	X(LWC1, "lwc1") X(SWC1, "swc1") X(LDC1, "ldc1") X(SDC1, "sdc1") \
	/* FPU arithmetic */ \
	X(ADD_S, "add_s") X(ADD_D, "add_d") X(SUB_S, "sub_s") X(SUB_D, "sub_d") \
	X(MUL_S, "mul_s") X(MUL_D, "mul_d") X(DIV_S, "div_s") X(DIV_D, "div_d") \
	X(SQRT_S, "sqrt_s") X(SQRT_D, "sqrt_d") X(ABS_S, "abs_s") X(ABS_D, "abs_d") \
	X(NEG_S, "neg_s") X(NEG_D, "neg_d") X(MOV_S, "mov_s") X(MOV_D, "mov_d") \
	/* FPU conversions */ \
	X(CVT_S_D, "cvt_s_d") X(CVT_S_W, "cvt_s_w") X(CVT_S_L, "cvt_s_l") \
	X(CVT_D_S, "cvt_d_s") X(CVT_D_W, "cvt_d_w") X(CVT_D_L, "cvt_d_l") \
	X(CVT_W_S, "cvt_w_s") X(CVT_W_D, "cvt_w_d") X(CVT_L_S, "cvt_l_s") X(CVT_L_D, "cvt_l_d") \
	X(ROUND_W_S, "round_w_s") X(ROUND_W_D, "round_w_d") X(ROUND_L_S, "round_l_s") X(ROUND_L_D, "round_l_d") \
	X(TRUNC_W_S, "trunc_w_s") X(TRUNC_W_D, "trunc_w_d") X(TRUNC_L_S, "trunc_l_s") X(TRUNC_L_D, "trunc_l_d") \
	X(CEIL_W_S,  "ceil_w_s")  X(CEIL_W_D,  "ceil_w_d")  X(CEIL_L_S,  "ceil_l_s")  X(CEIL_L_D,  "ceil_l_d") \
	X(FLOOR_W_S, "floor_w_s") X(FLOOR_W_D, "floor_w_d") X(FLOOR_L_S, "floor_l_s") X(FLOOR_L_D, "floor_l_d") \
	/* FPU compares (write %fcc0) */ \
	X(C_F_S, "c_f_s") X(C_UN_S, "c_un_s") X(C_EQ_S, "c_eq_s") X(C_UEQ_S, "c_ueq_s") \
	X(C_OLT_S, "c_olt_s") X(C_ULT_S, "c_ult_s") X(C_OLE_S, "c_ole_s") X(C_ULE_S, "c_ule_s") \
	X(C_SF_S, "c_sf_s") X(C_NGLE_S, "c_ngle_s") X(C_SEQ_S, "c_seq_s") X(C_NGL_S, "c_ngl_s") \
	X(C_LT_S, "c_lt_s") X(C_NGE_S, "c_nge_s") X(C_LE_S, "c_le_s") X(C_NGT_S, "c_ngt_s") \
	X(C_F_D, "c_f_d") X(C_UN_D, "c_un_d") X(C_EQ_D, "c_eq_d") X(C_UEQ_D, "c_ueq_d") \
	X(C_OLT_D, "c_olt_d") X(C_ULT_D, "c_ult_d") X(C_OLE_D, "c_ole_d") X(C_ULE_D, "c_ule_d") \
	X(C_SF_D, "c_sf_d") X(C_NGLE_D, "c_ngle_d") X(C_SEQ_D, "c_seq_d") X(C_NGL_D, "c_ngl_d") \
	X(C_LT_D, "c_lt_d") X(C_NGE_D, "c_nge_d") X(C_LE_D, "c_le_d") X(C_NGT_D, "c_ngt_d") \
	/* FPU branches (read %fcc0) */ \
	X(BC1F, "bc1f") X(BC1T, "bc1t") X(BC1FL, "bc1fl") X(BC1TL, "bc1tl") \
	/* NOT on the VR4300: gated behind their LLVM feature (see feature_name) */ \
	X(EXT, "ext") X(INS, "ins") X(DEXT, "dext") X(DINS, "dins") \
	X(ROTR, "rotr") X(ROTRV, "rotrv") X(DROTR, "drotr") X(DROTR32, "drotr32") X(DROTRV, "drotrv") \
	X(SEB, "seb") X(SEH, "seh") X(WSBH, "wsbh") X(DSBH, "dsbh") X(DSHD, "dshd") \
	X(CLZ, "clz") X(CLO, "clo") X(DCLZ, "dclz") X(DCLO, "dclo") \
	X(MUL, "mul") X(MADD, "madd") X(MADDU, "maddu") X(MSUB, "msub") X(MSUBU, "msubu") \
	X(MOVZ, "movz") X(MOVN, "movn") X(MOVZ_S, "movz_s") X(MOVZ_D, "movz_d") X(MOVN_S, "movn_s") X(MOVN_D, "movn_d") \
	X(PREF, "pref") X(SYNCI, "synci") X(EHB, "ehb") X(WAIT, "wait") X(DI, "di") X(EI, "ei") X(RDHWR, "rdhwr") \
	X(JR_HB, "jr_hb") X(JALR_HB, "jalr_hb") X(MFHC1, "mfhc1") X(MTHC1, "mthc1") \
	X(MADD_S, "madd_s") X(MADD_D, "madd_d") X(MSUB_S, "msub_s") X(MSUB_D, "msub_d") \
	X(NMADD_S, "nmadd_s") X(NMADD_D, "nmadd_d") X(NMSUB_S, "nmsub_s") X(NMSUB_D, "nmsub_d") \
	X(RECIP_S, "recip_s") X(RECIP_D, "recip_d") X(RSQRT_S, "rsqrt_s") X(RSQRT_D, "rsqrt_d")

// ENUM, name, class, number. %at ($1) is deliberately absent: see the header.
#define ASM_MIPS_REGISTERS(X) \
	X(ZERO, "zero", GPR, 0) \
	X(V0, "v0", GPR, 2)   X(V1, "v1", GPR, 3) \
	X(A0, "a0", GPR, 4)   X(A1, "a1", GPR, 5)   X(A2, "a2", GPR, 6)   X(A3, "a3", GPR, 7) \
	X(T0, "t0", GPR, 8)   X(T1, "t1", GPR, 9)   X(T2, "t2", GPR, 10)  X(T3, "t3", GPR, 11) \
	X(T4, "t4", GPR, 12)  X(T5, "t5", GPR, 13)  X(T6, "t6", GPR, 14)  X(T7, "t7", GPR, 15) \
	X(S0, "s0", GPR, 16)  X(S1, "s1", GPR, 17)  X(S2, "s2", GPR, 18)  X(S3, "s3", GPR, 19) \
	X(S4, "s4", GPR, 20)  X(S5, "s5", GPR, 21)  X(S6, "s6", GPR, 22)  X(S7, "s7", GPR, 23) \
	X(T8, "t8", GPR, 24)  X(T9, "t9", GPR, 25)  X(K0, "k0", GPR, 26)  X(K1, "k1", GPR, 27) \
	X(GP, "gp", GPR, 28)  X(SP, "sp", GPR, 29)  X(FP, "fp", GPR, 30)  X(RA, "ra", GPR, 31) \
	X(S8, "s8", GPR, 30) \
	X(F0,  "f0",  FPR, 0)  X(F1,  "f1",  FPR, 1)  X(F2,  "f2",  FPR, 2)  X(F3,  "f3",  FPR, 3) \
	X(F4,  "f4",  FPR, 4)  X(F5,  "f5",  FPR, 5)  X(F6,  "f6",  FPR, 6)  X(F7,  "f7",  FPR, 7) \
	X(F8,  "f8",  FPR, 8)  X(F9,  "f9",  FPR, 9)  X(F10, "f10", FPR, 10) X(F11, "f11", FPR, 11) \
	X(F12, "f12", FPR, 12) X(F13, "f13", FPR, 13) X(F14, "f14", FPR, 14) X(F15, "f15", FPR, 15) \
	X(F16, "f16", FPR, 16) X(F17, "f17", FPR, 17) X(F18, "f18", FPR, 18) X(F19, "f19", FPR, 19) \
	X(F20, "f20", FPR, 20) X(F21, "f21", FPR, 21) X(F22, "f22", FPR, 22) X(F23, "f23", FPR, 23) \
	X(F24, "f24", FPR, 24) X(F25, "f25", FPR, 25) X(F26, "f26", FPR, 26) X(F27, "f27", FPR, 27) \
	X(F28, "f28", FPR, 28) X(F29, "f29", FPR, 29) X(F30, "f30", FPR, 30) X(F31, "f31", FPR, 31) \
	X(C0_INDEX,    "c0_index",    CP0, 0)  X(C0_RANDOM,   "c0_random",   CP0, 1) \
	X(C0_ENTRYLO0, "c0_entrylo0", CP0, 2)  X(C0_ENTRYLO1, "c0_entrylo1", CP0, 3) \
	X(C0_CONTEXT,  "c0_context",  CP0, 4)  X(C0_PAGEMASK, "c0_pagemask", CP0, 5) \
	X(C0_WIRED,    "c0_wired",    CP0, 6)  X(C0_BADVADDR, "c0_badvaddr", CP0, 8) \
	X(C0_COUNT,    "c0_count",    CP0, 9)  X(C0_ENTRYHI,  "c0_entryhi",  CP0, 10) \
	X(C0_COMPARE,  "c0_compare",  CP0, 11) X(C0_STATUS,   "c0_status",   CP0, 12) \
	X(C0_CAUSE,    "c0_cause",    CP0, 13) X(C0_EPC,      "c0_epc",      CP0, 14) \
	X(C0_PRID,     "c0_prid",     CP0, 15) X(C0_CONFIG,   "c0_config",   CP0, 16) \
	X(C0_LLADDR,   "c0_lladdr",   CP0, 17) X(C0_WATCHLO,  "c0_watchlo",  CP0, 18) \
	X(C0_WATCHHI,  "c0_watchhi",  CP0, 19) X(C0_XCONTEXT, "c0_xcontext", CP0, 20) \
	X(C0_PERR,     "c0_perr",     CP0, 26) X(C0_CACHEERR, "c0_cacheerr", CP0, 27) \
	X(C0_TAGLO,    "c0_taglo",    CP0, 28) X(C0_TAGHI,    "c0_taghi",    CP0, 29) \
	X(C0_ERROREPC, "c0_errorepc", CP0, 30) \
	X(FCR0, "fcr0", FCR, 0) X(FCR31, "fcr31", FCR, 31) \
	X(HI, "hi", HILO, 0) X(LO, "lo", HILO, 1) \
	X(FCC0, "fcc0", FCC, 0)


struct Asm_mips {
	enum Mnemonic : u16 {
		M_INVALID,
	#define ASM_MIPS_X(e, s) M_##e,
		ASM_MIPS_MNEMONICS(ASM_MIPS_X)
	#undef ASM_MIPS_X

		MNEMONIC_COUNT
	};
	static String const mnemonic_strings[MNEMONIC_COUNT];

	enum Prefix : u8 {
		PREFIX_INVALID,
		PREFIX_COUNT
	};
	enum PrefixKind : u8 { PrefixKind_None };


	static const u16 REG_CLASS_NONE = 0x0000;
	static const u16 REG_CLASS_GPR  = 0x0100; // $0..$31 (64-bit on the VR4300)
	static const u16 REG_CLASS_FPR  = 0x0200; // $f0..$f31 (64-bit, FR=1)
	static const u16 REG_CLASS_CP0  = 0x0300; // COP0 $0..$31, only fillable by name
	static const u16 REG_CLASS_FCR  = 0x0400; // COP1 control: FIR ($0) and FCSR ($31)
	static const u16 REG_CLASS_HILO = 0x0500; // HI, LO (implicit operands of mult/div)
	static const u16 REG_CLASS_FCC  = 0x0600; // FP condition bit (FCSR bit 23)

	enum Register : u16 {
		REG_INVALID,
	#define ASM_MIPS_X(e, s, c, n) REG_##e,
		ASM_MIPS_REGISTERS(ASM_MIPS_X)
	#undef ASM_MIPS_X

		REG_COUNT
	};


	enum OperandType : u8 {
		OP_NONE,
		OP_GPR,      // any GPR, 64-bit data
		OP_GPR32,    // GPR holding a sign-extended 32-bit value (addu, sll, mult, mfc0, ...)
		OP_FPR_S,    // FPR, single (f32)
		OP_FPR_D,    // FPR, double (f64)
		OP_FPR_W,    // FPR holding a 32-bit integer (cvt.*.w source, *.w.* result); Odin type f32
		OP_FPR_L,    // FPR holding a 64-bit integer (cvt.*.l source, *.l.* result); Odin type f64
		OP_CP0,      // a COP0 register, only fillable by a %c0_* register
		OP_FCR,      // a COP1 control register, only fillable by %fcr0 / %fcr31
		OP_FCC,      // an FP condition code, only fillable by %fcc0 (MIPS IV forms only)
		OP_SIMM16,   // signed 16-bit
		OP_UIMM16,   // unsigned 16-bit
		OP_SHAMT,    // 5-bit shift amount
		OP_UIMM5,    // 5-bit unsigned field (pref hint, rdhwr register)
		OP_UIMM6,    // 6-bit unsigned field (ext/ins/dext/dins position and size)
		OP_CACHEOP,  // 5-bit CACHE operation: bits [4:2] operation, [1:0] cache
		OP_CODE10,   // 10-bit break / trap code
		OP_CODE20,   // 20-bit syscall code
		OP_SEL,      // 3-bit COP0 select (MIPS32 only)
		OP_IMM32,    // `li` macro immediate: any 32-bit value, sign-extended to 64 bits
		OP_MEM,      // offset(base), signed 16-bit offset
		OP_REL16,    // branch target label (PC-relative, 16-bit word offset)
		OP_TARGET26, // jump target label (256 MiB region, 26-bit word index)
	};


	enum OperandEncoding : u8 {
		ENC_NONE,
		ENC_RS,           // bits 25..21
		ENC_RT,           // bits 20..16
		ENC_RD,           // bits 15..11
		ENC_SA,           // bits 10..6
		ENC_IMM16,        // bits 15..0
		ENC_IMM_MACRO,    // `li`: expanded by the assembler into lui/ori/addiu
		ENC_OFFSET_BASE,  // base in bits 25..21, offset in bits 15..0
		ENC_BRANCH16,     // (target - (pc+4)) >> 2 in bits 15..0
		ENC_TARGET26,     // target >> 2 in bits 25..0
		ENC_CODE10,       // bits 15..6 (trap) or 25..16 (break)
		ENC_CODE20,       // bits 25..6
		ENC_CACHE_OP,     // bits 20..16
		ENC_HINT,         // bits 20..16
		ENC_CP0_RD,       // bits 15..11
		ENC_FCR_RD,       // bits 15..11
		ENC_SEL,          // bits 2..0
		ENC_FS,           // bits 15..11
		ENC_FT,           // bits 20..16
		ENC_FD,           // bits 10..6
		ENC_FR,           // bits 25..21 (COP1X)
		ENC_POS,          // bits 10..6
		ENC_SIZE,         // bits 15..11
	};


	enum Feature : u16 {
		// Available on the VR4300
		FEATURE_MIPS_I,
		FEATURE_MIPS_II,
		FEATURE_MIPS_III,
		FEATURE_COP0,
		FEATURE_FPU,
		FEATURE_MACRO,    // assembler alias or macro of the instructions above
		// Not on the VR4300; the value is the LLVM feature that enables it
		FEATURE_MIPS4,
		FEATURE_MIPS32,
		FEATURE_MIPS32R2,
		FEATURE_MIPS64,
		FEATURE_MIPS64R2,
	};

	typedef u8 EncodingFlags; // cannot use a C++ bit field to due lack of portability

	struct Encoding {
		Mnemonic        mnemonic;
		OperandType     ops[4];
		OperandEncoding enc[4];
		u32             bits;
		u32             mask;
		Feature         feature;
		EncodingFlags   flags; // bit 7: has implicit operands, bits 4..6: explicit operand count

		bool has_implicit  () const { return ((flags>>7u)&1) != 0; }
		u8   explicit_count() const { return cast(u8)((flags>>4u)&((1u<<3)-1)); }
	};


	// MIPS has no condition-flags register. FCSR exception flags are not modelled.
	enum ClobberFlags : u8 {
		ClobberFlag_None = 0,
	};

	char const *clobber_flag_bit_name(u16 bit) {
		gb_unused(bit);
		return "?";
	}

	enum ClobberRegs : u8 {
		ClobberReg_RA  = 1<<0, // $31, written by jal/jalr/bal/b*zal
		ClobberReg_HI  = 1<<1, // written by mult/div/mthi, read by mfhi
		ClobberReg_LO  = 1<<2, // written by mult/div/mtlo, read by mflo
		ClobberReg_FCC = 1<<3, // FP condition bit: written by c.cond.fmt/ctc1, read by bc1t/bc1f
	};

	static u8 const CLOBBER_REGS_NAMED = ClobberReg_RA|ClobberReg_HI|ClobberReg_LO|ClobberReg_FCC;

	enum SideEffectFlags : u8 {
		SideEffectFlag_CONTROL = 1<<0, // writes pc
		SideEffectFlag_COND    = 1<<1, // conditional transfer: may fall through
		SideEffectFlag_CALL    = 1<<2, // links through a register and returns: falls through
		SideEffectFlag_TRAP    = 1<<3, // may raise a synchronous exception (syscall, break, t*)
		SideEffectFlag_SYSTEM  = 1<<4, // COP0, TLB, CACHE, ERET: machine state the compiler cannot see
		SideEffectFlag_FENCE   = 1<<5, // SYNC
		SideEffectFlag_ATOMIC  = 1<<6, // LL/SC reservation
		SideEffectFlag_FPCSR   = 1<<7, // reads or writes the FP control/status register
	};

	enum OperandSet : u8 {
		OperandSet_OP0 = 1<<0,
		OperandSet_OP1 = 1<<1,
		OperandSet_OP2 = 1<<2,
		OperandSet_OP3 = 1<<3,
	};

	u16 clobber_bit_for_reg_name(String const &pin) {
		static const struct { String name; u16 bit; } table[] = {
			{str_lit("ra"),   ClobberReg_RA},
			{str_lit("hi"),   ClobberReg_HI},
			{str_lit("lo"),   ClobberReg_LO},
			{str_lit("fcc0"), ClobberReg_FCC},
		};
		for (auto const &t : table) {
			if (pin == t.name) {
				return t.bit;
			}
		}
		return 0;
	}

	char const *clobber_reg_bit_name(u16 bit) {
		switch (bit) {
		case ClobberReg_RA:  return "ra";
		case ClobberReg_HI:  return "hi";
		case ClobberReg_LO:  return "lo";
		case ClobberReg_FCC: return "fcc0";
		}
		return "<reg>";
	}


	// MIPS has no condition flags register, so `%flags` and flag pins do not exist.
	u16 flag_from_name(String const &name) {
		gb_unused(name);
		return 0;
	}
	u16 flags_from_name(String const &name) {
		gb_unused(name);
		return 0;
	}
	i32 flag_bit_from_name(String const &name, i32 *width_) {
		gb_unused(name);
		gb_unused(width_);
		return -1;
	}


	struct Clobber {
		OperandSet      written;     // operand slots whose register is written
		OperandSet      read;        // operand slots whose register / memory base is read
		ClobberRegs     implicit_wr; // implicit tracked-register writes (ra, hi, lo, fcc0)
		ClobberRegs     implicit_rd; // implicit tracked-register reads
		ClobberFlags    flags_wr;    // always empty: no flags register
		bool            writes_mem;
		bool            reads_mem;
		SideEffectFlags side_effects;

		ClobberFlags flags_rd_call() const {
			return {};
		}
		ClobberFlags flags_wr_call() const {
			return flags_wr;
		}

		bool implies_clobber_flags() const {
			return false;
		}
		bool implies_clobber_memory() const {
			return writes_mem || reads_mem ||
				(side_effects & (SideEffectFlag_FENCE|SideEffectFlag_SYSTEM|SideEffectFlag_ATOMIC)) != 0;
		}
		bool implies_side_effects() const {
			return side_effects != 0;
		}
		u8 is_call_or_mem() const {
			return (cast(u16)side_effects & SideEffectFlag_CALL) != 0;
		}
		bool has_control() const {
			return (cast(u16)side_effects & SideEffectFlag_CONTROL) != 0;
		}
		bool has_halt() const {
			// syscall/break/traps return to the next instruction when handled.
			return false;
		}
		bool is_conditional(struct Encoding const &valid_form) const {
			gb_unused(valid_form);
			// Conditional branches fall through; calls (jal/jalr/bal) return. Only
			// j/b/jr/eret end straight-line flow.
			return has_control() && (side_effects & (SideEffectFlag_COND|SideEffectFlag_CALL)) != 0;
		}
		bool is_nondeterministic() const {
			// mfc0 of Count/Random is, but that depends on the operand, not the form;
			// SYSTEM already makes every COP0 access volatile and impure.
			return false;
		}
		bool has_implicit_mem() const {
			// cache/sync/tlbw*/eret act on memory or the address map without an
			// operand the checker would see as a memory access.
			if (!writes_mem && !reads_mem) {
				return false;
			}
			return (side_effects & (SideEffectFlag_SYSTEM|SideEffectFlag_FENCE|SideEffectFlag_ATOMIC)) != 0;
		}
		bool is_status_snapshot() const {
			// No flags register: nothing reads status implicitly.
			return true;
		}
	};

	void clobber_implicit_regs(StringSet *clobber_registers_set, u16 implicit_regs) {
		u8 regs = cast(u8)implicit_regs & CLOBBER_REGS_NAMED;

		for (u8 bit = 1; bit != 0; bit <<= 1) {
			if ((regs & bit) == 0) {
				continue;
			}
			char const *rname = clobber_reg_bit_name(bit);
			string_set_update(clobber_registers_set, make_string_c(rname));
		}
	}

	// MIPS assembler aliases (move, b, beqz, li, ...) are first-class mnemonics
	// with their own Clobber record rather than pseudo-aliases, so `b` can be an
	// unconditional branch and `li` a macro. The pseudo-alias machinery is empty.
	enum AliasSrc : u8 {
		AliasSrc_NONE,
		AliasSrc_ARG0,
		AliasSrc_ARG1,
		AliasSrc_ARG2,
		AliasSrc_ZERO,
		AliasSrc_LINK,
		AliasSrc_LIT,
	};

	struct PseudoAlias {
		Mnemonic target;
		AliasSrc src[4];
		i16      lit;
		u8       nargs;

		bool is_nondeterministic() const {
			return false;
		}
	};

	enum PseudoMnemonic : u16 {
		PM_INVALID,
		PSEUDO_MNEMONIC_COUNT
	};
	PseudoMnemonic pseudo_mnemonic_lookup(String const &name) {
		gb_unused(name);
		return PM_INVALID;
	}

	PseudoAlias pseudo_alias(u16 pm) {
		gb_unused(pm);
		return {};
	}

	static String const pseudo_mnemonic_strings[PSEUDO_MNEMONIC_COUNT];


	static u16    const register_codes  [REG_COUNT];
	static String const register_strings[REG_COUNT];


	// Companion run index: encode_runs[mnemonic] -> contiguous run in encode_forms.
	struct EncodeRun {
		u32 start; // start index in encode_forms
		u32 count; // number of forms for this mnemonic
	};

	// The hand-written source of truth: one row per form, pairing the operand
	// shape with the Clobber record. init() splits it into the two parallel
	// arrays the checker indexes (encode_forms, clobber_forms_table).
	enum Shape : u8;
	struct FormShape {
		OperandType     ops[4];
		OperandEncoding enc[4];
	};
	struct FormRow {
		Mnemonic mnemonic;
		Shape    shape;
		u32      bits;
		u32      mask;
		Feature  feature;
		Clobber  clobber;
	};
	static FormShape const form_shapes[];
	static FormRow   const form_rows[];
	static isize     const form_row_count;

	EncodeRun  encode_runs[MNEMONIC_COUNT];
	Encoding * encode_forms;
	Clobber *  clobber_forms_table;

	StringMap<Mnemonic> mnemonic_map;
	StringMap<Register> register_map;

	u16 GPRLEN;
	u16 FPRLEN;

	bool init(i64 word_size);


	Mnemonic mnemonic_lookup_ordered(String const &name, u8 *suffixes_) {
		gb_unused(name);
		gb_unused(suffixes_);
		return M_INVALID;
	}

	// `li` is a real mnemonic here (see LI), so no pseudo-macro mnemonics exist;
	// check_pseudo_macro_mnemonic is riscv-only anyway.
	enum PseudoMacroMnemonic : u8 {
		PseudoMacroMnemonic_INVALID,
		PseudoMacroMnemonic_COUNT
	};

	PseudoMacroMnemonic pseudo_macro_mnemonic_lookup(String const &name) {
		gb_unused(name);
		return PseudoMacroMnemonic_INVALID;
	}

	Mnemonic mnemonic_lookup(String const &name) {
		Mnemonic *found = string_map_get(&mnemonic_map, name);
		return found ? *found : M_INVALID;
	}
	Prefix prefix_lookup(String const &name) {
		gb_unused(name);
		return PREFIX_INVALID;
	}
	static String const prefix_strings[PREFIX_COUNT];

	Register register_lookup(String const &name) {
		Register *found = string_map_get(&register_map, name);
		return found ? *found : REG_INVALID;
	}
	Slice<Encoding> encoding_forms(/*Mnemonic*/ u16 m) const {
		EncodeRun r = encode_runs[m];
		return Slice<Encoding>{encode_forms+r.start, r.count};
	}
	Slice<Clobber> clobber_forms(/*Mnemonic*/ u16 m) const {
		EncodeRun r = encode_runs[m];
		return Slice<Clobber>{clobber_forms_table+r.start, r.count};
	}
	u16 reg_class(/*Register*/ u16 r) const {
		return 0xFF00 & r;
	}
	// size in bits for register
	u16 reg_size(Register r) const {
		switch (reg_class(register_codes[r])) {
		case REG_CLASS_GPR:  return GPRLEN;
		case REG_CLASS_FPR:  return FPRLEN;
		case REG_CLASS_CP0:  return 32; // EntryLo*/Context/BadVAddr/EntryHi/EPC/XContext/ErrorEPC are 64-bit via dmfc0
		case REG_CLASS_FCR:  return 32;
		case REG_CLASS_HILO: return 64; // LLVM accepts {hi}/{lo} pins for i32 and i64
		case REG_CLASS_FCC:  return 32;
		}
		return 0;
	}
	// The register's hardware number, used by the LLVM generator.
	u16 reg_number(Register r) const {
		return 0x00FF & register_codes[r];
	}

	bool reg_is_segment(/*Register*/ u16 r) {
		gb_unused(r);
		return false;
	}

	// GPRs are always 64 bits wide and FPRs always 64 bits (FR=1): register names
	// carry no width, so a narrower integer fits a GPR slot.
	bool integer_reg_width_is_exact() const {
		return false;
	}
	// An f32 in a `.d` slot (or an f64 in a `.s` slot) would read half a register:
	// FPU operand widths must match exactly.
	bool float_reg_width_is_exact() const {
		return true;
	}
	bool supports_memory_index_not_just_disp() const {
		return false;
	}

	bool reg_is_non_allocateable(Register r) const {
		switch (r) {
		case REG_ZERO:
		case REG_K0:
		case REG_K1:
		case REG_GP:
		case REG_SP:
			return true;
		}
		switch (reg_class(register_codes[r])) {
		case REG_CLASS_CP0:
		case REG_CLASS_FCR:
		case REG_CLASS_FCC: // LLVM cannot copy a value into $fcc0 (copyPhysReg asserts)
			return true;
		}
		return false;
	}

	AsmOperandKind kind_from_operand_type(OperandType type) const {
		switch (type) {
		case OP_NONE:
			return AsmOperand_Invalid;

		case OP_GPR:
		case OP_GPR32:
		case OP_FPR_S:
		case OP_FPR_D:
		case OP_FPR_W:
		case OP_FPR_L:
		case OP_CP0:
		case OP_FCR:
		case OP_FCC:
			return AsmOperand_Register;

		case OP_SIMM16:
		case OP_UIMM16:
		case OP_SHAMT:
		case OP_UIMM5:
		case OP_UIMM6:
		case OP_CACHEOP:
		case OP_CODE10:
		case OP_CODE20:
		case OP_SEL:
		case OP_IMM32:
			return AsmOperand_Immediate;

		case OP_MEM:
			return AsmOperand_Memory;

		// Branch/jump targets are written as template labels.
		case OP_REL16:
		case OP_TARGET26:
			return AsmOperand_Label;
		}
		return AsmOperand_Invalid;
	}

	AsmRegClass reg_class_from_operand_type(OperandType type) const {
		switch (type) {
		case OP_GPR:
		case OP_GPR32:
			return AsmRegClass_Integer;

		case OP_FPR_S:
		case OP_FPR_D:
		case OP_FPR_W:
		case OP_FPR_L:
			return AsmRegClass_Float;

		default:
			// COP0/FCR/FCC slots are only fillable by name; immediates, memory and
			// labels have no register class.
			return AsmRegClass_Unknown;
		}
	}

	bool operand_type_is_implicit(OperandType t) const {
		gb_unused(t);
		return false;
	}

	bool operand_type_is_cond_code(OperandType t) const {
		gb_unused(t);
		return false;
	}

	bool is_cond_code_name(String name, u32 *bit_code_) const {
		gb_unused(name);
		gb_unused(bit_code_);
		return false;
	}

	String required_vector_feature(i32 w) const {
		// The VR4300 has no vector registers.
		gb_unused(w);
		return str_lit("");
	}

	AsmRegClass operand_type_reg_class(OperandType t) const {
		return reg_class_from_operand_type(t);
	}

	u16 operand_type_named_reg_class(OperandType t) const {
		switch (t) {
		case OP_CP0: return REG_CLASS_CP0;
		case OP_FCR: return REG_CLASS_FCR;
		case OP_FCC: return REG_CLASS_FCC;
		}
		return REG_CLASS_NONE;
	}

	String named_reg_class_string(u16 reg_class) const {
		switch (reg_class) {
		case REG_CLASS_CP0: return str_lit("COP0");
		case REG_CLASS_FCR: return str_lit("COP1 control");
		case REG_CLASS_FCC: return str_lit("FP condition code");
		}
		return str_lit("hardware");
	}

	// Width of a 32-bit GPR slot (addu, sll, mult, mfc0, ...). MIPS III leaves those
	// instructions UNPREDICTABLE on a value that is not a sign-extended 32-bit
	// value, so a u64/i64 parameter is rejected there. A literal register
	// (`addu %t0, x, y`, `mtc0 %zero, ...`) is typed at the full 64-bit GPR width
	// but carries no operand width, so literal_register_width_agnostic exempts it.
	static u16 const GPR32_SLOT_BITS = 32;

	// jal/bal write all of %ra, mult/div/mthi/mtlo all of %hi/%lo, c.cond.fmt and
	// ctc1 the whole %fcc0 bit: an implicit write is never a partial one.
	bool implicit_register_writes_are_full_width() const {
		return true;
	}

	// GPRs are one 64-bit file whatever slot they fill (see GPR32_SLOT_BITS), and
	// FPRs are 64 bits (FR=1) with the instruction choosing the S/W (32-bit) or
	// D/L (64-bit) view, so a literal %fN fills any FPR slot.
	bool literal_register_width_agnostic() const {
		return true;
	}

	// %f0..%f31 are typed f64 (FPRLEN), not u64, so they match FPR slots and f32/f64
	// parameters can be pinned to them.
	bool register_is_float(Register r) const {
		return r != REG_INVALID && reg_class(register_codes[r]) == REG_CLASS_FPR;
	}

	u16 operand_type_bit_width(OperandType t) const {
		switch (t) {
		case OP_NONE:     return 0;

		case OP_GPR:      return GPRLEN;
		case OP_GPR32:    return GPR32_SLOT_BITS;
		case OP_FPR_S:    return 32;
		case OP_FPR_D:    return 64;
		case OP_FPR_W:    return 32;
		case OP_FPR_L:    return 64;
		case OP_CP0:      return 0;
		case OP_FCR:      return 0;
		case OP_FCC:      return 0;

		case OP_SIMM16:   return 16;
		case OP_UIMM16:   return 16;
		case OP_SHAMT:    return 5;
		case OP_UIMM5:    return 5;
		case OP_UIMM6:    return 6;
		case OP_CACHEOP:  return 5;
		case OP_CODE10:   return 10;
		case OP_CODE20:   return 20;
		case OP_SEL:      return 3;
		case OP_IMM32:    return 32;

		case OP_MEM:      return 0; // access size comes from the mnemonic
		case OP_REL16:    return 16;
		case OP_TARGET26: return 26;
		}
		return 0;
	}

	bool target_has_feature(u64 enabled_features, u32 f) const {
		gb_unused(enabled_features);
		return f <= FEATURE_MACRO;
	}
	// LLVM feature names, as accepted by -target-features / -microarch.
	char const *feature_name(u32 f) const {
		switch (f) {
		case FEATURE_MIPS4:    return "mips4";
		case FEATURE_MIPS32:   return "mips32";
		case FEATURE_MIPS32R2: return "mips32r2";
		case FEATURE_MIPS64:   return "mips64";
		case FEATURE_MIPS64R2: return "mips64r2";
		}
		return "";
	}
	u16 operand_type_transfer_bytes(OperandType t) const {
		gb_unused(t);
		return 0;
	}
	bool operand_type_is_lane(OperandType t) const {
		gb_unused(t);
		return false;
	}

	// Forms the VR4300 has need no feature. Later-ISA forms name the LLVM feature
	// that enables them; with the default -microarch:mips3 it is never enabled.
	String feature_name_from_form(Encoding const &form) const {
		return make_string_c(feature_name(cast(u32)form.feature));
	}

	int form_explicit_slot(Encoding const &form, int explicit_index) const {
		int seen = 0;
		for (int j = 0; j < gb_count_of(form.ops); j++) {
			auto t = form.ops[j];
			if (!t) {
				break;
			}
			if (operand_type_is_implicit(t)) {
				continue;
			}
			if (seen == explicit_index) {
				return j;
			}
			seen += 1;
		}
		return -1;
	}

	// Bytes moved by a load/store form; 0 for everything else.
	u16 form_transfer_bytes(Encoding const &form) const {
		switch (form.mnemonic) {
		case M_LB: case M_LBU: case M_SB:
			return 1;
		case M_LH: case M_LHU: case M_SH:
			return 2;
		case M_LW: case M_LWU: case M_SW: case M_LWL: case M_LWR: case M_SWL: case M_SWR:
		case M_LL: case M_SC: case M_LWC1: case M_SWC1:
			return 4;
		case M_LD: case M_SD: case M_LDL: case M_LDR: case M_SDL: case M_SDR:
		case M_LLD: case M_SCD: case M_LDC1: case M_SDC1:
			return 8;
		}
		return 0;
	}

	bool prefix_kind_okay(u8 prefix, Encoding const &form, bool *requires_memory_dest_) const {
		// MIPS does not have prefixes
		gb_unused(prefix);
		gb_unused(form);
		gb_unused(requires_memory_dest_);
		return false;
	}

	AsmOperandConstraint operand_value_constraint(u16 m, int op) const {
		switch (m) {
		case M_SLL: case M_SRL: case M_SRA:
		case M_DSLL: case M_DSRL: case M_DSRA:
		case M_DSLL32: case M_DSRL32: case M_DSRA32:
		case M_ROTR: case M_DROTR: case M_DROTR32:
			// 5-bit shamt field: 0..<32 (word_bits is 32 under o64). Also rejects
			// negative counts, which the bare 5-bit width check would accept.
			if (op == 2) return {AsmOperandConstraint_ShiftCount, /*word bits*/-1};
			break;
		}
		return {AsmOperandConstraint_None, -1};
	}

	bool is_self_zeroing_idiom(u16 m) const {
		switch (m) {
		case M_XOR:
		case M_SUB:
		case M_SUBU:
		case M_DSUB:
		case M_DSUBU:
		case M_SLT:
		case M_SLTU:
			return true;
		}
		return false;
	}


	// ---------------------------------------------------------------------------
	// MIPS-specific queries. The generator uses operand_type_accepts_register,
	// constraint_spelling and operand_spelling; the ones marked "Not consumed yet"
	// exist for checks the generic checker does not perform yet.
	// ---------------------------------------------------------------------------

	// Optional checker hooks (see the asm_hook_* overloads in check_asm.cpp).

	// LLVM's MIPS assembler rejects an instruction the enclosing function's
	// "target-features" lack (`ext` without +mips32r2), and a template is expanded
	// into its caller, so the caller must enable what the template enables.
	bool template_features_bind_to_caller() const {
		return true;
	}

	// Every MIPS load/store, CACHE and PREF has the one offset(base) form with a
	// signed 16-bit offset. A larger constant offset would be expanded by the
	// assembler through $at instead of being encoded.
	bool memory_disp_range(Encoding const &form, OperandType slot, i64 *min_, i64 *max_) const {
		gb_unused(form);
		if (slot != OP_MEM) {
			return false;
		}
		*min_ = -32768;
		*max_ =  32767;
		return true;
	}

	// Exact range of each immediate slot (operand_type_imm_range). Replaces the
	// generic bit-width check, which accepts both readings of the bit pattern.
	bool immediate_range(Encoding const &form, OperandType slot, i64 *min_, i64 *max_) const {
		gb_unused(form);
		return operand_type_imm_range(slot, min_, max_);
	}

	// A literal write to an allocatable GPR or FPR must be pinned or declared:
	// LLVM may keep a live value in any of them across the template. The
	// non-allocatable registers are never handed out ($zero, $k0/$k1, $gp, $sp),
	// and %ra/%hi/%lo/%fcc0 are tracked and clobbered automatically.
	bool register_write_needs_clobber(Register r) const {
		if (r == REG_INVALID || reg_is_non_allocateable(r)) {
			return false;
		}
		if (r == REG_RA) {
			return false; // tracked (ClobberReg_RA)
		}
		u16 cls = reg_class(register_codes[r]);
		return cls == REG_CLASS_GPR || cls == REG_CLASS_FPR;
	}

	// Per-value rejection of a constant immediate the range check cannot express:
	// nullptr if `value` is fine for explicit operand `op` of mnemonic `m`.
	char const *operand_value_reject_reason(u16 m, int op, i64 value) const {
		if (m == M_CACHE && op == 0 && !cache_op_is_vr4300(value)) {
			return "is not a VR4300 cache operation (valid: 0x00 0x04 0x08 0x10 0x14 0x18 for the I-cache, 0x01 0x05 0x09 0x0D 0x11 0x15 0x19 for the D-cache)";
		}
		return nullptr;
	}

	// Can a literal register `r` fill a slot of type `t`? The Odin type a literal
	// gets (an integer for every non-FPR name) would let `addu r, %c0_status, x` or
	// `addu r, %hi, x` through the generic class comparison, so the checker asks
	// literal_register_reject_reason first, and the generator asserts the same
	// before LLVM sees `$12`.
	bool operand_type_accepts_register(OperandType t, Register r) const {
		if (r == REG_INVALID) {
			return false;
		}
		u16 cls = reg_class(register_codes[r]);
		switch (t) {
		case OP_GPR:
		case OP_GPR32:
			return cls == REG_CLASS_GPR;
		case OP_FPR_S:
		case OP_FPR_D:
		case OP_FPR_W:
		case OP_FPR_L:
			return cls == REG_CLASS_FPR;
		case OP_CP0: return cls == REG_CLASS_CP0;
		case OP_FCR: return cls == REG_CLASS_FCR;
		case OP_FCC: return cls == REG_CLASS_FCC;
		}
		return false;
	}

	// What a register slot of type `t` needs, phrased to follow "it must be".
	char const *operand_type_register_description(OperandType t) const {
		switch (t) {
		case OP_GPR:
		case OP_GPR32:   return "a general-purpose register";
		case OP_FPR_S:
		case OP_FPR_D:
		case OP_FPR_W:
		case OP_FPR_L:   return "a floating-point register (%f0..%f31)";
		case OP_CP0:     return "a COP0 register (%c0_*)";
		case OP_FCR:     return "a COP1 control register (%fcr0, %fcr31)";
		case OP_FCC:     return "an FP condition code (%fcc0)";
		}
		return "a register";
	}

	// Checker hook: rejects `addu r, %c0_status, x`, `addu r, %hi, x`,
	// `add_s r, %t0, b` and the like at `odin check` (operand_type_accepts_register).
	char const *literal_register_reject_reason(OperandType t, Register r) const {
		if (operand_type_accepts_register(t, r)) {
			return nullptr;
		}
		return operand_type_register_description(t);
	}

	// Checker hook: a pin LLVM cannot honour, on an input, output or scratch
	// parameter. COP0 and COP1 control registers have no LLVM constraint (a pin
	// would silently mean GPR N), and LLVM cannot copy a value into or out of
	// $fcc0 (copyPhysReg asserts). The generator asserts the same.
	char const *pin_reject_reason(Register r) const {
		if (r == REG_INVALID) {
			return nullptr;
		}
		switch (reg_class(register_codes[r])) {
		case REG_CLASS_FCC:
			return "LLVM cannot copy a value into or out of the FP condition bit; use a 'c_*' compare or 'bc1t'/'bc1f' in the template";
		case REG_CLASS_CP0:
		case REG_CLASS_FCR:
			return "LLVM has no register constraint for it; move the value with 'mtc0'/'mfc0' or 'ctc1'/'cfc1' in the template";
		}
		return nullptr;
	}

	// Is the instruction UNPREDICTABLE when its destination GPR is also a source?
	// `jalr rd, rs` with rd == rs is (MIPS III: not restartable). The generator
	// then makes the template's outputs early-clobber so LLVM keeps them apart.
	bool destination_must_differ_from_sources(u16 m) const {
		switch (m) {
		case M_JALR:
		case M_JALR_HB:
			return true;
		}
		return false;
	}

	// What a GPR (or HI/LO) write leaves in the upper 32 bits. LLVM keeps every
	// value narrower than 64 bits sign-extended from bit 31 in its 64-bit GPR and
	// compares, shifts and spills it as such, so the generator requests an output
	// narrower than 64 bits as i64 (and truncates it, which re-sign-extends) when
	// any write to it may leave the upper half set some other way.
	enum WordWrite : u8 {
		WordWrite_Doubleword,    // may set bits 63..32 freely: ld, lwu, daddu, dsll, dmfc0, dmfc1, ...
		WordWrite_Word,          // always a sign-extended word (or 0/1): lw, lui, li, slt, andi, mult, any GPR32 slot
		WordWrite_WordIfSources, // a sign-extended word when every register it reads is one: move, or, nor, xori, mthi
		WordWrite_FromHiLo,      // mfhi/mflo: whatever the last HI/LO write left
	};

	// The write to an explicit GPR slot of type `slot_type` in `form`.
	WordWrite gpr_word_write(Encoding const &form, OperandType slot_type) const {
		if (slot_type == OP_GPR32) {
			return WordWrite_Word;
		}
		switch (form.mnemonic) {
		case M_LB: case M_LBU: case M_LH: case M_LHU: case M_LW: case M_LL:
		case M_SC:                                   // 0 or 1
		case M_LUI: case M_LI:                       // li takes a 32-bit immediate, sign-extended
		case M_SLT: case M_SLTU: case M_SLTI: case M_SLTIU:
		case M_ANDI:                                 // bits 63..16 cleared
		case M_JALR: case M_JALR_HB:                 // the link address, a sign-extended 32-bit pc
			return WordWrite_Word;
		case M_MOVE: case M_NOT:
		case M_AND: case M_OR: case M_XOR: case M_NOR:
		case M_ORI: case M_XORI:                     // bits 63..16 come from rs
		case M_MOVZ: case M_MOVN:
			return WordWrite_WordIfSources;
		case M_MFHI: case M_MFLO:
			return WordWrite_FromHiLo;
		}
		return WordWrite_Doubleword;
	}

	// The implicit HI/LO write of mnemonic `m` (its Clobber::implicit_wr has HI or LO).
	WordWrite hilo_word_write(u16 m) const {
		switch (m) {
		case M_MULT: case M_MULTU: case M_DIV: case M_DIVU:
		case M_MUL: case M_MADD: case M_MADDU: case M_MSUB: case M_MSUBU:
			return WordWrite_Word;
		case M_MTHI: case M_MTLO:
			return WordWrite_WordIfSources;
		}
		return WordWrite_Doubleword; // dmult, dmultu, ddiv, ddivu
	}

	// Does the assembler (under `.set reorder`) follow this instruction with a
	// delay-slot nop? Every branch, jump and call does; eret has no delay slot.
	bool has_delay_slot(u16 m, Clobber const &clobber) const {
		return clobber.has_control() && m != M_ERET;
	}

	// LLVM register-constraint spelling written into `buf`; returns false when LLVM
	// has no constraint for the register (COP0 and COP1 control registers can be
	// neither pinned nor clobbered). Named GPRs (`{$t1}`) assert in LLVM's Mips
	// backend, so numbers are used. $fcc0 is valid as a clobber only: a pin to it
	// asserts in copyPhysReg, see reg_is_non_allocateable.
	bool constraint_spelling(Register r, char *buf, isize cap) const {
		int n = cast(int)reg_number(r);
		switch (reg_class(register_codes[r])) {
		case REG_CLASS_GPR:  gb_snprintf(buf, cap, "$%d", n);   return true;
		case REG_CLASS_FPR:  gb_snprintf(buf, cap, "$f%d", n);  return true;
		case REG_CLASS_HILO: gb_snprintf(buf, cap, "%s", n == 0 ? "hi" : "lo"); return true;
		case REG_CLASS_FCC:  gb_snprintf(buf, cap, "$fcc%d", n); return true;
		}
		return false;
	}

	// Spelling of a literal register inside the template text. `$` must be doubled
	// in an LLVM asm string, so these come out as `$$N` / `$$fN` in the IR.
	bool operand_spelling(Register r, char *buf, isize cap) const {
		int n = cast(int)reg_number(r);
		switch (reg_class(register_codes[r])) {
		case REG_CLASS_GPR:
		case REG_CLASS_CP0:
		case REG_CLASS_FCR:  gb_snprintf(buf, cap, "$$%d", n);    return true;
		case REG_CLASS_FPR:  gb_snprintf(buf, cap, "$$f%d", n);   return true;
		case REG_CLASS_FCC:  gb_snprintf(buf, cap, "$$fcc%d", n); return true;
		}
		return false; // %hi / %lo are never explicit operands
	}

	// Optional checker hook. Why a register name that is not in the table is
	// unavailable, phrased to follow "Register %<name> ", for a better message
	// than "Unknown register" (check_register).
	char const *register_unavailable_reason(String const &name) const {
		if (name == "at") {
			return "($1) is reserved for the assembler, which uses it to expand macros (large offsets, li, ...); "
			       "every MIPS asm template clobbers it, so it cannot be named in one";
		}
		return nullptr;
	}

	// Exact immediate range of a slot (consumed through immediate_range): the
	// generic check accepts either a signed or an unsigned reading of the bit
	// pattern, but LLVM rejects `andi r, s, -1` ("expected 16-bit unsigned
	// immediate"), `slti r, s, 65535` and `cache -1, ...` at code generation.
	// `li` takes what the assembler takes: -2^31..2^32-1.
	bool operand_type_imm_range(OperandType t, i64 *lo_, i64 *hi_) const {
		i64 lo = 0, hi = 0;
		switch (t) {
		case OP_SIMM16:  lo = -32768;              hi = 32767;              break;
		case OP_UIMM16:  lo = 0;                   hi = 65535;              break;
		case OP_SHAMT:   lo = 0;                   hi = 31;                 break;
		case OP_UIMM5:   lo = 0;                   hi = 31;                 break;
		case OP_UIMM6:   lo = 0;                   hi = 63;                 break;
		case OP_CACHEOP: lo = 0;                   hi = 31;                 break;
		case OP_CODE10:  lo = 0;                   hi = 1023;               break;
		case OP_CODE20:  lo = 0;                   hi = (1<<20)-1;          break;
		case OP_SEL:     lo = 0;                   hi = 7;                  break;
		case OP_IMM32:   lo = -(cast(i64)1<<31);   hi = (cast(i64)1<<32)-1; break;
		default:
			return false;
		}
		if (lo_) *lo_ = lo;
		if (hi_) *hi_ = hi;
		return true;
	}

	// CACHE operations the VR4300 implements: op = (operation<<2)|cache,
	// primary caches only (0 = I, 1 = D). Codes 2/3 (secondary caches), operation 7
	// and Create_Dirty_Exclusive on the I-cache (0x0C) do not exist on the VR4300.
	static bool cache_op_is_vr4300(i64 op) {
		switch (op) {
		case 0x00: // Index_Invalidate (I)
		case 0x04: // Index_Load_Tag (I)
		case 0x08: // Index_Store_Tag (I)
		case 0x10: // Hit_Invalidate (I)
		case 0x14: // Fill (I)
		case 0x18: // Hit_Write_Back (I)
		case 0x01: // Index_Write_Back_Invalidate (D)
		case 0x05: // Index_Load_Tag (D)
		case 0x09: // Index_Store_Tag (D)
		case 0x0D: // Create_Dirty_Exclusive (D)
		case 0x11: // Hit_Invalidate (D)
		case 0x15: // Hit_Write_Back_Invalidate (D)
		case 0x19: // Hit_Write_Back (D)
			return true;
		}
		return false;
	}

};



gb_global Asm_mips g_asm_mips;



String const Asm_mips::prefix_strings[Asm_mips::PREFIX_COUNT]{};
String const Asm_mips::pseudo_mnemonic_strings[Asm_mips::PSEUDO_MNEMONIC_COUNT]{};

String const Asm_mips::mnemonic_strings[Asm_mips::MNEMONIC_COUNT] {
	str_lit(""),
#define ASM_MIPS_X(e, s) str_lit(s),
	ASM_MIPS_MNEMONICS(ASM_MIPS_X)
#undef ASM_MIPS_X
};

u16 const Asm_mips::register_codes[Asm_mips::REG_COUNT] {
	0,
#define ASM_MIPS_X(e, s, c, n) cast(u16)(Asm_mips::REG_CLASS_##c | (n)),
	ASM_MIPS_REGISTERS(ASM_MIPS_X)
#undef ASM_MIPS_X
};

String const Asm_mips::register_strings[Asm_mips::REG_COUNT] {
	str_lit(""),
#define ASM_MIPS_X(e, s, c, n) str_lit(s),
	ASM_MIPS_REGISTERS(ASM_MIPS_X)
#undef ASM_MIPS_X
};


// -----------------------------------------------------------------------------
// Operand shapes
// -----------------------------------------------------------------------------

enum Asm_mips::Shape : u8 {
	SH_NONE,
	SH_R3_64,      // rd, rs, rt                       and, or, slt, daddu, ...
	SH_R3_32,      // rd, rs, rt (32-bit)              addu, subu, add, sub, mul
	SH_SHV_32,     // rd, rt, rs (32-bit value)        sllv, srlv, srav, rotrv
	SH_SHV_64,     // rd, rt, rs                       dsllv, dsrlv, dsrav, drotrv
	SH_SHI_32,     // rd, rt, sa (32-bit value)        sll, srl, sra, rotr
	SH_SHI_64,     // rd, rt, sa                       dsll, dsll32, ..., drotr
	SH_I_S32,      // rt, rs, simm16 (32-bit)          addi, addiu
	SH_I_S64,      // rt, rs, simm16                   daddi, daddiu, slti, sltiu
	SH_I_U64,      // rt, rs, uimm16                   andi, ori, xori
	SH_LUI,        // rt, uimm16
	SH_LI,         // rt, imm32                        li (macro)
	SH_MD_32,      // rs, rt (32-bit)                  mult, div, madd, ...
	SH_MD_64,      // rs, rt                           dmult, ddiv, ...
	SH_RD,         // rd                               mfhi, mflo
	SH_RS,         // rs                               mthi, mtlo, jr, jalr
	SH_RD_RS,      // rd, rs                           jalr, move, not, clz
	SH_RD_RT_32,   // rd, rt (32-bit)                  negu, seb, seh, wsbh
	SH_RD_RT_64,   // rd, rt                           dnegu, dsbh, dshd
	SH_RT,         // rt                               di, ei
	SH_MEM,        // rt, offset(base)                 loads, stores, ll, sc
	SH_FMEM_S,     // ft, offset(base) (single)        lwc1, swc1
	SH_FMEM_D,     // ft, offset(base) (double)        ldc1, sdc1
	SH_CACHE,      // op, offset(base)                 cache
	SH_PREF,       // hint, offset(base)               pref
	SH_MEM_ONLY,   // offset(base)                     synci
	SH_BR2,        // rs, rt, label                    beq, bne, ...
	SH_BR1,        // rs, label                        blez, bgez, beqz, ...
	SH_BR0,        // label                            b, bal, bc1t, ...
	SH_JUMP,       // label                            j, jal
	SH_TRAP_CODE,  // rs, rt, code                     teq, ...
	SH_TRAPI,      // rs, simm16                       teqi, ...
	SH_CODE10,     // code                             break
	SH_CODE10x2,   // code, code                       break
	SH_CODE20,     // code                             syscall
	SH_CP0_32,     // rt, cp0                          mfc0, mtc0
	SH_CP0_64,     // rt, cp0                          dmfc0, dmtc0
	SH_CP0_SEL,    // rt, cp0, sel                     mfc0/mtc0 (MIPS32)
	SH_FCR,        // rt, fcr                          cfc1, ctc1
	SH_C1_32,      // rt, fs (32-bit)                  mfc1, mtc1
	SH_C1_64,      // rt, fs (64-bit)                  dmfc1, dmtc1
	SH_C1_HI,      // rt, fs (high half of a double)   mfhc1, mthc1
	SH_F3_S,       // fd, fs, ft                       add.s, ...
	SH_F3_D,
	SH_F2_S,       // fd, fs                           sqrt.s, abs.s, neg.s, mov.s, ...
	SH_F2_D,
	SH_CVT_S_D, SH_CVT_S_W, SH_CVT_S_L,
	SH_CVT_D_S, SH_CVT_D_W, SH_CVT_D_L,
	SH_CVT_W_S, SH_CVT_W_D, SH_CVT_L_S, SH_CVT_L_D,
	SH_FCMP_S,     // fs, ft                           c.cond.s
	SH_FCMP_D,
	SH_F4_S,       // fd, fr, fs, ft                   madd.s, ...
	SH_F4_D,
	SH_FMOVC_S,    // fd, fs, rt                       movz.s, movn.s
	SH_FMOVC_D,
	SH_EXT,        // rt, rs, pos, size                ext, ins
	SH_DEXT,       // rt, rs, pos, size                dext, dins
	SH_RDHWR,      // rt, hwr                          rdhwr

	SHAPE_COUNT
};

#define MS_O(o, e) Asm_mips::o
#define MS_E(o, e) Asm_mips::e
#define MS(a, b, c, d) {{MS_O a, MS_O b, MS_O c, MS_O d}, {MS_E a, MS_E b, MS_E c, MS_E d}}
#define N_ (OP_NONE,ENC_NONE)
Asm_mips::FormShape const Asm_mips::form_shapes[SHAPE_COUNT] = {
	/* SH_NONE      */ MS(N_, N_, N_, N_),
	/* SH_R3_64     */ MS((OP_GPR,ENC_RD), (OP_GPR,ENC_RS), (OP_GPR,ENC_RT), N_),
	/* SH_R3_32     */ MS((OP_GPR32,ENC_RD), (OP_GPR32,ENC_RS), (OP_GPR32,ENC_RT), N_),
	/* SH_SHV_32    */ MS((OP_GPR32,ENC_RD), (OP_GPR32,ENC_RT), (OP_GPR,ENC_RS), N_),
	/* SH_SHV_64    */ MS((OP_GPR,ENC_RD), (OP_GPR,ENC_RT), (OP_GPR,ENC_RS), N_),
	/* SH_SHI_32    */ MS((OP_GPR32,ENC_RD), (OP_GPR32,ENC_RT), (OP_SHAMT,ENC_SA), N_),
	/* SH_SHI_64    */ MS((OP_GPR,ENC_RD), (OP_GPR,ENC_RT), (OP_SHAMT,ENC_SA), N_),
	/* SH_I_S32     */ MS((OP_GPR32,ENC_RT), (OP_GPR32,ENC_RS), (OP_SIMM16,ENC_IMM16), N_),
	/* SH_I_S64     */ MS((OP_GPR,ENC_RT), (OP_GPR,ENC_RS), (OP_SIMM16,ENC_IMM16), N_),
	/* SH_I_U64     */ MS((OP_GPR,ENC_RT), (OP_GPR,ENC_RS), (OP_UIMM16,ENC_IMM16), N_),
	/* SH_LUI       */ MS((OP_GPR,ENC_RT), (OP_UIMM16,ENC_IMM16), N_, N_),
	/* SH_LI        */ MS((OP_GPR,ENC_RT), (OP_IMM32,ENC_IMM_MACRO), N_, N_),
	/* SH_MD_32     */ MS((OP_GPR32,ENC_RS), (OP_GPR32,ENC_RT), N_, N_),
	/* SH_MD_64     */ MS((OP_GPR,ENC_RS), (OP_GPR,ENC_RT), N_, N_),
	/* SH_RD        */ MS((OP_GPR,ENC_RD), N_, N_, N_),
	/* SH_RS        */ MS((OP_GPR,ENC_RS), N_, N_, N_),
	/* SH_RD_RS     */ MS((OP_GPR,ENC_RD), (OP_GPR,ENC_RS), N_, N_),
	/* SH_RD_RT_32  */ MS((OP_GPR32,ENC_RD), (OP_GPR32,ENC_RT), N_, N_),
	/* SH_RD_RT_64  */ MS((OP_GPR,ENC_RD), (OP_GPR,ENC_RT), N_, N_),
	/* SH_RT        */ MS((OP_GPR,ENC_RT), N_, N_, N_),
	/* SH_MEM       */ MS((OP_GPR,ENC_RT), (OP_MEM,ENC_OFFSET_BASE), N_, N_),
	/* SH_FMEM_S    */ MS((OP_FPR_S,ENC_FT), (OP_MEM,ENC_OFFSET_BASE), N_, N_),
	/* SH_FMEM_D    */ MS((OP_FPR_D,ENC_FT), (OP_MEM,ENC_OFFSET_BASE), N_, N_),
	/* SH_CACHE     */ MS((OP_CACHEOP,ENC_CACHE_OP), (OP_MEM,ENC_OFFSET_BASE), N_, N_),
	/* SH_PREF      */ MS((OP_UIMM5,ENC_HINT), (OP_MEM,ENC_OFFSET_BASE), N_, N_),
	/* SH_MEM_ONLY  */ MS((OP_MEM,ENC_OFFSET_BASE), N_, N_, N_),
	/* SH_BR2       */ MS((OP_GPR,ENC_RS), (OP_GPR,ENC_RT), (OP_REL16,ENC_BRANCH16), N_),
	/* SH_BR1       */ MS((OP_GPR,ENC_RS), (OP_REL16,ENC_BRANCH16), N_, N_),
	/* SH_BR0       */ MS((OP_REL16,ENC_BRANCH16), N_, N_, N_),
	/* SH_JUMP      */ MS((OP_TARGET26,ENC_TARGET26), N_, N_, N_),
	/* SH_TRAP_CODE */ MS((OP_GPR,ENC_RS), (OP_GPR,ENC_RT), (OP_CODE10,ENC_CODE10), N_),
	/* SH_TRAPI     */ MS((OP_GPR,ENC_RS), (OP_SIMM16,ENC_IMM16), N_, N_),
	/* SH_CODE10    */ MS((OP_CODE10,ENC_CODE10), N_, N_, N_),
	/* SH_CODE10x2  */ MS((OP_CODE10,ENC_CODE10), (OP_CODE10,ENC_CODE10), N_, N_),
	/* SH_CODE20    */ MS((OP_CODE20,ENC_CODE20), N_, N_, N_),
	/* SH_CP0_32    */ MS((OP_GPR32,ENC_RT), (OP_CP0,ENC_CP0_RD), N_, N_),
	/* SH_CP0_64    */ MS((OP_GPR,ENC_RT), (OP_CP0,ENC_CP0_RD), N_, N_),
	/* SH_CP0_SEL   */ MS((OP_GPR32,ENC_RT), (OP_CP0,ENC_CP0_RD), (OP_SEL,ENC_SEL), N_),
	/* SH_FCR       */ MS((OP_GPR32,ENC_RT), (OP_FCR,ENC_FCR_RD), N_, N_),
	/* SH_C1_32     */ MS((OP_GPR32,ENC_RT), (OP_FPR_S,ENC_FS), N_, N_),
	/* SH_C1_64     */ MS((OP_GPR,ENC_RT), (OP_FPR_D,ENC_FS), N_, N_),
	/* SH_C1_HI     */ MS((OP_GPR32,ENC_RT), (OP_FPR_D,ENC_FS), N_, N_),
	/* SH_F3_S      */ MS((OP_FPR_S,ENC_FD), (OP_FPR_S,ENC_FS), (OP_FPR_S,ENC_FT), N_),
	/* SH_F3_D      */ MS((OP_FPR_D,ENC_FD), (OP_FPR_D,ENC_FS), (OP_FPR_D,ENC_FT), N_),
	/* SH_F2_S      */ MS((OP_FPR_S,ENC_FD), (OP_FPR_S,ENC_FS), N_, N_),
	/* SH_F2_D      */ MS((OP_FPR_D,ENC_FD), (OP_FPR_D,ENC_FS), N_, N_),
	/* SH_CVT_S_D   */ MS((OP_FPR_S,ENC_FD), (OP_FPR_D,ENC_FS), N_, N_),
	/* SH_CVT_S_W   */ MS((OP_FPR_S,ENC_FD), (OP_FPR_W,ENC_FS), N_, N_),
	/* SH_CVT_S_L   */ MS((OP_FPR_S,ENC_FD), (OP_FPR_L,ENC_FS), N_, N_),
	/* SH_CVT_D_S   */ MS((OP_FPR_D,ENC_FD), (OP_FPR_S,ENC_FS), N_, N_),
	/* SH_CVT_D_W   */ MS((OP_FPR_D,ENC_FD), (OP_FPR_W,ENC_FS), N_, N_),
	/* SH_CVT_D_L   */ MS((OP_FPR_D,ENC_FD), (OP_FPR_L,ENC_FS), N_, N_),
	/* SH_CVT_W_S   */ MS((OP_FPR_W,ENC_FD), (OP_FPR_S,ENC_FS), N_, N_),
	/* SH_CVT_W_D   */ MS((OP_FPR_W,ENC_FD), (OP_FPR_D,ENC_FS), N_, N_),
	/* SH_CVT_L_S   */ MS((OP_FPR_L,ENC_FD), (OP_FPR_S,ENC_FS), N_, N_),
	/* SH_CVT_L_D   */ MS((OP_FPR_L,ENC_FD), (OP_FPR_D,ENC_FS), N_, N_),
	/* SH_FCMP_S    */ MS((OP_FPR_S,ENC_FS), (OP_FPR_S,ENC_FT), N_, N_),
	/* SH_FCMP_D    */ MS((OP_FPR_D,ENC_FS), (OP_FPR_D,ENC_FT), N_, N_),
	/* SH_F4_S      */ MS((OP_FPR_S,ENC_FD), (OP_FPR_S,ENC_FR), (OP_FPR_S,ENC_FS), (OP_FPR_S,ENC_FT)),
	/* SH_F4_D      */ MS((OP_FPR_D,ENC_FD), (OP_FPR_D,ENC_FR), (OP_FPR_D,ENC_FS), (OP_FPR_D,ENC_FT)),
	/* SH_FMOVC_S   */ MS((OP_FPR_S,ENC_FD), (OP_FPR_S,ENC_FS), (OP_GPR,ENC_RT), N_),
	/* SH_FMOVC_D   */ MS((OP_FPR_D,ENC_FD), (OP_FPR_D,ENC_FS), (OP_GPR,ENC_RT), N_),
	/* SH_EXT       */ MS((OP_GPR,ENC_RT), (OP_GPR,ENC_RS), (OP_SHAMT,ENC_POS), (OP_UIMM6,ENC_SIZE)),
	/* SH_DEXT      */ MS((OP_GPR,ENC_RT), (OP_GPR,ENC_RS), (OP_UIMM6,ENC_POS), (OP_UIMM6,ENC_SIZE)),
	/* SH_RDHWR     */ MS((OP_GPR,ENC_RT), (OP_UIMM5,ENC_RD), N_, N_),
};
#undef N_
#undef MS
#undef MS_O
#undef MS_E


// -----------------------------------------------------------------------------
// Forms. Rows for one mnemonic must be adjacent and appear in Mnemonic order.
// -----------------------------------------------------------------------------

// Clobber record: CLB(written, read, implicit_wr, implicit_rd, writes_mem, reads_mem, side_effects)
#define MIPS_CLB(wr, rd, iwr, ird, wm, rm, se) {cast(Asm_mips::OperandSet)(wr), cast(Asm_mips::OperandSet)(rd), \
	cast(Asm_mips::ClobberRegs)(iwr), cast(Asm_mips::ClobberRegs)(ird), Asm_mips::ClobberFlag_None, wm, rm, \
	cast(Asm_mips::SideEffectFlags)(se)}

#define MIPS_O0 Asm_mips::OperandSet_OP0
#define MIPS_O1 Asm_mips::OperandSet_OP1
#define MIPS_O2 Asm_mips::OperandSet_OP2
#define MIPS_O3 Asm_mips::OperandSet_OP3
#define MIPS_RA Asm_mips::ClobberReg_RA
#define MIPS_HI Asm_mips::ClobberReg_HI
#define MIPS_LO Asm_mips::ClobberReg_LO
#define MIPS_FCC Asm_mips::ClobberReg_FCC
#define MIPS_CTL  Asm_mips::SideEffectFlag_CONTROL
#define MIPS_COND Asm_mips::SideEffectFlag_COND
#define MIPS_CALL Asm_mips::SideEffectFlag_CALL
#define MIPS_TRAP Asm_mips::SideEffectFlag_TRAP
#define MIPS_SYS  Asm_mips::SideEffectFlag_SYSTEM
#define MIPS_FNC  Asm_mips::SideEffectFlag_FENCE
#define MIPS_ATM  Asm_mips::SideEffectFlag_ATOMIC
#define MIPS_FCSR Asm_mips::SideEffectFlag_FPCSR

#define MIPS_C_NONE      MIPS_CLB(0,  0,        0,     0,     false, false, 0)
#define MIPS_C_W0_R12    MIPS_CLB(MIPS_O0, MIPS_O1|MIPS_O2,    0,     0,     false, false, 0)          // rd <- rs op rt
#define MIPS_C_W0_R1     MIPS_CLB(MIPS_O0, MIPS_O1,       0,     0,     false, false, 0)          // rd <- op rs
#define MIPS_C_W0        MIPS_CLB(MIPS_O0, 0,        0,     0,     false, false, 0)          // lui, li
#define MIPS_C_MULDIV    MIPS_CLB(0,  MIPS_O0|MIPS_O1,    MIPS_HI|MIPS_LO, 0,     false, false, 0)          // HI:LO <- rs op rt
#define MIPS_C_MACC      MIPS_CLB(0,  MIPS_O0|MIPS_O1,    MIPS_HI|MIPS_LO, MIPS_HI|MIPS_LO, false, false, 0)          // HI:LO += rs * rt
#define MIPS_C_MFHI      MIPS_CLB(MIPS_O0, 0,        0,     MIPS_HI,    false, false, 0)
#define MIPS_C_MFLO      MIPS_CLB(MIPS_O0, 0,        0,     MIPS_LO,    false, false, 0)
#define MIPS_C_MTHI      MIPS_CLB(0,  MIPS_O0,       MIPS_HI,    0,     false, false, 0)
#define MIPS_C_MTLO      MIPS_CLB(0,  MIPS_O0,       MIPS_LO,    0,     false, false, 0)
#define MIPS_C_CMOV      MIPS_CLB(MIPS_O0, MIPS_O0|MIPS_O1|MIPS_O2, 0,     0,     false, false, 0)          // movz/movn: rd kept if not moved
#define MIPS_C_LOAD      MIPS_CLB(MIPS_O0, MIPS_O1,       0,     0,     false, true,  0)
#define MIPS_C_STORE     MIPS_CLB(0,  MIPS_O0|MIPS_O1,    0,     0,     true,  false, 0)
#define MIPS_C_LL        MIPS_CLB(MIPS_O0, MIPS_O1,       0,     0,     false, true,  MIPS_ATM)
#define MIPS_C_SC        MIPS_CLB(MIPS_O0, MIPS_O0|MIPS_O1,    0,     0,     true,  true,  MIPS_ATM)        // rt <- 1 on success, 0 on failure
#define MIPS_C_BR2       MIPS_CLB(0,  MIPS_O0|MIPS_O1,    0,     0,     false, false, MIPS_CTL|MIPS_COND)
#define MIPS_C_BR1       MIPS_CLB(0,  MIPS_O0,       0,     0,     false, false, MIPS_CTL|MIPS_COND)
#define MIPS_C_BR1_LINK  MIPS_CLB(0,  MIPS_O0,       MIPS_RA,    0,     false, false, MIPS_CTL|MIPS_COND)
#define MIPS_C_BR0       MIPS_CLB(0,  0,        0,     0,     false, false, MIPS_CTL)        // b, j
#define MIPS_C_BR0_LINK  MIPS_CLB(0,  0,        MIPS_RA,    0,     false, false, MIPS_CTL|MIPS_CALL)   // bal, jal
#define MIPS_C_JR        MIPS_CLB(0,  MIPS_O0,       0,     0,     false, false, MIPS_CTL)
// jalr calls code outside the template, which may read and write any memory.
#define MIPS_C_JALR1     MIPS_CLB(0,  MIPS_O0,       MIPS_RA,    0,     true,  true,  MIPS_CTL|MIPS_CALL)
#define MIPS_C_JALR2     MIPS_CLB(MIPS_O0, MIPS_O1,       0,     0,     true,  true,  MIPS_CTL|MIPS_CALL)
#define MIPS_C_BC1       MIPS_CLB(0,  0,        0,     MIPS_FCC,   false, false, MIPS_CTL|MIPS_COND)
#define MIPS_C_FCMP      MIPS_CLB(0,  MIPS_O0|MIPS_O1,    MIPS_FCC,   0,     false, false, 0)
#define MIPS_C_TRAP2     MIPS_CLB(0,  MIPS_O0|MIPS_O1,    0,     0,     false, false, MIPS_TRAP)
#define MIPS_C_TRAP1     MIPS_CLB(0,  MIPS_O0,       0,     0,     false, false, MIPS_TRAP)
#define MIPS_C_TRAP0     MIPS_CLB(0,  0,        0,     0,     false, false, MIPS_TRAP)
#define MIPS_C_MFC0      MIPS_CLB(MIPS_O0, 0,        0,     0,     false, false, MIPS_SYS)
#define MIPS_C_MTC0      MIPS_CLB(0,  MIPS_O0,       0,     0,     false, false, MIPS_SYS)
#define MIPS_C_SYS_MEM   MIPS_CLB(0,  0,        0,     0,     true,  true,  MIPS_SYS)        // tlbw*, tlbp, tlbr
#define MIPS_C_SYS       MIPS_CLB(0,  0,        0,     0,     false, false, MIPS_SYS)        // ehb, wait
#define MIPS_C_SYS_W0    MIPS_CLB(MIPS_O0, 0,        0,     0,     false, false, MIPS_SYS)        // di, ei, rdhwr
#define MIPS_C_ERET      MIPS_CLB(0,  0,        0,     0,     true,  true,  MIPS_SYS|MIPS_CTL)
#define MIPS_C_CACHE     MIPS_CLB(0,  MIPS_O1,       0,     0,     true,  true,  MIPS_SYS)
#define MIPS_C_SYNCI     MIPS_CLB(0,  MIPS_O0,       0,     0,     true,  true,  MIPS_SYS)
#define MIPS_C_PREF      MIPS_CLB(0,  MIPS_O1,       0,     0,     false, true,  0)
#define MIPS_C_SYNC      MIPS_CLB(0,  0,        0,     0,     true,  true,  MIPS_FNC)
#define MIPS_C_MFC1      MIPS_CLB(MIPS_O0, MIPS_O1,       0,     0,     false, false, 0)
#define MIPS_C_MTC1      MIPS_CLB(MIPS_O1, MIPS_O0,       0,     0,     false, false, 0)          // the FPR (operand 1) is written
#define MIPS_C_MTHC1     MIPS_CLB(MIPS_O1, MIPS_O0|MIPS_O1,    0,     0,     false, false, 0)          // writes the high half only
#define MIPS_C_CFC1      MIPS_CLB(MIPS_O0, 0,        0,     0,     false, false, MIPS_FCSR)
#define MIPS_C_CTC1      MIPS_CLB(0,  MIPS_O0,       MIPS_FCC,   0,     false, false, MIPS_FCSR)       // FCSR bit 23 is the condition bit
#define MIPS_C_F4        MIPS_CLB(MIPS_O0, MIPS_O1|MIPS_O2|MIPS_O3, 0,     0,     false, false, 0)

// Row: mnemonic, shape, encoding bits, mask of the fixed bits, feature, Clobber
#define R(M, SH, BITS, MASK, FEAT, CLOB) {Asm_mips::M_##M, Asm_mips::SH, BITS, MASK, Asm_mips::FEATURE_##FEAT, CLOB}

// COP1 helpers: fmt S=16, D=17, W=20, L=21
#define FP3(fmt, fn)  (0x44000000u | ((fmt)<<21) | (fn))
#define FPFMT_S 16u
#define FPFMT_D 17u
#define FPFMT_W 20u
#define FPFMT_L 21u

Asm_mips::FormRow const Asm_mips::form_rows[] = {
	// system, cache, COP0
	R(NOP,     SH_NONE,      0x00000000, 0xFFFFFFFF, MACRO,    MIPS_C_NONE),      // sll $0, $0, 0
	R(SSNOP,   SH_NONE,      0x00000040, 0xFFFFFFFF, MACRO,    MIPS_C_NONE),      // sll $0, $0, 1
	R(SYNC,    SH_NONE,      0x0000000F, 0xFFFFFFFF, MIPS_II,  MIPS_C_SYNC),
	R(SYSCALL, SH_NONE,      0x0000000C, 0xFFFFFFFF, MIPS_I,   MIPS_C_TRAP0),
	R(SYSCALL, SH_CODE20,    0x0000000C, 0xFC00003F, MIPS_I,   MIPS_C_TRAP0),
	R(BREAK,   SH_NONE,      0x0000000D, 0xFFFFFFFF, MIPS_I,   MIPS_C_TRAP0),
	R(BREAK,   SH_CODE10,    0x0000000D, 0xFC00FFFF, MIPS_I,   MIPS_C_TRAP0),
	R(BREAK,   SH_CODE10x2,  0x0000000D, 0xFC00003F, MIPS_I,   MIPS_C_TRAP0),
	R(CACHE,   SH_CACHE,     0xBC000000, 0xFC000000, MIPS_III, MIPS_C_CACHE),
	R(MFC0,    SH_CP0_32,    0x40000000, 0xFFE007FF, COP0,     MIPS_C_MFC0),
	R(MFC0,    SH_CP0_SEL,   0x40000000, 0xFFE007F8, MIPS32,   MIPS_C_MFC0),
	R(MTC0,    SH_CP0_32,    0x40800000, 0xFFE007FF, COP0,     MIPS_C_MTC0),
	R(MTC0,    SH_CP0_SEL,   0x40800000, 0xFFE007F8, MIPS32,   MIPS_C_MTC0),
	R(DMFC0,   SH_CP0_64,    0x40200000, 0xFFE007FF, COP0,     MIPS_C_MFC0),
	R(DMTC0,   SH_CP0_64,    0x40A00000, 0xFFE007FF, COP0,     MIPS_C_MTC0),
	R(TLBR,    SH_NONE,      0x42000001, 0xFFFFFFFF, COP0,     MIPS_C_SYS_MEM),
	R(TLBWI,   SH_NONE,      0x42000002, 0xFFFFFFFF, COP0,     MIPS_C_SYS_MEM),
	R(TLBWR,   SH_NONE,      0x42000006, 0xFFFFFFFF, COP0,     MIPS_C_SYS_MEM),
	R(TLBP,    SH_NONE,      0x42000008, 0xFFFFFFFF, COP0,     MIPS_C_SYS_MEM),
	R(ERET,    SH_NONE,      0x42000018, 0xFFFFFFFF, COP0,     MIPS_C_ERET),

	// integer ALU (add/sub/dadd/dsub/addi/daddi trap on signed overflow)
	R(ADD,     SH_R3_32,     0x00000020, 0xFC0007FF, MIPS_I,   MIPS_C_W0_R12),
	R(ADDU,    SH_R3_32,     0x00000021, 0xFC0007FF, MIPS_I,   MIPS_C_W0_R12),
	R(SUB,     SH_R3_32,     0x00000022, 0xFC0007FF, MIPS_I,   MIPS_C_W0_R12),
	R(SUBU,    SH_R3_32,     0x00000023, 0xFC0007FF, MIPS_I,   MIPS_C_W0_R12),
	R(AND,     SH_R3_64,     0x00000024, 0xFC0007FF, MIPS_I,   MIPS_C_W0_R12),
	R(OR,      SH_R3_64,     0x00000025, 0xFC0007FF, MIPS_I,   MIPS_C_W0_R12),
	R(XOR,     SH_R3_64,     0x00000026, 0xFC0007FF, MIPS_I,   MIPS_C_W0_R12),
	R(NOR,     SH_R3_64,     0x00000027, 0xFC0007FF, MIPS_I,   MIPS_C_W0_R12),
	R(SLT,     SH_R3_64,     0x0000002A, 0xFC0007FF, MIPS_I,   MIPS_C_W0_R12),
	R(SLTU,    SH_R3_64,     0x0000002B, 0xFC0007FF, MIPS_I,   MIPS_C_W0_R12),
	R(DADD,    SH_R3_64,     0x0000002C, 0xFC0007FF, MIPS_III, MIPS_C_W0_R12),
	R(DADDU,   SH_R3_64,     0x0000002D, 0xFC0007FF, MIPS_III, MIPS_C_W0_R12),
	R(DSUB,    SH_R3_64,     0x0000002E, 0xFC0007FF, MIPS_III, MIPS_C_W0_R12),
	R(DSUBU,   SH_R3_64,     0x0000002F, 0xFC0007FF, MIPS_III, MIPS_C_W0_R12),
	R(ADDI,    SH_I_S32,     0x20000000, 0xFC000000, MIPS_I,   MIPS_C_W0_R1),
	R(ADDIU,   SH_I_S32,     0x24000000, 0xFC000000, MIPS_I,   MIPS_C_W0_R1),
	R(SLTI,    SH_I_S64,     0x28000000, 0xFC000000, MIPS_I,   MIPS_C_W0_R1),
	R(SLTIU,   SH_I_S64,     0x2C000000, 0xFC000000, MIPS_I,   MIPS_C_W0_R1),
	R(ANDI,    SH_I_U64,     0x30000000, 0xFC000000, MIPS_I,   MIPS_C_W0_R1),
	R(ORI,     SH_I_U64,     0x34000000, 0xFC000000, MIPS_I,   MIPS_C_W0_R1),
	R(XORI,    SH_I_U64,     0x38000000, 0xFC000000, MIPS_I,   MIPS_C_W0_R1),
	R(LUI,     SH_LUI,       0x3C000000, 0xFFE00000, MIPS_I,   MIPS_C_W0),
	R(DADDI,   SH_I_S64,     0x60000000, 0xFC000000, MIPS_III, MIPS_C_W0_R1),
	R(DADDIU,  SH_I_S64,     0x64000000, 0xFC000000, MIPS_III, MIPS_C_W0_R1),
	R(SLL,     SH_SHI_32,    0x00000000, 0xFFE0003F, MIPS_I,   MIPS_C_W0_R1),
	R(SRL,     SH_SHI_32,    0x00000002, 0xFFE0003F, MIPS_I,   MIPS_C_W0_R1),
	R(SRA,     SH_SHI_32,    0x00000003, 0xFFE0003F, MIPS_I,   MIPS_C_W0_R1),
	R(SLLV,    SH_SHV_32,    0x00000004, 0xFC0007FF, MIPS_I,   MIPS_C_W0_R12),
	R(SRLV,    SH_SHV_32,    0x00000006, 0xFC0007FF, MIPS_I,   MIPS_C_W0_R12),
	R(SRAV,    SH_SHV_32,    0x00000007, 0xFC0007FF, MIPS_I,   MIPS_C_W0_R12),
	R(DSLL,    SH_SHI_64,    0x00000038, 0xFFE0003F, MIPS_III, MIPS_C_W0_R1),
	R(DSRL,    SH_SHI_64,    0x0000003A, 0xFFE0003F, MIPS_III, MIPS_C_W0_R1),
	R(DSRA,    SH_SHI_64,    0x0000003B, 0xFFE0003F, MIPS_III, MIPS_C_W0_R1),
	R(DSLL32,  SH_SHI_64,    0x0000003C, 0xFFE0003F, MIPS_III, MIPS_C_W0_R1),
	R(DSRL32,  SH_SHI_64,    0x0000003E, 0xFFE0003F, MIPS_III, MIPS_C_W0_R1),
	R(DSRA32,  SH_SHI_64,    0x0000003F, 0xFFE0003F, MIPS_III, MIPS_C_W0_R1),
	R(DSLLV,   SH_SHV_64,    0x00000014, 0xFC0007FF, MIPS_III, MIPS_C_W0_R12),
	R(DSRLV,   SH_SHV_64,    0x00000016, 0xFC0007FF, MIPS_III, MIPS_C_W0_R12),
	R(DSRAV,   SH_SHV_64,    0x00000017, 0xFC0007FF, MIPS_III, MIPS_C_W0_R12),

	// multiply / divide and HI/LO (div* written as `div $zero, rs, rt` by the backend)
	R(MULT,    SH_MD_32,     0x00000018, 0xFC00FFFF, MIPS_I,   MIPS_C_MULDIV),
	R(MULTU,   SH_MD_32,     0x00000019, 0xFC00FFFF, MIPS_I,   MIPS_C_MULDIV),
	R(DIV,     SH_MD_32,     0x0000001A, 0xFC00FFFF, MIPS_I,   MIPS_C_MULDIV),
	R(DIVU,    SH_MD_32,     0x0000001B, 0xFC00FFFF, MIPS_I,   MIPS_C_MULDIV),
	R(DMULT,   SH_MD_64,     0x0000001C, 0xFC00FFFF, MIPS_III, MIPS_C_MULDIV),
	R(DMULTU,  SH_MD_64,     0x0000001D, 0xFC00FFFF, MIPS_III, MIPS_C_MULDIV),
	R(DDIV,    SH_MD_64,     0x0000001E, 0xFC00FFFF, MIPS_III, MIPS_C_MULDIV),
	R(DDIVU,   SH_MD_64,     0x0000001F, 0xFC00FFFF, MIPS_III, MIPS_C_MULDIV),
	R(MFHI,    SH_RD,        0x00000010, 0xFFFF07FF, MIPS_I,   MIPS_C_MFHI),
	R(MFLO,    SH_RD,        0x00000012, 0xFFFF07FF, MIPS_I,   MIPS_C_MFLO),
	R(MTHI,    SH_RS,        0x00000011, 0xFC1FFFFF, MIPS_I,   MIPS_C_MTHI),
	R(MTLO,    SH_RS,        0x00000013, 0xFC1FFFFF, MIPS_I,   MIPS_C_MTLO),

	// assembler aliases
	R(MOVE,    SH_RD_RS,     0x00000025, 0xFC1F07FF, MACRO,    MIPS_C_W0_R1),     // or rd, rs, $0
	R(NOT,     SH_RD_RS,     0x00000027, 0xFC1F07FF, MACRO,    MIPS_C_W0_R1),     // nor rd, rs, $0
	R(NEGU,    SH_RD_RT_32,  0x00000023, 0xFFE007FF, MACRO,    MIPS_C_W0_R1),     // subu rd, $0, rt
	R(DNEGU,   SH_RD_RT_64,  0x0000002F, 0xFFE007FF, MACRO,    MIPS_C_W0_R1),     // dsubu rd, $0, rt
	R(LI,      SH_LI,        0x24000000, 0x00000000, MACRO,    MIPS_C_W0),        // addiu / ori / lui+ori

	// loads / stores (lwl/lwr/ldl/ldr merge into rt but are modelled as full
	// writes so the usual lwl+lwr pair does not read an unassigned output)
	R(LB,      SH_MEM,       0x80000000, 0xFC000000, MIPS_I,   MIPS_C_LOAD),
	R(LBU,     SH_MEM,       0x90000000, 0xFC000000, MIPS_I,   MIPS_C_LOAD),
	R(LH,      SH_MEM,       0x84000000, 0xFC000000, MIPS_I,   MIPS_C_LOAD),
	R(LHU,     SH_MEM,       0x94000000, 0xFC000000, MIPS_I,   MIPS_C_LOAD),
	R(LW,      SH_MEM,       0x8C000000, 0xFC000000, MIPS_I,   MIPS_C_LOAD),
	R(LWU,     SH_MEM,       0x9C000000, 0xFC000000, MIPS_III, MIPS_C_LOAD),
	R(LD,      SH_MEM,       0xDC000000, 0xFC000000, MIPS_III, MIPS_C_LOAD),
	R(SB,      SH_MEM,       0xA0000000, 0xFC000000, MIPS_I,   MIPS_C_STORE),
	R(SH,      SH_MEM,       0xA4000000, 0xFC000000, MIPS_I,   MIPS_C_STORE),
	R(SW,      SH_MEM,       0xAC000000, 0xFC000000, MIPS_I,   MIPS_C_STORE),
	R(SD,      SH_MEM,       0xFC000000, 0xFC000000, MIPS_III, MIPS_C_STORE),
	R(LWL,     SH_MEM,       0x88000000, 0xFC000000, MIPS_I,   MIPS_C_LOAD),
	R(LWR,     SH_MEM,       0x98000000, 0xFC000000, MIPS_I,   MIPS_C_LOAD),
	R(LDL,     SH_MEM,       0x68000000, 0xFC000000, MIPS_III, MIPS_C_LOAD),
	R(LDR,     SH_MEM,       0x6C000000, 0xFC000000, MIPS_III, MIPS_C_LOAD),
	R(SWL,     SH_MEM,       0xA8000000, 0xFC000000, MIPS_I,   MIPS_C_STORE),
	R(SWR,     SH_MEM,       0xB8000000, 0xFC000000, MIPS_I,   MIPS_C_STORE),
	R(SDL,     SH_MEM,       0xB0000000, 0xFC000000, MIPS_III, MIPS_C_STORE),
	R(SDR,     SH_MEM,       0xB4000000, 0xFC000000, MIPS_III, MIPS_C_STORE),
	R(LL,      SH_MEM,       0xC0000000, 0xFC000000, MIPS_II,  MIPS_C_LL),
	R(LLD,     SH_MEM,       0xD0000000, 0xFC000000, MIPS_III, MIPS_C_LL),
	R(SC,      SH_MEM,       0xE0000000, 0xFC000000, MIPS_II,  MIPS_C_SC),
	R(SCD,     SH_MEM,       0xF0000000, 0xFC000000, MIPS_III, MIPS_C_SC),

	// branches
	R(BEQ,     SH_BR2,       0x10000000, 0xFC000000, MIPS_I,   MIPS_C_BR2),
	R(BNE,     SH_BR2,       0x14000000, 0xFC000000, MIPS_I,   MIPS_C_BR2),
	R(BLEZ,    SH_BR1,       0x18000000, 0xFC1F0000, MIPS_I,   MIPS_C_BR1),
	R(BGTZ,    SH_BR1,       0x1C000000, 0xFC1F0000, MIPS_I,   MIPS_C_BR1),
	R(BLTZ,    SH_BR1,       0x04000000, 0xFC1F0000, MIPS_I,   MIPS_C_BR1),
	R(BGEZ,    SH_BR1,       0x04010000, 0xFC1F0000, MIPS_I,   MIPS_C_BR1),
	R(BLTZAL,  SH_BR1,       0x04100000, 0xFC1F0000, MIPS_I,   MIPS_C_BR1_LINK),
	R(BGEZAL,  SH_BR1,       0x04110000, 0xFC1F0000, MIPS_I,   MIPS_C_BR1_LINK),
	R(BEQL,    SH_BR2,       0x50000000, 0xFC000000, MIPS_II,  MIPS_C_BR2),
	R(BNEL,    SH_BR2,       0x54000000, 0xFC000000, MIPS_II,  MIPS_C_BR2),
	R(BLEZL,   SH_BR1,       0x58000000, 0xFC1F0000, MIPS_II,  MIPS_C_BR1),
	R(BGTZL,   SH_BR1,       0x5C000000, 0xFC1F0000, MIPS_II,  MIPS_C_BR1),
	R(BLTZL,   SH_BR1,       0x04020000, 0xFC1F0000, MIPS_II,  MIPS_C_BR1),
	R(BGEZL,   SH_BR1,       0x04030000, 0xFC1F0000, MIPS_II,  MIPS_C_BR1),
	R(BLTZALL, SH_BR1,       0x04120000, 0xFC1F0000, MIPS_II,  MIPS_C_BR1_LINK),
	R(BGEZALL, SH_BR1,       0x04130000, 0xFC1F0000, MIPS_II,  MIPS_C_BR1_LINK),
	R(B,       SH_BR0,       0x10000000, 0xFFFF0000, MACRO,    MIPS_C_BR0),       // beq $0, $0, label
	R(BAL,     SH_BR0,       0x04110000, 0xFFFF0000, MACRO,    MIPS_C_BR0_LINK),  // bgezal $0, label
	R(BEQZ,    SH_BR1,       0x10000000, 0xFC1F0000, MACRO,    MIPS_C_BR1),       // beq rs, $0, label
	R(BNEZ,    SH_BR1,       0x14000000, 0xFC1F0000, MACRO,    MIPS_C_BR1),       // bne rs, $0, label
	R(BEQZL,   SH_BR1,       0x50000000, 0xFC1F0000, MACRO,    MIPS_C_BR1),       // beql rs, $0, label
	R(BNEZL,   SH_BR1,       0x54000000, 0xFC1F0000, MACRO,    MIPS_C_BR1),       // bnel rs, $0, label

	// jumps
	R(J,       SH_JUMP,      0x08000000, 0xFC000000, MIPS_I,   MIPS_C_BR0),
	R(JAL,     SH_JUMP,      0x0C000000, 0xFC000000, MIPS_I,   MIPS_C_BR0_LINK),
	R(JR,      SH_RS,        0x00000008, 0xFC1FFFFF, MIPS_I,   MIPS_C_JR),
	R(JALR,    SH_RS,        0x0000F809, 0xFC1FFFFF, MIPS_I,   MIPS_C_JALR1),     // jalr $ra, rs
	R(JALR,    SH_RD_RS,     0x00000009, 0xFC1F07FF, MIPS_I,   MIPS_C_JALR2),

	// traps
	R(TEQ,     SH_MD_64,     0x00000034, 0xFC00FFFF, MIPS_II,  MIPS_C_TRAP2),
	R(TEQ,     SH_TRAP_CODE, 0x00000034, 0xFC00003F, MIPS_II,  MIPS_C_TRAP2),
	R(TNE,     SH_MD_64,     0x00000036, 0xFC00FFFF, MIPS_II,  MIPS_C_TRAP2),
	R(TNE,     SH_TRAP_CODE, 0x00000036, 0xFC00003F, MIPS_II,  MIPS_C_TRAP2),
	R(TGE,     SH_MD_64,     0x00000030, 0xFC00FFFF, MIPS_II,  MIPS_C_TRAP2),
	R(TGE,     SH_TRAP_CODE, 0x00000030, 0xFC00003F, MIPS_II,  MIPS_C_TRAP2),
	R(TGEU,    SH_MD_64,     0x00000031, 0xFC00FFFF, MIPS_II,  MIPS_C_TRAP2),
	R(TGEU,    SH_TRAP_CODE, 0x00000031, 0xFC00003F, MIPS_II,  MIPS_C_TRAP2),
	R(TLT,     SH_MD_64,     0x00000032, 0xFC00FFFF, MIPS_II,  MIPS_C_TRAP2),
	R(TLT,     SH_TRAP_CODE, 0x00000032, 0xFC00003F, MIPS_II,  MIPS_C_TRAP2),
	R(TLTU,    SH_MD_64,     0x00000033, 0xFC00FFFF, MIPS_II,  MIPS_C_TRAP2),
	R(TLTU,    SH_TRAP_CODE, 0x00000033, 0xFC00003F, MIPS_II,  MIPS_C_TRAP2),
	R(TEQI,    SH_TRAPI,     0x040C0000, 0xFC1F0000, MIPS_II,  MIPS_C_TRAP1),
	R(TNEI,    SH_TRAPI,     0x040E0000, 0xFC1F0000, MIPS_II,  MIPS_C_TRAP1),
	R(TGEI,    SH_TRAPI,     0x04080000, 0xFC1F0000, MIPS_II,  MIPS_C_TRAP1),
	R(TGEIU,   SH_TRAPI,     0x04090000, 0xFC1F0000, MIPS_II,  MIPS_C_TRAP1),
	R(TLTI,    SH_TRAPI,     0x040A0000, 0xFC1F0000, MIPS_II,  MIPS_C_TRAP1),
	R(TLTIU,   SH_TRAPI,     0x040B0000, 0xFC1F0000, MIPS_II,  MIPS_C_TRAP1),

	// COP1 moves, loads and stores
	R(MFC1,    SH_C1_32,     0x44000000, 0xFFE007FF, FPU,      MIPS_C_MFC1),
	R(MTC1,    SH_C1_32,     0x44800000, 0xFFE007FF, FPU,      MIPS_C_MTC1),
	R(DMFC1,   SH_C1_64,     0x44200000, 0xFFE007FF, FPU,      MIPS_C_MFC1),
	R(DMTC1,   SH_C1_64,     0x44A00000, 0xFFE007FF, FPU,      MIPS_C_MTC1),
	R(CFC1,    SH_FCR,       0x44400000, 0xFFE007FF, FPU,      MIPS_C_CFC1),
	R(CTC1,    SH_FCR,       0x44C00000, 0xFFE007FF, FPU,      MIPS_C_CTC1),
	R(LWC1,    SH_FMEM_S,    0xC4000000, 0xFC000000, FPU,      MIPS_C_LOAD),
	R(SWC1,    SH_FMEM_S,    0xE4000000, 0xFC000000, FPU,      MIPS_C_STORE),
	R(LDC1,    SH_FMEM_D,    0xD4000000, 0xFC000000, FPU,      MIPS_C_LOAD),
	R(SDC1,    SH_FMEM_D,    0xF4000000, 0xFC000000, FPU,      MIPS_C_STORE),

	// FPU arithmetic
	R(ADD_S,   SH_F3_S,      FP3(FPFMT_S, 0x00), 0xFFE0003F, FPU, MIPS_C_W0_R12),
	R(ADD_D,   SH_F3_D,      FP3(FPFMT_D, 0x00), 0xFFE0003F, FPU, MIPS_C_W0_R12),
	R(SUB_S,   SH_F3_S,      FP3(FPFMT_S, 0x01), 0xFFE0003F, FPU, MIPS_C_W0_R12),
	R(SUB_D,   SH_F3_D,      FP3(FPFMT_D, 0x01), 0xFFE0003F, FPU, MIPS_C_W0_R12),
	R(MUL_S,   SH_F3_S,      FP3(FPFMT_S, 0x02), 0xFFE0003F, FPU, MIPS_C_W0_R12),
	R(MUL_D,   SH_F3_D,      FP3(FPFMT_D, 0x02), 0xFFE0003F, FPU, MIPS_C_W0_R12),
	R(DIV_S,   SH_F3_S,      FP3(FPFMT_S, 0x03), 0xFFE0003F, FPU, MIPS_C_W0_R12),
	R(DIV_D,   SH_F3_D,      FP3(FPFMT_D, 0x03), 0xFFE0003F, FPU, MIPS_C_W0_R12),
	R(SQRT_S,  SH_F2_S,      FP3(FPFMT_S, 0x04), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(SQRT_D,  SH_F2_D,      FP3(FPFMT_D, 0x04), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(ABS_S,   SH_F2_S,      FP3(FPFMT_S, 0x05), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(ABS_D,   SH_F2_D,      FP3(FPFMT_D, 0x05), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(NEG_S,   SH_F2_S,      FP3(FPFMT_S, 0x07), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(NEG_D,   SH_F2_D,      FP3(FPFMT_D, 0x07), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(MOV_S,   SH_F2_S,      FP3(FPFMT_S, 0x06), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(MOV_D,   SH_F2_D,      FP3(FPFMT_D, 0x06), 0xFFFF003F, FPU, MIPS_C_W0_R1),

	// FPU conversions (`.w`/`.l` values live in an FPR: use f32/f64 Odin values)
	R(CVT_S_D,   SH_CVT_S_D, FP3(FPFMT_D, 0x20), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(CVT_S_W,   SH_CVT_S_W, FP3(FPFMT_W, 0x20), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(CVT_S_L,   SH_CVT_S_L, FP3(FPFMT_L, 0x20), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(CVT_D_S,   SH_CVT_D_S, FP3(FPFMT_S, 0x21), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(CVT_D_W,   SH_CVT_D_W, FP3(FPFMT_W, 0x21), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(CVT_D_L,   SH_CVT_D_L, FP3(FPFMT_L, 0x21), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(CVT_W_S,   SH_CVT_W_S, FP3(FPFMT_S, 0x24), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(CVT_W_D,   SH_CVT_W_D, FP3(FPFMT_D, 0x24), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(CVT_L_S,   SH_CVT_L_S, FP3(FPFMT_S, 0x25), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(CVT_L_D,   SH_CVT_L_D, FP3(FPFMT_D, 0x25), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(ROUND_W_S, SH_CVT_W_S, FP3(FPFMT_S, 0x0C), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(ROUND_W_D, SH_CVT_W_D, FP3(FPFMT_D, 0x0C), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(ROUND_L_S, SH_CVT_L_S, FP3(FPFMT_S, 0x08), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(ROUND_L_D, SH_CVT_L_D, FP3(FPFMT_D, 0x08), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(TRUNC_W_S, SH_CVT_W_S, FP3(FPFMT_S, 0x0D), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(TRUNC_W_D, SH_CVT_W_D, FP3(FPFMT_D, 0x0D), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(TRUNC_L_S, SH_CVT_L_S, FP3(FPFMT_S, 0x09), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(TRUNC_L_D, SH_CVT_L_D, FP3(FPFMT_D, 0x09), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(CEIL_W_S,  SH_CVT_W_S, FP3(FPFMT_S, 0x0E), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(CEIL_W_D,  SH_CVT_W_D, FP3(FPFMT_D, 0x0E), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(CEIL_L_S,  SH_CVT_L_S, FP3(FPFMT_S, 0x0A), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(CEIL_L_D,  SH_CVT_L_D, FP3(FPFMT_D, 0x0A), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(FLOOR_W_S, SH_CVT_W_S, FP3(FPFMT_S, 0x0F), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(FLOOR_W_D, SH_CVT_W_D, FP3(FPFMT_D, 0x0F), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(FLOOR_L_S, SH_CVT_L_S, FP3(FPFMT_S, 0x0B), 0xFFFF003F, FPU, MIPS_C_W0_R1),
	R(FLOOR_L_D, SH_CVT_L_D, FP3(FPFMT_D, 0x0B), 0xFFFF003F, FPU, MIPS_C_W0_R1),

	// FPU compares: c.cond.fmt fs, ft (cond 0..15 in funct 0x30..0x3F)
	R(C_F_S,    SH_FCMP_S, FP3(FPFMT_S, 0x30), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_UN_S,   SH_FCMP_S, FP3(FPFMT_S, 0x31), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_EQ_S,   SH_FCMP_S, FP3(FPFMT_S, 0x32), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_UEQ_S,  SH_FCMP_S, FP3(FPFMT_S, 0x33), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_OLT_S,  SH_FCMP_S, FP3(FPFMT_S, 0x34), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_ULT_S,  SH_FCMP_S, FP3(FPFMT_S, 0x35), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_OLE_S,  SH_FCMP_S, FP3(FPFMT_S, 0x36), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_ULE_S,  SH_FCMP_S, FP3(FPFMT_S, 0x37), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_SF_S,   SH_FCMP_S, FP3(FPFMT_S, 0x38), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_NGLE_S, SH_FCMP_S, FP3(FPFMT_S, 0x39), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_SEQ_S,  SH_FCMP_S, FP3(FPFMT_S, 0x3A), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_NGL_S,  SH_FCMP_S, FP3(FPFMT_S, 0x3B), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_LT_S,   SH_FCMP_S, FP3(FPFMT_S, 0x3C), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_NGE_S,  SH_FCMP_S, FP3(FPFMT_S, 0x3D), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_LE_S,   SH_FCMP_S, FP3(FPFMT_S, 0x3E), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_NGT_S,  SH_FCMP_S, FP3(FPFMT_S, 0x3F), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_F_D,    SH_FCMP_D, FP3(FPFMT_D, 0x30), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_UN_D,   SH_FCMP_D, FP3(FPFMT_D, 0x31), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_EQ_D,   SH_FCMP_D, FP3(FPFMT_D, 0x32), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_UEQ_D,  SH_FCMP_D, FP3(FPFMT_D, 0x33), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_OLT_D,  SH_FCMP_D, FP3(FPFMT_D, 0x34), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_ULT_D,  SH_FCMP_D, FP3(FPFMT_D, 0x35), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_OLE_D,  SH_FCMP_D, FP3(FPFMT_D, 0x36), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_ULE_D,  SH_FCMP_D, FP3(FPFMT_D, 0x37), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_SF_D,   SH_FCMP_D, FP3(FPFMT_D, 0x38), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_NGLE_D, SH_FCMP_D, FP3(FPFMT_D, 0x39), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_SEQ_D,  SH_FCMP_D, FP3(FPFMT_D, 0x3A), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_NGL_D,  SH_FCMP_D, FP3(FPFMT_D, 0x3B), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_LT_D,   SH_FCMP_D, FP3(FPFMT_D, 0x3C), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_NGE_D,  SH_FCMP_D, FP3(FPFMT_D, 0x3D), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_LE_D,   SH_FCMP_D, FP3(FPFMT_D, 0x3E), 0xFFE007FF, FPU, MIPS_C_FCMP),
	R(C_NGT_D,  SH_FCMP_D, FP3(FPFMT_D, 0x3F), 0xFFE007FF, FPU, MIPS_C_FCMP),

	// FPU branches
	R(BC1F,    SH_BR0,       0x45000000, 0xFFFF0000, FPU,      MIPS_C_BC1),
	R(BC1T,    SH_BR0,       0x45010000, 0xFFFF0000, FPU,      MIPS_C_BC1),
	R(BC1FL,   SH_BR0,       0x45020000, 0xFFFF0000, FPU,      MIPS_C_BC1),
	R(BC1TL,   SH_BR0,       0x45030000, 0xFFFF0000, FPU,      MIPS_C_BC1),

	// NOT on the VR4300. Each names the LLVM feature that would enable it.
	R(EXT,     SH_EXT,       0x7C000000, 0xFC00003F, MIPS32R2, MIPS_C_W0_R1),
	R(INS,     SH_EXT,       0x7C000004, 0xFC00003F, MIPS32R2, MIPS_CLB(MIPS_O0, MIPS_O0|MIPS_O1, 0, 0, false, false, 0)),
	R(DEXT,    SH_DEXT,      0x7C000003, 0xFC00003F, MIPS64R2, MIPS_C_W0_R1),
	R(DINS,    SH_DEXT,      0x7C000007, 0xFC00003F, MIPS64R2, MIPS_CLB(MIPS_O0, MIPS_O0|MIPS_O1, 0, 0, false, false, 0)),
	R(ROTR,    SH_SHI_32,    0x00200002, 0xFFE0003F, MIPS32R2, MIPS_C_W0_R1),
	R(ROTRV,   SH_SHV_32,    0x00000046, 0xFC0007FF, MIPS32R2, MIPS_C_W0_R12),
	R(DROTR,   SH_SHI_64,    0x0020003A, 0xFFE0003F, MIPS64R2, MIPS_C_W0_R1),
	R(DROTR32, SH_SHI_64,    0x0020003E, 0xFFE0003F, MIPS64R2, MIPS_C_W0_R1),
	R(DROTRV,  SH_SHV_64,    0x00000056, 0xFC0007FF, MIPS64R2, MIPS_C_W0_R12),
	R(SEB,     SH_RD_RT_32,  0x7C000420, 0xFFE007FF, MIPS32R2, MIPS_C_W0_R1),
	R(SEH,     SH_RD_RT_32,  0x7C000620, 0xFFE007FF, MIPS32R2, MIPS_C_W0_R1),
	R(WSBH,    SH_RD_RT_32,  0x7C0000A0, 0xFFE007FF, MIPS32R2, MIPS_C_W0_R1),
	R(DSBH,    SH_RD_RT_64,  0x7C0000A4, 0xFFE007FF, MIPS64R2, MIPS_C_W0_R1),
	R(DSHD,    SH_RD_RT_64,  0x7C000164, 0xFFE007FF, MIPS64R2, MIPS_C_W0_R1),
	R(CLZ,     SH_RD_RS,     0x70000020, 0xFC0007FF, MIPS32,   MIPS_C_W0_R1),
	R(CLO,     SH_RD_RS,     0x70000021, 0xFC0007FF, MIPS32,   MIPS_C_W0_R1),
	R(DCLZ,    SH_RD_RS,     0x70000024, 0xFC0007FF, MIPS64,   MIPS_C_W0_R1),
	R(DCLO,    SH_RD_RS,     0x70000025, 0xFC0007FF, MIPS64,   MIPS_C_W0_R1),
	R(MUL,     SH_R3_32,     0x70000002, 0xFC0007FF, MIPS32,   MIPS_CLB(MIPS_O0, MIPS_O1|MIPS_O2, MIPS_HI|MIPS_LO, 0, false, false, 0)),
	R(MADD,    SH_MD_32,     0x70000000, 0xFC00FFFF, MIPS32,   MIPS_C_MACC),
	R(MADDU,   SH_MD_32,     0x70000001, 0xFC00FFFF, MIPS32,   MIPS_C_MACC),
	R(MSUB,    SH_MD_32,     0x70000004, 0xFC00FFFF, MIPS32,   MIPS_C_MACC),
	R(MSUBU,   SH_MD_32,     0x70000005, 0xFC00FFFF, MIPS32,   MIPS_C_MACC),
	R(MOVZ,    SH_R3_64,     0x0000000A, 0xFC0007FF, MIPS4,    MIPS_C_CMOV),
	R(MOVN,    SH_R3_64,     0x0000000B, 0xFC0007FF, MIPS4,    MIPS_C_CMOV),
	R(MOVZ_S,  SH_FMOVC_S,   FP3(FPFMT_S, 0x12), 0xFFE0003F, MIPS4, MIPS_C_CMOV),
	R(MOVZ_D,  SH_FMOVC_D,   FP3(FPFMT_D, 0x12), 0xFFE0003F, MIPS4, MIPS_C_CMOV),
	R(MOVN_S,  SH_FMOVC_S,   FP3(FPFMT_S, 0x13), 0xFFE0003F, MIPS4, MIPS_C_CMOV),
	R(MOVN_D,  SH_FMOVC_D,   FP3(FPFMT_D, 0x13), 0xFFE0003F, MIPS4, MIPS_C_CMOV),
	R(PREF,    SH_PREF,      0xCC000000, 0xFC000000, MIPS4,    MIPS_C_PREF),
	R(SYNCI,   SH_MEM_ONLY,  0x041F0000, 0xFC1F0000, MIPS32R2, MIPS_C_SYNCI),
	R(EHB,     SH_NONE,      0x000000C0, 0xFFFFFFFF, MIPS32R2, MIPS_C_SYS),
	R(WAIT,    SH_NONE,      0x42000020, 0xFE00003F, MIPS32,   MIPS_C_SYS),
	R(DI,      SH_NONE,      0x41606000, 0xFFFFFFFF, MIPS32R2, MIPS_C_SYS),
	R(DI,      SH_RT,        0x41606000, 0xFFE0FFFF, MIPS32R2, MIPS_C_SYS_W0),
	R(EI,      SH_NONE,      0x41606020, 0xFFFFFFFF, MIPS32R2, MIPS_C_SYS),
	R(EI,      SH_RT,        0x41606020, 0xFFE0FFFF, MIPS32R2, MIPS_C_SYS_W0),
	R(RDHWR,   SH_RDHWR,     0x7C00003B, 0xFFE007FF, MIPS32R2, MIPS_C_SYS_W0),
	R(JR_HB,   SH_RS,        0x00000408, 0xFC1FFFFF, MIPS32R2, MIPS_C_JR),
	R(JALR_HB, SH_RS,        0x0000FC09, 0xFC1FFFFF, MIPS32R2, MIPS_C_JALR1),
	R(JALR_HB, SH_RD_RS,     0x00000409, 0xFC1F07FF, MIPS32R2, MIPS_C_JALR2),
	R(MFHC1,   SH_C1_HI,     0x44600000, 0xFFE007FF, MIPS32R2, MIPS_C_MFC1),
	R(MTHC1,   SH_C1_HI,     0x44E00000, 0xFFE007FF, MIPS32R2, MIPS_C_MTHC1),
	R(MADD_S,  SH_F4_S,      0x4C000020, 0xFC00003F, MIPS4,    MIPS_C_F4),
	R(MADD_D,  SH_F4_D,      0x4C000021, 0xFC00003F, MIPS4,    MIPS_C_F4),
	R(MSUB_S,  SH_F4_S,      0x4C000028, 0xFC00003F, MIPS4,    MIPS_C_F4),
	R(MSUB_D,  SH_F4_D,      0x4C000029, 0xFC00003F, MIPS4,    MIPS_C_F4),
	R(NMADD_S, SH_F4_S,      0x4C000030, 0xFC00003F, MIPS4,    MIPS_C_F4),
	R(NMADD_D, SH_F4_D,      0x4C000031, 0xFC00003F, MIPS4,    MIPS_C_F4),
	R(NMSUB_S, SH_F4_S,      0x4C000038, 0xFC00003F, MIPS4,    MIPS_C_F4),
	R(NMSUB_D, SH_F4_D,      0x4C000039, 0xFC00003F, MIPS4,    MIPS_C_F4),
	R(RECIP_S, SH_F2_S,      FP3(FPFMT_S, 0x15), 0xFFFF003F, MIPS4, MIPS_C_W0_R1),
	R(RECIP_D, SH_F2_D,      FP3(FPFMT_D, 0x15), 0xFFFF003F, MIPS4, MIPS_C_W0_R1),
	R(RSQRT_S, SH_F2_S,      FP3(FPFMT_S, 0x16), 0xFFFF003F, MIPS4, MIPS_C_W0_R1),
	R(RSQRT_D, SH_F2_D,      FP3(FPFMT_D, 0x16), 0xFFFF003F, MIPS4, MIPS_C_W0_R1),
};
isize const Asm_mips::form_row_count = gb_count_of(Asm_mips::form_rows);

#undef FP3
#undef FPFMT_S
#undef FPFMT_D
#undef FPFMT_W
#undef FPFMT_L
#undef R
#undef MIPS_C_NONE
#undef MIPS_C_W0_R12
#undef MIPS_C_W0_R1
#undef MIPS_C_W0
#undef MIPS_C_MULDIV
#undef MIPS_C_MACC
#undef MIPS_C_MFHI
#undef MIPS_C_MFLO
#undef MIPS_C_MTHI
#undef MIPS_C_MTLO
#undef MIPS_C_CMOV
#undef MIPS_C_LOAD
#undef MIPS_C_STORE
#undef MIPS_C_LL
#undef MIPS_C_SC
#undef MIPS_C_BR2
#undef MIPS_C_BR1
#undef MIPS_C_BR1_LINK
#undef MIPS_C_BR0
#undef MIPS_C_BR0_LINK
#undef MIPS_C_JR
#undef MIPS_C_JALR1
#undef MIPS_C_JALR2
#undef MIPS_C_BC1
#undef MIPS_C_FCMP
#undef MIPS_C_TRAP2
#undef MIPS_C_TRAP1
#undef MIPS_C_TRAP0
#undef MIPS_C_MFC0
#undef MIPS_C_MTC0
#undef MIPS_C_SYS_MEM
#undef MIPS_C_SYS
#undef MIPS_C_SYS_W0
#undef MIPS_C_ERET
#undef MIPS_C_CACHE
#undef MIPS_C_SYNCI
#undef MIPS_C_PREF
#undef MIPS_C_SYNC
#undef MIPS_C_MFC1
#undef MIPS_C_MTC1
#undef MIPS_C_MTHC1
#undef MIPS_C_CFC1
#undef MIPS_C_CTC1
#undef MIPS_C_F4
#undef MIPS_O0
#undef MIPS_O1
#undef MIPS_O2
#undef MIPS_O3
#undef MIPS_RA
#undef MIPS_HI
#undef MIPS_LO
#undef MIPS_FCC
#undef MIPS_CTL
#undef MIPS_COND
#undef MIPS_CALL
#undef MIPS_TRAP
#undef MIPS_SYS
#undef MIPS_FNC
#undef MIPS_ATM
#undef MIPS_FCSR
#undef MIPS_CLB


bool Asm_mips::init(i64 word_size) {
	// o64: pointers are 32-bit but the VR4300's GPRs and (FR=1) FPRs are 64-bit.
	gb_unused(word_size);
	GPRLEN = 64;
	FPRLEN = 64;

	isize n = form_row_count;
	encode_forms        = gb_alloc_array(permanent_allocator(), Encoding, n);
	clobber_forms_table = gb_alloc_array(permanent_allocator(), Clobber,  n);
	gb_zero_array(encode_runs, MNEMONIC_COUNT);

	u16 prev = M_INVALID;
	for (isize i = 0; i < n; i++) {
		FormRow   const &row   = form_rows[i];
		FormShape const &shape = form_shapes[row.shape];

		u8 count = 0;
		for (int j = 0; j < 4 && shape.ops[j] != OP_NONE; j++) {
			count += 1;
		}

		Encoding *e = &encode_forms[i];
		e->mnemonic = row.mnemonic;
		for (int j = 0; j < 4; j++) {
			e->ops[j] = shape.ops[j];
			e->enc[j] = shape.enc[j];
		}
		e->bits    = row.bits;
		e->mask    = row.mask;
		e->feature = row.feature;
		e->flags   = cast(EncodingFlags)(count << 4);

		clobber_forms_table[i] = row.clobber;

		// Rows are grouped by mnemonic, in Mnemonic order.
		u16 m = cast(u16)row.mnemonic;
		GB_ASSERT_MSG(m >= prev, "asm_tables_mips: form rows out of order at %.*s", LIT(mnemonic_strings[m]));
		if (m != prev) {
			encode_runs[m].start = cast(u32)i;
			prev = m;
		}
		encode_runs[m].count += 1;
	}
	for (u16 m = M_INVALID+1; m < MNEMONIC_COUNT; m++) {
		GB_ASSERT_MSG(encode_runs[m].count != 0, "asm_tables_mips: mnemonic %.*s has no forms", LIT(mnemonic_strings[m]));
	}

	string_map_init(&mnemonic_map, MNEMONIC_COUNT*2);
	for (u16 m = M_INVALID+1; m < MNEMONIC_COUNT; m++) {
		string_map_set(&mnemonic_map, mnemonic_strings[m], cast(Mnemonic)m);
	}

	string_map_init(&register_map, REG_COUNT*2);
	for (u16 r = REG_INVALID+1; r < REG_COUNT; r++) {
		string_map_set(&register_map, register_strings[r], cast(Register)r);
	}
	return true;
}
