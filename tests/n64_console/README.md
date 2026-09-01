# Odin N64 console lifecycle fixture

For SDK setup and ordinary ROM builds, see [`N64_BUILD.md`](../../N64_BUILD.md).
For the raw console ownership rule, see the
[`vendor:libdragon` guide](../../vendor/libdragon/README.md#console-lifecycle).

This fixture is intentionally separate from the direct-display tracer because
libdragon's console initializes and owns display state. Odin initializes the
console, clears and renders it, closes it, then reacquires direct display and
presents a fixed confirmation frame.

Build and run it from the Odin repository root:

```sh
N64_INST=/path/to/n64_toolchain make -C tests/n64_console clean all
N64_INST=/path/to/n64_toolchain ARES_TEST=/path/to/ares-test \
  make -C tests/n64_console check
```

The C `main` and Makefile are retained legacy scaffolding for this focused
lifecycle probe. New project-facing samples use the integrated Odin build path.
