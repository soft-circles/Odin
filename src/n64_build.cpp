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

gb_internal bool n64_validate_sdk_root(String const &sdk_root) {
	Array<String> required_files = {};
	array_init(&required_files, temporary_allocator());
	array_add(&required_files, STR_LIT("mips64-elf/lib/libdragon.a"));
	array_add(&required_files, STR_LIT("mips64-elf/lib/libdragonsys.a"));
	array_add(&required_files, STR_LIT("mips64-elf/lib/n64.ld"));
	// Tools spawned during packaging, plus objdump, which n64sym runs through
	// $N64_INST/bin. n64sym also runs addr2line; that one stays unchecked, as
	// before, because the test SDK facades do not provide it.
	Array<String> required_tools = {};
	array_init(&required_tools, temporary_allocator());
	array_add(&required_tools, STR_LIT("bin/ed64romconfig"));
	array_add(&required_tools, STR_LIT("bin/mips64-elf-g++"));
	array_add(&required_tools, STR_LIT("bin/mips64-elf-objdump"));
	array_add(&required_tools, STR_LIT("bin/mips64-elf-strip"));
	array_add(&required_tools, STR_LIT("bin/n64elfcompress"));
	array_add(&required_tools, STR_LIT("bin/n64sym"));
	array_add(&required_tools, STR_LIT("bin/n64tool"));

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
		gb_printf_err("-target:n64 executable builds do not support -no-crt or -no-entry-point; libdragon's crt0 and n64.ld own startup\n");
		return false;
	}
	if (request.linker_choice != Linker_Default) {
		gb_printf_err("-target:n64 executable builds do not support -linker; the SDK's mips64-elf-g++ driver links the ROM\n");
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
gb_internal bool n64_remove_directory(String const &path) {
	return rmdir(alloc_cstring(temporary_allocator(), path)) == 0;
}

gb_internal bool n64_create_symlink(String const &source_path, String const &link_path) {
	return symlink(
		alloc_cstring(temporary_allocator(), source_path),
		alloc_cstring(temporary_allocator(), link_path)
	) == 0;
}

gb_internal bool n64_foreign_input_is_sdk_library(String const &input) {
	return input == "c" || input == "m" || input == "dragon" || input == "dragonsys";
}

// Pure validation, run before the intermediates directory exists so a
// rejected request leaves nothing behind in the output directory.
gb_internal bool n64_validate_link_inputs(N64BuildRequest const &request) {
	for (N64ForeignLibrary const &library : request.foreign_libraries) {
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

gb_internal bool n64_environment_key_is_filtered(char const *entry) {
	char const *equals = strchr(entry, '=');
	if (equals == nullptr) {
		return true;
	}
	isize key_length = equals-entry;
	char const *exact_keys[] = {
		"PATH", "SHELL",
	};
	for (char const *key : exact_keys) {
		isize length = cast(isize)strlen(key);
		if (key_length == length && memcmp(entry, key, length) == 0) {
			return true;
		}
	}
	return key_length >= 4 && memcmp(entry, "N64_", 4) == 0;
}

// extra_entry, when given, is appended after filtering. n64sym gets
// N64_INST=<sdk> this way because it finds objdump and addr2line through it.
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
// n64.mk's `>/dev/null` on mkdfs; working_dir (optional) is the child's cwd.
gb_internal i32 n64_spawn_and_wait(char const *const *arguments, char **environment, bool discard_stdout, char const *working_dir = nullptr) {
	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	if (discard_stdout) {
		posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
	}
	if (working_dir != nullptr) {
		// macOS 26 deprecates the _np name in favour of POSIX 2024's
		// posix_spawn_file_actions_addchdir, which older hosts and libcs lack.
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
		posix_spawn_file_actions_addchdir_np(&actions, working_dir);
	#pragma GCC diagnostic pop
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

gb_internal bool n64_remove_file_if_present(String const &path) {
	char const *path_c = alloc_cstring(temporary_allocator(), path);
	return !gb_file_exists(path_c) || gb_file_remove(path_c);
}

// ---- Packaging --------------------------------------------------------------
// Runs the commands libdragon's n64.mk runs for an `odin-n64.z64` goal
// (n64.mk:135-163, :221-240), spawning each tool directly. Intermediates keep
// n64.mk's basenames because `n64tool --toc` records each input's basename in
// the ROM. A libdragon bump that changes those recipes must be mirrored here.

struct N64Intermediates {
	String dir;
	String elf;
	String map;
	String sym;
	String stripped;
	String dfs;
	String rom_tmp;
	String sdk_link; // only created when the SDK path is not shell-safe
};

typedef Array<char const *> N64Argv;

gb_internal bool n64_is_shell_safe(String const &path) {
	for (isize index = 0; index < path.len; index += 1) {
		char c = cast(char)path[index];
		if (!gb_char_is_alphanumeric(c) && strchr("/._-+,:@%=", c) == nullptr) {
			return false;
		}
	}
	return true;
}

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

gb_internal i32 n64_run_tool(char const *label, N64Argv argv, char **environment, bool show_system_calls,
                             bool discard_stdout = false, char const *working_dir = nullptr) {
	if (show_system_calls) {
		gb_printf_err("[SYSTEM CALL] n64-%s\n", label);
		if (working_dir != nullptr) {
			gb_printf_err("(cd %s) ", working_dir);
		}
		for (char const *argument : argv) {
			gb_printf_err("%s ", argument);
		}
		gb_printf_err("\n\n");
	}
	array_add(&argv, cast(char const *)nullptr);
	i32 result = n64_spawn_and_wait(argv.data, environment, discard_stdout, working_dir);
	if (result != 0) {
		gb_printf_err("N64 packaging step '%s' failed with exit code %d\n", label, result);
	}
	return result;
}

// One fixed directory beside the output, named after it. Rebuilding the same
// output reuses it, and -keep-temp-files leaves the ELF at a predictable path.
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
	out->sdk_link = n64_path_join(a, out->dir, STR_LIT("sdk"));
	char const *dir_c = alloc_cstring(temporary_allocator(), out->dir);
	if (mkdir(dir_c, 0755) != 0 && errno != EEXIST) {
		gb_printf_err("Failed to create N64 build directory %.*s: %s\n", LIT(out->dir), strerror(errno));
		return false;
	}
	return true;
}

gb_internal i32 n64_link_elf(N64BuildRequest *request, N64Intermediates const &files, char **env) {
	N64BuildSettings const &s = request->settings;
	// Compiler object paths come from a hash map whose order is not stable
	// between runs; sort them so the link, and so the ROM, is reproducible.
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
	// n64sym popen()s "$N64_INST/bin/mips64-elf-objdump -t <elf>" unquoted
	// (n64sym.cpp:330, :450), so it runs inside the intermediates directory on
	// relative names, and N64_INST must be shell-safe: a relative `sdk` link
	// stands in for an SDK path with spaces or shell metacharacters. Newer
	// n64sym builds run objdump through an argv and do not need this.
	char const *sym_env_entry = nullptr;
	if (n64_is_shell_safe(s.sdk_root)) {
		sym_env_entry = alloc_cstring(a, concatenate_strings(a, STR_LIT("N64_INST="), s.sdk_root));
	} else {
		n64_remove_file_if_present(files.sdk_link);
		if (!n64_create_symlink(s.sdk_root, files.sdk_link)) {
			gb_printf_err("Failed to link %.*s -> %.*s for n64sym: %s\n", LIT(files.sdk_link), LIT(s.sdk_root), strerror(errno));
			return 1;
		}
		sym_env_entry = "N64_INST=sdk";
	}
	array_init(&argv, a);
	n64_arg(&argv, n64_sdk_path(s, "bin/n64sym")); n64_arg(&argv, "--all");
	n64_arg(&argv, "odin-n64.elf"); n64_arg(&argv, "odin-n64.elf.sym");
	result = n64_run_tool("sym", argv, n64_sanitized_environment(sym_env_entry), show, false, alloc_cstring(a, files.dir));
	if (result != 0) return result;

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

gb_internal N64BuildResult n64_package_rom(N64BuildRequest request) {
	if (!n64_validate_link_inputs(request)) {
		return {1, {}};
	}
	N64Intermediates files = {};
	if (!n64_init_intermediates(&files, request.output_filename, request.output_name)) {
		return {1, {}};
	}
	// Inherited N64_* variables are dropped so they cannot redirect the tools;
	// only n64sym gets N64_INST (see n64_build_rom_from_elf).
	char **env = n64_sanitized_environment();

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
		gb_printf_err("N64 build failed; intermediates were retained at %.*s\n", LIT(files.dir));
		return {result, files.dir};
	}
	if (request.keep_temp_files) {
		gb_printf_err("Retained N64 build intermediates: %.*s\n", LIT(files.dir));
		return {0, files.dir};
	}
	String generated[] = {files.elf, files.map, files.sym, files.stripped, files.dfs, files.sdk_link};
	bool clean = true;
	for (String const &path : generated) {
		clean = n64_remove_file_if_present(path) && clean;
	}
	if (!clean || !n64_remove_directory(files.dir)) {
		gb_printf_err("Warning: could not completely remove N64 build intermediates at %.*s\n", LIT(files.dir));
	}
	return {0, {}};
}
#else
gb_internal N64BuildResult n64_package_rom(N64BuildRequest request) {
	// ponytail: Windows hosts are rejected in n64_prepare_build; this stub only
	// keeps the linker adapter compiling until a Windows packaging path exists.
	gb_printf_err("The integrated N64 ROM build currently requires a POSIX host\n");
	return {1, {}};
}
#endif
