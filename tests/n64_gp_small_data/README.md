# Small data through $gp

Run `python3 tests/n64_gp_small_data/test_gp_small_data.py` from the compiler checkout.
The test needs a built Odin compiler and Python, but no SDK or emulator. `ODIN`
selects the compiler under test.

libdragon's GCC builds with `-G 8`: it puts globals of 8 bytes or less in `.sdata`
and `.sbss`, and reaches them in one instruction from `$gp`, which libdragon's
`n64.ld` sets to `_gp`. `-target:n64` does the same, so a global costs one `lw`
instead of `lui` and `lw`, and C and Odin agree on where a small global lives. The
fixture builds at `-o:speed` and checks that:

- a 4-byte and an 8-byte global, and a 4-byte foreign one, are read and written as
  `%gp_rel(...)($gp)`, and the defined ones are in `.sbss`;
- a float constant loads from the constant pool through `$gp`;
- a choice between two small globals' addresses builds, as `$gp` plus the
  chosen `%gp_rel` offset (the LLVM fork selects that offset on its own);
- a 9-byte global, a global with its own `link_section` and a zero-sized foreign
  one keep `%hi`/`%lo`;
- `-target:freestanding_mips32be`, which has no `_gp`, never uses `$gp`.

With stock LLVM, as in CI's quick mode, these checks skip; see
[tests/n64_validation/o64_fork.py](../n64_validation/o64_fork.py).
