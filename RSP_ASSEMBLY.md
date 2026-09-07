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
payload. `rspq_scratch_bytes` defaults to zero and accepts an explicit compile-time zero.
Nonzero scratch is unsupported in this minimal profile.

## Supported instructions

The complete body must be exactly `jr %ra` followed by `nop`. `%r31` is the
canonical spelling of the same queue return register. Both produce the words
`0x03e00008` and `0x00000000`; the no-op explicitly occupies the delay slot.
No other instructions, labels, meaningful delay slots, vector operations, memory
accesses, scratch symbols, DMA helpers or assembly directives are accepted.

GPR names resolve as `%r0`–`%r31` or unambiguous ABI aliases (`zero`, `at`,
`a0`–`a3`, `t0`–`t9`, `s0`–`s8`, `k0`, `k1`, `gp`, `sp`, `fp`, `ra`).
Only register 31 is valid for the supported return. `%v0`/`%v1` are ambiguous
and rejected, as are vector registers, unknown names and element selectors.
Aliases cannot change a register's identity or bypass queue restrictions.

CPU compilation rejects RSP metadata even when the template body would be valid
CPU assembly. A wrapped RSP object is imported as CPU data through the SDK;
the template itself cannot be called or have its address taken from Odin.

The generated wrapper includes the SDK queue first, preserves its boot entry,
and emits one index-zero descriptor, terminator and nonempty SDK empty-state
macro. Source `#line` records accompany emitted instructions. GNU preprocessing,
relocation completion and linked IMEM/DMEM limits remain required downstream
checks; successful text emission alone is not execution or release qualification.

Run the public CLI regression suite with:

```sh
python3 tests/rsp_asm/test_rsp_asm.py
```

SDK tests assemble and link the minimal command, compare its words and common
sections against an independently authored reference, inspect the descriptor,
terminator and saved-state layout, and package it through the SDK's `n64.mk` rule.
Set `N64_INST` to select the SDK. Without an installed SDK these tests explicitly
skip; full validation requires it. ROM execution qualification belongs to the
Odin64 integration tickets.
