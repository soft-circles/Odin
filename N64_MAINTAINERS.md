# Maintaining Odin's Nintendo 64 target

This guide is the routine maintenance and release reference for the N64 target.
The user-facing contract is in [`N64_BUILD.md`](N64_BUILD.md); the raw binding
contract is in
[`vendor/libdragon/README.md`](vendor/libdragon/README.md). Historical plans
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
ABI assertions belong in `vendor/libdragon`.

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

The coordination workspace's `toolchain.lock.toml` is the active compatibility
record. It owns:

- the Odin and LLVM revisions;
- libdragon commit and `n64.mk` SHA-256;
- expected toolchain provenance and hard/warning policy;
- authoritative `ares-test` revision and environment;
- accepted upstream Ares identity;
- accepted ROM hashes.

The compiler cannot depend on a source-tree-relative runtime file, so
[`src/n64_toolchain_pins.hpp`](src/n64_toolchain_pins.hpp) contains checked-in
constants. The hub test verifies that they match the lock. Test helpers import
those compiler constants rather than copying active pins. Historical ledgers
may retain literal hashes as immutable evidence.

### Updating the toolchain pins

Perform pin changes in a dedicated review, not incidentally:

1. Create a clean candidate SDK from the proposed libdragon revision and
   record its commit, clean state, `include/n64.mk` SHA-256, host, binutils,
   GCC, and newlib provenance.
2. Update the relevant fields in the hub `toolchain.lock.toml`.
3. Update `src/n64_toolchain_pins.hpp` to match the lock exactly.
4. Compare every declaration in `vendor/libdragon` with the proposed headers.
   Add C and Odin layout assertions and bidirectional runtime transport for
   every new or changed concrete crossing type.
5. Update active pin statements in the project and binding guides. Do not
   rewrite old validation or playtest records.
6. From the coordination workspace root, run:

   ```sh
   python3 tests/test_toolchain_lock.py
   ```

7. Rebuild the compiler and run the quick and full layers below using the new
   SDK. Record every provenance warning and decide whether to update the
   expected baseline or reject the candidate.
8. Rebuild tracer, Pong, and DFS from clean source copies. Compare ELF,
   packaging, ROM, and framebuffer identities.
9. If any accepted ROM changes, rerun the affected upstream Ares and
   Analogue3D/SummerCart64 gates before accepting the pin.
10. Commit the Odin changes, rerun qualification from that exact commit, then
    update the hub's Odin commit and release ledger. The lock must never claim
    an untested or uncommitted compiler revision.

## Validation layers

[`tests/n64_validate.py`](tests/n64_validate.py) is the single automated entry
point. It labels every stage, streams its output, and writes a separate log plus
an identity manifest below `.n64-validation-artifacts/<timestamp>/`. A failed
run keeps all logs, clean fixture copies, framebuffer diffs, generated ROMs,
and the compiler's retained `-keep-temp-files` directories. Set
`N64_VALIDATION_ARTIFACTS` or pass `--artifacts` to choose another retention
root. Use `--list` to inspect the stages without running them.

The individual fixture commands remain supported. Use the first failed stage's
log and fixture README for focused diagnosis.

### Source locations in release ROMs

Ordinary user builds retain Odin's default source-location behavior. The
project-facing and fixture build commands therefore do not add a
`-source-code-locations` option, and this release does not change the compiler's
default.

Release qualification has a narrower reproducibility requirement. The full
validation layer explicitly builds the tracer, Pong, and DFS fixtures with
`-source-code-locations:filename` so source locations cannot make their ROM
bytes depend on the absolute checkout path. Derive candidate hashes only from
those path-independent builds of an exact committed Odin candidate, and record
the Odin commit plus all three hashes in the candidate manifest. The manifest
is the source of candidate identities during qualification; only fully
qualified and promoted values become accepted identities in the coordination
lock.

Run candidate qualification with an absolute manifest path:

```sh
python3 odin/tests/n64_validate.py full \
  --candidate-manifest /absolute/path/to/v0.2.1-candidate.toml
```

Without `--candidate-manifest`, full validation uses the accepted identities in
the coordination lock.

### Quick: no SDK or emulator required

Build the compiler, then run from the Odin repository root:

```sh
python3 tests/n64_validate.py quick
```

Quick mode never uses an installed SDK or emulator. It checks active pins,
canonical documentation links, the internal N64 module boundary, public option
and failure behavior, SDK-validator behavior, and compilation of the checked-in
Pong quickstart. When the coordination workspace is present, the pin stage also
checks compiler constants and active Ares statements against
`toolchain.lock.toml`; a standalone Odin checkout checks its local compiler and
documentation statements.

The focused commands remain:

```sh
python3 tests/n64_build/test_n64_module.py
N64_VALIDATION_MODE=quick python3 tests/n64_build/test_n64_build.py
python3 tests/o64_abi/test_validate_sdk.py
python3 tests/n64_validation/check_active_pins.py
python3 tests/n64_validation/check_documentation_links.py
```

### Full: pinned SDK and authoritative runner required

Full mode is the authoritative Ubuntu/AMD64 release job. Run it from a clean
Linux/AMD64 host with the coordination workspace topology, then set all five
explicit inputs:

```sh
export ODIN_N64_WORKSPACE=/absolute/path/to/odin-n64
export N64_INST=/absolute/path/to/n64_toolchain
export ARES_TEST=/absolute/path/to/ares-test
export ARES_TEST_SOURCE=/absolute/path/to/ares-64
export N64_VALIDATION_CONTAINER=ubuntu-image@sha256:<immutable-digest>
python3 odin/tests/n64_validate.py full
```

