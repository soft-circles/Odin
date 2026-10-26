# Bounds-check elision

Run `python3 tests/n64_bounds_check/test_bounds_check.py` from the compiler checkout.
The test needs a built Odin compiler and Python, but no SDK or emulator.

Upstream drops bounds checks it can prove redundant. The test builds N64
assembly at `-o:none -use-single-module`, where every check left is one the
frontend emitted, and counts each procedure's references to
`runtime.bounds_check_error*` and `runtime.slice_expr_error*`.

These keep exactly the checks they had before elision: a slice index past a
`len` guard, a fixed array index masked past its length, a struct-field slice
guarded by another field, a loop that runs one past `len`, a slice expression
`s[lo:hi]` with only `lo <= hi` known, and two procedures from upstream
2804ac31c that check `s[k]` before a loop whose bound or iterator call reads
`s[k]` again while the body changes `k` (two checks each).

These must have none: a constant in-range index into a fixed array, a constant
in-range slice of a fixed array, and a loop over `0..<len(s)`. The last one shows
the elision is active, so the probe isn't vacuous.

At `-o:speed` LLVM removes provably redundant checks itself, so that build only
checks that the procedures in the first group still reach a check handler.

`ODIN` selects the compiler under test.
