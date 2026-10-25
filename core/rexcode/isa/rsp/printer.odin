// rexcode  ·  Brendan Punsky (dotbmp@github), original author

package rexcode_rsp

import "core:strings"
import "core:reflect"
import "core:os"
import "core:io"
import "core:rexcode/isa"

// =============================================================================
// N64 RSP PRINTER
// =============================================================================
//
// libdragon syntax: GNU as with `#include <rsp.inc>` (preprocessed as
// assembler-with-cpp), the usual way RSP ucode is built today. With the
// default options, a decoded line assembles to the word it was decoded from.
// rsp.inc's names are case-sensitive, so `uppercase` applies to mnemonics
// only; `show_offsets` prefixes don't assemble, and a branch whose target is
// outside the printed range prints as an absolute address.
//   - Scalar GPRs use the bare ABI names that `regdef.h` `#define`s
//     (`t0`, `sp`, `ra`). `$1` stays `$at`: there is no `at` define (GNU as
//     warns about `$at` under rsp.inc's `.set at`; the word is right).
//   - Vector regs are two-digit `$v00..$v31`. A VR_ELEM element uses
//     rsp.inc's accessor names (`vmulf $v01, $v02, $v03.q1`); field 1 has no
//     name and falls back to the 4-operand form (`..., $v03, 1`).
//   - Single-lane ops (vrcp, vmov, ...) print `op vd, de, vt, element`.
//   - Vector loads/stores print `op vt, element, byte_offset, base`: the
//     element is always present and the offset is in bytes, not units.
//   - mtc2/mfc2 print the byte element as a third operand when non-zero.
//   - CP0 and COP2 control registers print as rsp.inc's `COP0_*` and
//     `COP2_CTRL_*` names.
//
// All seven sink families (sbprint / print / aprint / tprint / bprint /
// fprint / wprint) wrap the same core `sbprint`.

Token                 :: isa.Token
Token_Kind            :: isa.Token_Kind
Print_Options         :: isa.Print_Options
Print_Result          :: isa.Print_Result
DEFAULT_PRINT_OPTIONS :: isa.DEFAULT_PRINT_OPTIONS

@(rodata, private="file")
GPR_NAMES_ABI := [32]string{
	"zero", "$at",
	"v0", "v1",
	"a0", "a1", "a2", "a3",
	"t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
	"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
	"t8", "t9",
	"k0", "k1",
	"gp", "sp", "fp", "ra",
}

@(rodata, private="file")
CP0_NAMES := [16]string{
	"COP0_DMA_SPADDR", "COP0_DMA_RAMADDR", "COP0_DMA_READ",   "COP0_DMA_WRITE",
	"COP0_SP_STATUS",  "COP0_DMA_FULL",    "COP0_DMA_BUSY",   "COP0_SEMAPHORE",
	"COP0_DP_START",   "COP0_DP_END",      "COP0_DP_CURRENT", "COP0_DP_STATUS",
	"COP0_DP_CLOCK",   "COP0_DP_BUSY",     "COP0_DP_PIPE_BUSY", "COP0_DP_TMEM_BUSY",
}

@(rodata, private="file")
VC_NAMES := [3]string{"COP2_CTRL_VCO", "COP2_CTRL_VCC", "COP2_CTRL_VCE"}

// rsp.inc accessor names for a vector ALU element field (index = field).
// Field 0 is the whole vector (no suffix); field 1 has no name.
@(rodata, private="file")
VU_ELEMENT_NAMES := [16]string{
	"", "",
	"q0", "q1",
	"h0", "h1", "h2", "h3",
	"e0", "e1", "e2", "e3", "e4", "e5", "e6", "e7",
}

mnemonic_to_string :: proc(m: Mnemonic, lowercase: bool = true, allocator := context.temp_allocator) -> string {
	sb := strings.builder_make(allocator)
	write_mnemonic(&sb, m, !lowercase)
	return strings.to_string(sb)
}

register_name :: proc(r: Register, lowercase: bool = true, allocator := context.temp_allocator) -> string {
	sb := strings.builder_make(allocator)
	write_register(&sb, r)
	if !lowercase {
		defer strings.builder_destroy(&sb)
		return strings.to_upper(strings.to_string(sb), allocator)
	}
	return strings.to_string(sb)
}

// =============================================================================
// Core sbprint
// =============================================================================

