package test_core_odin_parser

import "base:runtime"

import "core:fmt"
import "core:log"
import "core:odin/ast"
import "core:odin/parser"
import "core:odin/tokenizer"
import "core:os"
import "core:strings"
import "core:testing"

// Asm template bodies as the compiler's parser reads them: `#unroll for` (fork PR #39),
// register groups and lists, element accessors and RSP register names.

RSP_TEMPLATES :: ODIN_ROOT + "tests/rsp_templates"

@test
test_parse_rsp_templates :: proc(t: ^testing.T) {
	// Every package under tests/rsp_templates parses: the rejections in bad/ are
	// the checker's, not syntax errors.
	templates, unrolls: int

	w := os.walker_create(RSP_TEMPLATES)
	defer os.walker_destroy(&w)

	for info in os.walker_walk(&w) {
		if info.type != .Directory {
			continue
		}

		context.allocator = context.temp_allocator
		runtime.DEFAULT_TEMP_ALLOCATOR_TEMP_GUARD()

		pkg, ok := parser.collect_package(info.fullpath)
		if !testing.expectf(t, ok, "%v could not be read", info.fullpath) {
			continue
		}

		p := parser.default_parser()
		for path, file in pkg.files {
			parser.parse_file(&p, file)
			testing.expectf(t, file.syntax_error_count == 0, "%v should contain zero errors", path)

			counts: [2]int
			v := ast.Visitor{visit = count_asm_visit, data = &counts}
			for decl in file.decls {
				ast.walk(&v, decl)
			}
			templates += counts[0]
			unrolls   += counts[1]
		}
	}

	path, err := os.walker_error(&w)
	testing.expectf(t, err == nil, "failed walking %v: %v", path, err)

	testing.expect(t, templates > 0, "tests/rsp_templates should hold asm templates")
	testing.expect(t, unrolls > 0, "tests/rsp_templates should hold '#unroll for's")
}

count_asm_visit :: proc(v: ^ast.Visitor, node: ^ast.Node) -> ^ast.Visitor {
	if node == nil {
		return nil
	}
	counts := (^[2]int)(v.data)
	#partial switch _ in node.derived {
	case ^ast.Asm_Template: counts[0] += 1
	case ^ast.Asm_Unroll:   counts[1] += 1
	}
	return v
}

@test
test_parse_asm_unroll_round_trip :: proc(t: ^testing.T) {
	// transform_unrolled's body printed back from the AST, and from a clone of it,
	// has the source's tokens.
	context.allocator = context.temp_allocator
	runtime.DEFAULT_TEMP_ALLOCATOR_TEMP_GUARD()

	pkg, ok := parser.collect_package(RSP_TEMPLATES + "/valid/transform")
	if !testing.expect(t, ok, "valid/transform could not be read") {
		return
	}
	p := parser.default_parser()
	template: ^ast.Asm_Template
	src: string
	for _, file in pkg.files {
		parser.parse_file(&p, file)
		testing.expectf(t, file.syntax_error_count == 0, "%v should contain zero errors", file.fullpath)
		if template == nil {
			template = find_template(file, "transform_unrolled")
			src = file.src
		}
	}
	if !testing.expect(t, template != nil, "transform_unrolled should be parsed") {
		return
	}

	// three loops at the top, the first with two loops inside
	top: [dynamic]^ast.Asm_Unroll
	for s in template.instructions {
		if u, is_unroll := s.derived.(^ast.Asm_Unroll); is_unroll {
			append(&top, u)
		}
	}
	if testing.expect_value(t, len(top), 3) {
		nested := 0
		for s in top[0].instructions {
			if _, is_unroll := s.derived.(^ast.Asm_Unroll); is_unroll {
				nested += 1
			}
		}
		testing.expect_value(t, nested, 2)
		testing.expect_value(t, ident_name(top[0].val0), "first")
		testing.expect_value(t, ident_name(top[0].val1), "half")

		group, is_group := top[2].expr.derived.(^ast.Asm_Register_Group)
		if testing.expect(t, is_group, "the last loop iterates a register group") {
			testing.expect_value(t, group.range_token.kind, tokenizer.Token_Kind.Range_Full)
			testing.expect_value(t, len(group.registers), 2)
		}
		testing.expect_value(t, ident_name(top[2].val1), "i")
	}

	first := template.instructions[0]
	body  := src[first.pos.offset:template.end.offset-1] // up to the template's `}`

	printed := print_asm_body(src, template.instructions)
	expect_same_tokens(t, printed, body)

	cloned := ast.clone(template.instructions)
	expect_same_tokens(t, print_asm_body(src, cloned), body)

}

