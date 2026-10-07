// =============================================================================
// HAND-WRITTEN SPIKE TABLE - MIPS III (NEC VR4300) subset for asm templates
// =============================================================================
//
// Unlike asm_tables_{amd64,riscv,arm64}.cpp this table is not generated from
// core:rexcode. It covers just enough of MIPS III to write cache maintenance,
// COP0 access and simple integer/memory code. A full table would come from a
// core/rexcode/isa/mips/tablegen/cpp-compiler stage (cf. the riscv one),
// restricted to the VR4300 feature set.
//
// Operand order is the assembler's (destination first), the same as riscv64.
// Branch delay slots are filled by the assembler: LLVM wraps every inline asm
// block in `.set push; .set reorder; ...; .set pop`.

struct Asm_mips {
	enum Mnemonic : u16 {
		M_INVALID,
		// system / cache
		M_NOP, M_SSNOP, M_SYNC, M_ERET, M_CACHE, M_MFC0, M_MTC0, M_DMFC0, M_DMTC0,
		// integer ALU
		M_ADDU, M_SUBU, M_DADDU, M_DSUBU, M_AND, M_OR, M_XOR, M_NOR, M_SLT, M_SLTU,
		M_ADDIU, M_DADDIU, M_SLTI, M_SLTIU, M_ANDI, M_ORI, M_XORI, M_LUI,
		M_SLL, M_SRL, M_SRA, M_DSLL, M_DSRL, M_DSRA, M_DSLL32, M_DSRL32, M_DSRA32,
		M_MOVE,
		// loads / stores
		M_LB, M_LBU, M_LH, M_LHU, M_LW, M_LWU, M_LD, M_SB, M_SH, M_SW, M_SD,
		// branches (delay slot filled by the assembler under `.set reorder`)
		M_BEQ, M_BNE,

		MNEMONIC_COUNT
	};
	static String const mnemonic_strings[MNEMONIC_COUNT];

	enum Prefix : u8 {
		PREFIX_INVALID,
		PREFIX_COUNT
	};
	enum PrefixKind : u8 { PrefixKind_None };

	static const u16 REG_CLASS_NONE = 0x0000;
	static const u16 REG_CLASS_GPR  = 0x0100; // $0..$31
	static const u16 REG_CLASS_CP0  = 0x0200; // COP0 $0..$31

	enum Register : u16 {
		REG_INVALID,
		REG_ZERO, REG_AT, REG_V0, REG_V1, REG_A0, REG_A1, REG_A2, REG_A3,
		REG_T0, REG_T1, REG_T2, REG_T3, REG_T4, REG_T5, REG_T6, REG_T7,
		REG_S0, REG_S1, REG_S2, REG_S3, REG_S4, REG_S5, REG_S6, REG_S7,
		REG_T8, REG_T9, REG_K0, REG_K1, REG_GP, REG_SP, REG_FP, REG_RA,
		REG_S8, // alias of fp ($30)

		REG_C0_INDEX, REG_C0_RANDOM, REG_C0_ENTRYLO0, REG_C0_ENTRYLO1, REG_C0_CONTEXT,
		REG_C0_PAGEMASK, REG_C0_WIRED, REG_C0_BADVADDR, REG_C0_COUNT, REG_C0_ENTRYHI,
		REG_C0_COMPARE, REG_C0_STATUS, REG_C0_CAUSE, REG_C0_EPC, REG_C0_PRID,
		REG_C0_CONFIG, REG_C0_LLADDR, REG_C0_WATCHLO, REG_C0_WATCHHI, REG_C0_XCONTEXT,
		REG_C0_PERR, REG_C0_CACHEERR, REG_C0_TAGLO, REG_C0_TAGHI, REG_C0_ERROREPC,

		REG_COUNT
	};

	enum OperandType : u8 {
		OP_NONE,
		OP_GPR,
		OP_CP0,     // a COP0 register, only fillable by a %c0_* register
		OP_IMM16,   // signed 16-bit
		OP_IMM16U,  // unsigned 16-bit
		OP_IMM5,    // shift amount / cache op selector
		OP_MEM,     // offset(base), signed 16-bit offset
		OP_REL16,   // branch target label
	};

