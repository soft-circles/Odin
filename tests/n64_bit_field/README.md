# bit_field fields on a big-endian target

A `bit_field` numbers its bits like its backing integer: field 0 starts at bit 0,
the `0x0001` mask, as the constant lowering already does. Run-time field loads and
stores copy bits through memory, so on `-target:n64` (big-endian) each byte of a
backing integer, and of the field value, is read at its mirrored address. An
array backing mirrors each element. Little-endian targets keep plain memory order,
and their code generation is unchanged.

The ROM stores and loads fields of `u16`, `u32` and `[2]u16` backings, including
booleans, fields that cross bytes and elements, and a sign-extended field, and
compares each with the integer it should match.

```sh
./odin check tests/n64_bit_field/rom -target:n64 -vet -warnings-as-errors
./odin build tests/n64_bit_field/rom -target:n64 -out:bit_field.z64
"$ARES_TEST" tests/n64_bit_field/rom.test.js bit_field.z64 --timeout 30
```

Building the ROM needs an [installed SDK](../../N64_BUILD.md); running it needs
the ares-64 `ares-test` runner.
