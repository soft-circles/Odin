#+build n64
#+private
package runtime

foreign import libc "system:c"
foreign import dragon "system:dragon"

_HAS_RAND_BYTES :: false

@(default_calling_convention="c")
foreign libc {
	@(link_name="write")
	_n64_write :: proc(fd: i32, data: rawptr, size: int) -> int ---
	abort :: proc() -> ! ---
}

@(default_calling_convention="c")
foreign dragon {
	debug_init_emulog :: proc() -> bool ---
}

// libdragon's stderr returns EBADF until a debug channel is opened.
_n64_emulog_ready: bool

_stderr_write :: proc "contextless" (data: []byte) -> (int, _OS_Errno) {
	if !_n64_emulog_ready {
		_n64_emulog_ready = true
		debug_init_emulog()
	}
	written := _n64_write(2, raw_data(data), len(data))
	if written < len(data) {
		return 0, -1
	}
	return written, 0
}

_exit :: proc "contextless" (code: int) -> ! {
	// libdragon's abort shows its crash screen and logs the failure.
	abort()
}
