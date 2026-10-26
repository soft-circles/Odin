// =============================================================================
// HAND-WRITTEN TABLE - N64 RSP (Reality Signal Processor) asm templates
// =============================================================================
//
// An `@(rsp)` template is RSP Microcode: it is checked against this table in
// whatever build compiles it (Odin64's host Microcode generator), never lowered
// through LLVM, and never called from CPU code. `intrinsics.rsp_template_code`
// encodes it from this table (check_asm_rsp.cpp). The layout follows the MIPS
// table (asm_tables_mips.cpp): the same sections, enums, Encoding/Clobber
// records and AsmCtx interface, so the generic checker needs no RSP branches.
// core:rexcode/isa/rsp is the oracle: tests/rsp_templates encodes every form
// here through both and diffs the words.
//
// Scope: every rexcode RSP mnemonic (core:rexcode/isa/rsp/tablegen/
// encoding_table.odin, 116 mnemonics, one form each) except the nine a job
// body may not use, which mnemonic_unavailable_reason names:
//
//     j jal jr jalr    jumps and calls: a body branches only to its own labels
//     bltzal bgezal    they write %ra, which holds the rspq return address
//     mfc0 mtc0        CP0 (DMA, semaphore, RDP registers): the prologue's and
//                      epilogue's
//     break            halting the RSP is rspq's
//
// Spelling (libdragon's rsp.inc and rexcode's printer):
//
//   * GPRs by ABI name (%t0, %s6, ...). %v0 and %v1 are the GPRs $2/$3, as in
//     libdragon; vector registers are %v00..%v31, always two digits. The COP2
//     control registers of cfc2/ctc2 are %vco, %vcc and %vce.
//   * A VU operand's element is a register accessor: none (whole vector),
//     .q0 .q1, .h0-.h3, .e0-.e7 (element fields 0, 2-3, 4-7, 8-15). Element
//     field 1 has no meaning and no accessor.
//   * Single-lane ops (vrcp vrcpl vrcph vrsq vrsql vrsqh) take two operands,
//     each with a lane, as rsp.inc's two-operand form does: `vrcp %v01.e3,
//     %v02.e5` writes lane 3 of %v01 from lane 5 of %v02. The destination lane
//     goes in the vs field as 0-7 (rexcode's `de`; rsp.inc writes 8-15, and the
//     RSP reads only its low three bits), the source lane in the element field
//     as 8-15. vmov is spelled the same, but rexcode's vmov has no destination
//     lane field, so its destination is always .e0 (vs = 0).
//   * mtc2/mfc2 name a byte of the vector register: .e0-.e7 (lane N, byte 2N)
//     or .b0-.b15 (byte N), or none for byte 0, into bits 10-7.
//   * A vector load/store names its element as rsp.inc does for that
//     instruction: .e0-.e7 (byte 2N) or .b0-.b15 (byte N) for most; lpv, luv,
//     spv and suv take a lane, .e0-.e7, encoded as N; ltv and stv take a lane,
//     .e0-.e7, encoded as byte 2N. No accessor is element 0. Each takes a BYTE
//     offset that must be a multiple of its access size and within -64..63
//     units of it: 1 (lbv sbv), 2 (lsv ssv), 4 (llv slv), 8 (ldv sdv lpv spv
//     luv suv) or 16 bytes (the rest), e.g. `lqv %v01.e0, [input + 16]`. The
//     encoder stores offset/size.

// Register model: nothing is tracked (CLOBBER_REGS_NAMED is empty) and no
// write needs a #clobber: every register outside the pins is the body's to use
// by the job contract. Pinned inputs may be written (a body advances input,
// output and count in place); there is no CPU caller to preserve them for.
// `gp` and `ra` hold rspq state and must not be written, and `sp` must not be
// used at all. Def-before-use, delay slots and VU effects (accumulator,
// VCO/VCC/VCE) stay in Odin64's generator check.
//
// Delay slots: the instruction after a branch is its delay slot and runs on
// both edges, as under `.set noreorder`. Nothing is inserted.

#define ASM_RSP_MNEMONICS(X) \
	/* scalar ALU */ \
	X(ADD, "add") X(ADDU, "addu") X(SUB, "sub") X(SUBU, "subu") X(AND, "and") X(OR, "or") \
	X(XOR, "xor") X(NOR, "nor") X(SLT, "slt") X(SLTU, "sltu") X(SLL, "sll") X(SRL, "srl") \
	X(SRA, "sra") X(SLLV, "sllv") X(SRLV, "srlv") X(SRAV, "srav") X(ADDI, "addi") X(ADDIU, "addiu") \
	X(SLTI, "slti") X(SLTIU, "sltiu") X(ANDI, "andi") X(ORI, "ori") X(XORI, "xori") X(LUI, "lui") \
	/* branches (explicit delay slot) */ \
	X(BEQ, "beq") X(BNE, "bne") X(BLEZ, "blez") X(BGTZ, "bgtz") X(BLTZ, "bltz") X(BGEZ, "bgez") \
	/* scalar loads / stores, nop */ \
	X(LB, "lb") X(LH, "lh") X(LW, "lw") X(LBU, "lbu") X(LHU, "lhu") X(SB, "sb") \
	X(SH, "sh") X(SW, "sw") X(NOP, "nop") \
	/* COP2 moves */ \
	X(MFC2, "mfc2") X(MTC2, "mtc2") X(CFC2, "cfc2") X(CTC2, "ctc2") \
	/* vector unit */ \
	X(VMULF, "vmulf") X(VMULU, "vmulu") X(VMUDL, "vmudl") X(VMUDM, "vmudm") X(VMUDN, "vmudn") X(VMUDH, "vmudh") \
	X(VMACF, "vmacf") X(VMACU, "vmacu") X(VMADL, "vmadl") X(VMADM, "vmadm") X(VMADN, "vmadn") X(VMADH, "vmadh") \
	X(VADD, "vadd") X(VSUB, "vsub") X(VABS, "vabs") X(VADDC, "vaddc") X(VSUBC, "vsubc") X(VSAR, "vsar") \
	X(VLT, "vlt") X(VEQ, "veq") X(VNE, "vne") X(VGE, "vge") X(VCL, "vcl") X(VCH, "vch") \
	X(VCR, "vcr") X(VMRG, "vmrg") X(VAND, "vand") X(VNAND, "vnand") X(VOR, "vor") X(VNOR, "vnor") \
	X(VXOR, "vxor") X(VNXOR, "vnxor") X(VRCP, "vrcp") X(VRCPL, "vrcpl") X(VRCPH, "vrcph") X(VMOV, "vmov") \
	X(VRSQ, "vrsq") X(VRSQL, "vrsql") X(VRSQH, "vrsqh") X(VNOP, "vnop") \
	/* vector loads */ \
	X(LBV, "lbv") X(LSV, "lsv") X(LLV, "llv") X(LDV, "ldv") X(LQV, "lqv") X(LRV, "lrv") \
	X(LPV, "lpv") X(LUV, "luv") X(LHV, "lhv") X(LFV, "lfv") X(LWV, "lwv") X(LTV, "ltv") \
	/* vector stores */ \
	X(SBV, "sbv") X(SSV, "ssv") X(SLV, "slv") X(SDV, "sdv") X(SQV, "sqv") X(SRV, "srv") \
	X(SPV, "spv") X(SUV, "suv") X(SHV, "shv") X(SFV, "sfv") X(SWV, "swv") X(STV, "stv")

