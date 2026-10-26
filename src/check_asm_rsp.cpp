// check_asm_rsp.cpp: @(rsp) templates' body check, intrinsics.rsp_template_code and
// intrinsics.rsp_template_location
//
//     rsp_template_code     :: proc(t: <an @(rsp) asm template>) -> []Rsp_Code_Line
//     rsp_template_location :: proc(t: <an @(rsp) asm template>) -> runtime.Source_Code_Location
//     Rsp_Code_Line         :: struct {word: u32, line: i32}
//
// An @(rsp) template's check (check_rsp_template_body) is the CPU tables' check against
// the RSP table (asm_tables_rsp.cpp), plus what only the RSP needs: no type whose size
// is the build target's, and no layout measure (size_of, align_of, offset_of) of one.
// When that check reports nothing, it encodes the template, so every @(rsp) template is
// fully checked by `odin check` whether or not anything takes its words, and keeps the
// words for the builtin.
//
// rsp_template_code returns those words, one per instruction, each with the line of
// the template instruction it came from, as a constant built like `#load(path, []T)`:
// the value is the elements' bytes in the target's byte order. Odin64's Microcode
// generator takes a body's words from it in the host build that runs the generator.
// rsp_template_location is the template's declaration, for the generator's errors.
//
// The template must be a declared `@(rsp)` template: an RSP template is never called,
// so these are the places it is used as a value.

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

// What the checker knows of one @(rsp) template, guarded by rsp_template_body_mutex.
struct RspTemplateState {
	bool               in_progress; // its body check is running, further up this thread's stack
	bool               encoded;     // its check reported nothing, and `lines` holds its words
	Array<RspCodeLine> lines;
};

// Serialises @(rsp) templates' body checks between their queued checks and the
// builtins, so each runs once. Recursive: a template's operand may itself take another
// template's words.
gb_global RecursiveMutex                         rsp_template_body_mutex;
gb_global PtrMap<Entity *, RspTemplateState *>   rsp_template_states;
gb_global bool                                   rsp_template_states_init;

gb_internal RspTemplateState *rsp_template_state(Entity *e) {
	if (!rsp_template_states_init) {
		map_init(&rsp_template_states);
		rsp_template_states_init = true;
	}
	RspTemplateState **found = map_get(&rsp_template_states, e);
	if (found != nullptr) {
		return *found;
	}
	RspTemplateState *st = gb_alloc_item(permanent_allocator(), RspTemplateState);
	map_set(&rsp_template_states, e, st);
	return st;
}

// int, uint and uintptr (and distinct or enum types of them): integers whose size is
// the build target's. An @(rsp) template's registers are 32 bits on every target.
gb_internal bool rsp_is_target_sized_integer(Type *type) {
	Type *bt = base_type(type);
	if (bt != nullptr && bt->kind == Type_Enum) {
		bt = base_type(bt->Enum.base_type);
	}
	return bt != nullptr && bt->kind == Type_Basic && is_type_integer(bt) && bt->Basic.size < 0;
}

// The type inside `type` that makes its layout the build target's, or nullptr when its
// size, alignment and field offsets are the same for every target. That is a type of
// target size (int, uint, uintptr, pointers, string, any, typeid, slices, maps, procs),
// or one whose alignment is capped by the target's max_align (128-bit integers, #simd,
// matrix), found through fields, elements and backing types.
gb_internal Type *rsp_target_layout_culprit(Type *type) {
	Type *t = base_type(type);
	if (t == nullptr) {
		return nullptr;
	}
	switch (t->kind) {
	case Type_Basic:
		if (t->Basic.size < 0) {
			return type;
		}
		if (is_type_integer(t) && t->Basic.size > 8) {
			return type;
		}
		return nullptr;
	case Type_Pointer:
	case Type_MultiPointer:
	case Type_SoaPointer:
	case Type_Proc:
	case Type_Slice:
	case Type_DynamicArray:
	case Type_FixedCapacityDynamicArray:
	case Type_Map:
	case Type_SimdVector:
	case Type_Matrix:
		return type;
	case Type_Array:
		return rsp_target_layout_culprit(t->Array.elem);
	case Type_EnumeratedArray:
		return rsp_target_layout_culprit(t->EnumeratedArray.elem);
	case Type_Enum:
		return rsp_target_layout_culprit(t->Enum.base_type);
	case Type_BitSet:
		return t->BitSet.underlying != nullptr ? rsp_target_layout_culprit(t->BitSet.underlying) : nullptr;
	case Type_BitField:
		return rsp_target_layout_culprit(t->BitField.backing_type);
	case Type_Struct:
		for (Entity *f : t->Struct.fields) {
			if (Type *c = rsp_target_layout_culprit(f->type)) {
				return c;
			}
		}
		return nullptr;
	case Type_Union:
		for (Type *v : t->Union.variants) {
			if (Type *c = rsp_target_layout_culprit(v)) {
				return c;
			}
		}
		return nullptr;
	case Type_Tuple:
		for (Entity *v : t->Tuple.variables) {
			if (Type *c = rsp_target_layout_culprit(v->type)) {
				return c;
			}
		}
		return nullptr;
	}
	return nullptr;
}

