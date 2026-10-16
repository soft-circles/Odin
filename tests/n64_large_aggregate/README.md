# Large aggregate arguments

Run `python3 tests/n64_large_aggregate/test_large_aggregate.py` from the compiler
checkout. The test needs a built Odin compiler and Python, but no SDK or emulator.
`ODIN` selects the compiler under test.

O64 passes a struct by value in consecutive 8-byte slots: the first four in
`$4`-`$7`, the rest in the caller's outgoing argument area. The compiler used to
hand LLVM every such struct as a first-class aggregate of `size/8` integers,
whatever its size. LLVM's MIPS instruction selection takes super-linear time and
memory on those: a 70 KB `Static_Handle_Map` passed by value ran for many minutes
and gigabytes, and a 32 KB one hit a register scavenger fatal error.

A struct wider than the four argument registers now goes indirect:

- an Odin procedure takes a pointer to the caller's value, as on the other
  targets, since Odin parameters are immutable;
- a C procedure gets it `byval`, which LLVM lays out in the same registers and
  stack slots as GCC but copies with `memcpy`.

The fixture builds a 64 KB struct for both calling conventions, checks that a
24-byte struct stays in registers, and checks a 48-byte C struct's register and
stack layout.