// ENUM, name, class, number. %sp ($29) is deliberately absent: see register_unavailable_reason.
#define ASM_RSP_REGISTERS(X) \
	X(ZERO, "zero", GPR, 0)  X(AT, "at", GPR, 1) \
	X(V0, "v0", GPR, 2)   X(V1, "v1", GPR, 3) \
	X(A0, "a0", GPR, 4)   X(A1, "a1", GPR, 5)   X(A2, "a2", GPR, 6)   X(A3, "a3", GPR, 7) \
	X(T0, "t0", GPR, 8)   X(T1, "t1", GPR, 9)   X(T2, "t2", GPR, 10)  X(T3, "t3", GPR, 11) \
	X(T4, "t4", GPR, 12)  X(T5, "t5", GPR, 13)  X(T6, "t6", GPR, 14)  X(T7, "t7", GPR, 15) \
	X(S0, "s0", GPR, 16)  X(S1, "s1", GPR, 17)  X(S2, "s2", GPR, 18)  X(S3, "s3", GPR, 19) \
	X(S4, "s4", GPR, 20)  X(S5, "s5", GPR, 21)  X(S6, "s6", GPR, 22)  X(S7, "s7", GPR, 23) \
	X(T8, "t8", GPR, 24)  X(T9, "t9", GPR, 25)  X(K0, "k0", GPR, 26)  X(K1, "k1", GPR, 27) \
	X(GP, "gp", GPR, 28)  X(FP, "fp", GPR, 30)  X(S8, "s8", GPR, 30)  X(RA, "ra", GPR, 31) \
	X(V00, "v00", VR, 0)  X(V01, "v01", VR, 1)  X(V02, "v02", VR, 2)  X(V03, "v03", VR, 3) \
	X(V04, "v04", VR, 4)  X(V05, "v05", VR, 5)  X(V06, "v06", VR, 6)  X(V07, "v07", VR, 7) \
	X(V08, "v08", VR, 8)  X(V09, "v09", VR, 9)  X(V10, "v10", VR, 10) X(V11, "v11", VR, 11) \
	X(V12, "v12", VR, 12) X(V13, "v13", VR, 13) X(V14, "v14", VR, 14) X(V15, "v15", VR, 15) \
	X(V16, "v16", VR, 16) X(V17, "v17", VR, 17) X(V18, "v18", VR, 18) X(V19, "v19", VR, 19) \
	X(V20, "v20", VR, 20) X(V21, "v21", VR, 21) X(V22, "v22", VR, 22) X(V23, "v23", VR, 23) \
	X(V24, "v24", VR, 24) X(V25, "v25", VR, 25) X(V26, "v26", VR, 26) X(V27, "v27", VR, 27) \
	X(V28, "v28", VR, 28) X(V29, "v29", VR, 29) X(V30, "v30", VR, 30) X(V31, "v31", VR, 31) \
	X(VCO, "vco", VC, 0)  X(VCC, "vcc", VC, 1)  X(VCE, "vce", VC, 2)


struct Asm_rsp {
	enum Mnemonic : u16 {
		M_INVALID,
	#define ASM_RSP_X(e, s) M_##e,
		ASM_RSP_MNEMONICS(ASM_RSP_X)
	#undef ASM_RSP_X

		MNEMONIC_COUNT
	};
	static String const mnemonic_strings[MNEMONIC_COUNT];

	enum Prefix : u8 {
		PREFIX_INVALID,
		PREFIX_COUNT
	};
	enum PrefixKind : u8 { PrefixKind_None };


	static const u16 REG_CLASS_NONE = 0x0000;
	static const u16 REG_CLASS_GPR  = 0x0100; // $0..$31, 32-bit
	static const u16 REG_CLASS_VR   = 0x0200; // $v00..$v31, 8 x 16-bit lanes
	static const u16 REG_CLASS_VC   = 0x0300; // COP2 control: VCO, VCC, VCE, only fillable by name

	enum Register : u16 {
		REG_INVALID,
	#define ASM_RSP_X(e, s, c, n) REG_##e,
		ASM_RSP_REGISTERS(ASM_RSP_X)
	#undef ASM_RSP_X

		REG_COUNT
	};


	enum OperandType : u8 {
		OP_NONE,
		OP_GPR,         // any GPR (32-bit)
		OP_VR,          // a vector register without an element: VU vd/vs
		OP_VR_ELEM,     // a vector register with an optional VU element accessor: VU vt
		OP_VR_LS,       // L/S vt, optional byte element: .e0-.e7 (2N) or .b0-.b15 (N)
		OP_VR_LS_LANE,  // lpv/luv/spv/suv vt, optional lane element: .e0-.e7 (N)
		OP_VR_LS_EVEN,  // ltv/stv vt, optional lane element: .e0-.e7 (byte 2N)
		OP_VR_BYTE,     // mtc2/mfc2 vs, optional byte element: .e0-.e7 (2N) or .b0-.b15 (N)
		OP_VR_DE,       // single-lane vd, required destination lane: .e0-.e7 (N, in vs)
		OP_VR_DE0,      // vmov vd, required destination lane .e0 (rexcode encodes none)
		OP_VR_LANE,     // single-lane vt, required source lane: .e0-.e7 (8+N)
		OP_VC,          // a COP2 control register (%vco, %vcc, %vce), only by name
		OP_SIMM16,      // signed 16-bit
		OP_UIMM16,      // unsigned 16-bit
		OP_SHAMT,       // 5-bit shift amount
		OP_MEM,         // [base + disp], signed 16-bit byte displacement
		OP_VMEM,        // [base + disp], byte displacement scaled by the form's access size
		OP_REL16,       // branch target label (PC-relative, 16-bit word offset)
	};


	enum OperandEncoding : u8 {
		ENC_NONE,
		ENC_RS,           // bits 25..21
		ENC_RT,           // bits 20..16
		ENC_RD,           // bits 15..11
		ENC_SA,           // bits 10..6
		ENC_IMM16,        // bits 15..0
		ENC_OFFSET_BASE,  // base in bits 25..21, offset in bits 15..0
		ENC_BRANCH16,     // (target - (pc+4)) >> 2 in bits 15..0
		ENC_VD,           // bits 10..6
		ENC_VD_DE,        // vd in bits 10..6, its destination lane in bits 15..11 (vs)
		ENC_VS,           // bits 15..11
		ENC_VS_ELEM,      // vs in bits 15..11, byte element in bits 10..7 (mtc2/mfc2)
		ENC_VT,           // vt in bits 20..16, VU element in bits 24..21
		ENC_VT_LS,        // vt in bits 20..16, L/S element in bits 10..7
		ENC_VBASE,        // base in bits 25..21, offset/size in bits 6..0
	};


	enum Feature : u16 {
		FEATURE_RSP_SCALAR,
		FEATURE_RSP_VU,
		FEATURE_RSP_VLS,
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


	// The RSP has no condition-flags register.
	enum ClobberFlags : u8 {
		ClobberFlag_None = 0,
	};

