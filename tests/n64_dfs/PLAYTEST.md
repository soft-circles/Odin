# N64 DFS/metadata v0.2 playtest record

This record captures the two manual Milestone 5 gates. An unchecked box means
that observation was not performed; do not infer it from the headless test or
from another checkbox.

Use the same release-candidate ROM for both sections and record its identity:

- ROM filename: `n64_dfs.z64`
- ROM SHA-256: `09299b4c7d1b236ddbd0128409aa991f3ca2cbfefc8579e87cdcd0a8404b579c`
- Candidate raw RGB SHA-256: `b3c012ba911d27fe55875dd209a56ea17542e341881a0ce42edc06ec0c1e88ef`
- Candidate PNG SHA-256: `a2c056858a6f44f22b18751f1d9edf9a65f8d43649e515101663522b83b9967e`
- Odin revision: `c92d9883b9a2562efe898ad1c769d969c9de8ef4` plus the recorded Milestone 1-5 working tree
- libdragon SDK revision: `c79a52b42ac790e06e797aede43914dd8754cd5f`
- Metadata INI SHA-256: `f818ab577ad7987b5fef46243ae1253c5c9940be359a27fea434408c8beeed84`
- Raw asset SHA-256: `fbba418f0a0739284738e1f83eed6a8bce0019099e4b29b343e3ef7dc21b5cf1`

## Upstream Ares interactive gate

- Tester: `Codex interactive verification`
- Date (YYYY-MM-DD): `2026-09-01`
- Host and OS version: `macOS 26.6.2 (25G83), Apple Silicon`
- Ares version/build identity: official nightly `v148-116-g7b51c8ab7`, archive SHA-256 `f30393b324906b5545e5c0b0a3bcf09035089a8678a6f1ef52e2ab063c413753`
- ROM reached the success panel: [x] yes [ ] no
- Panel showed `DRAGONFS MOUNTED`: [x] yes [ ] no
- Panel showed `READ message.txt: 22 BYTES`: [x] yes [ ] no
- Panel showed `ODIN N64 DFS ASSET v1`: [x] yes [ ] no
- Panel matched `dfs.candidate.png`: [x] yes [ ] no
- Reset/relaunch reproduced the same panel: [x] yes [ ] no
- No flicker, hang, or crash during at least 2 minutes: [x] yes [ ] no
- Screenshot/debug-log evidence path: `ares-nightly-dfs-reset.jpeg`; pinned headless log reproduced every ordered v1 sentinel
- Result: [x] PASS [ ] FAIL
- Failure/issue reference, or `none`: `none`
- Notes: The first build returned from `main` and Ares unloaded it after the submitted frame. The accepted ROM loops after rendering; its unchanged panel survived the soak and console reset.

## Analogue3D with SummerCart64

- Tester: `user hardware confirmation`
- Date (YYYY-MM-DD): `2026-09-01`
- Analogue3D firmware version: `not supplied`
- SummerCart64 firmware/version: `not supplied`
- SHA-256 of the copied SD-card ROM matched the value above: [x] yes [ ] no
- Cold boot reached the success panel: [x] yes [ ] no
- Panel showed `DRAGONFS MOUNTED`: [x] yes [ ] no
- Panel showed `READ message.txt: 22 BYTES`: [x] yes [ ] no
- Panel showed `ODIN N64 DFS ASSET v1`: [x] yes [ ] no
- Panel matched `dfs.candidate.png`: [x] yes [ ] no
- Reset/relaunch reproduced the same panel: [x] yes [ ] no
- No flicker, hang, or crash during at least 5 minutes: [x] yes [ ] no
- Photo/video/debug-log evidence path: `not supplied`
- Result: [x] PASS [ ] FAIL
- Failure/issue reference, or `none`: `none`
- Notes: User confirmed the release candidate passed on Analogue3D and SC64.

Both result fields are explicitly marked `PASS`; the reviewed candidate was
promoted unchanged to `dfs.golden.png` after the two manual checks confirmed
the same deterministic success panel.
