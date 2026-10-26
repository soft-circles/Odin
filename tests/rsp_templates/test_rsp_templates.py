#!/usr/bin/env python3
"""Checks for @(rsp) asm templates (RSP Microcode) and intrinsics.rsp_template_code.

Packages under tests/rsp_templates/:
  valid/   templates every target accepts under -vet -strict-style, including
           Odin64's rsp_jobs/transform body written out flat (valid/transform)
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
        printed = {}
        for row in output.splitlines():
            parts = row.split()
            if len(parts) == 4 and parts[0] in lines:
                printed.setdefault(parts[0], []).append(int(parts[3]))
        for name in ("scalar_forms", "vector_forms", "transform"):
            with self.subTest(template=name):
                self.assertEqual(printed.get(name), lines[name])

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