	char const *clobber_flag_bit_name(u16 bit) {
		gb_unused(bit);
		return "?";
	}

	// No register is tracked: see the header.
	enum ClobberRegs : u8 {
		ClobberReg_None = 0,
	};

	static u8 const CLOBBER_REGS_NAMED = 0;

	enum SideEffectFlags : u8 {
		SideEffectFlag_CONTROL = 1<<0, // writes pc
		SideEffectFlag_COND    = 1<<1, // conditional transfer: may fall through
	};

	enum OperandSet : u8 {
		OperandSet_OP0 = 1<<0,
		OperandSet_OP1 = 1<<1,
		OperandSet_OP2 = 1<<2,
		OperandSet_OP3 = 1<<3,
	};

	u16 clobber_bit_for_reg_name(String const &pin) {
		gb_unused(pin);
		return 0;
	}

	char const *clobber_reg_bit_name(u16 bit) {
		gb_unused(bit);
		return "<reg>";
	}

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
		ClobberRegs     implicit_wr; // always empty
		ClobberRegs     implicit_rd; // always empty
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
			return writes_mem || reads_mem;
		}
		bool implies_side_effects() const {
			return side_effects != 0;
		}
		u8 is_call_or_mem() const {
			return 0;
		}
		bool has_control() const {
			return (cast(u16)side_effects & SideEffectFlag_CONTROL) != 0;
		}
		bool has_halt() const {
			return false;
		}
		bool is_conditional(struct Encoding const &valid_form) const {
			gb_unused(valid_form);
			return has_control() && (side_effects & SideEffectFlag_COND) != 0;
		}
		bool is_nondeterministic() const {
			return false;
		}
		bool has_implicit_mem() const {
			return false;
		}
		bool is_status_snapshot() const {
			return true;
		}
	};

	void clobber_implicit_regs(StringSet *clobber_registers_set, u16 implicit_regs) {
		gb_unused(clobber_registers_set);
		gb_unused(implicit_regs);
	}

	// No pseudo-aliases: every spelling is a real mnemonic.
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


	struct EncodeRun {
		u32 start; // start index in encode_forms
		u32 count; // number of forms for this mnemonic
	};

	// One row per form, pairing the operand shape with the Clobber record, as in
	// the MIPS table. init() splits it into the arrays the checker indexes.
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
	u16 VRLEN;

	bool init(i64 word_size);


	Mnemonic mnemonic_lookup_ordered(String const &name, u8 *suffixes_) {
		gb_unused(name);
		gb_unused(suffixes_);
		return M_INVALID;
	}

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
		case REG_CLASS_GPR: return GPRLEN;
		case REG_CLASS_VR:  return VRLEN;
		case REG_CLASS_VC:  return 16;
		}
		return 0;
	}
	// The register's hardware number, used by the encoder.
	u16 reg_number(Register r) const {
		return 0x00FF & register_codes[r];
	}
	bool reg_is_vector(Register r) const {
		return r != REG_INVALID && reg_class(register_codes[r]) == REG_CLASS_VR;
	}

	bool reg_is_segment(/*Register*/ u16 r) {
		gb_unused(r);
		return false;
	}

	// GPRs are 32 bits: a narrower integer fits a GPR slot.
	bool integer_reg_width_is_exact() const {
		return false;
	}
	bool float_reg_width_is_exact() const {
		return true;
	}
	bool supports_memory_index_not_just_disp() const {
		return false;
	}

	bool reg_is_non_allocateable(Register r) const {
		switch (r) {
		case REG_ZERO:
		case REG_GP:
		case REG_RA:
			return true;
		}
		return reg_class(register_codes[r]) == REG_CLASS_VC;
	}

	static bool operand_type_is_vector(OperandType type) {
		switch (type) {
		case OP_VR:
		case OP_VR_ELEM:
		case OP_VR_LS:
		case OP_VR_LS_LANE:
		case OP_VR_LS_EVEN:
		case OP_VR_BYTE:
		case OP_VR_DE:
		case OP_VR_DE0:
		case OP_VR_LANE:
			return true;
		}
		return false;
	}

	AsmOperandKind kind_from_operand_type(OperandType type) const {
		if (operand_type_is_vector(type)) {
			return AsmOperand_Register;
		}
		switch (type) {
		case OP_NONE:
			return AsmOperand_Invalid;

		case OP_GPR:
		case OP_VC:
			return AsmOperand_Register;

		case OP_SIMM16:
		case OP_UIMM16:
		case OP_SHAMT:
			return AsmOperand_Immediate;

		case OP_MEM:
		case OP_VMEM:
			return AsmOperand_Memory;

		case OP_REL16:
			return AsmOperand_Label;
		}
		return AsmOperand_Invalid;
	}

	AsmRegClass reg_class_from_operand_type(OperandType type) const {
		if (operand_type_is_vector(type)) {
			return AsmRegClass_Vector;
		}
		if (type == OP_GPR) {
			return AsmRegClass_Integer;
		}
		return AsmRegClass_Unknown;
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
		gb_unused(w);
		return str_lit("");
	}

	AsmRegClass operand_type_reg_class(OperandType t) const {
		return reg_class_from_operand_type(t);
	}

	// A COP2 control slot is filled only by %vco, %vcc or %vce, never a parameter.
	u16 operand_type_named_reg_class(OperandType t) const {
		return t == OP_VC ? REG_CLASS_VC : REG_CLASS_NONE;
	}

	String named_reg_class_string(u16 reg_class) const {
		if (reg_class == REG_CLASS_VC) {
			return str_lit("COP2 control");
		}
		return str_lit("hardware");
	}

	bool implicit_register_writes_are_full_width() const {
		return true;
	}

	// A GPR name carries no width beyond the file's 32 bits.
	bool literal_register_width_agnostic() const {
		return true;
	}

	u16 operand_type_bit_width(OperandType t) const {
		if (operand_type_is_vector(t)) {
			return VRLEN;
		}
		switch (t) {
		case OP_NONE:     return 0;
		case OP_GPR:      return GPRLEN;
		case OP_VC:       return 0; // named only
		case OP_SIMM16:   return 16;
		case OP_UIMM16:   return 16;
		case OP_SHAMT:    return 5;
		case OP_MEM:      return 0; // access size comes from the mnemonic
		case OP_VMEM:     return 0;
		case OP_REL16:    return 16;
		}
		return 0;
	}

	bool target_has_feature(u64 enabled_features, u32 f) const {
		gb_unused(enabled_features);
		gb_unused(f);
		return true;
	}
	char const *feature_name(u32 f) const {
		gb_unused(f);
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

	// Every form is in the RSP; none needs a target feature.
	String feature_name_from_form(Encoding const &form) const {
		gb_unused(form);
		return str_lit("");
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

	// Bytes a vector load/store moves, which its offset counts in: rsp.inc's sizes.
	static i64 vector_access_size(u16 m) {
		switch (m) {
		case M_LBV: case M_SBV:
			return 1;
		case M_LSV: case M_SSV:
			return 2;
		case M_LLV: case M_SLV:
			return 4;
		case M_LDV: case M_SDV:
		case M_LPV: case M_SPV:
		case M_LUV: case M_SUV:
			return 8;
		}
		return 16; // lqv lrv lhv lfv lwv ltv and their stores
	}

	// Bytes moved by a load/store form; 0 for everything else.
	u16 form_transfer_bytes(Encoding const &form) const {
		switch (form.mnemonic) {
		case M_LB: case M_LBU: case M_SB:
			return 1;
		case M_LH: case M_LHU: case M_SH:
			return 2;
		case M_LW: case M_SW:
			return 4;
		}
		if (form.feature == FEATURE_RSP_VLS) {
			return cast(u16)vector_access_size(form.mnemonic);
		}
		return 0;
	}

	bool prefix_kind_okay(u8 prefix, Encoding const &form, bool *requires_memory_dest_) const {
		gb_unused(prefix);
		gb_unused(form);
		gb_unused(requires_memory_dest_);
		return false;
	}

	// Shift amounts are range-checked by immediate_range (0..31).
	AsmOperandConstraint operand_value_constraint(u16 m, int op) const {
		gb_unused(m);
		gb_unused(op);
		return {AsmOperandConstraint_None, -1};
	}

	bool is_self_zeroing_idiom(u16 m) const {
		gb_unused(m);
		return false;
	}


	// ---------------------------------------------------------------------------
	// Optional checker hooks (see the asm_hook_* overloads in check_asm.cpp).
	// ---------------------------------------------------------------------------

	// An @(rsp) template is checked for the RSP whatever the build targets, so no
	// host architecture's rules apply to it (asm_check_arch), and a pointer
	// parameter is a 32-bit DMEM address (asm_check_pointer_bits).
	TargetArchKind table_arch() const {
		return TargetArch_Invalid;
	}
	i32 table_pointer_bits() const {
		return 32;
	}

	// The RSP instructions rexcode encodes that a job body may not use. They are not
	// in the table, so they fail the mnemonic lookup, and this says why.
	char const *mnemonic_unavailable_reason(String const &name) const {
		if (name == "j" || name == "jal" || name == "jr" || name == "jalr") {
			return "jumps out of the template, which an @(rsp) template cannot: a Microcode body branches only "
			       "to its own labels (beq, bne, blez, bgtz, bltz, bgez) and ends by falling through to the "
			       "generated epilogue";
		}
		if (name == "bltzal" || name == "bgezal") {
			return "writes %ra, which holds the rspq return address; an @(rsp) template branches with bltz or bgez";
		}
		if (name == "mfc0" || name == "mtc0") {
			return "is CP0 access (DMA, semaphore and RDP registers), which belongs to the generated prologue and "
			       "epilogue; an @(rsp) template has none";
		}
		if (name == "break") {
			return "halts the RSP, which only rspq does; an @(rsp) template ends by falling through to the "
			       "generated epilogue";
		}
		return nullptr;
	}

	// A scalar load/store takes a signed 16-bit byte displacement. A vector
	// load/store takes a byte offset within -64..63 units of its access size;
	// memory_disp_scale makes it a multiple of that size.
	bool memory_disp_range(Encoding const &form, OperandType slot, i64 *min_, i64 *max_) const {
		switch (slot) {
		case OP_MEM:
			*min_ = -32768;
			*max_ =  32767;
			return true;
		case OP_VMEM: {
			i64 size = vector_access_size(form.mnemonic);
			*min_ = -64*size;
			*max_ =  63*size;
			return true;
		}
		}
		return false;
	}
	i64 memory_disp_scale(Encoding const &form, OperandType slot) const {
		return slot == OP_VMEM ? vector_access_size(form.mnemonic) : 1;
	}

	bool immediate_range(Encoding const &form, OperandType slot, i64 *min_, i64 *max_) const {
		gb_unused(form);
		return operand_type_imm_range(slot, min_, max_);
	}

	// Every register outside the pins is free by the job contract: no write needs
	// a #clobber.
	bool register_write_needs_clobber(Register r) const {
		gb_unused(r);
		return false;
	}

	// Bodies advance their pinned input, output and count registers in place.
	bool input_parameters_are_writable() const {
		return true;
	}

	// Inputs arrive in the job's register convention; with no register allocator
	// an unpinned parameter would mean nothing.
	char const *unpinned_parameter_reason() const {
		return "an @(rsp) template has no caller to place its parameters, so each one must be pinned to the register "
		       "the job convention puts it in (e.g. '[args = %s6]')";
	}

	// rspq keeps its own state in gp and ra.
	char const *register_write_reject_reason(Register r) const {
		switch (r) {
		case REG_GP: return "holds the rspq command pointer; a Microcode body must not write it";
		case REG_RA: return "holds the rspq return address; a Microcode body must not write it";
		}
		return nullptr;
	}

	char const *register_unavailable_reason(String const &name) const {
		if (name == "sp") {
			return "($29) must not be used in RSP Microcode: rspq gives a body no stack";
		}
		if (name.len == 2 && name[0] == 'v' && gb_char_is_digit(name[1])) {
			// %v0/%v1 are GPRs; %v2..%v9 are neither
			return "is not a register; vector registers are written with two digits (%v00..%v31)";
		}
		return nullptr;
	}

	// A literal register may carry an element accessor only when it is a vector
	// register; which accessors fit is a property of the slot
	// (register_flag_reject_reason).
	bool register_accepts_flag(Register r, String const &flag) const {
		gb_unused(flag);
		return reg_is_vector(r);
	}

	// The element field an accessor encodes in a slot, or -1 when it does not fit
	// (rsp.inc's accessor encoders). No accessor is element 0, except in the
	// single-lane slots, which need one (missing_flag_reason).
	static i32 element_from_flag(OperandType slot, String const &flag) {
		if (flag.len == 0) {
			switch (slot) {
			case OP_VR_DE:
			case OP_VR_DE0:
			case OP_VR_LANE:
				return -1;
			}
			return 0;
		}
		char kind = cast(char)flag[0];
		i64 n = -1;
		if (flag.len >= 2 && flag.len <= 3) {
			n = 0;
			for (isize i = 1; i < flag.len; i++) {
				if (!gb_char_is_digit(cast(char)flag[i])) {
					return -1;
				}
				n = n*10 + (flag[i] - '0');
			}
			if (flag.len == 3 && flag[1] == '0') {
				return -1; // no leading zeros
			}
		}
		if (n < 0) {
			return -1;
		}
		switch (slot) {
		case OP_VR_ELEM:     // veAccessorEncoder
			if (kind == 'q' && n <= 1) return cast(i32)(2 + n);
			if (kind == 'h' && n <= 3) return cast(i32)(4 + n);
			if (kind == 'e' && n <= 7) return cast(i32)(8 + n);
			return -1;
		case OP_VR_LS:       // byteAccessorEncoder
		case OP_VR_BYTE:
			if (kind == 'e' && n <= 7)  return cast(i32)(2*n);
			if (kind == 'b' && n <= 15) return cast(i32)n;
			return -1;
		case OP_VR_LS_LANE:  // laneAccessorEncoder
		case OP_VR_DE:       // the destination lane, as rexcode's `de`
			if (kind == 'e' && n <= 7)  return cast(i32)n;
			return -1;
		case OP_VR_LS_EVEN:  // laneAsByteAccessorEncoder
			if (kind == 'e' && n <= 7)  return cast(i32)(2*n);
			return -1;
		case OP_VR_DE0:
			if (kind == 'e' && n == 0)  return 0;
			return -1;
		case OP_VR_LANE:     // singleLaneAccessorEncoder
			if (kind == 'e' && n <= 7)  return cast(i32)(8 + n);
			return -1;
		}
		return -1;
	}

	char const *register_flag_reject_reason(OperandType slot, String const &flag) const {
		if (element_from_flag(slot, flag) >= 0) {
			return nullptr;
		}
		switch (slot) {
		case OP_VR_ELEM:
			return "is not a VU element accessor; use .q0 .q1, .h0-.h3 or .e0-.e7 "
			       "(element field 1 has no meaning, so it has no accessor)";
		case OP_VR_LS:
			return "is not a load/store element accessor; use .e0-.e7 (lanes) or .b0-.b15 (bytes)";
		case OP_VR_LS_LANE:
			return "is not a packed load/store element accessor; lpv, luv, spv and suv take a lane, .e0-.e7";
		case OP_VR_LS_EVEN:
			return "is not a transposed load/store element accessor; ltv and stv take a lane, .e0-.e7";
		case OP_VR_BYTE:
			return "is not an mtc2/mfc2 element accessor; use .e0-.e7 (lanes) or .b0-.b15 (bytes)";
		case OP_VR_DE:
			return "is not a destination lane; a single-lane instruction writes one lane, .e0-.e7";
		case OP_VR_DE0:
			return "is not vmov's destination lane: the table encodes vmov with no destination lane field, "
			       "which is lane 0, so write .e0";
		case OP_VR_LANE:
			return "is not a source lane; a single-lane instruction reads one lane, .e0-.e7";
		case OP_VR:
			return "takes no element accessor: only a VU instruction's last operand selects elements";
		}
		return "takes no element accessor";
	}

	// Why a vector operand with no accessor does not fit `slot`, or nullptr when it
	// does: the single-lane slots name their lane, as in rsp.inc. The encoder reports it.
	static char const *missing_flag_reason(OperandType slot) {
		switch (slot) {
		case OP_VR_DE:
			return "needs a destination lane, .e0-.e7: a single-lane instruction writes one lane";
		case OP_VR_DE0:
			return "needs its destination lane, .e0: the table encodes vmov with no destination lane field";
		case OP_VR_LANE:
			return "needs a source lane, .e0-.e7: a single-lane instruction reads one lane";
		}
		return nullptr;
	}

	bool operand_type_accepts_register(OperandType t, Register r) const {
		if (r == REG_INVALID) {
			return false;
		}
		u16 cls = reg_class(register_codes[r]);
		if (operand_type_is_vector(t)) {
			return cls == REG_CLASS_VR;
		}
		switch (t) {
		case OP_GPR:
			return cls == REG_CLASS_GPR;
		case OP_VC:
			return cls == REG_CLASS_VC;
		}
		return false;
	}

	char const *literal_register_reject_reason(OperandType t, Register r) const {
		if (operand_type_accepts_register(t, r)) {
			return nullptr;
		}
		if (operand_type_is_vector(t)) {
			if (r == REG_V0 || r == REG_V1) {
				return "a vector register (%v00..%v31); %v0 and %v1 are the GPRs $2 and $3";
			}
			return "a vector register (%v00..%v31)";
		}
		switch (t) {
		case OP_GPR:
			return "a general-purpose register";
		case OP_VC:
			return "a COP2 control register (%vco, %vcc or %vce)";
		}
		return "a register";
	}

	// gp and ra hold rspq state; a body reads them only through the prologue. A
	// pin is an entry contract, so pinning a parameter to either is rejected too.
	char const *pin_reject_reason(Register r) const {
		switch (r) {
		case REG_ZERO: return "it is always zero";
		case REG_GP:   return "it holds the rspq command pointer";
		case REG_RA:   return "it holds the rspq return address";
		}
		if (r != REG_INVALID && reg_class(register_codes[r]) != REG_CLASS_GPR) {
			return "@(rsp) parameters are GPRs";
		}
		return nullptr;
	}

	bool operand_type_imm_range(OperandType t, i64 *lo_, i64 *hi_) const {
		i64 lo = 0, hi = 0;
		switch (t) {
		case OP_SIMM16: lo = -32768; hi = 32767; break;
		case OP_UIMM16: lo = 0;      hi = 65535; break;
		case OP_SHAMT:  lo = 0;      hi = 31;    break;
		default:
			return false;
		}
		if (lo_) *lo_ = lo;
		if (hi_) *hi_ = hi;
		return true;
	}

	bool has_delay_slot(u16 m, Clobber const &clobber) const {
		gb_unused(m);
		return clobber.has_control();
	}

	// The fields of explicit operand slot `enc` holding register number `reg`,
	// element `element` and value `value` (an immediate, displacement or word
	// offset); `ok` is cleared when a value does not fit, which the checker has
	// already reported. Fixed bits come from the form.
	static u32 encode_field(OperandEncoding enc, u32 reg, u32 element, i64 value, bool *ok) {
		switch (enc) {
		case ENC_NONE:        return 0;
		case ENC_RS:          return (reg & 0x1F) << 21;
		case ENC_RT:          return (reg & 0x1F) << 16;
		case ENC_RD:          return (reg & 0x1F) << 11;
		case ENC_SA:
			if (value < 0 || value > 31) *ok = false;
			return (cast(u32)value & 0x1F) << 6;
		case ENC_IMM16:
			if (value < -32768 || value > 65535) *ok = false;
			return cast(u32)value & 0xFFFF;
		case ENC_OFFSET_BASE:
			if (value < -32768 || value > 32767) *ok = false;
			return ((reg & 0x1F) << 21) | (cast(u32)value & 0xFFFF);
		case ENC_BRANCH16:
			if (value < -32768 || value > 32767) *ok = false;
			return cast(u32)value & 0xFFFF;
		case ENC_VD:          return (reg & 0x1F) << 6;
		case ENC_VD_DE:
			if (element > 7) *ok = false;
			return ((reg & 0x1F) << 6) | ((element & 0x1F) << 11);
		case ENC_VS:          return (reg & 0x1F) << 11;
		case ENC_VS_ELEM:
			if (element > 15) *ok = false;
			return ((reg & 0x1F) << 11) | ((element & 0xF) << 7);
		case ENC_VT:
			if (element > 15) *ok = false;
			return ((reg & 0x1F) << 16) | ((element & 0xF) << 21);
		case ENC_VT_LS:
			if (element > 15) *ok = false;
			return ((reg & 0x1F) << 16) | ((element & 0xF) << 7);
		case ENC_VBASE:
			if (value < -64 || value > 63) *ok = false;
			return ((reg & 0x1F) << 21) | (cast(u32)value & 0x7F);
		}
		*ok = false;
		return 0;
	}
};



gb_global Asm_rsp g_asm_rsp;



String const Asm_rsp::prefix_strings[Asm_rsp::PREFIX_COUNT]{};
String const Asm_rsp::pseudo_mnemonic_strings[Asm_rsp::PSEUDO_MNEMONIC_COUNT]{};

String const Asm_rsp::mnemonic_strings[Asm_rsp::MNEMONIC_COUNT] {
	str_lit(""),
#define ASM_RSP_X(e, s) str_lit(s),
	ASM_RSP_MNEMONICS(ASM_RSP_X)
#undef ASM_RSP_X
};

u16 const Asm_rsp::register_codes[Asm_rsp::REG_COUNT] {
	0,
#define ASM_RSP_X(e, s, c, n) cast(u16)(Asm_rsp::REG_CLASS_##c | (n)),
	ASM_RSP_REGISTERS(ASM_RSP_X)
#undef ASM_RSP_X
};

String const Asm_rsp::register_strings[Asm_rsp::REG_COUNT] {
	str_lit(""),
#define ASM_RSP_X(e, s, c, n) str_lit(s),
	ASM_RSP_REGISTERS(ASM_RSP_X)
#undef ASM_RSP_X
};


// -----------------------------------------------------------------------------
// Operand shapes
// -----------------------------------------------------------------------------

enum Asm_rsp::Shape : u8 {
	SH_NONE,       //                                    nop, vnop
	SH_R3,         // rd, rs, rt                         add addu sub subu and or xor nor slt sltu
	SH_SHI,        // rd, rt, sa                         sll srl sra
	SH_SHV,        // rd, rt, rs                         sllv srlv srav
	SH_I_S,        // rt, rs, simm16                     addi addiu slti sltiu
	SH_I_U,        // rt, rs, uimm16                     andi ori xori
	SH_LUI,        // rt, uimm16                         lui
	SH_BR2,        // rs, rt, label                      beq bne
	SH_BR1,        // rs, label                          blez bgtz bltz bgez
	SH_MEM,        // rt, [base + disp]                  lb lh lw lbu lhu sb sh sw
	SH_MXC2,       // rt, vs.byte                        mfc2 mtc2
	SH_CXC2,       // rt, %vco/%vcc/%vce                 cfc2 ctc2
	SH_VU,         // vd, vs, vt.e                       vmulf ... vnxor, vsar
	SH_VSINGLE,    // vd.lane, vt.lane                   vrcp vrcpl vrcph vrsq vrsql vrsqh
	SH_VMOV,       // vd.e0, vt.lane                     vmov
	SH_VLS,        // vt.byte, [base + disp]             lbv lsv llv ldv lqv lrv lhv lfv lwv (and stores)
	SH_VLS_LANE,   // vt.lane, [base + disp]             lpv luv spv suv
	SH_VLS_EVEN,   // vt.lane (byte 2N), [base + disp]   ltv stv

	SHAPE_COUNT
};

#define RS_O(o, e) Asm_rsp::o
#define RS_E(o, e) Asm_rsp::e
#define RS(a, b, c, d) {{RS_O a, RS_O b, RS_O c, RS_O d}, {RS_E a, RS_E b, RS_E c, RS_E d}}
#define N_ (OP_NONE,ENC_NONE)
Asm_rsp::FormShape const Asm_rsp::form_shapes[SHAPE_COUNT] = {
	/* SH_NONE     */ RS(N_, N_, N_, N_),
	/* SH_R3       */ RS((OP_GPR,ENC_RD), (OP_GPR,ENC_RS), (OP_GPR,ENC_RT), N_),
	/* SH_SHI      */ RS((OP_GPR,ENC_RD), (OP_GPR,ENC_RT), (OP_SHAMT,ENC_SA), N_),
	/* SH_SHV      */ RS((OP_GPR,ENC_RD), (OP_GPR,ENC_RT), (OP_GPR,ENC_RS), N_),
	/* SH_I_S      */ RS((OP_GPR,ENC_RT), (OP_GPR,ENC_RS), (OP_SIMM16,ENC_IMM16), N_),
	/* SH_I_U      */ RS((OP_GPR,ENC_RT), (OP_GPR,ENC_RS), (OP_UIMM16,ENC_IMM16), N_),
	/* SH_LUI      */ RS((OP_GPR,ENC_RT), (OP_UIMM16,ENC_IMM16), N_, N_),
	/* SH_BR2      */ RS((OP_GPR,ENC_RS), (OP_GPR,ENC_RT), (OP_REL16,ENC_BRANCH16), N_),
	/* SH_BR1      */ RS((OP_GPR,ENC_RS), (OP_REL16,ENC_BRANCH16), N_, N_),
	/* SH_MEM      */ RS((OP_GPR,ENC_RT), (OP_MEM,ENC_OFFSET_BASE), N_, N_),
	/* SH_MXC2     */ RS((OP_GPR,ENC_RT), (OP_VR_BYTE,ENC_VS_ELEM), N_, N_),
	/* SH_CXC2     */ RS((OP_GPR,ENC_RT), (OP_VC,ENC_RD), N_, N_),
	/* SH_VU       */ RS((OP_VR,ENC_VD), (OP_VR,ENC_VS), (OP_VR_ELEM,ENC_VT), N_),
	/* SH_VSINGLE  */ RS((OP_VR_DE,ENC_VD_DE), (OP_VR_LANE,ENC_VT), N_, N_),
	/* SH_VMOV     */ RS((OP_VR_DE0,ENC_VD), (OP_VR_LANE,ENC_VT), N_, N_),
	/* SH_VLS      */ RS((OP_VR_LS,ENC_VT_LS), (OP_VMEM,ENC_VBASE), N_, N_),
	/* SH_VLS_LANE */ RS((OP_VR_LS_LANE,ENC_VT_LS), (OP_VMEM,ENC_VBASE), N_, N_),
	/* SH_VLS_EVEN */ RS((OP_VR_LS_EVEN,ENC_VT_LS), (OP_VMEM,ENC_VBASE), N_, N_),
};
#undef N_
#undef RS
#undef RS_O
#undef RS_E


// -----------------------------------------------------------------------------
// Forms. Rows for one mnemonic must be adjacent and appear in Mnemonic order.
// Bits and masks are core:rexcode/isa/rsp/tablegen/encoding_table.odin's.
// -----------------------------------------------------------------------------

// Clobber record: CLB(written, read, writes_mem, reads_mem, side_effects)
#define RSP_CLB(wr, rd, wm, rm, se) {cast(Asm_rsp::OperandSet)(wr), cast(Asm_rsp::OperandSet)(rd), \
	Asm_rsp::ClobberReg_None, Asm_rsp::ClobberReg_None, Asm_rsp::ClobberFlag_None, wm, rm, \
	cast(Asm_rsp::SideEffectFlags)(se)}

