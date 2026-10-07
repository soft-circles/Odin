# CPU asm templates on the N64 (MIPS III)

Odin `asm` templates compile to LLVM inline assembly on the N64 CPU. They are
available on `-target:n64` and `-target:freestanding_mips32be`. Both targets
select the NEC VR4300 (MIPS III, `-microarch:mips3`) under the o64 ABI: GPRs and
FPRs are 64 bits wide, pointers are 32 bits. The checker validates every
instruction against a hand-written MIPS III table, and LLVM's integrated
assembler encodes the result.

These templates are ordinary CPU code. For separate RSP queue commands, see
[Handwritten RSP assembly](RSP_ASSEMBLY.md); its delay-slot and register rules
differ from the ones below.

## Dependencies

- Compiler: this branch, built against the MIPS O64 LLVM fork described in
  [Build this Odin compiler](N64_BUILD.md#build-this-odin-compiler).
- Checking and object output need no SDK. ROM output needs the
  [libdragon SDK](N64_BUILD.md#install-the-libdragon-sdk); the runtime ROM test
  also needs a compatible `ares-test` runner.

The templates below replace libdragon's `cache_op` loop body and its
`C0_COUNT`/`C0_WRITE_COMPARE` macros:

```odin
DCACHE_LINE :: 16

dcache_hit_wb_inv_line :: asm(p: rawptr) { cache 0x15, [p] }  // Hit_Write_Back_Invalidate (D)
icache_hit_inv_line    :: asm(p: rawptr) { cache 0x10, [p] }  // Hit_Invalidate (I)
read_cp0_count         :: asm() -> (r: u32) { mfc0 r, %c0_count }
write_cp0_compare      :: asm(v: u32) { mtc0 v, %c0_compare }

data_cache_hit_writeback_invalidate :: proc "contextless" (addr: rawptr, length: int) {
	if length <= 0 { return }
	cur  := uintptr(addr) &~ (DCACHE_LINE-1)
	stop := uintptr(addr) + uintptr(length)
	for ; cur < stop; cur += DCACHE_LINE {
		dcache_hit_wb_inv_line(rawptr(cur))
	}
}
```

The line loop is plain Odin. Each `cache`, `mfc0` and `mtc0` template is
inferred volatile with a memory clobber, so it is neither removed nor moved
across loads and stores. The [fixture](tests/n64_asm/fixture/fixture.odin)
holds these and the other templates the tests encode.

## Templates and operands

Operand order is the assembler's: destination first. Each instruction occupies
one line, or instructions are separated by `;`. Parameters, results, register
pins (`[r = %v1]`) and `#clobber` work as on the other asm targets.

### Registers

| Class | Names |
| --- | --- |
| GPR | `%zero`, `%v0`–`%v1`, `%a0`–`%a3`, `%t0`–`%t9`, `%s0`–`%s7`, `%k0`, `%k1`, `%gp`, `%sp`, `%fp` (alias `%s8`), `%ra` |
| FPR | `%f0`–`%f31` (FR=1: 32 independent 64-bit registers); literal operands in FPU slots, pins and `#clobber`, see below |
| COP0 | `%c0_index`, `%c0_random`, `%c0_entrylo0`, `%c0_entrylo1`, `%c0_context`, `%c0_pagemask`, `%c0_wired`, `%c0_badvaddr`, `%c0_count`, `%c0_entryhi`, `%c0_compare`, `%c0_status`, `%c0_cause`, `%c0_epc`, `%c0_prid`, `%c0_config`, `%c0_lladdr`, `%c0_watchlo`, `%c0_watchhi`, `%c0_xcontext`, `%c0_perr`, `%c0_cacheerr`, `%c0_taglo`, `%c0_taghi`, `%c0_errorepc` |
| COP1 control | `%fcr0` (FIR), `%fcr31` (FCSR) |
| Tracked implicit | `%hi`, `%lo`, `%fcc0` (FP condition bit, FCSR bit 23) |

Numeric names such as `%4` cannot be written. `%s8` and `%fp` are the same
register; a pin or clobber of either covers both.

`%at` is not a register name on this target. LLVM wraps every inline-asm block
in `.set at` and `.set macro`, so the assembler may expand `li`, a large memory
offset or an out-of-range immediate through `$at`. LLVM also allocates `$at` to
operands. Without a `$at` clobber, LLVM miscompiles a store through a large
offset:

```text
sll   at,a1,0x0        # value to store placed in $at
lui   at,0x1           # offset expansion overwrites it
addu  at,at,v0
sw    at,-25536(at)    # stores the address, not the value
```

Every template therefore clobbers `$1`, and a template can neither pin nor name
it. Writing `%at` anywhere, including a pin or `#clobber`, is reported as
`Register %at ($1) is reserved for the assembler, which uses it to expand macros (large offsets, li, ...); every MIPS asm template clobbers it, so it cannot be named in one`.
GCC treats `$at` as reserved, so C inline asm never meets this.

COP0 and COP1 control registers fill only the slots of `mfc0`/`mtc0`,
`dmfc0`/`dmtc0` and `cfc1`/`ctc1`, and only by name. `%hi`, `%lo` and `%fcc0`
are never explicit operands of the VR4300 forms. `odin check` rejects a
register of the wrong class in any slot, for example
`'addu' operand-1 cannot be %c0_status, it must be a general-purpose register`
or `'add_s' operand-1 cannot be %t0, it must be a floating-point register (%f0..%f31)`.

A literal FPR name (`%f0`..`%f31`) fills any FPU slot, whatever its view:
`add_s %f4, a, b`, `add_d %f6, a, b` and `cvt_d_w %f2, %f2` all check. A
parameter must still match the slot's width (`f32` for `_s`, `f64` for `_d`).
`f32` and `f64` parameters can be pinned to an FPR (`[a = %f12, r = %f0]`),
which lowers to `{$f12}` and `{$f0}` constraints.

Output pins to `%zero`, `%k0`, `%k1`, `%gp` and `%sp` are rejected. COP0,
COP1 control and `%fcc0` cannot be pinned at all, because LLVM has no
constraint that moves a value into them; `odin check` reports, for example,
`'asm' parameter 'v' cannot be pinned to %c0_count: LLVM has no register constraint for it; move the value with 'mtc0'/'mfc0' or 'ctc1'/'cfc1' in the template`.
Set `%fcc0` with a `c_*` compare inside the template. `%hi` and `%lo` can be
pinned, and a parameter pinned to `%hi` or `%lo` counts as used when an
instruction such as `mflo` reads it implicitly. LLVM treats its `{hi}` and
`{lo}` constraints as 32-bit registers whatever the value type, so a 64-bit
parameter pinned to `%hi` or `%lo` travels through a general-purpose register
instead: the compiler emits `mthi`/`mtlo` before the body and `mfhi`/`mflo`
after it. 32-bit pins use the constraints directly.

Integer and boolean inputs narrower than 32 bits (`u8`, `i16`, `bool`),
pinned or not, cross the asm boundary as 32-bit values, zero- or
sign-extended by their Odin type, and results are truncated back. An integer
output narrower than 64 bits that a 64-bit instruction writes, such as `ld`,
`lwu`, a `d` form or `dmfc0`, is converted back to its type's canonical
32-bit form after the template, so the compiler never sees a stale upper half.

### Operand types

| Slot | Accepts |
| --- | --- |
| GPR | An integer, boolean or pointer parameter of up to 64 bits, or a literal GPR. |
| 32-bit GPR | An integer, boolean or pointer parameter of at most 32 bits, or a literal GPR. LLVM keeps `i32` values sign-extended, which MIPS III requires. |
| FPR `.s`, `.w` | An `f32` parameter. A `.w` slot holds a 32-bit integer bit pattern. |
| FPR `.d`, `.l` | An `f64` parameter. A `.l` slot holds a 64-bit integer bit pattern. |
| Memory | `[base]`, `[base + disp]` or `[base - disp]`, where `base` is a GPR. |
| Label | `.name`, a label defined in the same template. |

The 32-bit GPR slots are those of `add`, `addu`, `sub`, `subu`, `addi`,
`addiu`, `negu`, `sll`, `srl`, `sra`, `sllv`, `srlv`, `srav`, `mult`, `multu`,
`div`, `divu`, `mfc0`, `mtc0`, `mfc1`, `mtc1`, `cfc1` and `ctc1`. The shift
amount register of `sllv`, `srlv` and `srav` is a full GPR slot. The VR4300
result of these instructions is unpredictable unless each input is a
sign-extended 32-bit value, so a 64-bit parameter there is an error:
`'addu' operand-1 has the wrong size: expected a 32-bit integer operand, got 64-bit`.
Use the `d` forms (`daddu`, `dsll`, `dmult`, `dmtc0`, ...) for 64-bit values.
A literal GPR carries no width and is accepted in either kind of slot.

FPU widths must match exactly: an `f32` in a `.d` slot is rejected, and an
integer parameter in a `.w` or `.l` slot is a register-class error.

Memory operands have one base register and a constant displacement in the
signed 16-bit range, -32768–32767. There is no index register or scale. A
displacement outside that range is an error rather than an `$at` expansion.
A displacement term that names a `$`-immediate parameter is resolved per
instantiation and is not range-checked; the assembler expands a large one
through `$at`.

### Immediates

| Slot | Range | Mnemonics |
| --- | --- | --- |
| Signed 16-bit | -32768–32767 | `addi`, `addiu`, `daddi`, `daddiu`, `slti`, `sltiu`, `teqi`, `tnei`, `tgei`, `tgeiu`, `tlti`, `tltiu` |
| Unsigned 16-bit | 0–65535 | `andi`, `ori`, `xori`, `lui` |
| Shift amount | 0–31 | `sll`, `srl`, `sra`, `dsll`, `dsrl`, `dsra`, `dsll32`, `dsrl32`, `dsra32` |
| Cache operation | 0–31, and a VR4300 operation from the table below | `cache` |
| Trap/break code | 0–1023 | `break`, `teq`, `tne`, `tge`, `tgeu`, `tlt`, `tltu` |
| Syscall code | 0–1048575 | `syscall` |
| `li` constant | -2147483648–4294967295 | `li` (expands to `addiu`, `ori`, or `lui` + `ori` on the destination) |

The checker reads each slot's sign: `addiu r, x, 40000` and `andi r, x, -1` are
errors, not reinterpreted bit patterns. The `*32` shifts take 0–31 and shift by
that amount + 32.

VR4300 cache operations are `(operation << 2) | cache`, with cache 0 for the
I-cache and 1 for the D-cache:

| Cache | Operations |
| --- | --- |
| I-cache | `0x00` Index_Invalidate, `0x04` Index_Load_Tag, `0x08` Index_Store_Tag, `0x10` Hit_Invalidate, `0x14` Fill, `0x18` Hit_Write_Back |
| D-cache | `0x01` Index_Write_Back_Invalidate, `0x05` Index_Load_Tag, `0x09` Index_Store_Tag, `0x0D` Create_Dirty_Exclusive, `0x11` Hit_Invalidate, `0x15` Hit_Write_Back_Invalidate, `0x19` Hit_Write_Back |

Any other value in 0–31 is an error, for example `cache 0x02, [p]`:
`'cache' operand-0 immediate 2 is not a VR4300 cache operation (valid: 0x00 0x04 0x08 0x10 0x14 0x18 for the I-cache, 0x01 0x05 0x09 0x0D 0x11 0x15 0x19 for the D-cache)`.
A value above 31, such as `0x20`, is out of range.

### FPU spelling

An Odin identifier cannot contain `.`, so FPU mnemonics use `_`: `add_s`,
`cvt_d_w`, `c_olt_d`, `trunc_w_s`. The backend writes the `.` form
(`add.s`, `cvt.d.w`, `c.olt.d`).

```odin
min_f64 :: asm(a: f64, b: f64) -> (r: f64) {
	mov_d r, a
	c_olt_d b, a
	bc1f .done
	mov_d r, b
.done:
}
```

## Supported instructions

Mnemonics come from the table's mnemonic list. Every VR4300 form is accepted;
the last group is rejected.

| Group | Mnemonics |
| --- | --- |
| System, cache, COP0 | `nop`, `ssnop`, `sync`, `syscall`, `break`, `cache`, `mfc0`, `mtc0`, `dmfc0`, `dmtc0`, `tlbr`, `tlbwi`, `tlbwr`, `tlbp`, `eret` |
| Integer ALU | `add`, `addu`, `sub`, `subu`, `and`, `or`, `xor`, `nor`, `slt`, `sltu`, `dadd`, `daddu`, `dsub`, `dsubu`, `addi`, `addiu`, `slti`, `sltiu`, `andi`, `ori`, `xori`, `lui`, `daddi`, `daddiu` |
| Shifts | `sll`, `srl`, `sra`, `sllv`, `srlv`, `srav`, `dsll`, `dsrl`, `dsra`, `dsll32`, `dsrl32`, `dsra32`, `dsllv`, `dsrlv`, `dsrav` |
| Multiply, divide, HI/LO | `mult`, `multu`, `div`, `divu`, `dmult`, `dmultu`, `ddiv`, `ddivu`, `mfhi`, `mflo`, `mthi`, `mtlo` |
| Assembler aliases | `move`, `not`, `negu`, `dnegu`, `li` |
| Loads and stores | `lb`, `lbu`, `lh`, `lhu`, `lw`, `lwu`, `ld`, `sb`, `sh`, `sw`, `sd`, `lwl`, `lwr`, `ldl`, `ldr`, `swl`, `swr`, `sdl`, `sdr`, `ll`, `lld`, `sc`, `scd` |
| Branches | `beq`, `bne`, `blez`, `bgtz`, `bltz`, `bgez`, `bltzal`, `bgezal`, `b`, `bal`, `beqz`, `bnez` |
| Branch-likely | `beql`, `bnel`, `blezl`, `bgtzl`, `bltzl`, `bgezl`, `bltzall`, `bgezall`, `beqzl`, `bnezl` |
| Jumps | `j`, `jal`, `jr`, `jalr` |
| Traps | `teq`, `tne`, `tge`, `tgeu`, `tlt`, `tltu`, `teqi`, `tnei`, `tgei`, `tgeiu`, `tlti`, `tltiu` |
| COP1 moves, loads, stores | `mfc1`, `mtc1`, `dmfc1`, `dmtc1`, `cfc1`, `ctc1`, `lwc1`, `swc1`, `ldc1`, `sdc1` |
| FPU arithmetic | `add_s`, `add_d`, `sub_s`, `sub_d`, `mul_s`, `mul_d`, `div_s`, `div_d`, `sqrt_s`, `sqrt_d`, `abs_s`, `abs_d`, `neg_s`, `neg_d`, `mov_s`, `mov_d` |
| FPU conversions | `cvt_s_d`, `cvt_s_w`, `cvt_s_l`, `cvt_d_s`, `cvt_d_w`, `cvt_d_l`, `cvt_w_s`, `cvt_w_d`, `cvt_l_s`, `cvt_l_d`; `round_`, `trunc_`, `ceil_` and `floor_` with `w_s`, `w_d`, `l_s`, `l_d` |
| FPU compares | `c_<cond>_s` and `c_<cond>_d` for `f`, `un`, `eq`, `ueq`, `olt`, `ult`, `ole`, `ule`, `sf`, `ngle`, `seq`, `ngl`, `lt`, `nge`, `le`, `ngt` |
| FPU branches | `bc1f`, `bc1t`, `bc1fl`, `bc1tl` |
| Rejected on the VR4300 | MIPS IV: `movz`, `movn`, `movz_s`, `movz_d`, `movn_s`, `movn_d`, `pref`, `madd_s`, `madd_d`, `msub_s`, `msub_d`, `nmadd_s`, `nmadd_d`, `nmsub_s`, `nmsub_d`, `recip_s`, `recip_d`, `rsqrt_s`, `rsqrt_d`. MIPS32: `clz`, `clo`, `mul`, `madd`, `maddu`, `msub`, `msubu`, `wait`, and the three-operand `mfc0`/`mtc0` select form. MIPS32r2: `ext`, `ins`, `rotr`, `rotrv`, `seb`, `seh`, `wsbh`, `synci`, `ehb`, `di`, `ei`, `rdhwr`, `jr_hb`, `jalr_hb`, `mfhc1`, `mthc1`. MIPS64: `dclz`, `dclo`. MIPS64r2: `dext`, `dins`, `drotr`, `drotr32`, `drotrv`, `dsbh`, `dshd`. |

The rejected group is gated behind its LLVM feature name, which the diagnostic
names. The VR4300 does not implement these instructions, so do not enable
those features for N64 code. LLVM's assembler would otherwise accept some of
them for `mips3`, such as `pref` and `wait`.

If a feature is enabled anyway, it must reach the code LLVM assembles. A
template is assembled as part of its caller, so the feature must be enabled
globally (`-target-features:"mips32r2"` or a matching `-microarch`) or on the
calling procedure with `@(enable_target_feature="mips32r2")`. A template that
only carries the attribute itself satisfies the table but not the assembler,
so `odin check` rejects a call to it from a procedure that lacks the feature.

Two-operand `div`, `divu`, `ddiv` and `ddivu` are LLVM assembler macros that
add a divide-by-zero trap and an `mflo` into the first operand. The backend
always emits the bare instruction, `div $zero, rs, rt`, which only writes HI
and LO.

`add`, `sub`, `dadd`, `dsub`, `addi` and `daddi` trap on signed overflow. Use
the `u` forms for wrapping arithmetic.

## Registers and effects

The checker tracks `%ra`, `%hi`, `%lo` and `%fcc0` as implicit registers:

| Register | Written by | Read by |
| --- | --- | --- |
| `%hi`, `%lo` | `mult`, `multu`, `div`, `divu` and their `d` forms; `mthi` writes `%hi`, `mtlo` writes `%lo` | `mfhi`, `mflo` |
| `%fcc0` | `c_<cond>_<fmt>`, `ctc1` | `bc1f`, `bc1t`, `bc1fl`, `bc1tl` |
| `%ra` | `jal`, `bal`, `bltzal`, `bgezal`, the branch-likely link forms, one-operand `jalr`, a literal `%ra` destination | A literal `%ra` operand |

A template that writes one of them clobbers it automatically, and each of
these writes defines the whole register. A read that no earlier instruction in
the template produced on every path is an error. For `%hi` and `%lo` the
diagnostic suggests pinning an input to supply a value from outside. `%fcc0`
cannot be pinned, so for `%fcc0` it ends "write %fcc0 first": produce it with
a compare or `ctc1` inside the template.

```odin
mul_lo :: asm(a: u32, b: u32) -> (r: u32) { multu a, b; mflo r }
```

Every other register is untracked. A template that writes a literal GPR must
cover it in one of these ways:

- pin it to an output or a scratch parameter;
- pin it to an input that is tied to an output, as in `[v -> r = %a1]`;
- declare it with `#clobber`.

An undeclared write is an error, for example `addu %t0, a, a` without
`#clobber %t0`:
`'addu' writes %t0, which is not pinned to an output or scratch parameter; add '#clobber %t0' so the compiler does not keep a live value in it`.
A write to a register pinned to an untied input is also an error. The check
covers GPRs and FPRs: `add_s %f4, a, b` without `#clobber %f4` is reported the
same way.

An input parameter written by name is an error too, unless it is tied to an
output: the compiler assumes an input keeps its value after the template.
For `addiu a, a, 1` with an untied input `a`, tie it to an output
(`a -> r`) or copy it to a scratch parameter and write that.

Writes to `%zero`, `%k0`, `%k1`, `%gp` and `%sp` are not checked and add no
clobber. A write to `%zero` is discarded. The others are reserved for the
kernel, the global pointer and the stack, so restore them before the
template ends.

The `rd` of `jalr rd, rs` is a write like any other: a literal `rd` needs a
pin or `#clobber`. `#clobber` every register the callee may change, as with
`call` on amd64.

```odin
double :: asm(v: u32) -> (r: u32) [v = %a1, r = %v1, #clobber %t0] {
	addu %t0, v, v
	addu r, %t0, %zero
}
```

A literal use of a pinned register counts as a use of its parameter, so
`%a1` above may be written instead of `v`.

COP0 instructions (`mfc0`, `mtc0`, `dmfc0`, `dmtc0`, `tlb*`, `eret`) and
`cache` change machine state the compiler cannot see. Templates that use them,
`sync`, `ll`/`sc` or `jalr` are volatile and clobber memory without a
declaration.
`#clobber %c0_status` and other COP0 or COP1-control clobbers are accepted;
LLVM cannot name those registers, so each lowers to a memory clobber.

## Local control flow and delay slots

Declare a label with `.name:` and refer to it as `.name`. Branch and jump
targets are template labels only; the backend emits them as numeric local
labels (`1:`, `1b`, `1f`).

- `b .label` and `j .label` are unconditional jumps inside the template.
- `jal`, `bal`, `bltzal` and `bgezal` link through `%ra`, which they clobber,
  and are modelled as calls that return.
- `jr rs` and `eret` leave the template. A template that uses them must be
  declared diverging (`-> !`) unless every path still reaches its end.
- `jalr rs` and `jalr rd, rs` call through a register and return. They write
  `%ra` or `rd`, make the template volatile and clobber memory, since the
  callee may read or write any of it.
- The VR4300 result of `jalr rd, rs` with `rd` equal to `rs` is unpredictable.
  An allocated `rd` is an early-clobber output (`=&r`), so it is never given
  the register of `rs`.
- A numeric or absolute target cannot be written. `j 0x80000400` is an
  operand-kind error.

LLVM assembles every template under `.set reorder`, and the template grammar
cannot change that. The assembler puts a `nop` in every branch and jump delay
slot. Write branches without a delay-slot instruction. The instruction after a
branch runs after the branch resolves, on the fall-through path only; an
explicit `nop` there is an extra instruction, not the slot.

```odin
count_down :: asm(n: u32) -> (r: u32) {
	move r, n
.top:
	addiu r, r, -1
	bne r, %zero, .top
}
```

The assembled loop is `addiu`, `bnez`, `nop`. Branch-likely forms are accepted,
but their slot also holds the assembler's `nop`, so they behave like the plain
branch.

### Hazards inside a template are not filled

The assembler fills delay slots only. These hazards stay the template
author's job:

- COP0 hazards after `mtc0` to Status, EntryHi, Index, EntryLo0/1, PageMask or
  Wired, and around `tlbr`, `tlbwi`, `tlbwr`, `tlbp` and `eret`;
- the HI/LO hazard: an `mfhi` or `mflo` followed within two instructions by
  `mult`, `div`, `mthi` or `mtlo`, when both are inside the template.

The HI/LO hazard cannot straddle a template boundary, because the template
does not see the code around it. The generator therefore pads a template
whose first two instructions write HI or LO with leading `nop`s, and one
whose last two instructions read HI or LO with trailing `nop`s.

Write the other `nop`s explicitly, as libdragon's `cop0.h` does. Its
`C0_WRITE_ENTRYHI` is `mtc0 %0,$10; nop; nop` and its `C0_TLBWI` is
`tlbwi; nop; nop; nop; nop`. Its `C0_WRITE_STATUS` is a bare `mtc0` that
leaves Status hazards to the caller.

```odin
write_entryhi :: asm(v: u32) {
	mtc0 v, %c0_entryhi
	nop
	nop
}
```

## Rejected forms

`odin check` reports these. An operand that fails its own check, such as an
unknown register, is not also reported as a form mismatch. Unknown mnemonics
and registers come with a did-you-mean suggestion.

- MIPS IV, MIPS32, MIPS32r2 and MIPS64(r2) instructions, including the
  `mfc0`/`mtc0` select form, unless their LLVM feature is enabled.
- `%at` in any position.
- Absolute or numeric branch and jump targets.
- Immediates outside their slot's range, `cache` operations the VR4300 does
  not implement, and memory displacements outside -32768–32767.
- A 64-bit parameter in a 32-bit GPR slot.
- A register of the wrong class: a GPR in an FPR slot; a COP0, COP1-control,
  FPR, `%hi`, `%lo` or `%fcc0` name in a GPR slot; a GPR, parameter or
  register of the other control class in a COP0 or COP1-control slot; or a
  parameter of the wrong FPU width.
- A write to a literal GPR or FPR that the template does not declare, to a
  register pinned to an untied input, or to an untied input parameter.
- A read of `%ra`, `%hi`, `%lo` or `%fcc0` that nothing in the template
  produced.
- A template with no path to its end that is not declared `-> !`.
- Output pins to `%zero`, `%k0`, `%k1`, `%gp` or `%sp`, and any pin to a
  COP0, COP1-control or `%fcc0` register.
- A call to a template that enables a target feature (see below) from a
  procedure that does not enable it.

An error LLVM's assembler reports for a template body fails `odin build`
with a non-zero exit; the object is never silently written without the
template.

## Inspect the output

Write LLVM IR to see each template's asm string and constraints:

```sh
odin build . -target:n64 -build-mode:llvm-ir -use-single-module
```

Literal registers appear as `$$N`, operands as `$N`, and every constraint
string ends with `~{$1}`:

```text
call void asm sideeffect "\09cache 21, 0($0)", "r,~{memory},~{$1}"(ptr %7)
%0 = call i32 asm sideeffect "\09mfc0 $0, $$9", "=r,~{memory},~{$1}"()
```

Write an object file and disassemble it with the SDK's `objdump` to see the
filled delay slots and final encodings:

```sh
odin build . -target:n64 -build-mode:obj -o:speed -use-single-module -out:out.o
"$N64_INST/bin/mips64-elf-objdump" -d out.o
```

Neither mode runs the ROM packager. Add `-no-entry-point` for a package without
`main`.

## Validation

The quick validation stage "CPU asm templates" checks the templates without an
SDK or emulator:

```sh
python3 tests/n64_asm/test_n64_asm.py
```

It builds the [fixture](tests/n64_asm/fixture/fixture.odin) and a
[coverage package](tests/n64_asm/coverage/coverage.odin) with one exported
procedure per VR4300 mnemonic for both MIPS targets, and checks every
instruction word in the object files; a test reads the table's mnemonic list
so an instruction without a coverage procedure fails. Integer branch forms
also run through a small MIPS interpreter, taken and not taken, with delay
slots. It checks that every IR constraint string clobbers `$1`, the inferred
volatile and memory effects, numeric pins and clobbers, HI/LO and `%fcc0`
clobbers, and numeric local labels; one test per MIPS-specific trap lives in
[regress](tests/n64_asm/regress/regress.odin). It also checks that `odin check`
rejects every [checker case](tests/n64_asm/bad_check/bad_check.odin) and every
[later-ISA mnemonic](tests/n64_asm/bad_features/bad_features.odin), with the
message on the template's own line and no other errors. When `N64_INST` or
`MIPS_O64_OBJDUMP` is set the coverage object is also cross-checked with the
SDK's objdump. Set `ODIN` to select a compiler other than the repository's
`odin`. The object checks need the MIPS O64 LLVM fork, which writes ELF32
objects. With stock LLVM they skip, as in CI, and every IR, constraint and
rejection check still runs. With `N64_INST` or `MIPS_O64_OBJDUMP` set, or in
full mode, they fail instead of skipping.

The [runtime ROM](tests/n64_asm/rom/rom.odin) exercises the templates on the
console. It writes a hand-assembled MIPS leaf function into cached RAM, writes
back the D-cache and invalidates the I-cache with Odin versions of libdragon's
`data_cache_hit_writeback_invalidate` and `inst_cache_hit_invalidate`, and
calls the leaf. It then rewrites the same buffer with another constant while
the old code is still in the I-cache, and calls it again. The new result
depends on the I-cache invalidate as well as the D-cache writeback: under ares,
dropping the invalidate runs the stale code and fails `ICACHE_REFRESH`, and
dropping the writeback crashes the first call. A second buffer with another
constant is the negative control.
CP0 Count, read with `mfc0`, must advance across the run. Build and run it with
an SDK and `ares-test`:

```sh
./odin build tests/n64_asm/rom -target:n64 -out:asm.z64
"$ARES_TEST" tests/n64_asm/rom.test.js asm.z64 --timeout 30
```

The script requires the ordered `ODIN_N64_ASM_CHECK` sentinels and the final
`ODIN_N64_ASM_PASS:v1`. Both layers run from
[tests/n64_validate.py](tests/n64_validate.py). Quick mode runs the encoding
suite and the "asm template ROM probe" type-check of the ROM. Full mode adds
"build asm template ROM" and "asm template ROM run". See
[validation layers](N64_MAINTAINERS.md#validation-layers).
