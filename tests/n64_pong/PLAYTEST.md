# N64 Pong v0.1 playtest record

This file records the evidence for the two manual Milestone 4 play gates. An
unchecked box means that specific observation was not performed or recorded;
do not infer it from a section's scoped result or from the automated headless
test.

Use the same release-candidate `.z64` for both sections. Record its identity
before either playtest:

- ROM filename: `n64_pong.z64`
- ROM SHA-256: `2259c158e6d1782196b9ed151554ee3c48379246920e09a65f736ca859c5782a`
- Odin revision: `c92d988 + current Milestone 4 worktree`
- libdragon SDK revision: `c79a52b42ac790e06e797aede43914dd8754cd5f`

## Upstream Ares interactive gate

### v148 diagnostic control

- Date (YYYY-MM-DD): `2026-08-31`
- Host and OS version: `macOS 26.6.2 (25G83)`
- Ares version/build identity: `v148 / dev.ares.ares application bundle`
- Flicker reproduced during Pong paddle movement: [x] yes [ ] no
- Same race reproduced in untouched official libdragon `spriteanim`: [x] yes [ ] no
- Synchronization eliminated the race: [ ] yes [x] no; it only reduced it
- Triple buffering eliminated the race: [ ] yes [x] no
- 32 bpp eliminated the race: [ ] yes [x] no
- Resampling eliminated the race: [ ] yes [x] no
- Upstream fix commit: `31526ded9196b046b0b5ce1769f14335eba9cc33`
- Evidence: [v148 flicker frames](ares-v148-flicker-frames.png)
- Control evidence: [official libdragon `spriteanim`](ares-v148-libdragon-spriteanim-control.png)

### Post-fix nightly result

- Tester: `User-confirmed interactive check`
- Date (YYYY-MM-DD): `2026-08-31`
- Ares version: `v148-116-g7b51c8ab7`
- Full revision: `7b51c8ab719e403a150aa700e0933d9e93a06851`
- Nightly asset SHA-256: `f30393b324906b5545e5c0b0a3bcf09035089a8678a6f1ef52e2ab063c413753`
- ROM loaded with homebrew support enabled: [x] yes [ ] no
- Playfield rendered without flicker during paddle movement: [x] yes [ ] no
- Keyboard Up/Down moved the player's paddle both ways: [x] yes [ ] no
- A and Start mappings were available in this session: [ ] yes [x] no
- A served the ball after a point: [ ] yes [ ] no
- Start reset the match: [ ] yes [ ] no
- Ball motion and paddle/wall collisions were visibly correct: [ ] yes [ ] no
- Score changed after a missed ball and play continued: [ ] yes [ ] no
- No hang or crash during at least 2 minutes of play: [ ] yes [ ] no
- Screenshot evidence: [post-fix paddle movement](ares-nightly-fixed-movement.jpeg)
- Result: [x] PASS [ ] FAIL
- Failure/issue reference, or `none`: `none after upstream fix`
- Notes: A and Start were not mapped in this Ares session. Those controls and
  full gameplay are covered across the headless controller gate and hardware
  PASS; the unperformed Ares items remain unchecked.

## Analogue3D with SummerCart64

- Tester: `User-confirmed hardware playtest`
- Date (YYYY-MM-DD): `2026-08-31`
- Analogue3D firmware version: `not reported`
- SummerCart64 firmware/version: `not reported`
- Controller model and port: `port 1; model not reported`
- SHA-256 of the copied SD-card ROM matched the value above: [x] yes [ ] no
- Cold boot reached the playfield: [x] yes [ ] no
- Port 1 controls moved the player's paddle in both directions: [x] yes [ ] no
- A served the ball after a point: [x] yes [ ] no
- Start reset the match: [x] yes [ ] no
- Ball motion and paddle/wall collisions were visibly correct: [x] yes [ ] no
- Score changed after a missed ball and play continued: [x] yes [ ] no
- Bottom-clamp pre-serve checkpoint matched the reviewed golden: [x] yes [ ] no
- Reset/relaunch returned to a playable initial state: [x] yes [ ] no
- No hang or crash during at least 5 minutes of play: [ ] yes [ ] no
- Photo/video/debug-log evidence path: `user confirmation in the Milestone 4 task`
- Result: [x] PASS [ ] FAIL
- Failure/issue reference, or `none`: `none`
- Notes: The user confirmed controls and graphics on
  Analogue3D/SummerCart64; firmware, controller model, and duration were not
  reported.

Milestone 4 manual evidence is complete only when both result fields are
explicitly marked `PASS`, the recorded ROM SHA-256 matches, and the combined
evidence establishes playable controls and correct graphics. The post-fix Ares
result is scoped to its performed visual and keyboard checks; A, Start, full
gameplay, and hardware behavior are covered across the automated and
Analogue3D/SummerCart64 gates. Firmware, controller-model, and duration fields
add reproducibility context; record them when available rather than inferring
them.