#define RSP_O0 Asm_rsp::OperandSet_OP0
#define RSP_O1 Asm_rsp::OperandSet_OP1
#define RSP_O2 Asm_rsp::OperandSet_OP2
#define RSP_CTL  Asm_rsp::SideEffectFlag_CONTROL
#define RSP_COND Asm_rsp::SideEffectFlag_COND

#define RSP_C_NONE    RSP_CLB(0,      0,             false, false, 0)
#define RSP_C_W0_R12  RSP_CLB(RSP_O0, RSP_O1|RSP_O2, false, false, 0)  // rd <- rs op rt
#define RSP_C_W0_R1   RSP_CLB(RSP_O0, RSP_O1,        false, false, 0)  // rt <- rs op imm
#define RSP_C_LOAD    RSP_CLB(RSP_O0, RSP_O1,        false, true,  0)
#define RSP_C_STORE   RSP_CLB(0,      RSP_O0|RSP_O1, true,  false, 0)
#define RSP_C_W0      RSP_CLB(RSP_O0, 0,             false, false, 0)  // lui
#define RSP_C_W1_R0   RSP_CLB(RSP_O1, RSP_O0,        false, false, 0)  // mtc2, ctc2
#define RSP_C_BR1     RSP_CLB(0,      RSP_O0,        false, false, RSP_CTL|RSP_COND)
#define RSP_C_BR2     RSP_CLB(0,      RSP_O0|RSP_O1, false, false, RSP_CTL|RSP_COND)
// The accumulator is not modelled (VU effects stay in the generator's check).
#define RSP_C_VU      RSP_CLB(RSP_O0, RSP_O1|RSP_O2, false, false, 0)

