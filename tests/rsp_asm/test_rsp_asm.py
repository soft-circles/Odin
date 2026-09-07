#!/usr/bin/env python3
"""Public CLI checks for the isolated RSP assembly artifact."""
import os
from pathlib import Path
import subprocess
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
ODIN = Path(os.environ.get("ODIN", ROOT / "odin"))
MINIMAL = """package overlay
@(rspq_command_words=1)
command :: asm() {
    jr %ra
    nop
}
"""

class RspArtifactTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="odin rsp ")
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)
        self.source = self.path / "command.odin"
        self.output = self.path / "rsp_command.S"

    def build(self, source=MINIMAL, *flags):
        self.source.write_text(source)
        return subprocess.run([str(ODIN), "build", str(self.source), "-file",
                               "-build-mode:rsp-asm", "-rsp-entry:command",
                               f"-out:{self.output}", *flags],
                              text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=15)

    def test_minimal_overlay_is_separate_and_deterministic(self):
        result = self.build()
        self.assertEqual(result.returncode, 0, result.stdout)
        first = self.output.read_bytes()
        self.assertIn(b"#include <rsp_queue.inc>", first)
        self.assertIn(b"RSPQ_DefineCommand rsp_command, 4", first)
        self.assertIn(b"RSPQ_EmptySavedState", first)
        self.assertIn(b"jr $31", first)
        self.assertIn(b"#line 4 ", first)
        self.assertIn(b"#line 5 ", first)
        self.assertIn(str(self.source.resolve()).encode(), first)
        self.assertEqual(self.build().returncode, 0)
        self.assertEqual(self.output.read_bytes(), first)
        self.assertEqual(sorted(p.name for p in self.path.iterdir()), ["command.odin", "rsp_command.S"])

    def test_nonzero_scratch_is_unsupported(self):
        result = self.build(MINIMAL.replace("words=1", "words=1, rspq_scratch_bytes=16"))
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("command.odin(2:", result.stdout)
        self.assertIn("unsupported", result.stdout)
        self.assertFalse(self.output.exists())

    def test_only_return_with_nop_delay_slot_is_supported(self):
        for body in ("ori %t0, %zero, 1; jr %ra; nop;",
                     "jr %ra; ori %t0, %zero, 1;",
                     "vxor %v01, %v00, %v00; jr %ra; nop;",
                     "j .done; nop; .done: jr %ra; nop;",
                     "jr %ra; nop; nop;", ".entry: jr %ra; nop;"):
            with self.subTest(body=body):
                result = self.build("package overlay\n@(rspq_command_words=1)\ncommand :: asm() { " + body + " }\n")
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("command.odin", result.stdout)
                self.assertFalse(self.output.exists())

    def test_metadata_and_source_domain_fail_closed(self):
        cases = [
            MINIMAL.replace("rspq_command_words=1", "rspq_command_words=0"),
            MINIMAL.replace("rspq_command_words=1", "rspq_command_words=63"),
            MINIMAL.replace("@(rspq_command_words=1)", ""),
            MINIMAL.replace("rspq_command_words=1", "rspq_command_words=1, rspq_scratch_bytes=15"),
            MINIMAL.replace("asm()", "asm(x: i32)"),
            MINIMAL.replace("asm()", "asm() -> (x: i32)"),
            MINIMAL + "extra :: asm() { jr %ra; nop; }\n",
            MINIMAL + "extra :: proc() {}\n",
            MINIMAL + "extra: i32\n",
            MINIMAL + "captured :: &command\n",
            MINIMAL.replace("package overlay", 'package overlay\nimport "core:fmt"'),
            MINIMAL.replace("jr %ra", "jr %v31"),
            MINIMAL.replace("jr %ra", "jr %v0"),
            MINIMAL.replace("jr %ra", "jr %sp"),
            MINIMAL.replace("jr %ra", "jr %r32"),
            MINIMAL.replace("jr %ra", "jr %ra.e0"),
            MINIMAL.replace("    nop", ""),
            MINIMAL.replace("    nop", "    break"),
            MINIMAL.replace("    nop", "    jr %ra"),
            MINIMAL.replace("    nop", "    nop 1"),
            MINIMAL.replace("    nop", "    nop\n    add.s %r1, %r2, %r3"),
            MINIMAL.replace("    nop", "    #align 16"),
            MINIMAL + "cycle :: cycle\n",
            MINIMAL.replace("words=1", "words=true"),
            MINIMAL.replace("words=1", "words=1.5"),
            MINIMAL.replace("words=1", "words=1, rspq_command_words=2"),
            MINIMAL.replace("words=1", "words=1, unknown=0"),
            MINIMAL.replace("@(rspq_command_words=1)", "@(rspq_command_words)"),
            MINIMAL.replace("words=1", "words=1, rspq_scratch_bytes=-1"),
            MINIMAL.replace("words=1", "words=1, rspq_scratch_bytes=4097"),
            MINIMAL + "group :: asm { command }\n",
            MINIMAL + 'foreign import ignored "system:bad"\n',
            MINIMAL + "when false { hidden :: proc() {} }\n",
            MINIMAL.replace("jr %ra", "jr captured") + "captured: i32\n",
            MINIMAL.replace("asm()", "asm() [#volatile]"),
        ]
        for source in cases:
            with self.subTest(source=source):
                self.output.unlink(missing_ok=True)
                result = self.build(source)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("command.odin", result.stdout)
                self.assertFalse(self.output.exists())

    def test_register_identity_is_independent_of_spelling(self):
        aliases = "zero at a0 a1 a2 a3 t0 t1 t2 t3 t4 t5 t6 t7 t8 t9 s0 s1 s2 s3 s4 s5 s6 s7 s8 k0 k1 gp sp fp ra".split()
        for name in [f"r{i}" for i in range(32)] + aliases + ["v0", "v1", "v31", "r32", "r031", "unknown"]:
            with self.subTest(register=name):
                self.output.unlink(missing_ok=True)
                result = self.build(MINIMAL.replace("%ra", f"%{name}"))
                if name in ("r31", "ra"):
                    self.assertEqual(result.returncode, 0, result.stdout)
                    self.assertIn("jr $31", self.output.read_text())
                else:
                    self.assertNotEqual(result.returncode, 0, result.stdout)
                    self.assertFalse(self.output.exists())

    def test_selector_is_required_and_selects_a_template(self):
        self.source.write_text(MINIMAL)
        for selection in ([], ["-rsp-entry:missing"]):
            result = self.run_compiler("build", str(self.source), "-file", "-build-mode:rsp-asm",
                                       f"-out:{self.output}", *selection)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn("-rsp-entry", result.stdout)
            self.assertFalse(self.output.exists())

    def test_whole_package_is_checked(self):
        self.source.write_text(MINIMAL)
        other = self.path / "other.odin"
        other.write_text("package overlay\nunused :: proc() {}\n")
        result = self.run_compiler("build", str(self.path), "-build-mode:rsp-asm", "-rsp-entry:command", f"-out:{self.output}")
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("other.odin(2:", result.stdout)
        self.assertFalse(self.output.exists())

    def test_emission_needs_no_runtime_or_cpu_tool_invocations(self):
        self.source.write_text(MINIMAL)
        for collection in ("base", "core", "vendor"):
            (self.path/collection).mkdir()
        result = self.run_compiler("build", str(self.source), "-file", "-build-mode:rsp-asm",
                                   "-rsp-entry:command", f"-out:{self.output}", "-show-system-calls",
                                   env={**os.environ, "ODIN_ROOT": str(self.path), "PATH": ""})
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual(result.stdout, "")
        self.assertTrue(self.output.is_file())

    def run_compiler(self, *arguments, **kwargs):
        return subprocess.run([str(ODIN), *arguments], text=True, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, timeout=15, **kwargs)

    def test_valid_metadata_limits_constants_and_alias(self):
        for words in ("1", "62", "WORDS"):
            source = MINIMAL.replace("rspq_command_words=1", f"rspq_command_words={words}, rspq_scratch_bytes=0")
            source += "WORDS :: 60 + 2\n"
            result = self.build(source.replace("%ra", "%r31"))
            self.assertEqual(result.returncode, 0, result.stdout)

    def test_non_template_entry_has_source_location(self):
        result = self.build("package overlay\ncommand :: 1\n")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("command.odin(2:", result.stdout)
        self.assertFalse(self.output.exists())

    def test_cpu_build_rejects_rsp_metadata_even_on_cpu_instructions(self):
        self.source.write_text("package cpu\n@(rspq_command_words=1)\ncommand :: asm() { nop; }\nmain :: proc() { command(); }\n")
        result = subprocess.run([str(ODIN), "check", str(self.source), "-file", "-target:linux_amd64"],
                                text=True, capture_output=True, timeout=15)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("command.odin(2:", result.stderr)
        self.assertIn("rsp-asm", result.stderr)

    def test_bad_output_keeps_previous_complete_artifact(self):
        self.assertEqual(self.build().returncode, 0)
        old = self.output.read_bytes()
        result = self.build(MINIMAL.replace("nop", "unknown"))
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.output.read_bytes(), old)
        self.output = self.path / "wrong.S"
        self.assertNotEqual(self.build().returncode, 0)
        self.assertFalse(self.output.exists())
        self.output = self.path / "missing" / "rsp_command.S"
        self.assertNotEqual(self.build().returncode, 0)
        self.assertFalse(self.output.exists())
        self.output = self.path / "rsp_directory.S"
        self.output.mkdir()
        self.assertNotEqual(self.build().returncode, 0)
        self.assertTrue(self.output.is_dir())
        self.assertFalse(list(self.path.glob("*.tmp*")))

    def test_sdk_return_words_and_wrapper(self):
        sdk = Path(os.environ.get("N64_INST", Path.home()/"n64_toolchain"))
        if not (sdk/"include/n64.mk").is_file():
            if os.environ.get("N64_VALIDATION_MODE") == "full" or "N64_INST" in os.environ:
                self.fail("Configured SDK is missing include/n64.mk")
            self.skipTest("set N64_INST for independent SDK encoding checks")
        self.run_tool([os.sys.executable, str(ROOT/"tests/o64_abi/validate_sdk.py"), str(sdk)])
        for words in (1, 62):
            with self.subTest(words=words):
                result = self.build(MINIMAL.replace("words=1", f"words={words}"))
                self.assertEqual(result.returncode, 0, result.stdout)
                elf = self.path / "rsp.elf"
                reference = self.path / "reference.elf"
                for source, target in ((self.output, elf), (ROOT/"tests/rsp_asm/reference.S", reference)):
                    self.run_tool([str(sdk/"bin/mips64-elf-gcc"), "-march=mips1", "-mabi=32",
                                   "-Wa,--fatal-warnings", f"-DREFERENCE_WORDS={words}",
                                   f"-I{sdk}/mips64-elf/include", f"-L{sdk}/mips64-elf/lib",
                                   "-nostartfiles", "-Wl,-Trsp.ld", "-Wl,--gc-sections", str(source), "-o", str(target)])
                text = self.sdk_section(sdk, elf, ".text")
                data = self.sdk_section(sdk, elf, ".data")
                self.assertEqual(text, self.sdk_section(sdk, reference, ".text"))
                self.assertEqual(data, self.sdk_section(sdk, reference, ".data"))
                self.assertEqual(text[-8:], bytes.fromhex("03e00008 00000000"))
                symbols = self.sdk_symbols(sdk, elf)
                self.assertEqual(symbols["_start"], symbols["_text_start"])
                self.assertGreater(symbols["rsp_command"], symbols["_start"])
                self.assertEqual(symbols["rsp_command"], symbols["_ovl_text_start"])
                self.assertLessEqual(len(text), 4096)
                self.assertLessEqual(len(data), 4096)
                header = symbols["_RSPQ_OVERLAY_HEADER"] - symbols["_data_start"]
                table = symbols["_RSPQ_OVERLAY_COMMAND_TABLE"] - symbols["_data_start"]
                state = symbols["_RSPQ_SAVED_STATE_START"] - symbols["_data_start"]
                state_size = symbols["_RSPQ_SAVED_STATE_END"] - symbols["_RSPQ_SAVED_STATE_START"]
                self.assertEqual(struct.unpack_from(">HHIHH", data, header), (state, state_size - 1, 0, 0, 0))
                self.assertGreaterEqual(state_size, 8)
                descriptor, terminator = struct.unpack_from(">HH", data, table)
                self.assertEqual(descriptor >> 10, words)
                self.assertEqual((descriptor & 1023) * 4, symbols["rsp_command"] - symbols["_start"])
                self.assertEqual(terminator, 0)
                self.assertIn("There are no relocations", self.run_tool([str(sdk/"bin/mips64-elf-readelf"), "-r", str(elf)]))

        # Exercise the actual SDK RSP filename rule in a directory containing spaces.
        (self.path/"sdk").symlink_to(sdk, target_is_directory=True)
        (self.path/"Makefile").write_text("N64_INST := sdk\nBUILD_DIR := build\nSOURCE_DIR := .\ninclude sdk/include/n64.mk\nRSPASFLAGS += $(N64_RSPASFLAGS)\n")
        self.run_tool(["/usr/bin/make", "build/rsp_command.o"], cwd=self.path)
        wrapped = self.path / "build/rsp_command.o"
        symbols = self.sdk_symbols(sdk, wrapped)
        self.assertEqual(symbols["rsp_command_text_end"] - symbols["rsp_command_text_start"], len(text))
        self.assertEqual(symbols["rsp_command_data_end"] - symbols["rsp_command_data_start"], len(data))
        self.assertEqual(self.sdk_section(sdk, wrapped, ".text"), b"")
        wrapped_data = self.sdk_section(sdk, wrapped, ".data")
        self.assertEqual(wrapped_data[symbols["rsp_command_text_start"]:symbols["rsp_command_text_end"]], text)

    def run_tool(self, command, **kwargs):
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=30, **kwargs)
        self.assertEqual(result.returncode, 0, result.stdout)
        return result.stdout

    def sdk_section(self, sdk, elf, section):
        raw = self.path / "section.bin"
        self.run_tool([str(sdk/"bin/mips64-elf-objcopy"), "-O", "binary", "-j", section, str(elf), str(raw)])
        return raw.read_bytes()

    def sdk_symbols(self, sdk, elf):
        listing = self.run_tool([str(sdk/"bin/mips64-elf-nm"), "--defined-only", str(elf)])
        return {name: int(address, 16) for address, kind, name in (line.split() for line in listing.splitlines())}

if __name__ == "__main__":
    unittest.main()