@test
test_parse_asm_unroll_forms :: proc(t: ^testing.T) {
	context.allocator = context.temp_allocator
	runtime.DEFAULT_TEMP_ALLOCATOR_TEMP_GUARD()

	file := ast.File{
		fullpath = "test.odin",
		src = `
package main

OFFSETS :: [?]i32{0, 4, 8, 12}

@(rsp)
forms :: asm(p: ^[8]u32) [p = %s1] {
	#unroll for offset in OFFSETS {
		sw    %t4, [p + offset]
	}
	#unroll for step, i in ([3]i32{1, 2, 3}) {
		addiu %t1, %t1, step
		addiu %t2, %t2, -i
		addiu %t1, %t1, step * 2
	}
	#unroll for r, i in {%t3, %t5, %a0} {
		addiu r, %r, i
	}
	#unroll for vr in {%v08..<%v10} {
		vmudn %vr, %v11, %v01.q0
	}
	#unroll for _, i in 2..=3 {
		#unroll for lane in 0..<(i + 1) {
			sw    %t4, [p + (4*lane)]
		}
	}
	#unroll for n in 0..<0 {}
	vsar  %v23, %acc_hi
	vmov  %v00.e0, %v31.e0
	lw    %t3, [p + 16 - 4 + 2*4 - 2]
	addu  %t0, %t1 << 2, %t2
	#unroll for c in {%vco, %vcc, %vce} {
		cfc2  %t0, %c
	}
}
`,
	}

	p := parser.default_parser()
	p.err = log_parse_error
	ok := parser.parse_file(&p, &file)
	testing.expect(t, ok, "bad parse")
	testing.expect_value(t, file.syntax_error_count, 0)

	template := find_template(&file, "forms")
	if !testing.expect(t, template != nil, "forms should be parsed") {
		return
	}

	unrolls: [dynamic]^ast.Asm_Unroll
	for s in template.instructions {
		if u, is_unroll := s.derived.(^ast.Asm_Unroll); is_unroll {
			append(&unrolls, u)
		}
	}
	if !testing.expect_value(t, len(unrolls), 7) {
		return
	}

	_, is_ident := unrolls[0].expr.derived.(^ast.Ident)
	testing.expect(t, is_ident, "OFFSETS is an identifier")
	_, is_paren := unrolls[1].expr.derived.(^ast.Paren_Expr)
	testing.expect(t, is_paren, "a compound literal after 'in' is in parentheses")

	// `{%a, %b, ...}` keeps every register
	if list, is_group := unrolls[2].expr.derived.(^ast.Asm_Register_Group); testing.expect(t, is_group, "a register list") {
		testing.expect_value(t, len(list.registers), 3)
		testing.expect_value(t, list.range_token.kind, tokenizer.Token_Kind.Invalid)
		names := [3]string{"t3", "t5", "a0"}
		for reg, i in list.registers {
			if r, is_reg := reg.derived.(^ast.Asm_Register); is_reg {
				testing.expect_value(t, r.name, names[i])
			}
		}
	}

	if group, is_group := unrolls[3].expr.derived.(^ast.Asm_Register_Group); testing.expect(t, is_group, "a register range") {
		testing.expect_value(t, group.range_token.kind, tokenizer.Token_Kind.Range_Half)
	}

	range, is_range := unrolls[4].expr.derived.(^ast.Binary_Expr)
	if testing.expect(t, is_range, "a range") {
		testing.expect_value(t, range.op.kind, tokenizer.Token_Kind.Range_Full)
	}
	testing.expect_value(t, ident_name(unrolls[4].val0), "_")
	testing.expect_value(t, len(unrolls[4].instructions), 1)
	if inner, is_unroll := unrolls[4].instructions[0].derived.(^ast.Asm_Unroll); testing.expect(t, is_unroll, "a nested loop") {
		testing.expect_value(t, ident_name(inner.val0), "lane")
		testing.expect_value(t, inner.val1, nil)
	}
	testing.expect_value(t, len(unrolls[5].instructions), 0)

	// element accessors and RSP register names; a register's end includes its accessor
	vmudn := unrolls[3].instructions[0].derived.(^ast.Asm_Instruction)
	q0 := vmudn.operands[2].derived.(^ast.Asm_Register)
	testing.expect_value(t, q0.name, "v01")
	testing.expect_value(t, q0.flag, "q0")
	testing.expect_value(t, file.src[q0.pos.offset:q0.end.offset], "%v01.q0")

	found_acc_hi := false
	for s in template.instructions {
		if ins, is_ins := s.derived.(^ast.Asm_Instruction); is_ins && ins.name.name == "vsar" {
			r := ins.operands[1].derived.(^ast.Asm_Register)
			found_acc_hi = r.name == "acc_hi"
		}
	}
	testing.expect(t, found_acc_hi, "vsar's second operand is %acc_hi")

	// a memory operand is a flat list of terms
	for s in template.instructions {
		ins, is_ins := s.derived.(^ast.Asm_Instruction)
		if !is_ins || ins.name.name != "lw" {
			continue
		}
		mem := ins.operands[1].derived.(^ast.Asm_Memory_Operand)
		if testing.expect_value(t, len(mem.terms), 5) {
			testing.expect_value(t, mem.terms[2].op.kind, tokenizer.Token_Kind.Sub)
			testing.expect_value(t, mem.terms[3].scale_op.kind, tokenizer.Token_Kind.Mul)
			testing.expect_value(t, file.src[mem.terms[3].pos.offset:mem.terms[3].end.offset], "+ 2*4")
		}
	}

	// a shifted or scaled register or name is a plain operand too
	scaled := unrolls[1].instructions[2].derived.(^ast.Asm_Instruction)
	if mul, is_binary := scaled.operands[2].derived.(^ast.Binary_Expr); testing.expect(t, is_binary, "`step * 2` is a binary expression") {
		testing.expect_value(t, mul.op.kind, tokenizer.Token_Kind.Mul)
		testing.expect_value(t, ident_name(mul.left), "step")
		testing.expect_value(t, file.src[mul.pos.offset:mul.end.offset], "step * 2")
	}
	found_shift := false
	for s in template.instructions {
		ins, is_ins := s.derived.(^ast.Asm_Instruction)
		if !is_ins || ins.name.name != "addu" {
			continue
		}
		found_shift = true
		testing.expect_value(t, len(ins.operands), 3)
		if shl, is_binary := ins.operands[1].derived.(^ast.Binary_Expr); testing.expect(t, is_binary, "`%t1 << 2` is a binary expression") {
			testing.expect_value(t, shl.op.kind, tokenizer.Token_Kind.Shl)
			_, left_is_reg := shl.left.derived.(^ast.Asm_Register)
			testing.expect(t, left_is_reg, "the shifted operand is a register")
			testing.expect_value(t, file.src[shl.pos.offset:shl.end.offset], "%t1 << 2")
		}
	}
	testing.expect(t, found_shift, "the addu instruction is parsed")

	expect_same_tokens(t, print_asm_body(file.src, template.instructions),
	                   file.src[template.instructions[0].pos.offset:template.end.offset-1])
}