// Row: mnemonic, shape, encoding bits, mask of the fixed bits, feature, Clobber
#define R(M, SH, BITS, MASK, FEAT, CLOB) {Asm_rsp::M_##M, Asm_rsp::SH, BITS, MASK, Asm_rsp::FEATURE_##FEAT, CLOB}

Asm_rsp::FormRow const Asm_rsp::form_rows[] = {
	R(ADD,    SH_R3,        0x00000020, 0xFC0007FF, RSP_SCALAR, RSP_C_W0_R12),
	R(ADDU,   SH_R3,        0x00000021, 0xFC0007FF, RSP_SCALAR, RSP_C_W0_R12),
	R(SUB,    SH_R3,        0x00000022, 0xFC0007FF, RSP_SCALAR, RSP_C_W0_R12),
	R(SUBU,   SH_R3,        0x00000023, 0xFC0007FF, RSP_SCALAR, RSP_C_W0_R12),
	R(AND,    SH_R3,        0x00000024, 0xFC0007FF, RSP_SCALAR, RSP_C_W0_R12),
	R(OR,     SH_R3,        0x00000025, 0xFC0007FF, RSP_SCALAR, RSP_C_W0_R12),
	R(XOR,    SH_R3,        0x00000026, 0xFC0007FF, RSP_SCALAR, RSP_C_W0_R12),
	R(NOR,    SH_R3,        0x00000027, 0xFC0007FF, RSP_SCALAR, RSP_C_W0_R12),
	R(SLT,    SH_R3,        0x0000002A, 0xFC0007FF, RSP_SCALAR, RSP_C_W0_R12),
	R(SLTU,   SH_R3,        0x0000002B, 0xFC0007FF, RSP_SCALAR, RSP_C_W0_R12),
	R(SLL,    SH_SHI,       0x00000000, 0xFFE0003F, RSP_SCALAR, RSP_C_W0_R1),
	R(SRL,    SH_SHI,       0x00000002, 0xFFE0003F, RSP_SCALAR, RSP_C_W0_R1),
	R(SRA,    SH_SHI,       0x00000003, 0xFFE0003F, RSP_SCALAR, RSP_C_W0_R1),
	R(SLLV,   SH_SHV,       0x00000004, 0xFC0007FF, RSP_SCALAR, RSP_C_W0_R12),
	R(SRLV,   SH_SHV,       0x00000006, 0xFC0007FF, RSP_SCALAR, RSP_C_W0_R12),
	R(SRAV,   SH_SHV,       0x00000007, 0xFC0007FF, RSP_SCALAR, RSP_C_W0_R12),
	R(ADDI,   SH_I_S,       0x20000000, 0xFC000000, RSP_SCALAR, RSP_C_W0_R1),
	R(ADDIU,  SH_I_S,       0x24000000, 0xFC000000, RSP_SCALAR, RSP_C_W0_R1),
	R(SLTI,   SH_I_S,       0x28000000, 0xFC000000, RSP_SCALAR, RSP_C_W0_R1),
	R(SLTIU,  SH_I_S,       0x2C000000, 0xFC000000, RSP_SCALAR, RSP_C_W0_R1),
	R(ANDI,   SH_I_U,       0x30000000, 0xFC000000, RSP_SCALAR, RSP_C_W0_R1),
	R(ORI,    SH_I_U,       0x34000000, 0xFC000000, RSP_SCALAR, RSP_C_W0_R1),
	R(XORI,   SH_I_U,       0x38000000, 0xFC000000, RSP_SCALAR, RSP_C_W0_R1),
	R(LUI,    SH_LUI,       0x3C000000, 0xFFE00000, RSP_SCALAR, RSP_C_W0),
	R(BEQ,    SH_BR2,       0x10000000, 0xFC000000, RSP_SCALAR, RSP_C_BR2),
	R(BNE,    SH_BR2,       0x14000000, 0xFC000000, RSP_SCALAR, RSP_C_BR2),
	R(BLEZ,   SH_BR1,       0x18000000, 0xFC1F0000, RSP_SCALAR, RSP_C_BR1),
	R(BGTZ,   SH_BR1,       0x1C000000, 0xFC1F0000, RSP_SCALAR, RSP_C_BR1),
	R(BLTZ,   SH_BR1,       0x04000000, 0xFC1F0000, RSP_SCALAR, RSP_C_BR1),
	R(BGEZ,   SH_BR1,       0x04010000, 0xFC1F0000, RSP_SCALAR, RSP_C_BR1),
	R(LB,     SH_MEM,       0x80000000, 0xFC000000, RSP_SCALAR, RSP_C_LOAD),
	R(LH,     SH_MEM,       0x84000000, 0xFC000000, RSP_SCALAR, RSP_C_LOAD),
	R(LW,     SH_MEM,       0x8C000000, 0xFC000000, RSP_SCALAR, RSP_C_LOAD),
	R(LBU,    SH_MEM,       0x90000000, 0xFC000000, RSP_SCALAR, RSP_C_LOAD),
	R(LHU,    SH_MEM,       0x94000000, 0xFC000000, RSP_SCALAR, RSP_C_LOAD),
	R(SB,     SH_MEM,       0xA0000000, 0xFC000000, RSP_SCALAR, RSP_C_STORE),
	R(SH,     SH_MEM,       0xA4000000, 0xFC000000, RSP_SCALAR, RSP_C_STORE),
	R(SW,     SH_MEM,       0xAC000000, 0xFC000000, RSP_SCALAR, RSP_C_STORE),
	R(NOP,    SH_NONE,      0x00000000, 0xFFFFFFFF, RSP_SCALAR, RSP_C_NONE),
	R(MFC2,   SH_MXC2,      0x48000000, 0xFFE0007F, RSP_SCALAR, RSP_C_W0_R1),
	R(MTC2,   SH_MXC2,      0x48800000, 0xFFE0007F, RSP_SCALAR, RSP_C_W1_R0),
	R(CFC2,   SH_CXC2,      0x48400000, 0xFFE007FF, RSP_SCALAR, RSP_C_W0_R1),
	R(CTC2,   SH_CXC2,      0x48C00000, 0xFFE007FF, RSP_SCALAR, RSP_C_W1_R0),
	R(VMULF,  SH_VU,        0x4A000000, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VMULU,  SH_VU,        0x4A000001, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VMUDL,  SH_VU,        0x4A000004, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VMUDM,  SH_VU,        0x4A000005, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VMUDN,  SH_VU,        0x4A000006, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VMUDH,  SH_VU,        0x4A000007, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VMACF,  SH_VU,        0x4A000008, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VMACU,  SH_VU,        0x4A000009, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VMADL,  SH_VU,        0x4A00000C, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VMADM,  SH_VU,        0x4A00000D, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VMADN,  SH_VU,        0x4A00000E, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VMADH,  SH_VU,        0x4A00000F, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VADD,   SH_VU,        0x4A000010, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VSUB,   SH_VU,        0x4A000011, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VABS,   SH_VU,        0x4A000013, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VADDC,  SH_VU,        0x4A000014, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VSUBC,  SH_VU,        0x4A000015, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VSAR,   SH_VU,        0x4A00001D, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VLT,    SH_VU,        0x4A000020, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VEQ,    SH_VU,        0x4A000021, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VNE,    SH_VU,        0x4A000022, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VGE,    SH_VU,        0x4A000023, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VCL,    SH_VU,        0x4A000024, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VCH,    SH_VU,        0x4A000025, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VCR,    SH_VU,        0x4A000026, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VMRG,   SH_VU,        0x4A000027, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VAND,   SH_VU,        0x4A000028, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VNAND,  SH_VU,        0x4A000029, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VOR,    SH_VU,        0x4A00002A, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VNOR,   SH_VU,        0x4A00002B, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VXOR,   SH_VU,        0x4A00002C, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VNXOR,  SH_VU,        0x4A00002D, 0xFE00003F, RSP_VU,     RSP_C_VU),
	R(VRCP,   SH_VSINGLE,   0x4A000030, 0xFE00003F, RSP_VU,     RSP_C_W0_R1),
	R(VRCPL,  SH_VSINGLE,   0x4A000031, 0xFE00003F, RSP_VU,     RSP_C_W0_R1),
	R(VRCPH,  SH_VSINGLE,   0x4A000032, 0xFE00003F, RSP_VU,     RSP_C_W0_R1),
	R(VMOV,   SH_VMOV,      0x4A000033, 0xFE00003F, RSP_VU,     RSP_C_W0_R1),
	R(VRSQ,   SH_VSINGLE,   0x4A000034, 0xFE00003F, RSP_VU,     RSP_C_W0_R1),
	R(VRSQL,  SH_VSINGLE,   0x4A000035, 0xFE00003F, RSP_VU,     RSP_C_W0_R1),
	R(VRSQH,  SH_VSINGLE,   0x4A000036, 0xFE00003F, RSP_VU,     RSP_C_W0_R1),
	R(VNOP,   SH_NONE,      0x4A000037, 0xFE00003F, RSP_VU,     RSP_C_NONE),
	R(LBV,    SH_VLS,       0xC8000000, 0xFC00F800, RSP_VLS,    RSP_C_LOAD),
	R(LSV,    SH_VLS,       0xC8000800, 0xFC00F800, RSP_VLS,    RSP_C_LOAD),
	R(LLV,    SH_VLS,       0xC8001000, 0xFC00F800, RSP_VLS,    RSP_C_LOAD),
	R(LDV,    SH_VLS,       0xC8001800, 0xFC00F800, RSP_VLS,    RSP_C_LOAD),
	R(LQV,    SH_VLS,       0xC8002000, 0xFC00F800, RSP_VLS,    RSP_C_LOAD),
	R(LRV,    SH_VLS,       0xC8002800, 0xFC00F800, RSP_VLS,    RSP_C_LOAD),
	R(LPV,    SH_VLS_LANE,  0xC8003000, 0xFC00F800, RSP_VLS,    RSP_C_LOAD),
	R(LUV,    SH_VLS_LANE,  0xC8003800, 0xFC00F800, RSP_VLS,    RSP_C_LOAD),
	R(LHV,    SH_VLS,       0xC8004000, 0xFC00F800, RSP_VLS,    RSP_C_LOAD),
	R(LFV,    SH_VLS,       0xC8004800, 0xFC00F800, RSP_VLS,    RSP_C_LOAD),
	R(LWV,    SH_VLS,       0xC8005000, 0xFC00F800, RSP_VLS,    RSP_C_LOAD),
	R(LTV,    SH_VLS_EVEN,  0xC8005800, 0xFC00F800, RSP_VLS,    RSP_C_LOAD),
	R(SBV,    SH_VLS,       0xE8000000, 0xFC00F800, RSP_VLS,    RSP_C_STORE),
	R(SSV,    SH_VLS,       0xE8000800, 0xFC00F800, RSP_VLS,    RSP_C_STORE),
	R(SLV,    SH_VLS,       0xE8001000, 0xFC00F800, RSP_VLS,    RSP_C_STORE),
	R(SDV,    SH_VLS,       0xE8001800, 0xFC00F800, RSP_VLS,    RSP_C_STORE),
	R(SQV,    SH_VLS,       0xE8002000, 0xFC00F800, RSP_VLS,    RSP_C_STORE),
	R(SRV,    SH_VLS,       0xE8002800, 0xFC00F800, RSP_VLS,    RSP_C_STORE),
	R(SPV,    SH_VLS_LANE,  0xE8003000, 0xFC00F800, RSP_VLS,    RSP_C_STORE),
	R(SUV,    SH_VLS_LANE,  0xE8003800, 0xFC00F800, RSP_VLS,    RSP_C_STORE),
	R(SHV,    SH_VLS,       0xE8004000, 0xFC00F800, RSP_VLS,    RSP_C_STORE),
	R(SFV,    SH_VLS,       0xE8004800, 0xFC00F800, RSP_VLS,    RSP_C_STORE),
	R(SWV,    SH_VLS,       0xE8005000, 0xFC00F800, RSP_VLS,    RSP_C_STORE),
	R(STV,    SH_VLS_EVEN,  0xE8005800, 0xFC00F800, RSP_VLS,    RSP_C_STORE),
};
isize const Asm_rsp::form_row_count = gb_count_of(Asm_rsp::form_rows);

