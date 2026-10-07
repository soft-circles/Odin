#!/usr/bin/env python3
"""SDK-free checks for the generic asm-template checker on amd64, arm64 and riscv64.

Packages under tests/asm_check/ (each file carries a #+build tag for its architecture):
  valid/       templates every target must accept under -vet; their LLVM IR is
               inspected for the clobber list (no "<reg>" placeholder)
  bad/         checker rejections, one template per line
  llvm_error/  an amd64 template LLVM rejects while emitting the object

`odin check` works with any compiler. IR and object builds need the compiler's LLVM
to have the target; a fork built against a MIPS-only LLVM does not, and those tests
are skipped. Run with:

    ODIN=$PWD/odin python3 -I tests/asm_check/test_asm_check.py
"""
import functools
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
ODIN = Path(os.environ.get("ODIN", ROOT / "odin")).resolve()
TARGETS = ("linux_amd64", "linux_arm64", "linux_riscv64")
DIAGNOSTIC = re.compile(r"^(.*)\((\d+):(\d+)\) (Error|Warning): (.*?)\s*$")

_scratch = None


def scratch_dir():
    global _scratch
    if _scratch is None:
        _scratch = tempfile.TemporaryDirectory(prefix="odin-asm-check-")
    return Path(_scratch.name)


def tearDownModule():
    if _scratch is not None:
        _scratch.cleanup()


def run_odin(*args):
    """Run the compiler; return (returncode, output with ANSI colours removed)."""
    env = dict(os.environ)
    env.setdefault("ODIN_ROOT", str(ROOT))
    result = subprocess.run([str(ODIN), *map(str, args)], capture_output=True, text=True, env=env)
    return result.returncode, re.sub(r"\x1b\[[0-9;]*m", "", result.stdout + result.stderr)


def unique_dir(stem):
    return Path(tempfile.mkdtemp(prefix=f"{stem}-", dir=scratch_dir()))


@functools.lru_cache(maxsize=None)
def check_package(package, target, *flags):
    return run_odin("check", HERE / package, f"-target:{target}", "-no-entry-point",
                    "-max-error-count:1000", *flags)


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


@functools.lru_cache(maxsize=None)
def build_ir(package, target):
    """LLVM IR text for a package; a single-module build writes `<dir>/.ll`."""
    directory = unique_dir(f"{package}-{target}-ir")
    code, output = run_odin("build", HERE / package, f"-target:{target}", "-build-mode:llvm-ir",
                            "-no-entry-point", "-use-single-module", f"-out:{directory}")
    if code != 0:
        raise AssertionError(f"IR for {package} ({target}) failed:\n{output}")
    return "\n".join(p.read_text() for p in directory.glob("*.ll") if p.is_file()) + \
        ((directory / ".ll").read_text() if (directory / ".ll").exists() else "")


def asm_calls(ir):
    """{exported proc name: [constraint string of each inline asm call in its body]}."""
    calls = {}
    current = None
    for line in ir.splitlines():
        m = re.match(r'^define .*@"?([\w.]+)"?\(', line)
        if m:
            current = m.group(1)
            calls[current] = []
        elif line.startswith("}"):
            current = None
        elif current is not None:
            m = re.search(r'asm (?:sideeffect )?(?:alignstack )?"(?:[^"\\]|\\.)*", "([^"]*)"', line)
            if m:
                calls[current].append(m.group(1))
    return calls


def diagnostics(output):
    """[(file name, line, kind, message, detail lines)] for every error/warning."""
    found = []
    for raw in output.splitlines():
        m = DIAGNOSTIC.match(raw)
        if m:
            found.append((Path(m.group(1)).name, int(m.group(2)), m.group(4), m.group(5), []))
        elif found:
            found[-1][4].append(raw.strip())
    return found


def template_lines(path):
    """{template name: line} for `name :: asm(...)` declarations in a fixture file."""
    lines = {}
    for number, text in enumerate(Path(path).read_text().splitlines(), 1):
        m = re.match(r"^(\w+)\s*::\s*asm\b", text)
        if m:
            lines[m.group(1)] = number
    return lines


def input_write(mnemonic, name, output="r"):
    return (f"'{mnemonic}' writes the input parameter '{name}'; the compiler assumes an input keeps "
            f"its value. Tie it to an output ('{name} -> {output}') or copy it to a scratch register first")


def literal_input_write(mnemonic, reg, name):
    return (f"'{mnemonic}' writes %{reg}, which holds the input '{name}'; the compiler assumes an input "
            f"keeps its value, so tie '{name}' to an output or copy it into a scratch register before writing")


