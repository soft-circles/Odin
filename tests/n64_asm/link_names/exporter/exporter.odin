package n64_asm_link_names_exporter

// The Odin definitions the importing package also declares as foreign.
@(export) shared_word: u32 = 7
@(export) shared_proc :: proc "c" () -> u32 { return shared_word }
