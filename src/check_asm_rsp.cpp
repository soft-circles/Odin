// check_asm_rsp.cpp: intrinsics.rsp_template_code
//
//     rsp_template_code :: proc(t: <an @(rsp) asm template>) -> []Rsp_Code_Line
//     Rsp_Code_Line     :: struct {word: u32, line: i32}
//
// Returns the template's encoded RSP words, one per instruction, each with the line
// of the template instruction it came from, as a constant built like
// `#load(path, []T)`: the value is the elements' bytes in the target's byte order. It forces the
// template's check, lays out its labels, encodes every instruction from the table
// (asm_tables_rsp.cpp) and range-checks branches. Odin64's Microcode generator takes
// a body's words from it in the host build that runs the generator.
//
// The template must be a declared `@(rsp)` template: an RSP template is never called,
// so this is the one place it is used as a value.

gb_internal void check_queued_asm_template_body(Checker *c, Entity *e, UntypedExprInfoMap *untyped);

// Its constant, like #load's, is global data, not a temporary of the stack frame.
gb_internal bool is_rsp_template_code_call(Ast *call) {
	call = unparen_expr(call);
	if (call == nullptr || call->kind != Ast_CallExpr) {
		return false;
	}
	Ast *proc = unparen_expr(call->CallExpr.proc);
	Entity *b = entity_of_node(proc);
	return proc->tav.mode == Addressing_Builtin && b != nullptr && b->kind == Entity_Builtin &&
	       b->Builtin.id == BuiltinProc_rsp_template_code;
}

struct RspCodeLine {
	u32 word;
	i32 line;
};

// The register a GPR operand, memory base or vector operand names: a literal register,
// or a parameter through its pin. Returns false when there is none, reported.
gb_internal bool rsp_template_operand_register(Entity *tmpl, Ast *expr, u32 *number_, String *flag_) {
	expr = unparen_expr(expr);
	if (flag_) *flag_ = {};
	if (expr != nullptr && expr->kind == Ast_AsmRegister) {
		auto r = g_asm_rsp.register_lookup(expr->AsmRegister.name.string);
		if (r == Asm_rsp::REG_INVALID) {
			return false; // reported by the check
		}
		*number_ = g_asm_rsp.reg_number(r);
		if (flag_ && expr->AsmRegister.flag.kind == Token_Ident) {
			*flag_ = expr->AsmRegister.flag.string;
		}
		return true;
	}
	Entity *pe = entity_of_node(expr);
	if (pe != nullptr && pe->kind == Entity_Variable) {
		for (auto const &ed : tmpl->AsmTemplate.decls) {
			if (ed.entity != pe) {
				continue;
			}
			String pin = ed.pin;
			if (pin.len == 0 && ed.tie >= 0 && ed.tie < tmpl->AsmTemplate.decls.count) {
				pin = tmpl->AsmTemplate.decls[ed.tie].pin;
			}
			auto r = g_asm_rsp.register_lookup(pin);
			if (r == Asm_rsp::REG_INVALID) {
				return false; // an unpinned parameter, reported by the check
			}
			*number_ = g_asm_rsp.reg_number(r);
			return true;
		}
	}
	error(expr, "Expected a register or a pinned parameter in an @(rsp) template");
	return false;
}

gb_internal bool rsp_template_constant(Ast *expr, i64 *value_) {
	if (expr == nullptr || expr->tav.mode != Addressing_Constant) {
		return false;
	}
	ExactValue ev = exact_value_to_integer(expr->tav.value());
	if (ev.kind != ExactValue_Integer) {
		return false;
	}
	*value_ = exact_value_to_i64(ev);
	return true;
}

