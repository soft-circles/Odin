#include <llvm/ADT/ArrayRef.h>
#include <llvm/Support/SHA256.h>
#include "n64_toolchain_pins.hpp"

gb_internal i32 system_exec_command_line_app(char const *name, char const *fmt, ...);
gb_internal bool system_exec_command_line_app_output(char const *command, gbString *output);
#if !defined(GB_SYSTEM_WINDOWS)
#include <spawn.h>
extern char **environ;
int run_subprocess(char const *name, char const **args);
#endif

// This module's boundary is the two request/result operations below:
// n64_prepare_build validates a complete set of parsed build settings, and
// n64_package_rom packages already-compiled and flattened link inputs. Keep
// compiler globals and linker entities in the adapter in linker.cpp.
struct N64BuildSettings {
	String sdk_root;
	String title;
	String region;
	String save_type;
	String controllers[4];
	String assets;
	String metadata;
	bool rtc;
	bool show_system_calls;
	bool keep_temp_files;
};

struct N64PrepareBuildRequest {
	bool is_n64_target;
	bool n64_options_given;
	bool command_does_build;
	bool is_build_command;
	BuildModeKind build_mode;
	LTOKind lto_kind;
	RelocMode reloc_mode;
	bool no_crt;
	bool no_entry_point;
	LinkerChoice linker_choice;
	bool print_linker_flags;
	N64BuildSettings settings;
};

struct N64PrepareBuildResult {
	bool success;
	String sdk_root;
};

struct N64ForeignLibrary {
	String name;
	String extra_linker_flags;
	Slice<String> paths;
};

struct N64BuildRequest {
	N64BuildSettings settings;
	String output_filename;
	String output_name;
	String extra_linker_flags;
	Array<String> object_paths;
	Array<N64ForeignLibrary> foreign_libraries;
};

struct N64BuildResult {
	i32 exit_code;
	String intermediates_path;
};

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
		{STR_LIT("host"),     make_string_c(N64_EXPECTED_TOOLCHAIN_HOST)},
		{STR_LIT("binutils"), make_string_c(N64_EXPECTED_BINUTILS_VERSION)},
		{STR_LIT("gcc"),      make_string_c(N64_EXPECTED_GCC_VERSION)},
		{STR_LIT("newlib"),   make_string_c(N64_EXPECTED_NEWLIB_VERSION)},
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

