#!/usr/bin/env python3
"""Checks for @(rsp) asm templates (RSP Microcode) and intrinsics.rsp_template_code.

Packages under tests/rsp_templates/:
  valid/   templates every target accepts under -vet -strict-style, including
           Odin64's rsp_jobs/transform body written out flat and with `#unroll for`
           (valid/transform)
  bad/     rejections; a line that must report an error ends with
           `// ERROR: <message prefix>`
  golden/  a host program that encodes every form of src/asm_tables_rsp.cpp
           through both the compiler and core:rexcode/isa/rsp and diffs the words

`odin check` works with any compiler. The golden program and the byte-order IR
checks need the compiler's LLVM to have the host and target backends; a fork built
against a MIPS-only LLVM has neither, and those tests are skipped. Run with:

    ODIN=$PWD/odin python3 -I tests/rsp_templates/test_rsp_templates.py
"""
import functools
import os
from pathlib import Path
import platform
import re
import subprocess
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
ODIN = Path(os.environ.get("ODIN", ROOT / "odin")).resolve()
DIAGNOSTIC = re.compile(r"^(.*)\((\d+):(\d+)\) (Error|Warning): (.*?)\s*$")
MARKER = re.compile(r"// ERROR: (.*?)\s*$")

# An @(rsp) template is checked for the RSP whatever the build targets: hosts with
# and without a CPU asm table of their own, and the N64 itself.
TARGETS = ("linux_amd64", "linux_arm64", "darwin_arm64", "linux_riscv64", "windows_amd64",
           "linux_i386", "freestanding_wasm32", "n64")

_scratch = None


def scratch_dir():
    global _scratch
    if _scratch is None:
        _scratch = tempfile.TemporaryDirectory(prefix="odin-rsp-templates-")
    return Path(_scratch.name)


def tearDownModule():
    if _scratch is not None:
        _scratch.cleanup()


def unique_dir(stem):
    return Path(tempfile.mkdtemp(prefix=f"{stem}-", dir=scratch_dir()))


def run_odin(*args):
    """Run the compiler; return (returncode, output with ANSI colours removed)."""
    env = dict(os.environ)
    env.setdefault("ODIN_ROOT", str(ROOT))
    result = subprocess.run([str(ODIN), *map(str, args)], capture_output=True, text=True, env=env)
    return result.returncode, re.sub(r"\x1b\[[0-9;]*m", "", result.stdout + result.stderr)


def check_package(package, target, *flags):
    return run_odin("check", package, f"-target:{target}", "-no-entry-point", "-max-error-count:1000", *flags)


def diagnostics(output):
    """[(file name, line, kind, message)] for every error/warning."""
    found = []
    for raw in output.splitlines():
        m = DIAGNOSTIC.match(raw)
        if m:
            found.append((Path(m.group(1)).name, int(m.group(2)), m.group(4), m.group(5)))
    return found


def host_target():
    machine = platform.machine().lower()
    arch = {"x86_64": "amd64", "amd64": "amd64", "arm64": "arm64", "aarch64": "arm64",
            "riscv64": "riscv64"}.get(machine, machine)
    system = {"Darwin": "darwin", "Linux": "linux", "Windows": "windows",
              "FreeBSD": "freebsd", "NetBSD": "netbsd", "OpenBSD": "openbsd"}.get(platform.system(), "")
    return f"{system}_{arch}"


@functools.lru_cache(maxsize=None)
def target_has_backend(target):
    """Whether the compiler's LLVM can emit code for `target` (a MIPS-only fork cannot)."""
    directory = unique_dir(f"probe-{target}")
    (directory / "probe.odin").write_text("package probe\n\n@(export) probe :: proc \"contextless\" () {}\n")
    code, _ = run_odin("build", directory, f"-target:{target}", "-build-mode:llvm-ir",
                       "-no-entry-point", "-use-single-module", f"-out:{unique_dir('probe-out')}")
    return code == 0


def require_backend(test, target):
    if not target_has_backend(target):
        test.skipTest(f"{ODIN.name} has no LLVM backend for {target}")


def template_instruction_lines(path):
    """{template name: [line of each instruction]} for the `@(rsp)` templates in a file."""
    templates = {}
    current = None
    for number, text in enumerate(Path(path).read_text().splitlines(), 1):
        m = re.match(r"^(\w+)\s*::\s*asm\b.*\{\s*$", text)
        if m:
            current = templates.setdefault(m.group(1), [])
            continue
        if current is None:
            continue
        stripped = text.split("//")[0].strip()
        if stripped == "}":
            current = None
        elif stripped and not stripped.startswith("."):
            current.append(number)
    return templates


