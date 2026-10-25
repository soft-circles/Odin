const rom = ares.args[0];
if (!rom || ares.args.length !== 1)
	throw new Error("usage: rom.test.js <bit_field.z64>");

ares.setHomebrew(true);
ares.setRenderer("none");
ares.loadRom(rom);
ares.resume();
if (!ares.waitLog("ODIN_N64_BIT_FIELD_DONE", 10))
	throw new Error("bit_field probe did not finish:\n" + ares.log());
const log = ares.log();
if (log.includes("ODIN_N64_BIT_FIELD_FAIL:") || !log.includes("ODIN_N64_BIT_FIELD_PASS"))
	throw new Error("bit_field failure:\n" + log);
console.log("bit_field: run-time field loads and stores match integer bit positions PASS");