// Every parameter, output and named scratch is a 32-bit RSP register (or a vector
// register), so a type whose size is the host's means something else in each build.
gb_internal void rsp_check_parameter_types(Entity *e) {
	for (auto const &ed : e->AsmTemplate.decls) {
		Entity *pe = ed.entity;
		if (pe == nullptr || pe->type == nullptr || is_type_internally_pointer_like(pe->type) ||
		    !rsp_is_target_sized_integer(pe->type)) {
			continue;
		}
		gbString s = type_to_string(pe->type);
		error(pe->token, "'%.*s' has type '%s', whose size is the build target's; an @(rsp) template's registers "
		                 "are 32 bits whatever the build targets, so use i32 or u32",
		      LIT(pe->token.string), s);
		gb_string_free(s);
	}
}

// The type a size_of, align_of, offset_of or offset_of_by_string call measures, or
// nullptr when `call` is none of them.
gb_internal Type *rsp_measured_type(Ast *call) {
	ast_node(ce, CallExpr, call);
	Entity *b = entity_of_node(unparen_expr(ce->proc));
	if (b == nullptr || b->kind != Entity_Builtin || ce->args.count == 0) {
		return nullptr;
	}
	switch (b->Builtin.id) {
	case BuiltinProc_size_of:
	case BuiltinProc_align_of:
	case BuiltinProc_offset_of_by_string:
		return type_of_expr(ce->args[0]);
	case BuiltinProc_offset_of: {
		if (ce->args.count >= 2) {
			return type_of_expr(ce->args[0]);
		}
		Ast *sel = unparen_expr(ce->args[0]);
		if (sel != nullptr && sel->kind == Ast_SelectorExpr) {
			Type *t = type_of_expr(sel->SelectorExpr.expr);
			if (t != nullptr && is_type_pointer(t)) {
				t = type_deref(t);
			}
			return t;
		}
		return type_of_expr(ce->args[0]);
	}
	}
	return nullptr;
}

