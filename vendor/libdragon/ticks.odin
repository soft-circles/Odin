package libdragon

foreign import lib "system:dragon"

@(default_calling_convention="c")
foreign lib {
	// Read libdragon's monotonic millisecond counter. Use unsigned subtraction
	// for elapsed time. Callback timers are intentionally outside this binding.
	get_ticks_ms :: proc() -> u64 ---
}
