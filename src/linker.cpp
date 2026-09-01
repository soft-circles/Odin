struct LinkerData {
	BlockingMutex foreign_mutex;
	PtrSet<Entity *> foreign_libraries_set;
	Array<Entity *>  foreign_libraries;

	Array<String> output_object_paths;
	Array<String> output_temp_paths;
	String   output_base;
	String   output_name;
	bool     needs_system_library_linked;
};

#include <llvm/ADT/ArrayRef.h>
#include <llvm/Support/SHA256.h>

gb_internal i32 system_exec_command_line_app(char const *name, char const *fmt, ...);
gb_internal bool system_exec_command_line_app_output(char const *command, gbString *output);
#if !defined(GB_SYSTEM_WINDOWS)
#include <spawn.h>
extern char **environ;
int run_subprocess(char const *name, char const **args);
#endif

gb_global char const *N64_PINNED_LIBDRAGON_COMMIT = "c79a52b42ac790e06e797aede43914dd8754cd5f";
gb_global char const *N64_PINNED_MAKEFILE_SHA256 = "9bbe6e8efbb515cddbd7ce8f730cd003c12f655868cf63c4d6ed983592808ee6";

gb_internal String n64_path_join(gbAllocator allocator, String const &directory, String const &name) {
	if (directory.len > 0 && (directory[directory.len-1] == '/' || directory[directory.len-1] == '\\')) {
		return concatenate_strings(allocator, directory, name);
	}
	return concatenate3_strings(allocator, directory, STR_LIT("/"), name);
}

gb_internal String n64_load_text_file(String const &path) {
	char const *path_c = alloc_cstring(permanent_allocator(), path);
	gbFileContents contents = gb_file_read_contents(permanent_allocator(), true, path_c);
	if (contents.data == nullptr) {
		return {};
	}
	return make_string(cast(u8 *)contents.data, contents.size);
}

gb_internal isize n64_json_field_value_start(String const &json, String const &field) {
	for (isize index = 0; index < json.len; index += 1) {
		if (json[index] != '"') {
			continue;
		}
		isize key_start = index+1;
		isize key_end = key_start;
		while (key_end < json.len && json[key_end] != '"') {
			if (json[key_end] == '\\') {
				key_end += 1;
			}
			key_end += 1;
		}
		if (key_end >= json.len) {
			return -1;
		}
		index = key_end;
		if (substring(json, key_start, key_end) != field) {
			continue;
		}
		isize value_start = key_end+1;
		while (value_start < json.len && gb_char_is_space(json[value_start])) {
			value_start += 1;
		}
		if (value_start >= json.len || json[value_start] != ':') {
			continue;
		}
		value_start += 1;
		while (value_start < json.len && gb_char_is_space(json[value_start])) {
			value_start += 1;
		}
		return value_start;
	}
	return -1;
}

gb_internal String n64_json_string_field(String const &json, String const &field) {
	isize index = n64_json_field_value_start(json, field);
	if (index < 0 || index >= json.len || json[index] != '"') {
		return {};
	}
	index += 1;
	isize start = index;
	while (index < json.len && json[index] != '"' && json[index] != '\\') {
		index += 1;
	}
	if (index >= json.len) {
		return {};
	}
	return substring(json, start, index);
}

gb_internal bool n64_json_false_field(String const &json, String const &field) {
	isize index = n64_json_field_value_start(json, field);
	if (index < 0 || index+5 > json.len || substring(json, index, index+5) != "false") {
		return false;
	}
	index += 5;
	while (index < json.len && gb_char_is_space(json[index])) {
		index += 1;
	}
	return index >= json.len || json[index] == ',' || json[index] == '}';
}

gb_internal String n64_sha256(String const &contents) {
	auto digest = llvm::SHA256::hash(llvm::ArrayRef<u8>(contents.text, contents.len));
	char *hex = gb_alloc_array(permanent_allocator(), char, 65);
	char const digits[] = "0123456789abcdef";
	for (isize index = 0; index < 32; index += 1) {
		u8 byte = digest[index];
		hex[2*index+0] = digits[byte >> 4];
		hex[2*index+1] = digits[byte & 0xf];
	}
	hex[64] = 0;
	return make_string(cast(u8 *)hex, 64);
}

gb_internal bool n64_sdk_tool_is_executable(String const &path) {
	char const *path_c = alloc_cstring(temporary_allocator(), path);
	if (!gb_file_exists(path_c) || path_is_directory(path)) {
		return false;
	}
#if defined(GB_SYSTEM_WINDOWS)
	return true;
#else
	return access(path_c, X_OK) == 0;
#endif
}

gb_internal bool n64_validate_sdk_root(String const &sdk_root) {
	String required_files[] = {
		STR_LIT("include/n64.mk"),
		STR_LIT("mips64-elf/include/libdragon.version"),
		STR_LIT("mips64-elf/include/toolchain.version"),
		STR_LIT("mips64-elf/lib/libdragon.a"),
		STR_LIT("mips64-elf/lib/libdragonsys.a"),
		STR_LIT("mips64-elf/lib/n64.ld"),
	};
	String required_tools[] = {
		STR_LIT("bin/ed64romconfig"),
		STR_LIT("bin/mips64-elf-g++"),
		STR_LIT("bin/mips64-elf-gcc"),
		STR_LIT("bin/mips64-elf-objdump"),
		STR_LIT("bin/mips64-elf-size"),
		STR_LIT("bin/mips64-elf-strip"),
		STR_LIT("bin/n64elfcompress"),
		STR_LIT("bin/n64sym"),
		STR_LIT("bin/n64tool"),
	};

	bool valid = true;
	for (String const &relative_path : required_files) {
		String path = n64_path_join(temporary_allocator(), sdk_root, relative_path);
		if (!gb_file_exists(alloc_cstring(temporary_allocator(), path)) || path_is_directory(path)) {
			gb_printf_err("N64 SDK %.*s is missing required file: %.*s\n", LIT(sdk_root), LIT(relative_path));
			valid = false;
		}
	}
	for (String const &relative_path : required_tools) {
		String path = n64_path_join(temporary_allocator(), sdk_root, relative_path);
		if (!n64_sdk_tool_is_executable(path)) {
			gb_printf_err("N64 SDK %.*s is missing required executable tool: %.*s\n", LIT(sdk_root), LIT(relative_path));
			valid = false;
		}
	}
	if (!valid) {
		return false;
	}

	String libdragon_version_path = n64_path_join(temporary_allocator(), sdk_root, STR_LIT("mips64-elf/include/libdragon.version"));
	String toolchain_version_path = n64_path_join(temporary_allocator(), sdk_root, STR_LIT("mips64-elf/include/toolchain.version"));
	String makefile_path = n64_path_join(temporary_allocator(), sdk_root, STR_LIT("include/n64.mk"));
	String libdragon_version = n64_load_text_file(libdragon_version_path);
	String toolchain_version = n64_load_text_file(toolchain_version_path);
	String makefile_sha256 = n64_sha256(n64_load_text_file(makefile_path));
	if (makefile_sha256 != make_string_c(N64_PINNED_MAKEFILE_SHA256)) {
		gb_printf_err("libdragon SDK mismatch: expected pinned n64.mk SHA-256 %s, found %.*s in %.*s\n",
		              N64_PINNED_MAKEFILE_SHA256, LIT(makefile_sha256), LIT(makefile_path));
		return false;
	}
	String actual_commit = n64_json_string_field(libdragon_version, STR_LIT("hash"));
	String expected_commit = make_string_c(N64_PINNED_LIBDRAGON_COMMIT);
	if (actual_commit != expected_commit) {
		String shown_commit = actual_commit.len > 0 ? actual_commit : STR_LIT("<missing hash>");
		gb_printf_err("libdragon SDK mismatch: expected %s, found %.*s in %.*s\n",
		              N64_PINNED_LIBDRAGON_COMMIT, LIT(shown_commit), LIT(libdragon_version_path));
		return false;
	}
	if (!n64_json_false_field(libdragon_version, STR_LIT("dirty"))) {
		gb_printf_err("libdragon SDK mismatch: pinned SDK provenance must be clean in %.*s\n", LIT(libdragon_version_path));
		return false;
	}

	gb_printf_err("Validated N64 SDK: %.*s\n", LIT(sdk_root));
	gb_printf_err("  libdragon provenance: %.*s\n", LIT(string_trim_whitespace(libdragon_version)));
	gb_printf_err("  toolchain provenance: %.*s\n", LIT(string_trim_whitespace(toolchain_version)));
	struct ToolchainField {
		String name;
		String expected;
	};
	ToolchainField expected_toolchain[] = {
		{STR_LIT("host"),     STR_LIT("aarch64-apple-darwin25.5.0")},
		{STR_LIT("binutils"), STR_LIT("2.47")},
		{STR_LIT("gcc"),     STR_LIT("16.2.0")},
		{STR_LIT("newlib"),  STR_LIT("4.4.0.20231231")},
	};
	for (ToolchainField const &field : expected_toolchain) {
		String actual = n64_json_string_field(toolchain_version, field.name);
		if (actual != field.expected) {
			String shown_actual = actual.len > 0 ? actual : STR_LIT("<missing>");
			gb_printf_err(
				"Warning: N64 SDK %.*s provenance differs from the validated baseline: expected %.*s, found %.*s.\n",
				LIT(field.name), LIT(field.expected), LIT(shown_actual)
			);
		}
	}
	return true;
}

