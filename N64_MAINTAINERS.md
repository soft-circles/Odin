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
   `-n64-inst` was not supplied, translates compiler state into a complete
   `N64PrepareBuildRequest`, and calls `n64_prepare_build`.
4. Odin compiles the application and the target-tagged runtime files in
   [`base/runtime`](base/runtime). [`entry_n64.odin`](base/runtime/entry_n64.odin)
   installs the context and exposes the C-ABI `main` required by libdragon.
5. The linker adapter flattens compiled object paths and declared foreign
   libraries into an `N64BuildRequest` without exposing linker entities.
6. [`src/n64_build.cpp`](src/n64_build.cpp) validates the SDK, stages inputs,
   writes the private Makefile, starts a sanitized `/usr/bin/make` process,
   retains failures, and atomically places the completed ROM.
7. The pinned libdragon `n64.mk` owns final static linking, symbols, stripping,
   compression, DragonFS, header configuration, and extended metadata.

Target selection and option parsing stay in their existing compiler modules.
N64 SDK provenance, staging, generated graph details, subprocess policy,
cleanup, and ROM placement belong in `src/n64_build.cpp`. General linker code
should contain only translation adapters. Runtime startup and allocation belong
in target-tagged `base/runtime/*_n64.odin` files. Bound C declarations and their
ABI assertions belong in the independent Odin64 repository's `libdragon/` and
`tests/abi/libdragon/`, not this compiler's vendor collection.

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
- installed `libdragon.version` names the pinned commit;
- installed libdragon provenance reports `dirty: false`;
- installed `include/n64.mk` has the pinned SHA-256;
- `mkdfs` is present when assets are requested;
- `n64metadata` is present when metadata is requested.

Host, binutils, GCC, and newlib provenance differences are warnings. They are
still release-record fields and must be reviewed; warning status is not
permission to omit them from qualification evidence.

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

N64 is single-threaded and disables TLS. There is no supported mechanism for a
C callback, interrupt, or timer entry to install an Odin context. Do not expose
callback-taking APIs until their context, reentrancy, stack, allocator, and
failure rules have a separate design and hardware validation.

## Authoritative pins

[Odin64's toolchain.lock.toml](https://github.com/soft-circles/Odin64/blob/main/toolchain.lock.toml)
owns the compatible Odin/LLVM revisions, libdragon SDK identity and validation
runner pins. It does not record automatic release acceptance for new ROMs.
The compiler embeds SDK constants in
[src/n64_toolchain_pins.hpp](src/n64_toolchain_pins.hpp); tests compare them with
the lock when available, or check local documentation in a standalone checkout.

### Updating the toolchain pins

1. Review the proposed SDK commit, clean provenance, n64.mk hash and tool versions.
2. Update compiler constants and the Odin64 lock together, in their owning repositories.
3. Review changed bindings and their C/Odin ABI assertions in Odin64.
4. Run compiler quick/full checks and Odin64 cross-repository validation.
5. Commit and publish compatible compiler changes, then lock that exact commit.
6. Record new hashes honestly; changed artifacts require affected manual/hardware
   qualification under Odin64's Foundation matrix. Do not rewrite historical evidence.

## Validation layers

[tests/n64_validate.py](tests/n64_validate.py) runs compiler-owned checks only.
Use `--list` to inspect stages and `--artifacts <parent>` for unique retained
logs and a compiler identity manifest. It never builds LLVM or installs dependencies.
Build Odin separately using an explicit compatible `LLVM_CONFIG`.

### Quick: no SDK or emulator required

```sh
python3 tests/n64_validate.py quick
```

Checks cover pins, local documentation links, validation contracts, module
boundaries, public options/failures, SDK-validator behavior and compilation of
[tests/n64_runtime](tests/n64_runtime). No project-local binding is required.

### Full: compiler SDK and runtime checks

```sh
N64_INST=/absolute/sdk ARES_TEST=/absolute/ares-test \\
  python3 tests/n64_validate.py full
```

Missing tools fail instead of skipping. Full mode repeats quick checks, validates
the SDK, runs public build/packaging tests, the Odin/GCC differential, linked O64
ABI ROM, and standalone runtime lifecycle. Fixture logs are retained on failure.
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
