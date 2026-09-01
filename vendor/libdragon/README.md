# Raw libdragon binding for Nintendo 64

`vendor:libdragon` is a handwritten, deliberately narrow binding for the
libdragon revision
`c79a52b42ac790e06e797aede43914dd8754cd5f`. It exists to support the current
Odin N64 runtime fixtures and canonical samples. It is not a complete binding,
and the presence of an API in upstream libdragon does not imply that it is
available or compatible here.

See [`N64_BUILD.md`](../../N64_BUILD.md) for SDK setup and ROM packaging, and
[`N64_MAINTAINERS.md`](../../N64_MAINTAINERS.md) before changing the pin or ABI
surface.

## Bound subsystems

The package currently exposes only:

| Area | Bound surface |
| --- | --- |
| Debug | EMUX initialization and formatted logging |
| Display | Basic resolution, bit depth, gamma/filter selection, buffer acquire, and buffer submit |
| CPU graphics | Solid fills, boxes, the default font, color selection, and text |
| Joypads | Initialization, polling, four ports, current inputs, and raw button masks |
| Timing | Millisecond tick counter |
| Console | Initialization, debug mirroring, manual/automatic render mode, clear, render, and close |
| DragonFS | Mount, open, size, read, and close |

There are no bindings here for RDPQ, sprites, textures, audio, save devices,
interrupts, timers, threads, callbacks, networking, or most other libdragon
subsystems.

From the Odin repository root, inspect the declaration-level documentation
with:

```sh
./odin doc vendor:libdragon -target:n64
```

## Import and debug logging

```odin
import ld "vendor:libdragon"

if ld.debug_init_emulog() {
	ld.debugf("Odin N64 started: %d\n", 1)
}
```

`debugf` is a C variadic function. Keep arguments compatible with the C format
string; the binding cannot make mismatched format arguments safe.

## Display and CPU graphics

Initialize display once, acquire a surface, draw only to that surface, and
submit it once:

```odin
ld.display_init(
	ld.RESOLUTION_320x240,
	.DEPTH_16_BPP,
	2,
	.GAMMA_NONE,
	.FILTERS_DISABLED,
)

frame := ld.display_get()
if frame != nil {
	background := ld.graphics_make_color(8, 16, 32, 255)
	white := ld.graphics_make_color(255, 255, 255, 255)
	ld.graphics_fill_screen(frame, background)
	ld.graphics_set_default_font()
	ld.graphics_set_color(white, background)
	ld.graphics_draw_text(frame, 24, 24, "HELLO FROM ODIN")
	ld.display_show(frame)
}
```

`surface_t` is intentionally opaque. Odin code must not allocate it by value or
guess its fields; it only passes pointers returned by libdragon.

## Joypad polling and button masks

Call `joypad_init` once. Each input cycle must call `joypad_poll` before reading
current inputs:

```odin
ld.joypad_init()

ld.joypad_poll()
input := ld.joypad_get_inputs(.JOYPAD_PORT_1)
if input.btn.raw & ld.JOYPAD_BUTTON_A != 0 {
	ld.debugf("A is held\n")
}
```

The C button type contains bitfields, whose layout is implementation-defined.
The Odin binding exposes only its ABI-stable 16-bit `raw` member. Use the
`JOYPAD_BUTTON_*` masks rather than declaring matching bitfields. The constants
are defined for libdragon's pinned big-endian N64 layout; do not byte-swap the
value before applying them.

## Elapsed time

`get_ticks_ms` returns libdragon's monotonic elapsed millisecond counter:

```odin
start := ld.get_ticks_ms()
// Do work.
elapsed := ld.get_ticks_ms() - start
```

Handle counter differences with unsigned arithmetic. This narrow binding does
not expose timer callbacks.

## Console lifecycle

The console owns display state while it is active. Do not mix console rendering
with direct `display_get`/`display_show` calls. Close the console before
reinitializing direct display:

```odin
ld.console_init()
ld.console_set_debug(true)
ld.console_set_render_mode(ld.RENDER_MANUAL)
ld.console_clear()
ld.console_render()
ld.console_close()

ld.display_init(
	ld.RESOLUTION_320x240,
	.DEPTH_16_BPP,
	2,
	.GAMMA_NONE,
	.FILTERS_DISABLED,
)
```

