# Odin N64 Pong v0.1

This is the canonical visible quickstart linked from
[`N64_BUILD.md`](../../N64_BUILD.md). That guide owns SDK setup, general build
options, loading, and packaging diagnostics. The
[`vendor:libdragon` guide](../../vendor/libdragon/README.md) owns raw API rules.

This is a small, no-external-asset Pong game built directly on the raw
`vendor:libdragon` binding. The left paddle is controlled from port 1 and the
right paddle is CPU-controlled. The sample exercises direct display graphics,
elapsed-time frame updates, real joypad polling, persistent allocation, and a
temporary allocation reset on every submitted frame.

The persistent `Game_State` is owned by `context.allocator`. Each changed
frame's center-line layout scratch is owned by `context.temp_allocator`, then
discarded with `free_all` after the frame is displayed.

The N64 target supplies the hidden C-ABI entry point and runtime setup. The
application is ordinary Odin source: it has no C entry point, project Makefile,
or asset-build step.

## Fixture build

With the pinned libdragon SDK
`c79a52b42ac790e06e797aede43914dd8754cd5f` configured through the main guide,
build from this directory:

```sh
odin build . -target:n64 -out:n64_pong.z64
```

The result is `n64_pong.z64`. Only `pong.odin` is application source; the
README, playtest record, headless script, and golden image are fixture inputs,
not part of the ROM graph.

This ordinary fixture command intentionally retains Odin's default
source-location behavior. Release qualification builds a clean fixture copy
from the exact committed Odin candidate with
`-source-code-locations:filename`. During qualification, the commit-bound
candidate manifest supplies the proposed v0.2.1 candidate identity. The
historical hash below is not a current accepted v0.2.1 ROM identity.

## Controls

- D-pad Up/Down or the analog stick moves the left paddle.
- A serves after a point.
- Start resets the match.

Before a serve, holding a vertical direction moves the paddle through the same
ordinary gameplay path used during a rally. The automated test holds Down to
reach a deterministic pre-serve checkpoint; the ball remains waiting there
until A is pressed.

## Automated headless test

The automated gate uses `ares-test` from `HailToDodongo/ares-64` commit
`09008b610a16c375f793d0e124a366227bc4839c`. After building the ROM, run:

```sh
/path/to/ares-test pong.test.js n64_pong.z64 --timeout 30
```

The script enables homebrew mode, selects the CPU angrylion renderer, and holds
real port-1 Down input until ordinary fixed-step movement reaches the bottom
paddle clamp and submits a deterministic pre-serve frame. It then releases
Down, requires the structured readiness, input, state, allocator/timing, and
final PASS markers, and rejects every `ODIN_N64_PONG_FAIL:v1:` marker. The ball
remains waiting while the 640x240 scanout is captured. The test then presses A
through the same port-1 controller, requires the ROM to observe it, and requires
a rally marker after the ball moves. The captured pre-serve scanout must match
`pong.golden.png` pixel-for-pixel. On a mismatch the test writes
`pong.diff.png` and reports the actual RGB SHA-256.

To establish or intentionally replace the golden, capture a candidate rather
than writing the golden directly:

```sh
/path/to/ares-test test_capture_pong_candidate.js n64_pong.z64 \
  pong.candidate.png --timeout 30
```

Review `pong.candidate.png` visually, confirm the same bottom-clamp pre-serve
checkpoint on hardware, and then promote the candidate to `pong.golden.png`
through an explicit reviewed change. The capture helper refuses to overwrite
`pong.golden.png`. Finally, run the normal `pong.test.js` gate shown above. No
frame hash is accepted until this review workflow is complete.

Ubuntu/AMD64 is the authoritative headless environment. On Apple Silicon, use
the [documented Linux/AMD64 Docker-emulation
route](../o64_abi/README.md#apple-silicon-through-linuxamd64-docker-emulation);
native macOS and Linux/ARM64 headless runs are not claimed as supported.
Upstream Ares is for interactive play and visual debugging, not the automated
oracle. The v148 release has a CPU-framebuffer race described below; use the
verified post-fix nightly for this sample's interactive visual gate.

## Historical v0.2 Milestone 4 evidence

The v0.2 software-complete candidate built on 2026-08-31 had these identities:

- ROM SHA-256: `2259c158e6d1782196b9ed151554ee3c48379246920e09a65f736ca859c5782a`
- Controller-driven raw RGB SHA-256:
  `fc5b084bd52a02a4e3a3c9a73a5dfe72cdfca9b31f52d88ef4befe1acca1a767`
- Candidate PNG SHA-256:
  `ae8cd14fa4aa6168fc2d8f3ccaf6331a4f496dda5dc1718e6dff7e3670d1f108`

The reviewed candidate was confirmed on Analogue3D/SummerCart64 and promoted
to `pong.golden.png`. The game remains configured exactly as tested:
`display_init` at 320x240, 16 bpp, two buffers, and `FILTERS_DISABLED`.

Upstream Ares v148 intermittently showed flicker during paddle movement. The
same CPU-framebuffer race was reproduced in the untouched official libdragon
[`spriteanim` example](https://github.com/DragonMinded/libdragon/tree/preview/examples/spriteanim),
isolating it from the Odin game. Adding synchronization
reduced but did not eliminate the flicker; triple buffering, 32 bpp, and
resampling also did not fix it. This is an upstream Ares issue, not a change to
the historical v0.2 Pong ROM or display configuration. The
[official fix](https://github.com/ares-emulator/ares/commit/31526ded9196b046b0b5ce1769f14335eba9cc33)
is commit `31526ded9196b046b0b5ce1769f14335eba9cc33`.

The [official nightly](https://github.com/ares-emulator/ares/releases/tag/nightly)
`v148-116-g7b51c8ab7`, full revision
`7b51c8ab719e403a150aa700e0933d9e93a06851`, was verified with asset SHA-256
`f30393b324906b5545e5c0b0a3bcf09035089a8678a6f1ef52e2ab063c413753`.
Interactive keyboard Up/Down moved the paddle, and the user confirmed the
flicker was gone during movement. Diagnostic and post-fix evidence is retained
as [v148 flicker frames](ares-v148-flicker-frames.png), the [official
libdragon control](ares-v148-libdragon-spriteanim-control.png), and the
[post-fix movement capture](ares-nightly-fixed-movement.jpeg). Full controls
are covered across the automated controller test and the
Analogue3D/SummerCart64 playtest. See `PLAYTEST.md` for the scoped manual
results.

## Fixture diagnosis

The game writes structured progress and failure markers through libdragon's
debug channel. A headless failure includes the captured log, making the first
missing PASS marker or `ODIN_N64_PONG_FAIL:v1:<reason>` the best place to begin.
Use the main guide's
[inspection flags](../../N64_BUILD.md#load-and-debug-a-rom) for the generated
link and package graph.

The upstream-Ares and Analogue3D/SummerCart64 checks are independent release
gates. Their evidence is recorded in [PLAYTEST.md](PLAYTEST.md); an automated
pass is not a substitute for either manual check.
