# Building Nintendo 64 ROMs with Odin

This Odin branch compiles an ordinary Odin package directly into a big-endian
Nintendo 64 `.z64` ROM:

```sh
odin build . -target:n64
```

The application supplies `main :: proc()`. The compiler supplies the N64 entry
bridge and runtime, then drives a validated libdragon SDK to link and package
the ROM. Projects do not need a C entry point, Makefile, or manifest.

For the narrow raw libdragon API, see
[Odin64's project-local binding](https://github.com/soft-circles/Odin64/blob/main/libdragon/README.md). Maintainers should
also read [`N64_MAINTAINERS.md`](N64_MAINTAINERS.md).

## Supported hosts

The integrated executable-ROM pipeline requires a POSIX host and GNU make at
`/usr/bin/make`.

| Host | ROM builds | Release-validation status |
| --- | --- | --- |
| macOS on Apple Silicon | Supported | Validated build host |
| macOS on Intel | Supported by the POSIX build path | Not part of the current release matrix |
| Linux on AMD64 | Supported | Authoritative full and headless environment |
| Linux on ARM64 | Supported by the POSIX build path | Native headless execution is not qualified |
| Windows | Not supported for integrated `.z64` builds | Use a supported POSIX host |

Object, assembly, and LLVM IR output can still be requested for the N64 target
without running the ROM packager. Executable builds are the supported
project-facing path described here.

## Build this Odin compiler

The N64 target is developed on this repository's `n64` branch. An upstream
Odin release or nightly does not contain this branch's N64 target and is not a
substitute for building this checkout.

First install the ordinary Odin source-build prerequisites described by the
[Odin installation guide](https://odin-lang.org/docs/install/). The N64 target
also requires the matching MIPS O64 LLVM fork. Reuse a compatible existing LLVM installation when available. Only if no such
installation exists, build the pinned LLVM revision used by Odin64's lock:

```sh
git clone https://github.com/soft-circles/llvm-project.git llvm-project
git -C llvm-project checkout --detach ff570ff21ae6f2b1343084273d8bc3ad3a3d3cf4

cmake -S llvm-project/llvm -B llvm-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_PROJECTS=clang \
  -DLLVM_TARGETS_TO_BUILD=Mips \
  -DLLVM_ENABLE_ASSERTIONS=ON
cmake --build llvm-build --target llvm-config clang
```

Then build this Odin checkout from its repository root, selecting the
`llvm-config` produced above:

```sh
LLVM_CONFIG=/absolute/path/to/llvm-build/bin/llvm-config \
  ./build_odin.sh release
./odin version
```

The build script supports LLVM 17 through 22, but release reproduction uses
the exact fork revision above. If `build_odin.sh` reports that no supported
`llvm-config` was found, check the absolute `LLVM_CONFIG` path rather than
falling back to an unrelated host LLVM installation. The resulting `./odin`
binary is the compiler used by the commands below.

## Install the pinned libdragon SDK

The required libdragon revision is
`c79a52b42ac790e06e797aede43914dd8754cd5f`. Do not build from a moving branch:
Odin checks the installed revision, its clean provenance, and the exact
`n64.mk` packaging recipe before every executable build.

1. Install the libdragon GCC toolchain by following the official
   [installation guide](https://github.com/DragonMinded/libdragon/wiki/Installing-libdragon)
   or using the official
   [toolchain release](https://github.com/DragonMinded/libdragon/releases/tag/toolchain-continuous-prerelease).
   Choose an installation directory and export it as `N64_INST`.
2. Check out the exact libdragon source revision and install its library and
   host tools into that same directory:

   ```sh
   git clone https://github.com/DragonMinded/libdragon.git libdragon
   git -C libdragon checkout --detach c79a52b42ac790e06e797aede43914dd8754cd5f
   export N64_INST=/absolute/path/to/n64_toolchain
   (cd libdragon && make install tools-install)
   ```

   Building the toolchain from source is also supported by the pinned
   checkout's `tools/build-toolchain.sh`; follow the upstream installation
   guide for its host prerequisites.
3. Keep the checkout clean while running the install. A dirty source checkout
   produces dirty installed provenance, which Odin rejects intentionally. The
   upstream `./build.sh` also builds the complete libdragon example suite; that
   is optional for Odin and may download additional example assets.

Select the installed SDK for subsequent builds in either of these ways:

```sh
export N64_INST=/absolute/path/to/n64_toolchain
odin build . -target:n64
```

```sh
odin build . -target:n64 \
  -n64-inst:/absolute/path/to/n64_toolchain
```

`-n64-inst` takes precedence over `N64_INST`. Odin does not search for,
download, or update the SDK automatically.

## Visible quickstart

The canonical visible quickstart is [Odin64 Pong](https://github.com/soft-circles/Odin64/tree/main/examples/pong).
Bindings and application examples are owned by that independent repository, not
Odin's vendor collection. Follow its collection-mapped build instructions.

For a new package outside the repository, the minimum shape is:

```text
my_game/
└── main.odin
```

```odin
package my_game

main :: proc() {
}
```

From inside `my_game`, run:

```sh
odin build . -target:n64
```

Without `-out`, the ROM is named after the package directory and gains a
`.z64` extension. In this example the result is `my_game.z64`. An explicit
extensionless output also gains the extension:

```sh
odin build . -target:n64 -out:demo
# Produces demo.z64.
```

## Project-local libraries

Odin64 owns display/input/debug examples and the raw libdragon package.
Its applications import `odin64:libdragon` and supply an explicit
`-collection:odin64=/absolute/path/to/Odin64` mapping to build, check, doc,
and editor invocations. Odin itself does not install this project package.

## ROM configuration options

N64-specific options are valid only with `-target:n64`. Unless noted otherwise,
they apply only to executable ROM output.

| Option | Accepted value | Default when omitted |
| --- | --- | --- |
| `-n64-inst:<directory>` | Existing installed SDK root | `N64_INST`; no search fallback |
| `-n64-title:<title>` | 1–20 ASCII letters, digits, spaces, `-`, `_`, `.`, or `!` | Output name, sanitized and truncated to 20 characters; `Odin N64` if empty |
| `-n64-region:<letter>` | One ASCII letter, case-insensitive | No fixed region code; libdragon's region-free header remains enabled |
| `-n64-save-type:<type>` | One save type listed below | No declared save hardware (`none`) |
| `-n64-rtc` | Flag with no value | RTC declaration disabled |
| `-n64-controllers:<list>` | 1–4 semicolon-separated declarations | No controller hints in the advanced homebrew header |
| `-n64-assets:<directory>` | Existing directory | No DragonFS image is added |
| `-n64-metadata:<file>` | Existing INI file | No extended metadata is added |
| `-out:<path>` | Output file path, with optional `.z64` suffix | Package-directory name plus `.z64` |

Valid save types are `none`, `eeprom4k`, `eeprom16k`, `sram256k`,
`sram768k`, `sram1m`, and `flashram`. The pinned header format cannot combine
`-n64-rtc` with either EEPROM type.

Controller ports use semicolons because one controller declaration can contain
commas:

```sh
odin build . -target:n64 \
  -n64-controllers:"n64,pak=rumble;none;mouse;gamecube"
```

Accepted declarations are `n64`, `n64,pak=rumble`,
`n64,pak=controller`, `n64,pak=transfer`, `none`, `mouse`, `vru`,
`gamecube`, `randnetkeyboard`, and `gamecubekeyboard`. One to four entries are
allowed; omitted ports remain unspecified.

The integrated executable pipeline also rejects these combinations:

- commands other than `odin build`;
- static-library or dynamic-library output;
- LTO;
- relocation modes other than `-reloc-mode:static`;
- `-no-crt` or `-no-entry-point`;
- `-linker` and `-extra-linker-flags`;
- `-print-linker-flags` (use `-show-system-calls` instead);
- foreign inputs other than static `.o` and `.a` files, except the built-in
  `c`, `m`, `dragon`, and `dragonsys` libraries;
- extra linker flags attached to a foreign import.

Object, assembly, and LLVM IR build modes do not run libdragon packaging and
therefore do not accept ROM configuration options.

## Full configured build

A project containing its own `assets/` directory and `metadata.ini` can build with:

```sh
odin build . -target:n64 -out:game.z64 \
  -n64-title:"My Game" \
  -n64-region:E \
  -n64-save-type:none \
  -n64-controllers:"n64;none;none;none" \
  -n64-assets:assets \
  -n64-metadata:metadata.ini
```

See the [Odin64 DragonFS example](https://github.com/soft-circles/Odin64/tree/main/examples/dragonfs)
for a complete application with its project collection mapping.

Omit any setting the project does not need. The example deliberately omits
`-n64-rtc`, so RTC remains disabled.

## Embed raw assets with DragonFS

Place raw files below one directory:

```text
my_game/
├── main.odin
└── assets/
    ├── message.txt
    └── levels/
        └── level01.dat
```

Build with:

```sh
odin build . -target:n64 -n64-assets:assets
```

Odin requires the SDK's `mkdfs` tool only when this option is present. It
converts the complete directory into one DragonFS image and embeds it in the
ROM. Runtime filesystem access is a project concern; the
[Odin64 DragonFS example](https://github.com/soft-circles/Odin64/tree/main/examples/dragonfs)
shows checked mount/open/read/close behavior.

## Add extended metadata

Odin passes the existing libdragon INI format to `n64metadata`; it does not
define another metadata format.

```ini
[meta]
name = My Odin Game
author = Example Author
release-date = 2026-09-01
num-players = 1
short-desc = An N64 game written in Odin.

[meta.es]
name = Mi Juego Odin
short-desc = Un juego de N64 escrito en Odin.
```

```sh
odin build . -target:n64 -n64-metadata:metadata.ini
```

Localized `[meta.*]`, `[boxart.*]`, and `[cartart.*]` sections are preserved.
References in `meta.screenshots`, `meta.long-desc`, and the `front`, `back`,
`top`, `bottom`, `left`, and `right` art fields are resolved relative to the
INI file. Odin stages only the first path component needed by each safe,
relative reference. Absolute paths and references containing `.` or `..` path
components are not staged. Keep every referenced companion present beside the
INI and use `-keep-temp-files` to inspect what was selected.

## Runtime contract

Before application initialization, the N64 runtime:

1. creates the Odin context;
2. installs the libdragon-backed heap allocator as `context.allocator`;
3. initializes a fixed temporary arena and installs it as
   `context.temp_allocator`;
4. runs dynamic global initialization and `@(init)` procedures;
5. calls the application's ordinary `main`;
6. runs `@(fini)` cleanup if `main` returns.

The default temporary arena is 256 KiB. Change its compile-time capacity with
an Odin define:

```sh
odin build . -target:n64 -define:N64_TEMP_ARENA_SIZE=524288
```

The value must be greater than zero. Temporary allocations are zeroed by
default, support aligned grow/shrink resize, and are reclaimed together with
`free_all`; individual frees are not supported. Both context allocators are
ordinary replaceable Odin allocator values.

The current runtime is single-threaded and has no TLS runtime. Callback,
interrupt, timer, thread, and TLS context propagation are not supported.

## Load and debug a ROM

For ordinary development, use a current emulator with accurate N64 homebrew
support, such as [Ares](https://ares-emu.net/) or
[Gopher64](https://github.com/gopher64/gopher64). Enable Ares homebrew mode.
Open the generated `.z64` using the emulator's normal load-ROM action.

For hardware, copy the `.z64` to a compatible flash cartridge or use that
cartridge's loader. Libdragon documents examples including 64drive,
EverDrive64, and SummerCart64. Emulator convenience is not release evidence:
this project qualifies releases separately with the pinned Ubuntu/AMD64
headless runner, a recorded upstream Ares build, and Analogue3D/SummerCart64.

`debug_init_emulog` plus `debugf` sends EMUX-compatible debug output. Emulator
logs and compatible cartridge loaders can display it. For packaging diagnosis,
run:

```sh
odin build . -target:n64 \
  -keep-temp-files \
  -show-system-calls
```

Odin prints the isolated directory as
`Retained N64 build intermediates: <path>`. It contains the generated Makefile,
staged `.o`/`.a` inputs, ELF, map, symbol file, stripped ELF, and any DFS or
metadata staging. Successful builds remove it unless `-keep-temp-files` is
present. Failed packaging stages are retained automatically. The final ROM is
renamed into place only after packaging succeeds, so a failed build does not
replace an existing output.

## Diagnostics and next actions

| Diagnostic | What to do next |
| --- | --- |
| `N64 SDK is not configured` | Set `N64_INST` or pass `-n64-inst:<directory>`. |
| `missing required file` or `missing required executable tool` | Reinstall the pinned SDK into the selected root; do not assemble roots from unrelated installations. |
| `libdragon SDK mismatch` for the revision or clean state | Check out the pinned commit in a clean libdragon tree and rerun `make install tools-install`. |
| `pinned n64.mk SHA-256` mismatch | Restore `n64.mk` from the pinned checkout and reinstall it. |
| Toolchain provenance warning | Confirm the host/binutils/GCC/newlib difference is intentional; release qualification uses the versions recorded by the Odin64 compatibility lock. |
| `GNU make is required at /usr/bin/make` | Install GNU make so that exact path exists, or use a supported host image. |
| `-n64-assets requires ... mkdfs` | Install the pinned libdragon host tools into the selected SDK. |
| `-n64-metadata requires ... n64metadata` | Install the pinned libdragon host tools into the selected SDK. |
| RTC cannot be used with EEPROM | Remove `-n64-rtc` or choose a non-EEPROM save type. |
| Invalid controller list | Use 1–4 semicolon-separated declarations; keep attachment commas inside one declaration. |
| Metadata companion staging failure | Make every referenced path relative to the INI, remove `.`/`..`, and confirm the first referenced path component exists. |
| Packaging subprocess failed | Read the first make/tool error, then inspect the automatically retained staging directory. |
| ROM builds but shows no useful output | Start from the Pong or DFS sample, enable EMUX logging, and verify display initialization/get/show ordering. |

Quote whole path-valued arguments and controller lists when the shell requires
it. Source, SDK, asset, metadata, output, and staging paths containing spaces
are supported.

## Current limitations

- Integrated executable builds require a supported POSIX host.
- Only static executable ROM packaging is supported; LTO and a custom linker
  path are not.
- `odin run -target:n64` and `odin test -target:n64` are not implemented.
- The runtime does not support threads, TLS, or callback-context installation.
- Odin validates and drives the SDK but does not install or update it.
- Asset conversion beyond raw DragonFS directory packaging remains an external
  project concern.

## Canonical samples and evidence

After building the compiler, contributors can run the portable N64 confidence
suite without an SDK or emulator:

```sh
python3 tests/n64_validate.py quick
```

The compiler-only full suite additionally exercises public ROM builds,
O64 interop and the [standalone runtime probe](tests/n64_runtime):

```sh
N64_INST=/absolute/sdk ARES_TEST=/absolute/ares-test python3 tests/n64_validate.py full
```

It requires explicit tools and never builds LLVM. Cross-repository qualification,
binding ABI checks, and canonical framebuffer goldens belong to
[Odin64 validation](https://github.com/soft-circles/Odin64#verify).
See [validation layers](N64_MAINTAINERS.md#validation-layers),
[public-build tests](tests/n64_build) and [compiler ABI tests](tests/o64_abi).
