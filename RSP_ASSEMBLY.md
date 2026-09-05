# Handwritten RSP queue commands

## Dependencies

- Pinned libdragon SDK: use the [N64 setup](N64_BUILD.md#install-the-pinned-libdragon-sdk).
- Odin64 integration: use the engine repository's `tests/n64/rspq` fixture for
  packaging, ABI and bounded queue execution checks.

Compile a separate source unit to an inspectable assembly artifact:

```sh
odin build command.odin -file -build-mode:rsp-asm -rsp-entry:command -out:rsp_command.S
```

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

The entire unit may contain one selected parameterless/resultless assembly
command and named integer constants. Runtime declarations, imports, raw directives,
other templates, CPU calls and captures are rejected. Integer constants support
literals, references, parentheses, unary `+`, `-`, `~`, and binary `+`, `-`, `*`,
`&`, `|`, `~` and `&~`. All declaration metadata is checked at its source location.
`rspq_command_words` is required in 1–62, including the first word's low-24-bit
payload. `rspq_scratch_bytes` defaults to zero; otherwise it is a multiple of 16
and at most 4096. Actual available DMEM is smaller because the queue, header,
empty saved state and alignment also occupy it; the SDK link and artifact check
must pass before execution.

## Supported instructions

- `addu`, `and`, `or`, `xor`: destination GPR and two source GPRs.
- `addiu`: destination, source, signed-16 immediate; `andi` and `ori` use unsigned-16
  immediates. `lui` takes destination and unsigned-16 immediate.
- `li`: destination and a signed/unsigned 32-bit constant. Expands to one `ori` or
  `addiu` when possible, otherwise `lui` followed by `ori`; only the destination is
  written. `la destination, rspq_scratch` expands to one `ori` with a checked local
  relocation, without using `at`.
- `lw`/`sw`: GPR and `[base + displacement]`, with a signed-16 displacement. Memory
  must derive from the declared scratch symbol. Alignment, bounds and initialized
  scratch reads are checked. No scale, index register, segment or type decoration.
- `vxor`: three whole vectors. Reads all elements of both sources, writes the full
  destination and accumulator. No accumulator-reading or flag/divider operations
  are supported.
- `mtc2`/`mfc2`: scalar GPR, then vector with `.e0`–`.e7`. Transfers one 16-bit
  element; `mfc2` sign-extends. Byte selectors and broadcast forms are rejected.
- `beq`/`bne`: two GPRs and `.label`; `j .label`; `jr %ra`; `j DMAOut`; `nop`.

Registers use `%r0`–`%r31` and `%v00`–`%v31`. Unambiguous GPR ABI aliases are
accepted, including `%ra` and `%s8`/`%fp`. `%v0`/`%v1` are not scalar aliases.
Stack-pointer use and writes to queue `gp`/`ra` are rejected regardless of spelling.
Only declared input words (`a0`–`a3`), command byte count (`t7`), continuation state,
architectural zero and queue vectors `v00`, `v30`, `v31` start defined.

Labels use `.name:`. Every transfer has an explicit one-instruction delay slot;
a transfer or two-instruction pseudo in that slot is rejected. Effects occur on
both conditional outcomes before their control-flow join. Branches cannot target
slots or fall off the command. Loops are allowed; termination is not proven.
Scheduling is explicit: the checker does not insert hardware dependency spacing.

`DMAOut` is a tail transfer through the pinned synchronous helper. Initialize
`t0` to byte length minus one, `t1` to pitch (ignored for this single-row profile),
`s0` to the destination physical address, and `s4` to scratch. The transfer must
be a known multiple of eight, aligned, within scratch, and fully initialized.
The helper clobbers `t2`, `at` and `s4`, then returns through inherited `ra`.
CPU code owns destination validation, cache discipline, lifetime and bounded waits.

The generated wrapper includes the SDK queue first, preserves its boot entry,
and emits one index-zero descriptor, terminator and nonempty SDK empty-state
macro. Source `#line` records accompany emitted instructions. GNU preprocessing,
relocation completion and linked IMEM/DMEM limits remain required downstream
checks; successful text emission alone is not execution or release qualification.

Run the public CLI regression suite with:

```sh
python3 tests/rsp_asm/test_rsp_asm.py
```

SDK encoding tests require the installed SDK and use reviewed instruction bitfields.
The scalar, vector and branch fixtures are compiler-owned source examples; the
engine owns their ROM execution and independent reference comparison.
