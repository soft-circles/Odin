const rom = ares.args[0];
const passSentinel = "PASS: Odin O64 ABI 23/23";
const failSentinel = "FAIL: Odin O64 ABI";

if (!rom || ares.args.length !== 1)
	throw new Error("usage: baseline.test.js <interop.z64>");

ares.setHomebrew(true);
ares.setRenderer("none");
ares.loadRom(rom);
ares.resume();

if (!ares.waitLog(passSentinel, 10))
	throw new Error("O64 baseline did not pass before the emulated-time timeout:\n" + ares.log());
if (ares.log().includes(failSentinel))
	throw new Error("O64 baseline emitted a failure sentinel:\n" + ares.log());

console.log("baseline: observed " + passSentinel);
