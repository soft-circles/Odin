package n64_asm_link_names_exporter

foreign import self "system:self"

// Definitions the importing package declares as foreign, and foreign declarations of what it defines.
@(export) shared_word: u32 = 7
@(export) shared_proc :: proc "c" () -> u32 { return shared_word + importer_word + importer_proc() }

@(default_calling_convention="c")
foreign self {
	importer_word: u32
	importer_proc :: proc() -> u32 ---
}
