# Ship one issue as a PR

The flow for a session a coordinator started to ship one soft-circles/Odin issue. Each step ends on its gate; fix a red
gate before the next step. `fork` is soft-circles/Odin and `origin` is upstream odin-lang/Odin: push to `fork`, and pass
`-R soft-circles/Odin` to every `gh` command.

## Sub-agents

Spawn subagents for exploration and research with the Agent tool and `model: "sonnet"`: `Explore` to find code in the
repo, `general-purpose` for research that needs the web or several steps. Brief each with the question and what you have
ruled out, and keep its conclusion rather than its file dumps. Step 5's reviews run on your own model.

## Handing off at 30% context

Read your context with `get_usage` at each step's done line and after each fix you push. Once it reports 30% or more,
hand off before starting more work:

1. Commit everything, half-done work included, and push it to your branch on `fork`.
2. Send the coordinator (`send_message` to `parent`) a message that starts with `HANDOFF` and gives the branch, the
   pushed SHA, the PR URL if there is one, your context percent, the step of this flow you are on, what is done, what is
   left, the decisions you made and anything that failed.
3. End the turn with the recap `HANDOFF <pushed SHA>` and do no more work; the coordinator starts a session to continue.

A session started to continue another's work pushes with `git push fork HEAD:<branch>` to the branch its brief names,
opens the PR with `--head <branch>` when none exists, and hands off the same way.

## 1. Pin the base

The base is the branch the coordinator names, usually `n64`. Run `git fetch fork <base>`; a branch with no work yet
starts from `fork/<base>` (`git reset --hard fork/<base>`).

Done when `git diff fork/<base>...HEAD` holds only this branch's work.

## 2. Implement

Read the issue with its comments (`gh issue view <n> -R soft-circles/Odin --comments`) and build what it asks
test-first. [N64_MAINTAINERS.md](../../../N64_MAINTAINERS.md) maps which file owns each part of the N64 target and what
belongs in Odin64 or the LLVM fork instead; an issue that needs work outside this repo is a question for the coordinator.
Keep changes to upstream-owned files to the lines the issue needs, so upstream syncs stay mergeable.

Odin64's shared `.env` points every Odin64 checkout at the `odin` binary in this repo's main checkout. Build and test
only in your worktree, and leave the main checkout's binary and HEAD alone.

Done when every acceptance criterion in the issue has code and a test that pins it.

## 3. Verify

Build Odin in your worktree against the MIPS O64 LLVM fork, the `LLVM_CONFIG` in Odin64's `.env`, so the encoding and
O64 machine-code tests run instead of skipping as they do in CI: `LLVM_CONFIG=<that path> ./build_odin.sh release`. Then
run:

- `python3 tests/n64_validation/test_validation_contract.py` and `python3 tests/n64_validate.py quick`.
- `N64_INST=<sdk> ARES_TEST=<ares-test> python3 tests/n64_validate.py full`, with both paths from Odin64's `.env`, when
  the diff touches the N64 runtime, packaging, `src/n64_build.cpp`, the linker adapter or asm templates.
- `./odin test` on each `tests/` or `core` package the diff touches, and `./odin check examples/all` when it touches
  `core` or `base`.
- Odin64 built with this compiler, when the change reaches what Odin64 compiles: in an Odin64 worktree run
  `ODIN64_ODIN_SOURCE=<your worktree> ODIN64_COMPILER=<your worktree>/odin just verify`. The shell's values override
  Odin64's `.env`.

Long Linux builds can run on `ssh rig` as the user's global instructions say.

Done when every gate is green on the current HEAD, and each one you skipped has a reason for the PR body.

## 4. Review

In parallel, spawn three review subagents on `git diff fork/<base>...HEAD`, each grading its findings must-fix,
should-fix or nit:

- **Spec**: what the issue asks for that the diff misses or gets wrong, and what the diff adds that the issue did not ask
  for.
- **Correctness**: bugs and regressions the diff introduces, including in upstream-owned code it touches and in the
  ownership boundaries N64_MAINTAINERS.md draws.
- **Coverage**: behaviour the diff adds or changes that no test pins.

Apply every must-fix and should-fix in the changed hunks, and list the rest as follow-ups for the PR description.

Done when no review has an open finding in the changed hunks and the gates are green on the fixed HEAD.

## 5. Open the PR

Push the branch to `fork`. Write the body into a file in your scratchpad in the shape of the repo's recent PRs: a
paragraph on why, then `## What changes`, `## Decisions` and `## Verification` (the platform, the LLVM it was built with,
the base SHA, and each gate with its counts), plus `Closes #<n>`, each review's result and step 4's follow-ups. Run
`gh pr create -R soft-circles/Odin --base <base> --head <branch> --body-file <file>`.

Done when the PR exists, its body renders, and the turn ends as the next section's done line says.

## Fixing a red check, a conflict or a review comment

Answer each review comment with a fix or a reply on the PR. For each failed check, read its failed log
(`gh run view <run id> -R soft-circles/Odin --log-failed`), then check whether the same job fails on the base branch's
latest run. Fix what this branch broke, and name what was already red in a PR comment. For a conflict with the base,
merge `fork/<base>`. When a fix changes behaviour, rerun step 4 on it. Rerun step 3's gates and push.

Done when the fix is pushed and the turn ends with the PR URL, then the pushed HEAD SHA, first in the wrap-up recap.