`ARES_TEST_SOURCE` must be a clean checkout at the commit recorded in the
coordination lock. Full preflight verifies that revision and fails before any
stage when the SDK, runner, source checkout, container identity, workspace,
lock, or authoritative host is missing. It never converts a missing dependency
into a skip.

The job rebuilds the compiler, repeats quick validation, validates the SDK,
runs the public-build suite, both LLVM/GCC and Odin/GCC O64 differentials, the
linked ABI ROM, raw binding layout/runtime, and console lifecycle. It then
builds tracer, Pong, and DFS from clean copied fixtures, checks every accepted
ROM SHA-256 from the coordination lock, and runs all three reviewed framebuffer
goldens. Focused commands and failure interpretation remain documented in:

- [`tests/n64_build/README.md`](tests/n64_build/README.md)
- [`tests/o64_abi/README.md`](tests/o64_abi/README.md)
- [`tests/n64_tracer/README.md`](tests/n64_tracer/README.md)
- [`tests/n64_pong/README.md`](tests/n64_pong/README.md)
- [`tests/n64_dfs/README.md`](tests/n64_dfs/README.md)

### Reproducible Ubuntu/AMD64 provisioning

Provision the full job from immutable source revisions rather than moving
branches or cache names:

1. Check out the hub, Odin fork, and LLVM fork at the commits in
   `toolchain.lock.toml`. Confirm all three statuses separately.
2. Check out libdragon at the lock's exact commit. Set `N64_INST` to an empty
   absolute directory, run that checkout's `tools/build-toolchain.sh`, then run
   `make install tools-install`. Do not reuse an SDK until
   `tests/o64_abi/validate_sdk.py` accepts its installed commit, clean bit, and
   `n64.mk` hash.
3. Clone `HailToDodongo/ares-64` recursively, detach at the lock's exact
   `ares_test.commit`, initialize every submodule, and build its
   `linux-headless` target. Set `ARES_TEST_SOURCE` to that checkout and
   `ARES_TEST` to the resulting executable.
4. Run the full command above. Archive the entire selected artifacts directory
   on success or failure; its `identity.json` records host, repository, SDK,
   runner path, and runner binary hash, while the stage logs and fixture copies
   retain the remaining evidence.

The SDK/toolchain and headless-runner build steps are intentionally separate
from the validation driver: automatic installation is outside the supported
Odin interface, while the exact provisioning inputs remain auditable and
repeatable. CI may cache their build directories, but validation must inspect
the installed SDK and runner source revision after restoring a cache.

### Manual emulator and hardware

The recorded upstream Ares build is the interactive visual/debugging gate.
Analogue3D with SummerCart64 is the hardware gate. Use the exact ROM already
accepted by the full automated layer, hash it before loading, perform every
fixture-specific checklist item, and record unchecked observations honestly.
Neither a generic emulator launch nor a headless pass substitutes for these
release gates.

## Golden capture and promotion

Normal golden tests compare against the three reviewed images and must never
rewrite them. Candidate scripts write only `*.candidate.png` and refuse the
golden path.

For an intentional visual change:

1. build one release-candidate ROM and record its SHA-256;
2. run the fixture's candidate capture script with the pinned headless runner;
3. record candidate PNG and decoded RGB hashes;
4. inspect the exact ROM/candidate in the recorded upstream Ares build;
5. confirm the same state on Analogue3D/SummerCart64;
6. record evidence in the fixture playtest file;
7. promote the reviewed candidate in an explicit change;
8. rerun the normal headless test against the unchanged ROM and new golden.

If the visual change was not intended, do not update the golden. Retain the
diff and diagnose the first changed build/runtime stage.

## Release qualification

Release candidates must come from exact committed Odin and LLVM trees. Record
the branch, commit, remotes, and complete status for the hub, Odin, and LLVM
repositories separately. A clean hub says nothing about its ignored nested
repositories.

Use this validation-ledger template:

```text
Release/version:
Date and operator:
Hub commit/status:
Odin commit/status:
LLVM commit/status:
Host OS/architecture:
Compiler build command and result:
libdragon commit/dirty/n64.mk SHA-256:
Toolchain host/binutils/GCC/newlib:
ares-test commit/container digest:
Upstream Ares version/commit/archive SHA-256:

Quick layer command/result:
Full layer command/result:
O64 argument locations:
LLVM/GCC differential:
Odin/GCC differential and call state:
Linked interop:
Raw binding layout/runtime:

Tracer ROM/RGB/PNG SHA-256 and headless result:
Pong ROM/RGB/PNG SHA-256 and headless result:
DFS ROM/DFS/metadata/asset/RGB/PNG SHA-256 and headless result:
Upstream Ares result/evidence:
Hardware result/evidence:
Documentation walkthrough host/result:
git diff --check and final per-repository statuses:
Exceptions or renewed qualification:
```

The reproducible-release record must also name every command, working
directory, environment variable that selects infrastructure, and retained log
or diff path. Rebuild all three canonical ROMs from clean source copies. If
their hashes match the accepted lock values byte-for-byte, existing manual
evidence still applies to the identical artifacts. Any changed ROM requires
ELF/package analysis and renewed affected manual/hardware gates.

## Intentionally deferred

Do not broaden v0.2.1 maintenance work to include:

- new libdragon subsystems or a convenience game framework;
- callback, interrupt, timer, thread, or TLS support;
- automatic SDK/emulator installation;
- new asset converters or project metadata formats;
- `odin run -target:n64` or `odin test -target:n64`;
- generalized post-link packaging for other targets;
- save-game, audio, RDPQ, sprite, or texture vertical slices.

Those features require separate designs and qualification plans.