gb_internal bool n64_prepare_build(void) {
	if (build_context.metrics.os != TargetOs_n64) {
		if (build_context.n64_inst_given || build_context.n64_rom_options_given) {
			gb_printf_err("N64 build options may only be used with -target:n64\n");
			return false;
		}
		return true;
	}
	if ((build_context.command_kind & Command__does_build) == 0) {
		return true;
	}
	if (build_context.command_kind != Command_build) {
		gb_printf_err("-target:n64 build outputs currently support the build command only\n");
		return false;
	}
	if (build_context.build_mode != BuildMode_Executable && build_context.n64_rom_options_given) {
		gb_printf_err("N64 ROM configuration options require executable ROM output\n");
		return false;
	}
	switch (build_context.build_mode) {
	case BuildMode_Object:
	case BuildMode_Assembly:
	case BuildMode_LLVM_IR:
		return true;
	case BuildMode_StaticLibrary:
	case BuildMode_DynamicLibrary:
		gb_printf_err("-target:n64 currently supports executable ROM, object, assembly, and LLVM IR output\n");
		return false;
	case BuildMode_Executable:
		break;
	}
#if defined(GB_SYSTEM_WINDOWS)
	gb_printf_err("The integrated N64 ROM build currently requires a POSIX host\n");
	return false;
#endif
	if (build_context.lto_kind != LTO_None) {
		gb_printf_err("-target:n64 does not support LTO in the pinned libdragon build pipeline\n");
		return false;
	}
	if (build_context.reloc_mode != RelocMode_Static) {
		gb_printf_err("-target:n64 executable builds require -reloc-mode:static\n");
		return false;
	}
	if (build_context.no_crt || build_context.no_entry_point) {
		gb_printf_err("-target:n64 executable builds do not support -no-crt or -no-entry-point; pinned n64.mk owns startup\n");
		return false;
	}
	if (build_context.linker_choice != Linker_Default) {
		gb_printf_err("-target:n64 executable builds do not support -linker; pinned n64.mk selects the linker\n");
		return false;
	}
	if (build_context.print_linker_flags) {
		gb_printf_err("-print-linker-flags is not supported by the integrated N64 packaging pipeline; use -show-system-calls\n");
		return false;
	}
	if (build_context.n64_rtc &&
	    (build_context.n64_save_type == STR_LIT("eeprom4k") ||
	     build_context.n64_save_type == STR_LIT("eeprom16k"))) {
		gb_printf_err("-n64-rtc cannot be combined with -n64-save-type:%.*s; the pinned N64 header format cannot use RTC with EEPROM\n",
		              LIT(build_context.n64_save_type));
		return false;
	}
#if !defined(GB_SYSTEM_WINDOWS)
	if (!n64_sdk_tool_is_executable(STR_LIT("/usr/bin/make"))) {
		gb_printf_err("GNU make is required at /usr/bin/make for the integrated N64 build\n");
		return false;
	}
#endif

	if (build_context.n64_inst.len == 0) {
		char const *environment_sdk = gb_get_env("N64_INST", permanent_allocator());
		if (environment_sdk != nullptr) {
			String path = string_trim_whitespace(make_string_c(environment_sdk));
			if (path.len > 0) {
				build_context.n64_inst = path_to_full_path(permanent_allocator(), path);
			}
		}
	}
	if (build_context.n64_inst.len == 0) {
		gb_printf_err("N64 SDK is not configured; use -n64-inst:<path> or set the N64_INST environment variable\n");
		return false;
	}
	if (!n64_validate_sdk_root(build_context.n64_inst)) {
		return false;
	}
	if (build_context.n64_assets.len > 0) {
		String tool = n64_path_join(temporary_allocator(), build_context.n64_inst, STR_LIT("bin/mkdfs"));
		if (!n64_sdk_tool_is_executable(tool)) {
			gb_printf_err("-n64-assets requires the executable N64 SDK tool bin/mkdfs in %.*s\n", LIT(build_context.n64_inst));
			return false;
		}
	}
	if (build_context.n64_metadata.len > 0) {
		String tool = n64_path_join(temporary_allocator(), build_context.n64_inst, STR_LIT("bin/n64metadata"));
		if (!n64_sdk_tool_is_executable(tool)) {
			gb_printf_err("-n64-metadata requires the executable N64 SDK tool bin/n64metadata in %.*s\n", LIT(build_context.n64_inst));
			return false;
		}
	}
	return true;
}

struct N64BuildStage {
	String work_dir;
	String build_dir;
	String makefile_path;
	String sdk_link_path;
	String staged_rom_path;
	String assets_link_path;
	String metadata_dir_path;
	String metadata_path;
	String metadata_make_path;
	String dfs_path;
	Array<String> input_names;
	Array<String> input_paths;
	Array<String> metadata_entry_paths;
};

gb_internal bool n64_remove_directory(String const &path) {
#if defined(GB_SYSTEM_WINDOWS)
	String16 wide_path = string_to_string16(temporary_allocator(), path);
	return RemoveDirectoryW(cast(wchar_t *)wide_path.text) != 0;
#else
	return rmdir(alloc_cstring(temporary_allocator(), path)) == 0;
#endif
}

gb_internal bool n64_create_staging_link(String const &source_path, String const &link_path) {
#if defined(GB_SYSTEM_WINDOWS)
	(void)source_path;
	(void)link_path;
	return false;
#else
	return symlink(
		alloc_cstring(temporary_allocator(), source_path),
		alloc_cstring(temporary_allocator(), link_path)
	) == 0;
#endif
}

enum N64MetadataSectionKind {
	N64MetadataSection_None,
	N64MetadataSection_Meta,
	N64MetadataSection_Art,
};

gb_internal bool n64_metadata_section_matches(String const &section, String const &name) {
	return section == name ||
	       (section.len > name.len && section[name.len] == '.' && string_starts_with(section, name));
}

gb_internal bool n64_metadata_reference_root(String reference, String *root) {
	reference = string_trim_whitespace(reference);
	if (reference.len == 0 || is_separator(reference[0])) {
		return false;
	}

	isize component_start = 0;
	isize root_end = reference.len;
	for (isize index = 0; index <= reference.len; index += 1) {
		if (index != reference.len && !is_separator(reference[index])) {
			continue;
		}
		String component = substring(reference, component_start, index);
		if (component.len == 0 || component == "." || component == "..") {
			return false;
		}
		if (component_start == 0) {
			root_end = index;
		}
		component_start = index+1;
	}

	*root = substring(reference, 0, root_end);
	return true;
}

gb_internal void n64_add_metadata_reference(Array<String> *roots, String reference) {
	String root = {};
	if (!n64_metadata_reference_root(reference, &root)) {
		return;
	}
	for (String const &existing : *roots) {
		if (existing == root) {
			return;
		}
	}
	array_add(roots, copy_string(permanent_allocator(), root));
}

gb_internal Array<String> n64_metadata_companion_roots(String const &source_ini) {
	Array<String> roots = {};
	array_init(&roots, permanent_allocator());

	String contents = n64_load_text_file(source_ini);
	N64MetadataSectionKind section = N64MetadataSection_None;
	for (isize line_start = 0; line_start < contents.len; ) {
		isize line_end = line_start;
		while (line_end < contents.len && contents[line_end] != '\n') {
			line_end += 1;
		}
		String line = string_trim_whitespace(substring(contents, line_start, line_end));
		line_start = line_end+1;
		if (line.len == 0 || line[0] == '#' || line[0] == ';') {
			continue;
		}
		if (line[0] == '[' && line[line.len-1] == ']') {
			String name = substring(line, 1, line.len-1);
			if (n64_metadata_section_matches(name, STR_LIT("meta"))) {
				section = N64MetadataSection_Meta;
			} else if (n64_metadata_section_matches(name, STR_LIT("boxart")) ||
			           n64_metadata_section_matches(name, STR_LIT("cartart"))) {
				section = N64MetadataSection_Art;
			} else {
				section = N64MetadataSection_None;
			}
			continue;
		}

		isize equals = string_index_byte(line, '=');
		if (equals < 0) {
			continue;
		}
		String key = string_trim_whitespace(substring(line, 0, equals));
		String value = string_trim_whitespace(substring(line, equals+1, line.len));
		if (section == N64MetadataSection_Meta && key == "screenshots") {
			String_Iterator iterator = {value, 0};
			String screenshot = {};
			while (string_split_iterator_next(&iterator, ',', &screenshot)) {
				n64_add_metadata_reference(&roots, screenshot);
			}
		} else if (section == N64MetadataSection_Meta && key == "long-desc") {
			n64_add_metadata_reference(&roots, value);
		} else if (section == N64MetadataSection_Art &&
		           (key == "front" || key == "back" || key == "top" ||
		            key == "bottom" || key == "left" || key == "right")) {
			n64_add_metadata_reference(&roots, value);
		}
	}
	return roots;
}

gb_internal bool n64_stage_metadata_directory(N64BuildStage *stage, String const &source_ini) {
#if defined(GB_SYSTEM_WINDOWS)
	(void)stage;
	(void)source_ini;
	return false;
#else
	if (mkdir(alloc_cstring(temporary_allocator(), stage->metadata_dir_path), 0700) != 0) {
		gb_printf_err("Failed to create N64 metadata staging directory %.*s: %s\n",
		              LIT(stage->metadata_dir_path), strerror(errno));
		return false;
	}

	String source_directory = directory_from_path(source_ini);
	Array<String> companion_roots = n64_metadata_companion_roots(source_ini);
	for (String const &name : companion_roots) {
		String source = n64_path_join(permanent_allocator(), source_directory, name);
		String destination = n64_path_join(permanent_allocator(), stage->metadata_dir_path, name);
		if (!n64_create_staging_link(source, destination)) {
			gb_printf_err("Failed to stage N64 metadata companion %.*s: %s\n", LIT(source), strerror(errno));
			return false;
		}
		array_add(&stage->metadata_entry_paths, destination);
	}

	for (isize index = 0; ; index += 1) {
		gbString alias = gb_string_make(temporary_allocator(), "");
		alias = gb_string_append_fmt(alias, "odin-input-%04td.ini", index);
		String alias_name = make_string_c(alias);
		String candidate = n64_path_join(permanent_allocator(), stage->metadata_dir_path, alias_name);
		if (gb_file_exists(alloc_cstring(temporary_allocator(), candidate))) {
			continue;
		}
		stage->metadata_path = candidate;
		stage->metadata_make_path = n64_path_join(permanent_allocator(), STR_LIT("metadata"), alias_name);
		break;
	}
	if (!gb_file_copy(
		alloc_cstring(temporary_allocator(), source_ini),
		alloc_cstring(temporary_allocator(), stage->metadata_path),
		true
	)) {
		gb_printf_err("Failed to stage N64 metadata INI %.*s\n", LIT(source_ini));
		return false;
	}
	return true;
#endif
}

