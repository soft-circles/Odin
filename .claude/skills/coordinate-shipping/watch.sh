#!/bin/sh
set -u
repo=soft-circles/Odin
pr=${1:?usage: sh .claude/skills/coordinate-shipping/watch.sh <pr number or url> [pushed commit, default HEAD]}
sha=${2:-$(git rev-parse HEAD)}
interval=10
limit=30
tries=0
until [ "$(gh pr view "$pr" -R "$repo" --json headRefOid -q .headRefOid 2>/dev/null)" = "$sha" ] &&
    ! gh pr checks "$pr" -R "$repo" 2>&1 | grep -q 'no checks reported'; do
    tries=$((tries + 1))
    if [ "$tries" -ge "$limit" ]; then
        echo "No checks reported on $pr for $sha after $((limit * interval)) seconds" >&2
        exit 1
    fi
    sleep "$interval"
done
gh pr checks "$pr" -R "$repo" --watch --interval 30 >/dev/null 2>&1
status=$?
gh pr checks "$pr" -R "$repo"
exit "$status"
