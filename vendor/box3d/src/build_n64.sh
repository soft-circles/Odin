#!/usr/bin/env sh
# Builds lib/n64/libbox3d.a for -target:n64 with the libdragon SDK at N64_INST.
# Flags follow libdragon's N64_CFLAGS without its warnings, -ftrivial-auto-var-init=pattern and the -ffast-math group,
# so Box3D keeps IEEE semantics, and with NDEBUG, which turns B3_ASSERT off. Scalar math (BOX3D_DISABLE_SIMD), and
# B3_MAX_WORLDS defaults to 1 because each world slot costs about 3 KiB of BSS; set BOX3D_MAX_WORLDS to reserve more,
# and pass the same value to Odin as -define:BOX3D_MAX_WORLDS so vendor:box3d's MAX_WORLDS matches.
# B3_FORCE_OVERFLOW solves every contact with the scalar solver, which drops the wide solver's lane-by-lane emulation
# (about 210 KB of code) at no cost in step time. B3_DISABLE_RECORDING compiles out recording (about 17 KB), so
# b3World_StartRecording does nothing. The worker arena starts at BOX3D_ARENA_CAPACITY bytes, 32 KiB by default
# instead of 128 KiB: it holds one contact's scratch, about 300 bytes per mesh triangle touched, and a step that
# needs more falls back to the heap and grows the arena.
set -e

: "${N64_INST:?Set N64_INST to the libdragon SDK}"
ODIN_ROOT=${ODIN_ROOT:-$(cd "$(dirname "$0")/../../.." && pwd)}
max_worlds=${BOX3D_MAX_WORLDS:-1}
arena_capacity=${BOX3D_ARENA_CAPACITY:-32768}
cc="$N64_INST/bin/mips64-elf-gcc"
ar="$N64_INST/bin/mips64-elf-ar"

cd "$ODIN_ROOT/vendor/box3d/src" || exit 1

out=../lib/n64
objects=build/n64
rm -rf "$objects"
mkdir -p "$out" "$objects"
for src in src/*.c; do
	"$cc" -c -std=gnu17 -O2 -g -DN64 -DNDEBUG -DBOX3D_DISABLE_SIMD -DB3_MAX_WORLDS="$max_worlds" \
		-DB3_FORCE_OVERFLOW=1 -DB3_DISABLE_RECORDING -DB3_ARENA_CAPACITY="$arena_capacity" \
		-march=vr4300 -mtune=vr4300 -mabi=o64 \
		-I"$N64_INST/mips64-elf/include/newlib_overrides" -I"$N64_INST/mips64-elf/include" -include ktls.h \
		-falign-functions=32 -ffunction-sections -fdata-sections -ffile-prefix-map="$PWD"= \
		-Werror=incompatible-pointer-types \
		-Iinclude "$src" -o "$objects/$(basename "${src%.c}.o")"
done
rm -f "$out/libbox3d.a"
"$ar" rcs "$out/libbox3d.a" "$objects"/*.o
rm -rf build/n64