// Encodes the checked body of `e`. Returns false when it has errors, which the check
// has reported, or reports what the encoder itself rejects.
gb_internal bool rsp_template_encode(Entity *e, Array<RspCodeLine> *out) {
	Ast *node = e->AsmTemplate.node;
	GB_ASSERT(node != nullptr && node->kind == Ast_AsmTemplate);
	ast_node(at, AsmTemplate, node);

	// Lay out the labels: each one is the index of the next instruction.
	PtrMap<Ast *, i32> label_index = {};
	map_init(&label_index);
	defer (map_destroy(&label_index));
	i32 count = 0;
	bool ok = true;
	for (Ast *instruction : at->instructions) {
		switch (instruction->kind) {
		case Ast_AsmInstruction:
			count += 1;
			break;
		case Ast_AsmLabelDecl:
			map_set(&label_index, instruction, count);
			break;
		case Ast_AsmDirective:
			error(instruction, "Asm directives are not supported in an @(rsp) template");
			ok = false;
			break;
		}
	}
	if (!ok) {
		return false;
	}

	i32 pc = 0;
	for (Ast *instruction : at->instructions) {
		if (instruction->kind != Ast_AsmInstruction) {
			continue;
		}
		ast_node(instr, AsmInstruction, instruction);
		if (instr->mnemonic == 0) {
			return false; // the instruction matched no form: reported by the check
		}
		auto forms = g_asm_rsp.encoding_forms(instr->mnemonic);
		if (instr->valid_form_index < 0 || instr->valid_form_index >= forms.count) {
			return false;
		}
		auto const &form = forms[instr->valid_form_index];
		if (instr->operands.count != form.explicit_count()) {
			return false;
		}

		u32 word = form.bits;
		bool fits = true;
		for_array(i, instr->operands) {
			Ast *expr = instr->operands[i];
			int  slot = g_asm_rsp.form_explicit_slot(form, cast(int)i);
			GB_ASSERT(0 <= slot && slot < 4);
			auto type = form.ops[slot];
			auto enc  = form.enc[slot];

			u32 reg     = 0;
			u32 element = 0;
			i64 value   = 0;
			switch (type) {
			case Asm_rsp::OP_GPR:
				if (!rsp_template_operand_register(e, expr, &reg, nullptr)) {
					return false;
				}
				break;
			case Asm_rsp::OP_VR:
			case Asm_rsp::OP_VR_ELEM:
			case Asm_rsp::OP_VR_LS: {
				String flag = {};
				if (!rsp_template_operand_register(e, expr, &reg, &flag)) {
					return false;
				}
				i32 el = Asm_rsp::element_from_flag(type, flag);
				if (el < 0) {
					return false; // reported by the check
				}
				element = cast(u32)el;
				break;
			}
			case Asm_rsp::OP_SIMM16:
			case Asm_rsp::OP_UIMM16:
			case Asm_rsp::OP_SHAMT:
				if (!rsp_template_constant(expr, &value)) {
					error(expr, "Expected a constant immediate in an @(rsp) template");
					return false;
				}
				break;
			case Asm_rsp::OP_MEM:
			case Asm_rsp::OP_VMEM16: {
				Ast *m = unparen_expr(expr);
				if (m == nullptr || m->kind != Ast_AsmMemoryOperand) {
					return false;
				}
				AsmMemClassify const &cl = m->AsmMemoryOperand.classify;
				if (cl.base == nullptr || cl.index != nullptr || cl.label != nullptr) {
					error(expr, "An @(rsp) memory operand is a base register plus a constant displacement");
					return false;
				}
				if (!rsp_template_operand_register(e, cl.base, &reg, nullptr)) {
					return false;
				}
				value = cl.has_disp_const ? cl.disp_total : 0;
				if (type == Asm_rsp::OP_VMEM16) {
					i64 size = g_asm_rsp.memory_disp_scale(form, type);
					if (value % size != 0) {
						return false; // reported by the check
					}
					value /= size;
				}
				break;
			}
			case Asm_rsp::OP_REL16: {
				Ast *l = unparen_expr(expr);
				Entity *le = nullptr;
				if (l != nullptr && l->kind == Ast_AsmLabelDecl) {
					le = entity_of_node(l->AsmLabelDecl.name);
				}
				i32 *target = (le != nullptr && le->kind == Entity_Label) ? map_get(&label_index, le->Label.node) : nullptr;
				if (target == nullptr) {
					error(expr, "Expected a label of this template");
					return false;
				}
				value = cast(i64)*target - cast(i64)(pc + 1);
				if (value < -32768 || value > 32767) {
					error(expr, "'%.*s' branches %lld instructions, beyond the -32768..=32767 a branch reaches",
					      LIT(g_asm_rsp.mnemonic_strings[instr->mnemonic]), cast(long long)value);
					return false;
				}
				break;
			}
			default:
				GB_PANIC("unhandled RSP operand type %d", cast(int)type);
			}
			word |= Asm_rsp::encode_field(enc, reg, element, value, &fits);
		}
		if (!fits) {
			return false; // a value the check reported out of range
		}

		RspCodeLine line = {};
		line.word = word;
		line.line = cast(i32)ast_token(instruction).pos.line;
		array_add(out, line);
		pc += 1;
	}
	return true;
}

