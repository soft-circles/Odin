---
name: coordinate-shipping
description: "Ship a list of soft-circles/Odin issues as one PR each into n64: start a session per issue, watch every PR's CI, and squash-merge each once it is green and reviewed."
argument-hint: "The issues to ship, and the base branch when it is not n64"
disable-model-invocation: true
---

You are the coordinator. Each issue's session does the work in [PR.md](PR.md); you start those sessions, watch their PRs
and merge them. Anything outside the issue list waits for the user's yes in this chat. Sessions may spawn Sonnet
subagents for exploration and research, and each hands its work to a new session once its context reaches 30%.

This checkout has two remotes: `origin` is upstream odin-lang/Odin and `fork` is soft-circles/Odin, where the issues and
PRs live. The base is `n64` unless the user names another; `master` mirrors upstream and never takes these PRs. Pass
`-R soft-circles/Odin` to every `gh` command, since this checkout sets no default repository. Upstream syncs
(`Merge odin-lang/Odin master … into n64`) land as merge commits and are not shipped with this skill.

Between events, end the turn with a one-line status per issue. The watches and the sessions' notes wake you.

## 1. Plan the order

Run `git fetch fork <base>` and read each issue. An issue is _blocked_ by another in the list when it needs that issue's
code or both touch the same files, and it waits until its blocker merges.

Done when every issue is ready or blocked by a named issue in the list, and the plan is posted as issue, title and
blocked-by.

## 2. Start a session per ready issue

A session holds one of four slots from its start until its PR merges. Whenever a slot is free and an issue is ready and
unstarted, call `start_session` with initiation `user_asked`, context `fresh`, `use_worktree: true`, the title
`#<n> <issue title>`, and this brief:

> Ship soft-circles/Odin issue #<n> into `<base>`. First run `git fetch fork <base>` and
> `git reset --hard fork/<base>`, then follow `.claude/skills/coordinate-shipping/PR.md` from step 2. A coordinator
> started you; it watches CI, merges, and messages you with fixes to make. Ask it when the issue leaves a decision open.
> You may spawn Sonnet subagents for exploration and research, and you hand off at 30% context, both as PR.md says.

Done when every slot is taken or no ready issue is left unstarted.

## 3. Take in each PR

A session's note is its report, not proof. A note with no PR URL, question or `HANDOFF` means the session is stuck:
message it once, then take it to the user. Read the PR's head SHA, base and body, and check its base, its `Closes #<n>`,
that its Verification names the gates PR.md step 3 requires for what the diff touches, and that each of the three
reviews is reported with no open must-fix or should-fix. Send any gap back to the session. Then stop any earlier watch
of the PR and run `sh .claude/skills/coordinate-shipping/watch.sh <pr> <head SHA>` with `run_in_background`, noting the
SHA it watches.

Done when every open PR has a watch on its current head.

## 4. Route failures

A watch that exits 1 with "No checks reported" either has a moved head (back to step 3) or has not seen CI start (run
`gh pr checks <pr> -R soft-circles/Odin`, then rewatch, or take it to the user when the PR has no CI). When a watch ends
red, send the session the failing checks and their run IDs. A check that fails the same way on the base's latest run is
already red: ask the user whether to merge past it. Answer a session's question from its issue, or ask the user and
forward their words.

Done when every red PR has its failures with its session or with the user.

## 5. Hand off a full session

A session sends a `HANDOFF` message once its context reaches 30%. Before anything else, tell the user in this chat:
`#<n>: <session> reached <percent>% context; starting a new session to continue from <SHA>`. Check that the branch's tip
on `fork` is the SHA the message names, then call `start_session` with initiation `user_asked`, context `fresh`,
`use_worktree: true`, the title `#<n> <issue title> (part <k>)`, and this brief followed by the handoff message:

> Continue shipping soft-circles/Odin issue #<n> into `<base>` on branch `<branch>`. First run
> `git fetch fork <branch>` and `git reset --hard fork/<branch>`, then follow `.claude/skills/coordinate-shipping/PR.md`
> from <step>. A coordinator started you; it watches CI, merges, and messages you with fixes to make. Ask it when the
> issue leaves a decision open. You may spawn Sonnet subagents for exploration and research, and you hand off at 30%
> context, both as PR.md says. The previous session's handoff follows.

The new session takes over the old one's slot and its PR. Archive the old session with the reason
`Handed off to <new session>`; invoking this skill is the user's yes for this archive too. When a session reports
without a handoff, read its context with `get_usage` on its id, and message it to hand off once that is 30% or more.

Done when every handoff has a running successor and the user has been told of it.

## 6. Merge

A PR is ready when its latest watch exited 0, the PR's head is still the watched SHA, it is mergeable (read again while
`UNKNOWN`), and no review comment is left unanswered. Merge ready PRs one at a time with
`gh pr merge <pr> -R soft-circles/Odin --squash --match-head-commit <watched SHA>`. After each merge, send every open
PR's session whose diff touches the merged files to bring its branch up to date and rerun its gates.

Then prune the merged work: stop the PR's watch and call `archive_session` on its session with the reason
`PR #<pr> merged`, which also removes its worktree. A session the call reports as still working gets archived at the next
event instead. Invoking this skill is the user's yes for these archives; archive nothing outside the issue list.

Done for an issue when its PR is merged, its session is archived and, on a base that is not the default branch (`n64`
is not: `master` is), you have closed the issue with `gh issue close <n> -R soft-circles/Odin`.

## 7. Report

Post a table of issue, PR and merge commit, with every check the user let you merge past, and name any merged issue's
session that could not be archived. Merging does not move Odin64's pin: say that the `[odin]` commit in Odin64's
`toolchain.lock.toml` still names the old revision, and offer a session in Odin64 to bump it to the last merge commit.
Offer to archive the sessions of issues the user took off the list, and archive them only on the user's yes.

Done when every issue in the list is merged or the user has taken it off the list.
