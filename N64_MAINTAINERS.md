# Maintaining Odin's Nintendo 64 target

This guide is the routine maintenance and release reference for the N64 target.
The user-facing contract is in [`N64_BUILD.md`](N64_BUILD.md); the raw binding
contract is in
[Odin64's binding guide](https://github.com/soft-circles/Odin64/blob/main/libdragon/README.md). Historical plans
and ledgers explain past decisions but are not required to navigate the current
implementation.

## Architecture map

The build flows through these owned seams:

1. [`src/main.cpp`](src/main.cpp) registers and validates N64 CLI options.
2. [`src/build_settings.cpp`](src/build_settings.cpp) selects the big-endian
   MIPS/O64 `n64` target and disables TLS for its single-threaded runtime.
3. [`src/linker.cpp`](src/linker.cpp) reads `N64_INST` only when an explicit
   `-n64-inst` was not supplied, then falls back to `<odin root>/n64` when that
   directory exists. It defaults `-n64-title` to the main package's directory
   name, translates compiler state into a complete `N64PrepareBuildRequest`,
   and calls `n64_prepare_build`.
4. Odin compiles the application and the target-tagged runtime files in
   [`base/runtime`](base/runtime). [`entry_n64.odin`](base/runtime/entry_n64.odin)
   installs the context and exposes the C-ABI `main` required by libdragon.
5. The linker adapter flattens compiled object paths and declared foreign
   libraries into an `N64BuildRequest` without exposing linker entities.
6. [`src/n64_build.cpp`](src/n64_build.cpp) validates the SDK, stages inputs,
   writes the private Makefile, starts a sanitized `/usr/bin/make` process,
   retains failures, and atomically places the completed ROM.
7. The selected libdragon `n64.mk` owns final static linking, symbols, stripping,
   compression, DragonFS, header configuration, and extended metadata.

Target selection and option parsing stay in their existing compiler modules.
N64 SDK availability checks, staging, generated graph details, subprocess policy,
cleanup, and ROM placement belong in `src/n64_build.cpp`. General linker code
should contain only translation adapters. Runtime startup and allocation belong
in target-tagged `base/runtime/*_n64.odin` files. Bound C declarations and their
ABI assertions belong in the independent Odin64 repository's `libdragon/` and
`tests/abi/libdragon/`, not this compiler's vendor collection.

CPU `asm` templates for `TargetArch_mips32be` sit beside that flow:

- [`src/asm_tables_mips.cpp`](src/asm_tables_mips.cpp) is the MIPS III
  (VR4300) instruction table: mnemonics, registers, operand types, form rows
  and clobber records. Its header comment is the reference for the jump,
  delay-slot, hazard, `$at` and HI/LO policies.
- [`src/check_asm.cpp`](src/check_asm.cpp) and
  [`src/check_asm_cfg.cpp`](src/check_asm_cfg.cpp) are the generic checker. The
  MIPS table supplies slot ranges and the literal-write policy through its
  optional table hooks.
- `lbAsmGenerate_mips` in [`src/llvm_backend_asm.cpp`](src/llvm_backend_asm.cpp)
  writes the LLVM inline-asm string and constraint list.

The user contract is [`MIPS_ASM.md`](MIPS_ASM.md).

## Interfaces

The external interface is deliberately small:

```text
odin build <package> -target:n64 [N64 options]
```

Applications provide ordinary Odin source and `main :: proc()`. Preserve
option names, `-n64-inst` precedence over `N64_INST`, output naming, retained
intermediate behavior, and atomic placement. Do not require a project Makefile,
C entry point, or new manifest.

The internal N64 module exposes two request/result operations:

```text
n64_prepare_build(N64PrepareBuildRequest) -> N64PrepareBuildResult
n64_package_rom(N64BuildRequest) -> N64BuildResult
```

`N64PrepareBuildRequest` contains already-parsed command, mode, relocation,
linker, and ROM settings. `N64BuildRequest` contains those settings plus final
output paths, compiled object paths, and flattened foreign libraries. The
module must not read `build_context`, `LinkerData`, or `Entity`. Keep staging
names, metadata discovery, Makefile syntax, process details, and cleanup order
private to it.

The architecture regression in
[`tests/n64_build/test_n64_module.py`](tests/n64_build/test_n64_module.py)
guards that boundary.

## SDK validation and environment threat model

Treat the selected SDK root and the parent process environment as inputs, not
authority.

Hard SDK gates are:

- all required headers, libraries, linker script, and packaging tools exist;
- required tools are executable;
- `mkdfs` is present when assets are requested;
- `n64metadata` is present when metadata is requested.

The compiler does not inspect revision, clean-state, recipe-hash or toolchain
version metadata. SDK compatibility is exercised by compilation, packaging
and the ABI/runtime tests.

The packaging process removes inherited `PATH`, `SHELL`, make recursion and
override variables, `CCACHE`/`CCACHE_*`, `V`, `D`, and every `N64_*` variable.
It supplies a fixed system `PATH` and shell. The generated Makefile overrides
the SDK, target, tool prefix, output paths, header inputs, cache, and verbosity.
The process uses `posix_spawn` with a fixed argument vector rather than a shell
command. These controls prevent an inherited make flag, compiler wrapper, or
toolchain prefix from escaping the SDK that was validated.

Preserve these guarantees when adding a tool or setting. A new environment
input needs an explicit trust decision and a regression proving it cannot
override a validated executable or hide a failed stage.

## Generated stage and cleanup guarantees

Every executable build creates a mode-0700 directory beside the requested ROM:

```text
.odin-n64-build-XXXXXX/
├── Makefile
├── sdk -> <validated SDK>
├── assets -> <requested raw directory>       # only with -n64-assets
├── metadata/                                 # only with -n64-metadata
│   ├── odin-input-0000.ini
│   └── <referenced companion roots> -> ...
├── odin-0000.o ...
├── foreign-0000.o/.a ...
├── odin-n64.z64                              # before final placement
└── build/
    ├── odin-n64.elf
    ├── odin-n64.map
    ├── odin-n64.elf.sym
    ├── odin-n64.elf.stripped
    └── odin-n64.dfs                          # when assets are present
```

Compiler object paths are sorted before staging because their original hash-map
order is not stable between processes. Foreign `.o` and `.a` inputs keep their
declared order. Do not weaken deterministic naming or input ordering without
comparing ELF, symbol, and ROM hashes.

After make succeeds, `rename` places the staged ROM at the final path. Only
then may successful intermediates be removed. `-keep-temp-files` retains the
entire graph. Validation failures before stage creation print direct SDK or
option diagnostics; staging, packaging, and placement failures retain the
useful stage and print its path. Cleanup failures are warnings and name the
remaining directory.

## Metadata companion staging

The source INI is copied under a collision-free `odin-input-NNNN.ini` name.
The scanner recognizes localized forms of `[meta]`, `[boxart]`, and `[cartart]`.
It selects:

- comma-separated `screenshots` entries and `long-desc` in meta sections;
- `front`, `back`, `top`, `bottom`, `left`, and `right` in art sections.

References must be relative and may not contain an empty, `.` or `..` path
component. Only each reference's first component is linked into the metadata
stage, which preserves subpaths without copying unrelated siblings. Keep the
selection algorithm aligned with the pinned `n64metadata` semantics and add a
public-build regression for each new field or section.

## Runtime, allocators, and callback context

The C-ABI entry procedure creates `default_context()`, installs the N64 heap
allocator, initializes the fixed temporary arena, runs Odin startup, calls the
language entry point, and runs cleanup. The heap allocator delegates to the
pinned C runtime's `malloc`, `realloc`, and `free`, with Odin-side zeroing for
ordinary allocations.

The temporary allocator has 256 KiB by default and is configured at compile
time by `N64_TEMP_ARENA_SIZE`. It supports aligned allocation, zeroing,
grow/shrink resize, feature queries, and bulk reset. It does not support
individual frees. Both context allocator fields may be replaced by an
application; tests must keep covering replacement and allocation during
`@(init)`.

`core:mem` is supported on N64 except for its `core:sync`-backed allocators
(`Mutex_Allocator` and `Tracking_Allocator`), which carry `#+build !n64`
because `core:sync` has no N64 futex or thread-identity primitives. Keep those
tags when merging upstream; [tests/n64_core_mem](tests/n64_core_mem) checks
that `core:mem` still type-checks for the target.

N64 is single-threaded and disables TLS. There is no supported mechanism for a
C callback, interrupt, or timer entry to install an Odin context. Do not expose
callback-taking APIs until their context, reentrancy, stack, allocator, and
failure rules have a separate design and hardware validation.

## CPU asm templates

The amd64, arm64 and riscv64 tables are generated from `core:rexcode`. The MIPS
table is hand-written: upstream Odin has no MIPS target, and rexcode's MIPS ISA
has no clobber semantics. It keeps the generated riscv64 table's sections,
enums, `Encoding`/`Clobber` records and `AsmCtx` interface, so a generated
table can replace it without touching the checker or backend.

To add an instruction:

1. Add an `X(ENUM, "spelling")` row to `ASM_MIPS_MNEMONICS` in its group. FPU
   spellings use `_` for `.`.
2. Add one `R(...)` form row per operand shape, adjacent to the mnemonic's
   other rows and in mnemonic order. A row gives the shape, fixed encoding bits
   and mask, feature and `Clobber` record. Add a shape or a `MIPS_C_*` clobber
   macro only when no existing one fits. `init` asserts the ordering and that
   every mnemonic has a form.
3. Use a VR4300 feature (`MIPS_I`, `MIPS_II`, `MIPS_III`, `COP0`, `FPU`,
   `MACRO`) only for instructions the VR4300 implements. Anything else takes
   its LLVM feature (`MIPS4`, `MIPS32`, ...), which keeps it rejected unless
   that feature is enabled.
4. Add an exported proc to [tests/n64_asm/coverage](tests/n64_asm/coverage)
   with the instruction's word in the test's encoding table; the suite reads
   `ASM_MIPS_MNEMONICS` and fails when a mnemonic has no coverage proc. Add
   constraint expectations when it has implicit registers or effects, a case
   to `regress/` if it touches a MIPS-specific trap, and rejected forms to
   `bad_check/` (checker), `bad_features/` (later-ISA) or `bad_backend/`
   (wrong-class literals and pins the generator also rejects). Integer branch forms also run through the test's small
   MIPS interpreter, which checks taken and not-taken results with delay slots.

Preserve these guarantees:

- Every MIPS template clobbers `~{$1}`, and `%at` is not in the register table.
  LLVM allocates `$at` to operands while assembler macros expand through it.
- Pins and clobbers use numeric constraints: pins `{$4}`, `{hi}`, `{lo}`;
  clobbers `~{$8}`, `~{$f4}`, `~{$fcc0}`, `~{hi}`, `~{lo}`. A named GPR
  constraint such as `{$t1}` asserts in LLVM's Mips backend. FPR pins lower
  to `{$f12}`-style constraints.
- Literal registers are written `$$N`/`$$fN`; `$N` is an operand reference.
- Labels are numeric locals (`N:`, `Nb`, `Nf`). Branch and jump targets are
  template labels only.
- Delay slots belong to the assembler under `.set reorder`. Nothing in the
  table or backend inserts or expects an explicit slot instruction. Hazards
  inside a template are not filled; the generator only pads HI/LO hazards that
  would straddle the template boundary.
- `%ra`, `%hi`, `%lo` and `%fcc0` stay tracked: their writes clobber
  automatically and unproduced reads are errors.
  `implicit_register_writes_are_full_width` makes each implicit write define
  the whole register.
- Slot ranges come from `memory_disp_range` and `immediate_range`, and
  `operand_value_reject_reason` limits `cache` to VR4300 operations.
- The 32-bit GPR slots (`OP_GPR32`) reject 64-bit parameters.
  `literal_register_width_agnostic` exempts literal registers from that check.
- `register_write_needs_clobber` requires a pin or `#clobber` for a literal
  write to an allocatable GPR or FPR. `%ra` is tracked instead, and
  `$zero`, `$k0`, `$k1`, `$gp` and `$sp` are exempt.
- `jalr` reads and writes memory in its clobber record, and
  `destination_must_differ_from_sources` makes the generator emit `=&`
  outputs so `rd` never equals `rs`.
- `#clobber` of a COP0 or COP1-control register lowers to `~{memory}`.
  `pin_reject_reason` makes `odin check` reject pins to those registers and
  to `%fcc0`; the generator keeps the same checks as a backstop.
- Two-operand `div`/`divu`/`ddiv`/`ddivu` are emitted as `div $zero, rs, rt`.
- Feature gating stays: `feature_name_from_form` names no feature for VR4300
  forms and the LLVM feature for later-ISA forms, which the checker rejects
  unless the build or the template enables that feature. Because
  `template_features_bind_to_caller` is true, a call to a feature-enabling
  template is rejected unless the calling procedure or the build enables it.

Table hooks the generic checker asks for (all optional, SFINAE-detected in
`check_asm.cpp`; a table without one keeps the old behaviour): `register_is_float`
types a literal FPR as `f32`/`f64`; `literal_register_reject_reason` and
`pin_reject_reason` reject wrong-class literals and impossible pins at check
time; `register_unavailable_reason` explains `%at`; `template_features_bind_to_caller`
makes a call to a feature-enabling template require the feature on the caller
or the build; `pinned_integer_operand_min_bits` on the generator widens narrow
pinned operands to 32 bits, since LLVM's MIPS backend asserts on an i8/i16
explicit-register constraint.

## Setup defaults

[Odin64's toolchain.lock.toml](https://github.com/soft-circles/Odin64/blob/main/toolchain.lock.toml)
records the default Odin/LLVM, libdragon and runner revisions used for setup.
The compiler has no duplicate SDK pin constants or lock-drift gate. When changing
those defaults, review affected bindings and run compiler and C/Odin ABI tests
with the selected installation. Keep historical test evidence unchanged.

## Validation layers

[tests/n64_validate.py](tests/n64_validate.py) runs compiler-owned checks only.
Use `--list` to inspect stages and `--artifacts <parent>` for unique retained
logs. It never builds LLVM or installs dependencies.
Build Odin separately using an explicit compatible `LLVM_CONFIG`.

### Quick: no SDK or emulator required

```sh
python3 tests/n64_validate.py quick
```

Checks cover local documentation links, validation contracts, module
boundaries, public options/failures, SDK-validator behavior and compilation of
[tests/n64_runtime](tests/n64_runtime) and [tests/n64_core_mem](tests/n64_core_mem).
The "CPU asm templates" stage runs
[tests/n64_asm/test_n64_asm.py](tests/n64_asm/test_n64_asm.py). With any LLVM it
checks the LLVM IR asm strings and constraints, checker and generator
rejections, target-feature diagnostics and the known-bug reproductions.
Instruction words are read from ELF32 objects, which only the MIPS O64 LLVM
fork writes. With stock LLVM, which writes an ELF64 n64-ABI object, the object
tests skip with "compiler emits ELF64: not the MIPS O64 LLVM fork". This
covers the encodings, the branch interpreter, delay slots, labels and the
`ext` encodings in the target-feature tests. CI builds Odin with stock LLVM, so
CI does not check encodings; run the suite with a fork build before merging
template changes. When `N64_INST` or `MIPS_O64_OBJDUMP` is set, or in full
mode, an ELF64 object fails these tests instead of skipping them. The "asm
template ROM probe" stage type-checks [tests/n64_asm/rom](tests/n64_asm/rom).
No project-local binding is required.

### Full: compiler SDK and runtime checks

```sh
N64_INST=/absolute/sdk ARES_TEST=/absolute/ares-test \\
  python3 tests/n64_validate.py full
```

Missing tools fail instead of skipping. Full mode repeats quick checks, validates
the SDK, runs public build/packaging tests, the Odin/GCC differential, linked O64
ABI ROM, and standalone runtime lifecycle. It then builds the asm template ROM
and runs [tests/n64_asm/rom.test.js](tests/n64_asm/rom.test.js), which checks
cache writeback/invalidate, a call into generated code, a call after rewriting
that code in place, which fails if the I-cache is not invalidated, its negative
control and CP0 Count. Fixture logs are retained on failure.
Runner provisioning is documented in [tests/o64_abi](tests/o64_abi).

The separate [Odin64 driver](https://github.com/soft-circles/Odin64#verify)
owns Linux/AMD64/container and runner-source gates, the LLVM/GCC differential,
binding ABI, console lifecycle, and canonical tracer/Pong/DFS goldens. It rebuilds
Odin only, records new hashes, and is not release or hardware certification.
The compiler CI must remain usable without that repository.

## Scope boundaries

Compiler ownership includes N64 target selection, runtime, packaging and generic
O64 regressions. Bindings, games, assets, engine API, goldens and cross-repository
qualification belong to Odin64. LLVM backend changes belong to the independent
LLVM fork. Record all repository states separately for coordinated changes and
preserve unrelated work. Reuse compatible builds; an integration task is not
implicit permission to recompile LLVM.