@test
test_parse_asm_register_list_needs_commas :: proc(t: ^testing.T) {
	// As in the compiler: `{%a %b}` is a syntax error.
	context.allocator = context.temp_allocator
	runtime.DEFAULT_TEMP_ALLOCATOR_TEMP_GUARD()

	file := ast.File{
		fullpath = "test.odin",
		src = `
package main

t :: asm() {
	#unroll for r in {%t3 %t5} {
		nop
	}
}
`,
	}
	p := parser.default_parser()
	p.err = proc(pos: tokenizer.Pos, format: string, args: ..any) {}
	parser.parse_file(&p, &file)
	testing.expect(t, file.syntax_error_count > 0, "a register list without commas should not parse")
}

log_parse_error :: proc(pos: tokenizer.Pos, format: string, args: ..any) {
	message := fmt.tprintf(format, ..args)
	log.errorf("%s(%d:%d): %s", pos.file, pos.line, pos.column, message)
}

find_template :: proc(file: ^ast.File, name: string) -> ^ast.Asm_Template {
	for decl in file.decls {
		vd, is_value := decl.derived.(^ast.Value_Decl)
		if !is_value || len(vd.names) != 1 || len(vd.values) != 1 || ident_name(vd.names[0]) != name {
			continue
		}
		if at, is_template := vd.values[0].derived.(^ast.Asm_Template); is_template {
			return at
		}
	}
	return nil
}

ident_name :: proc(e: ^ast.Expr) -> string {
	if e == nil {
		return ""
	}
	if ident, ok := e.derived.(^ast.Ident); ok {
		return ident.name
	}
	return ""
}

// The tokens of `a` and `b` are the same, ignoring comments and semicolons.
expect_same_tokens :: proc(t: ^testing.T, a, b: string, loc := #caller_location) {
	ta, tb := significant_tokens(a), significant_tokens(b)
	if !testing.expect_value(t, len(ta), len(tb), loc) {
		log.infof("printed:\n%s", a)
		return
	}
	for i in 0..<len(ta) {
		if !testing.expectf(t, ta[i] == tb[i], "token %d: %q, the source has %q", i, ta[i], tb[i], loc = loc) {
			return
		}
	}
}