# Checker rejections in bad/<arch>.odin: template name -> message (verbatim prefix).
BAD = {
    "linux_amd64": ("amd64.odin", {
        "write_input":         input_write("add", "a"),
        "write_pinned_input":  input_write("add", "a"),
        "write_input_literal": literal_input_write("add", "r12", "a"),
        "write_input_view":    ("'mov' writes 'ab', a view of the input parameter 'a'; the compiler assumes an "
                                "input keeps its value. Tie it to an output ('a -> r') or copy it to a scratch "
                                "register first"),
        "zero_input":          input_write("xor", "a"),
        "write_input_no_out":  input_write("add", "a", "<output>"),
        "register_typo":       "Unknown register for this target platform: %rbxx",
    }),
    "linux_arm64": ("arm64.odin", {
        "write_input":         input_write("add", "a"),
        "write_input_literal": literal_input_write("add", "x9", "a"),
        "post_index_input":    input_write("ldr", "p"),
        "failed_operand":      "Unknown register for this target platform: %bogus",
        "output_pin_sp":       ("'asm' output 'r' is pinned to a register '%sp' which cannot be an output; "
                                "copy it into the output in the body instead"),
    }),
    "linux_riscv64": ("riscv64.odin", {
        "write_input":         input_write("addi", "a"),
        "write_input_literal": literal_input_write("addi", "t1", "a"),
        "mv_three_operands":   "The asm instruction 'mv' expects 2 operands, got 3",
    }),
}

# Exported proc in valid/ -> the register its #clobber must name in the LLVM constraint.
UNTRACKED_CLOBBERS = {
    "linux_amd64":   ("amd64_copy_via_r12",   "~{r12}"),
    "linux_arm64":   ("arm64_copy_via_x9",    "~{x9}"),
    "linux_riscv64": ("riscv64_copy_via_t0",  "~{t0}"),
}


class Check(unittest.TestCase):
    def test_valid_templates_check_under_vet(self):
        # covers: riscv64 mv/not/nop operand counts; a pinned input read implicitly (cqo)
        # or through a register whose code needs more than 16 bits (arm64 %daif) is a use;
        # a literal write to a pinned output assigns it; tied inputs and scratch registers
        # may be written.
        for target in TARGETS:
            with self.subTest(target=target):
                code, output = check_package("valid", target, "-vet", "-strict-style")
                self.assertEqual(code, 0, output)
                self.assertEqual(diagnostics(output), [])

    def test_rejections(self):
        for target, (file, expected) in BAD.items():
            with self.subTest(target=target):
                code, output = check_package("bad", target)
                self.assertNotEqual(code, 0)
                lines = template_lines(HERE / "bad" / file)
                errors = [d for d in diagnostics(output) if d[2] == "Error"]
                for template, message in expected.items():
                    with self.subTest(template=template):
                        at_line = [m for f, l, _, m, _ in errors if f == file and l == lines[template]]
                        self.assertTrue(any(m.startswith(message) for m in at_line), f"got {at_line}")
                expected_lines = {lines[t] for t in expected}
                self.assertEqual([(l, m) for f, l, _, m, _ in errors if l not in expected_lines], [])

    def test_one_error_per_failed_operand(self):
        # A failed operand no longer cascades into "invalid kind" / "nearly matched" errors,
        # nor into a definite-assignment error for the output the instruction would set.
        _, output = check_package("bad", "linux_arm64")
        line = template_lines(HERE / "bad" / "arm64.odin")["failed_operand"]
        at_line = [m for f, l, k, m, _ in diagnostics(output) if f == "arm64.odin" and l == line]
        self.assertEqual(at_line, ["Unknown register for this target platform: %bogus"])

    def test_register_did_you_mean_keeps_the_percent_prefix(self):
        _, output = check_package("bad", "linux_amd64")
        line = template_lines(HERE / "bad" / "amd64.odin")["register_typo"]
        details = [d for f, l, _, _, d in diagnostics(output) if f == "amd64.odin" and l == line]
        self.assertTrue(details and "%rbx" in details[0], f"got {details}")

    def test_llvm_error_package_checks(self):
        code, output = check_package("llvm_error", "linux_amd64", "-vet")
        self.assertEqual(code, 0, output)


class Backend(unittest.TestCase):
    def test_untracked_clobber_has_no_placeholder(self):
        for target, (proc, clobber) in UNTRACKED_CLOBBERS.items():
            with self.subTest(target=target):
                require_backend(self, target)
                ir = build_ir("valid", target)
                self.assertNotIn("<reg>", ir)
                constraints = asm_calls(ir).get(proc)
                self.assertTrue(constraints, f"no inline asm found in {proc}")
                self.assertIn(clobber, constraints[0].split(","))

    def test_riscv64_pseudo_instructions_lower(self):
        require_backend(self, "linux_riscv64")
        ir = build_ir("valid", "linux_riscv64")
        self.assertIn('asm "\\09mv $0, $1", "=r,r"', ir)
        self.assertIn('asm "\\09not $0, $1", "=r,r"', ir)

    def test_llvm_error_fails_the_build(self):
        # The non-threaded object path (-use-single-module) printed LLVM's error and exited 0.
        require_backend(self, "darwin_amd64")
        for flags in ((), ("-use-single-module",)):
            with self.subTest(flags=flags):
                out = unique_dir("llvm-error") / "out.o"
                code, output = run_odin("build", HERE / "llvm_error", "-target:darwin_amd64",
                                        "-build-mode:obj", "-no-entry-point", *flags, f"-out:{out}")
                self.assertIn("inline assembly requires more registers than available", output)
                self.assertNotEqual(code, 0, output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
