# MIPS O64 baseline verification

These tests lock the compiler and ROM baseline used by the Odin N64 work. The
libdragon fixture accepts only SDK commit
`c79a52b42ac790e06e797aede43914dd8754cd5f`; `interop/Makefile` runs
`validate_sdk.py` before compiling either object.

## Regression gates

Run the first command from the workspace's `test` directory and the remaining
commands from the LLVM/Odin workspace root:

```sh
(cd test && python3 compare.py)
python3 llvm-project/llvm/utils/o64-abi-differential.py
python3 odin/tests/o64_abi/differential.py
N64_INST=/path/to/n64_toolchain make -C odin/tests/o64_abi/interop clean all check
```

The expected summaries are 49/49 argument locations, 43/43 LLVM/GCC execution
differentials, 19/19 focused Odin/GCC leaf probes plus 7/7 call-state probes,
and 23/23 linked Odin/GCC/libdragon probes. The Milestone 0 accepted ROM
SHA-256 was:

```text
824dbf5772b3e08348d0a57e410b143447350e36d6826968beba3d35e3179d6f
```

Milestone 1 fixed ordinary integer constant lowering on big-endian targets,
which intentionally changed bytes in the rebuilt Odin object without changing
the 23/23 ABI result. The clean post-fix baseline ROM SHA-256 is:

```text
d42a9772d84f81a0935fda616b9ca00aa850bfda4a82a0c9e74a3100aeee47e1
```

## Exact libdragon binding gate

The `libdragon_bindings` fixture adds C and Odin layout assertions plus 23
linked runtime calls for `resolution_t`, `joypad_buttons_t`,
`joypad_inputs_t`, and C `_Bool`. From the Odin repository root:

```sh
N64_INST=/path/to/n64_toolchain \
  make -C tests/o64_abi/libdragon_bindings clean all check-layout
N64_INST=/path/to/n64_toolchain ARES_TEST=/path/to/ares-test \
  make -C tests/o64_abi/libdragon_bindings check
```

## Headless ROM gate

Use `ares-test` from `HailToDodongo/ares-64` commit
`09008b610a16c375f793d0e124a366227bc4839c`. On Ubuntu/AMD64, after building
that revision's `linux-headless` preset, run from the Odin repository root:

```sh
/path/to/ares-test tests/o64_abi/interop/baseline.test.js \
  tests/o64_abi/interop/interop.z64 --timeout 30
```

The JavaScript enables homebrew mode before loading the ROM, waits at most 10
emulated seconds for the unique `PASS: Odin O64 ABI 23/23` sentinel, rejects a
failure sentinel, and relies on the runner's 30-second wall-clock watchdog.

### Apple Silicon through Linux/AMD64 Docker emulation

Native macOS and Linux/ARM64 headless execution are not supported by this gate.
Build the pinned runner and label its source revision:

```sh
git clone --recursive https://github.com/HailToDodongo/ares-64.git /tmp/odin-ares-64
git -C /tmp/odin-ares-64 checkout --detach 09008b610a16c375f793d0e124a366227bc4839c
git -C /tmp/odin-ares-64 submodule update --init --recursive
docker build --platform linux/amd64 --target ares-builder \
  --label org.opencontainers.image.revision=09008b610a16c375f793d0e124a366227bc4839c \
  -t odin-ares-test:09008b610a /tmp/odin-ares-64
```

Then, from the Odin repository root, expose only the fixture directory and keep
the mount read-only:

```sh
docker run --rm --platform linux/amd64 \
  --mount type=bind,src="$PWD/tests/o64_abi/interop",dst=/fixture,readonly \
  odin-ares-test:09008b610a /opt/ares-test/ares-test \
  /fixture/baseline.test.js /fixture/interop.z64 --timeout 30
```

Verify the image pin independently with:

```sh
docker image inspect odin-ares-test:09008b610a \
  --format '{{index .Config.Labels "org.opencontainers.image.revision"}}'
```
