# Building Nintendo 64 ROMs with Odin

This Odin branch can compile an Odin package directly into a Nintendo 64
`.z64` ROM. A project does not need its own C entry point, Makefile, or package
manifest. Odin compiles the program and then drives the pinned libdragon tools
that assemble the final cartridge image.

## Prerequisites

You need:

- this N64-enabled Odin compiler;
- a POSIX host, currently macOS or Linux;
- the installed libdragon SDK pinned to commit
  `c79a52b42ac790e06e797aede43914dd8754cd5f`;
- an ordinary Odin package containing `main :: proc()`.

Tell Odin where the installed SDK is located by setting `N64_INST`:

```sh
export N64_INST=/absolute/path/to/n64_toolchain
```

Alternatively, pass `-n64-inst:/absolute/path/to/n64_toolchain` with every
build. The command-line option takes precedence over `N64_INST`.

Odin validates the SDK before building. It intentionally rejects an SDK with
the wrong libdragon revision, dirty provenance, missing tools, or a modified
`n64.mk` packaging recipe.

## Minimal project

A project can be as small as:

```text
my_game/
└── main.odin
```

Run the build from inside `my_game`:

```sh
odin build . -target:n64 -out:my_game.z64
```

If `odin` is not on `PATH`, use the compiler's full path:

```sh
/absolute/path/to/odin/odin build . -target:n64 -out:my_game.z64
```

On success, `my_game.z64` is a big-endian N64 cartridge image ready for an
emulator or flash cartridge.

## Full configured build

This example supplies every commonly used ROM setting, embeds an asset
directory, and attaches extended metadata:

```sh
odin build . -target:n64 -out:my_game.z64 \
  -n64-title:"My Odin Game" \
  -n64-region:E \
  -n64-save-type:sram256k \
  -n64-rtc \
  -n64-controllers:"n64,pak=rumble;none;none;none" \
  -n64-assets:assets \
  -n64-metadata:metadata.ini
```

Omit any setting the project does not need. In particular, omit `-n64-rtc` to
leave the real-time clock disabled.

## ROM options

| Option | Meaning |
| --- | --- |
| `-target:n64` | Select the Nintendo 64 target. This is required. |
| `-out:<file>.z64` | Select the output ROM filename. Odin adds `.z64` when needed. |
| `-n64-inst:<directory>` | Select the installed libdragon SDK instead of using `N64_INST`. |
| `-n64-title:<title>` | Set a 1–20 character ROM title. Letters, numbers, spaces, `-`, `_`, `.`, and `!` are accepted. |
| `-n64-region:<letter>` | Set one region letter, such as `E`, `J`, or `P`. Lowercase input is changed to uppercase. |
| `-n64-save-type:<type>` | Declare the cartridge save hardware. |
| `-n64-rtc` | Declare that the cartridge uses a real-time clock. |
| `-n64-controllers:<list>` | Describe one to four controller ports, separated by semicolons. |
| `-n64-assets:<directory>` | Turn a raw directory into an embedded DragonFS image. |
| `-n64-metadata:<file>` | Pass a libdragon-compatible metadata INI file to `n64metadata`. |

Valid save types are:

- `none`
- `eeprom4k`
- `eeprom16k`
- `sram256k`
- `sram768k`
- `sram1m`
- `flashram`

The pinned N64 header format cannot combine `-n64-rtc` with `eeprom4k` or
`eeprom16k`. Odin reports an error before compilation if they are combined.

Controller ports use semicolons because some individual controller settings
contain commas. For example:

```sh
-n64-controllers:"n64,pak=rumble;none;mouse;gamecube"
```

Accepted controller declarations are:

- `n64`
- `n64,pak=rumble`
- `n64,pak=controller`
- `n64,pak=transfer`
- `none`
- `mouse`
- `vru`
- `gamecube`
- `randnetkeyboard`
- `gamecubekeyboard`