class Check(unittest.TestCase):
    def test_valid_templates_check_on_every_target(self):
        for target in TARGETS:
            with self.subTest(target=target):
                code, output = check_package(HERE / "valid", target, "-vet", "-strict-style")
                self.assertEqual(code, 0, output)
                self.assertEqual(diagnostics(output), [])

    def test_rejections(self):
        expected = {}
        for number, text in enumerate((HERE / "bad" / "bad.odin").read_text().splitlines(), 1):
            m = MARKER.search(text)
            if m and not text.lstrip().startswith("//"):
                expected[number] = m.group(1)
        self.assertGreater(len(expected), 40)
        for target in TARGETS:
            with self.subTest(target=target):
                code, output = check_package(HERE / "bad", target)
                self.assertNotEqual(code, 0)
                found = diagnostics(output)
                for line, message in expected.items():
                    at_line = [m for f, l, k, m in found if f == "bad.odin" and l == line and k == "Error"]
                    with self.subTest(line=line):
                        self.assertTrue(any(m.startswith(message) for m in at_line),
                                        f"line {line}: want '{message}', got {at_line}")
                stray = [d for d in found if d[1] not in expected]
                self.assertEqual(stray, [], "diagnostics on lines with no ERROR marker")

    def test_branch_range_is_checked(self):
        # bgtz reaches -32768..=32767 instructions from its delay slot
        for gap, ok in ((32767, True), (32768, False)):
            with self.subTest(gap=gap):
                directory = unique_dir(f"branch-{gap}")
                body = "\n".join(["\tbgtz  %t0, .far"] + ["\tnop"] * gap + [".far:", "\tnop"])
                (directory / "branch.odin").write_text(
                    "package branch\n\nimport \"base:intrinsics\"\n\n"
                    f"@(rsp)\nfar :: asm() {{\n{body}\n}}\n\n"
                    "FAR :: intrinsics.rsp_template_code(far)\n")
                code, output = check_package(directory, host_target())
                if ok:
                    self.assertEqual(code, 0, output)
                else:
                    self.assertNotEqual(code, 0)
                    self.assertIn("'bgtz' branches 32768 instructions, beyond the -32768..=32767 a branch reaches",
                                  output)


    def test_backward_branch_range_is_checked(self):
        # a branch back to the first instruction from index n reaches -(n+1) instructions
        for gap, ok in ((32767, True), (32768, False)):
            with self.subTest(gap=gap):
                directory = unique_dir(f"back-{gap}")
                body = "\n".join([".far:"] + ["\tnop"] * gap + ["\tbgtz  %s1, .far", "\tnop"])
                (directory / "branch.odin").write_text(
                    "package branch\n\nimport \"base:intrinsics\"\nimport \"core:fmt\"\n\n"
                    f"@(rsp)\nfar :: asm() {{\n{body}\n}}\n\n"
                    "FAR :: intrinsics.rsp_template_code(far)\n\n"
                    "main :: proc() {\n\tcode := FAR\n\tfmt.printfln(\"%08x\", code[len(code)-2].word)\n}\n")
                code, output = run_odin("check", directory, f"-target:{host_target()}")
                if not ok:
                    self.assertNotEqual(code, 0)
                    self.assertIn("'bgtz' branches -32769 instructions, beyond the -32768..=32767 a branch reaches",
                                  output)
                    continue
                self.assertEqual(code, 0, output)
                if target_has_backend(host_target()):
                    # bgtz $s1 with offset 0x8000: -32768 words
                    code, output = run_odin("run", directory, f"-out:{unique_dir('back-out') / 'branch'}")
                    self.assertEqual(code, 0, output)
                    self.assertEqual(output.strip().splitlines()[-1], "1e208000")

    def test_mutual_reference_is_reported_once(self):
        # Whichever of a and b is checked first, the other's use of it is the one error,
        # and nothing repeats it or reports "already pinned".
        directory = unique_dir("mutual")
        (directory / "mutual.odin").write_text(
            "package mutual\n\nimport \"base:intrinsics\"\n\n"
            "@(rsp)\na :: asm(input: ^u32) [input = %s1] {\n"
            "\taddiu input, input, (len(intrinsics.rsp_template_code(b)))\n}\n\n"
            "@(rsp)\nb :: asm(input: ^u32) [input = %s1] {\n"
            "\taddiu input, input, (len(intrinsics.rsp_template_code(a)))\n}\n\n"
            "A :: intrinsics.rsp_template_code(a)\n")
        for target in ("n64", host_target()):
            with self.subTest(target=target):
                code, output = check_package(directory, target)
                self.assertNotEqual(code, 0)
                found = diagnostics(output)
                cycle = [d for d in found if "refers to the template being checked" in d[3]]
                self.assertEqual(len(cycle), 1, output)
                self.assertIn(cycle[0][1], (7, 12))
                self.assertIn(cycle[0][3], ("'intrinsics.rsp_template_code(a)' refers to the template being checked; "
                                            "an @(rsp) template's words cannot depend on its own",
                                            "'intrinsics.rsp_template_code(b)' refers to the template being checked; "
                                            "an @(rsp) template's words cannot depend on its own"))
                self.assertEqual({d[1] for d in found}, {7, 12}, output)  # the uses' own errors: len is not constant
                self.assertNotIn("pinned", output)
                self.assertNotIn("Invalid declaration value", output)

    def test_cpu_register_flag_is_reported_once(self):
        # The @(rsp) accessor check runs only for a flag the table accepted, so an amd64
        # template's bad flag keeps its one error.
        directory = unique_dir("flag")
        (directory / "flag.odin").write_text("package flag\n\nt :: asm() {\n\tsetz %rcx.zf\n}\n")
        code, output = check_package(directory, "linux_amd64")
        self.assertNotEqual(code, 0)
        at_flag = [d[3] for d in diagnostics(output)
                   if d[1] == 4 and (".zf" in d[3] or "register flag" in d[3].lower())]
        self.assertEqual(at_flag, ["Register flags can only be called on %flags"], output)

    def test_unroll_is_rsp_only(self):
        # A CPU template's body is lowered as written, so `#unroll for` is an error there.
        directory = unique_dir("cpu-unroll")
        (directory / "cpu.odin").write_text(
            "package cpu\n\nt :: asm() {\n\t#unroll for i in 0..<2 {\n\t\tnop\n\t}\n}\n")
        code, output = check_package(directory, "linux_amd64")
        self.assertNotEqual(code, 0)
        self.assertEqual([(d[1], d[3]) for d in diagnostics(output)],
                         [(4, "'#unroll for' in an asm template body is supported only in @(rsp) templates")], output)


