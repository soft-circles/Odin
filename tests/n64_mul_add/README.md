# Multiply-add lowering

Run `python3 tests/n64_mul_add/test_mul_add.py` from the compiler checkout.
The test needs a built Odin compiler and Python, but no SDK or emulator.

The VR4300 has no fused multiply-add instruction, so `llvm.fma` lowers to a
call to newlib's software `fmaf`/`fma`. The test builds N64 assembly at
`-o:none` and `-o:speed` and checks that matrix-matrix and matrix-vector
products use inline `mul.s`/`mul.d` with no `fma` calls. An explicit
`intrinsics.fused_mul_add` still asks for fused rounding, so it must still call
`fmaf`.

`ODIN` selects the compiler under test.
