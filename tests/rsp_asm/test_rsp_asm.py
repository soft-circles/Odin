#!/usr/bin/env python3
"""Public CLI checks for the isolated RSP assembly artifact."""
import os
from pathlib import Path
import subprocess
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
        self.assertEqual(self.build().returncode, 0)
        self.assertEqual(self.output.read_bytes(), first)
        self.assertEqual(sorted(p.name for p in self.path.iterdir()), ["command.odin", "rsp_command.S"])

    def test_branch_artifact(self):
        result = self.build((ROOT / "tests/rsp_asm/branch.odin").read_text())
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_vector_artifact(self):
        result = self.build((ROOT / "tests/rsp_asm/vector.odin").read_text())
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_scalar_scratch_dma_artifact(self):
        result = self.build((ROOT / "tests/rsp_asm/scalar.odin").read_text())
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn(".space 16", self.output.read_text())
        self.assertIn("j DMAOut", self.output.read_text())

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
        ]
        for source in cases:
            with self.subTest(source=source):
                self.output.unlink(missing_ok=True)
                result = self.build(source)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("command.odin", result.stdout)
                self.assertFalse(self.output.exists())
        self.output = self.path / "missing" / "rsp_command.S"
        self.assertNotEqual(self.build().returncode, 0)
        self.assertFalse(self.output.exists())
        self.output = self.path / "rsp_directory.S"
        self.output.mkdir()
        self.assertNotEqual(self.build().returncode, 0)
        self.assertTrue(self.output.is_dir())
        self.assertFalse(list(self.path.glob("*.tmp*")))

    def test_valid_metadata_limits_constants_and_alias(self):
        for words in ("1", "62", "WORDS"):
            source = MINIMAL.replace("rspq_command_words=1", f"rspq_command_words={words}, rspq_scratch_bytes=0")
            source += "WORDS :: 60 + 2\n"
            result = self.build(source.replace("%ra", "%r31"))
            self.assertEqual(result.returncode, 0, result.stdout)

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

    def test_effects_and_boundaries(self):
        scalar = (ROOT / "tests/rsp_asm/scalar.odin").read_text()
        vector = (ROOT / "tests/rsp_asm/vector.odin").read_text()
        bad = [scalar.replace("%t3, %a1, %a2", "%t3, %t2, %a2"),
               scalar.replace("%t3, %a1, %a2", "%gp, %a1, %a2"),
               scalar.replace("%t3, %a1, %a2", "%r31, %a1, %a2"),
               scalar.replace("[%s4 + 4]", "[%s4 + 1]"),
               scalar.replace("sw %zero, [%s4 + 12]", "nop"),
               scalar.replace("ori %t0, %zero, 15", "ori %t0, %zero, 31"),
               scalar.replace("ori %t0, %zero, 15", "li %t0, 65536"),
               scalar.replace("ori %t1, %zero, 0", "addiu %t1, %zero, 32768"),
               scalar.replace("ori %t1, %zero, 0", "andi %t1, %zero, -1"),
               scalar.replace("[%s4]", "[%zero]"),
               scalar.replace("[%s4]", "[%s4 + %t3]"),
               vector.replace("vxor %v01, %v00, %v00", "nop"),
               vector.replace(".e0", ".e8"), vector.replace(".e0", ".b0"),
               vector.replace("vxor %v03", "vadd %v03")]
        for source in bad:
            with self.subTest(source=source):
                result = self.build(source)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("Error:", result.stdout)
        adjusted = scalar.replace("la %s4, rspq_scratch", "la %s4, rspq_scratch\naddiu %s4, %s4, -4").replace("sw %t3, [%s4]", "sw %t3, [%s4 + 4]\naddiu %s4, %s4, 4")
        self.assertEqual(self.build(adjusted).returncode, 0)
        for literal in ("-32768", "32767"):
            self.assertEqual(self.build(scalar.replace("ori %t1, %zero, 0", f"addiu %t1, %zero, {literal}")).returncode, 0)
        source = MINIMAL + "N0 :: ~1\n" + "\n".join(f"N{i} :: N{i-1} + N{i-1}" for i in range(1, 35))
        self.assertEqual(self.build(source).returncode, 0)

    def test_branch_joins_and_delay_slots(self):
        branch = (ROOT / "tests/rsp_asm/branch.odin").read_text()
        for source in (branch.replace(".equal", ".missing", 1),
                       branch.replace("addiu %t3, %t3, 1", "j .done"),
                       branch.replace("addiu %t3, %t3, 1", "li %t3, 65536"),
                       branch.replace("li %t3, 10", "nop"),
                       branch + "extra :: asm() { jr %ra; nop; }\n"):
            self.assertNotEqual(self.build(source).returncode, 0)
        loop = "package loop\n@(rspq_command_words=2)\ncommand :: asm() {\n.loop:\naddiu %a1, %a1, -1\nbne %a1, %zero, .loop\nnop\njr %ra\nnop\n}\n"
        result = self.build(loop)
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_sdk_return_and_element_words(self):
        sdk = Path(os.environ.get("N64_INST", Path.home()/"n64_toolchain"))
        if not (sdk/"include/n64.mk").is_file():
            self.skipTest("set N64_INST for independent SDK encoding checks")
        source = MINIMAL.replace("    jr %ra", "    mtc2 %a0, %v01.e7\n    mfc2 %t0, %v01.e7\n    jr %ra")
        result = self.build(source)
        self.assertEqual(result.returncode, 0, result.stdout)
        elf, raw = self.path/"rsp.elf", self.path/"text.bin"
        subprocess.run([str(sdk/"bin/mips64-elf-gcc"), "-march=mips1", "-mabi=32",
                        f"-I{sdk}/mips64-elf/include", f"-L{sdk}/mips64-elf/lib",
                        "-nostartfiles", "-Wl,-Trsp.ld", "-Wl,--gc-sections", str(self.output), "-o", str(elf)], check=True, capture_output=True)
        subprocess.run([str(sdk/"bin/mips64-elf-objcopy"), "-O", "binary", "-j", ".text", str(elf), str(raw)], check=True)
        # Reviewed COP2 fields: rt=4/8, vector=1, byte element=14; then JR r31/NOP.
        self.assertEqual(raw.read_bytes()[-16:], bytes.fromhex("48840f00 48080f00 03e00008 00000000"))

if __name__ == "__main__":
    unittest.main()