// Reports each size_of, align_of or offset_of in `node` (and in the constants it
// names, through their declarations) that measures a type whose layout is the build
// target's: the value is the host's in a host build and the N64's in an n64 build, so
// the template's words would depend on which one encodes them.
gb_internal void rsp_check_layout_measures(Ast *node, Ast *report_at, PtrSet<Entity *> *visited) {
	node = unparen_expr(node);
	if (node == nullptr) {
		return;
	}
	switch (node->kind) {
	case Ast_Ident:
	case Ast_SelectorExpr: {
		if (node->kind == Ast_SelectorExpr) {
			rsp_check_layout_measures(node->SelectorExpr.expr, report_at, visited);
		}
		Entity *ce = entity_of_node(node);
		if (ce != nullptr && ce->kind == Entity_Constant && ce->decl_info != nullptr &&
		    ce->decl_info->init_expr != nullptr && !ptr_set_update(visited, ce)) {
			// a measure in a constant's declaration is reported where the template uses it
			rsp_check_layout_measures(ce->decl_info->init_expr, report_at != nullptr ? report_at : node, visited);
		}
		return;
	}
	case Ast_UnaryExpr:
		rsp_check_layout_measures(node->UnaryExpr.expr, report_at, visited);
		return;
	case Ast_BinaryExpr:
		rsp_check_layout_measures(node->BinaryExpr.left,  report_at, visited);
		rsp_check_layout_measures(node->BinaryExpr.right, report_at, visited);
		return;
	case Ast_TernaryIfExpr:
		rsp_check_layout_measures(node->TernaryIfExpr.cond, report_at, visited);
		rsp_check_layout_measures(node->TernaryIfExpr.x,    report_at, visited);
		rsp_check_layout_measures(node->TernaryIfExpr.y,    report_at, visited);
		return;
	case Ast_TernaryWhenExpr:
		rsp_check_layout_measures(node->TernaryWhenExpr.cond, report_at, visited);
		rsp_check_layout_measures(node->TernaryWhenExpr.x,    report_at, visited);
		rsp_check_layout_measures(node->TernaryWhenExpr.y,    report_at, visited);
		return;
	case Ast_TypeCast:
		rsp_check_layout_measures(node->TypeCast.expr, report_at, visited);
		return;
	case Ast_AutoCast:
		rsp_check_layout_measures(node->AutoCast.expr, report_at, visited);
		return;
	case Ast_IndexExpr:
		rsp_check_layout_measures(node->IndexExpr.expr,  report_at, visited);
		rsp_check_layout_measures(node->IndexExpr.index, report_at, visited);
		return;
	case Ast_AsmMemoryOperand:
		for (Ast *term : node->AsmMemoryOperand.terms) {
			rsp_check_layout_measures(term, report_at, visited);
		}
		return;
	case Ast_AsmMemoryTerm:
		rsp_check_layout_measures(node->AsmMemoryTerm.operand, report_at, visited);
		rsp_check_layout_measures(node->AsmMemoryTerm.scale,   report_at, visited);
		return;
	case Ast_CallExpr: {
		if (Type *measured = rsp_measured_type(node)) {
			if (Type *culprit = rsp_target_layout_culprit(measured)) {
				gbString call = expr_to_string(node);
				gbString m    = type_to_string(measured);
				gbString c    = type_to_string(culprit);
				Ast *at = report_at != nullptr ? report_at : node;
				if (are_types_identical(measured, culprit)) {
					error(at, "'%s' measures '%s', whose layout is the build target's, so the @(rsp) template's words "
					          "would differ between a host and an n64 build; measure a type of fixed-size fields "
					          "(i32 or u32 for a DMEM address)", call, m);
				} else {
					error(at, "'%s' measures '%s', whose layout is the build target's because it holds '%s', so the "
					          "@(rsp) template's words would differ between a host and an n64 build; measure a type of "
					          "fixed-size fields (i32 or u32 for a DMEM address)", call, m, c);
				}
				gb_string_free(c);
				gb_string_free(m);
				gb_string_free(call);
			}
			return;
		}
		rsp_check_layout_measures(node->CallExpr.proc, report_at, visited);
		for (Ast *arg : node->CallExpr.args) {
			rsp_check_layout_measures(arg, report_at, visited);
		}
		return;
	}
	case Ast_FieldValue:
		rsp_check_layout_measures(node->FieldValue.value, report_at, visited);
		return;
	}
}

gb_internal void rsp_check_template_layout_measures(Entity *e) {
	ast_node(at, AsmTemplate, e->AsmTemplate.node);
	PtrSet<Entity *> visited = {};
	ptr_set_init(&visited);
	defer (ptr_set_destroy(&visited));
	for (Ast *instruction : at->instructions) {
		if (instruction->kind != Ast_AsmInstruction) {
			continue;
		}
		for (Ast *operand : instruction->AsmInstruction.operands) {
			rsp_check_layout_measures(operand, nullptr, &visited);
		}
	}
}

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

// `#unroll for` in an @(rsp) body, expanded before the body's check
//
//     #unroll for lane in 0..<4 { sw %t4, [scratch + (16 + 4*lane)] }  // a constant range
//     #unroll for off in OFFSETS { sw %t0, [p + off] }                 // a constant array
//     #unroll for r, i in {%v10..=%v14} { lqv %r.e0, [scratch + (16*i)] } // a register group
//
// Each iteration is a copy of the body. An integer loop variable becomes a constant
// literal wherever it is named: an immediate, a memory term, inside parentheses. A
// register loop variable is spelled as a register, `%r` (with any accessor, `%r.e0`) or
// bare `r`, and stands for the iteration's register. A register range steps the number
// at the end of the name and keeps its digit count (`%v08..=%v11`, `%t0..<%t4`). The
// second name, when given, is the iteration's index. The copies keep the body's tokens,
// so an error inside an unrolled body is reported at the body's line (once, for the
// first iteration that has it) and each word's `line` is the body line it came from.
// A label can't be declared inside an unrolled body: each copy would declare it again.

