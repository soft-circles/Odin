# Pinned libdragon binding ABI probe

This fixture gates the concrete tracer-facing Odin declarations against the
installed libdragon SDK at commit
`c79a52b42ac790e06e797aede43914dd8754cd5f`.

Both C and Odin compile-time assertions cover these layouts:

| Type | Size | Alignment | Field offsets |
| --- | ---: | ---: | --- |
| `resolution_t` | 20 | 4 | `width=0`, `height=4`, `interlaced=8`, `aspect_ratio=12`, `overscan_margin=16` |
| `joypad_buttons_t` | 2 | 2 | `raw=0` |
| `joypad_inputs_t` | 8 | 1 | `btn=0`, `stick_x=2`, `stick_y=3`, `cstick_x=4`, `cstick_y=5`, `analog_l=6`, `analog_r=7` |
| C `_Bool` / Odin `bool` | 1 | 1 | n/a |

The ROM runs 23 linked calls. It transports each aggregate by value in both
directions as a parameter and a result, transports false and true `_Bool`
parameters and results in both directions, and calls the real libdragon
`debug_init_emulog` and `console_set_debug` bindings. Both false and true are
sent through the real bool-parameter binding.

During initial execution, the uncorrected compiler byte-swapped ordinary
multi-byte integer literals on this big-endian target. In `odin_side.o`, the
body for `return u32(0x46414c53)` contained `lui 0x534c; ori 0x4146`, returning
`0x534c4146`; a comparison with `i32(2)` used `0x02000000`. That run failed
16/23 probes. The compiler now swaps only explicitly endian-qualified values
whose endianness differs from the target. This fixture deliberately uses
ordinary typed literals again so its runtime gate covers that prerequisite as
well as aggregate transport.

Build the layout gates and ROM with the pinned installed SDK:

```sh
N64_INST=/path/to/n64_toolchain make clean all
```

Run it with `ares-test` from `HailToDodongo/ares-64` commit
`09008b610a16c375f793d0e124a366227bc4839c`:

```sh
N64_INST=/path/to/n64_toolchain \
ARES_TEST=/path/to/ares-test make check
```

The runner requires `PASS: Odin libdragon binding ABI 23/23`, rejects the
fixture-specific failure prefix, allows 10 emulated seconds, and enforces a
45-second host-side timeout around the runner's 30-second watchdog.