	enum Feature : u16 {
		FEATURE_MIPS_I,
		FEATURE_MIPS_II,
		FEATURE_MIPS_III,
		FEATURE_COP0,
	};

	typedef u8 EncodingFlags;

	struct Encoding {
		Mnemonic      mnemonic;
		OperandType   ops[4];
		u32           bits;
		u32           mask;
		Feature       feature;
		EncodingFlags flags; // bits 4..6: explicit operand count

		bool has_implicit  () const { return ((flags>>7u)&1) != 0; }
		u8   explicit_count() const { return cast(u8)((flags>>4u)&((1u<<3)-1)); }
	};

	enum ClobberFlags : u8 {
		ClobberFlag_None = 0,
	};
	char const *clobber_flag_bit_name(u16 bit) { return "?"; }

	enum ClobberRegs : u8 {
		ClobberReg_RA = 1<<0, // $31, implicit link
		ClobberReg_HI = 1<<1,
		ClobberReg_LO = 1<<2,
	};
	static u8 const CLOBBER_REGS_NAMED = ClobberReg_RA|ClobberReg_HI|ClobberReg_LO;

	enum SideEffectFlags : u8 {
		SideEffectFlag_CONTROL = 1<<0, // writes pc
		SideEffectFlag_SYSTEM  = 1<<1, // COP0 / cache / eret: machine state the compiler cannot see
		SideEffectFlag_FENCE   = 1<<2, // sync
	};

	enum OperandSet : u8 {
		OperandSet_OP0 = 1<<0,
		OperandSet_OP1 = 1<<1,
		OperandSet_OP2 = 1<<2,
		OperandSet_OP3 = 1<<3,
	};

	u16 clobber_bit_for_reg_name(String const &pin) {
		if (pin == "ra") return ClobberReg_RA;
		if (pin == "hi") return ClobberReg_HI;
		if (pin == "lo") return ClobberReg_LO;
		return 0;
	}
	char const *clobber_reg_bit_name(u16 bit) {
		switch (bit) {
		case ClobberReg_RA: return "ra";
		case ClobberReg_HI: return "hi";
		case ClobberReg_LO: return "lo";
		}
		return "<reg>";
	}

	// MIPS has no condition flags register.
	u16 flag_from_name(String const &name)  { return 0; }
	u16 flags_from_name(String const &name) { return 0; }
	i32 flag_bit_from_name(String const &name, i32 *width_) { return -1; }

	struct Clobber {
		OperandSet      written;
		OperandSet      read;
		ClobberRegs     implicit_wr;
		ClobberRegs     implicit_rd;
		ClobberFlags    flags_wr;
		bool            writes_mem;
		bool            reads_mem;
		SideEffectFlags side_effects;

		ClobberFlags flags_rd_call() const { return {}; }
		ClobberFlags flags_wr_call() const { return flags_wr; }

		bool implies_clobber_flags() const { return false; }
		bool implies_clobber_memory() const {
			return writes_mem || reads_mem || (side_effects & (SideEffectFlag_FENCE|SideEffectFlag_SYSTEM)) != 0;
		}
		bool implies_side_effects() const { return side_effects != 0; }
		u8 is_call_or_mem() const {
			return (cast(u16)side_effects & SideEffectFlag_CONTROL) != 0;
		}
		bool has_control() const { return (cast(u16)side_effects & SideEffectFlag_CONTROL) != 0; }
		bool has_halt() const { return false; }
		bool is_conditional(Encoding const &valid_form) const { return has_control(); }
		bool is_nondeterministic() const { return false; }
		bool has_implicit_mem() const {
			// cache ops act on the line addressed by their memory operand
			return (side_effects & SideEffectFlag_SYSTEM) != 0 && (writes_mem || reads_mem);
		}
		bool is_status_snapshot() const { return true; }
	};

