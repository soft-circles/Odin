# Handwritten RSP queue commands

## Dependencies

- Pinned libdragon SDK: use the [N64 setup](N64_BUILD.md#install-the-pinned-libdragon-sdk).
  The queue and synchronous `DMAOut` contracts come from its `rsp_queue.inc` and
  `rsp_dma.inc`. Set `N64_INST` to select this SDK for tests.
- Odin64 integration: use the engine repository's `tests/n64/rspq` fixtures for
  packaging, ABI and bounded queue execution checks.

Compile a separate source unit to an inspectable assembly artifact:

```sh
odin build command.odin -file -build-mode:rsp-asm -rsp-entry:command -out:rsp_command.S
```

The [scalar fixture](tests/rsp_asm/scalar.odin) consumes four command words:
first-word payload, `x`, `y`, and a physical RDRAM output address. It writes the
32-bit wrapping sum, the low 16 bits of `x XOR y`, and two zero words to scratch,
then transfers all 16 bytes through `DMAOut`. This scalar slice does not implement
the later vector-command milestone. A command that does no work remains valid:

```odin
package overlay

@(rspq_command_words=1)
command :: asm() {
	jr %ra
	nop
}
```

This mode parses Odin source and checks a restricted RSP dialect. It bypasses the
CPU checker, runtime dependencies and LLVM generation. It produces assembly only;
the SDK must assemble/link/wrap it before ordinary CPU code imports the wrapped
object as data. The output basename must begin with `rsp` and end in `.S`.
A failed check leaves any previous complete artifact unchanged and exits nonzero.
Only consume output after a successful invocation.

## Source and metadata

The entire unit may contain one selected parameterless/resultless assembly
command and named integer constants. Runtime declarations, imports, raw directives,
other templates, CPU calls and captures are rejected. Integer constants support
literals, references, parentheses, unary `+`, `-`, `~`, and binary `+`, `-`, `*`,
`&`, `|`, `~` and `&~`. Parenthesize compound expressions used as instruction
operands. All metadata is checked at its source location.

`rspq_command_words` is required in 1–62, including the first word. Only its low
24 bits are payload; its high byte contains queue opcode information.
`rspq_scratch_bytes` defaults to zero. Nonzero sizes must be multiples of 16,
up to 4096. They declare one 16-byte-aligned `.bss` region and the reserved symbol
`rspq_scratch`. Zero declares no symbol. Scratch has unknown contents on every
invocation and is not persistent saved state. User sections are not supported.
The SDK common data, header, empty saved state and alignment also consume DMEM;
the final linker budget therefore permits less than 4096 bytes of scratch.

## Supported instructions

Every operand shown is required; two-operand arithmetic aliases are unsupported.
`d`, `s`, `t` and `base` below are explicit GPRs. All scalar values are 32 bits.

| Source form | Meaning and checked effects |
| --- | --- |
| `addu d, s, t` | Read `s,t`, write their wrapping sum to `d`. |
| `addiu d, s, imm` | Read `s`, add a signed immediate in -32768–32767, write `d`. |
| `and d, s, t`, `or d, s, t`, `xor d, s, t` | Read both sources, write the bitwise result. |
| `andi d, s, imm`, `ori d, s, imm`, `xori d, s, imm` | Zero-extend an immediate in 0–65535, read `s`, write `d`. |
| `lui d, imm` | Immediate in 0–65535; write `imm << 16` to `d`. |
| `li d, constant` | Constant in -2147483648–4294967295; finite expansion below. |
| `move d, s` | Exactly `addu d, s, zero`; read `s`, write `d`. |
| `la d, rspq_scratch` | Exactly `ori d, zero, %lo(rspq_scratch)`; write a checked local scratch address. Requires declared scratch. |
| `lw d, [base + constant]` | Read an initialized scratch word and write `d`. |
| `sw s, [base + constant]` | Read `s`, initialize one scratch word. |
| `nop` | No effects; may also separate scalar instructions. |
| `jr %ra` | Return through inherited `r31`; requires a final explicit `nop`. |
| `j DMAOut` | Synchronous helper tail transfer under the contract below; requires a final explicit `nop`. |

`li` emits `addiu d, zero, constant` for -32768–32767, otherwise
`ori d, zero, constant` for 0–65535, otherwise exactly two instructions:
`lui d, upper16; ori d, d, lower16`. The upper/lower halves come from the
validated 32-bit bit pattern. It writes only `d`, with no hidden `at` scratch.
The other admitted instructions emit one real instruction each. The generated
body disables assembler reordering, implicit `at` use and multi-instruction
macro expansion. No invalid operand is truncated to fit an encoding.

GPR names resolve as `%r0`–`%r31` or unambiguous ABI aliases (`zero`, `at`,
`a0`–`a3`, `t0`–`t9`, `s0`–`s8`, `k0`, `k1`, `gp`, `sp`, `fp`, `ra`).
`%v0`/`%v1` are ambiguous and rejected, as are vector registers, unknown names
and element selectors. Aliases cannot bypass queue restrictions.

The only initial scalar definitions are zero, the declared first four words in
`a0`–`a3`, the byte command size in `t7`, and inherited `gp`/`ra` state. Sizes
above four words do not seed additional registers; fetching later words is not
in this slice. Reading another register before writing it is an error, including
when writing to zero. Writes to zero have no effect. All writes to `gp`/`r28` or
`ra`/`r31` and every use of `sp`/`r29` are rejected.

## Scratch memory and DMA

Memory operands accept `[base]`, `[base + constant]` and `[base - constant]`.
The resulting displacement must fit -32768–32767. Segments, register indexes,
scales, extra displacements and type suffixes are rejected. The base must retain
a checked scratch address: `la`, `move`, word stores/loads, and addition of a
known constant preserve this information. Addition retains scratch offsets only
within -4096–4096; other arithmetic loses the address proof. Each effective word
address must be 4-byte aligned and entirely inside the declared region. Numeric
addresses, unknown bases and CPU symbols cannot substitute for scratch.
Every word read by `lw` or DMA must have been initialized by an earlier `sw`.

`j DMAOut` is the sole direct target. Its required inputs must be defined before
the jump, with this restricted single-row transfer contract:

| Register | Requirement |
| --- | --- |
| `t0` / `r8` | Known byte count minus one: 8–4096 bytes, a multiple of 8. Height/skip encoding is not accepted here. Use 15 for the four-word result. |
| `t1` / `r9` | Explicitly defined pitch (normally zero). The helper reads it even though hardware ignores pitch for one row. |
| `s0` / `r16` | Physical RDRAM destination. Known constants must fit 24 bits and be 8-byte aligned; a scratch pointer is rejected. |
| `s4` / `r20` | Known 8-byte-aligned scratch address whose full transfer extent fits the declared region and is initialized. |
| `gp`, `ra` | Unmodified inherited queue state and continuation. |

The pinned helper destroys `at`/`r1` and `t2`/`r10`, adjusts `s4`, touches the
DMA/status coprocessor state, waits for completion and returns through inherited
`ra`. The checker marks those GPR results unknown and permits no authored code
after the tail transfer and its no-op slot. No stack or alternate return address
is introduced. For a runtime RDRAM destination, alignment, bounds, lifetime and
CPU cache ownership remain the caller's responsibility; emission does not prove
general computed-address safety.

Both return forms must end the command with an explicit `nop`. All scalar work
and helper setup precede the transfer. Meaningful slots, general branches, labels,
arbitrary calls, indirect targets, halt/break completion, vector/FPU operations,
HI/LO multiply/divide, 64-bit operations and other COP0 operations are unsupported.
Instruction scheduling remains explicit; no automatic scheduler is provided.

## Validation

CPU compilation rejects RSP metadata even on otherwise valid CPU instructions.
A wrapped RSP object is imported as CPU data through the SDK; the template itself
cannot be called or have its address taken from Odin.

The generated wrapper includes the SDK queue first, preserves its boot entry,
and emits one index-zero descriptor, terminator and nonempty SDK empty-state
macro. Source `#line` records accompany emitted instructions. GNU preprocessing,
relocation completion and linked IMEM/DMEM limits remain required downstream
checks; successful text emission alone is not execution or release qualification.

Run the public CLI regression suite with:

```sh
python3 tests/rsp_asm/test_rsp_asm.py
```

SDK tests compare generated and independent scalar words, relocations, common
sections and scratch alignment under default, release and profiling settings.
They check the minimal descriptor/empty-state layout and SDK wrapper rule,
exercise every admitted scalar form, and reject linked IMEM/DMEM overflow.
Without an installed SDK these tests explicitly skip; full validation requires it.
ROM execution qualification belongs to the Odin64 integration tickets.
