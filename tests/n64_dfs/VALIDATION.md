# Milestone 5 final validation ledger

Validated on 2026-09-01 from Odin revision
`c92d9883b9a2562efe898ad1c769d969c9de8ef4` plus the uncommitted Milestone
1–5 working tree, on macOS 26.6.2 (25G83), Apple Silicon.

## Accepted artifacts

- ROM `n64_dfs.z64`: SHA-256 `09299b4c7d1b236ddbd0128409aa991f3ca2cbfefc8579e87cdcd0a8404b579c`
- Reviewed `dfs.golden.png`: SHA-256 `a2c056858a6f44f22b18751f1d9edf9a65f8d43649e515101663522b83b9967e`
- Decoded 640x240 RGB framebuffer: SHA-256 `b3c012ba911d27fe55875dd209a56ea17542e341881a0ce42edc06ec0c1e88ef`
- Generated DragonFS image: SHA-256 `b017c7b6791a485ebb20bbc6d7f7872a2a480f1840e5ac54c1fb5d41f92c8632`
- `metadata.ini`: SHA-256 `f818ab577ad7987b5fef46243ae1253c5c9940be359a27fea434408c8beeed84`
- `assets/message.txt`: SHA-256 `fbba418f0a0739284738e1f83eed6a8bce0019099e4b29b343e3ef7dc21b5cf1`

The final one-command build completed with exit status 0 using the installed
libdragon SDK at `c79a52b42ac790e06e797aede43914dd8754cd5f`. Its retained
metadata directory contained only `odin-input-0000.ini`; no build-stage cycle
or unrelated sibling was staged. Rebuilding after that fix preserved both the
ROM and DragonFS hashes above.

`verify_rom.py` completed with exit status 0 and reported `DFS ROM package
verification passed`. It verified the big-endian ROM header and configured
fields, rompak TOC, exact retained DragonFS image, libdragon/toolchain version
payloads, metadata ZIP, source-identical INI, and `[meta.es]` section.

## Automated regression gates

The compiler was rebuilt in release mode before these final runs. All commands
completed with exit status 0:

- N64 public build/option suite: 26/26 tests.
- O64 argument-location baseline: 49/49.
- LLVM/GCC execution differential: 43/43.
- Odin/GCC focused differential: 19/19, plus 7/7 call-state probes.
- Linked Odin/GCC/libdragon interop: 23/23.
- Exact libdragon binding layout build: passed.
- Exact libdragon binding runtime: `PASS: Odin libdragon binding ABI 23/23`.
- `git diff --check`: passed.

The authoritative headless runs used Linux/AMD64 container image
`sha256:23f1811f51d15a26c59c64a1a6c3169121f7a39680300f2760a26c1d12eb70ec`,
labeled with pinned `ares-test` revision
`09008b610a16c375f793d0e124a366227bc4839c`. The final normal DFS golden gate
completed with exit status 0 and observed, in order:

```text
ODIN_N64_DFS_CHECK:v1:MOUNT:PASS
ODIN_N64_DFS_CHECK:v1:OPEN:PASS
ODIN_N64_DFS_CHECK:v1:SIZE:PASS
ODIN_N64_DFS_CHECK:v1:READ:PASS
ODIN_N64_DFS_CHECK:v1:CONTENT:PASS
ODIN_N64_DFS_CHECK:v1:CLOSE:PASS
ODIN_N64_DFS_STATE:v1:FRAME_SUBMITTED
ODIN_N64_DFS_READY:v1
ODIN_N64_DFS_PASS:v1
```

The runner reported that the framebuffer matched `dfs.golden.png` with raw RGB
SHA-256 `b3c012ba911d27fe55875dd209a56ea17542e341881a0ce42edc06ec0c1e88ef`.

## Manual gates

- Official upstream Ares nightly `v148-116-g7b51c8ab7`: PASS, including reset
  and a two-minute stability check. The downloaded archive SHA-256 was
  `f30393b324906b5545e5c0b0a3bcf09035089a8678a6f1ef52e2ab063c413753`.
- Analogue3D and SummerCart64: PASS, confirmed by the user on 2026-09-01.
  Firmware versions and photo/video evidence were not supplied.

See `PLAYTEST.md` for the full manual checklist and evidence path.
