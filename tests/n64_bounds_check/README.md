# Bounds-check elision

Run `python3 tests/n64_bounds_check/test_bounds_check.py` from the compiler checkout.
The test needs a built Odin compiler and Python, but no SDK or emulator.

Upstream drops bounds checks it can prove redundant. The test builds N64
assembly at `-o:speed` and checks that indices it cannot prove in range still
call `runtime.bounds_check_error*`: a slice index past a `len` guard, a fixed
array index masked past its length, a struct-field slice guarded by another
field, and a loop that runs one past `len`. A constant in-range index into a
fixed array and a loop over `0..<len(s)` must have no check.

`ODIN` selects the compiler under test.