struct RspUnrollBinding {
	String name;
	String reg;   // a register loop variable's register, without '%'
	i64    value; // an integer loop variable's value
};

gb_internal RspUnrollBinding const *rsp_unroll_find(Array<RspUnrollBinding> const &bindings, String const &name) {
	for (isize i = bindings.count-1; i >= 0; i--) {
		if (bindings[i].name == name) {
			return &bindings[i];
		}
	}
	return nullptr;
}

gb_internal Ast *rsp_unroll_literal(Ast *at, i64 value) {
	Token token = ast_token(at);
	token.kind = Token_Integer;
	char buf[32] = {};
	snprintf(buf, gb_size_of(buf), "%lld", cast(long long)value);
	token.string = copy_string(permanent_allocator(), make_string_c(buf));
	Ast *lit = alloc_ast_node(at->file(), Ast_BasicLit);
	lit->BasicLit.token = token;
	lit->tav.mode = Addressing_Constant;
	lit->tav.set_value(exact_value_i64(value));
	return lit;
}

// Replaces the loop variables in a copy of the body. `is_operand`: `*slot` is an
// instruction operand or memory term, where a register variable may stand bare.
gb_internal void rsp_unroll_substitute(Ast **slot, Array<RspUnrollBinding> const &b, bool is_operand) {
	Ast *node = *slot;
	if (node == nullptr) {
		return;
	}
	switch (node->kind) {
	case Ast_Ident: {
		RspUnrollBinding const *found = rsp_unroll_find(b, node->Ident.token.string);
		if (found == nullptr) {
			break;
		}
		if (found->reg.len == 0) {
			*slot = rsp_unroll_literal(node, found->value);
		} else if (is_operand) {
			Ast *reg = alloc_ast_node(node->file(), Ast_AsmRegister);
			reg->AsmRegister.token = node->Ident.token;
			reg->AsmRegister.token.kind = Token_Mod;
			reg->AsmRegister.name = node->Ident.token;
			reg->AsmRegister.name.string = found->reg;
			*slot = reg;
		}
		break;
	}
	case Ast_AsmRegister: {
		RspUnrollBinding const *found = rsp_unroll_find(b, node->AsmRegister.name.string);
		if (found != nullptr && found->reg.len != 0) {
			node->AsmRegister.name.string = found->reg;
		}
		break;
	}
	case Ast_AsmInstruction:
		for (Ast *&op : node->AsmInstruction.operands) rsp_unroll_substitute(&op, b, true);
		break;
	case Ast_AsmDirective:
		for (Ast *&op : node->AsmDirective.operands) rsp_unroll_substitute(&op, b, true);
		break;
	case Ast_AsmMemoryOperand:
		rsp_unroll_substitute(&node->AsmMemoryOperand.segment_override, b, true);
		for (Ast *&term : node->AsmMemoryOperand.terms) rsp_unroll_substitute(&term, b, true);
		break;
	case Ast_AsmMemoryTerm:
		rsp_unroll_substitute(&node->AsmMemoryTerm.operand, b, true);
		rsp_unroll_substitute(&node->AsmMemoryTerm.scale, b, false);
		break;
	case Ast_AsmRegisterGroup:
		for (Ast *&r : node->AsmRegisterGroup.registers) rsp_unroll_substitute(&r, b, true);
		break;
	case Ast_UnaryExpr:
		rsp_unroll_substitute(&node->UnaryExpr.expr, b, false);
		break;
	case Ast_BinaryExpr:
		rsp_unroll_substitute(&node->BinaryExpr.left,  b, false);
		rsp_unroll_substitute(&node->BinaryExpr.right, b, false);
		break;
	case Ast_ParenExpr:
		rsp_unroll_substitute(&node->ParenExpr.expr, b, false);
		break;
	case Ast_SelectorExpr:
		rsp_unroll_substitute(&node->SelectorExpr.expr, b, false);
		break;
	case Ast_IndexExpr:
		rsp_unroll_substitute(&node->IndexExpr.expr,  b, false);
		rsp_unroll_substitute(&node->IndexExpr.index, b, false);
		break;
	case Ast_TernaryIfExpr:
		rsp_unroll_substitute(&node->TernaryIfExpr.x,    b, false);
		rsp_unroll_substitute(&node->TernaryIfExpr.cond, b, false);
		rsp_unroll_substitute(&node->TernaryIfExpr.y,    b, false);
		break;
	case Ast_TernaryWhenExpr:
		rsp_unroll_substitute(&node->TernaryWhenExpr.x,    b, false);
		rsp_unroll_substitute(&node->TernaryWhenExpr.cond, b, false);
		rsp_unroll_substitute(&node->TernaryWhenExpr.y,    b, false);
		break;
	case Ast_CallExpr: {
		// `offset_of(T, field)` names a field, not a value
		Ast *proc = unparen_expr(node->CallExpr.proc);
		bool names_field = proc != nullptr && proc->kind == Ast_Ident &&
		                   (proc->Ident.token.string == "offset_of" || proc->Ident.token.string == "offset_of_by_string");
		for_array(i, node->CallExpr.args) {
			if (!(names_field && i > 0)) {
				rsp_unroll_substitute(&node->CallExpr.args[i], b, false);
			}
		}
		break;
	}
	}
}

