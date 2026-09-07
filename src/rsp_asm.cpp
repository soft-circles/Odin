// Isolated queue-overlay domain: Odin AST and integers, no CPU checker or LLVM.
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
	std::string temporary = path + ".tmp-XXXXXX";
	std::vector<char> name(temporary.begin(), temporary.end());
	name.push_back(0);
	int fd = mkstemp(name.data());
	if (fd < 0) return false;
	FILE *file = fdopen(fd, "wb");
	if (!file) { close(fd); unlink(name.data()); return false; }
	bool ok = fwrite(text.data(), 1, text.size(), file) == text.size();
	ok = (fclose(file) == 0) && ok;
	if (ok) ok = rename(name.data(), path.c_str()) == 0;
	if (!ok) unlink(name.data());
	return ok;
#endif
}
static int rsp_gpr(Ast *node) {
	if (node->kind == Ast_AsmRegister && !node->AsmRegister.flag.string.len) {
		auto name = rsp_string(node->AsmRegister.name.string);
		char const *aliases[32] = {"zero", "at", "", "", "a0", "a1", "a2", "a3",
			"t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "s0", "s1", "s2", "s3",
			"s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"};
		for (int i = 0; i < 32; i++) {
			if (name == "r"+std::to_string(i) || name == aliases[i] || (i == 30 && name == "s8")) {
				if (i == 29) error(node, "RSP queue commands cannot use the stack pointer");
				return i;
			}
		}
	}
	error(node, "Expected an explicit RSP GPR (r0..r31 or an unambiguous ABI alias)");
	return -1;
}
enum RspValueKind { RspUndefined, RspUnknown, RspConstant, RspScratch };
struct RspValue {
	RspValueKind kind = RspUndefined;
	i64 value = 0;
};
struct RspEffects {
	RspValue registers[32] = {};
	std::vector<RspValue> scratch;
};
static RspEffects rsp_live_inputs(i64 words, i64 scratch) {
	RspEffects state;
	state.scratch.resize(size_t(scratch/4));
	state.registers[0] = {RspConstant, 0};
	for (int i = 0; i < 4 && i < words; i++) state.registers[4+i] = {RspUnknown, 0};
	state.registers[15] = {RspConstant, words*4}; // SDK rspq_cmd_size (t7).
	state.registers[28] = state.registers[31] = {RspUnknown, 0};
	return state;
}
static RspValue rsp_read(RspEffects const &state, Ast *operand, int reg) {
	if (reg < 0) return {};
	auto value = state.registers[reg];
	if (value.kind == RspUndefined) error(operand, "RSP register r%d is read before definition (not a declared queue input)", reg);
	return value;
}
static void rsp_write(RspEffects &state, Ast *operand, int reg, RspValue value) {
	if (reg < 0) return;
	if (reg == 28 || reg == 31) {
		error(operand, "RSP queue commands cannot modify gp/r28 or ra/r31");
		return;
	}
	if (reg != 0) state.registers[reg] = value;
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
	if (left.kind != RspConstant || right.kind != RspConstant) return {RspUnknown, 0};
	u32 a = u32(left.value), b = u32(right.value);
	if (operation == RspAdd) return {RspConstant, u32(a+b)};
	if (operation == RspAnd) return {RspConstant, a & b};
	if (operation == RspOr) return {RspConstant, a | b};
	if (operation == RspXor) return {RspConstant, a ^ b};
	return {RspUnknown, 0};
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
static RspValue rsp_materialize(RspScalarForm form, int dest, i64 immediate, std::ostringstream &out) {
	u32 bits = u32(immediate);
	if (form == RspUpper) {
		out << "    lui $" << dest << ", " << immediate << "\n";
		bits <<= 16;
	} else if (immediate >= -32768 && immediate <= 32767) {
		out << "    addiu $" << dest << ", $0, " << immediate << "\n";
	} else if (bits <= 65535) {
		out << "    ori $" << dest << ", $0, " << bits << "\n";
	} else {
		out << "    lui $" << dest << ", " << (bits >> 16) << "\n"
		    << "    ori $" << dest << ", $" << dest << ", " << (bits & 65535) << "\n";
	}
	return {RspConstant, bits};
}
static bool rsp_scalar(RspUnit &unit, RspEffects &state, Ast *node, std::ostringstream &out) {
	auto name = rsp_string(node->AsmInstruction.name->Ident.token.string);
	auto operands = node->AsmInstruction.operands;
	auto form = rsp_scalar_form(name);
	if (!form) { error(node, "Unsupported RSP scalar instruction or transfer"); return false; }
	if (operands.count != form->operands) { error(node, "RSP %s requires %d operands", name.c_str(), form->operands); return false; }
	int dest = rsp_gpr(operands[0]);
	RspValue result = {RspUnknown, 0};
	if (form->form == RspLiteral || form->form == RspUpper) {
		i64 immediate = 0;
		if (!rsp_integer(unit, operands[1], form->minimum, form->maximum, &immediate)) return false;
		result = rsp_materialize(form->form, dest, immediate, out);
	} else {
		int source = rsp_gpr(operands[1]);
		auto left = rsp_read(state, operands[1], source);
		if (form->form == RspMove) {
			out << "    addu $" << dest << ", $" << source << ", $0\n";
			result = left;
		} else if (form->form == RspRegisterBinary) {
			int other = rsp_gpr(operands[2]);
			auto right = rsp_read(state, operands[2], other);
			out << "    " << name << " $" << dest << ", $" << source << ", $" << other << "\n";
			result = rsp_binary_value(form->operation, left, right);
		} else {
			i64 immediate = 0;
			if (!rsp_integer(unit, operands[2], form->minimum, form->maximum, &immediate)) return false;
			out << "    " << name << " $" << dest << ", $" << source << ", " << immediate << "\n";
			result = rsp_binary_value(form->operation, left, {RspConstant, u32(immediate)});
		}
	}
	rsp_write(state, operands[0], dest, result);
	return true;
}
static bool rsp_address(RspEffects &state, Ast *node, std::ostringstream &out) {
	auto operands = node->AsmInstruction.operands;
	if (operands.count != 2) { error(node, "RSP la requires a destination and rspq_scratch"); return false; }
	int dest = rsp_gpr(operands[0]);
	if (operands[1]->kind != Ast_Ident || operands[1]->Ident.token.string != "rspq_scratch" || state.scratch.empty()) {
		error(operands[1], "RSP la requires declared rspq_scratch; CPU addresses and other relocations are unsupported"); return false;
	}
	rsp_write(state, operands[0], dest, {RspScratch, 0});
	out << "    ori $" << dest << ", $0, %lo(rspq_scratch)\n";
	return true;
}
struct RspMemory {
	int base = -1;
	i64 displacement = 0;
	size_t word = 0;
};
static bool rsp_memory_operand(RspUnit &unit, RspEffects const &state, Ast *node, RspMemory *memory) {
	if (node->kind != Ast_AsmMemoryOperand) { error(node, "RSP word memory requires [base + constant]"); return false; }
	auto &operand = node->AsmMemoryOperand;
	if (operand.segment_override || operand.scale || operand.disp || operand.type) {
		error(node, "RSP word memory has no segment, index, scale, extra displacement or type"); return false;
	}
	memory->base = rsp_gpr(operand.base);
	auto base = rsp_read(state, operand.base, memory->base);
	if (operand.index) {
		bool subtract = operand.index_op.kind == Token_Sub;
		if (!rsp_integer(unit, operand.index, subtract ? -32767 : -32768, subtract ? 32768 : 32767, &memory->displacement)) return false;
		if (subtract) memory->displacement = -memory->displacement;
	}
	if (base.kind != RspScratch) {
		error(node, "RSP word memory requires a statically known address within declared rspq_scratch"); return false;
	}
	i64 offset = base.value + memory->displacement;
	if (offset % 4) { error(node, "RSP word memory address must be 4-byte aligned"); return false; }
	if (offset < 0 || offset + 4 > i64(state.scratch.size()*4)) {
		error(node, "RSP word memory exceeds declared scratch"); return false;
	}
	memory->word = size_t(offset/4);
	return true;
}
static bool rsp_memory(RspUnit &unit, RspEffects &state, Ast *node, std::ostringstream &out) {
	auto operands = node->AsmInstruction.operands;
	if (operands.count != 2) { error(node, "RSP lw/sw requires a GPR and [base + constant]"); return false; }
	int reg = rsp_gpr(operands[0]);
	RspMemory memory;
	if (!rsp_memory_operand(unit, state, operands[1], &memory)) return false;
	auto name = rsp_string(node->AsmInstruction.name->Ident.token.string);
	if (name == "sw") {
		state.scratch[memory.word] = rsp_read(state, operands[0], reg);
	} else {
		auto value = state.scratch[memory.word];
		if (value.kind == RspUndefined) { error(node, "RSP load reads uninitialized scratch"); return false; }
		rsp_write(state, operands[0], reg, value);
	}
	out << "    " << name << " $" << reg << ", " << memory.displacement << "($" << memory.base << ")\n";
	return true;
}
static bool rsp_dma_out(RspEffects &state, Ast *node) {
	// Pinned rsp_dma.inc reads t1 even for height=1, then destroys at/t2 and adjusts s4.
	for (int reg : {8, 9, 16, 20, 28, 31}) rsp_read(state, node, reg);
	auto size = state.registers[8], address = state.registers[20], rdram = state.registers[16];
	if (size.kind != RspConstant || size.value < 7 || size.value > 4095 || (size.value+1) % 8) {
		error(node, "DMAOut requires a constant single-row byte count minus one in t0 (8..4096 bytes, multiple of 8)"); return false;
	}
	if (address.kind != RspScratch || address.value < 0 || address.value % 8 ||
	    address.value + size.value + 1 > i64(state.scratch.size()*4)) {
		error(node, "DMAOut requires an 8-byte-aligned extent within declared scratch in s4"); return false;
	}
	if (rdram.kind == RspScratch || (rdram.kind == RspConstant && (rdram.value > 0xffffff || rdram.value % 8))) {
		error(node, "DMAOut s0 must be an aligned physical RDRAM address, not a scratch address"); return false;
	}
	for (i64 offset = address.value; offset < address.value + size.value + 1; offset += 4) {
		if (state.scratch[size_t(offset/4)].kind == RspUndefined) {
			error(node, "DMAOut reads uninitialized scratch at byte %lld", (long long)offset); return false;
		}
	}
	state.registers[1] = state.registers[10] = state.registers[20] = {RspUnknown, 0};
	return true;
}
static bool rsp_transfer(RspEffects &state, Ast *node, std::ostringstream &out) {
	auto name = rsp_string(node->AsmInstruction.name->Ident.token.string);
	auto operands = node->AsmInstruction.operands;
	if (name == "jr" && operands.count == 1 && rsp_gpr(operands[0]) == 31) {
		rsp_read(state, operands[0], 31);
		out << "    jr $31\n";
		return true;
	}
	if (name == "j" && operands.count == 1 && operands[0]->kind == Ast_Ident && operands[0]->Ident.token.string == "DMAOut") {
		if (!rsp_dma_out(state, node)) return false;
		out << "    j DMAOut\n";
		return true;
	}
	error(node, "Expected final queue return jr %%ra or tail transfer j DMAOut followed by nop");
	return false;
}
static bool rsp_check_body(RspUnit &unit, Ast *entry, i64 words, i64 scratch, std::ostringstream &out) {
	auto instructions = entry->AsmTemplate.instructions;
	if (instructions.count < 2) { error(entry, "RSP commands require a final queue transfer followed by an explicit nop"); return false; }
	auto state = rsp_live_inputs(words, scratch);
	for (isize i = 0; i < instructions.count; i++) {
		Ast *node = instructions[i];
		if (node->kind != Ast_AsmInstruction) { error(node, "RSP labels and directives are unsupported in the scalar profile"); return false; }
		auto name = rsp_string(node->AsmInstruction.name->Ident.token.string);
		auto operands = node->AsmInstruction.operands;
		out << ".Lrsp_" << i << ":\n";
		rsp_location(out, node);
		if (i == instructions.count-1) {
			if (name != "nop" || operands.count) { error(node, "RSP transfer delay slot must be an explicit nop"); return false; }
			out << "    nop\n";
		} else if (i == instructions.count-2) {
			if (!rsp_transfer(state, node, out)) return false;
		} else if (name == "nop" && operands.count == 0) {
			out << "    nop\n";
		} else if (name == "la") {
			if (!rsp_address(state, node, out)) return false;
		} else if (name == "lw" || name == "sw") {
			if (!rsp_memory(unit, state, node, out)) return false;
		} else if (!rsp_scalar(unit, state, node, out)) return false;
	}
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
	if (!words) error(declaration, "rspq_command_words is required in 1..62");
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
	out << ".text\n.set noreorder\n.set noat\n.set nomacro\n.globl rsp_command\nrsp_command:\n";
	if (!rsp_check_body(unit, entry, words, scratch, out) || any_errors()) return false;
	if (!rsp_publish(output, out.str())) { error(declaration, "Cannot publish RSP artifact at '%.*s'", LIT(output_path)); return false; }
	return true;
}
