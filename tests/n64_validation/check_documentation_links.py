#!/usr/bin/env python3
"""Validate local Markdown links in the canonical N64 documentation set."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from urllib.parse import unquote


ODIN_ROOT = Path(__file__).resolve().parents[2]
DOCUMENTS = (
	"README.md",
	"N64_BUILD.md",
	"N64_MAINTAINERS.md",
	"examples/README.md",
	"vendor/README.md",
	"vendor/libdragon/README.md",
	"tests/n64_build/README.md",
	"tests/n64_console/README.md",
	"tests/n64_dfs/README.md",
	"tests/n64_pong/README.md",
	"tests/n64_tracer/README.md",
	"tests/o64_abi/README.md",
	"tests/o64_abi/libdragon_bindings/README.md",
)
LINK_PATTERN = re.compile(r"!?\[[^]]+\]\(([^)]+)\)")
HEADING_PATTERN = re.compile(r"^#{1,6}\s+(.+?)\s*#*\s*$", re.MULTILINE)


def slug(text: str) -> str:
	text = re.sub(r"[`*_~]", "", text.strip().lower())
	text = re.sub(r"[^\w\- ]", "", text, flags=re.UNICODE)
	return re.sub(r"[ -]+", "-", text).strip("-")


def anchors(path: Path) -> set[str]:
	return {slug(heading) for heading in HEADING_PATTERN.findall(path.read_text(encoding="utf-8"))}


def link_target(raw_target: str) -> str:
	target = raw_target.strip()
	if target.startswith("<"):
		closing = target.find(">")
		return target[1:closing] if closing >= 0 else target[1:]
	return target.split(maxsplit=1)[0]


def main() -> int:
	errors: list[str] = []
	for relative in DOCUMENTS:
		document = ODIN_ROOT / relative
		contents = document.read_text(encoding="utf-8")
		for raw_target in LINK_PATTERN.findall(contents):
			target = link_target(raw_target)
			if target.startswith(("http://", "https://", "mailto:")):
				continue
			path_text, _, fragment = target.partition("#")
			if path_text.startswith("/"):
				linked = (ODIN_ROOT / unquote(path_text.lstrip("/"))).resolve()
			else:
				linked = document if not path_text else (document.parent / unquote(path_text)).resolve()
			try:
				linked.relative_to(ODIN_ROOT)
			except ValueError:
				errors.append(f"{relative}: local link escapes the repository: {target}")
				continue
			if not linked.exists():
				errors.append(f"{relative}: missing link target: {target}")
				continue
			if fragment and linked.suffix.lower() == ".md" and unquote(fragment).lower() not in anchors(linked):
				errors.append(f"{relative}: missing Markdown anchor: {target}")

	if errors:
		for error in errors:
			print(f"error: {error}", file=sys.stderr)
		return 1
	print(f"validated local links in {len(DOCUMENTS)} canonical N64 documents")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
