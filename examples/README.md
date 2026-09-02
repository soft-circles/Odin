# Examples

The `example` directory contains two packages:

A [demo](demo) illustrating the basics of Odin.

It further contains [all](all), which imports all [core](/core) and [vendor](/vendor) packages so we can conveniently run `odin check` on everything at once.

For additional example code, see the [examples](https://github.com/odin-lang/examples) repository.

## Nintendo 64

The [Odin64 repository](https://github.com/soft-circles/Odin64/tree/main/examples)
owns canonical Pong and DragonFS examples and project-local libdragon bindings.
Start with this compiler's [N64 build guide](../N64_BUILD.md); compiler-only
lifecycle coverage is in [tests/n64_runtime](../tests/n64_runtime).
