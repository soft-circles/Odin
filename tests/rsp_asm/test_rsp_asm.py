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
SCALAR = (ROOT / "tests/rsp_asm/scalar.odin").read_text()
BRANCH = (ROOT / "tests/rsp_asm/branch.odin").read_text()

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

    def test_declared_scratch_is_aligned_and_reserved(self):
        result = self.build(MINIMAL.replace("words=1", "words=1, rspq_scratch_bytes=16"))
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn(".balign 16\nrspq_scratch:\n    .space 16", self.output.read_text())

    def test_scalar_arithmetic_and_finite_materialization(self):
        body = """li %r2, 0x12345678
    move %r3, %a0
    addu %t0, %r2, %r3
    addiu %t0, %t0, -32768
    andi %t1, %t0, 65535
    ori %t2, %zero, 65535
    xori %t3, %t1, 0
    and %t4, %t2, %t3
    or %t5, %t4, %r2
    xor %t6, %t5, %a0
    lui %at, 65535
    jr %ra
    nop"""
        result = self.build(MINIMAL.replace("jr %ra\n    nop", body))
        self.assertEqual(result.returncode, 0, result.stdout)
        text = self.output.read_text()
        self.assertIn("lui $2, 4660\n    ori $2, $2, 22136", text)
        self.assertIn("addu $3, $4, $0", text)
        self.assertIn("addiu $8, $8, -32768", text)

    def test_scalar_command_initializes_scratch_and_tail_transfers(self):
        result = self.build(SCALAR)
        self.assertEqual(result.returncode, 0, result.stdout)
        text = self.output.read_text()
        self.assertIn("ori $20, $0, %lo(rspq_scratch)", text)
        self.assertIn("sw $0, 12($20)", text)
        self.assertIn("j DMAOut", text)

    def test_conditional_command_with_meaningful_slots(self):
        result = self.build(BRANCH)
        self.assertEqual(result.returncode, 0, result.stdout)
        first = self.output.read_bytes()
        self.assertIn(b"beq $5, $6, .Lrsp_", first)
        self.assertIn(b"j DMAOut", first)
        self.assertEqual(self.build(BRANCH).returncode, 0)
        self.assertEqual(self.output.read_bytes(), first)

    def test_word_loads_and_constant_scratch_offsets(self):
        body = """la %t2, rspq_scratch
    addiu %t2, %t2, 16
    li %r2, 15
    sw %r2, [%t2 - 16]
    lw %t0, [%t2 + -16]
    nop
    move %t1, %zero
    sw %zero, [%t2 - 12]
    sw %zero, [%t2 - 8]
    sw %zero, [%t2 - 4]
    addiu %s4, %t2, -16
    move %s0, %a3
    j DMAOut
    nop"""
        source = SCALAR[:SCALAR.index("command ::")] + "command :: asm() {\n" + body + "\n}\n"
        result = self.build(source)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("lw $8, -16($10)", self.output.read_text())

    def assert_rejected(self, source, diagnostic=None):
        self.output.unlink(missing_ok=True)
        result = self.build(source)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("command.odin", result.stdout)
        if diagnostic:
            self.assertIn(diagnostic, result.stdout)
        self.assertFalse(self.output.exists())

    def test_scalar_immediate_boundaries_and_operand_classes(self):
        for name, low, high in (("addiu", -32768, 32767), ("andi", 0, 65535),
                                ("ori", 0, 65535), ("xori", 0, 65535),
                                ("lui", 0, 65535), ("li", -2147483648, 4294967295)):
            for value in (low-1, low, high, high+1):
                with self.subTest(name=name, value=value):
                    args = f"%r2, {value}" if name in ("li", "lui") else f"%r2, %zero, {value}"
                    source = MINIMAL.replace("jr %ra", f"{name} {args}; jr %ra")
                    if low <= value <= high:
                        result = self.build(source)
                        self.assertEqual(result.returncode, 0, result.stdout)
                    else:
                        self.assert_rejected(source, "RSP integer")
        for instruction in ("addu %r2, %zero", "addu %r2, %zero, 1", "li %r2, 1, 2",
                            "andi %r2, %zero, %a0", "lui %r2, 1.5", "li %r2, (1 << 40)",
                            "move %r2, %v00", "li %r32, 0", "li %v0, 0", "li %v1, 0",
                            "li %r2.e0, 0", "li %r2, rspq_scratch",
                            "add.s %r2, %r0, %r0", "mult %r0, %r0", "mfhi %r2", "div %r0, %r0",
                            "daddu %r2, %r0, %r0", "ld %r2, [%r0]", "mfc0 %r2, %r0", "break"):
            with self.subTest(instruction=instruction):
                self.assert_rejected(MINIMAL.replace("jr %ra", instruction + "; jr %ra"))

    def test_only_declared_queue_inputs_are_defined(self):
        for words in (1, 2, 3, 4, 62):
            for reg in range(32):
                with self.subTest(words=words, reg=reg):
                    source = MINIMAL.replace("words=1", f"words={words}").replace(
                        "jr %ra", f"move %r2, %r{reg}; jr %ra")
                    live = reg in (0, 15, 28, 31) or 4 <= reg < 4 + min(words, 4)
                    if live:
                        result = self.build(source)
                        self.assertEqual(result.returncode, 0, result.stdout)
                    else:
                        self.assert_rejected(source)
        for reg in ("gp", "r28", "ra", "r31", "sp", "r29"):
            for instruction in (f"li %{reg}, 0", f"move %{reg}, %a0", f"addu %{reg}, %zero, %zero"):
                self.assert_rejected(MINIMAL.replace("jr %ra", instruction + "; jr %ra"))
        self.assert_rejected(MINIMAL.replace("jr %ra", "addu %zero, %t0, %zero; jr %ra"), "read before definition")
        self.assert_rejected(SCALAR.replace("%a1", "%v01"))

    def test_scratch_memory_forms_and_initialization_fail_closed(self):
        for memory in ("[%s4 + 2]", "[%s4 - 4]", "[%s4 + 16]", "[%s4 + 32768]",
                       "[%s4 - -32768]", "[%s4 + %a1]", "[%s4 + 4*2]", "[%s4 + 4 + 4]",
                       "[%zero:%s4]", "[%s4]:i32", "[%a0]", "[rspq_scratch]", "[%sp]", "%s4"):
            with self.subTest(memory=memory):
                self.assert_rejected(SCALAR.replace("[%s4]", memory))
        for source in (SCALAR.replace("rspq_scratch_bytes=16", "rspq_scratch_bytes=0"),
                       SCALAR.replace(", rspq_scratch_bytes=16", ""),
                       SCALAR.replace("rspq_scratch_bytes=16", "rspq_scratch_bytes=17"),
                       SCALAR.replace("rspq_scratch_bytes=16", "rspq_scratch_bytes=4112"),
                       SCALAR.replace("rspq_scratch_bytes=16", "rspq_scratch_bytes=-16"),
                       SCALAR.replace("la %s4, rspq_scratch", "la %s4, command"),
                       SCALAR.replace("la %s4, rspq_scratch", "la %s4, (rspq_scratch + 4)"),
                       SCALAR.replace("sw %r2, [%s4]", "lw %r2, [%s4]"),
                       SCALAR.replace("sw %r2, [%s4]", "sw %t6, [%s4]"),
                       SCALAR.replace("sw %r2, [%s4]", "lw %gp, [%s4 + 4]"),
                       SCALAR.replace("la %s4, rspq_scratch", "li %s4, 0"),
                       SCALAR.replace("la %s4, rspq_scratch", "la %s4, rspq_scratch; ori %s4, %s4, 0"),
                       SCALAR.replace("j DMAOut", "#bss; j DMAOut")):
            with self.subTest(source=source):
                self.assert_rejected(source)

    def test_dma_requires_initialized_extent_and_explicit_setup(self):
        for line in ("sw %r2, [%s4]", "sw %r3, [%s4 + 4]", "sw %zero, [%s4 + 8]",
                     "sw %zero, [%s4 + 12]", "move %s0, %a3", "li %t0, 15", "li %t1, 0"):
            with self.subTest(missing=line):
                self.assert_rejected(SCALAR.replace(line, ""))
        for replacement in ("li %t0, 0", "li %t0, 16", "li %t0, 23", "li %t0, 4096",
                            "li %t0, 0x100f", "move %t0, %a1"):
            self.assert_rejected(SCALAR.replace("li %t0, 15", replacement))
        for instruction in ("addiu %s4, %s4, 4", "addiu %s4, %s4, 8", "move %s4, %a1",
                            "li %s0, 3", "li %s0, 0x80000000", "move %s0, %s4"):
            self.assert_rejected(SCALAR.replace("j DMAOut", instruction + "; j DMAOut"))
        for transfer in ("jal DMAOut", "j DMAOutAsync", "j DMAExec", "j command", "jr %s0",
                         "j %s0", "halt", "break", "beq %a1, %a2, DMAOut"):
            self.assert_rejected(SCALAR.replace("j DMAOut", transfer))
        self.assert_rejected(SCALAR.replace("j DMAOut\n\tnop", "j DMAOut\n\tli %t0, 16"), "byte count")
        # Initializing only the first half permits exactly that fixed DMA extent.
        half = SCALAR.replace("sw %zero, [%s4 + 8]", "").replace("sw %zero, [%s4 + 12]", "").replace("li %t0, 15", "li %t0, 7")
        result = self.build(half)
        self.assertEqual(result.returncode, 0, result.stdout)

    def command_body(self, body, scratch=0):
        return MINIMAL.replace("words=1", f"words=1, rspq_scratch_bytes={scratch}").replace("jr %ra\n    nop", body)

    def test_local_forward_backward_and_scoped_labels(self):
        cases = (
            "j .done; nop; .done: jr %ra; addiu %r2, %zero, 1",
            ".entry: li %r2, 2; .loop: addiu %r2, %r2, -1; bne %r2, %zero, .loop; nop; jr %ra; nop",
            "beq %a0, %zero, .done; li %r2, 1; addiu %r2, %r2, 1; .done: jr %ra; move %r3, %r2",
            # A closed loop is safe at its exits; static checking does not prove termination.
            ".loop: j .loop; nop",
            # Local names cannot collide with wrapper symbols or unit constants.
            "j .rspq_scratch; nop; .rspq_scratch: j .DMAOut; nop; .DMAOut: jr %ra; nop",
        )
        for body in cases:
            with self.subTest(body=body):
                result = self.build(self.command_body(body))
                self.assertEqual(result.returncode, 0, result.stdout)

    def test_labels_and_delayed_transfers_fail_closed(self):
        cases = (
            (".same: nop; .same: jr %ra; nop", "Duplicate RSP label"),
            ("j .missing; nop", "Unresolved RSP label"),
            ("beq %a0, %zero, .end; nop; jr %ra; nop; .end:", "outside the command"),
            ("j rspq_scratch; nop", "scoped .label"),
            ("j command; nop", "scoped .label"),
            ("j 4; nop", "scoped .label"),
            ("j (.done + 2); nop; .done: jr %ra; nop", None),
            ("beq %a0, %zero, DMAOut; nop; jr %ra; nop", "scoped .label"),
            ("jr %ra", "explicit delay slot"),
            ("beq %a0, %zero, .entry; .entry:", "explicit delay slot"),
            ("j .end; jr %ra; .end: jr %ra; nop", "delay slot"),
            ("jr %ra; li %r2, 0x12345678", "multi-instruction expansion"),
            ("j .slot; .slot: nop; jr %ra; nop", "target a delay slot"),
            ("jal DMAOut; nop", "Unsupported"),
            ("jalr %ra, %r2; nop", "Unsupported"),
            ("jr %a0; nop", "inherited queue return"),
            ("j %a0; nop", "scoped .label"),
            ("nop", "fallthrough"),
            (".loop: bne %a0, %zero, .loop; nop", "fallthrough"),
            ("beql %a0, %zero, .done; nop; .done: jr %ra; nop", "Unsupported"),
        )
        for body, diagnostic in cases:
            with self.subTest(body=body):
                self.assert_rejected(self.command_body(body), diagnostic)

    def test_delay_reads_precede_transfer_and_joins_require_all_definitions(self):
        self.assert_rejected(self.command_body(
            "beq %r2, %zero, .done; li %r2, 1; .done: jr %ra; nop"), "read before definition")
        self.assert_rejected(self.command_body(
            "beq %a0, %zero, .done; nop; li %r2, 1; .done: jr %ra; move %r3, %r2"), "read before definition")
        self.assert_rejected(self.command_body(
            "li %r2, 1; .loop: move %r3, %t0; li %t0, 1; bne %r2, %zero, .loop; nop; jr %ra; nop"), "read before definition")
        for reg in ("gp", "r28", "ra", "r31", "sp", "r29"):
            for body in (f"jr %ra; li %{reg}, 0",
                         f"beq %a0, %zero, .done; nop; li %{reg}, 0; .done: jr %ra; nop",
                         f"j .done; nop; li %{reg}, 0; .done: jr %ra; nop"):
                with self.subTest(body=body):
                    self.assert_rejected(self.command_body(body))
        self.assert_rejected(self.command_body("jr %ra; move %r2, %sp"), "stack pointer")

    def test_scratch_join_and_memory_delay_slots(self):
        body = """la %s4, rspq_scratch
    beq %a0, %zero, .other
    sw %zero, [%s4]
    li %r2, 1
    j .join
    sw %r2, [%s4 + 4]
.other:
    li %r2, 2
    sw %r2, [%s4 + 4]
.join:
    lw %r3, [%s4 + 4]
    jr %ra
    lw %r2, [%s4]"""
        result = self.build(self.command_body(body, 16))
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assert_rejected(self.command_body(body.replace("sw %zero, [%s4]", "nop"), 16), "uninitialized scratch")
        self.assert_rejected(self.command_body(body.replace("li %r2, 2\n    sw %r2, [%s4 + 4]", "nop"), 16), "uninitialized scratch")
        self.assert_rejected(self.command_body(body.replace("li %r2, 2", "li %r2, 2; addiu %s4, %s4, 4"), 16), "statically known address")
        # Backedges must not retain a constant scratch address from only the first iteration.
        self.assert_rejected(self.command_body(
            "la %s4, rspq_scratch; .loop: sw %zero, [%s4]; addiu %s4, %s4, 4; bne %a0, %zero, .loop; nop; jr %ra; nop", 16), "statically known address")

    def test_dma_uses_post_slot_facts_on_every_path(self):
        for source in (SCALAR.replace("li %t1, 0", "").replace("j DMAOut\n\tnop", "j DMAOut\n\tli %t1, 0"),
                       SCALAR.replace("sw %zero, [%s4 + 12]", "").replace("j DMAOut\n\tnop", "j DMAOut\n\tsw %zero, [%s4 + 12]")):
            result = self.build(source)
            self.assertEqual(result.returncode, 0, result.stdout)
        self.assert_rejected(BRANCH.replace("li %t1, 0", "li %t0, 7"), "read before definition")
        self.assert_rejected(BRANCH.replace("li %t1, 0", "addiu %s4, %s4, 8"), "read before definition")
        # Different defined values remain initialized, but are no longer a known DMA size.
        source = SCALAR.replace("li %t0, 15", "beq %a1, %a2, .size; li %t0, 15; li %t0, 7; .size:")
        self.assert_rejected(source, "constant single-row")

    def test_dma_join_cannot_hide_a_known_invalid_destination(self):
        for invalid in ("li %s0, 3", "li %s0, 0x80000000", "move %s0, %s4"):
            for identity in ("nop", "ori %s0, %s0, 0", "addiu %s0, %s0, 0", "addu %s0, %zero, %s0"):
                for delayed in (False, True):
                    source = SCALAR.replace("move %s0, %a3",
                        f"beq %a1, %a2, .address; move %s0, %a3; {invalid}; .address:")
                    source = source.replace("j DMAOut\n\tnop", f"j DMAOut\n\t{identity}" if delayed else f"{identity}; j DMAOut\n\tnop")
                    with self.subTest(invalid=invalid, identity=identity, delayed=delayed):
                        self.assert_rejected(source, "physical RDRAM")

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

    def sdk(self):
        sdk = Path(os.environ.get("N64_INST", Path.home()/"n64_toolchain"))
        if not (sdk/"include/n64.mk").is_file():
            if os.environ.get("N64_VALIDATION_MODE") == "full" or "N64_INST" in os.environ:
                self.fail("Configured SDK is missing include/n64.mk")
            self.skipTest("set N64_INST for independent SDK encoding checks")
        self.run_tool([os.sys.executable, str(ROOT/"tests/o64_abi/validate_sdk.py"), str(sdk)])
        return sdk

    def sdk_command(self, sdk, source, target, *flags):
        return [str(sdk/"bin/mips64-elf-gcc"), "-march=mips1", "-mabi=32", "-Wa,--fatal-warnings",
                f"-I{sdk}/mips64-elf/include", f"-L{sdk}/mips64-elf/lib", "-nostartfiles",
                "-Wl,-Trsp.ld", "-Wl,--gc-sections", *flags, str(source), "-o", str(target)]

    def assert_sdk_command_matches_reference(self, sdk, source, reference_name):
        result = self.build(source)
        self.assertEqual(result.returncode, 0, result.stdout)
        for flags in ((), ("-DNDEBUG",), ("-DRSPQ_PROFILE=1",)):
            with self.subTest(flags=flags):
                elf, reference = self.path/"scalar.elf", self.path/"reference.elf"
                obj, ref_obj = self.path/"scalar.o", self.path/"reference.o"
                for source, linked, unlinked in ((self.output, elf, obj),
                        (ROOT/"tests/rsp_asm"/reference_name, reference, ref_obj)):
                    self.run_tool(self.sdk_command(sdk, source, linked, *flags))
                    self.run_tool(self.sdk_command(sdk, source, unlinked, "-c", *flags))
                for section in (".text", ".data"):
                    self.assertEqual(self.sdk_section(sdk, obj, section), self.sdk_section(sdk, ref_obj, section))
                    self.assertEqual(self.sdk_section(sdk, elf, section), self.sdk_section(sdk, reference, section))
                relocations = [self.sdk_relocations(sdk, artifact) for artifact in (obj, ref_obj)]
                self.assertEqual(*relocations)
                self.assertTrue(any("R_MIPS_LO16" in line and ".bss" in line for line in relocations[0]))
                self.assertIn("There are no relocations", self.run_tool([str(sdk/"bin/mips64-elf-readelf"), "-r", str(elf)]))
                symbols = self.sdk_symbols(sdk, elf)
                reference_symbols = self.sdk_symbols(sdk, reference)
                scratch = symbols["rspq_scratch"]
                self.assertEqual(scratch, reference_symbols["rspq_scratch"])
                self.assertEqual(scratch % 16, 0)
                self.assertGreaterEqual(scratch, symbols["_data_end"])
                self.assertLessEqual(scratch + 16, 0xa4001000)
                self.assertLessEqual(symbols["_text_end"], 0xa4002000)
                self.assertEqual(self.sdk_section_size(sdk, elf, ".bss"), 16)

    def test_sdk_scalar_words_relocations_and_scratch_bounds(self):
        sdk = self.sdk()
        self.assert_sdk_command_matches_reference(sdk, SCALAR, "scalar-reference.S")
        # Allocations include the SDK prefixes, header, saved state and padding.
        for source in (SCALAR.replace("scratch_bytes=16", "scratch_bytes=4096"),
                       MINIMAL.replace("jr %ra", "nop; " * 1024 + "jr %ra")):
            self.assertEqual(self.build(source).returncode, 0)
            result = subprocess.run(self.sdk_command(sdk, self.output, self.path/"overflow.elf"),
                                    text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn("will not fit in region", result.stdout)

    def test_sdk_conditional_words_targets_and_relocations(self):
        sdk = self.sdk()
        self.assert_sdk_command_matches_reference(sdk, BRANCH, "branch-reference.S")
        elf = self.path/"scalar.elf"
        symbols = self.sdk_symbols(sdk, elf)
        entry = symbols["rsp_command"]
        offset = entry - symbols["_text_start"]
        self.assertEqual(offset % 4, 0)
        self.assertTrue(0 <= offset <= 4092)
        words = struct.unpack(">16I", self.sdk_section(sdk, elf, ".text")[offset:offset+64])
        self.assertEqual(words[1], 0x10a60003)  # BEQ: target is instruction 5.
        self.assertEqual(words[2], 0x24030001)  # Meaningful slot: r3 = 1 on both paths.
        self.assertEqual(words[3] >> 26, 2)    # J: target is instruction 7.
        self.assertEqual((words[3] & 0x3ffffff) * 4, (entry+28) & 0xfffffff)
        self.assertEqual(words[4], 0x00431021) # Jump slot adds the independently defined 1.
        self.assertEqual((words[14] & 0x3ffffff) * 4, symbols["DMAOut"] & 0xfffffff)
        self.assertEqual(words[15], 0x24090000)
        data = self.sdk_section(sdk, elf, ".data")
        table = symbols["_RSPQ_OVERLAY_COMMAND_TABLE"] - symbols["_data_start"]
        descriptor, terminator = struct.unpack_from(">HH", data, table)
        self.assertEqual(descriptor, (4 << 10) | (offset // 4))
        self.assertEqual(terminator, 0)

    def test_sdk_backward_branch_and_expanded_pseudo_offsets(self):
        sdk = self.sdk()
        body = "li %r2, 2; .loop: li %r3, 0x12345678; addiu %r2, %r2, -1; bne %r2, %zero, .loop; ori %r3, %zero, 7; jr %ra; nop"
        result = self.build(self.command_body(body))
        self.assertEqual(result.returncode, 0, result.stdout)
        elf = self.path/"backward.elf"
        self.run_tool(self.sdk_command(sdk, self.output, elf))
        symbols = self.sdk_symbols(sdk, elf)
        offset = symbols["rsp_command"] - symbols["_text_start"]
        expected = bytes.fromhex("24020002 3c031234 34635678 2442ffff 1440fffc 34030007 03e00008 00000000")
        self.assertEqual(self.sdk_section(sdk, elf, ".text")[offset:offset+len(expected)], expected)

    def test_branch_displacement_limits_are_source_located(self):
        # The ISA range is wider than IMEM. Frontend emission checks encodability;
        # SDK linking separately enforces the physical 4 KiB budget.
        for count, valid in ((32766, True), (32767, False)):
            source = self.command_body("beq %a0, %zero, .end; nop; " + "nop\n" * count + ".end: jr %ra; nop")
            if valid:
                result = self.build(source)
                self.assertEqual(result.returncode, 0, result.stdout)
            else:
                self.assert_rejected(source, "branch displacement")
        for count, valid in ((32767, True), (32768, False)):
            source = self.command_body(".start: " + "nop\n" * count + "bne %a0, %zero, .start; nop; jr %ra; nop")
            if valid:
                result = self.build(source)
                self.assertEqual(result.returncode, 0, result.stdout)
            else:
                self.assert_rejected(source, "branch displacement")

    def test_sdk_rejects_misaligned_targets_and_bad_entry_with_source_diagnostics(self):
        sdk = self.sdk()
        self.assertEqual(self.build(BRANCH).returncode, 0)
        original = self.output.read_text()
        mutations = (
            original.replace(".Lrsp_5:", ".byte 0\n.Lrsp_5:"),
            original.replace("rsp_command:\n", ".byte 0\nrsp_command:\n"),
            original.replace("rsp_command:\n", ".space 4096\nrsp_command:\n"),
        )
        for source in mutations:
            with self.subTest(source=source):
                self.output.write_text(source)
                result = subprocess.run(self.sdk_command(sdk, self.output, self.path/"bad.elf"),
                                        text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("command.odin:", result.stdout)
                self.assertIn("RSP", result.stdout)

    def test_sdk_every_scalar_form_and_literal_expansion(self):
        sdk = self.sdk()
        # Reviewed real instruction words, independent of the compiler checker/emitter.
        cases = [
            ("li %r2, -32768", "24028000"), ("li %r3, 32767", "24037fff"),
            ("li %r4, 65535", "3404ffff"), ("li %r5, 65536", "3c050001 34a50000"),
            ("li %r6, -2147483648", "3c068000 34c60000"),
            ("li %r7, 0xffffffff", "3c07ffff 34e7ffff"),
            ("li %at, 0x12345678", "3c011234 34215678"),
            ("lui %t0, 65535", "3c08ffff"), ("move %r3, %r2", "00401821"),
            ("addu %t0, %r2, %r3", "00434021"), ("addiu %t0, %t0, -32768", "25088000"),
            ("andi %t1, %t0, 65535", "3109ffff"), ("ori %t2, %zero, 65535", "340affff"),
            ("xori %t3, %t1, 0", "392b0000"), ("and %t4, %t2, %t3", "014b6024"),
            ("or %t5, %t4, %r2", "01826825"), ("xor %t6, %t5, %a0", "01a47026"),
        ]
        body = "; ".join(instruction for instruction, _ in cases)
        memory = "la %s4, rspq_scratch; sw %r2, [%s4 + 4]; lw %t0, [%s4 + 4]; nop; "
        result = self.build(MINIMAL.replace("words=1", "words=1, rspq_scratch_bytes=16").replace("jr %ra", body + "; " + memory + "jr %ra"))
        self.assertEqual(result.returncode, 0, result.stdout)
        elf = self.path/"forms.elf"
        self.run_tool(self.sdk_command(sdk, self.output, elf))
        symbols = self.sdk_symbols(sdk, elf)
        offset = symbols["rsp_command"] - symbols["_text_start"]
        text = self.sdk_section(sdk, elf, ".text")[offset:]
        expected = bytes.fromhex(" ".join(words for _, words in cases))
        expected += struct.pack(">I", 0x34140000 | (symbols["rspq_scratch"] & 0xffff))
        expected += bytes.fromhex("ae820004 8e880004 00000000 03e00008 00000000")
        self.assertEqual(text[:len(expected)], expected)

    def test_sdk_return_words_and_wrapper(self):
        sdk = self.sdk()
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

    def sdk_relocations(self, sdk, artifact):
        listing = self.run_tool([str(sdk/"bin/mips64-elf-objdump"), "-r", str(artifact)])
        return [line for line in listing.splitlines() if "R_MIPS_" in line]

    def sdk_section_size(self, sdk, elf, section):
        listing = self.run_tool([str(sdk/"bin/mips64-elf-objdump"), "-h", str(elf)])
        fields = next(line.split() for line in listing.splitlines() if f" {section} " in line)
        return int(fields[2], 16)

    def sdk_symbols(self, sdk, elf):
        listing = self.run_tool([str(sdk/"bin/mips64-elf-nm"), "--defined-only", str(elf)])
        return {name: int(address, 16) for address, kind, name in (line.split() for line in listing.splitlines())}

if __name__ == "__main__":
    unittest.main()
