import {captureDrivenCheckpoint, GOLDEN_PATH} from "./test_pong_harness.js";

const rom = ares.args[0];
const diffPath = "pong.diff.png";

if (!rom || ares.args.length !== 1)
	throw new Error("usage: pong.test.js <n64_pong.z64>");

const {frame} = captureDrivenCheckpoint(ares, rom);

let comparison;
try {
	comparison = frame.compare(GOLDEN_PATH);
} catch (error) {
	throw new Error(
		`could not compare the Pong framebuffer with reviewed ${GOLDEN_PATH}: ${String(error)}`,
	);
}

if (!comparison.match) {
	if (comparison.diff)
		comparison.diff.save(diffPath);

	const details = comparison.reason ||
		`${comparison.diffPixels}/${comparison.totalPixels} pixels differ; ` +
		`max delta ${comparison.maxDelta}, average delta ${comparison.avgDelta}`;
	const diffDetails = comparison.diff ? `; visual diff ${diffPath}` : "";
	throw new Error(
		`Pong framebuffer mismatch (${details}); actual RGB SHA-256 ${frame.sha256}` +
		diffDetails,
	);
}

console.log(
	"pong: observed real port-1 Down and A input, checkpoint, and moving rally; " +
	`framebuffer matches ${GOLDEN_PATH} (${frame.sha256})`,
);
