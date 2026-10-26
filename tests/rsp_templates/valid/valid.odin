// @(rsp) templates every build target must accept under -vet -strict-style:
// test_rsp_templates.py checks this package for several hosts, the n64 and
// targets with no CPU asm table at all.
package rsp_templates_valid

import "base:intrinsics"

import "transform"

// Through a package selector, at file scope
TRANSFORM :: intrinsics.rsp_template_code(transform.transform)

// The output pinned as the exit contract; a scratch register by name.
@(rsp)
fill :: asm(output: ^[4]u32, value: u32) -> (end: u32) [output = %s2, value = %a0, end = %s3, word: u32 = %t2] {
	or    word, value, %zero
	sw    word, [output + 0]
	sw    word, [output + 4]
	sw    word, [output + 8]
	sw    word, [output + 12]
	addiu end, output, 16
}

@(export)
fill_code :: proc "contextless" () -> []intrinsics.Rsp_Code_Line {
	return intrinsics.rsp_template_code(fill)
}

@(export)
transform_words :: proc "contextless" () -> (count: int, first: u32, line: i32) {
	code := TRANSFORM
	return len(code), code[0].word, code[0].line
}