struct RspUnrollValue {
	String reg;   // a register group's register
	i64    value; // otherwise, the integer
};

// "v10" -> ("v", 10, 2 digits). False when the name has no number at its end.
gb_internal bool rsp_unroll_split_register(String const &name, String *prefix_, i64 *number_, isize *digits_) {
	isize n = name.len;
	while (n > 0 && gb_char_is_digit(cast(char)name[n-1])) {
		n -= 1;
	}
	if (n == name.len || name.len - n > 4) {
		return false;
	}
	*prefix_ = substring(name, 0, n);
	*digits_ = name.len - n;
	i64 number = 0;
	for (isize i = n; i < name.len; i++) {
		number = number*10 + (name[i] - '0');
	}
	*number_ = number;
	return true;
}

gb_internal bool rsp_unroll_group_values(Ast *group, Array<RspUnrollValue> *values) {
	ast_node(rg, AsmRegisterGroup, group);
	for (Ast *r : rg->registers) {
		if (r == nullptr || r->kind != Ast_AsmRegister || r->AsmRegister.flag.kind == Token_Ident) {
			error(r ? r : group, "An '#unroll for' register group holds registers without accessors, such as {%%v10..=%%v14} or {%%t0, %%t2}");
			return false;
		}
	}
	if (rg->range_token.kind == Token_RangeHalf || rg->range_token.kind == Token_RangeFull) {
		String lo_name = rg->registers[0]->AsmRegister.name.string;
		String hi_name = rg->registers[1]->AsmRegister.name.string;
		String lo_prefix = {}, hi_prefix = {};
		i64 lo = 0, hi = 0;
		isize lo_digits = 0, hi_digits = 0;
		if (!rsp_unroll_split_register(lo_name, &lo_prefix, &lo, &lo_digits) ||
		    !rsp_unroll_split_register(hi_name, &hi_prefix, &hi, &hi_digits) ||
		    lo_prefix != hi_prefix) {
			error(group, "An '#unroll for' register range steps the number at the end of one register name, such as {%%v10..=%%v14}; got '%%%.*s' and '%%%.*s'",
			      LIT(lo_name), LIT(hi_name));
			return false;
		}
		if (rg->range_token.kind == Token_RangeHalf) {
			hi -= 1;
		}
		for (i64 n = lo; n <= hi; n++) {
			char buf[64] = {};
			snprintf(buf, gb_size_of(buf), "%.*s%0*lld", LIT(lo_prefix), cast(int)lo_digits, cast(long long)n);
			RspUnrollValue v = {copy_string(permanent_allocator(), make_string_c(buf)), 0};
			array_add(values, v);
		}
	} else {
		for (Ast *r : rg->registers) {
			RspUnrollValue v = {r->AsmRegister.name.string, 0};
			array_add(values, v);
		}
	}
	for (RspUnrollValue const &v : *values) {
		if (g_asm_rsp.register_lookup(v.reg) == Asm_rsp::REG_INVALID) {
			error(group, "'%%%.*s' in this '#unroll for' register group is not an RSP register", LIT(v.reg));
			return false;
		}
	}
	return true;
}

// A parameter's value is a register's at run time, not a constant.
gb_internal bool rsp_unroll_names_parameter(Entity *e, Ast *expr) {
	Ast *x = unparen_expr(expr);
	if (x == nullptr || x->kind != Ast_Ident ||
	    scope_lookup_current(e->AsmTemplate.param_scope, x->Ident.interned, x->Ident.hash) == nullptr) {
		return false;
	}
	error(x, "'%.*s' is a parameter of the template, a register at run time; an '#unroll for' iterates over constants",
	      LIT(x->Ident.token.string));
	return true;
}

