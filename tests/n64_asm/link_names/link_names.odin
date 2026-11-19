package n64_asm_link_names

// A foreign declaration of a symbol another package of the same build defines and exports: one module (as
// -use-single-module builds) must keep the definition under its name rather than rename it `shared_word.1`.
import "exporter"

foreign import self "system:self"

@(default_calling_convention="c")
foreign self {
	shared_word: u32
	shared_proc :: proc() -> u32 ---
}

@(export) read_shared :: proc "c" () -> u32 {
	return shared_word + shared_proc() + exporter.shared_proc()
}