gb_internal bool n64_init_build_stage(N64BuildStage *stage, String const &output_filename) {
	GB_ASSERT(stage != nullptr);
	gbAllocator allocator = permanent_allocator();
	array_init(&stage->input_names, allocator);
	array_init(&stage->input_paths, allocator);
	array_init(&stage->metadata_entry_paths, allocator);

	String output_directory = directory_from_path(output_filename);
	String stage_template = n64_path_join(temporary_allocator(), output_directory, STR_LIT(".odin-n64-build-XXXXXX"));
#if defined(GB_SYSTEM_WINDOWS)
	stage->work_dir = copy_string(allocator, stage_template);
	gb_printf_err("The integrated N64 ROM build currently requires a POSIX host\n");
	return false;
#else
	char *stage_template_c = alloc_cstring(temporary_allocator(), stage_template);
	char *created_work_dir = mkdtemp(stage_template_c);
	if (created_work_dir == nullptr) {
		gb_printf_err("Failed to create isolated N64 build directory beside %.*s: %s\n",
		              LIT(output_filename), strerror(errno));
		return false;
	}
	stage->work_dir = copy_string(allocator, make_string_c(created_work_dir));
#endif
	stage->build_dir = n64_path_join(allocator, stage->work_dir, STR_LIT("build"));
	stage->makefile_path = n64_path_join(allocator, stage->work_dir, STR_LIT("Makefile"));
	stage->sdk_link_path = n64_path_join(allocator, stage->work_dir, STR_LIT("sdk"));
	stage->staged_rom_path = n64_path_join(allocator, stage->work_dir, STR_LIT("odin-n64.z64"));
	stage->assets_link_path = n64_path_join(allocator, stage->work_dir, STR_LIT("assets"));
	stage->metadata_dir_path = n64_path_join(allocator, stage->work_dir, STR_LIT("metadata"));
	stage->dfs_path = n64_path_join(allocator, stage->build_dir, STR_LIT("odin-n64.dfs"));

	if (!n64_create_staging_link(build_context.n64_inst, stage->sdk_link_path)) {
		gb_printf_err("Failed to create N64 SDK staging link %.*s -> %.*s\n",
		              LIT(stage->sdk_link_path), LIT(build_context.n64_inst));
		if (!n64_remove_directory(stage->work_dir)) {
			gb_printf_err("N64 build failed; intermediates were retained at %.*s\n", LIT(stage->work_dir));
		}
		return false;
	}
	if (build_context.n64_assets.len > 0 &&
	    !n64_create_staging_link(build_context.n64_assets, stage->assets_link_path)) {
		gb_printf_err("Failed to stage N64 asset directory %.*s: %s\n", LIT(build_context.n64_assets), strerror(errno));
		gb_printf_err("N64 build failed; intermediates were retained at %.*s\n", LIT(stage->work_dir));
		return false;
	}
	if (build_context.n64_metadata.len > 0 &&
	    !n64_stage_metadata_directory(stage, build_context.n64_metadata)) {
		gb_printf_err("N64 build failed; intermediates were retained at %.*s\n", LIT(stage->work_dir));
		return false;
	}
	return true;
}

gb_internal bool n64_stage_input(
	N64BuildStage *stage,
	String const &source_path,
	String const &prefix,
	isize index,
	String const &extension
) {
	gbString name = gb_string_make(temporary_allocator(), "");
	name = gb_string_append_fmt(name, "%.*s-%04td.%.*s", LIT(prefix), index, LIT(extension));
	String input_name = copy_string(permanent_allocator(), make_string_c(name));
	String destination = n64_path_join(permanent_allocator(), stage->work_dir, input_name);
	if (!gb_file_copy(
		alloc_cstring(temporary_allocator(), source_path),
		alloc_cstring(temporary_allocator(), destination),
		false
	)) {
		gb_printf_err("Failed to stage N64 link input %.*s as %.*s\n", LIT(source_path), LIT(destination));
		return false;
	}
	array_add(&stage->input_names, input_name);
	array_add(&stage->input_paths, destination);
	return true;
}

gb_internal bool n64_stage_link_inputs(N64BuildStage *stage, LinkerData *linker) {
	// LLVM modules are collected from a hash map, so their completion/list order
	// is not stable between compiler processes. Link in path order to keep an
	// unchanged N64 application's ELF, symbols, compressed payload, and ROM
	// byte-for-byte reproducible.
	array_sort(linker->output_object_paths, string_cmp);

	isize odin_index = 0;
	for (String const &object_path : linker->output_object_paths) {
		if (!n64_stage_input(stage, object_path, STR_LIT("odin"), odin_index, STR_LIT("o"))) {
			return false;
		}
		odin_index += 1;
	}

	isize foreign_index = 0;
	for (Entity *library : linker->foreign_libraries) {
		GB_ASSERT(library->kind == Entity_LibraryName);
		String extra_flags = string_trim_whitespace(library->LibraryName.extra_linker_flags);
		if (extra_flags.len > 0) {
			gb_printf_err("N64 foreign import '%.*s' uses unsupported extra linker flags: %.*s\n",
			              LIT(library->token.string), LIT(extra_flags));
			return false;
		}
		for (String const &path : library->LibraryName.paths) {
			String input = string_trim_whitespace(path);
			if (input == "c" || input == "m" || input == "dragon" || input == "dragonsys") {
				continue;
			}
			String extension = path_extension(input, false);
			if (!str_eq_ignore_case(extension, STR_LIT("o")) &&
			    !str_eq_ignore_case(extension, STR_LIT("a"))) {
				gb_printf_err("N64 foreign import input must be a static .o or .a file, got: %.*s\n", LIT(input));
				return false;
			}
			if (!gb_file_exists(alloc_cstring(temporary_allocator(), input))) {
				gb_printf_err("N64 foreign import input does not exist: %.*s\n", LIT(input));
				return false;
			}
			String staged_extension = str_eq_ignore_case(extension, STR_LIT("o")) ? STR_LIT("o") : STR_LIT("a");
			if (!n64_stage_input(stage, input, STR_LIT("foreign"), foreign_index, staged_extension)) {
				return false;
			}
			foreign_index += 1;
		}
	}
	return true;
}

gb_internal String n64_sanitized_rom_title(String const &source) {
	gbString title = gb_string_make_reserve(permanent_allocator(), 21);
	for (isize index = 0; index < source.len && gb_string_length(title) < 20; index += 1) {
		char c = cast(char)source[index];
		if (gb_char_is_alphanumeric(c) || c == ' ' || c == '-' || c == '_') {
			title = gb_string_append_length(title, &c, 1);
		} else {
			title = gb_string_appendc(title, "_");
		}
	}
	if (gb_string_length(title) == 0) {
		title = gb_string_appendc(title, "Odin N64");
	}
	return make_string_c(title);
}

gb_internal bool n64_write_makefile(N64BuildStage const &stage) {
	gbFile file = {};
	gbFileError error = gb_file_open_mode(
		&file,
		gbFileMode_Write,
		alloc_cstring(temporary_allocator(), stage.makefile_path)
	);
	if (error != gbFileError_None) {
		gb_printf_err("Failed to create generated N64 Makefile: %.*s\n", LIT(stage.makefile_path));
		return false;
	}
	defer (gb_file_close(&file));

	String title = build_context.n64_title.len > 0
		? build_context.n64_title
		: n64_sanitized_rom_title(build_context.build_paths[BuildPath_Output].name);
	bool has_metadata = stage.metadata_make_path.len > 0;
	gb_fprintf(&file,
		"override CURDIR := .\n"
		"override N64_INST := sdk\n"
		"export N64_INST\n"
		"override N64_GCCPREFIX := sdk\n"
		"override N64_TARGET := mips64-elf\n"
		"override N64_BACKTRACE_FILE_PREFIX := .\n"
		"override N64_ROM_HEADER :=\n"
		"override CCACHE :=\n"
		"override BUILD_DIR := build\n"
		"override SOURCE_DIR := .\n"
		"override ROM := odin-n64.z64\n"
		"override ELF := $(BUILD_DIR)/odin-n64.elf\n"
		"override N64_ROM_TITLE := \"%.*s\"\n"
		"override N64_ROM_REGION := %.*s\n"
		"override N64_ROM_SAVETYPE := %.*s\n"
		"override N64_ROM_RTC := %s\n"
		"override N64_ROM_CONTROLLER1 := %.*s\n"
		"override N64_ROM_CONTROLLER2 := %.*s\n"
		"override N64_ROM_CONTROLLER3 := %.*s\n"
		"override N64_ROM_CONTROLLER4 := %.*s\n"
		"override N64_ROM_METADATA := %.*s\n"
		"override N64_MKDFS_ROOT := %s\n"
		"override V := %s\n"
		"\n"
		"include sdk/include/n64.mk\n"
		"\n",
		LIT(title),
		LIT(build_context.n64_region),
		LIT(build_context.n64_save_type),
		build_context.n64_rtc ? "1" : "",
		LIT(build_context.n64_controllers[0]),
		LIT(build_context.n64_controllers[1]),
		LIT(build_context.n64_controllers[2]),
		LIT(build_context.n64_controllers[3]),
		LIT(stage.metadata_make_path),
		build_context.n64_assets.len > 0 ? "assets" : "filesystem",
		build_context.show_system_calls ? "1" : "");
	if (has_metadata) {
		// The pinned n64.mk requests --padding 0 to defer final padding to
		// n64metadata, while its pinned n64tool requires a unit suffix for
		// a zero value. 0B preserves the intended no-prepadding build graph.
		gb_fprintf(&file,
			"override N64_TOOLFLAGS := $(filter-out --padding 0,$(N64_TOOLFLAGS)) --padding 0B\n"
			"\n");
	}
	gb_fprintf(&file, "$(ELF):");
	for (String const &input_name : stage.input_names) {
		gb_fprintf(&file, " %.*s", LIT(input_name));
	}
	gb_fprintf(&file, "\n");
	if (build_context.n64_assets.len > 0) {
		gb_fprintf(&file, "$(ROM): $(BUILD_DIR)/odin-n64.dfs\n");
	}
	gb_fprintf(&file,
		"\n"
		".PHONY: all\n"
		"all: $(ROM)\n");
	return true;
}

#if !defined(GB_SYSTEM_WINDOWS)
gb_internal bool n64_environment_key_is_filtered(char const *entry) {
	char const *equals = strchr(entry, '=');
	if (equals == nullptr) {
		return true;
	}
	isize key_length = equals-entry;
	char const *exact_keys[] = {
		"PATH", "SHELL", "MAKEFLAGS", "MFLAGS", "GNUMAKEFLAGS", "MAKEOVERRIDES", "MAKELEVEL",
		"CCACHE", "V", "D",
	};
	for (char const *key : exact_keys) {
		isize length = cast(isize)strlen(key);
		if (key_length == length && memcmp(entry, key, length) == 0) {
			return true;
		}
	}
	return (key_length >= 4 && memcmp(entry, "N64_", 4) == 0) ||
	       (key_length >= 7 && memcmp(entry, "CCACHE_", 7) == 0);
}

