const rom = ares.args[0];
if (!rom || ares.args.length !== 1)
	throw new Error("usage: runtime.test.js <runtime.z64>");

ares.setHomebrew(true);
ares.setRenderer("angrylion");
ares.loadRom(rom);
ares.resume();
if (!ares.waitLog("ODIN_N64_RUNTIME_CLEANUP:v2", 10))
	throw new Error("runtime did not return through cleanup:\n" + ares.log());
const log = ares.log();
if (log.includes("ODIN_N64_RUNTIME_FAIL:"))
	throw new Error("runtime failure:\n" + log);
const ordered = [
	"ODIN_N64_RUNTIME_CHECK:v2:MAIN_REACHED:PASS",
	"ODIN_N64_RUNTIME_CHECK:v2:ORDERING:PASS",
	"ODIN_N64_RUNTIME_CHECK:v2:GENERAL_ALLOCATOR:PASS",
	"ODIN_N64_RUNTIME_CHECK:v2:TEMP_ALLOCATOR:PASS",
	"ODIN_N64_RUNTIME_CHECK:v2:ALLOCATOR_REPLACEABILITY:PASS",
	"ODIN_N64_RUNTIME_PASS:v2",
	"ODIN_N64_RUNTIME_MAIN_RETURN:v2",
	"ODIN_N64_RUNTIME_CLEANUP:v2",
];
let previous = -1;
for (const sentinel of ordered) {
	const index = log.indexOf(sentinel);
	if (index <= previous)
		throw new Error("missing or out-of-order sentinel " + sentinel + ":\n" + log);
	previous = index;
}
console.log("runtime: ordered startup, allocators, main return and cleanup PASS");