sbprint :: proc(
	sb:           ^strings.Builder,
	instructions: []Instruction,
	inst_info:    []Instruction_Info,
	label_defs:   []Label_Definition,
	tokens:       ^[dynamic]Token = nil,
	options:      ^Print_Options  = nil,
	label_names:  ^isa.Label_Names = nil,
) {
	opts := options
	if opts == nil {
		@(static) defaults := DEFAULT_PRINT_OPTIONS
		opts = &defaults
	}

	// Display-side label naming: numbers in ADDRESS order (independent of the internal ids'
	// allocation order), caller names keyed by byte offset (isa.Label_Display).
	display: isa.Label_Display
	isa.label_display_init(&display, label_defs, label_names)
	defer isa.label_display_destroy(&display)

	for i in 0..<len(instructions) {
		inst := &instructions[i]
		offset := u32(i) * 4
		if i < len(inst_info) {
			offset = inst_info[i].offset
		}

		// A displayable label at this offset — a definition, or a caller-named offset?
		if isa.label_display_at(&display, offset) {
			isa.label_display_write(&display, sb, offset, opts.label_prefix)
			strings.write_byte(sb, ':')
			strings.write_string(sb, opts.separator)
		}

		strings.write_string(sb, opts.indent)
		if opts.show_offsets {
			isa.print_hex(sb, u64(offset), opts)
			strings.write_string(sb, ": ")
		}

		write_instruction(sb, inst, &display, opts)
		strings.write_string(sb, opts.separator)
	}
}

sbprintln :: proc(
	sb:           ^strings.Builder,
	instructions: []Instruction,
	inst_info:    []Instruction_Info,
	label_defs:   []Label_Definition,
	tokens:       ^[dynamic]Token = nil,
	options:      ^Print_Options  = nil,
	label_names:  ^isa.Label_Names = nil,
) {
	sbprint(sb, instructions, inst_info, label_defs, tokens, options, label_names)
	strings.write_byte(sb, '\n')
}

// =============================================================================
// Sink wrappers
// =============================================================================

print :: proc(
	instructions: []Instruction, inst_info: []Instruction_Info, label_defs: []Label_Definition,
	tokens: ^[dynamic]Token = nil, options: ^Print_Options = nil, label_names: ^isa.Label_Names = nil,
) {
	sb := strings.builder_make(context.temp_allocator)
	sbprint(&sb, instructions, inst_info, label_defs, tokens, options, label_names)
	os.write_string(os.stdout, strings.to_string(sb))
}

println :: proc(
	instructions: []Instruction, inst_info: []Instruction_Info, label_defs: []Label_Definition,
	tokens: ^[dynamic]Token = nil, options: ^Print_Options = nil, label_names: ^isa.Label_Names = nil,
) {
	sb := strings.builder_make(context.temp_allocator)
	sbprintln(&sb, instructions, inst_info, label_defs, tokens, options, label_names)
	os.write_string(os.stdout, strings.to_string(sb))
}

aprint :: proc(
	instructions: []Instruction, inst_info: []Instruction_Info, label_defs: []Label_Definition,
	tokens: ^[dynamic]Token = nil, options: ^Print_Options = nil, label_names: ^isa.Label_Names = nil,
	allocator := context.allocator,
) -> string {
	sb := strings.builder_make(allocator)
	sbprint(&sb, instructions, inst_info, label_defs, tokens, options, label_names)
	return strings.to_string(sb)
}

aprintln :: proc(
	instructions: []Instruction, inst_info: []Instruction_Info, label_defs: []Label_Definition,
	tokens: ^[dynamic]Token = nil, options: ^Print_Options = nil, label_names: ^isa.Label_Names = nil,
	allocator := context.allocator,
) -> string {
	sb := strings.builder_make(allocator)
	sbprintln(&sb, instructions, inst_info, label_defs, tokens, options, label_names)
	return strings.to_string(sb)
}

tprint :: proc(
	instructions: []Instruction, inst_info: []Instruction_Info, label_defs: []Label_Definition,
	tokens: ^[dynamic]Token = nil, options: ^Print_Options = nil, label_names: ^isa.Label_Names = nil,
) -> string {
	sb := strings.builder_make(context.temp_allocator)
	sbprint(&sb, instructions, inst_info, label_defs, tokens, options, label_names)
	return strings.to_string(sb)
}

tprintln :: proc(
	instructions: []Instruction, inst_info: []Instruction_Info, label_defs: []Label_Definition,
	tokens: ^[dynamic]Token = nil, options: ^Print_Options = nil, label_names: ^isa.Label_Names = nil,
) -> string {
	sb := strings.builder_make(context.temp_allocator)
	sbprintln(&sb, instructions, inst_info, label_defs, tokens, options, label_names)
	return strings.to_string(sb)
}

