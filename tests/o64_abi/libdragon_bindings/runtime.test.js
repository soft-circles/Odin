const rom = ares.args[0];
const passSentinel = "PASS: Odin libdragon binding ABI 23/23";
const failSentinel = "FAIL: Odin libdragon binding ABI";

if (!rom || ares.args.length !== 1)
	throw new Error("usage: runtime.test.js <libdragon_bindings.z64>");

ares.setHomebrew(true);
ares.setRenderer("none");
ares.loadRom(rom);
ares.resume();

if (!ares.waitLog(passSentinel, 10))
	throw new Error("libdragon binding ABI probe timed out:\n" + ares.log());
if (ares.log().includes(failSentinel))
	throw new Error("libdragon binding ABI probe emitted a failure:\n" + ares.log());

console.log("libdragon-bindings: observed " + passSentinel);
