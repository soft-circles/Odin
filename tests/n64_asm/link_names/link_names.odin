package n64_asm_link_names

// Foreign declarations of symbols another package of the same build defines and exports, and the reverse: one
// module (as -use-single-module builds) must keep each definition under its name, with its body or initializer.
import "exporter"

foreign import self "system:self"

@(default_calling_convention="c")
foreign self {
	shared_word: u32
	shared_proc :: proc() -> u32 ---
}

@(export) importer_word: u32 = 9
@(export) importer_proc :: proc "c" () -> u32 { return 11 }

@(export) read_shared :: proc "c" () -> u32 {
	return shared_word + shared_proc() + exporter.shared_proc()
}
