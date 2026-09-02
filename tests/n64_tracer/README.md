# Odin N64 entry/runtime tracer

Use [`N64_BUILD.md`](../../N64_BUILD.md) for SDK setup, ordinary builds,
emulator loading, and packaging diagnosis. The raw calls are documented by the
[`vendor:libdragon` guide](../../vendor/libdragon/README.md).

This Milestone 3 ROM fixture is an Odin-only application with an ordinary
`main :: proc()`. The N64 target supplies the hidden C-ABI entry bridge; there
is no application C entry point or project Makefile.

The ROM is a public-seam integration test for:

- runtime startup before dynamic global initialization, `@(init)`, and ordinary
  `main`, plus `@(fini)` cleanup after `main` returns;
- allocation during `@(init)` through both default context allocators;
- general allocator alignment, zeroing, grow/shrink resize, failed-resize
  preservation, deterministic OOM reporting, and free;
- temporary allocator alignment, zeroing, grow/shrink resize, failed-resize
  preservation, OOM, `free_all` reset/reuse, and replaceability of both
  context allocator fields;
- EMUX logging, direct CPU graphics, current joypad polling, and elapsed time.

The headless test requires structured, ordered `v2` PASS sentinels and rejects
any `ODIN_N64_TRACER_FAIL:v2:` sentinel. It injects a real port-1 A press and
requires the reviewed deterministic 640x240 angrylion frame hash
`13B30B82C8227FAA1DE66FBC058C2F6E5B980118096CE6B090A342D868DDB79F`.
The displayed scene is intentionally unchanged from Milestone 1, so the
checked-in `tracer.golden.png` remains the reviewed reference.

With the pinned SDK configured through the main guide, build from this
directory:

```sh
N64_INST=/path/to/n64_toolchain odin build . -target:n64 -out:n64_tracer.z64
```

This ordinary fixture command intentionally retains Odin's default
source-location behavior. Release qualification builds a clean fixture copy
from the exact committed Odin candidate with
`-source-code-locations:filename`. During qualification, the commit-bound
candidate manifest supplies the proposed v0.2.1 candidate identity. The
historical hashes below are not current accepted v0.2.1 ROM identities.

Run the headless gate with:

```sh
/path/to/ares-test tracer.test.js n64_tracer.z64 --timeout 30
```

The authoritative automated runner is the coordination lock's pinned
Ubuntu/AMD64 `ares-test`. On Apple Silicon, use the documented Linux/AMD64
Docker-emulation path. Native macOS Ares is an interactive visual check, not
the automated oracle.

## Historical v0.2 evidence

The 2026-08-31 v0.2 software validation used `ares-test` commit
`09008b610a16c375f793d0e124a366227bc4839c` in its Linux/AMD64 image. It
observed every ordered runtime/allocator sentinel, libdragon's EMUX exit
request, and framebuffer hash
`13B30B82C8227FAA1DE66FBC058C2F6E5B980118096CE6B090A342D868DDB79F`.
Upstream macOS Ares v148 also loaded the same ROM and visibly rendered the
reviewed tracer frame. That historical ROM's SHA-256 is
`d2939a7e6ca5b2cfc5aace448917eb74f0679191efd01a9ae72fad39431534c7`.

This exact ROM subsequently passed its Analogue3D/SummerCart64 hardware
validation on 2026-08-31. Together with the headless and upstream-Ares results,
that completes the Milestone 2 acceptance gates.

The v0.2 Milestone 3 run rebuilt the same tracer from a clean directory
containing only `tracer.odin` with `odin build . -target:n64`. The integrated
ROM passed the same pinned headless runner and framebuffer oracle. That
historical v0.2 ROM's SHA-256 is
`50bdcc3d0fa9729fe72597a58e292c38261ee508466a4d1acab5e03eb60b8dd2`.
