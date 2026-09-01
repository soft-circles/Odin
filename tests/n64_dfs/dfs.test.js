import {captureDfsSuccess, GOLDEN_PATH} from "./test_dfs_harness.js";

const rom = ares.args[0];
const diffPath = "dfs.diff.png";

if (!rom || ares.args.length !== 1)
	throw new Error("usage: dfs.test.js <n64_dfs.z64>");

const {frame} = captureDfsSuccess(ares, rom);

let comparison;
try {
	comparison = frame.compare(GOLDEN_PATH);
} catch (error) {
	throw new Error(
		`could not compare the DFS framebuffer with reviewed ${GOLDEN_PATH}: ${String(error)}; ` +
		"capture a candidate, review it in upstream Ares and on hardware, then promote it explicitly",
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
		`DFS framebuffer mismatch (${details}); actual RGB SHA-256 ${frame.sha256}` +
		diffDetails,
	);
}

console.log(
	"dfs: observed mount/open/size/read/content/close PASS sentinels; " +
	`framebuffer matches ${GOLDEN_PATH} (${frame.sha256})`,
);
