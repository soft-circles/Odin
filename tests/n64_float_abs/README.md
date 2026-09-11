# Floating-point absolute value

Run `python3 tests/n64_float_abs/test_float_abs.py` from the compiler checkout.
The test needs a built Odin compiler and Python, but no SDK or emulator.

It checks host values, including signed zero and mantissa bit 7, and inspects
host/N64 LLVM IR for all native, little-endian and big-endian `f16`, `f32` and
`f64` sign masks. The N64 cases catch treating the native big-endian target as
though its float storage were byte-swapped relative to native integers.

`ODIN` selects the compiler under test. A Mips-only LLVM build cannot execute
host programs; set `HOST_ODIN` to a host-capable build of the same compiler
revision for that configuration. With an ordinary host LLVM build, the default
compiler can run both parts. Odin64 quick CI runs this test, including cache hits.
