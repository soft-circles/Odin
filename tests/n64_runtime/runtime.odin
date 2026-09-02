#+feature global-context

package n64_runtime

import "base:runtime"
import "core:c"

// Fixture-local logging only; this compiler test has no Odin64 dependency.
foreign import dragon "system:dragon"
@(default_calling_convention="c")
foreign dragon {
	debug_init_emulog :: proc() -> c.bool ---
	debugf :: proc(msg: cstring, #c_vararg args: ..any) ---
}

#assert(ODIN_OS == .N64)

main :: proc() {
	_ = debug_init_emulog()
	debugf("ODIN_N64_RUNTIME_CHECK:v2:MAIN_REACHED:PASS\n")
	if !verify_runtime_ordering() || !verify_general_allocator() ||
	   !verify_temp_allocator() || !verify_allocator_replaceability() {
		return
	}
	debugf(PASS_SENTINEL)
	runtime_phase = 3
	main_returned = true
	debugf(MAIN_RETURN_SENTINEL)
}

ORDERING_SENTINEL    :: "ODIN_N64_RUNTIME_CHECK:v2:ORDERING:PASS\n"
GENERAL_SENTINEL     :: "ODIN_N64_RUNTIME_CHECK:v2:GENERAL_ALLOCATOR:PASS\n"
TEMP_SENTINEL        :: "ODIN_N64_RUNTIME_CHECK:v2:TEMP_ALLOCATOR:PASS\n"
REPLACE_SENTINEL     :: "ODIN_N64_RUNTIME_CHECK:v2:ALLOCATOR_REPLACEABILITY:PASS\n"
MAIN_RETURN_SENTINEL :: "ODIN_N64_RUNTIME_MAIN_RETURN:v2\n"
CLEANUP_SENTINEL     :: "ODIN_N64_RUNTIME_CLEANUP:v2\n"
PASS_SENTINEL        :: "ODIN_N64_RUNTIME_PASS:v2\n"

OOM_SIZE         :: 64 * 1024 * 1024
GLOBAL_MAGIC     :: u32(0x4f44_494e)

@(private="file")
runtime_phase: u32

@(private="file")
init_general_memory: rawptr

@(private="file")
init_temp_memory: rawptr

@(private="file")
main_returned: bool

@(private="file")
mark_global_initialized :: proc "contextless" () -> u32 {
	runtime_phase = 1
	return GLOBAL_MAGIC
}

// A dynamic global initializer proves that runtime startup ran global
// initialization before the package's @(init) procedure and ordinary main.
@(private="file")
global_initialized: u32 = mark_global_initialized()

@(private="file")
bytes_are_zero :: proc "contextless" (p: rawptr, count: int) -> bool {
	if p == nil {
		return false
	}
	bytes := ([^]byte)(p)
	for i in 0..<count {
		if bytes[i] != 0 {
			return false
		}
	}
	return true
}

@(private="file")
fill_bytes :: proc "contextless" (p: rawptr, count: int, seed: byte) {
	bytes := ([^]byte)(p)
	for i in 0..<count {
		bytes[i] = seed + byte(i)
	}
}

@(private="file")
bytes_have_pattern :: proc "contextless" (p: rawptr, count: int, seed: byte) -> bool {
	if p == nil {
		return false
	}
	bytes := ([^]byte)(p)
	for i in 0..<count {
		if bytes[i] != seed + byte(i) {
			return false
		}
	}
	return true
}

@(private="file", require_results)
alloc :: proc(
	size, alignment: int,
	allocator := context.allocator,
) -> (rawptr, runtime.Allocator_Error) {
	data, err := runtime.mem_alloc(size, alignment, allocator)
	return raw_data(data), err
}

@(private="file", require_results)
resize :: proc(
	p: rawptr,
	old_size, new_size, alignment: int,
	allocator := context.allocator,
) -> (rawptr, runtime.Allocator_Error) {
	data, err := runtime.mem_resize(p, old_size, new_size, alignment, allocator)
	return raw_data(data), err
}

@(init)
initialize_runtime_probe :: proc() {
	if runtime_phase != 1 || global_initialized != GLOBAL_MAGIC {
		runtime_phase = 0xffff_ff01
		return
	}
	if context.allocator.procedure == nil || context.temp_allocator.procedure == nil {
		runtime_phase = 0xffff_ff02
		return
	}

	general, general_err := alloc(32, 16)
	if general_err != .None || !bytes_are_zero(general, 32) {
		runtime_phase = 0xffff_ff03
		return
	}
	fill_bytes(general, 32, 0x20)
	init_general_memory = general

	temporary, temp_err := alloc(32, 16, context.temp_allocator)
	if temp_err != .None || !bytes_are_zero(temporary, 32) {
		runtime_phase = 0xffff_ff04
		return
	}
	fill_bytes(temporary, 32, 0x40)
	init_temp_memory = temporary
	runtime_phase = 2
}

@(fini)
finalize_runtime_probe :: proc "contextless" () {
	if main_returned && runtime_phase == 3 {
		debugf(CLEANUP_SENTINEL)
	} else {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:CLEANUP_ORDER\n")
	}
}

@(private="file")
verify_runtime_ordering :: proc() -> bool {
	if runtime_phase != 2 || global_initialized != GLOBAL_MAGIC {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:STARTUP_ORDER\n")
		return false
	}
	if !bytes_have_pattern(init_general_memory, 32, 0x20) ||
	   !bytes_have_pattern(init_temp_memory, 32, 0x40) {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:INIT_ALLOCATION\n")
		return false
	}
	if runtime.mem_free(init_general_memory) != .None {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:INIT_GENERAL_FREE\n")
		return false
	}
	init_general_memory = nil
	if runtime.mem_free_all(context.temp_allocator) != .None {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:INIT_TEMP_RESET\n")
		return false
	}
	init_temp_memory = nil
	debugf(ORDERING_SENTINEL)
	return true
}

@(private="file")
verify_general_allocator :: proc() -> bool {
	p, err := alloc(32, 64)
	if err != .None || p == nil || uintptr(p) & 63 != 0 || !bytes_are_zero(p, 32) {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:GENERAL_ALLOC_ALIGN_ZERO\n")
		return false
	}
	fill_bytes(p, 32, 0x60)

	grown, grow_err := resize(p, 32, 96, 64)
	if grow_err != .None || grown == nil || uintptr(grown) & 63 != 0 ||
	   !bytes_have_pattern(grown, 32, 0x60) || !bytes_are_zero(rawptr(uintptr(grown)+32), 64) {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:GENERAL_RESIZE_GROW\n")
		if grown != nil {
			_ = runtime.mem_free(grown)
		} else {
			_ = runtime.mem_free(p)
		}
		return false
	}

	shrunk, shrink_err := resize(grown, 96, 16, 64)
	if shrink_err != .None || shrunk == nil || uintptr(shrunk) & 63 != 0 ||
	   !bytes_have_pattern(shrunk, 16, 0x60) {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:GENERAL_RESIZE_SHRINK\n")
		if shrunk != nil {
			_ = runtime.mem_free(shrunk)
		} else {
			_ = runtime.mem_free(grown)
		}
		return false
	}

	failed_resize, failed_resize_err := resize(shrunk, 16, OOM_SIZE, 64)
	if failed_resize != nil || failed_resize_err != .Out_Of_Memory ||
	   !bytes_have_pattern(shrunk, 16, 0x60) {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:GENERAL_FAILED_RESIZE\n")
		if failed_resize != nil {
			_ = runtime.mem_free(failed_resize)
		} else {
			_ = runtime.mem_free(shrunk)
		}
		return false
	}

	oom, oom_err := alloc(OOM_SIZE, 16)
	if oom != nil || oom_err != .Out_Of_Memory {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:GENERAL_OOM\n")
		if oom != nil {
			_ = runtime.mem_free(oom)
		}
		_ = runtime.mem_free(shrunk)
		return false
	}

	if runtime.mem_free(shrunk) != .None {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:GENERAL_FREE\n")
		return false
	}
	debugf(GENERAL_SENTINEL)
	return true
}

@(private="file")
verify_temp_allocator :: proc() -> bool {
	temp := context.temp_allocator
	first, first_err := alloc(64, 32, temp)
	if first_err != .None || first == nil || uintptr(first) & 31 != 0 || !bytes_are_zero(first, 64) {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:TEMP_ALLOC_ALIGN_ZERO\n")
		return false
	}
	fill_bytes(first, 64, 0x80)

	grown, grow_err := resize(first, 64, 128, 32, temp)
	if grow_err != .None || grown != first || uintptr(grown) & 31 != 0 ||
	   !bytes_have_pattern(grown, 64, 0x80) || !bytes_are_zero(rawptr(uintptr(grown)+64), 64) {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:TEMP_RESIZE_GROW\n")
		return false
	}

	shrunk, shrink_err := resize(grown, 128, 32, 32, temp)
	if shrink_err != .None || shrunk != grown || uintptr(shrunk) & 31 != 0 ||
	   !bytes_have_pattern(shrunk, 32, 0x80) {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:TEMP_RESIZE_SHRINK\n")
		return false
	}

	failed_resize, failed_resize_err := resize(shrunk, 32, OOM_SIZE, 32, temp)
	if failed_resize != nil || failed_resize_err != .Out_Of_Memory ||
	   !bytes_have_pattern(shrunk, 32, 0x80) {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:TEMP_FAILED_RESIZE\n")
		return false
	}

	if runtime.mem_free_all(temp) != .None {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:TEMP_RESET\n")
		return false
	}
	second, second_err := alloc(64, 32, temp)
	if second_err != .None || second != first || !bytes_are_zero(second, 64) {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:TEMP_RESET_REUSE_ZERO\n")
		return false
	}

	temp_oom, temp_oom_err := alloc(OOM_SIZE, 16, temp)
	if temp_oom != nil || temp_oom_err != .Out_Of_Memory {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:TEMP_OOM\n")
		return false
	}
	if runtime.mem_free_all(temp) != .None {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:TEMP_FINAL_RESET\n")
		return false
	}
	debugf(TEMP_SENTINEL)
	return true
}

@(private="file")
Recording_Allocator :: struct {
	backing: runtime.Allocator,
	calls:   int,
}

@(private="file")
recording_allocator_proc :: proc(
	allocator_data: rawptr,
	mode: runtime.Allocator_Mode,
	size, alignment: int,
	old_memory: rawptr,
	old_size: int,
	loc := #caller_location,
) -> ([]byte, runtime.Allocator_Error) {
	state := (^Recording_Allocator)(allocator_data)
	state.calls += 1
	return state.backing.procedure(
		state.backing.data,
		mode,
		size,
		alignment,
		old_memory,
		old_size,
		loc,
	)
}

@(private="file")
verify_allocator_replaceability :: proc() -> bool {
	general_state := Recording_Allocator{backing = context.allocator}
	context.allocator = runtime.Allocator{
		procedure = recording_allocator_proc,
		data = &general_state,
	}
	general, general_err := alloc(24, 16)
	general_ok := general_err == .None && general != nil && general_state.calls == 1
	if general != nil {
		general_ok = runtime.mem_free(general) == .None && general_ok
	}
	context.allocator = general_state.backing
	if !general_ok || general_state.calls != 2 {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:GENERAL_REPLACE\n")
		return false
	}

	temp_state := Recording_Allocator{backing = context.temp_allocator}
	context.temp_allocator = runtime.Allocator{
		procedure = recording_allocator_proc,
		data = &temp_state,
	}
	temporary, temp_err := alloc(24, 16, context.temp_allocator)
	temp_ok := temp_err == .None && temporary != nil && temp_state.calls == 1
	if runtime.mem_free_all(context.temp_allocator) != .None {
		temp_ok = false
	}
	context.temp_allocator = temp_state.backing
	if !temp_ok || temp_state.calls != 2 {
		debugf("ODIN_N64_RUNTIME_FAIL:v2:TEMP_REPLACE\n")
		return false
	}

	debugf(REPLACE_SENTINEL)
	return true
}
