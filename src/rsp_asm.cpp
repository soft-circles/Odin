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
	return 0;
}
static bool rsp_check_return(Ast *entry) {
	auto instructions = entry->AsmTemplate.instructions;
	if (instructions.count != 2) {
		error(entry, "Minimal RSP commands require exactly jr %%ra followed by nop; other instructions and labels are unsupported");
		return false;
	}
	for (isize i = 0; i < instructions.count; i++) {
		Ast *node = instructions[i];
		if (node->kind != Ast_AsmInstruction) {
			error(node, "RSP labels and directives are unsupported in the minimal profile");
			return false;
		}
		auto &instruction = node->AsmInstruction;
		String expected = i == 0 ? str_lit("jr") : str_lit("nop");
		if (instruction.name->Ident.token.string != expected || instruction.operands.count != (i == 0 ? 1 : 0)) {
			error(node, "Expected %.*s%s; unsupported RSP instruction or operand form", LIT(expected), i == 0 ? " %ra" : " delay slot");
			return false;
		}
		if (i == 0 && rsp_gpr(instruction.operands[0]) != 31) {
			error(node, "Only the inherited queue return register r31 (ra) is permitted");
			return false;
		}
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
				if (rsp_integer(unit, element->FieldValue.value, 0, 4096, &scratch) && scratch != 0)
					error(element, "Nonzero rspq_scratch_bytes is unsupported in the minimal RSP profile");
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
	       "\nRSPQ_EndOverlayHeader\nRSPQ_EmptySavedState\n.text\n.globl rsp_command\nrsp_command:\n";
	if (!rsp_check_return(entry) || any_errors()) return false;
	for (isize i = 0; i < body.instructions.count; i++) {
		out << ".Lrsp_" << i << ":\n";
		rsp_location(out, body.instructions[i]);
		out << (i == 0 ? "    jr $31\n" : "    nop\n");
	}
	if (any_errors()) return false;
	if (!rsp_publish(output, out.str())) { error(declaration, "Cannot publish RSP artifact at '%.*s'", LIT(output_path)); return false; }
	return true;
}
