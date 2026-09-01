# Examples

The `example` directory contains two packages:

A [demo](demo) illustrating the basics of Odin.

It further contains [all](all), which imports all [core](/core) and [vendor](/vendor) packages so we can conveniently run `odin check` on everything at once.

For additional example code, see the [examples](https://github.com/odin-lang/examples) repository.

## Nintendo 64

The N64 programs live with the integration fixtures that build and verify the
same source; they are not duplicated under `examples`:

- [Pong](../tests/n64_pong) is the visible, playable quickstart.
- [Runtime tracer](../tests/n64_tracer) covers startup, allocators, debug output,
  display, input, and elapsed time.
- [DragonFS and metadata](../tests/n64_dfs) packages and reads a raw asset and
  renders a deterministic success panel.

Start with the [N64 build guide](../N64_BUILD.md). The
[`vendor:libdragon` guide](../vendor/libdragon/README.md) documents the raw API
used by these programs.
