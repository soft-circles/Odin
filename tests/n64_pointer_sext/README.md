# Pointer sign extension

Run `python3 tests/n64_pointer_sext/test_pointer_sext.py` from the compiler checkout.
The test needs a built Odin compiler and Python, but no SDK or emulator. `ODIN`
selects the compiler under test.

The N64 keeps 32-bit pointers in 64-bit registers, and a KSEG0 address such as
`0x80001000` is only valid sign-extended, as `0xffffffff80001000`. GCC-built
libdragon relies on that for every pointer argument. The fixture builds at
`-o:speed` and checks two cases that LLVM used to get wrong:

- a pointer read from the upper half of a by-value struct's register slot must
  be extracted with `dsra`, not `dsrl`;
- a constant KSEG0 pointer argument must be built with `lui`/`ori`, not as a
  zero-extended 64-bit constant.

The fix is in the MIPS backend of the soft-circles/llvm-project fork, so this
test fails with an LLVM older than that fix.
