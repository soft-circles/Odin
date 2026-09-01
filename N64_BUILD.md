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
[`vendor/libdragon/README.md`](vendor/libdragon/README.md). Maintainers should
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

The canonical quickstart is the checked-in Odin-only Pong sample. From the
Odin repository root, after building the compiler and configuring `N64_INST`:

```sh
cd tests/n64_pong
../../odin build . -target:n64
```

This creates `n64_pong.z64` in that directory. Load it in a current N64
emulator or on a flash cartridge; the left paddle uses port 1, A serves, and
Start resets the match. The application source is
[`tests/n64_pong/pong.odin`](tests/n64_pong/pong.odin), and its fixture-specific
controls and evidence are in
[`tests/n64_pong/README.md`](tests/n64_pong/README.md).

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

## First display, input, and debug loop

Import the raw package as `vendor:libdragon`. This compact loop uses the same
ordering as the canonical Pong and tracer programs: initialize logging and
joypads once, initialize display once, poll input before reading it, acquire a
display surface, draw, and submit that same surface.

```odin
package my_game

import ld "vendor:libdragon"

main :: proc() {
	_ = ld.debug_init_emulog()
	ld.debugf("Odin N64 started\n")
	ld.joypad_init()
	ld.display_init(
		ld.RESOLUTION_320x240,
		.DEPTH_16_BPP,
		2,
		.GAMMA_NONE,
		.FILTERS_DISABLED,
	)

	for {
		ld.joypad_poll()
		input := ld.joypad_get_inputs(.JOYPAD_PORT_1)
		frame := ld.display_get()
		if frame == nil {
			continue
		}
		background := ld.graphics_make_color(8, 16, 32, 255)
		if input.btn.raw & ld.JOYPAD_BUTTON_A != 0 {
			background = ld.graphics_make_color(24, 112, 72, 255)
		}
		ld.graphics_fill_screen(frame, background)
		ld.graphics_set_default_font()
		ld.graphics_set_color(
			ld.graphics_make_color(255, 255, 255, 255),
			background,
		)
		ld.graphics_draw_text(frame, 28, 36, "ODIN ON N64 - HOLD A")
		ld.display_show(frame)
	}
}
```

The raw package is intentionally incomplete. Its full supported inventory,
type rules, and lifecycle requirements are documented in the
[binding guide](vendor/libdragon/README.md).

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

The DFS fixture combines raw assets and metadata with explicit header settings
and is the canonical configured build example:

```sh
cd tests/n64_dfs
../../odin build . -target:n64 -out:n64_dfs.z64 \
  -n64-title:"Odin DFS v0.2" \
  -n64-region:E \
  -n64-save-type:none \
  -n64-controllers:"n64;none;none;none" \
  -n64-assets:assets \
  -n64-metadata:metadata.ini
```

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
ROM. At runtime, mount the image, open a root-relative path, read it, and close
the handle:

```odin
import "core:c"
import ld "vendor:libdragon"

if ld.dfs_init(ld.DFS_DEFAULT_LOCATION) == ld.DFS_ESUCCESS {
	handle := ld.dfs_open("message.txt")
	if handle >= 0 {
		size := ld.dfs_size(u32(handle))
		buffer: [256]byte
		if size >= 0 && size <= c.int(len(buffer)) {
			read := ld.dfs_read(rawptr(&buffer[0]), 1, size, u32(handle))
			_ = read
		}
		_ = ld.dfs_close(u32(handle))
	}
}
```

See [`tests/n64_dfs/dfs.odin`](tests/n64_dfs/dfs.odin) for checked error
handling and exact content validation.

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
interrupt, timer, thread, and TLS context propagation are not supported. The
raw binding does not expose those callback-oriented subsystems.

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
| Toolchain provenance warning | Confirm the host/binutils/GCC/newlib difference is intentional; release qualification uses the versions recorded by the coordination lock. |
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
- The raw binding is narrow: there is no RDPQ, sprite/texture, audio, save-game,
  interrupt, timer, or broad libdragon surface.
- Odin validates and drives the SDK but does not install or update it.
- Asset conversion beyond raw DragonFS directory packaging remains an external
  project concern.

## Canonical samples and evidence

After building the compiler, contributors can run the portable N64 confidence
suite without an SDK or emulator:

```sh
python3 tests/n64_validate.py quick
```

Maintainers use `python3 odin/tests/n64_validate.py full` from the coordination
workspace on Ubuntu/AMD64. Full setup, strict dependency behavior, retained
logs, and focused commands are documented in
[`N64_MAINTAINERS.md`](N64_MAINTAINERS.md#validation-layers).

- Runtime, allocator, debug, display, input, and timing tracer:
  [`tests/n64_tracer`](tests/n64_tracer)
- Playable Pong quickstart: [`tests/n64_pong`](tests/n64_pong)
- DragonFS and metadata sample: [`tests/n64_dfs`](tests/n64_dfs)
- Raw binding C/Odin ABI probe:
  [`tests/o64_abi/libdragon_bindings`](tests/o64_abi/libdragon_bindings)
- Integrated public-build tests: [`tests/n64_build`](tests/n64_build)
- O64 baseline and runner setup: [`tests/o64_abi`](tests/o64_abi)
- DFS accepted validation ledger:
  [`tests/n64_dfs/VALIDATION.md`](tests/n64_dfs/VALIDATION.md)
- Pong and DFS manual records:
  [`tests/n64_pong/PLAYTEST.md`](tests/n64_pong/PLAYTEST.md) and
  [`tests/n64_dfs/PLAYTEST.md`](tests/n64_dfs/PLAYTEST.md)