gb_internal bool rsp_unroll_constant_integer(CheckerContext *ctx, Entity *e, Ast *expr, i64 *value_) {
	if (rsp_unroll_names_parameter(e, expr)) {
		return false;
	}
	Operand o = {};
	check_expr(ctx, &o, expr);
	if (o.mode == Addressing_Invalid) {
		return false;
	}
	if (o.mode != Addressing_Constant || !is_type_integer(o.type)) {
		gbString s = expr_to_string(expr);
		error(expr, "An '#unroll for' range is of constant integers, got '%s'", s);
		gb_string_free(s);
		return false;
	}
	*value_ = exact_value_to_i64(exact_value_to_integer(o.value));
	return true;
}

// The values an `#unroll for` takes, in order: a range's integers, a constant array's
// elements (integers), or a register group's registers.
gb_internal bool rsp_unroll_values(CheckerContext *ctx, Entity *e, Ast *expr, Array<RspUnrollValue> *values) {
	if (expr->kind == Ast_AsmRegisterGroup) {
		return rsp_unroll_group_values(expr, values);
	}
	Ast *range = unparen_expr(expr);
	if (is_ast_range(range)) {
		ast_node(be, BinaryExpr, range);
		i64 lo = 0, hi = 0;
		if (!rsp_unroll_constant_integer(ctx, e, be->left, &lo) || !rsp_unroll_constant_integer(ctx, e, be->right, &hi)) {
			return false;
		}
		if (be->op.kind == Token_RangeHalf) {
			hi -= 1;
		}
		for (i64 n = lo; n <= hi && values->count <= 1024; n++) {
			RspUnrollValue v = {{}, n};
			array_add(values, v);
		}
		return true;
	}

	if (rsp_unroll_names_parameter(e, expr)) {
		return false;
	}
	Operand o = {};
	check_expr(ctx, &o, expr);
	if (o.mode == Addressing_Invalid) {
		return false;
	}
	Type *t = base_type(o.type);
	Type *elem = t->kind == Type_Array ? t->Array.elem : t->kind == Type_Slice ? t->Slice.elem : nullptr;
	if (o.mode != Addressing_Constant || o.value.kind != ExactValue_Compound || elem == nullptr || !is_type_integer(elem) ||
	    o.value.value_compound == nullptr || o.value.value_compound->kind != Ast_CompoundLit) {
		gbString s = expr_to_string(expr);
		gbString ts = type_to_string(o.type);
		error(expr, "An '#unroll for' in an @(rsp) template iterates over a constant range, a constant array of integers or a register group, got '%s' of type '%s'", s, ts);
		gb_string_free(ts);
		gb_string_free(s);
		return false;
	}
	ast_node(cl, CompoundLit, o.value.value_compound);
	i64 count = t->kind == Type_Array ? t->Array.count : cl->elems.count;
	if (t->kind == Type_Slice && cl->elems.count > 0 && cl->elems[0]->kind == Ast_FieldValue) {
		error(expr, "An '#unroll for' over a constant slice takes positional elements");
		return false;
	}
	for (i64 k = 0; k < count && values->count <= 1024; k++) {
		Ast *e = constant_compound_elem_node(cl, k);
		RspUnrollValue v = {};
		if (e != nullptr) { // an element left out of an array literal is zero
			ExactValue ev = exact_value_to_integer(e->tav.value());
			if (e->tav.mode != Addressing_Constant || ev.kind != ExactValue_Integer) {
				error(e, "An '#unroll for' element must be a constant integer");
				return false;
			}
			v.value = exact_value_to_i64(ev);
		}
		array_add(values, v);
	}
	return true;
}

gb_internal bool rsp_unroll_bind(Entity *e, Array<RspUnrollBinding> *b, Ast *name) {
	if (name == nullptr || is_blank_ident(name)) {
		return false;
	}
	String s = name->Ident.token.string;
	char const *why = nullptr;
	if (scope_lookup_current(e->AsmTemplate.param_scope, name->Ident.interned, name->Ident.hash) != nullptr) {
		why = "a parameter of the template";
	} else if (rsp_unroll_find(*b, s) != nullptr) {
		why = "the variable of an enclosing '#unroll for'";
	} else if (g_asm_rsp.register_lookup(s) != Asm_rsp::REG_INVALID) {
		why = "an RSP register's name";
	}
	if (why != nullptr) {
		error(name, "'#unroll for' variable '%.*s' is %s; choose another name", LIT(s), why);
		return false;
	}
	RspUnrollBinding binding = {s};
	array_add(b, binding);
	return true;
}

