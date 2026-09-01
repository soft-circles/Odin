# Odin N64 console lifecycle fixture

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

The C `main` and Makefile are milestone-1 scaffolding. A later milestone moves
entry, linking, and ROM packaging into the Odin N64 target.