The focused lifecycle fixture is
[`tests/n64_console`](../../tests/n64_console).

## DragonFS

Package a raw directory with `-n64-assets`, then mount the embedded image once
and close every successful handle:

```odin
import "core:c"
import ld "vendor:libdragon"

if ld.dfs_init(ld.DFS_DEFAULT_LOCATION) == ld.DFS_ESUCCESS {
	handle := ld.dfs_open("message.txt")
	if handle >= 0 {
		size := ld.dfs_size(u32(handle))
		buffer: [256]byte
		if size >= 0 && size <= c.int(len(buffer)) {
			count := ld.dfs_read(rawptr(&buffer[0]), 1, size, u32(handle))
			_ = count
		}
		_ = ld.dfs_close(u32(handle))
	}
}
```

`dfs_open`, `dfs_size`, and `dfs_read` use negative results for failure. A
successful `dfs_open` result is converted to `u32` only after that check. The
complete checked example is
[`tests/n64_dfs/dfs.odin`](../../tests/n64_dfs/dfs.odin).

## C-to-Odin type rules

Binding changes must state the ABI, not merely the apparent source meaning:

- Use `c.int` for C `int` parameters, results, enum storage, sizes, and status
  codes. Do not substitute Odin `int`, whose N64 size follows the Odin target.
- Use `c.bool` for C99 `_Bool` in foreign signatures. The pinned ABI stores it
  in one byte; [`abi.odin`](abi.odin) asserts its size and alignment.
- Represent pointer-only C types as opaque Odin structs, such as `surface_t`,
  and pass only pointers obtained from the C library.
- Match concrete C structs field-for-field and add size, alignment, and offset
  assertions. Use `#packed` only when the C layout requires byte alignment.
- Represent a C union that crosses the ABI as `struct #raw_union`. Omit C
  bitfield views and expose stable raw storage plus masks.
- Use `cstring` for C `char const *` inputs. C strings must remain
  NUL-terminated and alive for as long as the C API retains them. The currently
  bound procedures consume their string arguments during the call.
- Use fixed-width integers where the C typedef fixes width, such as the `u32`
  `pi_addr_t`.
- Preserve `proc "c"`/the foreign block's C calling convention on every
  crossing declaration.

The target is big-endian. Integer fields and aggregates follow the target ABI;
do not manually reverse ordinary integer values. Raw byte buffers remain byte
sequences and must be interpreted according to their format.

## Ownership and ordering

This is a raw binding. It does not add RAII, deferred cleanup, state tracking,
or context installation:

- the caller initializes each subsystem before use;
- the caller obeys display acquire/draw/show ordering;
- the caller polls joypads before reading them;
- console and direct display ownership do not overlap;
- the caller closes DragonFS handles and validates all signed status results;
- the caller keeps C strings and buffers valid for the required call duration;
- the caller does not invoke missing callback-oriented APIs from C as a way to
  bypass Odin's unsupported callback-context contract.

Consult the upstream
[libdragon API reference](https://libdragon.dev/ref/index.html) for the C
semantics, but check the pinned source revision when declarations differ. The
live upstream documentation may describe APIs newer than this binding and does
not expand its compatibility promise.

## Adding or updating a declaration

Every newly exposed concrete type that crosses the C/Odin boundary must be
added to the ABI probe in
[`tests/o64_abi/libdragon_bindings`](../../tests/o64_abi/libdragon_bindings).
The probe needs matching C and Odin compile-time layout assertions plus runtime
parameter/result transport in both directions. Pointer-only opaque types still
need a documented ownership and lifecycle rule.

For a pin update:

1. follow the exact checklist in
   [`N64_MAINTAINERS.md`](../../N64_MAINTAINERS.md#updating-the-toolchain-pins);
2. compare every bound declaration with the new headers;
3. extend and run the C/Odin layout and runtime ABI probe;
4. run the integrated public-build, tracer, Pong, and DFS gates;
5. update this compatibility statement only after the coordination lock and
   checked-in compiler constants agree.