#undef R
#undef RSP_C_NONE
#undef RSP_C_W0_R12
#undef RSP_C_W0_R1
#undef RSP_C_LOAD
#undef RSP_C_STORE
#undef RSP_C_BR1
#undef RSP_C_W0
#undef RSP_C_W1_R0
#undef RSP_C_BR2
#undef RSP_C_VU
#undef RSP_O0
#undef RSP_O1
#undef RSP_O2
#undef RSP_CTL
#undef RSP_COND
#undef RSP_CLB


bool Asm_rsp::init(i64 word_size) {
	// The RSP's width is fixed whatever the build target.
	gb_unused(word_size);
	GPRLEN = 32;
	VRLEN  = 128;

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

		u16 m = cast(u16)row.mnemonic;
		GB_ASSERT_MSG(m >= prev, "asm_tables_rsp: form rows out of order at %.*s", LIT(mnemonic_strings[m]));
		if (m != prev) {
			encode_runs[m].start = cast(u32)i;
			prev = m;
		}
		encode_runs[m].count += 1;
	}
	for (u16 m = M_INVALID+1; m < MNEMONIC_COUNT; m++) {
		GB_ASSERT_MSG(encode_runs[m].count != 0, "asm_tables_rsp: mnemonic %.*s has no forms", LIT(mnemonic_strings[m]));
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
