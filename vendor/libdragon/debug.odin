package libdragon

import "core:c"

foreign import lib "system:dragon"

@(default_calling_convention="c")
foreign lib {
	debug_init_emulog :: proc() -> c.bool ---
	debugf            :: proc(msg: cstring, #c_vararg args: ..any) ---
}