class Encode(unittest.TestCase):
    def test_every_form_matches_rexcode(self):
        target = host_target()
        require_backend(self, target)
        out = unique_dir("golden") / "golden"
        code, output = run_odin("run", HERE / "golden", "-vet", "-strict-style", f"-out:{out}")
        self.assertEqual(code, 0, output)
        self.assertEqual(output.strip().splitlines()[-1], "ok", output)

        lines = template_instruction_lines(HERE / "golden" / "golden.odin")
        lines.update(template_instruction_lines(HERE / "valid" / "transform" / "transform.odin"))
        lines.update(template_instruction_lines(HERE / "valid" / "valid.odin"))
        printed = {}
        for row in output.splitlines():
            parts = row.split()
            if len(parts) == 4 and parts[0] in lines:
                printed.setdefault(parts[0], []).append(int(parts[3]))
        for name in ("scalar_forms", "edge_forms", "fill", "vector_forms", "transform"):
            with self.subTest(template=name):
                self.assertEqual(printed.get(name), lines[name])

        # Each word of the unrolled transform is from the line, inside the unrolled
        # body, of the same instruction as the flat template's word.
        flat_path = HERE / "valid" / "transform" / "transform.odin"
        unrolled_path = HERE / "valid" / "transform" / "transform_unrolled.odin"
        flat_text = flat_path.read_text().splitlines()
        unrolled_text = unrolled_path.read_text().splitlines()
        unrolled_lines = [int(row.split()[3]) for row in output.splitlines()
                          if len(row.split()) == 4 and row.split()[0] == "transform_unrolled"]
        self.assertEqual(len(unrolled_lines), len(lines["transform"]))
        for i, (flat, unrolled) in enumerate(zip(lines["transform"], unrolled_lines)):
            with self.subTest(transform_unrolled=i):
                self.assertEqual(unrolled_text[unrolled - 1].split()[0], flat_text[flat - 1].split()[0])
        self.assertEqual(len(set(unrolled_lines)), 28)  # 61 words from 28 written instructions

        # intrinsics.rsp_template_location: the declaration's file, line and column
        locations = {}
        for row in output.splitlines():
            parts = row.split(" ", 4)
            if len(parts) == 5 and parts[0] == "location":
                locations[parts[1]] = (int(parts[2]), int(parts[3]), Path(parts[4]).resolve())
        for name, path in (("transform", HERE / "valid" / "transform" / "transform.odin"),
                           ("fill", HERE / "valid" / "valid.odin")):
            with self.subTest(location=name):
                declared = [n for n, text in enumerate(path.read_text().splitlines(), 1)
                            if text.startswith(f"{name} :: asm")]
                self.assertEqual(locations.get(name), (declared[0], 1, path.resolve()))

    def test_constant_is_in_the_target_byte_order(self):
        # transform's first word is `lh $t0, 0($s6)`, 0x86c80000, from line 17
        for target, word, line in (("n64", r"\86\C8\00\00", r"\00\00\00\11"),
                                   ("linux_amd64", r"\00\00\C8\86", r"\11\00\00\00")):
            with self.subTest(target=target):
                require_backend(self, target)
                directory = unique_dir(f"ir-{target}")
                code, output = run_odin("build", HERE / "valid", f"-target:{target}", "-build-mode:llvm-ir",
                                        "-no-entry-point", "-use-single-module", f"-out:{directory}")
                self.assertEqual(code, 0, output)
                ir = "".join(p.read_text() for p in directory.glob("*.ll"))
                self.assertIn(f'c"{word}{line}', ir)


if __name__ == "__main__":
    unittest.main(verbosity=2)