significant_tokens :: proc(src: string) -> []string {
	tokens: [dynamic]string
	tk: tokenizer.Tokenizer
	tokenizer.init(&tk, src, "tokens")
	for {
		tok := tokenizer.scan(&tk)
		#partial switch tok.kind {
		case .EOF:
			return tokens[:]
		case .Comment, .Semicolon:
			continue
		}
		append(&tokens, tok.text)
	}
}

// Prints an asm template body from its AST: the asm nodes from their fields,
// Odin expressions (identifiers, literals, parenthesized expressions) from the source.
print_asm_body :: proc(src: string, stmts: []^ast.Stmt) -> string {
	b := strings.builder_make()
	print_asm_stmts(&b, src, stmts)
	return strings.to_string(b)
}

print_asm_stmts :: proc(b: ^strings.Builder, src: string, stmts: []^ast.Stmt) {
	for s in stmts {
		#partial switch n in s.derived_stmt {
		case ^ast.Asm_Instruction:
			strings.write_string(b, n.name.name)
			for op, i in n.operands {
				strings.write_string(b, i == 0 ? " " : ", ")
				print_asm_expr(b, src, op)
			}
			strings.write_string(b, "\n")
		case ^ast.Asm_Label_Decl:
			fmt.sbprintf(b, ".%s:\n", n.label.name)
		case ^ast.Asm_Directive:
			fmt.sbprintf(b, "#%s", n.name)
			for op, i in n.operands {
				strings.write_string(b, i == 0 ? " " : ", ")
				print_asm_expr(b, src, op)
			}
			strings.write_string(b, "\n")
		case ^ast.Asm_Unroll:
			strings.write_string(b, "#unroll for ")
			print_asm_expr(b, src, n.val0)
			if n.val1 != nil {
				strings.write_string(b, ", ")
				print_asm_expr(b, src, n.val1)
			}
			strings.write_string(b, " in ")
			print_asm_expr(b, src, n.expr)
			strings.write_string(b, " {\n")
			print_asm_stmts(b, src, n.instructions)
			strings.write_string(b, "}\n")
		case:
			fmt.sbprintf(b, "<unexpected %T>\n", s.derived_stmt)
		}
	}
}

print_asm_expr :: proc(b: ^strings.Builder, src: string, e: ^ast.Expr) {
	if e == nil {
		return
	}
	#partial switch n in e.derived_expr {
	case ^ast.Asm_Register:
		fmt.sbprintf(b, "%%%s", n.name)
		if n.flag != "" {
			fmt.sbprintf(b, ".%s", n.flag)
		}
	case ^ast.Asm_Label:
		fmt.sbprintf(b, ".%s", n.name)
	case ^ast.Asm_Register_Group:
		strings.write_string(b, "{")
		if n.range_token.kind != .Invalid && len(n.registers) == 2 {
			print_asm_expr(b, src, n.registers[0])
			strings.write_string(b, n.range_token.text)
			print_asm_expr(b, src, n.registers[1])
		} else {
			for reg, i in n.registers {
				if i > 0 {
					strings.write_string(b, ", ")
				}
				print_asm_expr(b, src, reg)
			}
		}
		strings.write_string(b, "}")
		if n.type != nil {
			strings.write_string(b, ":")
			print_asm_expr(b, src, n.type)
		}
	case ^ast.Asm_Memory_Operand:
		switch n.kind {
		case .Default:
		case .Pre:  strings.write_string(b, "#pre ")
		case .Post: strings.write_string(b, "#post ")
		}
		strings.write_string(b, "[")
		if n.segment_override != nil {
			print_asm_expr(b, src, n.segment_override)
			strings.write_string(b, ":")
		}
		for term, i in n.terms {
			if i > 0 {
				fmt.sbprintf(b, " %s ", term.op.text)
			}
			print_asm_expr(b, src, term.operand)
			if term.scale != nil {
				strings.write_string(b, term.scale_op.text)
				print_asm_expr(b, src, term.scale)
			}
		}
		strings.write_string(b, "]")
		if n.type != nil {
			strings.write_string(b, ":")
			print_asm_expr(b, src, n.type)
		}
	case ^ast.Unary_Expr:
		strings.write_string(b, n.op.text)
		print_asm_expr(b, src, n.expr)
	case ^ast.Binary_Expr: // `%t0 << 2` as an operand
		print_asm_expr(b, src, n.left)
		strings.write_string(b, n.op.text)
		print_asm_expr(b, src, n.right)
	case ^ast.Index_Expr:
		print_asm_expr(b, src, n.expr)
		strings.write_string(b, "[")
		print_asm_expr(b, src, n.index)
		strings.write_string(b, "]")
	case:
		strings.write_string(b, src[e.pos.offset:e.end.offset])
	}
}
