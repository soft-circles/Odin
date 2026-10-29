#!/usr/bin/env sh
# Builds lib/n64/libbox3d.a for -target:n64 with the libdragon SDK at N64_INST.
# Flags follow libdragon's N64_CFLAGS without its warnings, -ftrivial-auto-var-init=pattern and the -ffast-math group,
# so Box3D keeps IEEE semantics, and with NDEBUG, which turns B3_ASSERT off. Scalar math (BOX3D_DISABLE_SIMD), and
# B3_MAX_WORLDS defaults to 1 because each world slot costs about 3 KiB of BSS; set BOX3D_MAX_WORLDS to reserve more,
# and pass the same value to Odin as -define:BOX3D_MAX_WORLDS so vendor:box3d's MAX_WORLDS matches.
# B3_FORCE_OVERFLOW solves every contact with the scalar solver, which drops the wide solver's lane-by-lane emulation
# (about 210 KB of code) at no cost in step time. B3_DISABLE_RECORDING compiles out recording (about 17 KB), so
# b3World_StartRecording does nothing.
# Memory: the worker arena starts at BOX3D_ARENA_CAPACITY bytes (16 KiB): it holds one contact's scratch, about
# 450 bytes per mesh triangle touched, and a step that needs more falls back to the heap and grows the arena to 1.5x
# that need. Manifold pools allocate one manifold array per block, lazily (BOX3D_BLOCK_EXPONENT=0,
# BOX3D_MANIFOLD_POOL_INITIAL_COUNT=0), instead of preallocating 512 arrays per manifold count, which cost up to 4 MB.
# World arrays start at BOX3D_MIN_CAPACITY (4) elements, worker debug buffers hold BOX3D_DEBUG_CAPACITY (1) points and
# lines, and only BOX3D_GRAPH_COLOR_COUNT (2) graph colors exist because B3_FORCE_OVERFLOW only uses the overflow color.
# BOX3D_STACK_CAPACITY and BOX3D_MAX_MESH_CONTACT_TRIANGLES, when set, size the step stack and cap the triangles one
# mesh contact gathers.
# Features: BOX3D_JOINTS and BOX3D_SHAPES name the joint and shape types to keep, all of them by default, for example
# BOX3D_JOINTS="mover pogo" BOX3D_SHAPES="capsule hull mesh". A left out joint type is never solved and its solver
# compiles out of the dispatch, as do the compound and height shapes' routines; capsule, hull, mesh and sphere code
# stays. Creating a left out shape type returns a null id. Keeping capsule keeps sphere, which a capsule shorter than
# B3_LINEAR_SLOP becomes.
# BOX3D_CONTINUOUS=0, BOX3D_SENSORS=0 and BOX3D_NAMES=0 compile out continuous collision, sensors and names.
# Code generation: -O2 with functions aligned to BOX3D_ALIGN_FUNCTIONS (8) bytes, and BOX3D_CFLAGS appended to every
# compile. BOX3D_LTO=1 builds fat LTO objects and indexes the archive with gcc-ar; a ROM link that passes -flto
# optimizes them with the game's code, and one that does not links their regular code.
set -e

: "${N64_INST:?Set N64_INST to the libdragon SDK}"
ODIN_ROOT=${ODIN_ROOT:-$(cd "$(dirname "$0")/../../.." && pwd)}
max_worlds=${BOX3D_MAX_WORLDS:-1}
arena_capacity=${BOX3D_ARENA_CAPACITY:-16384}
memory_flags="-DB3_BLOCK_EXPONENT=${BOX3D_BLOCK_EXPONENT:-0}"
memory_flags="$memory_flags -DB3_MANIFOLD_POOL_INITIAL_COUNT=${BOX3D_MANIFOLD_POOL_INITIAL_COUNT:-0}"
memory_flags="$memory_flags -DB3_MIN_CAPACITY=${BOX3D_MIN_CAPACITY:-4}"
memory_flags="$memory_flags -DB3_DEBUG_POINT_CAPACITY=${BOX3D_DEBUG_CAPACITY:-1}"
memory_flags="$memory_flags -DB3_DEBUG_LINE_CAPACITY=${BOX3D_DEBUG_CAPACITY:-1}"
memory_flags="$memory_flags -DB3_GRAPH_COLOR_COUNT=${BOX3D_GRAPH_COLOR_COUNT:-2}"
if [ -n "${BOX3D_STACK_CAPACITY:-}" ]; then
	memory_flags="$memory_flags -DB3_STACK_CAPACITY=$BOX3D_STACK_CAPACITY"
