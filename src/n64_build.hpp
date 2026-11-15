// A DSO module -n64-dso builds into the -n64-assets directory: name is its path there, ending in .dso.
struct N64Dso {
	String name;
	Array<String> sources;
};

// N64 ROM settings parsed from the command line. Shared by BuildContext and
// the n64_build.cpp packaging module so the adapter in linker.cpp does not
// copy them field by field.
struct N64BuildSettings {
	String sdk_root;
	String sdk_description; // the SDK description file; see n64_load_sdk_description
	String title;
	String region;
	String save_type;
	String controllers[4];
	String assets;
	String metadata;
	String pack; // runs `<pack> --n64-tool <tool> ...` in place of the SDK's packaging tools
	String cflags; // appended to the SDK flags for foreign .c, .cpp and .S sources
	Array<N64Dso> dsos;
	bool   rtc;
};
