#!/usr/bin/env python3
"""Compare a file's SHA-256 identity with an accepted literal."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("path", type=Path)
	parser.add_argument("expected")
	args = parser.parse_args()
	actual = hashlib.sha256(args.path.read_bytes()).hexdigest()
	print(f"{args.path}: {actual}")
	if actual != args.expected.lower():
		parser.exit(1, f"error: expected {args.expected.lower()}, got {actual}\n")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
