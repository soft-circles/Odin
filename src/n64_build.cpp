#if !defined(GB_SYSTEM_WINDOWS)
#include <spawn.h>
extern char **environ;
#endif

// This module's boundary is the two request/result operations below:
// n64_prepare_build validates a complete set of parsed build settings, and
// n64_package_rom packages already-compiled and flattened link inputs. Keep
// compiler globals and linker entities in the adapter in linker.cpp.
// N64BuildSettings itself lives in n64_build.hpp so BuildContext can hold it.
struct N64PrepareBuildRequest {
	bool is_n64_target;
	bool n64_options_given;
	bool rom_options_given;
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

struct N64ForeignLibrary {
	String name;
	String extra_linker_flags;
	Slice<String> paths;
};

struct N64BuildRequest {
	N64BuildSettings settings;
	bool show_system_calls;
	bool keep_temp_files;
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

// A/B switch for the B1 spike: ODIN_N64_USE_MAKE=1 selects the old
// generated-Makefile path; the default spawns the packaging tools directly.
gb_internal bool n64_use_make(void) {
	char const *value = gb_get_env("ODIN_N64_USE_MAKE", temporary_allocator());
	return value != nullptr && value[0] == '1';
}

gb_internal bool n64_validate_sdk_root(String const &sdk_root) {
	bool use_make = n64_use_make();
	Array<String> required_files = {};
	array_init(&required_files, temporary_allocator());
	array_add(&required_files, STR_LIT("mips64-elf/lib/libdragon.a"));
	array_add(&required_files, STR_LIT("mips64-elf/lib/libdragonsys.a"));
	array_add(&required_files, STR_LIT("mips64-elf/lib/n64.ld"));
	// Tools spawned by the direct path, plus objdump and addr2line, which n64sym
	// runs through $N64_INST/bin.
	Array<String> required_tools = {};
	array_init(&required_tools, temporary_allocator());
	array_add(&required_tools, STR_LIT("bin/ed64romconfig"));
	array_add(&required_tools, STR_LIT("bin/mips64-elf-g++"));
	array_add(&required_tools, STR_LIT("bin/mips64-elf-addr2line"));
	array_add(&required_tools, STR_LIT("bin/mips64-elf-objdump"));
	array_add(&required_tools, STR_LIT("bin/mips64-elf-strip"));
	array_add(&required_tools, STR_LIT("bin/n64elfcompress"));
	array_add(&required_tools, STR_LIT("bin/n64sym"));
	array_add(&required_tools, STR_LIT("bin/n64tool"));
	if (use_make) {
		array_add(&required_files, STR_LIT("include/n64.mk"));
		array_add(&required_tools, STR_LIT("bin/mips64-elf-gcc"));
		array_add(&required_tools, STR_LIT("bin/mips64-elf-size"));
	}

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
	return valid;
}

gb_internal bool n64_prepare_build(N64PrepareBuildRequest const &request) {
	if (!request.is_n64_target) {
		if (request.n64_options_given) {
			gb_printf_err("N64 build options may only be used with -target:n64\n");
			return false;
		}
		return true;
	}
	if (!request.command_does_build) {
		return true;
	}
	if (!request.is_build_command) {
		gb_printf_err("-target:n64 build outputs currently support the build command only\n");
		return false;
	}
	if (request.build_mode != BuildMode_Executable && request.rom_options_given) {
		gb_printf_err("N64 ROM configuration options require executable ROM output\n");
		return false;
	}
	switch (request.build_mode) {
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
#else
	if (request.lto_kind != LTO_None) {
		gb_printf_err("-target:n64 does not support LTO in the libdragon build pipeline\n");
		return false;
	}
	if (request.reloc_mode != RelocMode_Static) {
		gb_printf_err("-target:n64 executable builds require -reloc-mode:static\n");
		return false;
	}
	if (request.no_crt || request.no_entry_point) {
		gb_printf_err("-target:n64 executable builds do not support -no-crt or -no-entry-point; the selected n64.mk owns startup\n");
		return false;
	}
	if (request.linker_choice != Linker_Default) {
		gb_printf_err("-target:n64 executable builds do not support -linker; the selected n64.mk selects the linker\n");
		return false;
	}
	if (request.print_linker_flags) {
		gb_printf_err("-print-linker-flags is not supported by the integrated N64 packaging pipeline; use -show-system-calls\n");
		return false;
	}
	if (request.settings.rtc &&
	    (request.settings.save_type == STR_LIT("eeprom4k") ||
	     request.settings.save_type == STR_LIT("eeprom16k"))) {
		gb_printf_err("-n64-rtc cannot be combined with -n64-save-type:%.*s; the N64 header format cannot use RTC with EEPROM\n",
		              LIT(request.settings.save_type));
		return false;
	}
	if (n64_use_make() && !n64_sdk_tool_is_executable(STR_LIT("/usr/bin/make"))) {
		gb_printf_err("GNU make is required at /usr/bin/make for the integrated N64 build\n");
		return false;
	}

	String sdk_root = request.settings.sdk_root;
	if (sdk_root.len == 0) {
		gb_printf_err("N64 SDK is not configured; use -n64-inst:<path>, set the N64_INST environment variable, or install the SDK at <odin root>/n64\n");
		return false;
	}
	if (!n64_validate_sdk_root(sdk_root)) {
		return false;
	}
	if (request.settings.assets.len > 0) {
		String tool = n64_path_join(temporary_allocator(), sdk_root, STR_LIT("bin/mkdfs"));
		if (!n64_sdk_tool_is_executable(tool)) {
			gb_printf_err("-n64-assets requires the executable N64 SDK tool bin/mkdfs in %.*s\n", LIT(sdk_root));
			return false;
		}
	}
	if (request.settings.metadata.len > 0) {
		String tool = n64_path_join(temporary_allocator(), sdk_root, STR_LIT("bin/n64metadata"));
		if (!n64_sdk_tool_is_executable(tool)) {
			gb_printf_err("-n64-metadata requires the executable N64 SDK tool bin/n64metadata in %.*s\n", LIT(sdk_root));
			return false;
		}
	}
	return true;
#endif
}

#if !defined(GB_SYSTEM_WINDOWS)
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
	return rmdir(alloc_cstring(temporary_allocator(), path)) == 0;
}

gb_internal bool n64_create_staging_link(String const &source_path, String const &link_path) {
	return symlink(
		alloc_cstring(temporary_allocator(), source_path),
		alloc_cstring(temporary_allocator(), link_path)
	) == 0;
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
	char *stage_template_c = alloc_cstring(temporary_allocator(), stage_template);
	char *created_work_dir = mkdtemp(stage_template_c);
	if (created_work_dir == nullptr) {
		gb_printf_err("Failed to create isolated N64 build directory beside %.*s: %s\n",
		              LIT(output_filename), strerror(errno));
		return false;
	}
	stage->work_dir = copy_string(allocator, make_string_c(created_work_dir));
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
		if (n64_remove_directory(stage->work_dir)) {
			stage->work_dir = {};
		}
		return false;
	}
	if (settings.assets.len > 0 &&
	    !n64_create_staging_link(settings.assets, stage->assets_link_path)) {
		gb_printf_err("Failed to stage N64 asset directory %.*s: %s\n", LIT(settings.assets), strerror(errno));
		return false;
	}
	return settings.metadata.len == 0 || n64_stage_metadata_directory(stage, settings.metadata);
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

gb_internal bool n64_foreign_input_is_sdk_library(String const &input) {
	return input == "c" || input == "m" || input == "dragon" || input == "dragonsys";
}

// Pure validation, run before any staging directory exists so a rejected
// request leaves nothing behind in the output directory.
gb_internal bool n64_validate_link_inputs(N64BuildRequest const &request, bool allow_extra_linker_flags) {
	String extra_flags = string_trim_whitespace(request.extra_linker_flags);
	if (extra_flags.len > 0 && !allow_extra_linker_flags) {
		gb_printf_err("-extra-linker-flags is not supported by the N64 packaging pipeline\n");
		return false;
	}
	for (N64ForeignLibrary const &library : request.foreign_libraries) {
		String library_flags = string_trim_whitespace(library.extra_linker_flags);
		if (library_flags.len > 0 && !allow_extra_linker_flags) {
			gb_printf_err("N64 foreign import '%.*s' uses unsupported extra linker flags: %.*s\n",
			              LIT(library.name), LIT(library_flags));
			return false;
		}
		for (String const &path : library.paths) {
			String input = string_trim_whitespace(path);
			if (n64_foreign_input_is_sdk_library(input)) {
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
		}
	}
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
		for (String const &path : library.paths) {
			String input = string_trim_whitespace(path);
			if (n64_foreign_input_is_sdk_library(input)) {
				continue;
			}
			String staged_extension = str_eq_ignore_case(path_extension(input, false), STR_LIT("o")) ? STR_LIT("o") : STR_LIT("a");
			if (!n64_stage_input(stage, input, STR_LIT("foreign"), foreign_index, staged_extension)) {
				return false;
			}
			foreign_index += 1;
		}
	}
	return true;
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
		request.show_system_calls ? "1" : "");
	if (has_metadata) {
		// The selected n64.mk requests --padding 0 to defer final padding to
		// n64metadata, while n64tool requires a unit suffix for
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

gb_internal bool n64_environment_key_is_filtered(char const *entry) {
	char const *equals = strchr(entry, '=');
	if (equals == nullptr) {
		return true;
	}
	isize key_length = equals-entry;
	char const *exact_keys[] = {
		"PATH", "SHELL", "MAKEFLAGS", "MFLAGS", "GNUMAKEFLAGS", "MAKEOVERRIDES", "MAKELEVEL",
		"CCACHE", "V", "D", "CFLAGS", "CXXFLAGS", "ASFLAGS", "LDFLAGS",
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

// extra_entry, when given, is appended after filtering (the direct path
// passes N64_INST=<sdk> because n64sym finds objdump/addr2line through it).
gb_internal char **n64_sanitized_environment(char const *extra_entry = nullptr) {
	isize source_count = 0;
	while (environ[source_count] != nullptr) {
		source_count += 1;
	}
	char **environment = gb_alloc_array(temporary_allocator(), char *, source_count+4);
	isize destination_count = 0;
	for (isize index = 0; index < source_count; index += 1) {
		if (!n64_environment_key_is_filtered(environ[index])) {
			environment[destination_count++] = environ[index];
		}
	}
	environment[destination_count++] = cast(char *)"PATH=/usr/bin:/bin:/usr/sbin:/sbin";
	environment[destination_count++] = cast(char *)"SHELL=/bin/sh";
	if (extra_entry != nullptr) {
		environment[destination_count++] = cast(char *)extra_entry;
	}
	environment[destination_count] = nullptr;
	return environment;
}

// Spawns argv[0] with the given environment and waits. discard_stdout mirrors
// n64.mk's `>/dev/null` on mkdfs.
gb_internal i32 n64_spawn_and_wait(char const *const *arguments, char **environment, bool discard_stdout) {
	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	if (discard_stdout) {
		posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
	}
	pid_t pid = 0;
	int status = posix_spawn(&pid, arguments[0], &actions, nullptr, cast(char *const *)arguments, environment);
	posix_spawn_file_actions_destroy(&actions);
	if (status != 0) {
		gb_printf_err("Could not spawn N64 packaging subprocess %s: %s\n", arguments[0], strerror(status));
		return -1;
	}
	for (;;) {
		if (waitpid(pid, &status, 0) < 0) {
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
}

gb_internal i32 n64_run_make(N64BuildStage const &stage, bool show_system_calls) {
	char const *make_path = "/usr/bin/make"; // existence checked in n64_prepare_build
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
	return n64_spawn_and_wait(arguments, n64_sanitized_environment(), false);
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

gb_internal N64BuildResult n64_package_rom_with_make(N64BuildRequest request) {
	if (!n64_validate_link_inputs(request, false)) {
		return {1, {}};
	}

	N64BuildStage stage = {};
	i32 result = 1;
	if (n64_init_build_stage(&stage, request.settings, request.output_filename)) {
		gb_printf_err("N64 build intermediates: %.*s\n", LIT(stage.work_dir));
		if (n64_stage_link_inputs(&stage, &request) && n64_write_makefile(stage, request)) {
			result = n64_run_make(stage, request.show_system_calls);
		}
		if (result == 0) {
			char const *staged_rom = alloc_cstring(temporary_allocator(), stage.staged_rom_path);
			char const *final_rom = alloc_cstring(temporary_allocator(), request.output_filename);
			if (rename(staged_rom, final_rom) != 0) {
				gb_printf_err("Failed to atomically place N64 ROM at %.*s: %s\n", LIT(request.output_filename), strerror(errno));
				result = 1;
			}
		}
	}
	if (result != 0) {
		if (stage.work_dir.len > 0) {
			gb_printf_err("N64 build failed; intermediates were retained at %.*s\n", LIT(stage.work_dir));
		}
		return {result, stage.work_dir};
	}

	if (request.keep_temp_files) {
		gb_printf_err("Retained N64 build intermediates: %.*s\n", LIT(stage.work_dir));
	} else if (!n64_cleanup_successful_stage(stage, request.settings.metadata.len > 0)) {
		gb_printf_err("Warning: could not completely remove N64 build intermediates at %.*s\n", LIT(stage.work_dir));
	}
	return {0, request.keep_temp_files ? stage.work_dir : String{}};
}

// ---- Direct packaging -------------------------------------------------------
// Runs the commands libdragon's n64.mk runs for an `odin-n64.z64` goal
// (n64.mk:135-163, :221-240) without make. Intermediates keep n64.mk's
// basenames because `n64tool --toc` records each input's basename in the ROM.

struct N64Intermediates {
	String dir;
	String elf;
	String map;
	String sym;
	String stripped;
	String dfs;
	String rom_tmp;
};

typedef Array<char const *> N64Argv;

gb_internal void n64_arg(N64Argv *argv, String const &value) {
	array_add(argv, cast(char const *)alloc_cstring(permanent_allocator(), value));
}

gb_internal void n64_arg(N64Argv *argv, char const *value) {
	array_add(argv, value);
}

gb_internal String n64_sdk_path(N64BuildSettings const &settings, char const *relative_path) {
	return n64_path_join(permanent_allocator(), settings.sdk_root, make_string_c(relative_path));
}

// -extra-linker-flags reaches the link driver as whitespace-separated words;
// unlike the shell-based linkers for other targets there is no quoting.
gb_internal void n64_add_words(N64Argv *argv, String const &words) {
	isize start = -1;
	for (isize index = 0; index <= words.len; index += 1) {
		bool space = index == words.len || gb_char_is_space(cast(char)words[index]);
		if (!space && start < 0) {
			start = index;
		} else if (space && start >= 0) {
			n64_arg(argv, substring(words, start, index));
			start = -1;
		}
	}
}

gb_internal i32 n64_run_tool(char const *label, N64Argv argv, char **environment, bool show_system_calls, bool discard_stdout = false) {
	if (show_system_calls) {
		gb_printf_err("[SYSTEM CALL] n64-%s\n", label);
		for (char const *argument : argv) {
			gb_printf_err("%s ", argument);
		}
		gb_printf_err("\n\n");
	}
	array_add(&argv, cast(char const *)nullptr);
	i32 result = n64_spawn_and_wait(argv.data, environment, discard_stdout);
	if (result != 0) {
		gb_printf_err("N64 packaging step '%s' failed with exit code %d\n", label, result);
	}
	return result;
}

// One fixed directory beside the output, named after it: rebuilding the same
// output reuses it, so the ELF for a debugger is always at a known path.
gb_internal bool n64_init_intermediates(N64Intermediates *out, String const &output_filename, String const &output_name) {
	gbAllocator a = permanent_allocator();
	String directory = directory_from_path(output_filename);
	out->dir      = n64_path_join(a, directory, concatenate_strings(a, output_name, STR_LIT(".n64-build")));
	out->elf      = n64_path_join(a, out->dir, STR_LIT("odin-n64.elf"));
	out->map      = n64_path_join(a, out->dir, STR_LIT("odin-n64.map"));
	out->sym      = n64_path_join(a, out->dir, STR_LIT("odin-n64.elf.sym"));
	out->stripped = n64_path_join(a, out->dir, STR_LIT("odin-n64.elf.stripped"));
	out->dfs      = n64_path_join(a, out->dir, STR_LIT("odin-n64.dfs"));
	out->rom_tmp  = n64_path_join(a, out->dir, STR_LIT("odin-n64.z64.tmp"));
	char const *dir_c = alloc_cstring(temporary_allocator(), out->dir);
	if (mkdir(dir_c, 0755) != 0 && errno != EEXIST) {
		gb_printf_err("Failed to create N64 build directory %.*s: %s\n", LIT(out->dir), strerror(errno));
		return false;
	}
	return true;
}

gb_internal i32 n64_link_elf(N64BuildRequest *request, N64Intermediates const &files, char **env) {
	N64BuildSettings const &s = request->settings;
	// See n64_stage_link_inputs: hash-map order is not stable between runs.
	array_sort(request->object_paths, string_cmp);

	N64Argv argv = {};
	array_init(&argv, permanent_allocator());
	n64_arg(&argv, n64_sdk_path(s, "bin/mips64-elf-g++"));
	// N64_C_AND_CXX_FLAGS verbatim (n64.mk:69-75); n64.mk passes them to the link too.
	n64_arg(&argv, "-march=vr4300"); n64_arg(&argv, "-mtune=vr4300"); n64_arg(&argv, "-mabi=o64");
	n64_arg(&argv, concatenate_strings(permanent_allocator(), STR_LIT("-I"), n64_sdk_path(s, "mips64-elf/include/newlib_overrides")));
	n64_arg(&argv, concatenate_strings(permanent_allocator(), STR_LIT("-I"), n64_sdk_path(s, "mips64-elf/include")));
	char const *flags[] = {
		"-include", "ktls.h", "-falign-functions=32", "-ffunction-sections", "-fdata-sections", "-g",
		"-ffile-prefix-map=.=.", "-ffast-math", "-ftrapping-math", "-fno-associative-math", "-DN64", "-O2",
		"-Wall", "-Werror", "-Wno-error=deprecated-declarations", "-fdiagnostics-color=always",
		"-Wno-error=unused-variable", "-Wno-error=unused-but-set-variable", "-Wno-error=unused-function",
		"-Wno-error=unused-parameter", "-Wno-error=unused-but-set-parameter", "-Wno-error=unused-label",
		"-Wno-error=unused-local-typedefs", "-Wno-error=unused-const-variable", "-ftrivial-auto-var-init=pattern",
	};
	for (char const *flag : flags) {
		n64_arg(&argv, flag);
	}
	n64_arg(&argv, "-o"); n64_arg(&argv, files.elf);
	for (String const &object : request->object_paths) {
		n64_arg(&argv, object);
	}
	for (N64ForeignLibrary const &library : request->foreign_libraries) {
		for (String const &path : library.paths) {
			String input = string_trim_whitespace(path);
			if (!n64_foreign_input_is_sdk_library(input)) {
				n64_arg(&argv, input);
			}
		}
	}
	n64_arg(&argv, "-lc"); n64_arg(&argv, "-mabi=o64");
	// N64_LDFLAGS (n64.mk:83), each word prefixed with -Wl, as the %.elf rule does.
	n64_arg(&argv, "-Wl,-g");
	n64_arg(&argv, concatenate_strings(permanent_allocator(), STR_LIT("-Wl,-L"), n64_sdk_path(s, "mips64-elf/lib")));
	char const *ldflags[] = {"-Wl,-ldragon", "-Wl,-lm", "-Wl,-ldragonsys", "-Wl,-Tn64.ld", "-Wl,--gc-sections", "-Wl,--wrap", "-Wl,__do_global_ctors"};
	for (char const *flag : ldflags) {
		n64_arg(&argv, flag);
	}
	n64_arg(&argv, concatenate3_strings(permanent_allocator(), STR_LIT("-Wl,-Map="), files.map, STR_LIT(",--cref")));
	n64_add_words(&argv, request->extra_linker_flags);
	for (N64ForeignLibrary const &library : request->foreign_libraries) {
		n64_add_words(&argv, library.extra_linker_flags);
	}
	return n64_run_tool("link", argv, env, request->show_system_calls);
}

gb_internal i32 n64_build_rom_from_elf(N64BuildRequest const &request, N64Intermediates const &files, char **env) {
	N64BuildSettings const &s = request.settings;
	bool show = request.show_system_calls;
	gbAllocator a = permanent_allocator();
	i32 result = 0;
	N64Argv argv = {};

	// n64sym --all elf elf.sym
	array_init(&argv, a);
	n64_arg(&argv, n64_sdk_path(s, "bin/n64sym")); n64_arg(&argv, "--all");
	n64_arg(&argv, files.elf); n64_arg(&argv, files.sym);
	if ((result = n64_run_tool("sym", argv, env, show)) != 0) return result;

	// cp elf elf.stripped; strip -s elf.stripped
	if (!gb_file_copy(alloc_cstring(a, files.elf), alloc_cstring(a, files.stripped), false)) {
		gb_printf_err("Failed to copy %.*s to %.*s\n", LIT(files.elf), LIT(files.stripped));
		return 1;
	}
	array_init(&argv, a);
	n64_arg(&argv, n64_sdk_path(s, "bin/mips64-elf-strip")); n64_arg(&argv, "-s"); n64_arg(&argv, files.stripped);
	if ((result = n64_run_tool("strip", argv, env, show)) != 0) return result;

	// n64elfcompress -o <dir>/ -c 1 elf.stripped   (rewrites the file in place)
	array_init(&argv, a);
	n64_arg(&argv, n64_sdk_path(s, "bin/n64elfcompress"));
	n64_arg(&argv, "-o"); n64_arg(&argv, concatenate_strings(a, files.dir, STR_LIT("/")));
	n64_arg(&argv, "-c"); n64_arg(&argv, "1"); n64_arg(&argv, files.stripped);
	if ((result = n64_run_tool("elfcompress", argv, env, show)) != 0) return result;

	// mkdfs dfs <assets> >/dev/null
	if (s.assets.len > 0) {
		array_init(&argv, a);
		n64_arg(&argv, n64_sdk_path(s, "bin/mkdfs")); n64_arg(&argv, files.dfs); n64_arg(&argv, s.assets);
		if ((result = n64_run_tool("mkdfs", argv, env, show, true)) != 0) return result;
	}

	// n64tool N64_TOOLFLAGS --output z64.tmp --align 256 stripped sym [dfs] N64_TOOLFILES
	String title = s.title.len > 0 ? s.title : n64_sanitized_rom_title(request.output_name);
	n64_remove_file_if_present(files.rom_tmp);
	array_init(&argv, a);
	n64_arg(&argv, n64_sdk_path(s, "bin/n64tool"));
	n64_arg(&argv, "--toc"); n64_arg(&argv, "--title"); n64_arg(&argv, title);
	n64_arg(&argv, "--category"); n64_arg(&argv, "N");
	if (s.region.len > 0) { n64_arg(&argv, "--region"); n64_arg(&argv, s.region); }
	if (s.metadata.len > 0) {
		// n64.mk requests --padding 0 so n64metadata pads; n64tool needs a unit suffix.
		n64_arg(&argv, "--padding"); n64_arg(&argv, "0B");
	}
	n64_arg(&argv, "--output"); n64_arg(&argv, files.rom_tmp);
	n64_arg(&argv, "--align"); n64_arg(&argv, "256"); n64_arg(&argv, files.stripped);
	n64_arg(&argv, files.sym);
	if (s.assets.len > 0) {
		n64_arg(&argv, files.dfs);
	}
	// N64_TOOLFILES = $(wildcard <sdk>/mips64-elf/include/*.version), sorted like make's glob.
	Array<FileInfo> include_files = {};
	Array<String> version_files = {};
	array_init(&version_files, a);
	if (read_directory(n64_sdk_path(s, "mips64-elf/include"), &include_files) == ReadDirectory_None) {
		for (FileInfo const &file : include_files) {
			if (!file.is_dir && string_ends_with(file.name, STR_LIT(".version"))) {
				array_add(&version_files, file.fullpath);
			}
		}
	}
	array_sort(version_files, string_cmp);
	for (String const &path : version_files) {
		n64_arg(&argv, path);
	}
	if ((result = n64_run_tool("n64tool", argv, env, show)) != 0) return result;

	// ed64romconfig N64_ED64ROMCONFIGFLAGS z64.tmp (always runs: --regionfree is a default)
	array_init(&argv, a);
	n64_arg(&argv, n64_sdk_path(s, "bin/ed64romconfig"));
	if (s.save_type.len > 0) { n64_arg(&argv, "--savetype"); n64_arg(&argv, s.save_type); }
	if (s.rtc) { n64_arg(&argv, "--rtc"); }
	n64_arg(&argv, "--regionfree");
	char const *controller_flags[4] = {"--controller1", "--controller2", "--controller3", "--controller4"};
	for (isize index = 0; index < 4; index += 1) {
		if (s.controllers[index].len > 0) {
			n64_arg(&argv, controller_flags[index]); n64_arg(&argv, s.controllers[index]);
		}
	}
	n64_arg(&argv, files.rom_tmp);
	if ((result = n64_run_tool("ed64romconfig", argv, env, show)) != 0) return result;

	// n64metadata [-v] z64.tmp ini   (companion files resolve against the INI's own directory)
	if (s.metadata.len > 0) {
		array_init(&argv, a);
		n64_arg(&argv, n64_sdk_path(s, "bin/n64metadata"));
		if (show) { n64_arg(&argv, "-v"); }
		n64_arg(&argv, files.rom_tmp); n64_arg(&argv, s.metadata);
		if ((result = n64_run_tool("n64metadata", argv, env, show)) != 0) return result;
	}
	return 0;
}

gb_internal N64BuildResult n64_package_rom_direct(N64BuildRequest request) {
	if (!n64_validate_link_inputs(request, true)) {
		return {1, {}};
	}
	N64Intermediates files = {};
	if (!n64_init_intermediates(&files, request.output_filename, request.output_name)) {
		return {1, {}};
	}
	char const *sdk_entry = alloc_cstring(permanent_allocator(),
		concatenate_strings(permanent_allocator(), STR_LIT("N64_INST="), request.settings.sdk_root));
	char **env = n64_sanitized_environment(sdk_entry);

	i32 result = n64_link_elf(&request, files, env);
	if (result == 0) {
		result = n64_build_rom_from_elf(request, files, env);
	}
	if (result == 0 && rename(alloc_cstring(temporary_allocator(), files.rom_tmp),
	                          alloc_cstring(temporary_allocator(), request.output_filename)) != 0) {
		gb_printf_err("Failed to place N64 ROM at %.*s: %s\n", LIT(request.output_filename), strerror(errno));
		result = 1;
	}
	if (result != 0) {
		gb_printf_err("N64 build failed; intermediates are in %.*s\n", LIT(files.dir));
		return {result, files.dir};
	}
	if (request.keep_temp_files) {
		gb_printf_err("Retained N64 build intermediates: %.*s\n", LIT(files.dir));
		return {0, files.dir};
	}
	String generated[] = {files.elf, files.map, files.sym, files.stripped, files.dfs};
	bool clean = true;
	for (String const &path : generated) {
		clean = n64_remove_file_if_present(path) && clean;
	}
	if (!clean || !n64_remove_directory(files.dir)) {
		gb_printf_err("Warning: could not completely remove N64 build intermediates at %.*s\n", LIT(files.dir));
	}
	return {0, {}};
}

gb_internal N64BuildResult n64_package_rom(N64BuildRequest request) {
	return n64_use_make() ? n64_package_rom_with_make(request) : n64_package_rom_direct(request);
}
#else
gb_internal N64BuildResult n64_package_rom(N64BuildRequest request) {
	// ponytail: Windows hosts are rejected in n64_prepare_build; this stub only
	// keeps the linker adapter compiling until a Windows packaging path exists.
	gb_printf_err("The integrated N64 ROM build currently requires a POSIX host\n");
	return {1, {}};
}
#endif
