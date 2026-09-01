const rom = ares.args[0];
const readySentinel = "ODIN_N64_CONSOLE_READY:v1";
const passSentinel = "ODIN_N64_CONSOLE_PASS:v1";
const failSentinel = "ODIN_N64_CONSOLE_FAIL:v1";

if (!rom || ares.args.length !== 1)
	throw new Error("usage: console.test.js <n64_console.z64>");

ares.setHomebrew(true);
ares.setRenderer("angrylion");
ares.loadRom(rom);
ares.resume();

if (!ares.waitLog(readySentinel, 10))
	throw new Error("console fixture did not become ready:\n" + ares.log());
if (!ares.waitLog(passSentinel, 5))
	throw new Error("console lifecycle did not pass:\n" + ares.log());
if (ares.log().includes(failSentinel))
	throw new Error("console fixture emitted a failure sentinel:\n" + ares.log());

ares.waitVI();
const frame = ares.screenshot();
if (frame.width !== 640 || frame.height !== 240)
	throw new Error(`unexpected console scanout size: ${frame.width}x${frame.height}`);

console.log("console: observed " + passSentinel);
