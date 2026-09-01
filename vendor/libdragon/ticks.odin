package libdragon

foreign import lib "system:dragon"

@(default_calling_convention="c")
foreign lib {
	get_ticks_ms :: proc() -> u64 ---
}