gb_internal N64PrepareBuildResult n64_prepare_build(N64PrepareBuildRequest const &request) {
	if (!request.is_n64_target) {
		if (request.n64_options_given) {
			gb_printf_err("N64 build options may only be used with -target:n64\n");
			return {false, {}};
		}
		return {true, request.settings.sdk_root};
	}
	if (!request.command_does_build) {
		return {true, request.settings.sdk_root};
	}
	if (!request.is_build_command) {
		gb_printf_err("-target:n64 build outputs currently support the build command only\n");
		return {false, {}};
	}
	if (request.build_mode != BuildMode_Executable && request.n64_options_given) {
		gb_printf_err("N64 ROM configuration options require executable ROM output\n");
		return {false, {}};
	}
	switch (request.build_mode) {
	case BuildMode_Object:
	case BuildMode_Assembly:
	case BuildMode_LLVM_IR:
		return {true, request.settings.sdk_root};
	case BuildMode_StaticLibrary:
	case BuildMode_DynamicLibrary:
		gb_printf_err("-target:n64 currently supports executable ROM, object, assembly, and LLVM IR output\n");
		return {false, {}};
	case BuildMode_Executable:
		break;
	}
#if defined(GB_SYSTEM_WINDOWS)
	gb_printf_err("The integrated N64 ROM build currently requires a POSIX host\n");
	return {false, {}};
#endif
	if (request.lto_kind != LTO_None) {
		gb_printf_err("-target:n64 does not support LTO in the pinned libdragon build pipeline\n");
		return {false, {}};
	}
	if (request.reloc_mode != RelocMode_Static) {
		gb_printf_err("-target:n64 executable builds require -reloc-mode:static\n");
		return {false, {}};
	}
	if (request.no_crt || request.no_entry_point) {
		gb_printf_err("-target:n64 executable builds do not support -no-crt or -no-entry-point; pinned n64.mk owns startup\n");
		return {false, {}};
	}
	if (request.linker_choice != Linker_Default) {
		gb_printf_err("-target:n64 executable builds do not support -linker; pinned n64.mk selects the linker\n");
		return {false, {}};
	}
	if (request.print_linker_flags) {
		gb_printf_err("-print-linker-flags is not supported by the integrated N64 packaging pipeline; use -show-system-calls\n");
		return {false, {}};
	}
	if (request.settings.rtc &&
	    (request.settings.save_type == STR_LIT("eeprom4k") ||
	     request.settings.save_type == STR_LIT("eeprom16k"))) {
		gb_printf_err("-n64-rtc cannot be combined with -n64-save-type:%.*s; the pinned N64 header format cannot use RTC with EEPROM\n",
		              LIT(request.settings.save_type));
		return {false, {}};
	}
#if !defined(GB_SYSTEM_WINDOWS)
	if (!n64_sdk_tool_is_executable(STR_LIT("/usr/bin/make"))) {
		gb_printf_err("GNU make is required at /usr/bin/make for the integrated N64 build\n");
		return {false, {}};
	}
#endif

	String sdk_root = request.settings.sdk_root;
	if (sdk_root.len == 0) {
		gb_printf_err("N64 SDK is not configured; use -n64-inst:<path> or set the N64_INST environment variable\n");
		return {false, {}};
	}
	if (!n64_validate_sdk_root(sdk_root)) {
		return {false, {}};
	}
	if (request.settings.assets.len > 0) {
		String tool = n64_path_join(temporary_allocator(), sdk_root, STR_LIT("bin/mkdfs"));
		if (!n64_sdk_tool_is_executable(tool)) {
			gb_printf_err("-n64-assets requires the executable N64 SDK tool bin/mkdfs in %.*s\n", LIT(sdk_root));
			return {false, {}};
		}
	}
	if (request.settings.metadata.len > 0) {
		String tool = n64_path_join(temporary_allocator(), sdk_root, STR_LIT("bin/n64metadata"));
		if (!n64_sdk_tool_is_executable(tool)) {
			gb_printf_err("-n64-metadata requires the executable N64 SDK tool bin/n64metadata in %.*s\n", LIT(sdk_root));
			return {false, {}};
		}
	}
	return {true, sdk_root};
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

gb_internal bool n64_init_build_stage(
	N64BuildStage *stage,
	N64BuildSettings const &settings,
	String const &output_filename
) {
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

	if (!n64_create_staging_link(settings.sdk_root, stage->sdk_link_path)) {
		gb_printf_err("Failed to create N64 SDK staging link %.*s -> %.*s\n",
		              LIT(stage->sdk_link_path), LIT(settings.sdk_root));
		if (!n64_remove_directory(stage->work_dir)) {
			gb_printf_err("N64 build failed; intermediates were retained at %.*s\n", LIT(stage->work_dir));
		}
		return false;
	}
	if (settings.assets.len > 0 &&
	    !n64_create_staging_link(settings.assets, stage->assets_link_path)) {
		gb_printf_err("Failed to stage N64 asset directory %.*s: %s\n", LIT(settings.assets), strerror(errno));
		gb_printf_err("N64 build failed; intermediates were retained at %.*s\n", LIT(stage->work_dir));
		return false;
	}
	if (settings.metadata.len > 0 &&
	    !n64_stage_metadata_directory(stage, settings.metadata)) {
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

gb_internal bool n64_stage_link_inputs(N64BuildStage *stage, N64BuildRequest *request) {
	// LLVM modules are collected from a hash map, so their completion/list order
	// is not stable between compiler processes. Link in path order to keep an
	// unchanged N64 application's ELF, symbols, compressed payload, and ROM
	// byte-for-byte reproducible.
	array_sort(request->object_paths, string_cmp);

	isize odin_index = 0;
	for (String const &object_path : request->object_paths) {
		if (!n64_stage_input(stage, object_path, STR_LIT("odin"), odin_index, STR_LIT("o"))) {
			return false;
		}
		odin_index += 1;
	}

	isize foreign_index = 0;
	for (N64ForeignLibrary const &library : request->foreign_libraries) {
		String extra_flags = string_trim_whitespace(library.extra_linker_flags);
		if (extra_flags.len > 0) {
			gb_printf_err("N64 foreign import '%.*s' uses unsupported extra linker flags: %.*s\n",
			              LIT(library.name), LIT(extra_flags));
			return false;
		}
		for (String const &path : library.paths) {
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

gb_internal bool n64_write_makefile(N64BuildStage const &stage, N64BuildRequest const &request) {
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

	N64BuildSettings const &settings = request.settings;
	String title = settings.title.len > 0
		? settings.title
		: n64_sanitized_rom_title(request.output_name);
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
		LIT(settings.region),
		LIT(settings.save_type),
		settings.rtc ? "1" : "",
		LIT(settings.controllers[0]),
		LIT(settings.controllers[1]),
		LIT(settings.controllers[2]),
		LIT(settings.controllers[3]),
		LIT(stage.metadata_make_path),
		settings.assets.len > 0 ? "assets" : "filesystem",
		settings.show_system_calls ? "1" : "");
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
	if (settings.assets.len > 0) {
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

gb_internal i32 n64_run_make(N64BuildStage const &stage, bool show_system_calls) {
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
	if (show_system_calls) {
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

gb_internal bool n64_cleanup_successful_stage(N64BuildStage const &stage, bool has_metadata) {
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
	if (has_metadata) {
		clean = n64_remove_directory(stage.metadata_dir_path) && clean;
	}
	clean = n64_remove_directory(stage.build_dir) && clean;
	clean = n64_remove_directory(stage.work_dir) && clean;
	return clean;
}

gb_internal N64BuildResult n64_package_rom(N64BuildRequest request) {
	String extra_flags = string_trim_whitespace(request.extra_linker_flags);
	if (extra_flags.len > 0) {
		gb_printf_err("-extra-linker-flags is not supported by the pinned N64 packaging pipeline\n");
		return {1, {}};
	}

	N64BuildStage stage = {};
	if (!n64_init_build_stage(&stage, request.settings, request.output_filename)) {
		return {1, stage.work_dir};
	}
	gb_printf_err("N64 build intermediates: %.*s\n", LIT(stage.work_dir));
	if (!n64_stage_link_inputs(&stage, &request) || !n64_write_makefile(stage, request)) {
		gb_printf_err("N64 build failed; intermediates were retained at %.*s\n", LIT(stage.work_dir));
		return {1, stage.work_dir};
	}

	i32 result = n64_run_make(stage, request.settings.show_system_calls);
	if (result != 0) {
		gb_printf_err("N64 build failed; intermediates were retained at %.*s\n", LIT(stage.work_dir));
		return {result, stage.work_dir};
	}

	char const *staged_rom = alloc_cstring(temporary_allocator(), stage.staged_rom_path);
	char const *final_rom = alloc_cstring(temporary_allocator(), request.output_filename);
	if (rename(staged_rom, final_rom) != 0) {
		gb_printf_err("Failed to atomically place N64 ROM at %.*s: %s\n", LIT(request.output_filename), strerror(errno));
		gb_printf_err("N64 build intermediates were retained at %.*s\n", LIT(stage.work_dir));
		return {1, stage.work_dir};
	}

	if (request.settings.keep_temp_files) {
		gb_printf_err("Retained N64 build intermediates: %.*s\n", LIT(stage.work_dir));
	} else {
		if (!n64_cleanup_successful_stage(stage, request.settings.metadata.len > 0)) {
			gb_printf_err("Warning: could not completely remove N64 build intermediates at %.*s\n", LIT(stage.work_dir));
		}
	}
	return {0, request.settings.keep_temp_files ? stage.work_dir : String{}};
}