bprint :: proc(
	buf: []u8,
	instructions: []Instruction, inst_info: []Instruction_Info, label_defs: []Label_Definition,
	tokens: ^[dynamic]Token = nil, options: ^Print_Options = nil, label_names: ^isa.Label_Names = nil,
) -> string {
	sb := strings.builder_from_bytes(buf)
	sbprint(&sb, instructions, inst_info, label_defs, tokens, options, label_names)
	return strings.to_string(sb)
}

bprintln :: proc(
	buf: []u8,
	instructions: []Instruction, inst_info: []Instruction_Info, label_defs: []Label_Definition,
	tokens: ^[dynamic]Token = nil, options: ^Print_Options = nil, label_names: ^isa.Label_Names = nil,
) -> string {
	sb := strings.builder_from_bytes(buf)
	sbprintln(&sb, instructions, inst_info, label_defs, tokens, options, label_names)
	return strings.to_string(sb)
}

fprint :: proc(
	fd: ^os.File,
	instructions: []Instruction, inst_info: []Instruction_Info, label_defs: []Label_Definition,
	tokens: ^[dynamic]Token = nil, options: ^Print_Options = nil, label_names: ^isa.Label_Names = nil,
) {
	sb := strings.builder_make(context.temp_allocator)
	sbprint(&sb, instructions, inst_info, label_defs, tokens, options, label_names)
	os.write_string(fd, strings.to_string(sb))
}

fprintln :: proc(
	fd: ^os.File,
	instructions: []Instruction, inst_info: []Instruction_Info, label_defs: []Label_Definition,
	tokens: ^[dynamic]Token = nil, options: ^Print_Options = nil, label_names: ^isa.Label_Names = nil,
) {
	sb := strings.builder_make(context.temp_allocator)
	sbprintln(&sb, instructions, inst_info, label_defs, tokens, options, label_names)
	os.write_string(fd, strings.to_string(sb))
}

wprint :: proc(
	w: io.Writer,
	instructions: []Instruction, inst_info: []Instruction_Info, label_defs: []Label_Definition,
	tokens: ^[dynamic]Token = nil, options: ^Print_Options = nil, label_names: ^isa.Label_Names = nil,
) {
	sb := strings.builder_make(context.temp_allocator)
	sbprint(&sb, instructions, inst_info, label_defs, tokens, options, label_names)
	io.write_string(w, strings.to_string(sb))
}

wprintln :: proc(
	w: io.Writer,
	instructions: []Instruction, inst_info: []Instruction_Info, label_defs: []Label_Definition,
	tokens: ^[dynamic]Token = nil, options: ^Print_Options = nil, label_names: ^isa.Label_Names = nil,
) {
	sb := strings.builder_make(context.temp_allocator)
	sbprintln(&sb, instructions, inst_info, label_defs, tokens, options, label_names)
	io.write_string(w, strings.to_string(sb))
}

// =============================================================================
// Internal writers
// =============================================================================

@(private="file")
write_mnemonic :: proc(sb: ^strings.Builder, m: Mnemonic, uppercase: bool) {
	name, ok := reflect.enum_name_from_value(m)
	if !ok {
		strings.write_string(sb, "<?>")
		return
	}
	for i in 0..<len(name) {
		c := name[i]
		if !uppercase && c >= 'A' && c <= 'Z' {
			strings.write_byte(sb, c + 32)
		} else {
			strings.write_byte(sb, c)
		}
	}
}

// Operands are always lowercase: rsp.inc's names are case-sensitive.
@(private="file")
write_register :: proc(sb: ^strings.Builder, r: Register) {
	if r == NONE {
		strings.write_string(sb, "<none>")
		return
	}
	hw := reg_hw(r)
	switch reg_class(r) {
	case REG_GPR:
		strings.write_string(sb, GPR_NAMES_ABI[hw])
	case REG_VR:
		// rsp.inc only defines the two-digit spellings ($v01, not $v1).
		strings.write_string(sb, "$v")
		if hw < 10 { strings.write_byte(sb, '0') }
		write_decimal_u32(sb, u32(hw))
	case REG_VC:
		if int(hw) < len(VC_NAMES) {
			strings.write_string(sb, VC_NAMES[hw])
		} else {
			strings.write_byte(sb, '$')
			write_decimal_u32(sb, u32(hw))
		}
	case REG_CP0:
		if int(hw) < len(CP0_NAMES) {
			strings.write_string(sb, CP0_NAMES[hw])
		} else {
			strings.write_byte(sb, '$')
			write_decimal_u32(sb, u32(hw))
		}
	case:
		write_decimal_u32(sb, u32(hw))
	}
}

