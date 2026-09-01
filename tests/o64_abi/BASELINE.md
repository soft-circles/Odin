# Milestone 0 baseline record

Recorded on 2026-08-31 before the Milestone 0 harness changes.

| Component | Revision or provenance | Initial state |
| --- | --- | --- |
| LLVM | `ff570ff21ae6f2b1343084273d8bc3ad3a3d3cf4` (`o64`) | clean |
| Odin | `c92d9883b9a2562efe898ad1c769d969c9de8ef4` (`master`, four commits ahead of `origin/master`) | clean |
| libdragon checkout | `a23f659f4b5cc45e3733910bcfa64713e63021c5` (`preview`) | clean |
| installed libdragon SDK | `c79a52b42ac790e06e797aede43914dd8754cd5f`, `preview`, commit date 2026-08-18 | version record says clean |
| installed toolchain | host `aarch64-apple-darwin25.5.0`; binutils 2.47; GCC 16.2.0; newlib 4.4.0.20231231 | recorded by `toolchain.version` |
| headless runner | `HailToDodongo/ares-64` `09008b610a16c375f793d0e124a366227bc4839c`; tree `5a6d14d946d304c46b8f254aeb576e46af2d3a7b` | detached pin |

The runner's pinned submodules are SDL `c568c46f51e863e9c4658b31bfd6c455cc875c5f`,
Dear ImGui `5a76f2adf1b0403b86a45010121fb32a6bff8680`, and QuickJS-NG
`fd0a0210b7be00957751871e7e01b8291268fc29`.

LLVM's `clang` and Odin were rebuilt from the recorded LLVM and Odin heads
before the final verification run. The accepted results are:

- Phase 0 argument-location comparison: 49/49.
- LLVM/GCC execution differential: 43/43.
- Focused Odin/GCC differential: 19/19 leaf probes and 7/7 call-state probes.
- Linked Odin/GCC/libdragon differential: 23/23.
- ROM SHA-256: `824dbf5772b3e08348d0a57e410b143447350e36d6826968beba3d35e3179d6f`.
- ELF: ELF32, big-endian, statically linked, O64, MIPS III.
- Headless oracle: `PASS: Odin O64 ABI 23/23` within 10 emulated seconds and a 30-second wall-clock watchdog.

The final headless run exited 0 in Ubuntu 22.04.5 LTS on Linux/AMD64 through
Docker emulation on Apple Silicon. The image carried
`org.opencontainers.image.revision=09008b610a16c375f793d0e124a366227bc4839c`;
the built `ares-test` binary SHA-256 was
`1ec00a88ac281a1297f9ffb8466f203f9c3eb4479021376943cdead40f38d899`.

The authoritative headless host is Ubuntu/AMD64. Apple Silicon uses the
documented Linux/AMD64 Docker-emulation command and is not evidence of native
macOS or Linux/ARM64 support.
