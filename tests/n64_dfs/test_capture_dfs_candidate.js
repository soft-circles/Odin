import {captureDfsSuccess, GOLDEN_PATH} from "./test_dfs_harness.js";

const rom = ares.args[0];
const candidatePath = ares.args[1] || "dfs.candidate.png";

if (!rom || ares.args.length > 2)
	throw new Error("usage: test_capture_dfs_candidate.js <n64_dfs.z64> [candidate.png]");
if (candidatePath.split(/[\\/]/).pop() === GOLDEN_PATH) {
	throw new Error(
		`refusing to overwrite reviewed ${GOLDEN_PATH}; capture a candidate, review it, ` +
		"confirm the same state on hardware, then promote it explicitly",
	);
}

const {frame} = captureDfsSuccess(ares, rom);
frame.save(candidatePath);

console.log(
	`dfs: wrote candidate ${candidatePath} (${frame.width}x${frame.height}, ` +
	`RGB SHA-256 ${frame.sha256}); this is not a golden until it is reviewed in ` +
		"upstream Ares and confirmed against the hardware checkpoint",
	);
