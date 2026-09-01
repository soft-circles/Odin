#+build n64
#+private
package runtime

foreign import libc "system:c"

_HAS_RAND_BYTES :: false

@(default_calling_convention="c")
foreign libc {
	@(link_name="write")
	_n64_write :: proc(fd: i32, data: rawptr, size: int) -> int ---
}

_stderr_write :: proc "contextless" (data: []byte) -> (int, _OS_Errno) {
	written := _n64_write(2, raw_data(data), len(data))
	if written < 0 {
		return written, -1
	}
	if written < len(data) {
		return written, -1
	}
	return written, 0
}

_exit :: proc "contextless" (code: int) -> ! {
	trap()
}
