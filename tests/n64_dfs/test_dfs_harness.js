export const GOLDEN_PATH = "dfs.golden.png";
export const EXPECTED_WIDTH = 640;
export const EXPECTED_HEIGHT = 240;

const READY_SENTINEL = "ODIN_N64_DFS_READY:v1";
const PASS_SENTINEL = "ODIN_N64_DFS_PASS:v1";
const FAIL_SENTINEL = "ODIN_N64_DFS_FAIL:v1:";
const ORDERED_SENTINELS = [
	"ODIN_N64_DFS_CHECK:v1:MOUNT:PASS",
	"ODIN_N64_DFS_CHECK:v1:OPEN:PASS",
	"ODIN_N64_DFS_CHECK:v1:SIZE:PASS",
	"ODIN_N64_DFS_CHECK:v1:READ:PASS",
	"ODIN_N64_DFS_CHECK:v1:CONTENT:PASS",
	"ODIN_N64_DFS_CHECK:v1:CLOSE:PASS",
	"ODIN_N64_DFS_STATE:v1:FRAME_SUBMITTED",
	READY_SENTINEL,
	PASS_SENTINEL,
];

function assertNoRomFailure(log) {
	if (log.includes(FAIL_SENTINEL))
		throw new Error("DFS sample emitted a failure sentinel:\n" + log);
}

function assertSentinelOrder(log) {
	let previousIndex = -1;
	for (const sentinel of ORDERED_SENTINELS) {
		const index = log.indexOf(sentinel);
		if (index < 0)
			throw new Error("DFS sample omitted " + sentinel + ":\n" + log);
		if (index <= previousIndex)
			throw new Error("DFS sample sentinel was out of order: " + sentinel + ":\n" + log);
		previousIndex = index;
	}
}

function assertFrameDimensions(frame) {
	if (frame.width !== EXPECTED_WIDTH || frame.height !== EXPECTED_HEIGHT) {
		throw new Error(
			`unexpected DFS scanout size: ${frame.width}x${frame.height}; ` +
			`expected ${EXPECTED_WIDTH}x${EXPECTED_HEIGHT}`,
		);
	}
}

export function captureDfsSuccess(emulator, rom) {
	emulator.setHomebrew(true);
	emulator.setRenderer("angrylion");
	emulator.loadRom(rom);
	emulator.resume();

	if (!emulator.waitLog(READY_SENTINEL, 10))
		throw new Error("DFS sample did not become ready:\n" + emulator.log());
	if (!emulator.waitLog(PASS_SENTINEL, 5))
		throw new Error("DFS sample did not pass:\n" + emulator.log());

	const log = emulator.log();
	assertNoRomFailure(log);
	assertSentinelOrder(log);

	// The state marker is emitted after display_show. One VI makes the captured
	// scanout unambiguous while keeping screenshot comparison side-effect free.
	emulator.waitVI();
	const frame = emulator.screenshot();
	assertFrameDimensions(frame);
	return {frame, log};
}
