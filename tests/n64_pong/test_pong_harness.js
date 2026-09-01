export const GOLDEN_PATH = "pong.golden.png";
export const EXPECTED_WIDTH = 640;
export const EXPECTED_HEIGHT = 240;

const READY_SENTINEL = "ODIN_N64_PONG_READY:v1";
const INPUT_SENTINEL = "ODIN_N64_PONG_INPUT:v1:DOWN";
const STATE_SENTINEL = "ODIN_N64_PONG_STATE:v1:CHECKPOINT";
const PASS_SENTINEL = "ODIN_N64_PONG_PASS:v1";
const A_SENTINEL = "ODIN_N64_PONG_INPUT:v1:A";
const RALLY_SENTINEL = "ODIN_N64_PONG_STATE:v1:RALLY_STARTED";
const FAIL_SENTINEL = "ODIN_N64_PONG_FAIL:v1:";
const ORDERED_SENTINELS = [
	"ODIN_N64_PONG_CHECK:v1:GENERAL_ALLOCATOR:PASS",
	"ODIN_N64_PONG_CHECK:v1:TEMP_ALLOCATOR:PASS",
	READY_SENTINEL,
	"ODIN_N64_PONG_CHECK:v1:TICKS_ADVANCED:PASS",
	INPUT_SENTINEL,
	STATE_SENTINEL,
	PASS_SENTINEL,
	A_SENTINEL,
	RALLY_SENTINEL,
];

function waitForSentinel(emulator, sentinel, maxSeconds, failureDescription) {
	if (!emulator.waitLog(sentinel, maxSeconds))
		throw new Error(failureDescription + ":\n" + emulator.log());
	assertNoRomFailure(emulator.log());
}

function assertNoRomFailure(log) {
	if (log.includes(FAIL_SENTINEL))
		throw new Error("Pong emitted a failure sentinel:\n" + log);
}

function assertSentinelOrder(log) {
	let previousIndex = -1;
	for (const sentinel of ORDERED_SENTINELS) {
		const index = log.indexOf(sentinel);
		if (index < 0)
			throw new Error("Pong omitted " + sentinel + ":\n" + log);
		if (index <= previousIndex)
			throw new Error("Pong sentinel was out of order: " + sentinel + ":\n" + log);
		previousIndex = index;
	}
}

function assertFrameDimensions(frame) {
	if (frame.width !== EXPECTED_WIDTH || frame.height !== EXPECTED_HEIGHT) {
		throw new Error(
			`unexpected Pong scanout size: ${frame.width}x${frame.height}; ` +
			`expected ${EXPECTED_WIDTH}x${EXPECTED_HEIGHT}`,
		);
	}
}

export function captureDrivenCheckpoint(emulator, rom) {
	emulator.setHomebrew(true);
	emulator.setRenderer("angrylion");
	emulator.loadRom(rom);
	emulator.resume();

	waitForSentinel(emulator, READY_SENTINEL, 10, "Pong did not become ready");

	const controller = emulator.controller(1);
	controller.hold("Down");
	try {
		waitForSentinel(
			emulator,
			INPUT_SENTINEL,
			5,
			"Pong did not observe port-1 D-pad Down input",
		);
		waitForSentinel(
			emulator,
			STATE_SENTINEL,
			5,
			"Pong did not reach the controller-driven bottom-clamp checkpoint",
		);
	} finally {
		controller.release("Down");
	}

	waitForSentinel(emulator, PASS_SENTINEL, 5, "Pong did not pass its runtime checks");

	// STATE is emitted only after the checkpoint frame is submitted. Advancing
	// one VI makes the last presented frame unambiguous before the pure capture.
	emulator.waitVI();
	const frame = emulator.screenshot();
	assertFrameDimensions(frame);

	controller.hold("A");
	try {
		waitForSentinel(emulator, A_SENTINEL, 5, "Pong did not observe port-1 A input");
		waitForSentinel(emulator, RALLY_SENTINEL, 5, "Pong did not start a moving rally");
	} finally {
		controller.release("A");
	}

	const log = emulator.log();
	assertNoRomFailure(log);
	assertSentinelOrder(log);
	return {frame, log};
}
