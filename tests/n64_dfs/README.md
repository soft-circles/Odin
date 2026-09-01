# Odin N64 DFS and metadata v0.2 sample

Use [`N64_BUILD.md`](../../N64_BUILD.md) for SDK installation, ROM options,
DragonFS, metadata staging, emulator loading, and retained intermediates. The
raw filesystem calls are documented in the
[`vendor:libdragon` guide](../../vendor/libdragon/README.md#dragonfs).

This Odin-only sample is the Milestone 5 end-to-end fixture. Its ordinary
`main :: proc()` mounts the DragonFS image from the ROM package, opens
`message.txt` through the raw `vendor:libdragon` binding, validates its exact
size and bytes, closes the handle, and renders one deterministic success panel.

The build consumes two explicit inputs:

- `assets/` is a raw asset directory. Odin asks the pinned libdragon `mkdfs`
  tool to turn it into the embedded DragonFS image.
- `metadata.ini` is a libdragon-compatible extended metadata fixture, including
  English and Spanish sections. Odin passes it directly to `n64metadata`; the
  compiler does not define a second metadata format.

There is no C entry point, application Makefile, converted asset, or project
manifest in this directory.

`VALIDATION.md` records the accepted artifact hashes and final automated and
manual gate results.

## Fixture build

With the pinned libdragon SDK
`c79a52b42ac790e06e797aede43914dd8754cd5f` configured as described by the main
guide, build from this directory:

```sh
odin build . -target:n64 -out:n64_dfs.z64 \
  -n64-title:"Odin DFS v0.2" \
  -n64-region:E \
  -n64-save-type:none \
  -n64-controllers:"n64;none;none;none" \
  -n64-assets:assets \
  -n64-metadata:metadata.ini
```

This fixture deliberately omits `-n64-rtc`, so the reviewed artifact has RTC
disabled.

The expected runtime asset is exactly 22 bytes, including its final newline:

```text
ODIN N64 DFS ASSET v1
```

## Automated headless gate

The automated gate uses `ares-test` from `HailToDodongo/ares-64` commit
`09008b610a16c375f793d0e124a366227bc4839c`. After building, run:

```sh
/path/to/ares-test dfs.test.js n64_dfs.z64 --timeout 30
```

The harness selects homebrew mode and the CPU angrylion renderer. It requires
ordered v1 sentinels for mount, open, size, read, byte-for-byte content, close,
frame submission, readiness, and final PASS, and rejects every
`ODIN_N64_DFS_FAIL:v1:` marker. It then checks a 640x240 scanout against the
reviewed `dfs.golden.png` pixel-for-pixel and writes `dfs.diff.png` on a visual
mismatch.

The reviewed `dfs.golden.png` was promoted from the accepted candidate only
after the same release-candidate ROM passed the pinned headless runner,
official upstream Ares, Analogue3D, and SummerCart64 gates. The normal gate is
therefore expected to pass pixel-for-pixel.

## Candidate capture and golden promotion

Capture a candidate without touching the golden path:

```sh
/path/to/ares-test test_capture_dfs_candidate.js n64_dfs.z64 \
  dfs.candidate.png --timeout 30
```

The helper refuses to write `dfs.golden.png`. For a future intentional visual
change, record the candidate hash, inspect the panel in a verified upstream
Ares build, and confirm the same panel on Analogue3D/SummerCart64 using
`PLAYTEST.md`. Only then may a reviewed change replace `dfs.golden.png`. Finish
by rerunning the normal gate above against the unchanged ROM.

Ubuntu/AMD64 is the authoritative headless environment. Apple Silicon should
use the Linux/AMD64 Docker-emulation route documented by the O64 ABI fixture;
native macOS and Linux/ARM64 runs are not claimed as supported. Upstream Ares
and hardware remain independent release gates, not replacements for the
headless runner.

## Metadata and package inspection

Milestone 5 package verification should use the retained build graph and the
pinned SDK tools to confirm all of the following on the same candidate ROM:

- the basic header title, region, save type, RTC state, and controller fields;
- the embedded `metadata.ini` payload and its extended `[meta.es]` section;
- the DragonFS image containing the exact `assets/message.txt` bytes;
- the pinned `libdragon.version` and `toolchain.version` rompak payloads.

Use the main guide's
[inspection flags](../../N64_BUILD.md#load-and-debug-a-rom) when these
fixture-specific package assertions need retained inputs.

Run the checked-in verifier against that retained DFS image and the final ROM:

```sh
python3 verify_rom.py n64_dfs.z64 \
  /path/to/.odin-n64-build-XXXXXX/build/odin-n64.dfs \
  metadata.ini /path/to/n64_toolchain
```

The verifier rejects the wrong byte order, header values, rompak layout,
DragonFS image, SDK-version payloads, metadata ZIP structure, or localized INI
content.

The accepted release candidate is `n64_dfs.z64` with SHA-256
`09299b4c7d1b236ddbd0128409aa991f3ca2cbfefc8579e87cdcd0a8404b579c`.
Its reviewed framebuffer has raw RGB SHA-256
`b3c012ba911d27fe55875dd209a56ea17542e341881a0ce42edc06ec0c1e88ef`
and PNG SHA-256
`a2c056858a6f44f22b18751f1d9edf9a65f8d43649e515101663522b83b9967e`.
See `PLAYTEST.md` for the completed upstream Ares and hardware record. The
first `ODIN_N64_DFS_FAIL:v1:<stage>` marker in a headless log identifies the
runtime stage that failed.