gb_internal bool check_rsp_template_code(CheckerContext *c, Operand *operand, Ast *call, String const &builtin_name) {
	ast_node(ce, CallExpr, call);
	Ast *arg = ce->args[0];

	Operand o = {};
	Ast *prev_hint = c->asm_template_hint;
	c->asm_template_hint = arg; // the one non-call position an asm template may be named in
	check_expr_or_type(c, &o, arg);
	c->asm_template_hint = prev_hint;
	if (o.mode == Addressing_Invalid) {
		return false;
	}

	Entity *e = entity_of_node(arg);
	if (e == nullptr || e->kind != Entity_AsmTemplate) {
		gbString s = expr_to_string(arg);
		error(arg, "'%.*s' expects a declared @(rsp) asm template, got '%s'", LIT(builtin_name), s);
		gb_string_free(s);
		return false;
	}
	if (e->AsmTemplate.arch != AsmTemplateArch_Rsp) {
		error(arg, "'%.*s' expects an @(rsp) asm template; '%.*s' is a CPU template", LIT(builtin_name), LIT(e->token.string));
		return false;
	}
	if (e->type == nullptr || e->type == t_invalid || e->decl_info == nullptr) {
		return false;
	}

	if ((e->flags & EntityFlag_ProcBodyChecked) == 0) {
		UntypedExprInfoMap untyped = {};
		map_init(&untyped);
		check_queued_asm_template_body(c->checker, e, &untyped);
		map_destroy(&untyped);
	}

	Array<RspCodeLine> lines = {};
	array_init(&lines, heap_allocator());
	defer (array_free(&lines));
	if (!rsp_template_encode(e, &lines)) {
		return false;
	}

	// The elements' bytes in the build target's byte order, as an `[]Rsp_Code_Line` holds them
	bool big_endian = target_endians[build_context.metrics.arch] == TargetEndian_Big;
	isize size = lines.count * gb_size_of(RspCodeLine);
	u8 *bytes = permanent_alloc_array<u8>(gb_max(size, 1));
	for_array(i, lines) {
		u32 fields[2] = {lines[i].word, cast(u32)lines[i].line};
		for (isize f = 0; f < 2; f++) {
			for (isize b = 0; b < 4; b++) {
				isize shift = big_endian ? 8*(3-b) : 8*b;
				bytes[i*8 + f*4 + b] = cast(u8)(fields[f] >> shift);
			}
		}
	}

	GB_ASSERT(t_rsp_code_line != nullptr);
	GB_ASSERT(type_size_of(t_rsp_code_line) == gb_size_of(RspCodeLine));
	operand->mode  = Addressing_Constant;
	operand->type  = alloc_type_slice(t_rsp_code_line);
	operand->value = exact_value_string(make_string(bytes, size));
	return true;
}
