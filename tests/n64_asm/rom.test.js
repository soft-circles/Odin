const rom = ares.args[0];
if (!rom || ares.args.length !== 1)
	throw new Error("usage: rom.test.js <asm.z64>");

ares.setHomebrew(true);
ares.setRenderer("none");
ares.loadRom(rom);
ares.resume();
if (!ares.waitLog("ODIN_N64_ASM_PASS:v1", 10))
	throw new Error("asm template ROM did not pass:\n" + ares.log());
const log = ares.log();
if (log.includes(":FAIL") || log.includes("ODIN_N64_ASM_FAIL:"))
	throw new Error("asm template failure:\n" + log);
const ordered = [
	"ODIN_N64_ASM_CHECK:v1:MAIN_REACHED:PASS",
	"ODIN_N64_ASM_CHECK:v1:COUNT_READ:PASS",
	"ODIN_N64_ASM_CHECK:v1:CALL_GENERATED_CODE:PASS",
	"ODIN_N64_ASM_CHECK:v1:ICACHE_REFRESH:PASS",
	"ODIN_N64_ASM_CHECK:v1:NEGATIVE_CONTROL:PASS",
	"ODIN_N64_ASM_CHECK:v1:COUNT_ADVANCED:PASS",
	"ODIN_N64_ASM_PASS:v1",
];
let previous = -1;
for (const sentinel of ordered) {
	const index = log.indexOf(sentinel);
	if (index <= previous)
		throw new Error("missing or out-of-order sentinel " + sentinel + ":\n" + log);
	previous = index;
}
console.log("asm: cache sync, generated-code call, I-cache refresh, negative control and CP0 Count PASS");
