#+build n64
#+private
#+no-instrumentation
package runtime

import "base:intrinsics"

when !ODIN_TEST && !ODIN_NO_ENTRY_POINT {
	@(private="file")
	run_n64_application :: proc "contextless" () {
		context = default_context()
		context.allocator = heap_allocator()
		default_temp_allocator_init(
			&global_default_temp_allocator_data,
			N64_TEMP_ARENA_SIZE,
			context.allocator,
		)
		context.temp_allocator = default_temp_allocator(&global_default_temp_allocator_data)

		when !ODIN_BEDROCK {
			#force_no_inline _startup_runtime()
		}
		intrinsics.__entry_point()
		when !ODIN_BEDROCK {
			#force_no_inline _cleanup_runtime()
		}
	}

	@(link_name="main", linkage="strong", require)
	main :: proc "c" () -> i32 {
		run_n64_application()
		return 0
	}
}