gb_internal char **n64_sanitized_environment(void) {
	isize source_count = 0;
	while (environ[source_count] != nullptr) {
		source_count += 1;
	}
	char **environment = gb_alloc_array(temporary_allocator(), char *, source_count+3);
	isize destination_count = 0;
	for (isize index = 0; index < source_count; index += 1) {
		if (!n64_environment_key_is_filtered(environ[index])) {
			environment[destination_count++] = environ[index];
		}
	}
	environment[destination_count++] = cast(char *)"PATH=/usr/bin:/bin:/usr/sbin:/sbin";
	environment[destination_count++] = cast(char *)"SHELL=/bin/sh";
	environment[destination_count] = nullptr;
	return environment;
}
#endif

gb_internal i32 n64_run_make(N64BuildStage const &stage) {
#if defined(GB_SYSTEM_WINDOWS)
	gb_printf_err("The integrated N64 build currently requires a POSIX host\n");
	return 1;
#else
	char const *make_path = "/usr/bin/make";
	if (!n64_sdk_tool_is_executable(make_string_c(make_path))) {
		gb_printf_err("GNU make is required at %s for the integrated N64 build\n", make_path);
		return 1;
	}
	char const *work_dir = alloc_cstring(permanent_allocator(), stage.work_dir);
	char const *arguments[8] = {};
	isize count = 0;
	arguments[count++] = make_path;
	arguments[count++] = "-C";
	arguments[count++] = work_dir;
	arguments[count++] = "-f";
	arguments[count++] = "Makefile";
	arguments[count++] = "odin-n64.z64";
	arguments[count] = nullptr;
	if (build_context.show_system_calls) {
		gb_printf_err("[SYSTEM CALL] n64-make\n%s -C \"%s\" -f Makefile odin-n64.z64\n\n", make_path, work_dir);
	}

	pid_t pid = 0;
	int status = posix_spawn(&pid, make_path, nullptr, nullptr, cast(char *const *)arguments, n64_sanitized_environment());
	if (status != 0) {
		gb_printf_err("Could not spawn N64 packaging subprocess: %s\n", strerror(status));
		return -1;
	}
	for (;;) {
		if (waitpid(pid, &status, WUNTRACED) < 0) {
			gb_printf_err("Could not wait on N64 packaging subprocess: %s\n", strerror(errno));
			return -1;
		}
		if (WIFEXITED(status)) {
			return WEXITSTATUS(status);
		}
		if (WIFSIGNALED(status)) {
			gb_printf_err("N64 packaging subprocess terminated by signal %d\n", WTERMSIG(status));
			return 128+WTERMSIG(status);
		}
	}
#endif
}

gb_internal bool n64_remove_file_if_present(String const &path) {
	char const *path_c = alloc_cstring(temporary_allocator(), path);
	return !gb_file_exists(path_c) || gb_file_remove(path_c);
}

gb_internal bool n64_cleanup_successful_stage(N64BuildStage const &stage) {
	bool clean = true;
	for (String const &path : stage.input_paths) {
		clean = n64_remove_file_if_present(path) && clean;
	}
	for (String const &path : stage.metadata_entry_paths) {
		clean = n64_remove_file_if_present(path) && clean;
	}
	String generated_names[] = {
		STR_LIT("odin-n64.elf"),
		STR_LIT("odin-n64.map"),
		STR_LIT("odin-n64.elf.sym"),
		STR_LIT("odin-n64.elf.stripped"),
		STR_LIT("odin-n64.dfs"),
		STR_LIT("odin-n64.z64.tmp"),
	};
	for (String const &name : generated_names) {
		String path = n64_path_join(temporary_allocator(), stage.build_dir, name);
		clean = n64_remove_file_if_present(path) && clean;
	}
	clean = n64_remove_file_if_present(stage.staged_rom_path) && clean;
	clean = n64_remove_file_if_present(stage.makefile_path) && clean;
	clean = n64_remove_file_if_present(stage.metadata_path) && clean;
	clean = n64_remove_file_if_present(stage.assets_link_path) && clean;
	clean = gb_file_remove(alloc_cstring(temporary_allocator(), stage.sdk_link_path)) && clean;
	if (build_context.n64_metadata.len > 0) {
		clean = n64_remove_directory(stage.metadata_dir_path) && clean;
	}
	clean = n64_remove_directory(stage.build_dir) && clean;
	clean = n64_remove_directory(stage.work_dir) && clean;
	return clean;
}

gb_internal i32 n64_linker_stage(LinkerData *linker, String const &output_filename) {
	String extra_flags = string_trim_whitespace(build_context.extra_linker_flags);
	if (extra_flags.len > 0) {
		gb_printf_err("-extra-linker-flags is not supported by the pinned N64 packaging pipeline\n");
		return 1;
	}

	N64BuildStage stage = {};
	if (!n64_init_build_stage(&stage, output_filename)) {
		return 1;
	}
	gb_printf_err("N64 build intermediates: %.*s\n", LIT(stage.work_dir));
	if (!n64_stage_link_inputs(&stage, linker) || !n64_write_makefile(stage)) {
		gb_printf_err("N64 build failed; intermediates were retained at %.*s\n", LIT(stage.work_dir));
		return 1;
	}

	i32 result = n64_run_make(stage);
	if (result != 0) {
		gb_printf_err("N64 build failed; intermediates were retained at %.*s\n", LIT(stage.work_dir));
		return result;
	}

	char const *staged_rom = alloc_cstring(temporary_allocator(), stage.staged_rom_path);
	char const *final_rom = alloc_cstring(temporary_allocator(), output_filename);
	if (rename(staged_rom, final_rom) != 0) {
		gb_printf_err("Failed to atomically place N64 ROM at %.*s: %s\n", LIT(output_filename), strerror(errno));
		gb_printf_err("N64 build intermediates were retained at %.*s\n", LIT(stage.work_dir));
		return 1;
	}

	if (build_context.keep_temp_files) {
		gb_printf_err("Retained N64 build intermediates: %.*s\n", LIT(stage.work_dir));
	} else {
		if (!n64_cleanup_successful_stage(stage)) {
			gb_printf_err("Warning: could not completely remove N64 build intermediates at %.*s\n", LIT(stage.work_dir));
		}
	}
	return 0;
}

// No longer required not that LLVM 14 is removed(?)
gb_internal void linker_enable_system_library_linking(LinkerData *ld) {
	ld->needs_system_library_linked = true;
}

gb_internal void linker_data_init(LinkerData *ld, CheckerInfo *info, String const &init_fullpath) {
	gbAllocator ha = heap_allocator();
	array_init(&ld->output_object_paths, ha);
	array_init(&ld->output_temp_paths,   ha);
	array_init(&ld->foreign_libraries,   ha, 0, 1024);
	ptr_set_init(&ld->foreign_libraries_set, 1024);

	ld->needs_system_library_linked = false;

	if (build_context.out_filepath.len == 0) {
		ld->output_name = remove_directory_from_path(init_fullpath);
		ld->output_name = remove_extension_from_path(ld->output_name);
		ld->output_name = string_trim_whitespace(ld->output_name);
		if (ld->output_name.len == 0) {
			ld->output_name = info->init_scope->pkg->name;
		}
		ld->output_base = ld->output_name;
	} else {
		ld->output_name = build_context.out_filepath;
		ld->output_name = string_trim_whitespace(ld->output_name);
		if (ld->output_name.len == 0) {
			ld->output_name = info->init_scope->pkg->name;
		}
		isize pos = string_extension_position(ld->output_name);
		if (pos < 0) {
			ld->output_base = ld->output_name;
		} else {
			ld->output_base = substring(ld->output_name, 0, pos);
		}
	}

	ld->output_base = path_to_full_path(ha, ld->output_base);

}