@(private="file")
write_comma :: proc(sb: ^strings.Builder, opts: ^Print_Options) {
	strings.write_byte(sb, ',')
	if opts.space_after_comma {
		strings.write_byte(sb, ' ')
	}
}

// Access size in bytes of a vector load/store: the unit its 7-bit offset
// field counts in.
@(private="file")
vector_ls_size :: proc(m: Mnemonic) -> i32 {
	#partial switch m {
	case .LBV, .SBV:                     return 1
	case .LSV, .SSV:                     return 2
	case .LLV, .SLV:                     return 4
	case .LDV, .SDV, .LPV, .SPV, .LUV, .SUV: return 8
	}
	return 16
}

// Single-lane ops: vs holds the destination lane (`de`), not a register.
@(private="file")
is_single_lane :: proc(m: Mnemonic) -> bool {
	#partial switch m {
	case .VRCP, .VRCPL, .VRCPH, .VRSQ, .VRSQL, .VRSQH:
		return true
	}
	return false
}

// Mnemonic plus operands. Most instructions print their operands in order;
// the cases below are where rsp.inc's macros want a different shape.
@(private="file")
write_instruction :: proc(
	sb:      ^strings.Builder,
	inst:    ^Instruction,
	display: ^isa.Label_Display,
	opts:    ^Print_Options,
) {
	ops := inst.ops[:inst.operand_count]
	m   := inst.mnemonic

	// rsp.inc has no `lwv` macro; `loadVector` is the helper its load
	// macros expand to, and takes the raw 7-bit offset field.
	if m == .LWV && len(ops) == 2 && ops[1].kind == .VECTOR_MEM {
		strings.write_string(sb, "loadVector 0b01010")
		write_comma(sb, opts)
		write_register(sb, ops[0].reg)
		write_comma(sb, opts)
		write_decimal_u32(sb, u32(ops[1].vmem.element & 0x0F))
		write_comma(sb, opts)
		write_decimal_u32(sb, u32(ops[1].vmem.offset) & 0x7F)
		write_comma(sb, opts)
		write_register(sb, ops[1].vmem.base)
		return
	}

	// rsp.inc's `vsar` only takes `vsar vd, element` with vs = vt = 0 and an
	// accumulator slice (8-10); anything else goes through its `vectorOp`.
	if m == .VSAR && len(ops) == 3 {
		vs, vt, element := ops[1].reg, ops[2].reg, ops[2].element & 0x0F
		if reg_hw(vs) == 0 && reg_hw(vt) == 0 && element >= 8 && element <= 10 {
			write_mnemonic(sb, m, opts.uppercase)
			strings.write_byte(sb, ' ')
			write_register(sb, ops[0].reg)
			write_comma(sb, opts)
			write_decimal_u32(sb, u32(element))
		} else {
			strings.write_string(sb, "vectorOp 0b011101")
			for i in 0..<3 {
				write_comma(sb, opts)
				write_register(sb, ops[i].reg)
			}
			write_comma(sb, opts)
			write_decimal_u32(sb, u32(element))
		}
		return
	}

	write_mnemonic(sb, m, opts.uppercase)
	if len(ops) == 0 {
		return
	}
	strings.write_byte(sb, ' ')

	switch {
	case is_single_lane(m) && len(ops) == 3:
		// `op vd, de, vt, element`
		write_register(sb, ops[0].reg)
		write_comma(sb, opts)
		write_decimal_u32(sb, u32(reg_hw(ops[1].reg)))
		write_comma(sb, opts)
		write_register(sb, ops[2].reg)
		write_comma(sb, opts)
		write_decimal_u32(sb, u32(ops[2].element & 0x0F))

	case m == .VMOV && len(ops) == 2:
		// `vmov vd, de, vt, element`; the table encodes no de, so it is 0.
		write_register(sb, ops[0].reg)
		write_comma(sb, opts)
		strings.write_byte(sb, '0')
		write_comma(sb, opts)
		write_register(sb, ops[1].reg)
		write_comma(sb, opts)
		write_decimal_u32(sb, u32(ops[1].element & 0x0F))

	case len(ops) == 2 && ops[1].kind == .VECTOR_MEM:
		// `op vt, element, byte_offset, base`. The encoder takes the element
		// from the VECTOR_MEM operand, so vt prints without a suffix even when
		// a builder mirrored the element onto it.
		write_register(sb, ops[0].reg)
		write_comma(sb, opts)
		write_operand(sb, &ops[1], m, display, opts)

	case m == .BREAK && len(ops) == 1 && ops[0].kind == .IMMEDIATE:
		// The 20-bit code sits in bits 25-6, but GNU as puts a lone
		// `break N` in bits 25-16; `break hi, lo` fills both halves.
		code := u32(ops[0].immediate) & 0xFFFFF
		write_decimal_u32(sb, code >> 10)
		if code & 0x3FF != 0 {
			write_comma(sb, opts)
			write_decimal_u32(sb, code & 0x3FF)
		}

	case (m == .MTC2 || m == .MFC2) && len(ops) == 2:
		// `mtc2 rt, vs[, element]`, element = byte offset (bits 10-7).
		// rsp.inc resolves rt as `hex.<name>`, which only knows `at` for $1.
		if reg_class(ops[0].reg) == REG_GPR && reg_hw(ops[0].reg) == 1 {
			strings.write_string(sb, "at")
		} else {
			write_register(sb, ops[0].reg)
		}
		write_comma(sb, opts)
		write_register(sb, ops[1].reg)
		if ops[1].element & 0x0F != 0 {
			write_comma(sb, opts)
			write_decimal_u32(sb, u32(ops[1].element & 0x0F))
		}

	case:
		for &op, slot in ops {
			if slot > 0 {
				write_comma(sb, opts)
			}
			if op.kind == .VECTOR_REG && slot != 2 {
				// Only vt (slot 2) has an element field; encode ignores any other.
				write_register(sb, op.reg)
			} else {
				write_operand(sb, &op, m, display, opts)
			}
		}
	}
}