	void clobber_implicit_regs(StringSet *clobber_registers_set, u16 implicit_regs) {
		u8 regs = cast(u8)implicit_regs;
		for (u8 bit = 1; bit != 0; bit <<= 1) {
			if ((regs & bit) == 0) {
				continue;
			}
			string_set_update(clobber_registers_set, make_string_c(clobber_reg_bit_name(bit)));
		}
	}

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
		bool is_nondeterministic() const { return false; }
	};

	enum PseudoMnemonic : u16 {
		PM_INVALID,
		PSEUDO_MNEMONIC_COUNT
	};
	PseudoMnemonic pseudo_mnemonic_lookup(String const &name) { return PM_INVALID; }
	PseudoAlias pseudo_alias(u16 pm) { return {}; }
	static String const pseudo_mnemonic_strings[PSEUDO_MNEMONIC_COUNT];

	static u16    const register_codes  [REG_COUNT];
	static String const register_strings[REG_COUNT];

	struct EncodeRun {
		u32 start;
		u32 count;
	};

	static EncodeRun const encode_runs [MNEMONIC_COUNT];
	static Encoding  const encode_forms[];
	static Clobber   const clobber_forms_table[];

	StringMap<Mnemonic> mnemonic_map;
	StringMap<Register> register_map;

	u16 GPRLEN;

	bool init(i64 word_size) {
		// o64: pointers are 32-bit but the VR4300's GPRs are 64-bit (MIPS III).
		gb_unused(word_size);
		GPRLEN = 64;

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

	Mnemonic mnemonic_lookup_ordered(String const &name, u8 *suffixes_) { return M_INVALID; }

	enum PseudoMacroMnemonic : u8 {
		PseudoMacroMnemonic_INVALID,
		PseudoMacroMnemonic_COUNT
	};
	PseudoMacroMnemonic pseudo_macro_mnemonic_lookup(String const &name) { return PseudoMacroMnemonic_INVALID; }

	Mnemonic mnemonic_lookup(String const &name) {
		Mnemonic *found = string_map_get(&mnemonic_map, name);
		return found ? *found : M_INVALID;
	}
	Prefix prefix_lookup(String const &name) { return PREFIX_INVALID; }
	static String const prefix_strings[PREFIX_COUNT];

	Register register_lookup(String const &name) {
		Register *found = string_map_get(&register_map, name);
		return found ? *found : REG_INVALID;
	}
	Slice<Encoding> encoding_forms(u16 m) const {
		EncodeRun r = encode_runs[m];
		return Slice<Encoding>{cast(Encoding *)encode_forms+r.start, r.count};
	}
	Slice<Clobber> clobber_forms(u16 m) const {
		EncodeRun r = encode_runs[m];
		return Slice<Clobber>{cast(Clobber *)clobber_forms_table+r.start, r.count};
	}
	u16 reg_class(u16 r) const { return 0xFF00 & r; }
	u16 reg_size(Register r) const {
		switch (reg_class(register_codes[r])) {
		case REG_CLASS_GPR: return GPRLEN;
		case REG_CLASS_CP0: return 32; // VR4300 has a few 64-bit CP0 regs (dmfc0); 32 for the spike
		}
		return 0;
	}
	// The register's hardware number, used by the LLVM generator.
	u16 reg_number(Register r) const { return 0x00FF & register_codes[r]; }

	bool reg_is_segment(u16 r) { return false; }
	bool integer_reg_width_is_exact() const { return false; }
	bool float_reg_width_is_exact() const { return false; }
	bool supports_memory_index_not_just_disp() const { return false; }

	bool reg_is_non_allocateable(Register r) const {
		switch (r) {
		case REG_ZERO: case REG_AT: case REG_K0: case REG_K1: case REG_GP: case REG_SP:
			return true;
		}
		return reg_class(register_codes[r]) == REG_CLASS_CP0;
	}

	AsmOperandKind kind_from_operand_type(OperandType type) const {
		switch (type) {
		case OP_GPR:
		case OP_CP0:
			return AsmOperand_Register;
		case OP_IMM16:
		case OP_IMM16U:
		case OP_IMM5:
			return AsmOperand_Immediate;
		case OP_MEM:
			return AsmOperand_Memory;
		case OP_REL16:
			return AsmOperand_Label;
		}
		return AsmOperand_Invalid;
	}

	AsmRegClass reg_class_from_operand_type(OperandType type) const {
		switch (type) {
		case OP_GPR: return AsmRegClass_Integer;
		}
		return AsmRegClass_Unknown;
	}
	bool operand_type_is_implicit(OperandType t) const { return false; }
	bool operand_type_is_cond_code(OperandType t) const { return false; }
	bool is_cond_code_name(String name, u32 *bit_code_) const { return false; }
	String required_vector_feature(i32 w) const { return str_lit(""); }
	AsmRegClass operand_type_reg_class(OperandType t) const { return reg_class_from_operand_type(t); }

	u16 operand_type_named_reg_class(OperandType t) const {
		if (t == OP_CP0) {
			return REG_CLASS_CP0;
		}
		return REG_CLASS_NONE;
	}
	String named_reg_class_string(u16 reg_class) const {
		if (reg_class == REG_CLASS_CP0) {
			return str_lit("COP0");
		}
		return str_lit("hardware");
	}

	u16 operand_type_bit_width(OperandType t) const {
		switch (t) {
		case OP_GPR:    return GPRLEN;
		case OP_CP0:    return 0;
		case OP_IMM16:  return 16;
		case OP_IMM16U: return 16;
		case OP_IMM5:   return 5;
		case OP_MEM:    return 0;
		case OP_REL16:  return 0;
		}
		return 0;
	}

	bool operand_type_is_lane(OperandType t) const { return false; }
	String feature_name_from_form(Encoding const &form) const { return {}; }

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

	u16 form_transfer_bytes(Encoding const &form) const {
		switch (form.mnemonic) {
		case M_LB: case M_LBU: case M_SB: return 1;
		case M_LH: case M_LHU: case M_SH: return 2;
		case M_LW: case M_LWU: case M_SW: return 4;
		case M_LD: case M_SD:             return 8;
		}
		return 0;
	}

	bool prefix_kind_okay(u8 prefix, Encoding const &form, bool *requires_memory_dest_) const { return false; }

	AsmOperandConstraint operand_value_constraint(u16 m, int op) const {
		return {AsmOperandConstraint_None, -1};
	}

	bool is_self_zeroing_idiom(u16 m) const {
		switch (m) {
		case M_XOR: case M_SUBU: case M_DSUBU: case M_SLT: case M_SLTU:
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
	str_lit("nop"), str_lit("ssnop"), str_lit("sync"), str_lit("eret"), str_lit("cache"),
	str_lit("mfc0"), str_lit("mtc0"), str_lit("dmfc0"), str_lit("dmtc0"),
	str_lit("addu"), str_lit("subu"), str_lit("daddu"), str_lit("dsubu"), str_lit("and"), str_lit("or"),
	str_lit("xor"), str_lit("nor"), str_lit("slt"), str_lit("sltu"),
	str_lit("addiu"), str_lit("daddiu"), str_lit("slti"), str_lit("sltiu"), str_lit("andi"), str_lit("ori"),
	str_lit("xori"), str_lit("lui"),
	str_lit("sll"), str_lit("srl"), str_lit("sra"), str_lit("dsll"), str_lit("dsrl"), str_lit("dsra"),
	str_lit("dsll32"), str_lit("dsrl32"), str_lit("dsra32"),
	str_lit("move"),
	str_lit("lb"), str_lit("lbu"), str_lit("lh"), str_lit("lhu"), str_lit("lw"), str_lit("lwu"), str_lit("ld"),
	str_lit("sb"), str_lit("sh"), str_lit("sw"), str_lit("sd"),
	str_lit("beq"), str_lit("bne"),
};

#define MIPS_GPR(n) cast(u16)(Asm_mips::REG_CLASS_GPR | (n))
#define MIPS_CP0(n) cast(u16)(Asm_mips::REG_CLASS_CP0 | (n))
u16 const Asm_mips::register_codes[Asm_mips::REG_COUNT] {
	0,
	MIPS_GPR(0),  MIPS_GPR(1),  MIPS_GPR(2),  MIPS_GPR(3),  MIPS_GPR(4),  MIPS_GPR(5),  MIPS_GPR(6),  MIPS_GPR(7),
	MIPS_GPR(8),  MIPS_GPR(9),  MIPS_GPR(10), MIPS_GPR(11), MIPS_GPR(12), MIPS_GPR(13), MIPS_GPR(14), MIPS_GPR(15),
	MIPS_GPR(16), MIPS_GPR(17), MIPS_GPR(18), MIPS_GPR(19), MIPS_GPR(20), MIPS_GPR(21), MIPS_GPR(22), MIPS_GPR(23),
	MIPS_GPR(24), MIPS_GPR(25), MIPS_GPR(26), MIPS_GPR(27), MIPS_GPR(28), MIPS_GPR(29), MIPS_GPR(30), MIPS_GPR(31),
	MIPS_GPR(30),

	MIPS_CP0(0),  MIPS_CP0(1),  MIPS_CP0(2),  MIPS_CP0(3),  MIPS_CP0(4),
	MIPS_CP0(5),  MIPS_CP0(6),  MIPS_CP0(8),  MIPS_CP0(9),  MIPS_CP0(10),
	MIPS_CP0(11), MIPS_CP0(12), MIPS_CP0(13), MIPS_CP0(14), MIPS_CP0(15),
	MIPS_CP0(16), MIPS_CP0(17), MIPS_CP0(18), MIPS_CP0(19), MIPS_CP0(20),
	MIPS_CP0(26), MIPS_CP0(27), MIPS_CP0(28), MIPS_CP0(29), MIPS_CP0(30),
};
#undef MIPS_GPR
#undef MIPS_CP0

String const Asm_mips::register_strings[Asm_mips::REG_COUNT] {
	str_lit(""),
	str_lit("zero"), str_lit("at"), str_lit("v0"), str_lit("v1"), str_lit("a0"), str_lit("a1"), str_lit("a2"), str_lit("a3"),
	str_lit("t0"), str_lit("t1"), str_lit("t2"), str_lit("t3"), str_lit("t4"), str_lit("t5"), str_lit("t6"), str_lit("t7"),
	str_lit("s0"), str_lit("s1"), str_lit("s2"), str_lit("s3"), str_lit("s4"), str_lit("s5"), str_lit("s6"), str_lit("s7"),
	str_lit("t8"), str_lit("t9"), str_lit("k0"), str_lit("k1"), str_lit("gp"), str_lit("sp"), str_lit("fp"), str_lit("ra"),
	str_lit("s8"),

	str_lit("c0_index"), str_lit("c0_random"), str_lit("c0_entrylo0"), str_lit("c0_entrylo1"), str_lit("c0_context"),
	str_lit("c0_pagemask"), str_lit("c0_wired"), str_lit("c0_badvaddr"), str_lit("c0_count"), str_lit("c0_entryhi"),
	str_lit("c0_compare"), str_lit("c0_status"), str_lit("c0_cause"), str_lit("c0_epc"), str_lit("c0_prid"),
	str_lit("c0_config"), str_lit("c0_lladdr"), str_lit("c0_watchlo"), str_lit("c0_watchhi"), str_lit("c0_xcontext"),
	str_lit("c0_perr"), str_lit("c0_cacheerr"), str_lit("c0_taglo"), str_lit("c0_taghi"), str_lit("c0_errorepc"),
};

// One form per mnemonic, so the run for mnemonic m is {m-1, 1}.
#define MIPS_EXPLICIT(n) cast(Asm_mips::EncodingFlags)((n)<<4)
#define F(M, a,b,c, BITS, MASK, FEAT, N) {Asm_mips::M, {Asm_mips::a, Asm_mips::b, Asm_mips::c, Asm_mips::OP_NONE}, BITS, MASK, Asm_mips::FEAT, MIPS_EXPLICIT(N)}
Asm_mips::Encoding const Asm_mips::encode_forms[] = {
	F(M_NOP,    OP_NONE,  OP_NONE,  OP_NONE,   0x00000000, 0xFFFFFFFF, FEATURE_MIPS_I,   0),
	F(M_SSNOP,  OP_NONE,  OP_NONE,  OP_NONE,   0x00000040, 0xFFFFFFFF, FEATURE_MIPS_I,   0),
	F(M_SYNC,   OP_NONE,  OP_NONE,  OP_NONE,   0x0000000F, 0xFFFFFFFF, FEATURE_MIPS_II,  0),
	F(M_ERET,   OP_NONE,  OP_NONE,  OP_NONE,   0x42000018, 0xFFFFFFFF, FEATURE_COP0,     0),
	F(M_CACHE,  OP_IMM5,  OP_MEM,   OP_NONE,   0xBC000000, 0xFC000000, FEATURE_MIPS_III, 2),
	F(M_MFC0,   OP_GPR,   OP_CP0,   OP_NONE,   0x40000000, 0xFFE007FF, FEATURE_COP0,     2),
	F(M_MTC0,   OP_GPR,   OP_CP0,   OP_NONE,   0x40800000, 0xFFE007FF, FEATURE_COP0,     2),
	F(M_DMFC0,  OP_GPR,   OP_CP0,   OP_NONE,   0x40200000, 0xFFE007FF, FEATURE_COP0,     2),
	F(M_DMTC0,  OP_GPR,   OP_CP0,   OP_NONE,   0x40A00000, 0xFFE007FF, FEATURE_COP0,     2),

	F(M_ADDU,   OP_GPR,   OP_GPR,   OP_GPR,    0x00000021, 0xFC0007FF, FEATURE_MIPS_I,   3),
	F(M_SUBU,   OP_GPR,   OP_GPR,   OP_GPR,    0x00000023, 0xFC0007FF, FEATURE_MIPS_I,   3),
	F(M_DADDU,  OP_GPR,   OP_GPR,   OP_GPR,    0x0000002D, 0xFC0007FF, FEATURE_MIPS_III, 3),
	F(M_DSUBU,  OP_GPR,   OP_GPR,   OP_GPR,    0x0000002F, 0xFC0007FF, FEATURE_MIPS_III, 3),
	F(M_AND,    OP_GPR,   OP_GPR,   OP_GPR,    0x00000024, 0xFC0007FF, FEATURE_MIPS_I,   3),
	F(M_OR,     OP_GPR,   OP_GPR,   OP_GPR,    0x00000025, 0xFC0007FF, FEATURE_MIPS_I,   3),
	F(M_XOR,    OP_GPR,   OP_GPR,   OP_GPR,    0x00000026, 0xFC0007FF, FEATURE_MIPS_I,   3),
	F(M_NOR,    OP_GPR,   OP_GPR,   OP_GPR,    0x00000027, 0xFC0007FF, FEATURE_MIPS_I,   3),
	F(M_SLT,    OP_GPR,   OP_GPR,   OP_GPR,    0x0000002A, 0xFC0007FF, FEATURE_MIPS_I,   3),
	F(M_SLTU,   OP_GPR,   OP_GPR,   OP_GPR,    0x0000002B, 0xFC0007FF, FEATURE_MIPS_I,   3),
	F(M_ADDIU,  OP_GPR,   OP_GPR,   OP_IMM16,  0x24000000, 0xFC000000, FEATURE_MIPS_I,   3),
	F(M_DADDIU, OP_GPR,   OP_GPR,   OP_IMM16,  0x64000000, 0xFC000000, FEATURE_MIPS_III, 3),
	F(M_SLTI,   OP_GPR,   OP_GPR,   OP_IMM16,  0x28000000, 0xFC000000, FEATURE_MIPS_I,   3),
	F(M_SLTIU,  OP_GPR,   OP_GPR,   OP_IMM16,  0x2C000000, 0xFC000000, FEATURE_MIPS_I,   3),
	F(M_ANDI,   OP_GPR,   OP_GPR,   OP_IMM16U, 0x30000000, 0xFC000000, FEATURE_MIPS_I,   3),
	F(M_ORI,    OP_GPR,   OP_GPR,   OP_IMM16U, 0x34000000, 0xFC000000, FEATURE_MIPS_I,   3),
	F(M_XORI,   OP_GPR,   OP_GPR,   OP_IMM16U, 0x38000000, 0xFC000000, FEATURE_MIPS_I,   3),
	F(M_LUI,    OP_GPR,   OP_IMM16U, OP_NONE,  0x3C000000, 0xFFE00000, FEATURE_MIPS_I,   2),
	F(M_SLL,    OP_GPR,   OP_GPR,   OP_IMM5,   0x00000000, 0xFFE0003F, FEATURE_MIPS_I,   3),
	F(M_SRL,    OP_GPR,   OP_GPR,   OP_IMM5,   0x00000002, 0xFFE0003F, FEATURE_MIPS_I,   3),
	F(M_SRA,    OP_GPR,   OP_GPR,   OP_IMM5,   0x00000003, 0xFFE0003F, FEATURE_MIPS_I,   3),
	F(M_DSLL,   OP_GPR,   OP_GPR,   OP_IMM5,   0x00000038, 0xFFE0003F, FEATURE_MIPS_III, 3),
	F(M_DSRL,   OP_GPR,   OP_GPR,   OP_IMM5,   0x0000003A, 0xFFE0003F, FEATURE_MIPS_III, 3),
	F(M_DSRA,   OP_GPR,   OP_GPR,   OP_IMM5,   0x0000003B, 0xFFE0003F, FEATURE_MIPS_III, 3),
	F(M_DSLL32, OP_GPR,   OP_GPR,   OP_IMM5,   0x0000003C, 0xFFE0003F, FEATURE_MIPS_III, 3),
	F(M_DSRL32, OP_GPR,   OP_GPR,   OP_IMM5,   0x0000003E, 0xFFE0003F, FEATURE_MIPS_III, 3),
	F(M_DSRA32, OP_GPR,   OP_GPR,   OP_IMM5,   0x0000003F, 0xFFE0003F, FEATURE_MIPS_III, 3),
	F(M_MOVE,   OP_GPR,   OP_GPR,   OP_NONE,   0x0000002D, 0xFC1F07FF, FEATURE_MIPS_III, 2), // daddu rd, rs, $0

	F(M_LB,     OP_GPR,   OP_MEM,   OP_NONE,   0x80000000, 0xFC000000, FEATURE_MIPS_I,   2),
	F(M_LBU,    OP_GPR,   OP_MEM,   OP_NONE,   0x90000000, 0xFC000000, FEATURE_MIPS_I,   2),
	F(M_LH,     OP_GPR,   OP_MEM,   OP_NONE,   0x84000000, 0xFC000000, FEATURE_MIPS_I,   2),
	F(M_LHU,    OP_GPR,   OP_MEM,   OP_NONE,   0x94000000, 0xFC000000, FEATURE_MIPS_I,   2),
	F(M_LW,     OP_GPR,   OP_MEM,   OP_NONE,   0x8C000000, 0xFC000000, FEATURE_MIPS_I,   2),
	F(M_LWU,    OP_GPR,   OP_MEM,   OP_NONE,   0x9C000000, 0xFC000000, FEATURE_MIPS_III, 2),
	F(M_LD,     OP_GPR,   OP_MEM,   OP_NONE,   0xDC000000, 0xFC000000, FEATURE_MIPS_III, 2),
	F(M_SB,     OP_GPR,   OP_MEM,   OP_NONE,   0xA0000000, 0xFC000000, FEATURE_MIPS_I,   2),
	F(M_SH,     OP_GPR,   OP_MEM,   OP_NONE,   0xA4000000, 0xFC000000, FEATURE_MIPS_I,   2),
	F(M_SW,     OP_GPR,   OP_MEM,   OP_NONE,   0xAC000000, 0xFC000000, FEATURE_MIPS_I,   2),
	F(M_SD,     OP_GPR,   OP_MEM,   OP_NONE,   0xFC000000, 0xFC000000, FEATURE_MIPS_III, 2),

	F(M_BEQ,    OP_GPR,   OP_GPR,   OP_REL16,  0x10000000, 0xFC000000, FEATURE_MIPS_I,   3),
	F(M_BNE,    OP_GPR,   OP_GPR,   OP_REL16,  0x14000000, 0xFC000000, FEATURE_MIPS_I,   3),
};
#undef F
#undef MIPS_EXPLICIT

#define W(x) cast(Asm_mips::OperandSet)(x)
#define C(wr, rd, mw, mr, se) {W(wr), W(rd), cast(Asm_mips::ClobberRegs)0, cast(Asm_mips::ClobberRegs)0, Asm_mips::ClobberFlag_None, mw, mr, cast(Asm_mips::SideEffectFlags)(se)}
#define OP0 Asm_mips::OperandSet_OP0
#define OP1 Asm_mips::OperandSet_OP1
#define OP2 Asm_mips::OperandSet_OP2
#define SYS Asm_mips::SideEffectFlag_SYSTEM
#define ALU3 C(OP0, OP1|OP2, false, false, 0)
#define ALUI C(OP0, OP1,     false, false, 0)
#define LOAD C(OP0, OP1,     false, true,  0)
#define STORE C(0,  OP0|OP1, true,  false, 0)
Asm_mips::Clobber const Asm_mips::clobber_forms_table[] = {
	C(0, 0, false, false, 0),                             // nop
	C(0, 0, false, false, 0),                             // ssnop
	C(0, 0, true,  true,  Asm_mips::SideEffectFlag_FENCE), // sync
	C(0, 0, false, false, SYS|Asm_mips::SideEffectFlag_CONTROL), // eret
	C(0, OP1, true, true, SYS),                           // cache op, off(base)
	C(OP0, 0,   false, false, SYS),                       // mfc0
	C(0,   OP0, false, false, SYS),                       // mtc0
	C(OP0, 0,   false, false, SYS),                       // dmfc0
	C(0,   OP0, false, false, SYS),                       // dmtc0

	ALU3, ALU3, ALU3, ALU3, ALU3, ALU3, ALU3, ALU3, ALU3, ALU3, // addu..sltu
	ALUI, ALUI, ALUI, ALUI, ALUI, ALUI, ALUI,                   // addiu..xori
	C(OP0, 0, false, false, 0),                                 // lui
	ALUI, ALUI, ALUI, ALUI, ALUI, ALUI, ALUI, ALUI, ALUI,       // shifts
	ALUI,                                                       // move

	LOAD, LOAD, LOAD, LOAD, LOAD, LOAD, LOAD,
	STORE, STORE, STORE, STORE,

	C(0, OP0|OP1, false, false, Asm_mips::SideEffectFlag_CONTROL), // beq
	C(0, OP0|OP1, false, false, Asm_mips::SideEffectFlag_CONTROL), // bne
};
#undef W
#undef C
#undef OP0
#undef OP1
#undef OP2
#undef SYS
#undef ALU3
#undef ALUI
#undef LOAD
#undef STORE

GB_STATIC_ASSERT(gb_count_of(Asm_mips::encode_forms) == Asm_mips::MNEMONIC_COUNT-1);
GB_STATIC_ASSERT(gb_count_of(Asm_mips::clobber_forms_table) == Asm_mips::MNEMONIC_COUNT-1);

#define R(m) {cast(u32)((m)-1), 1}
Asm_mips::EncodeRun const Asm_mips::encode_runs[Asm_mips::MNEMONIC_COUNT] = {
	{0, 0},
	R(1),  R(2),  R(3),  R(4),  R(5),  R(6),  R(7),  R(8),  R(9),  R(10),
	R(11), R(12), R(13), R(14), R(15), R(16), R(17), R(18), R(19), R(20),
	R(21), R(22), R(23), R(24), R(25), R(26), R(27), R(28), R(29), R(30),
	R(31), R(32), R(33), R(34), R(35), R(36), R(37), R(38), R(39), R(40),
	R(41), R(42), R(43), R(44), R(45), R(46), R(47), R(48), R(49), R(50),
};
#undef R
GB_STATIC_ASSERT(Asm_mips::MNEMONIC_COUNT == 51);
