# Integrated N64 build tests

See [`N64_BUILD.md`](../../N64_BUILD.md) for the project-facing build guide.
This file documents the compiler's automated regression coverage.

This directory tests the integrated N64 build path, including Milestone 5 ROM
configuration, extended metadata, and raw DFS assets, only through its public
CLI:

```sh
python3 tests/n64_build/test_n64_build.py
```

The SDK discovery and validation tests run without invoking libdragon's build
tools. The end-to-end tests use `N64_INST` when it is set, otherwise they use
`~/n64_toolchain` when that directory exists. Set `ODIN` to select a compiler
binary other than the repository's `odin` executable.

The end-to-end tracer test deliberately puts the source, SDK alias, output,
and retained intermediates beneath paths containing spaces. It builds the
existing Odin-only tracer with a single `odin build . -target:n64` invocation,
checks the requested `.z64` and ROM byte order, and verifies that
`-keep-temp-files` retained the generated packaging graph and link artifacts.
When `ARES_TEST` names a compatible `ares-test` executable, the same test also
runs the generated tracer ROM through the pinned headless script and checks its
functional sentinel and framebuffer hash.
An additional link test supplies both a prebuilt foreign object and archive and
checks that the integrated path carries them into libdragon's static link.

The supported ROM options are:

- `-n64-title:<string>`: 1-20 make-safe title characters.
- `-n64-region:<letter>`: one region letter, canonicalized to uppercase.
- `-n64-save-type:<type>`: `none`, `eeprom4k`, `eeprom16k`, `sram256k`,
  `sram768k`, `sram1m`, or `flashram`.
- `-n64-rtc`: enables the Joybus RTC declaration; omit it to leave RTC off.
  The pinned header format does not allow RTC with either EEPROM save type.
- `-n64-controllers:<list>`: one to four semicolon-separated declarations.
  Semicolons are intentional because attachment declarations contain commas,
  for example `-n64-controllers:"n64,pak=rumble;none;none;none"`.
- `-n64-assets:<directory>`: passes one raw directory to `mkdfs` and embeds the
  resulting DFS through the generated `n64.mk` graph.
- `-n64-metadata:<file>`: passes one libdragon-compatible INI to
  `n64metadata`.

Asset directories and metadata files are staged under fixed, space-free names,
so source and output paths may contain spaces. `mkdfs` and `n64metadata` are
required from the validated SDK only when their corresponding options are used.
