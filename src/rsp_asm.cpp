// Isolated queue-overlay domain: Odin AST and integers, no CPU checker or LLVM.
#include <deque>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

static std::string rsp_string(String s) {
	return std::string(reinterpret_cast<char const *>(s.text), size_t(s.len));
}
struct RspUnit {
	std::map<std::string, Ast *> constants;
	std::map<std::string, ExactValue> constant_values;
	std::set<std::string> evaluating;
};
static ExactValue rsp_constant(RspUnit &unit, Ast *node) {
	node = unparen_expr(node);
	if (!node) return {};
	if (node->kind == Ast_BasicLit && node->BasicLit.token.kind == Token_Integer)
		return exact_value_from_basic_literal(Token_Integer, node->BasicLit.token.string);
	if (node->kind == Ast_Ident) {
		auto name = rsp_string(node->Ident.token.string);
		auto cached = unit.constant_values.find(name);
		if (cached != unit.constant_values.end()) return cached->second;
		auto found = unit.constants.find(name);
		if (found != unit.constants.end() && unit.evaluating.insert(name).second) {
			auto result = rsp_constant(unit, found->second);
			unit.evaluating.erase(name);
			if (result.kind == ExactValue_Integer) unit.constant_values[name] = result;
			return result;
		}
	}
	if (node->kind == Ast_UnaryExpr) {
		auto op = node->UnaryExpr.op.kind;
		auto value = rsp_constant(unit, node->UnaryExpr.expr);
		if (value.kind == ExactValue_Integer && (op == Token_Add || op == Token_Sub || op == Token_Xor))
			return op == Token_Xor ? exact_binary_operator_value(Token_Sub, exact_unary_operator_value(Token_Sub, value, 0, false), exact_value_i64(1)) : exact_unary_operator_value(op, value, 0, false);
	}
	if (node->kind == Ast_BinaryExpr) {
		auto op = node->BinaryExpr.op.kind;
		auto left = rsp_constant(unit, node->BinaryExpr.left);
		auto right = rsp_constant(unit, node->BinaryExpr.right);
		if (left.kind == ExactValue_Integer && right.kind == ExactValue_Integer &&
		    (op == Token_Add || op == Token_Sub || op == Token_Mul || op == Token_And ||
		     op == Token_Or || op == Token_Xor || op == Token_AndNot))
			return exact_binary_operator_value(op, left, right);
	}
	error(node, "Expected a compile-time RSP integer constant (no runtime values or cycles)");
	return {};
}
static bool rsp_integer(RspUnit &unit, Ast *node, i64 minimum, i64 maximum, i64 *out) {
	auto value = rsp_constant(unit, node);
	if (value.kind != ExactValue_Integer) return false;
	auto lo = exact_value_i64(minimum), hi = exact_value_i64(maximum);
	if (big_int_cmp(&value.value_integer, &lo.value_integer) < 0 ||
	    big_int_cmp(&value.value_integer, &hi.value_integer) > 0) {
		error(node, "RSP integer must be in %lld..%lld", (long long)minimum, (long long)maximum);
		return false;
	}
	*out = big_int_to_i64(&value.value_integer);
	return true;
}
static std::string rsp_quoted(std::string const &value) {
	std::string result = "\"";
	for (char c : value) {
		if (c == '\\' || c == '"') result += '\\';
		result += c;
	}
	return result + "\"";
}
static void rsp_location(std::ostringstream &out, Ast *node) {
	auto pos = ast_token(node).pos;
	out << "#line " << pos.line << " " << rsp_quoted(rsp_string(get_file_path_string(pos.file_id))) << "\n";
}
// Exclusive sibling temporary plus atomic rename; never publish partial output.
static bool rsp_publish(std::string const &path, std::string const &text) {
#if defined(GB_SYSTEM_WINDOWS)
	std::string temporary = path + ".tmp-" + std::to_string(GetCurrentProcessId());
	HANDLE file = CreateFileA(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) return false;
	DWORD written = 0;
	bool ok = WriteFile(file, text.data(), DWORD(text.size()), &written, nullptr) && written == text.size();
	ok = CloseHandle(file) && ok;
	if (ok) ok = MoveFileExA(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
	if (!ok) DeleteFileA(temporary.c_str());
	return ok;
#else
	// open(2) honours umask; mkstemp would publish a 0600 artifact.
	std::string temporary = path + ".tmp-" + std::to_string(getpid());
	int fd = open(temporary.c_str(), O_WRONLY|O_CREAT|O_EXCL, 0666);
	if (fd < 0) return false;
	FILE *file = fdopen(fd, "wb");
	if (!file) { close(fd); unlink(temporary.c_str()); return false; }
	bool ok = fwrite(text.data(), 1, text.size(), file) == text.size();
	ok = (fclose(file) == 0) && ok;
	if (ok) ok = rename(temporary.c_str(), path.c_str()) == 0;
	if (!ok) unlink(temporary.c_str());
	return ok;
#endif
}
// SDK register conventions: rsp_queue.inc (a0..a3 command words, t7 command
// size, gp/ra inherited) and rsp_dma.inc (t0 size, t1 pitch, s0 RDRAM, s4 DMEM).
enum RspGpr { RSP_ZERO = 0, RSP_AT = 1, RSP_A0 = 4, RSP_T0 = 8, RSP_T1 = 9, RSP_T2 = 10,
              RSP_T7 = 15, RSP_S0 = 16, RSP_S4 = 20, RSP_GP = 28, RSP_SP = 29, RSP_FP = 30, RSP_RA = 31 };
// Decimal `<prefix><n>` with no leading zero (or exactly `digits` zero-padded digits); -1 otherwise.
static int rsp_register_number(std::string const &name, char prefix, size_t digits) {
	if (name.size() < 2 || name.size() > 3 || name[0] != prefix ||
	    name.find_first_not_of("0123456789", 1) != std::string::npos) return -1;
	if (digits ? name.size() != digits+1 : (name[1] == '0' && name.size() > 2)) return -1;
	int reg = std::stoi(name.substr(1));
	return reg < 32 ? reg : -1;
}
static int rsp_gpr(Ast *node) {
	if (node->kind == Ast_AsmRegister && !node->AsmRegister.flag.string.len) {
		auto name = rsp_string(node->AsmRegister.name.string);
		static char const *const aliases[32] = {"zero", "at", nullptr, nullptr, "a0", "a1", "a2", "a3",
			"t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "s0", "s1", "s2", "s3",
			"s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"};
		int reg = name == "s8" ? RSP_FP : rsp_register_number(name, 'r', 0);
		for (int i = 0; reg < 0 && i < 32; i++) if (aliases[i] && name == aliases[i]) reg = i;
		if (reg >= 0) {
			if (reg == RSP_SP) error(node, "RSP queue commands cannot use the stack pointer");
			return reg;
		}
	}
	error(node, "Expected an explicit RSP GPR (r0..r31 or an unambiguous ABI alias)");
	return -1;
}
enum RspValueKind { RspUndefined, RspUnknown, RspConstant, RspScratch };
struct RspValue {
	RspValueKind kind = RspUndefined;
	i64 value = 0;
	// A join must not disguise a known bad DMA destination as caller-owned input.
	bool invalid_dma_address = false;
};
static bool rsp_invalid_dma_address(RspValue value) {
	return value.invalid_dma_address || value.kind == RspScratch ||
	       (value.kind == RspConstant && (value.value > 0xffffff || value.value % 8));
}
struct RspEffects {
	RspValue registers[32] = {};
	RspValue vectors[32][8] = {};
	std::vector<RspValue> scratch;
};
static RspEffects rsp_live_inputs(i64 words, i64 scratch) {
	RspEffects state;
	state.scratch.resize(size_t(scratch/4));
	state.registers[RSP_ZERO] = {RspConstant, 0};
	for (auto &element : state.vectors[0]) element = {RspConstant, 0}; // SDK vzero.
	for (int i = 0; i < 4 && i < words; i++) state.registers[RSP_A0+i] = {RspUnknown, 0};
	state.registers[RSP_T7] = {RspConstant, words*4}; // SDK rspq_cmd_size.
	state.registers[RSP_GP] = state.registers[RSP_RA] = {RspUnknown, 0};
	return state;
}
static RspValue rsp_read(RspEffects const &state, Ast *operand, int reg) {
	if (reg < 0) return {};
	auto value = state.registers[reg];
	if (value.kind == RspUndefined) error(operand, "RSP register r%d is read before definition (not a declared queue input)", reg);
	return value;
}
static void rsp_write(RspEffects &state, Ast *operand, int reg, RspValue value) {
	if (reg > RSP_ZERO) state.registers[reg] = value; // gp/ra were rejected by rsp_destination.
}
enum RspScalarOperation { RspNoOperation, RspAdd, RspAnd, RspOr, RspXor };
static RspValue rsp_binary_value(RspScalarOperation operation, RspValue left, RspValue right) {
	if (operation == RspAdd) {
		if (left.kind == RspConstant && right.kind == RspScratch) std::swap(left, right);
		if (left.kind == RspScratch && right.kind == RspConstant) {
			i64 offset = left.value + i64(i32(u32(right.value)));
			// Keep only local affine addresses; never turn wrapped integers into scratch provenance.
			if (offset >= -4096 && offset <= 4096) return {RspScratch, offset};
		}
	}
	if (left.kind != RspConstant || right.kind != RspConstant) {
		bool invalid_address = left.invalid_dma_address || right.invalid_dma_address ||
		                       left.kind == RspScratch || right.kind == RspScratch;
		return {RspUnknown, 0, invalid_address};
	}
	u32 a = u32(left.value), b = u32(right.value);
	if (operation == RspAdd) return {RspConstant, u32(a+b)};
	if (operation == RspAnd) return {RspConstant, a & b};
	if (operation == RspOr) return {RspConstant, a | b};
	if (operation == RspXor) return {RspConstant, a ^ b};
	return {RspUnknown, 0};
}
enum RspVectorMode { RspWholeVector, RspVectorElement };
struct RspVectorOperand {
	int reg = -1;
	int element = -1;
};
static std::string rsp_vector_name(int reg) {
	return std::string("$v") + (reg < 10 ? "0" : "") + std::to_string(reg);
}
static bool rsp_vector_operand(Ast *node, RspVectorMode mode, RspVectorOperand *operand) {
	if (node->kind != Ast_AsmRegister) {
		error(node, "Expected an explicit RSP vector register (v00..v31)"); return false;
	}
	operand->reg = rsp_register_number(rsp_string(node->AsmRegister.name.string), 'v', 2);
	if (operand->reg < 0) {
		error(node, "Expected an explicit RSP vector register (v00..v31)"); return false;
	}
	auto selector = rsp_string(node->AsmRegister.flag.string);
	if (mode == RspWholeVector) {
		if (selector.empty()) return true;
		error(node, "RSP whole-vector operand cannot have an element, byte or broadcast selector"); return false;
	}
	if (selector.size() == 2 && selector[0] == 'e' && selector[1] >= '0' && selector[1] <= '7') {
		operand->element = selector[1] - '0';
		return true;
	}
	error(node, "RSP vector transfer requires an element selector e0..e7 (not a byte or broadcast selector)");
	return false;
}
static RspValue rsp_vector_read(RspEffects const &state, Ast *node, int reg, int element) {
	auto value = state.vectors[reg][element];
	if (value.kind == RspUndefined)
		error(node, "RSP vector v%02d.e%d is read before definition", reg, element);
	return value;
}
static void rsp_apply_vector_xor(RspEffects &state, Ast *node,
                                 RspVectorOperand dest, RspVectorOperand left, RspVectorOperand right) {
	auto operands = node->AsmInstruction.operands;
	for (int element = 0; element < 8; element++) {
		// The zeroing idiom `vxor v, v, v` is defined whatever v holds.
		if (left.reg == right.reg) { state.vectors[dest.reg][element] = {RspConstant, 0}; continue; }
		auto a = rsp_vector_read(state, operands[1], left.reg, element);
		auto b = rsp_vector_read(state, operands[2], right.reg, element);
		state.vectors[dest.reg][element] = rsp_binary_value(RspXor, a, b);
	}
}
static RspValue rsp_vector_insert_value(RspValue value) {
	if (value.kind == RspConstant) return {RspConstant, value.value & 0xffff};
	if (value.kind == RspUndefined) return value;
	return {RspUnknown, 0, value.invalid_dma_address || value.kind == RspScratch}; // No exact scratch address survives truncation.
}
static RspValue rsp_vector_extract_value(RspValue value) {
	// MFC2 sign-extends the selected 16-bit element into a 32-bit GPR.
	if (value.kind == RspConstant && (value.value & 0x8000)) value.value |= 0xffff0000;
	return value;
}
enum RspScalarForm { RspRegisterBinary, RspImmediateBinary, RspUpper, RspLiteral, RspMove };
struct RspScalarInstruction {
	char const *name;
	RspScalarForm form;
	int operands;
	RspScalarOperation operation = RspNoOperation;
	i64 minimum = 0;
	i64 maximum = 0;
};
static RspScalarInstruction const *rsp_scalar_form(std::string const &name) {
	static RspScalarInstruction const forms[] = {
		{"addu", RspRegisterBinary, 3, RspAdd}, {"and", RspRegisterBinary, 3, RspAnd},
		{"or", RspRegisterBinary, 3, RspOr}, {"xor", RspRegisterBinary, 3, RspXor},
		{"addiu", RspImmediateBinary, 3, RspAdd, -32768, 32767},
		{"andi", RspImmediateBinary, 3, RspAnd, 0, 65535},
		{"ori", RspImmediateBinary, 3, RspOr, 0, 65535},
		{"xori", RspImmediateBinary, 3, RspXor, 0, 65535},
		{"lui", RspUpper, 2, RspNoOperation, 0, 65535},
		{"li", RspLiteral, 2, RspNoOperation, -2147483648LL, 4294967295LL},
		{"move", RspMove, 2},
	};
	for (auto const &candidate : forms) if (name == candidate.name) return &candidate;
	return nullptr;
}
static void rsp_materialize(RspScalarForm form, int dest, i64 immediate, std::ostringstream &out) {
	u32 bits = u32(immediate);
	if (form == RspUpper) {
		out << "    lui $" << dest << ", " << immediate << "\n";
	} else if (immediate >= -32768 && immediate <= 32767) {
		out << "    addiu $" << dest << ", $0, " << immediate << "\n";
	} else if (bits <= 65535) {
		out << "    ori $" << dest << ", $0, " << bits << "\n";
	} else {
		out << "    lui $" << dest << ", " << (bits >> 16) << "\n"
		    << "    ori $" << dest << ", $" << dest << ", " << (bits & 65535) << "\n";
	}
}
// Decode once, then use the same instruction effects on ordinary and delayed paths.
// Local labels index instructions, never CPU entities or assembler-supplied symbols.
enum RspInstructionKind { RspScalar, RspAddress, RspLoad, RspStore, RspNop,
                          RspVectorXor, RspVectorInsert, RspVectorExtract,
                          RspEqual, RspNotEqual, RspJump, RspReturn, RspDma };
struct RspInstruction {
	Ast *node = nullptr;
	RspInstructionKind kind = RspNop;
	RspScalarInstruction const *scalar = nullptr;
	RspVectorOperand vector_dest, vector_left, vector_right;
	int dest = -1, left = -1, right = -1;
	i64 immediate = 0;
	size_t target = 0;
	size_t offset = 0;
	int words = 1;
	bool slot = false;
};
struct RspProgram {
	std::vector<RspInstruction> instructions;
	std::map<std::string, size_t> labels;
};
static bool rsp_is_conditional(RspInstruction const &instruction) {
	return instruction.kind == RspEqual || instruction.kind == RspNotEqual;
}
static bool rsp_is_transfer(RspInstruction const &instruction) {
	return instruction.kind >= RspEqual;
}
static bool rsp_destination(Ast *node, int *reg) {
	*reg = rsp_gpr(node);
	if (*reg == RSP_GP || *reg == RSP_RA) {
		error(node, "RSP queue commands cannot modify gp/r28 or ra/r31"); return false;
	}
	return *reg >= 0 && *reg != RSP_SP;
}
static bool rsp_decode_memory(RspUnit &unit, Ast *node, RspInstruction *instruction) {
	if (node->kind != Ast_AsmMemoryOperand) { error(node, "RSP word memory requires [base + constant]"); return false; }
	auto &operand = node->AsmMemoryOperand;
	if (operand.kind != AsmMemoryOperand_Default || operand.segment_override || operand.type ||
	    operand.terms.count < 1 || operand.terms.count > 2) {
		error(node, "RSP word memory has no segment, index, scale, extra displacement or type"); return false;
	}
	auto &base = operand.terms[0]->AsmMemoryTerm;
	if (base.scale) { error(node, "RSP word memory has no scale"); return false; }
	instruction->left = rsp_gpr(base.operand);
	if (operand.terms.count == 2) {
		auto &offset = operand.terms[1]->AsmMemoryTerm;
		if (offset.scale) { error(node, "RSP word memory has no scale"); return false; }
		bool subtract = offset.op.kind == Token_Sub;
		if (!rsp_integer(unit, offset.operand, subtract ? -32767 : -32768, subtract ? 32768 : 32767, &instruction->immediate)) return false;
		if (subtract) instruction->immediate = -instruction->immediate;
	}
	return !any_errors();
}
static bool rsp_decode_instruction(RspUnit &unit, i64 scratch, RspInstruction *instruction) {
	Ast *node = instruction->node;
	auto name = rsp_string(node->AsmInstruction.name->Ident.token.string);
	auto operands = node->AsmInstruction.operands;
	int count = 0;
	if (name == "vxor") { instruction->kind = RspVectorXor; count = 3; }
	else if (name == "mtc2" || name == "mfc2") { instruction->kind = name == "mtc2" ? RspVectorInsert : RspVectorExtract; count = 2; }
	else if (name == "nop") instruction->kind = RspNop;
	else if (name == "beq" || name == "bne") { instruction->kind = name == "beq" ? RspEqual : RspNotEqual; count = 3; }
	else if (name == "j") { instruction->kind = RspJump; count = 1; }
	else if (name == "jr") { instruction->kind = RspReturn; count = 1; }
	else if (name == "la") { instruction->kind = RspAddress; count = 2; }
	else if (name == "lw" || name == "sw") { instruction->kind = name == "lw" ? RspLoad : RspStore; count = 2; }
	else {
		instruction->scalar = rsp_scalar_form(name);
		if (!instruction->scalar) { error(node, "Unsupported RSP scalar instruction or transfer"); return false; }
		instruction->kind = RspScalar;
		count = instruction->scalar->operands;
	}
	if (operands.count != count) { error(node, "RSP %s requires %d operands", name.c_str(), count); return false; }
	if (instruction->kind == RspNop) return true;
	if (instruction->kind == RspVectorXor) {
		return rsp_vector_operand(operands[0], RspWholeVector, &instruction->vector_dest) &&
		       rsp_vector_operand(operands[1], RspWholeVector, &instruction->vector_left) &&
		       rsp_vector_operand(operands[2], RspWholeVector, &instruction->vector_right);
	}
	if (instruction->kind == RspVectorInsert || instruction->kind == RspVectorExtract) {
		if (instruction->kind == RspVectorExtract) {
			if (!rsp_destination(operands[0], &instruction->dest)) return false;
		} else instruction->left = rsp_gpr(operands[0]);
		// One selected vector operand: written by MTC2, read by MFC2.
		if (!rsp_vector_operand(operands[1], RspVectorElement, &instruction->vector_dest)) return false;
		return !any_errors();
	}
	if (rsp_is_conditional(*instruction)) {
		instruction->left = rsp_gpr(operands[0]);
		instruction->right = rsp_gpr(operands[1]);
		return !any_errors(); // Targets resolve after the complete local label inventory.
	}
	if (instruction->kind == RspJump) {
		if (operands[0]->kind == Ast_Ident && operands[0]->Ident.token.string == "DMAOut") instruction->kind = RspDma;
		return true;
	}
	if (instruction->kind == RspReturn) {
		if (rsp_gpr(operands[0]) != RSP_RA) { error(node, "RSP indirect transfer requires inherited queue return jr %%ra"); return false; }
		return !any_errors();
	}
	if (instruction->kind == RspStore) instruction->dest = rsp_gpr(operands[0]);
	else if (!rsp_destination(operands[0], &instruction->dest)) return false;
	if (instruction->kind == RspLoad || instruction->kind == RspStore)
		return rsp_decode_memory(unit, operands[1], instruction);
	if (instruction->kind == RspAddress) {
		if (!scratch || operands[1]->kind != Ast_Ident || operands[1]->Ident.token.string != "rspq_scratch") {
			error(operands[1], "RSP la requires declared rspq_scratch; CPU addresses and other relocations are unsupported"); return false;
		}
		return true;
	}
	auto form = instruction->scalar;
	if (form->form == RspLiteral || form->form == RspUpper) {
		if (!rsp_integer(unit, operands[1], form->minimum, form->maximum, &instruction->immediate)) return false;
		if (form->form == RspLiteral && (instruction->immediate < -32768 || instruction->immediate > 65535)) instruction->words = 2;
	} else {
		instruction->left = rsp_gpr(operands[1]);
		if (form->form == RspRegisterBinary) instruction->right = rsp_gpr(operands[2]);
		else if (form->form == RspImmediateBinary && !rsp_integer(unit, operands[2], form->minimum, form->maximum, &instruction->immediate)) return false;
	}
	return !any_errors();
}
// Signed 16-bit word offset, relative to the delay slot.
static i64 const RSP_BRANCH_MIN = -32768*4, RSP_BRANCH_MAX = 32767*4;
static bool rsp_resolve_target(RspProgram const &program, RspInstruction *instruction) {
	auto operands = instruction->node->AsmInstruction.operands;
	Ast *target = operands[operands.count-1];
	if (target->kind != Ast_AsmLabelDecl) {
		error(target, "RSP branch target must be a scoped .label in this command (not scratch, CPU code or a helper)"); return false;
	}
	auto name = rsp_string(target->AsmLabelDecl.name->Ident.token.string);
	auto found = program.labels.find(name);
	if (found == program.labels.end()) { error(target, "Unresolved RSP label .%s", name.c_str()); return false; }
	if (found->second == program.instructions.size()) { error(target, "RSP branch target is outside the command"); return false; }
	instruction->target = found->second;
	auto const &destination = program.instructions[instruction->target];
	if (destination.slot) { error(target, "RSP branch cannot target a delay slot"); return false; }
	i64 displacement = i64(destination.offset) - i64(instruction->offset + 4);
	if (instruction->kind != RspJump && (displacement < RSP_BRANCH_MIN || displacement > RSP_BRANCH_MAX)) {
		error(target, "RSP branch displacement must fit a signed 16-bit word offset"); return false;
	}
	return true;
}
static bool rsp_collect_instructions(RspUnit &unit, Ast *entry, i64 scratch, RspProgram *program) {
	size_t offset = 0;
	for (auto node : entry->AsmTemplate.instructions) {
		if (node->kind == Ast_AsmLabelDecl) {
			auto name = rsp_string(node->AsmLabelDecl.name->Ident.token.string);
			if (!program->labels.emplace(name, program->instructions.size()).second) {
				error(node, "Duplicate RSP label .%s", name.c_str()); return false;
			}
			continue;
		}
		if (node->kind != Ast_AsmInstruction) { error(node, "RSP directives are unsupported"); return false; }
		RspInstruction instruction;
		instruction.node = node;
		instruction.offset = offset;
		if (!rsp_decode_instruction(unit, scratch, &instruction)) return false;
		offset += size_t(instruction.words*4);
		program->instructions.push_back(instruction);
	}
	if (program->instructions.empty()) { error(entry, "RSP command requires an explicit queue continuation"); return false; }
	return true;
}
static bool rsp_check_delay_slots(RspProgram &program) {
	auto &instructions = program.instructions;
	for (size_t i = 0; i < instructions.size(); i++) {
		if (!rsp_is_transfer(instructions[i])) continue;
		if (i+1 == instructions.size()) { error(instructions[i].node, "RSP transfer requires an explicit delay slot"); return false; }
		auto &slot = instructions[i+1];
		if (rsp_is_transfer(slot) || slot.words != 1) {
			error(slot.node, "RSP delay slot must be one instruction with no transfer or multi-instruction expansion"); return false;
		}
		slot.slot = true;
	}
	return true;
}
static bool rsp_decode_program(RspUnit &unit, Ast *entry, i64 scratch, RspProgram *program) {
	if (!rsp_collect_instructions(unit, entry, scratch, program) || !rsp_check_delay_slots(*program)) return false;
	for (auto &instruction : program->instructions) {
		if ((rsp_is_conditional(instruction) || instruction.kind == RspJump) &&
		    !rsp_resolve_target(*program, &instruction)) return false;
	}
	return true;
}
static bool rsp_word_address(RspEffects const &state, RspInstruction const &instruction, size_t *word) {
	auto base = rsp_read(state, instruction.node, instruction.left);
	if (base.kind != RspScratch) {
		error(instruction.node, "RSP word memory requires a statically known address within declared rspq_scratch"); return false;
	}
	i64 offset = base.value + instruction.immediate;
	if (offset % 4) { error(instruction.node, "RSP word memory address must be 4-byte aligned"); return false; }
	if (offset < 0 || offset + 4 > i64(state.scratch.size()*4)) {
		error(instruction.node, "RSP word memory exceeds declared scratch"); return false;
	}
	*word = size_t(offset/4);
	return true;
}
static bool rsp_apply_instruction(RspEffects &state, RspInstruction const &instruction) {
	Ast *node = instruction.node;
	RspValue result = {RspUnknown, 0};
	if (instruction.kind == RspNop) return true;
	if (instruction.kind == RspVectorXor) {
		rsp_apply_vector_xor(state, node, instruction.vector_dest, instruction.vector_left, instruction.vector_right);
		return !any_errors();
	}
	if (instruction.kind == RspVectorInsert) {
		auto value = rsp_read(state, node, instruction.left);
		state.vectors[instruction.vector_dest.reg][instruction.vector_dest.element] = rsp_vector_insert_value(value);
		return !any_errors();
	}
	if (instruction.kind == RspVectorExtract) {
		auto value = rsp_vector_read(state, node, instruction.vector_dest.reg, instruction.vector_dest.element);
		result = rsp_vector_extract_value(value);
	} else if (instruction.kind == RspAddress) result = {RspScratch, 0};
	else if (instruction.kind == RspLoad || instruction.kind == RspStore) {
		size_t word = 0;
		if (!rsp_word_address(state, instruction, &word)) return false;
		if (instruction.kind == RspStore) {
			state.scratch[word] = rsp_read(state, node, instruction.dest);
			return !any_errors();
		}
		result = state.scratch[word];
		if (result.kind == RspUndefined) { error(node, "RSP load reads uninitialized scratch"); return false; }
	} else {
		auto form = instruction.scalar;
		if (form->form == RspUpper) result = {RspConstant, u32(instruction.immediate) << 16};
		else if (form->form == RspLiteral) result = {RspConstant, u32(instruction.immediate)};
		else {
			auto left = rsp_read(state, node, instruction.left);
			if (form->form == RspMove) result = left;
			else {
				auto right = form->form == RspRegisterBinary ? rsp_read(state, node, instruction.right) : RspValue{RspConstant, u32(instruction.immediate)};
				result = rsp_binary_value(form->operation, left, right);
			}
		}
	}
	rsp_write(state, node, instruction.dest, result);
	return !any_errors();
}
static bool rsp_dma_out(RspEffects &state, Ast *node) {
	// Pinned rsp_dma.inc reads t1 even for height=1. DMAOut is terminal, so its
	// at/t2/s4 clobbers never reach a successor and are not modelled.
	for (int reg : {RSP_T0, RSP_T1, RSP_S0, RSP_S4}) rsp_read(state, node, reg);
	auto size = state.registers[RSP_T0], address = state.registers[RSP_S4], rdram = state.registers[RSP_S0];
	if (size.kind != RspConstant || size.value < 7 || size.value > 4095 || (size.value+1) % 8) {
		error(node, "DMAOut requires a constant single-row byte count minus one in t0 (8..4096 bytes, multiple of 8)"); return false;
	}
	if (address.kind != RspScratch || address.value < 0 || address.value % 8 ||
	    address.value + size.value + 1 > i64(state.scratch.size()*4)) {
		error(node, "DMAOut requires an 8-byte-aligned extent within declared scratch in s4"); return false;
	}
	if (rsp_invalid_dma_address(rdram)) {
		error(node, "DMAOut s0 must be an aligned physical RDRAM address, not a scratch address"); return false;
	}
	for (i64 offset = address.value; offset < address.value + size.value + 1; offset += 4) {
		if (state.scratch[size_t(offset/4)].kind == RspUndefined) {
			error(node, "DMAOut reads uninitialized scratch at byte %lld", (long long)offset); return false;
		}
	}
	return true;
}
// The meet only loses facts. Undefined on either path is undefined at the join;
// differing defined values remain defined but lose constant/address provenance.
static bool rsp_merge_value(RspValue *value, RspValue incoming) {
	RspValue merged = *value;
	if (value->kind == RspUndefined || incoming.kind == RspUndefined) merged = {};
	else {
		if (value->kind != incoming.kind || value->value != incoming.value) merged = {RspUnknown, 0};
		merged.invalid_dma_address = rsp_invalid_dma_address(*value) || rsp_invalid_dma_address(incoming);
	}
	if (merged.kind == value->kind && merged.value == value->value && merged.invalid_dma_address == value->invalid_dma_address) return false;
	*value = merged;
	return true;
}
static bool rsp_merge_vector_value(RspValue *value, RspValue incoming) {
	// MFC2 sign-extends bit 15. Losing a known negative lane at a join must
	// not disguise its eventual 0xffffxxxx GPR result as a valid DMA address.
	incoming.invalid_dma_address |= (value->kind == RspConstant && (value->value & 0x8000)) ||
	                                (incoming.kind == RspConstant && (incoming.value & 0x8000));
	return rsp_merge_value(value, incoming);
}
static bool rsp_merge_effects(RspEffects *state, RspEffects const &incoming) {
	bool changed = false;
	for (int reg = 0; reg < 32; reg++) changed |= rsp_merge_value(&state->registers[reg], incoming.registers[reg]);
	for (size_t word = 0; word < state->scratch.size(); word++) changed |= rsp_merge_value(&state->scratch[word], incoming.scratch[word]);
	for (int reg = 0; reg < 32; reg++) {
		for (int element = 0; element < 8; element++) changed |= rsp_merge_vector_value(&state->vectors[reg][element], incoming.vectors[reg][element]);
	}
	return changed;
}
// Execute one ordinary instruction or one transfer-plus-slot unit. Terminal
// transfers have no successors; both conditional outcomes receive the slot state.
static bool rsp_execute_unit(RspProgram const &program, size_t index, RspEffects &state, std::vector<size_t> *successors) {
	auto const &instructions = program.instructions;
	auto const &instruction = instructions[index];
	size_t next = index + 1;
	if (rsp_is_transfer(instruction)) {
		// The condition and inherited return address are read before the slot.
		if (rsp_is_conditional(instruction)) {
			rsp_read(state, instruction.node, instruction.left);
			rsp_read(state, instruction.node, instruction.right);
		} else if (instruction.kind == RspReturn) rsp_read(state, instruction.node, RSP_RA);
		if (any_errors() || !rsp_apply_instruction(state, instructions[next])) return false;
		next++;
		// DMA helper inputs are consumed after the caller's delay instruction.
		if (instruction.kind == RspDma) {
			if (!rsp_dma_out(state, instruction.node) || any_errors()) return false;
			return true;
		}
		if (instruction.kind == RspReturn) return true;
	} else if (!rsp_apply_instruction(state, instruction)) return false;
	if (instruction.kind == RspJump) successors->push_back(instruction.target);
	else {
		if (next == instructions.size()) { error(instruction.node, "RSP command has reachable fallthrough without a queue continuation"); return false; }
		successors->push_back(next);
		if (rsp_is_conditional(instruction)) successors->push_back(instruction.target);
	}
	return true;
}
static bool rsp_check_flow(RspProgram const &program, i64 words, i64 scratch) {
	auto const &instructions = program.instructions;
	std::vector<RspEffects> inputs(instructions.size());
	std::vector<bool> reached(instructions.size(), false), queued(instructions.size(), false);
	std::deque<size_t> pending;
	inputs[0] = rsp_live_inputs(words, scratch);
	reached[0] = queued[0] = true;
	pending.push_back(0);
	while (!pending.empty()) {
		size_t index = pending.front();
		pending.pop_front();
		queued[index] = false;
		auto state = inputs[index];
		std::vector<size_t> successors;
		if (!rsp_execute_unit(program, index, state, &successors)) return false;
		for (size_t successor : successors) {
			bool changed = !reached[successor];
			if (changed) inputs[successor] = state;
			else changed = rsp_merge_effects(&inputs[successor], state);
			reached[successor] = true;
			if (changed && !queued[successor]) { pending.push_back(successor); queued[successor] = true; }
		}
	}
	return true; // A fixed point proves safety at exits, not termination of loops.
}
static void rsp_emit_instruction(RspInstruction const &instruction, std::ostringstream &out) {
	auto kind = instruction.kind;
	int d = instruction.dest, s = instruction.left, t = instruction.right;
	i64 imm = instruction.immediate;
	if (kind == RspVectorXor)
		out << "    vxor " << rsp_vector_name(instruction.vector_dest.reg) << ", " << rsp_vector_name(instruction.vector_left.reg)
		    << ", " << rsp_vector_name(instruction.vector_right.reg) << "\n";
	else if (kind == RspVectorInsert || kind == RspVectorExtract)
		out << "    " << (kind == RspVectorInsert ? "mtc2" : "mfc2") << " $" << (kind == RspVectorInsert ? s : d)
		    << ", " << rsp_vector_name(instruction.vector_dest.reg) << ".e" << instruction.vector_dest.element << "\n";
	else if (kind == RspNop) out << "    nop\n";
	else if (kind == RspReturn) out << "    jr $31\n";
	else if (kind == RspDma) out << "    j DMAOut\n";
	else if (kind == RspJump) out << "    j .Lrsp_" << instruction.target << "\n";
	else if (rsp_is_conditional(instruction))
		out << "    " << (kind == RspEqual ? "beq" : "bne") << " $" << s << ", $" << t << ", .Lrsp_" << instruction.target << "\n";
	else if (kind == RspAddress) out << "    ori $" << d << ", $0, %lo(rspq_scratch)\n";
	else if (kind == RspLoad || kind == RspStore)
		out << "    " << (kind == RspLoad ? "lw" : "sw") << " $" << d << ", " << imm << "($" << s << ")\n";
	else {
		auto form = instruction.scalar;
		if (form->form == RspUpper || form->form == RspLiteral) rsp_materialize(form->form, d, imm, out);
		else if (form->form == RspMove) out << "    addu $" << d << ", $" << s << ", $0\n";
		else {
			out << "    " << form->name << " $" << d << ", $" << s << ", ";
			if (form->form == RspRegisterBinary) out << "$" << t;
			else out << imm;
			out << "\n";
		}
	}
}
static void rsp_layout_check(std::ostringstream &out, Ast *node, std::string const &condition, char const *diagnostic) {
	rsp_location(out, node);
	out << ".if " << condition << "\n";
	rsp_location(out, node);
	out << ".error " << rsp_quoted(diagnostic) << "\n.endif\n";
}
static void rsp_emit_layout_checks(RspProgram const &program, Ast *entry, std::ostringstream &out) {
	out << ".Lrsp_end:\n";
	rsp_layout_check(out, entry,
		"rsp_command - _ovl_text_start",
		"RSP command entry must start at the aligned SDK overlay boundary");
	for (size_t i = 0; i < program.instructions.size(); i++) {
		auto const &instruction = program.instructions[i];
		if (instruction.kind != RspJump && !rsp_is_conditional(instruction)) continue;
		auto target = ".Lrsp_" + std::to_string(instruction.target);
		rsp_layout_check(out, instruction.node,
			"((" + target + " - rsp_command) % 4) || ((" + target + " - rsp_command) < 0) || ((" + target + " - .Lrsp_end) >= 0)",
			"RSP branch target must be aligned and inside the command");
		if (instruction.kind != RspJump) {
			auto displacement = "(" + target + " - .Lrsp_" + std::to_string(i) + " - 4)";
			rsp_layout_check(out, instruction.node,
				displacement + " < " + std::to_string(RSP_BRANCH_MIN) + " || " + displacement + " > " + std::to_string(RSP_BRANCH_MAX),
				"RSP branch displacement must fit a signed 16-bit word offset");
		}
	}
}
static bool rsp_check_body(RspUnit &unit, Ast *entry, i64 words, i64 scratch, std::ostringstream &out) {
	RspProgram program;
	if (!rsp_decode_program(unit, entry, scratch, &program) || !rsp_check_flow(program, words, scratch)) return false;
	for (size_t i = 0; i < program.instructions.size(); i++) {
		auto const &instruction = program.instructions[i];
		out << ".Lrsp_" << i << ":\n";
		rsp_location(out, instruction.node);
		rsp_emit_instruction(instruction, out);
	}
	rsp_emit_layout_checks(program, entry, out);
	return true;
}

static bool rsp_emit_artifact(Parser *parser, String entry_name, String output_path) {
	RspUnit unit;
	Ast *entry = nullptr, *declaration = nullptr;
	std::set<std::string> names;
	for (auto package : parser->packages) {
		if (package->kind != Package_Init) { error({}, "RSP artifacts accept only their source unit"); continue; }
		for (auto file : package->files) {
			if (rsp_string(file->fullpath).find_first_of("\n\r") != std::string::npos) {
				error(file->pkg_decl, "RSP source path cannot contain a newline"); continue;
			}
			for (auto node : file->decls) {
				if (node->kind != Ast_ValueDecl) { error(node, "RSP units permit only integer constants and one asm template"); continue; }
				auto &d = node->ValueDecl;
				if (d.is_mutable || d.is_using || d.type || d.names.count != 1 || d.values.count != 1 || d.names[0]->kind != Ast_Ident) {
					error(node, "RSP declarations must be one named compile-time value"); continue;
				}
				auto name = rsp_string(d.names[0]->Ident.token.string);
				if (name == "rspq_scratch" || name == "_" || !names.insert(name).second) {
					error(node, "Duplicate or reserved RSP declaration name"); continue;
				}
				if (d.values[0]->kind == Ast_AsmTemplate) {
					if (entry || name != rsp_string(entry_name)) { error(node, "Expected only the selected -rsp-entry template"); continue; }
					entry = d.values[0]; declaration = node;
				} else {
					if (name == rsp_string(entry_name)) error(node, "-rsp-entry must select an asm template");
					if (d.attributes.count) error(node, "Attributes are only supported on the RSP entry");
					unit.constants[name] = d.values[0];
				}
			}
		}
	}
	for (auto const &constant : unit.constants) rsp_constant(unit, constant.second);
	if (!entry) { error({}, "-rsp-entry must select one named parameterless asm template"); return false; }
	std::string output = rsp_string(output_path);
	auto slash = output.find_last_of("/\\");
	auto basename = output.substr(slash == std::string::npos ? 0 : slash+1);
	if (basename.compare(0, 3, "rsp") != 0 || basename.size() < 5 || basename.substr(basename.size()-2) != ".S")
		error(declaration, "RSP output requires -out:<rsp-prefixed-name>.S");
	i64 words = 0, scratch = 0;
	std::set<std::string> attributes;
	for (auto attribute : declaration->ValueDecl.attributes) {
		for (auto element : attribute->Attribute.elems) {
			if (element->kind != Ast_FieldValue || element->FieldValue.field->kind != Ast_Ident) {
				error(element, "RSP metadata requires a named integer value"); continue;
			}
			auto key = rsp_string(element->FieldValue.field->Ident.token.string);
			if (!attributes.insert(key).second) { error(element, "Duplicate RSP metadata"); continue; }
			if (key == "rspq_command_words") rsp_integer(unit, element->FieldValue.value, 1, 62, &words);
			else if (key == "rspq_scratch_bytes") {
				if (rsp_integer(unit, element->FieldValue.value, 0, 4096, &scratch) && scratch % 16)
					error(element, "rspq_scratch_bytes must be a multiple of 16");
			}
			else error(element, "Unknown RSP metadata attribute");
		}
	}
	if (!attributes.count("rspq_command_words")) error(declaration, "rspq_command_words is required in 1..62");
	auto &body = entry->AsmTemplate;
	auto &signature = body.signature->ProcType;
	if ((signature.params && signature.params->FieldList.list.count) ||
	    (signature.results && signature.results->FieldList.list.count) || body.specs.count || body.clobbers.count)
		error(entry, "RSP templates have no parameters, results, specifications or CPU clobbers");
	std::ostringstream out;
	out << "// Checked Odin RSP queue overlay. Assemble with the pinned SDK.\n#include <rsp_queue.inc>\n"
	       ".data\nRSPQ_BeginOverlayHeader\n    RSPQ_DefineCommand rsp_command, " << words*4 <<
	       "\nRSPQ_EndOverlayHeader\nRSPQ_EmptySavedState\n";
	if (scratch) out << ".bss\n.balign 16\nrspq_scratch:\n    .space " << scratch << "\n";
	// SDK vector macros emit .long. Disable MIPS data auto-alignment so their
	// sizes remain fixed for delayed-flow layout assertions, with no hidden pad.
	out << ".text\n.align 0\n.set noreorder\n.set noat\n.set nomacro\n.globl rsp_command\nrsp_command:\n";
	if (!rsp_check_body(unit, entry, words, scratch, out) || any_errors()) return false;
	if (!rsp_publish(output, out.str())) { error(declaration, "Cannot publish RSP artifact at '%.*s'", LIT(output_path)); return false; }
	return true;
}