gb_internal i32 linker_stage(LinkerData *gen) {
	i32 result = 0;
	Timings *timings = &global_timings;

	String output_filename = path_to_string(heap_allocator(), build_context.build_paths[BuildPath_Output]);
	debugf("Linking %.*s\n", LIT(output_filename));
	if (build_context.metrics.os == TargetOs_n64 && build_context.build_mode == BuildMode_Executable) {
		return n64_linker_stage(gen, output_filename);
	}

	// TOOD(Jeroen): Make a `build_paths[BuildPath_Object] to avoid `%.*s.o`.

	if (is_arch_wasm()) {
		timings_start_section(timings, str_lit("wasm-ld"));

		gbString lib_str = gb_string_make(heap_allocator(), "");

		gbString extra_orca_flags = gb_string_make(temporary_allocator(), "");

		gbString inputs = gb_string_make(temporary_allocator(), "");
		inputs = gb_string_append_fmt(inputs, "\"%.*s.o\"", LIT(output_filename));


		for (Entity *e : gen->foreign_libraries) {
			GB_ASSERT(e->kind == Entity_LibraryName);
			// NOTE(bill): Add these before the linking values
			String extra_linker_flags = string_trim_whitespace(e->LibraryName.extra_linker_flags);
			if (extra_linker_flags.len != 0) {
				lib_str = gb_string_append_fmt(lib_str, " %.*s", LIT(extra_linker_flags));
			}

			for_array(i, e->LibraryName.paths) {
				String lib = e->LibraryName.paths[i];

				if (lib.len == 0) {
					continue;
				}

				if (!string_ends_with(lib, str_lit(".o"))) {
					continue;
				}

				inputs = gb_string_append_fmt(inputs, " \"%.*s\"", LIT(lib));
			}
		}

		if (build_context.metrics.os == TargetOs_orca) {
			gbString orca_sdk_path = gb_string_make(temporary_allocator(), "");
			if (!system_exec_command_line_app_output("orca sdk-path", &orca_sdk_path)) {
				gb_printf_err("executing `orca sdk-path` failed, make sure Orca is installed and added to your path\n");
				return 1;
			}
			if (gb_string_length(orca_sdk_path) == 0) {
				gb_printf_err("executing `orca sdk-path` did not produce output\n");
				return 1;
			}
			inputs = gb_string_append_fmt(inputs, " \"%s/orca-libc/lib/crt1.o\" \"%s/orca-libc/lib/libc.a\"", orca_sdk_path, orca_sdk_path);

			extra_orca_flags = gb_string_append_fmt(extra_orca_flags, " -L \"%s/bin\" -lorca_wasm --export-dynamic", orca_sdk_path);
		}


	#if defined(GB_SYSTEM_WINDOWS)
		result = system_exec_command_line_app("wasm-ld",
			"\"%.*s\\bin\\wasm-ld\" %s -o \"%.*s\" %.*s %.*s %s %s",
			LIT(build_context.ODIN_ROOT),
			inputs, LIT(output_filename), LIT(build_context.link_flags), LIT(build_context.extra_linker_flags),
			lib_str,
			extra_orca_flags);
	#else
		result = system_exec_command_line_app("wasm-ld",
			"wasm-ld %s -o \"%.*s\" %.*s %.*s %s %s",
			inputs, LIT(output_filename),
			LIT(build_context.link_flags),
			LIT(build_context.extra_linker_flags),
			lib_str,
			extra_orca_flags);
	#endif
		return result;
	}

	bool is_cross_linking = false;
	bool is_android = false;

	if (build_context.cross_compiling && (build_context.different_os || selected_subtarget != Subtarget_Default)) {
		switch (selected_subtarget) {
		case Subtarget_Android:
			is_cross_linking = true;
			is_android = true;
			goto try_cross_linking;
		default:
			gb_printf_err("Linking for cross compilation for this platform is not yet supported (%.*s %.*s)\n",
				LIT(target_os_names[build_context.metrics.os]),
				LIT(target_arch_names[build_context.metrics.arch])
			);
			build_context.keep_object_files = true;
			break;
		}
	} else {
try_cross_linking:;

	#if defined(GB_SYSTEM_WINDOWS)
		String section_name = str_lit("msvc-link");
		bool is_windows = build_context.metrics.os == TargetOs_windows;
	#else
		String section_name = str_lit("ld-link");
		bool is_windows = false;
	#endif

		bool is_osx = build_context.metrics.os == TargetOs_darwin;


		switch (build_context.linker_choice) {
		case Linker_Default:  break;
		case Linker_lld:      section_name = str_lit("lld-link"); break;
	#if defined(GB_SYSTEM_LINUX) || defined(GB_SYSTEM_FREEBSD) || defined(GB_SYSTEM_NETBSD)
		case Linker_mold:     section_name = str_lit("mold-link"); break;
	#endif
	#if defined(GB_SYSTEM_WINDOWS)
		case Linker_radlink:  section_name = str_lit("rad-link"); break;
	#endif
		default:
			gb_printf_err("'%.*s' linker is not supported on this platform\n", LIT(linker_choices[build_context.linker_choice]));
			return 1;
		}


		if (is_windows) {
			timings_start_section(timings, section_name);

			gbString lib_str = gb_string_make(heap_allocator(), "");
			defer (gb_string_free(lib_str));

			gbString link_settings = gb_string_make_reserve(heap_allocator(), 256);
			defer (gb_string_free(link_settings));

			// Add library search paths.
			if (build_context.build_paths[BuildPath_VS_LIB].basename.len > 0) {
				String path = {};
				auto add_path = [&](String path) {
					if (path[path.len-1] == '\\') {
						path.len -= 1;
					}
					link_settings = gb_string_append_fmt(link_settings, " /LIBPATH:\"%.*s\"", LIT(path));
				};
				add_path(build_context.build_paths[BuildPath_Win_SDK_UM_Lib].basename);
				add_path(build_context.build_paths[BuildPath_Win_SDK_UCRT_Lib].basename);
				add_path(build_context.build_paths[BuildPath_VS_LIB].basename);
			}


			StringSet min_libs_set = {};
			string_set_init(&min_libs_set, 64);
			defer (string_set_destroy(&min_libs_set));

			String prev_lib = {};

			StringSet asm_files = {};
			string_set_init(&asm_files, 64);
			defer (string_set_destroy(&asm_files));

			for (Entity *e : gen->foreign_libraries) {
				GB_ASSERT(e->kind == Entity_LibraryName);
				// NOTE(bill): Add these before the linking values
				String extra_linker_flags = string_trim_whitespace(e->LibraryName.extra_linker_flags);
				if (extra_linker_flags.len != 0) {
					lib_str = gb_string_append_fmt(lib_str, " %.*s", LIT(extra_linker_flags));
				}
				for_array(i, e->LibraryName.paths) {
					String lib = string_trim_whitespace(e->LibraryName.paths[i]);
					// IMPORTANT NOTE(bill): calling `string_to_lower` here is not an issue because
					// we will never uses these strings afterwards
					string_to_lower(&lib);
					if (lib.len == 0) {
						continue;
					}

					if (has_asm_extension(lib)) {
						if (!string_set_update(&asm_files, lib)) {
							String asm_file = lib;
							String obj_file = {};
							String temp_dir = temporary_directory(temporary_allocator());
							if (temp_dir.len != 0) {
								String filename = filename_without_directory(asm_file);

								gbString str = gb_string_make(heap_allocator(), "");
								str = gb_string_append_length(str, temp_dir.text, temp_dir.len);
								str = gb_string_appendc(str, "/");
								str = gb_string_append_length(str, filename.text, filename.len);
								str = gb_string_append_fmt(str, "-%p.obj", asm_file.text);
								obj_file = make_string_c(str);
							} else {
								obj_file = concatenate_strings(permanent_allocator(), asm_file, str_lit(".obj"));
							}

							String obj_format = str_lit("win64");
						#if defined(GB_ARCH_32_BIT)
							obj_format = str_lit("win32");
						#endif

							result = system_exec_command_line_app("nasm",
								"\"%.*s\\bin\\nasm\\windows\\nasm.exe\" \"%.*s\" "
								"-f \"%.*s\" "
								"-o \"%.*s\" "
								"%.*s "
								"",
								LIT(build_context.ODIN_ROOT), LIT(asm_file),
								LIT(obj_format),
								LIT(obj_file),
								LIT(build_context.extra_assembler_flags)
							);

							if (result) {
								return result;
							}
							array_add(&gen->output_object_paths, obj_file);
						}
					} else if (!string_set_update(&min_libs_set, lib) ||
					           !build_context.min_link_libs) {
						if (prev_lib != lib) {
							lib_str = gb_string_append_fmt(lib_str, " \"%.*s\"", LIT(lib));
						}
						prev_lib = lib;
					}
				}
			}

			if (build_context.build_mode == BuildMode_DynamicLibrary) {
				link_settings = gb_string_append_fmt(link_settings, " /DLL");
				if (build_context.no_entry_point) {
					link_settings = gb_string_append_fmt(link_settings, " /NOENTRY");
				}
			} else {
				// For i386 with CRT, libcmt provides the entry point
				// For other cases or no_crt, we need to specify the entry point
				if (!(build_context.metrics.arch == TargetArch_i386 && !build_context.no_crt)) {
					link_settings = gb_string_append_fmt(link_settings, " /ENTRY:mainCRTStartup");
				}
			}

			if (build_context.build_paths[BuildPath_Symbols].name != "") {
				String symbol_path = path_to_string(heap_allocator(), build_context.build_paths[BuildPath_Symbols]);
				link_settings = gb_string_append_fmt(link_settings, " /PDB:\"%.*s\"", LIT(symbol_path));
			}

			if (build_context.build_mode != BuildMode_StaticLibrary) {
				if (build_context.no_crt) {
					link_settings = gb_string_append_fmt(link_settings, " /nodefaultlib");
				} else {
					link_settings = gb_string_append_fmt(link_settings, " /defaultlib:libcmt");
				}
			}

			if (build_context.ODIN_DEBUG) {
				link_settings = gb_string_append_fmt(link_settings, " /DEBUG");
			}

			gbString object_files = gb_string_make(heap_allocator(), "");
			defer (gb_string_free(object_files));
			for (String const &object_path : gen->output_object_paths) {
				object_files = gb_string_append_fmt(object_files, "\"%.*s\" ", LIT(object_path));
			}

			String vs_exe_path = path_to_string(heap_allocator(), build_context.build_paths[BuildPath_VS_EXE]);
			defer (gb_free(heap_allocator(), vs_exe_path.text));

			String windows_sdk_bin_path = path_to_string(heap_allocator(), build_context.build_paths[BuildPath_Win_SDK_Bin_Path]);
			defer (gb_free(heap_allocator(), windows_sdk_bin_path.text));

			gbString lld_lto_flags = gb_string_make(heap_allocator(), "");
			defer (gb_string_free(lld_lto_flags));
			if (build_context.lto_kind != LTO_None) {
				lld_lto_flags = gb_string_append_fmt(lld_lto_flags, "/opt:lldltojobs=%d ", build_context.thread_count);
			}

			switch (build_context.linker_choice) {
			case Linker_lld:
				result = system_exec_command_line_app("msvc-lld-link",
					"\"%.*s\\bin\\lld-link\" %s -OUT:\"%.*s\" %s "
					"/nologo /incremental:no /opt:ref /subsystem:%.*s "
					"%.*s "
					"%.*s "
					"%s "
					"%s "
					"",
					LIT(build_context.ODIN_ROOT), object_files, LIT(output_filename),
					link_settings,
					LIT(windows_subsystem_names[build_context.ODIN_WINDOWS_SUBSYSTEM]),
					LIT(build_context.link_flags),
					LIT(build_context.extra_linker_flags),
					lib_str,
					lld_lto_flags
				);

				if (result) {
					return result;
				}
				break;
			case Linker_radlink:
				result = system_exec_command_line_app("msvc-rad-link",
					"\"%.*s\\bin\\radlink\" %s -OUT:\"%.*s\" %s "
					"/nologo /incremental:no /opt:ref /subsystem:%.*s "
					"%.*s "
					"%.*s "
					"%s "
					"",
					LIT(build_context.ODIN_ROOT), object_files, LIT(output_filename),
					link_settings,
					LIT(windows_subsystem_names[build_context.ODIN_WINDOWS_SUBSYSTEM]),
					LIT(build_context.link_flags),
					LIT(build_context.extra_linker_flags),
					lib_str
				);

				if (result) {
					return result;
				}
				break;
			default: { // msvc
				String res_path = quote_path(heap_allocator(), build_context.build_paths[BuildPath_RES]);
				String rc_path  = quote_path(heap_allocator(), build_context.build_paths[BuildPath_RC]);
				defer (gb_free(heap_allocator(), res_path.text));
				defer (gb_free(heap_allocator(), rc_path.text));

				if (build_context.has_resource) {
					if (build_context.build_paths[BuildPath_RC].basename == "")  {
						debugf("Using precompiled resource %.*s\n", LIT(res_path));
					} else {
						debugf("Compiling resource %.*s\n", LIT(res_path));

						result = system_exec_command_line_app("msvc-link",
							"\"%.*src.exe\" /nologo /fo %.*s %.*s",
							LIT(windows_sdk_bin_path),
							LIT(res_path),
							LIT(rc_path)
						);

						if (result) {
							return result;
						}
					}
				} else {
					res_path = {};
				}

				String linker_name = str_lit("link.exe");
				switch (build_context.build_mode) {
				case BuildMode_Executable:
					link_settings = gb_string_append_fmt(link_settings, " /NOIMPLIB /NOEXP");
					break;
				}

				switch (build_context.build_mode) {
				case BuildMode_StaticLibrary:
					linker_name = str_lit("lib.exe");
					break;
				default:
					link_settings = gb_string_append_fmt(link_settings, " /incremental:no /opt:ref");
					break;
				}


				result = system_exec_command_line_app("msvc-link",
					"\"%.*s%.*s\" %s %.*s -OUT:\"%.*s\" %s "
					"/nologo /subsystem:%.*s "
					"%.*s "
					"%.*s "
					"%s "
					"",
					LIT(vs_exe_path), LIT(linker_name), object_files, LIT(res_path), LIT(output_filename),
					link_settings,
					LIT(windows_subsystem_names[build_context.ODIN_WINDOWS_SUBSYSTEM]),
					LIT(build_context.link_flags),
					LIT(build_context.extra_linker_flags),
					lib_str
				);
				if (result) {
					return result;
				}
				break;
			}
			}
		} else {

			timings_start_section(timings, section_name);

			int const ODIN_ANDROID_API_LEVEL = build_context.ODIN_ANDROID_API_LEVEL;

			String ODIN_ANDROID_NDK                     = build_context.ODIN_ANDROID_NDK;
			String ODIN_ANDROID_NDK_TOOLCHAIN           = build_context.ODIN_ANDROID_NDK_TOOLCHAIN;
			String ODIN_ANDROID_NDK_TOOLCHAIN_LIB       = build_context.ODIN_ANDROID_NDK_TOOLCHAIN_LIB;
			String ODIN_ANDROID_NDK_TOOLCHAIN_LIB_LEVEL = build_context.ODIN_ANDROID_NDK_TOOLCHAIN_LIB_LEVEL;
			String ODIN_ANDROID_NDK_TOOLCHAIN_SYSROOT   = build_context.ODIN_ANDROID_NDK_TOOLCHAIN_SYSROOT;

			// Link using `clang`, unless overridden by `ODIN_CLANG_PATH` environment variable.
			const char* clang_path = gb_get_env("ODIN_CLANG_PATH", permanent_allocator());
			bool has_odin_clang_path_env = true;
			if (clang_path == NULL) {
				clang_path = "clang";
				has_odin_clang_path_env = false;
			}

			// NOTE(vassvik): needs to add the root to the library search paths, so that the full filenames of the library
			//                files can be passed with -l:
			gbString lib_str = gb_string_make(heap_allocator(), "");
			defer (gb_string_free(lib_str));
			#if !defined(GB_SYSTEM_WINDOWS)
				lib_str = gb_string_appendc(lib_str, "-L/ ");
			#endif
			
			StringSet asm_files = {};
			string_set_init(&asm_files, 64);
			defer (string_set_destroy(&asm_files));
			
			StringSet min_libs_set = {};
			string_set_init(&min_libs_set, 64);
			defer (string_set_destroy(&min_libs_set));

			String prev_lib = {};
			
			for (Entity *e : gen->foreign_libraries) {
				GB_ASSERT(e->kind == Entity_LibraryName);
				// NOTE(bill): Add these before the linking values
				String extra_linker_flags = string_trim_whitespace(e->LibraryName.extra_linker_flags);
				if (extra_linker_flags.len != 0) {
					lib_str = gb_string_append_fmt(lib_str, " %.*s", LIT(extra_linker_flags));
				}

				if (build_context.metrics.os == TargetOs_darwin) {
					// Print frameworks first
					for (String lib : e->LibraryName.paths) {
						lib = string_trim_whitespace(lib);
						if (lib.len == 0) {
							continue;
						}
						if (string_ends_with(lib, str_lit(".framework"))) {
							if (string_set_update(&min_libs_set, lib)) {
								continue;
							}

							String lib_name = lib;
							lib_name = remove_extension_from_path(lib_name);
							lib_str = gb_string_append_fmt(lib_str, " -framework %.*s ", LIT(lib_name));
						}
					}
				}

				for (String lib : e->LibraryName.paths) {
					lib = string_trim_whitespace(lib);
					if (lib.len == 0) {
						continue;
					}
					if (has_asm_extension(lib)) {
						if (string_set_update(&asm_files, lib)) {
							continue; // already handled
						}
						String asm_file = lib;
						String obj_file = {};

						String temp_dir = temporary_directory(temporary_allocator());
						if (temp_dir.len != 0) {
							String filename = filename_without_directory(asm_file);

							gbString str = gb_string_make(heap_allocator(), "");
							str = gb_string_append_length(str, temp_dir.text, temp_dir.len);
							str = gb_string_appendc(str, "/");
							str = gb_string_append_length(str, filename.text, filename.len);
							str = gb_string_append_fmt(str, "-%p.o", asm_file.text);
							obj_file = make_string_c(str);
						} else {
							obj_file = concatenate_strings(permanent_allocator(), asm_file, str_lit(".o"));
						}

						String obj_format;
						if (build_context.metrics.ptr_size == 8) {
							if (is_osx) {
								obj_format = str_lit("macho64");
							} else {
								obj_format = str_lit("elf64");
							}
						} else {
							GB_ASSERT(build_context.metrics.ptr_size == 4);
							if (is_osx) {
								obj_format = str_lit("macho32");
							} else {
								obj_format = str_lit("elf32");
							}
						}

						if (build_context.metrics.arch == TargetArch_riscv64) {
							result = system_exec_command_line_app("clang",
								"%s \"%.*s\" "
								"-c -o \"%.*s\" "
								"-target %.*s -march=rv64gc "
								"%.*s "
								"",
								clang_path,
								LIT(asm_file),
								LIT(obj_file),
								LIT(build_context.metrics.target_triplet),
								LIT(build_context.extra_assembler_flags)
							);
						} else if (is_osx) {
							// `as` comes with MacOS.
							result = system_exec_command_line_app("as",
								"as \"%.*s\" "
								"-o \"%.*s\" "
								"%.*s "
								"",
								LIT(asm_file),
								LIT(obj_file),
								LIT(build_context.extra_assembler_flags)
							);
						} else if (build_context.metrics.arch == TargetArch_arm64) {
							result = system_exec_command_line_app("clang",
								"%s \"%.*s\" "
								"-c -o \"%.*s\" "
								"-target %.*s "
								"%.*s "
								"",
								clang_path,
								LIT(asm_file),
								LIT(obj_file),
								LIT(build_context.metrics.target_triplet),
								LIT(build_context.extra_assembler_flags)
							);
						} else {
							// Note(bumbread): I'm assuming nasm is installed on the host machine.
							// Shipping binaries on unix-likes gets into the weird territorry of
							// "which version of glibc" is it linked with.
							result = system_exec_command_line_app("nasm",
								"nasm \"%.*s\" "
								"-f \"%.*s\" "
								"-o \"%.*s\" "
								"%.*s "
								"",
								LIT(asm_file),
								LIT(obj_format),
								LIT(obj_file),
								LIT(build_context.extra_assembler_flags)
							);						
							if (result) {
								gb_printf_err("executing `nasm` to assemble foreing import of %.*s failed.\n\tSuggestion: `nasm` does not ship with the compiler and should be installed with your system's package manager.\n", LIT(asm_file));
								return result;
							}
						}
						array_add(&gen->output_object_paths, obj_file);
					} else {
						bool short_circuit = false;
						if (string_ends_with(lib, str_lit(".framework"))) {
							short_circuit = true;
						} else if (string_ends_with(lib, str_lit(".dylib"))) {
							short_circuit = true;
						} else if (string_ends_with(lib, str_lit(".so"))) {
							short_circuit = true;
						} else if (e->LibraryName.ignore_duplicates) {
							short_circuit = true;
						}

						if (string_set_update(&min_libs_set, lib) && (build_context.min_link_libs || short_circuit)) {
							continue;
						}

						if (prev_lib == lib) {
							continue;
						}
						prev_lib = lib;

						// Do not add libc again, this is added later already, and omitted with
						// the `-no-crt` flag, not skipping here would cause duplicate library
						// warnings when linking on darwin and might link libc silently even with `-no-crt`.
						if (lib == str_lit("System.framework") || lib == str_lit("System") || lib == str_lit("c")) {
							continue;
						}

						if (build_context.metrics.os == TargetOs_darwin) {
							if (string_ends_with(lib, str_lit(".framework"))) {
								// framework thingie
								String lib_name = lib;
								lib_name = remove_extension_from_path(lib_name);
								lib_str = gb_string_append_fmt(lib_str, " -framework %.*s ", LIT(lib_name));
							} else if (string_ends_with(lib, str_lit(".a")) || string_ends_with(lib, str_lit(".o")) || string_ends_with(lib, str_lit(".dylib"))) {
								// For:
								// object
								// dynamic lib
								// static libs, absolute full path relative to the file in which the lib was imported from
								lib_str = gb_string_append_fmt(lib_str, " \"%.*s\" ", LIT(lib));
							} else {
								// dynamic or static system lib, just link regularly searching system library paths
								lib_str = gb_string_append_fmt(lib_str, " -l%.*s ", LIT(lib));
							}
						} else {
							// NOTE(vassvik): static libraries (.a files) in linux can be linked to directly using the full path,
							//                since those are statically linked to at link time. shared libraries (.so) has to be
							//                available at runtime wherever the executable is run, so we make require those to be
							//                local to the executable (unless the system collection is used, in which case we search
							//                the system library paths for the library file).
							if (string_ends_with(lib, str_lit(".a")) || string_ends_with(lib, str_lit(".o")) || string_ends_with(lib, str_lit(".so")) || string_contains_string(lib, str_lit(".so."))) {
								lib_str = gb_string_append_fmt(lib_str, " -l:\"%.*s\" ", LIT(lib));
							} else {
								// dynamic or static system lib, just link regularly searching system library paths
								lib_str = gb_string_append_fmt(lib_str, " -l%.*s ", LIT(lib));
							}
						}
					}
				}
			}

			gbString object_files = gb_string_make(heap_allocator(), "");
			defer (gb_string_free(object_files));


			if (is_android) { // NOTE(bill): glue code needed for Android
				TIME_SECTION("Android Native App Glue Compile");

				String android_glue_object = {};
				String android_glue_static_lib = {};

				char hash_buf[64] = {};
				gb_snprintf(hash_buf, gb_size_of(hash_buf), "%p", &hash_buf);
				String hash = make_string_c(hash_buf);

				String temp_dir = normalize_path(temporary_allocator(), temporary_directory(temporary_allocator()), NIX_SEPARATOR_STRING);
				android_glue_object = concatenate4_strings(temporary_allocator(), temp_dir, str_lit("android_native_app_glue-"), hash, str_lit(".o"));
				android_glue_static_lib = concatenate4_strings(permanent_allocator(), temp_dir, str_lit("libandroid_native_app_glue-"), hash, str_lit(".a"));

				gbString glue = gb_string_make_length(heap_allocator(), ODIN_ANDROID_NDK_TOOLCHAIN.text, ODIN_ANDROID_NDK_TOOLCHAIN.len);
				defer (gb_string_free(glue));

				glue = gb_string_append_fmt(glue, "bin/clang");
				glue = gb_string_append_fmt(glue, " --target=%.*s%d ", LIT(build_context.metrics.target_triplet), ODIN_ANDROID_API_LEVEL);
				glue = gb_string_appendc(glue, "-c \"");
				glue = gb_string_append_length(glue, ODIN_ANDROID_NDK.text, ODIN_ANDROID_NDK.len);
				glue = gb_string_appendc(glue, "sources/android/native_app_glue/android_native_app_glue.c");
				glue = gb_string_appendc(glue, "\" ");
				glue = gb_string_appendc(glue, "-o \"");
				glue = gb_string_append_length(glue, android_glue_object.text, android_glue_object.len);
				glue = gb_string_appendc(glue, "\" ");

				glue = gb_string_appendc(glue, "--sysroot \"");
				glue = gb_string_append_length(glue, ODIN_ANDROID_NDK_TOOLCHAIN.text, ODIN_ANDROID_NDK_TOOLCHAIN.len);
				glue = gb_string_appendc(glue, "sysroot");
				glue = gb_string_appendc(glue, "\" ");

				glue = gb_string_appendc(glue, "\"-I");
				glue = gb_string_append_length(glue, ODIN_ANDROID_NDK_TOOLCHAIN.text, ODIN_ANDROID_NDK_TOOLCHAIN.len);
				glue = gb_string_appendc(glue, "sysroot/usr/include/");
				glue = gb_string_appendc(glue, "\" ");

				glue = gb_string_appendc(glue, "\"-I");
				glue = gb_string_append_length(glue, ODIN_ANDROID_NDK_TOOLCHAIN.text, ODIN_ANDROID_NDK_TOOLCHAIN.len);
				glue = gb_string_appendc(glue, "sysroot/usr/include/");
				glue = gb_string_append_length(glue, ODIN_ANDROID_NDK_TOOLCHAIN_LIB.text, ODIN_ANDROID_NDK_TOOLCHAIN_LIB.len);
				glue = gb_string_appendc(glue, "/\" ");


				glue = gb_string_appendc(glue, "-Wno-macro-redefined ");

				result = system_exec_command_line_app("android-native-app-glue-compile", glue);
				if (result) {
					return result;
				}

				TIME_SECTION("Android Native App Glue ar");

				gbString ar = gb_string_make_length(heap_allocator(), ODIN_ANDROID_NDK_TOOLCHAIN.text, ODIN_ANDROID_NDK_TOOLCHAIN.len);
				defer (gb_string_free(ar));

				ar = gb_string_appendc(ar, "bin/llvm-ar");

				ar = gb_string_appendc(ar, " rcs ");

				ar = gb_string_appendc(ar, "\"");
				ar = gb_string_append_length(ar, android_glue_static_lib.text, android_glue_static_lib.len);
				ar = gb_string_appendc(ar, "\" ");

				ar = gb_string_appendc(ar, "\"");
				ar = gb_string_append_length(ar, android_glue_object.text, android_glue_object.len);
				ar = gb_string_appendc(ar, "\" ");

				result = system_exec_command_line_app("android-native-app-glue-ar", ar);
				if (result) {
					return result;
				}

				object_files = gb_string_append_fmt(object_files, "\"%.*s\" ", LIT(android_glue_static_lib));
			}


			for (String object_path : gen->output_object_paths) {
				object_files = gb_string_append_fmt(object_files, "\"%.*s\" ", LIT(object_path));
			}

			gbString link_settings = gb_string_make_reserve(heap_allocator(), 32);

			if (build_context.no_crt) {
				link_settings = gb_string_append_fmt(link_settings, "-nostdlib ");
			}

			if (build_context.build_mode == BuildMode_StaticLibrary) {
				TIME_SECTION("Static Library Creation");

				gbString ar_command = gb_string_make(heap_allocator(), "");
				defer (gb_string_free(ar_command));

				ar_command = gb_string_appendc(ar_command, "ar rcs ");
				ar_command = gb_string_append_fmt(ar_command, "\"%.*s\" ", LIT(output_filename));
				ar_command = gb_string_appendc(ar_command, object_files);

				result = system_exec_command_line_app("ar", ar_command);
				if (result) {
					return result;
				}

				return result;
			}

			// NOTE(dweiler): We use clang as a frontend for the linker as there are
			// other runtime and compiler support libraries that need to be linked in
			// very specific orders such as libgcc_s, ld-linux-so, unwind, etc.
			// These are not always typically inside /lib, /lib64, or /usr versions
			// of that, e.g libgcc.a is in /usr/lib/gcc/{version}, and can vary on
			// the distribution of Linux even. The gcc or clang specs is the only
			// reliable way to query this information to call ld directly.
			if (build_context.build_mode == BuildMode_DynamicLibrary) {
				// NOTE(dweiler): Let the frontend know we're building a shared library
				// so it doesn't generate symbols which cannot be relocated.
				link_settings = gb_string_appendc(link_settings, "-shared ");

				// NOTE(dweiler): _odin_entry_point must be called at initialization
				// time of the shared object, similarly, _odin_exit_point must be called
				// at deinitialization. We can pass both -init and -fini to the linker by
				// using a comma separated list of arguments to -Wl.
				//
				// This previously used ld but ld cannot actually build a shared library
				// correctly this way since all the other dependencies provided implicitly
				// by the compiler frontend are still needed and most of the command
				// line arguments prepared previously are incompatible with ld.
				if (build_context.metrics.os == TargetOs_darwin) {
					link_settings = gb_string_appendc(link_settings, "-Wl,-init,'__odin_entry_point' ");
					// NOTE(weshardee): __odin_exit_point should also be added, but -fini
					// does not exist on MacOS
				} else {
					link_settings = gb_string_appendc(link_settings, "-Wl,-init,'_odin_entry_point' ");
					link_settings = gb_string_appendc(link_settings, "-Wl,-fini,'_odin_exit_point' ");
				}
			} else if (is_android) {
				// Always shared even in android!
				link_settings = gb_string_appendc(link_settings, "-shared ");
			}

			if (build_context.build_mode == BuildMode_Executable && build_context.reloc_mode == RelocMode_PIC) {
				if (build_context.metrics.os == TargetOs_linux) {
					// Linux does not enable PIE by default but required for ASLR
					link_settings = gb_string_appendc(link_settings, "-pie ");
				} else {
					// Do not disable PIE, let the linker choose. (most likely you want it enabled)
				}
			} else if (build_context.build_mode != BuildMode_DynamicLibrary) {
				if (build_context.metrics.os != TargetOs_openbsd
					&& build_context.metrics.arch != TargetArch_riscv64
					&& !is_android
				) {
					// OpenBSD defaults to PIE executable, do not pass -no-pie for it.
					link_settings = gb_string_appendc(link_settings, "-no-pie ");
				}
			}

			gbString platform_lib_str = gb_string_make(heap_allocator(), "");
			defer (gb_string_free(platform_lib_str));
			if (build_context.metrics.os == TargetOs_darwin) {
				// Get the SDK path.
				gbString darwin_sdk_path = gb_string_make(temporary_allocator(), "");

				char const* darwin_platform_name  = "MacOSX";
				char const* darwin_xcrun_sdk_name = "macosx";
				char const* darwin_min_version_id = "macosx";

				const char* original_clang_path = clang_path;

				// NOTE(harold): We set the clang_path to run through xcrun because otherwise it complaints about the the sysroot
				//               being set to 'MacOSX' even though we've set the sysroot to the correct SDK (-Wincompatible-sysroot).
				//               This is because it is likely not using the SDK's toolchain Apple Clang but another one installed in the system.
				switch (selected_subtarget) {
				case Subtarget_iPhone:
					darwin_platform_name  = "iPhoneOS";
					darwin_xcrun_sdk_name = "iphoneos";
					darwin_min_version_id = "ios";
					if (!has_odin_clang_path_env) {
						clang_path = "xcrun --sdk iphoneos clang";
					}
					break;
				case Subtarget_iPhoneSimulator:
					darwin_platform_name  = "iPhoneSimulator";
					darwin_xcrun_sdk_name = "iphonesimulator";
					darwin_min_version_id = "ios-simulator";
					if (!has_odin_clang_path_env) {
						clang_path = "xcrun --sdk iphonesimulator clang";
					}
					break;
				}

				gbString darwin_find_sdk_cmd = gb_string_make(temporary_allocator(), "");
				darwin_find_sdk_cmd = gb_string_append_fmt(darwin_find_sdk_cmd, "xcrun --sdk %s --show-sdk-path", darwin_xcrun_sdk_name);

				if (!system_exec_command_line_app_output(darwin_find_sdk_cmd, &darwin_sdk_path)) {

					// Fallback to default clang, since `xcrun --sdk` did not work.
					clang_path = original_clang_path;

					// Best-effort fallback to known locations
					gbString darwin_sdk_path = gb_string_make(temporary_allocator(), "");
					darwin_sdk_path = gb_string_append_fmt(darwin_sdk_path, "/Library/Developer/CommandLineTools/SDKs/%s.sdk", darwin_platform_name);

					if (!path_is_directory(make_string_c(darwin_sdk_path))) {
						gb_string_clear(darwin_sdk_path);
						darwin_sdk_path = gb_string_append_fmt(darwin_sdk_path, "/Applications/Xcode.app/Contents/Developer/Platforms/%s.platform/Developer/SDKs/%s.sdk", darwin_platform_name);

						if (!path_is_directory(make_string_c(darwin_sdk_path))) {
							gb_printf_err("Failed to find %s SDK\n", darwin_platform_name);
							return -1;
						}
					}
				} else {
					// Trim the trailing newline.
					darwin_sdk_path = gb_string_trim_space(darwin_sdk_path);
				}
				platform_lib_str = gb_string_append_fmt(platform_lib_str, "--sysroot %s ", darwin_sdk_path);

				platform_lib_str = gb_string_appendc(platform_lib_str, "-L/usr/local/lib ");

				// Homebrew's default library path, checking if it exists to avoid linking warnings.
				if (gb_file_exists("/opt/homebrew/lib")) {
					platform_lib_str = gb_string_appendc(platform_lib_str, "-L/opt/homebrew/lib ");
				}

				// MacPort's default library path, checking if it exists to avoid linking warnings.
				if (gb_file_exists("/opt/local/lib")) {
					platform_lib_str = gb_string_appendc(platform_lib_str, "-L/opt/local/lib ");
				}

				// Only specify this flag if the user has given a minimum version to target.
				// This will cause warnings to show up for mismatched libraries.
				// NOTE(harold): For device subtargets we have to explicitly set the default version to 
				//               avoid the same warning since we configure our own minimum version when compiling for devices.
				if (build_context.minimum_os_version_string_given || selected_subtarget != Subtarget_Default) {
					link_settings = gb_string_append_fmt(link_settings, "-m%s-version-min=%.*s ", darwin_min_version_id, LIT(build_context.minimum_os_version_string));
				}

				if (build_context.build_mode != BuildMode_DynamicLibrary) {
					// This points the linker to where the entry point is
					link_settings = gb_string_appendc(link_settings, "-e _main ");
				}
			} else if (build_context.metrics.os == TargetOs_freebsd) {
				if (build_context.sanitizer_flags & (SanitizerFlag_Address | SanitizerFlag_Memory)) {
					// It's imperative that `pthread` is linked before `libc`,
					// otherwise ASan/MSan will be unable to call `pthread_key_create`
					// because FreeBSD's `libthr` implementation of `pthread`
					// needs to replace the relevant stubs first.
					//
					// (Presumably TSan implements its own `pthread` interface,
					//  which is why it isn't required.)
					//
					// See: https://reviews.llvm.org/D39254
					platform_lib_str = gb_string_appendc(platform_lib_str, "-lpthread ");
				}
				// FreeBSD pkg installs third-party shared libraries in /usr/local/lib.
				platform_lib_str = gb_string_appendc(platform_lib_str, "-Wl,-L/usr/local/lib ");
			} else if (build_context.metrics.os == TargetOs_openbsd) {
				// OpenBSD ports install shared libraries in /usr/local/lib. Also, we must explicitly link libpthread.
				platform_lib_str = gb_string_appendc(platform_lib_str, "-lpthread -Wl,-L/usr/local/lib ");
				// Until the LLVM back-end can be adapted to emit endbr64 instructions on amd64, we
				// need to pass -z nobtcfi in order to allow the resulting program to run under
				// OpenBSD 7.4 and newer. Once support is added at compile time, this can be dropped.
				platform_lib_str = gb_string_appendc(platform_lib_str, "-Wl,-z,nobtcfi ");
			} else if (build_context.metrics.os == TargetOs_linux) {
				// required for RELRO
				platform_lib_str = gb_string_appendc(platform_lib_str, "-Wl,-z,now -Wl,-z,relro ");
			}

			if (is_android) {
				GB_ASSERT(ODIN_ANDROID_NDK_TOOLCHAIN_LIB.len != 0);
				GB_ASSERT(ODIN_ANDROID_NDK_TOOLCHAIN_LIB_LEVEL.len != 0);
				GB_ASSERT(ODIN_ANDROID_NDK_TOOLCHAIN_SYSROOT.len != 0);

				platform_lib_str = gb_string_appendc(platform_lib_str, "\"-L");
				platform_lib_str = gb_string_append_length(platform_lib_str, ODIN_ANDROID_NDK_TOOLCHAIN_SYSROOT.text, ODIN_ANDROID_NDK_TOOLCHAIN_SYSROOT.len);
				platform_lib_str = gb_string_appendc(platform_lib_str, "usr/lib/");
				platform_lib_str = gb_string_append_length(platform_lib_str, ODIN_ANDROID_NDK_TOOLCHAIN_LIB.text, ODIN_ANDROID_NDK_TOOLCHAIN_LIB.len);
				platform_lib_str = gb_string_append_fmt(platform_lib_str, "/%d", ODIN_ANDROID_API_LEVEL);
				platform_lib_str = gb_string_appendc(platform_lib_str, "\" ");

				platform_lib_str = gb_string_appendc(platform_lib_str, "-landroid ");
				platform_lib_str = gb_string_appendc(platform_lib_str, "-llog ");

				platform_lib_str = gb_string_appendc(platform_lib_str, "\"--sysroot=");
				platform_lib_str = gb_string_append_length(platform_lib_str, ODIN_ANDROID_NDK_TOOLCHAIN_SYSROOT.text, ODIN_ANDROID_NDK_TOOLCHAIN_SYSROOT.len);
				platform_lib_str = gb_string_appendc(platform_lib_str, "\" ");

				link_settings = gb_string_appendc(link_settings, "-u ANativeActivity_onCreate ");
			}

			if (!build_context.no_rpath) {
				// Set the rpath to the $ORIGIN/@loader_path (the path of the executable),
				// so that dynamic libraries are looked for at that path.
				if (build_context.metrics.os == TargetOs_darwin) {
					link_settings = gb_string_appendc(link_settings, "-Wl,-rpath,@loader_path ");
				} else {
					if (is_android) {
						// ignore
					} else {
						link_settings = gb_string_appendc(link_settings, "-Wl,-rpath,\\$ORIGIN ");
					}
				}
			}

			if (!build_context.no_crt) {
				lib_str = gb_string_appendc(lib_str, "-lm ");
				if (build_context.metrics.os == TargetOs_darwin) {
					// NOTE: adding this causes a warning about duplicate libraries, I think it is
					// automatically assumed/added by clang when you don't do `-nostdlib`.
					// lib_str = gb_string_appendc(lib_str, "-lSystem ");
				} else {
					lib_str = gb_string_appendc(lib_str, "-lc ");
				}
			}

			gbString link_command_line = gb_string_make(heap_allocator(), "");
			defer (gb_string_free(link_command_line));

			if (is_android) {
				gbString ndk_bin_directory = gb_string_make_length(temporary_allocator(), ODIN_ANDROID_NDK_TOOLCHAIN.text, ODIN_ANDROID_NDK_TOOLCHAIN.len);
				link_command_line = gb_string_appendc(link_command_line, ndk_bin_directory);
				link_command_line = gb_string_appendc(link_command_line, "bin/clang");
				link_command_line = gb_string_append_fmt(link_command_line, " --target=%.*s%d ", LIT(build_context.metrics.target_triplet),  ODIN_ANDROID_API_LEVEL);
			} else {
				link_command_line = gb_string_appendc(link_command_line, clang_path);
			}
			link_command_line = gb_string_appendc(link_command_line, " -Wno-unused-command-line-argument ");

			if (build_context.lto_kind != LTO_None) {
				link_command_line = gb_string_appendc(link_command_line, " -flto=thin");
				link_command_line = gb_string_append_fmt(link_command_line, " -flto-jobs=%d ", build_context.thread_count);

				if (build_context.ODIN_DEBUG) {
					link_command_line = gb_string_appendc(link_command_line, " -g ");
				}

				if (is_osx && !build_context.minimum_os_version_string_given) {
					link_command_line = gb_string_appendc(link_command_line, " -Wno-override-module ");
				}
			}

			link_command_line = gb_string_appendc(link_command_line, object_files);
			link_command_line = gb_string_append_fmt(link_command_line, " -o \"%.*s\" ", LIT(output_filename));
			link_command_line = gb_string_append_fmt(link_command_line, " %s ", platform_lib_str);
			link_command_line = gb_string_append_fmt(link_command_line, " %s ", lib_str);
			link_command_line = gb_string_append_fmt(link_command_line, " %.*s ", LIT(build_context.link_flags));
			link_command_line = gb_string_append_fmt(link_command_line, " %.*s ", LIT(build_context.extra_linker_flags));
			link_command_line = gb_string_append_fmt(link_command_line, " %s ", link_settings);


			if (is_android) {
				TIME_SECTION("Linking");
			}

			if (build_context.linker_choice == Linker_lld) {
				link_command_line = gb_string_append_fmt(link_command_line, " -fuse-ld=lld");
				result = system_exec_command_line_app("lld-link", link_command_line);
			} else if (build_context.linker_choice == Linker_mold) {
				link_command_line = gb_string_append_fmt(link_command_line, " -fuse-ld=mold");
				result = system_exec_command_line_app("mold-link", link_command_line);
			} else {
				result = system_exec_command_line_app("ld-link", link_command_line);
			}

			if (result) {
				return result;
			}

			if (is_osx && build_context.ODIN_DEBUG) {
				// NOTE: macOS links DWARF symbols dynamically. Dsymutil will map the stubs in the exe
				// to the symbols in the object file
				result = system_exec_command_line_app("dsymutil", "dsymutil \"%.*s\"", LIT(output_filename));

				if (result) {
					return result;
				}
			}
		}
	}

	return result;
}