gb_internal void rsp_unroll_expand(CheckerContext *ctx, Entity *e, Slice<Ast *> const &body,
                                   Array<RspUnrollBinding> const &bindings, Array<Ast *> *out) {
	for (Ast *node : body) {
		if (node->kind != Ast_AsmUnroll) {
			if (bindings.count == 0) {
				array_add(out, node);
			} else {
				Ast *copy = clone_ast(node);
				rsp_unroll_substitute(&copy, bindings, false);
				array_add(out, copy);
			}
			continue;
		}

		ast_node(u, AsmUnroll, node);
		bool ok = true;
		for (Ast *inner : u->instructions) {
			if (inner->kind == Ast_AsmLabelDecl) {
				error(inner, "An asm label can't be declared inside an '#unroll for' body: each iteration would declare it again");
				ok = false;
			}
		}
		Ast *expr = clone_ast(u->expr);
		rsp_unroll_substitute(&expr, bindings, false);
		Array<RspUnrollValue> values = {};
		array_init(&values, heap_allocator());
		defer (array_free(&values));
		ok = rsp_unroll_values(ctx, e, expr, &values) && ok;
		if (ok && values.count > 1024) {
			error(u->expr, "Too many iterations for an '#unroll for' in an @(rsp) template, expected <= 1024");
			ok = false;
		}

		Array<RspUnrollBinding> inner = {};
		array_init(&inner, heap_allocator(), 0, bindings.count + 2);
		defer (array_free(&inner));
		array_add_elems(&inner, bindings.data, bindings.count);
		bool has_val0 = rsp_unroll_bind(e, &inner, u->val0);
		bool has_val1 = rsp_unroll_bind(e, &inner, u->val1);
		ok = ok && (has_val0 || u->val0 == nullptr || is_blank_ident(u->val0))
		        && (has_val1 || u->val1 == nullptr || is_blank_ident(u->val1));
		if (!ok) {
			continue;
		}
		for_array(k, values) {
			isize at = bindings.count;
			if (has_val0) {
				inner[at].reg   = values[k].reg;
				inner[at].value = values[k].value;
				at += 1;
			}
			if (has_val1) {
				inner[at].value = cast(i64)k; // the index
			}
			rsp_unroll_expand(ctx, e, u->instructions, inner, out);
		}
	}
}

// Replaces each `#unroll for` in the template's body with its iterations.
gb_internal void rsp_expand_unrolls(CheckerContext *ctx, Entity *e) {
	ast_node(at, AsmTemplate, e->AsmTemplate.node);
	bool any = false;
	for (Ast *node : at->instructions) {
		any |= node->kind == Ast_AsmUnroll;
	}
	if (!any) {
		return;
	}
	Array<Ast *> out = {};
	array_init(&out, heap_allocator());
	Array<RspUnrollBinding> none = {};
	rsp_unroll_expand(ctx, e, at->instructions, none, &out);
	at->instructions = slice_from_array(out);
}

// The check of an @(rsp) template's body, from its queued check or from a builtin
// that needs its words (check_asm_template_body_from_entity). It runs once.
gb_internal void check_rsp_template_body(CheckerContext *ctx, Entity *e, DeclInfo *d) {
	MUTEX_GUARD(&rsp_template_body_mutex);
	if (e->flags & EntityFlag_ProcBodyChecked) {
		return;
	}
	RspTemplateState *st = rsp_template_state(e);
	if (st->in_progress) {
		return; // re-entered through a builtin in its own body, which reports it
	}
	st->in_progress = true;

	// The check is the template's, not the expression that asked for its words: a
	// builtin checked in a muted trial (a procedure group's candidates) must not mute it,
	// because it runs once.
	i32 mute_depth = global_error_context.mute_depth;
	global_error_context.mute_depth = 0;
	i64 errors_before = thread_error_count();
	{
		AsmCheckTargetScope target(asm_hook_table_arch(&g_asm_rsp, 0), asm_hook_table_pointer_bits(&g_asm_rsp, 0));
		rsp_expand_unrolls(ctx, e);
		check_asm_template_body(&g_asm_rsp, ctx, e, d);
		rsp_check_parameter_types(e);
		rsp_check_template_layout_measures(e);
	}
	// Encode only a template whose check reported nothing (including any template its
	// operands took words from), so the encoder never repeats one of the check's errors.
	if (thread_error_count() == errors_before) {
		array_init(&st->lines, heap_allocator());
		if (rsp_template_encode(e, &st->lines)) {
			st->encoded = true;
		} else if (thread_error_count() == errors_before) {
			error(e->token, "'%.*s' could not be encoded for the RSP", LIT(e->token.string));
		}
	}
	global_error_context.mute_depth = mute_depth;

	st->in_progress = false;
	add_deps_from_child_to_parent(d);
	e->flags |= EntityFlag_ProcBodyChecked;
}

