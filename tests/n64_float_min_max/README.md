# Float min/max lowering

Run `python3 tests/n64_float_min_max/test_float_min_max.py` from the compiler checkout.
The test needs a built Odin compiler and Python, but no SDK or emulator.

The VR4300 has no float min/max instruction, so `llvm.minnum`/`llvm.maxnum`
lower to calls to newlib's software `fminf`/`fmaxf`/`fmin`/`fmax`. On n64 the
compiler picks inline with an FPU compare instead. The test builds N64 assembly
at `-o:none` and `-o:speed` and checks that `min`, `max` and `clamp` on `f32`
and `f64` use an inline compare and make no `fmin`/`fmax` call.

Results match newlib for every non-NaN operand, including equal operands such
as `-0` and `+0`, which yield the second one. A NaN operand reaches the FPU
compare, which traps while libdragon enables invalid-operation exceptions,
where newlib would return the other operand.

`ODIN` selects the compiler under test.