@(private="file")
write_operand :: proc(
	sb:       ^strings.Builder,
	op:       ^Operand,
	mnemonic: Mnemonic,
	display:  ^isa.Label_Display,
	opts:     ^Print_Options,
) {
	switch op.kind {
	case .NONE:

	case .REGISTER:
		write_register(sb, op.reg)

	case .VECTOR_REG:
		write_register(sb, op.reg)
		// Element 0 is the whole vector and needs no suffix.
		// Masked to the 4-bit field, as encode does.
		element := op.element & 0x0F
		if element != 0 {
			name := VU_ELEMENT_NAMES[element]
			if name != "" {
				strings.write_byte(sb, '.')
				strings.write_string(sb, name)
			} else {
				write_comma(sb, opts)
				write_decimal_u32(sb, u32(element))
			}
		}

	case .IMMEDIATE:
		write_signed_decimal(sb, op.immediate)

	case .MEMORY:
		write_signed_decimal(sb, i64(op.mem.disp))
		strings.write_byte(sb, '(')
		write_register(sb, op.mem.base)
		strings.write_byte(sb, ')')

	case .VECTOR_MEM:
		// `element, byte_offset, base` (the operand after vt). The encoded
		// offset counts access-size units; rsp.inc wants bytes. Both fields
		// are masked to their encoded width, as encode does.
		offset := (op.vmem.offset << 25) >> 25   // sign-extend the 7-bit field
		write_decimal_u32(sb, u32(op.vmem.element & 0x0F))
		write_comma(sb, opts)
		write_signed_decimal(sb, i64(offset) * i64(vector_ls_size(mnemonic)))
		write_comma(sb, opts)
		write_register(sb, op.vmem.base)

	case .RELATIVE:
		target := u32(op.relative)
		if isa.label_display_at(display, target) {
			isa.label_display_write(display, sb, target, opts.label_prefix)
		} else {
			isa.print_hex(sb, u64(target), opts)
		}
	}
}

@(private="file")
write_decimal_u32 :: proc(sb: ^strings.Builder, v: u32) {
	if v == 0 {
		strings.write_byte(sb, '0')
		return
	}
	buf: [10]u8
	i := 0
	n := v
	for n > 0 { buf[i] = '0' + u8(n % 10); n /= 10; i += 1 }
	for j := i - 1; j >= 0; j -= 1 { strings.write_byte(sb, buf[j]) }
}

@(private="file")
write_signed_decimal :: proc(sb: ^strings.Builder, v: i64) {
	if v < 0 {
		strings.write_byte(sb, '-')
		n := u64(-(v + 1)) + 1
		write_decimal_u64(sb, n)
	} else {
		write_decimal_u64(sb, u64(v))
	}
}

@(private="file")
write_decimal_u64 :: proc(sb: ^strings.Builder, v: u64) {
	if v == 0 {
		strings.write_byte(sb, '0')
		return
	}
	buf: [20]u8
	i := 0
	n := v
	for n > 0 { buf[i] = '0' + u8(n % 10); n /= 10; i += 1 }
	for j := i - 1; j >= 0; j -= 1 { strings.write_byte(sb, buf[j]) }
}
