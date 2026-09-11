// N64 ROM settings parsed from the command line. Shared by BuildContext and
// the n64_build.cpp packaging module so the adapter in linker.cpp does not
// copy them field by field.
struct N64BuildSettings {
	String sdk_root;
	String title;
	String region;
	String save_type;
	String controllers[4];
	String assets;
	String metadata;
	bool   rtc;
};
