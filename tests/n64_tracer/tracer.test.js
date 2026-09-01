const rom = ares.args[0];
const readySentinel = "ODIN_N64_TRACER_READY:v2";
const passSentinel = "ODIN_N64_TRACER_PASS:v2";
const failSentinel = "ODIN_N64_TRACER_FAIL:v2:";
const cleanupSentinel = "ODIN_N64_TRACER_CLEANUP:v2";
const expectedFrameHash = "13B30B82C8227FAA1DE66FBC058C2F6E5B980118096CE6B090A342D868DDB79F";
const orderedSentinels = [
	"ODIN_N64_TRACER_CHECK:v2:MAIN_REACHED:PASS",
	"ODIN_N64_TRACER_CHECK:v2:ORDERING:PASS",
	"ODIN_N64_TRACER_CHECK:v2:GENERAL_ALLOCATOR:PASS",
	"ODIN_N64_TRACER_CHECK:v2:TEMP_ALLOCATOR:PASS",
	"ODIN_N64_TRACER_CHECK:v2:ALLOCATOR_REPLACEABILITY:PASS",
	readySentinel,
	passSentinel,
	"ODIN_N64_TRACER_MAIN_RETURN:v2",
	cleanupSentinel,
];

if (!rom || ares.args.length !== 1)
	throw new Error("usage: tracer.test.js <n64_tracer.z64>");

ares.setHomebrew(true);
ares.setRenderer("angrylion");
ares.loadRom(rom);
ares.resume();

if (!ares.waitLog(readySentinel, 10))
	throw new Error("tracer did not become ready:\n" + ares.log());

const controller = ares.controller(1);
controller.hold("A");
if (!ares.waitLog(passSentinel, 5))
	throw new Error("tracer did not pass after controller input:\n" + ares.log());
controller.release("A");

if (!ares.waitLog(cleanupSentinel, 5))
	throw new Error("tracer did not return through runtime cleanup:\n" + ares.log());

// Advance after cleanup so libdragon's C entry can process the returned main
// and issue its normal xioctl_exit path. The runner has no direct exit-status
// query, so the cleanup sentinel remains the observable lifecycle oracle.
ares.waitVI();

const log = ares.log();
if (log.includes(failSentinel))
	throw new Error("tracer emitted a failure sentinel:\n" + log);

let previousIndex = -1;
for (const sentinel of orderedSentinels) {
	const index = log.indexOf(sentinel);
	if (index < 0)
		throw new Error("tracer omitted " + sentinel + ":\n" + log);
	if (index <= previousIndex)
		throw new Error("tracer sentinel was out of order: " + sentinel + ":\n" + log);
	previousIndex = index;
}

const frame = ares.screenshot();
if (frame.width !== 640 || frame.height !== 240)
	throw new Error(`unexpected tracer scanout size: ${frame.width}x${frame.height}`);
if (frame.sha256 !== expectedFrameHash)
	throw new Error(`tracer framebuffer mismatch: ${frame.sha256}`);

console.log("tracer: observed ordered runtime/allocator PASS sentinels; framebuffer " + frame.sha256);