## Embedding raw assets with DragonFS

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
odin build . -target:n64 -out:my_game.z64 -n64-assets:assets
```

Odin asks libdragon's `mkdfs` tool to convert the entire directory into a
DragonFS image and packages that image into the ROM. The program must mount and
read the filesystem through the DragonFS API at runtime. See
[`tests/n64_dfs/dfs.odin`](tests/n64_dfs/dfs.odin) for a complete Odin-only
example.

## Adding extended metadata

Write metadata using libdragon's existing INI format. Odin passes this file to
`n64metadata` without inventing a separate format:

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

Build with:

```sh
odin build . -target:n64 -out:my_game.z64 \
  -n64-metadata:metadata.ini
```

Files named by metadata fields such as `long-desc`, `screenshots`, `boxart`, or
`cartart` are resolved relative to the INI file. Odin stages only the referenced
file roots for packaging, including referenced subdirectories.

## What the build does

The command follows this sequence:

1. Odin recognizes `-target:n64` and selects big-endian MIPS with the O64
   calling convention.
2. Odin reads and validates the N64 command-line options.
3. Odin validates the pinned libdragon installation and any requested tools.
4. Odin compiles the Odin source and N64 runtime into MIPS object files.
5. Odin creates an isolated temporary build directory beside the output ROM.
6. Odin copies the compiled inputs there and safely stages requested assets and
   metadata.
7. Odin generates a small internal Makefile containing the validated ROM
   settings.
8. Odin starts the pinned libdragon build recipe with a cleaned environment.
9. Libdragon links the program, builds DragonFS when requested, creates symbols,
   strips and compresses the program, constructs the `.z64` image, writes the
   cartridge configuration, and appends metadata when requested.
10. Odin atomically places the completed ROM at the requested output path.
11. Odin removes successful intermediates unless they were requested for
    inspection.

The main implementation points are:

- N64 target definition: [`src/build_settings.cpp`](src/build_settings.cpp)
- option registration and validation: [`src/main.cpp`](src/main.cpp)
- SDK validation and ROM packaging pipeline: [`src/linker.cpp`](src/linker.cpp)
- N64 runtime: [`base/runtime`](base/runtime)
- raw libdragon bindings: [`vendor/libdragon`](vendor/libdragon)

## Inspecting a build

Add these flags when diagnosing packaging or inspecting the intermediate ELF,
DragonFS image, symbols, generated Makefile, and staged inputs:

```sh
odin build . -target:n64 -out:my_game.z64 \
  -keep-temp-files \
  -show-system-calls
```

Odin prints the retained directory as:

```text
Retained N64 build intermediates: /path/to/.odin-n64-build-XXXXXX
```

Failed packaging builds are also retained automatically. Successful builds
normally remove the temporary directory.

## Common errors

- **“N64 SDK is not configured”**: set `N64_INST` or pass `-n64-inst`.
- **“libdragon SDK mismatch”**: install the pinned clean libdragon revision;
  do not replace its `n64.mk` with a locally modified copy.
- **“requires ... mkdfs”**: the selected SDK is missing the asset-packaging
  tool required by `-n64-assets`.
- **“requires ... n64metadata”**: the selected SDK is missing the metadata tool
  required by `-n64-metadata`.
- **RTC and EEPROM error**: remove `-n64-rtc` or select a non-EEPROM save type.
- **Controller-list error**: separate ports with semicolons and keep commas
  inside individual attachment declarations.

Paths containing spaces are supported. Quote a whole argument when required by
the shell, especially controller lists and titles.

## Current limitations

- Integrated executable-ROM builds require a POSIX host.
- The N64 executable pipeline is static and does not support LTO.
- `-no-crt`, `-no-entry-point`, custom linkers, and extra linker flags are not
  supported for `.z64` builds.
- Odin validates and drives the installed SDK but does not download or update
  libdragon automatically.

For a validated end-to-end example, see [`tests/n64_dfs`](tests/n64_dfs) and
its [`VALIDATION.md`](tests/n64_dfs/VALIDATION.md).
