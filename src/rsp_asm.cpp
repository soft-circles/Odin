// Isolated queue-overlay domain: Odin AST and integers, no CPU checker or LLVM.
#include <map>
#include <bitset>
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
	std::map<std::string, int> labels;
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
struct RspInstruction {
	Ast *node = nullptr;
	std::string text;
	u32 reads = 0, writes = 0;
	u8 vector_reads[32] = {}, vector_writes[32] = {};
	bool accumulator_write = false;
	bool transfer = false, dma = false, conditional = false;
	int target = -1;
	int words = 1;
	std::string operation;
	int destination = -1, left = -1, right = -1, base = -1;
	i64 immediate = 0;
};
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
	return 0;
}
static int rsp_read_gpr(RspInstruction &instruction, Ast *node) {
	int reg = rsp_gpr(node);
	if (instruction.left < 0) instruction.left = reg; else instruction.right = reg;
	instruction.reads |= u32(1) << reg;
	return reg;
}
static int rsp_write_gpr(RspInstruction &instruction, Ast *node) {
	int reg = rsp_gpr(node);
	if (reg == 28 || reg == 31) error(node, "RSP queue gp and ra are protected");
	instruction.destination = reg;
	if (reg != 0) instruction.writes |= u32(1) << reg;
	return reg;
}
static std::string rsp_reg(int index) { return "$"+std::to_string(index); }
static int rsp_vector(Ast *node, bool element, int *lane) {
	if (node->kind == Ast_AsmRegister) {
		auto name = rsp_string(node->AsmRegister.name.string);
		auto selector = rsp_string(node->AsmRegister.flag.string);
		if (name.size() == 3 && name[0] == 'v' && name[1] >= '0' && name[1] <= '3' && name[2] >= '0' && name[2] <= '9') {
			int reg = (name[1]-'0')*10 + name[2]-'0';
			if (reg <= 31 && ((!element && selector.empty()) ||
			    (element && selector.size() == 2 && selector[0] == 'e' && selector[1] >= '0' && selector[1] <= '7'))) {
				*lane = element ? selector[1]-'0' : 0;
				return reg;
			}
		}
	}
	error(node, element ? "Expected an RSP vector v00..v31 with element e0..e7" : "Expected a whole RSP vector v00..v31 without selector");
	*lane = 0;
	return 0;
}
static std::string rsp_vreg(int reg) { return "$v" + std::string(reg < 10 ? "0" : "") + std::to_string(reg); }
static RspInstruction rsp_decode(RspUnit &unit, Ast *node, i64 scratch) {
	RspInstruction result;
	result.node = node;
	if (node->kind != Ast_AsmInstruction) { error(node, "Unsupported RSP directive or label"); return result; }
	auto &instruction = node->AsmInstruction;
	auto name = rsp_string(instruction.name->Ident.token.string);
	result.operation = name;
	auto args = instruction.operands;
	i64 value = 0;
	std::ostringstream text;
	text << "    ";
	if (name == "nop" && args.count == 0) text << "nop";
	else if (name == "jr" && args.count == 1) {
		if (rsp_read_gpr(result, args[0]) != 31) error(node, "Only the inherited queue return is permitted");
		result.transfer = true;
		text << "jr $31";
	} else if (name == "j" && args.count == 1 && args[0]->kind == Ast_Ident && args[0]->Ident.token.string == "DMAOut") {
		result.transfer = result.dma = true;
		text << "j DMAOut";
	} else if ((name == "addu" || name == "and" || name == "or" || name == "xor") && args.count == 3) {
		int d = rsp_write_gpr(result, args[0]), a = rsp_read_gpr(result, args[1]), b = rsp_read_gpr(result, args[2]);
		text << name << " " << rsp_reg(d) << ", " << rsp_reg(a) << ", " << rsp_reg(b);
	} else if ((name == "addiu" || name == "andi" || name == "ori") && args.count == 3) {
		int d = rsp_write_gpr(result, args[0]), a = rsp_read_gpr(result, args[1]);
		rsp_integer(unit, args[2], name == "addiu" ? -32768 : 0, name == "addiu" ? 32767 : 65535, &value);
		text << name << " " << rsp_reg(d) << ", " << rsp_reg(a) << ", " << value;
	} else if ((name == "lui" || name == "li" || name == "la") && args.count == 2) {
		int d = rsp_write_gpr(result, args[0]);
		if (name == "la") {
			if (!scratch || args[1]->kind != Ast_Ident || args[1]->Ident.token.string != "rspq_scratch")
				error(node, "la accepts only the declared rspq_scratch symbol");
			text << "ori " << rsp_reg(d) << ", $0, %lo(rspq_scratch)";
		} else if (name == "lui") {
			rsp_integer(unit, args[1], 0, 65535, &value);
			text << "lui " << rsp_reg(d) << ", " << value;
		} else {
			rsp_integer(unit, args[1], -2147483648LL, 4294967295LL, &value);
			if (value >= 0 && value <= 65535) text << "ori " << rsp_reg(d) << ", $0, " << value;
			else if (value >= -32768 && value < 0) text << "addiu " << rsp_reg(d) << ", $0, " << value;
			else {
				text << "lui " << rsp_reg(d) << ", " << (u32(value)>>16) << "\n    ori " << rsp_reg(d) << ", " << rsp_reg(d) << ", " << (u32(value)&65535);
				result.words = 2;
			}
		}
	} else if ((name == "lw" || name == "sw") && args.count == 2 && args[1]->kind == Ast_AsmMemoryOperand) {
		int reg = name == "lw" ? rsp_write_gpr(result, args[0]) : rsp_read_gpr(result, args[0]);
		auto &memory = args[1]->AsmMemoryOperand;
		int base = rsp_read_gpr(result, memory.base);
		result.base = base;
		if (memory.segment_override || memory.scale || memory.disp || memory.type)
			error(node, "RSP word memory accepts only [GPR + constant displacement]");
		if (memory.index) {
			rsp_integer(unit, memory.index, memory.index_op.kind == Token_Sub ? -32767 : -32768, memory.index_op.kind == Token_Sub ? 32768 : 32767, &value);
			if (memory.index_op.kind == Token_Sub) value = -value;
		}
		if (value < -32768 || value > 32767) error(node, "RSP word displacement must fit signed 16 bits");
		text << name << " " << rsp_reg(reg) << ", " << value << "(" << rsp_reg(base) << ")";
	} else if ((name == "beq" || name == "bne" || name == "j") && args.count == (name == "j" ? 1 : 3)) {
		Ast *target = args[args.count-1];
		if (target->kind != Ast_AsmLabelDecl) error(target, "RSP branch target must be a scoped .label");
		else {
			auto label = rsp_string(target->AsmLabelDecl.name->Ident.token.string);
			auto found = unit.labels.find(label);
			if (found == unit.labels.end()) error(target, "Unresolved RSP label");
			else result.target = found->second;
		}
		result.transfer = true;
		result.conditional = name != "j";
		text << name << " ";
		if (result.conditional) {
			int a = rsp_read_gpr(result, args[0]), b = rsp_read_gpr(result, args[1]);
			text << rsp_reg(a) << ", " << rsp_reg(b) << ", ";
		}
		text << ".Lrsp_" << result.target;
	} else if (name == "vxor" && args.count == 3) {
		int lane;
		int d = rsp_vector(args[0], false, &lane), a = rsp_vector(args[1], false, &lane), b = rsp_vector(args[2], false, &lane);
		result.vector_reads[a] = result.vector_reads[b] = 255;
		result.vector_writes[d] = 255;
		result.accumulator_write = true;
		text << "vxor " << rsp_vreg(d) << ", " << rsp_vreg(a) << ", " << rsp_vreg(b);
	} else if ((name == "mtc2" || name == "mfc2") && args.count == 2) {
		int lane;
		int vector = rsp_vector(args[1], true, &lane);
		int scalar;
		if (name == "mtc2") {
			scalar = rsp_read_gpr(result, args[0]);
			result.vector_writes[vector] = u8(1u<<lane);
		} else {
			scalar = rsp_write_gpr(result, args[0]);
			result.vector_reads[vector] = u8(1u<<lane);
		}
		text << name << " " << rsp_reg(scalar) << ", " << rsp_vreg(vector) << ".e" << lane;
	} else error(node, "Unsupported RSP instruction or operand form");
	result.immediate = value;
	result.text = text.str()+"\n";
	return result;
}
struct RspValue {
	// Unknown, a known 32-bit scalar, or an offset within the declared scratch.
	int kind = 0;
	u32 value = 0;
};
struct RspState {
	u32 defined = 0;
	u8 vectors[32] = {};
	bool accumulator_defined = false;
	RspValue values[32];
	std::bitset<4096> scratch;
};
static void rsp_apply(RspInstruction const &ins, RspState &state, i64 scratch_size, bool diagnose = true) {
	auto report = [&](char const *message) { if (diagnose) error(ins.node, "%s", message); };
	if (ins.reads & ~state.defined) report("RSP instruction reads an undefined GPR");
	for (int i = 0; i < 32; i++) {
		if (ins.vector_reads[i] & ~state.vectors[i]) report("RSP instruction reads undefined vector elements");
		state.vectors[i] |= ins.vector_writes[i];
	}
	state.accumulator_defined |= ins.accumulator_write;
	RspValue value, left, right;
	if (ins.left >= 0) left = state.values[ins.left];
	if (ins.right >= 0) right = state.values[ins.right];
	auto const &op = ins.operation;
	if (op == "li") value = {1, u32(ins.immediate)};
	else if (op == "lui") value = {1, u32(ins.immediate)<<16};
	else if (op == "la") value = {2, 0};
	else if (op == "addu" && left.kind && right.kind && !(left.kind == 2 && right.kind == 2))
		value = {left.kind == 2 || right.kind == 2 ? 2 : 1, left.value+right.value};
	else if (op == "addiu" && left.kind) value = {left.kind, left.value+u32(ins.immediate)};
	else if ((op == "andi" || op == "ori") && left.kind == 1)
		value = {1, op == "andi" ? left.value&u32(ins.immediate) : left.value|u32(ins.immediate)};
	else if ((op == "and" || op == "or" || op == "xor") && left.kind == 1 && right.kind == 1)
		value = {1, op == "and" ? left.value&right.value : op == "or" ? left.value|right.value : left.value^right.value};
	if (ins.base >= 0) {
		auto base = state.values[ins.base];
		i64 offset = (base.kind == 2 ? i64(i32(base.value)) : i64(base.value))+ins.immediate;
		if (base.kind != 2) report("RSP word memory must derive from the declared scratch symbol");
		if (base.kind && offset % 4) report("RSP word address is statically misaligned");
		if (base.kind == 2) {
			if (offset < 0 || offset+4 > scratch_size) report("RSP word access exceeds declared scratch");
			else for (i64 i = offset; i < offset+4; i++) {
				if (op == "sw") state.scratch.set(size_t(i));
				else if (!state.scratch.test(size_t(i))) report("RSP load reads uninitialized scratch");
			}
		}
	}
	state.defined |= ins.writes;
	if (ins.destination > 0) state.values[ins.destination] = value;
}
static void rsp_check_dma(RspInstruction const &ins, RspState const &state, i64 scratch_size) {
	u32 inputs = (1u<<8)|(1u<<9)|(1u<<16)|(1u<<20);
	if ((state.defined & inputs) != inputs) { error(ins.node, "DMAOut requires initialized t0, t1, s0 and s4"); return; }
	auto size = state.values[8], base = state.values[20];
	if (size.kind != 1 || size.value >= 4096 || (size.value+1)%8 || base.kind != 2 || base.value%8 ||
	    u64(base.value)+u64(size.value)+1 > u64(scratch_size)) {
		error(ins.node, "DMAOut requires a known single-row aligned transfer within declared scratch"); return;
	}
	for (u32 i = base.value; i <= base.value+size.value; i++) {
		if (!state.scratch.test(i)) { error(ins.node, "DMAOut reads uninitialized scratch bytes"); break; }
	}
	// Pinned tail helper consumes t0/t1/s0/s4, clobbers t2/at/s4, and returns
	// through inherited ra. No authored instruction follows this exit.
}
static bool rsp_merge(RspState &into, RspState const &other) {
	bool changed = false;
	u32 defined = into.defined & other.defined;
	changed |= defined != into.defined;
	into.defined = defined;
	for (int i = 0; i < 32; i++) {
		auto &value = into.values[i];
		if (value.kind && (value.kind != other.values[i].kind || value.value != other.values[i].value)) {
			value = {}; changed = true;
		}
		u8 elements = into.vectors[i] & other.vectors[i];
		changed |= elements != into.vectors[i];
		into.vectors[i] = elements;
	}
	auto scratch = into.scratch & other.scratch;
	changed |= scratch != into.scratch;
	into.scratch = scratch;
	bool accumulator = into.accumulator_defined && other.accumulator_defined;
	changed |= accumulator != into.accumulator_defined;
	into.accumulator_defined = accumulator;
	return changed;
}
static void rsp_check_flow(std::vector<RspInstruction> const &instructions, i64 words, i64 scratch) {
	int count = int(instructions.size());
	if (!count) return;
	std::vector<bool> slots(count, false), seen(count, false);
	for (int i = 0; i < count; i++) {
		if (instructions[i].transfer) {
			if (i+1 == count) error(instructions[i].node, "RSP transfer requires an explicit delay slot");
			else {
				slots[i+1] = true;
				if (instructions[i+1].transfer || instructions[i+1].words != 1)
					error(instructions[i+1].node, "Delay slot must be one non-transfer instruction");
			}
		}
	}
	for (auto const &ins : instructions) {
		if (ins.target >= count || (ins.target >= 0 && slots[ins.target]))
			error(ins.node, "RSP branch must target an instruction outside a delay slot");
	}
	if (any_errors()) return;
	std::vector<RspState> states(count);
	auto &initial = states[0];
	initial.defined = 1 | (1u<<15) | (1u<<28) | (1u<<31);
	initial.values[0] = {1, 0}; initial.values[15] = {1, u32(words*4)};
	initial.vectors[0] = initial.vectors[30] = initial.vectors[31] = 255;
	for (int i = 0; i < words && i < 4; i++) initial.defined |= 1u<<(4+i);
	seen[0] = true;
	std::vector<int> pending{0};
	while (!pending.empty()) {
		int i = pending.back(); pending.pop_back();
		auto const &ins = instructions[i];
		RspState state = states[i];
		rsp_apply(ins, state, scratch, false);
		if (ins.transfer) rsp_apply(instructions[i+1], state, scratch, false);
		auto propagate = [&](int target) {
			if (target >= count) return;
			if (!seen[target]) { states[target] = state; seen[target] = true; pending.push_back(target); }
			else if (rsp_merge(states[target], state)) pending.push_back(target);
		};
		if (ins.target >= 0) propagate(ins.target);
		if (!ins.transfer || ins.conditional) propagate(i+(ins.transfer ? 2 : 1));
	}
	for (int i = 0; i < count; i++) {
		if (!seen[i]) continue;
		auto const &ins = instructions[i];
		RspState state = states[i];
		rsp_apply(ins, state, scratch);
		if (ins.transfer) rsp_apply(instructions[i+1], state, scratch);
		if (ins.dma) rsp_check_dma(ins, state, scratch);
		if ((!ins.transfer || ins.conditional) && i+(ins.transfer ? 2 : 1) >= count)
			error(ins.node, "RSP command falls through without queue continuation");
	}
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
			else if (key == "rspq_scratch_bytes") rsp_integer(unit, element->FieldValue.value, 0, 4096, &scratch);
			else error(element, "Unknown RSP metadata attribute");
		}
	}
	if (scratch % 16) error(declaration, "rspq_scratch_bytes must be a multiple of 16");
	if (!words) error(declaration, "rspq_command_words is required in 1..62");
	auto &body = entry->AsmTemplate;
	auto &signature = body.signature->ProcType;
	if ((signature.params && signature.params->FieldList.list.count) ||
	    (signature.results && signature.results->FieldList.list.count) || body.specs.count || body.clobbers.count)
		error(entry, "RSP templates have no parameters, results, specifications or CPU clobbers");
	std::ostringstream out;
	out << "// Checked Odin RSP queue overlay. Assemble with the pinned SDK.\n#include <rsp_queue.inc>\n"
	       ".data\nRSPQ_BeginOverlayHeader\n    RSPQ_DefineCommand rsp_command, " << words*4 <<
	       "\nRSPQ_EndOverlayHeader\nRSPQ_EmptySavedState\n.text\n.globl rsp_command\nrsp_command:\n";
	if (scratch) out << ".bss\n.balign 16\nrspq_scratch:\n    .space " << scratch << "\n.text\n";
	int index = 0;
	for (auto node : body.instructions) {
		if (node->kind == Ast_AsmLabelDecl) {
			auto name = rsp_string(node->AsmLabelDecl.name->Ident.token.string);
			if (!unit.labels.insert({name, index}).second) error(node, "Duplicate RSP label");
		} else index++;
	}
	std::vector<RspInstruction> instructions;
	for (auto node : body.instructions) {
		if (node->kind != Ast_AsmLabelDecl) instructions.push_back(rsp_decode(unit, node, scratch));
	}
	if (instructions.empty()) error(entry, "RSP command requires a queue continuation");
	if (any_errors()) return false;
	rsp_check_flow(instructions, words, scratch);
	index = 0;
	for (auto const &instruction : instructions) {
		out << ".Lrsp_" << index++ << ":\n";
		rsp_location(out, instruction.node);
		out << instruction.text;
	}
	if (any_errors()) return false;
	if (!rsp_publish(output, out.str())) { error(declaration, "Cannot publish RSP artifact at '%.*s'", LIT(output_path)); return false; }
	return true;
}