fi
if [ -n "${BOX3D_MAX_MESH_CONTACT_TRIANGLES:-}" ]; then
	memory_flags="$memory_flags -DB3_MAX_MESH_CONTACT_TRIANGLES=$BOX3D_MAX_MESH_CONTACT_TRIANGLES"
fi
joint_names="parallel distance filter motor prismatic revolute spherical weld wheel mover pogo"
shape_names="capsule compound height hull mesh sphere"

type_mask() {
	mask=0
	for name in $2; do
		bit=1
		found=0
		for known in $1; do
			if [ "$known" = "$name" ]; then
				mask=$((mask | bit))
				found=1
			fi
			bit=$((bit * 2))
		done
		if [ "$found" = 0 ]; then
			echo "build_n64.sh: unknown type $name" >&2
			exit 1
		fi
	done
	echo "$mask"
}
type_flags=""
if [ -n "${BOX3D_JOINTS:-}" ]; then
	type_flags="$type_flags -DB3_JOINT_TYPES=$(type_mask "$joint_names" "$BOX3D_JOINTS")u"
fi
if [ -n "${BOX3D_SHAPES:-}" ]; then
	shapes=$BOX3D_SHAPES
	case " $(echo $shapes) " in *" capsule "*) shapes="$shapes sphere" ;; esac
	type_flags="$type_flags -DB3_SHAPE_TYPES=$(type_mask "$shape_names" "$shapes")u"
fi
align_functions=${BOX3D_ALIGN_FUNCTIONS:-8}
extra_cflags=${BOX3D_CFLAGS:-}
cc="$N64_INST/bin/mips64-elf-gcc"
ar="$N64_INST/bin/mips64-elf-ar"
if [ "${BOX3D_LTO:-0}" = 1 ]; then
	extra_cflags="$extra_cflags -flto -ffat-lto-objects"
	ar="$N64_INST/bin/mips64-elf-gcc-ar"
fi

cd "$ODIN_ROOT/vendor/box3d/src" || exit 1

out=../lib/n64
objects=build/n64
rm -rf "$objects"
mkdir -p "$out" "$objects"
for src in src/*.c; do
	"$cc" -c -std=gnu17 -O2 -g -DN64 -DNDEBUG -DBOX3D_DISABLE_SIMD -DB3_MAX_WORLDS="$max_worlds" \
		-DB3_FORCE_OVERFLOW=1 -DB3_DISABLE_RECORDING -DB3_ARENA_CAPACITY="$arena_capacity" $memory_flags \
		$type_flags -DB3_ENABLE_CONTINUOUS="${BOX3D_CONTINUOUS:-1}" \
		-DB3_ENABLE_SENSORS="${BOX3D_SENSORS:-1}" -DB3_ENABLE_NAMES="${BOX3D_NAMES:-1}" \
		-march=vr4300 -mtune=vr4300 -mabi=o64 \
		-I"$N64_INST/mips64-elf/include/newlib_overrides" -I"$N64_INST/mips64-elf/include" -include ktls.h \
		-falign-functions="$align_functions" -ffunction-sections -fdata-sections -ffile-prefix-map="$PWD"= \
		-Werror=incompatible-pointer-types $extra_cflags \
		-Iinclude "$src" -o "$objects/$(basename "${src%.c}.o")"
done
rm -f "$out/libbox3d.a"
"$ar" rcs "$out/libbox3d.a" "$objects"/*.o
rm -rf build/n64