// The @(rsp) template a builtin's argument names, or nullptr, reported.
gb_internal Entity *rsp_template_argument(CheckerContext *c, Ast *call, String const &builtin_name) {
	ast_node(ce, CallExpr, call);
	Ast *arg = ce->args[0];

	Operand o = {};
	Ast *prev_hint = c->asm_template_hint;
	c->asm_template_hint = arg; // the one non-call position an asm template may be named in
	check_expr_or_type(c, &o, arg);
	c->asm_template_hint = prev_hint;
	if (o.mode == Addressing_Invalid) {
		return nullptr;
	}

	Entity *e = entity_of_node(arg);
	if (e == nullptr || e->kind != Entity_AsmTemplate) {
		gbString s = expr_to_string(arg);
		error(arg, "'%.*s' expects a declared @(rsp) asm template, got '%s'", LIT(builtin_name), s);
		gb_string_free(s);
		return nullptr;
	}
	if (e->AsmTemplate.arch != AsmTemplateArch_Rsp) {
		error(arg, "'%.*s' expects an @(rsp) asm template; '%.*s' is a CPU template", LIT(builtin_name), LIT(e->token.string));
		return nullptr;
	}
	if (e->type == nullptr || e->type == t_invalid || e->decl_info == nullptr) {
		return nullptr;
	}
	return e;
}

gb_internal bool check_rsp_template_code(CheckerContext *c, Operand *operand, Ast *call, String const &builtin_name) {
	GB_ASSERT(t_rsp_code_line != nullptr);
	GB_ASSERT(type_size_of(t_rsp_code_line) == gb_size_of(RspCodeLine));

	// Without words (an error has been reported), the value is still an
	// `[]Rsp_Code_Line` constant, an empty one, so nothing that uses it adds errors.
	operand->mode  = Addressing_Constant;
	operand->type  = alloc_type_slice(t_rsp_code_line);
	operand->value = exact_value_string(make_string(permanent_alloc_array<u8>(1), 0));

	Entity *e = rsp_template_argument(c, call, builtin_name);
	if (e == nullptr) {
		return true;
	}

	MUTEX_GUARD(&rsp_template_body_mutex);
	RspTemplateState *st = rsp_template_state(e);
	if (st->in_progress) {
		gbString s = expr_to_string(call);
		error(call, "'%s' refers to the template being checked; an @(rsp) template's words cannot depend on its own", s);
		gb_string_free(s);
		return true;
	}
	if ((e->flags & EntityFlag_ProcBodyChecked) == 0) {
		UntypedExprInfoMap untyped = {};
		map_init(&untyped);
		check_queued_asm_template_body(c->checker, e, &untyped);
		map_destroy(&untyped);
	}
	if (!st->encoded) {
		return true; // its check reported why
	}

	// The elements' bytes in the build target's byte order, as an `[]Rsp_Code_Line` holds them
	auto const &lines = st->lines;
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
	operand->value = exact_value_string(make_string(bytes, size));
	return true;
}

// The template's declaration as a runtime.Source_Code_Location: its file, the line and
// column of its name, and its name as the procedure. A value, like #location; the LLVM
// backend builds it (lb_build_builtin_proc). It does not need the body checked.
gb_internal bool check_rsp_template_location(CheckerContext *c, Operand *operand, Ast *call, String const &builtin_name) {
	Entity *e = rsp_template_argument(c, call, builtin_name);
	if (e == nullptr) {
		return false;
	}
	init_core_source_code_location(c->checker);
	operand->mode = Addressing_Value;
	operand->type = t_source_code_location;
	return true;
}
