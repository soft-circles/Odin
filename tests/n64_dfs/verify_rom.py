#!/usr/bin/env python3
"""Verify the pinned libdragon package contents of the N64 DFS sample ROM."""

from __future__ import annotations

import argparse
import configparser
import io
import struct
import zipfile
from dataclasses import dataclass
from pathlib import Path


EXPECTED_TITLE = b"Odin DFS v0.2"
EXPECTED_CONTROLLERS = bytes((0x00, 0xFF, 0xFF, 0xFF))
EXPECTED_CATEGORY = ord("N")
EXPECTED_REGION = ord("E")
EXPECTED_CONFIG = 0x02  # no save, no RTC, region-free
VERSION_FILES = ("libdragon.version", "toolchain.version")


@dataclass(frozen=True)
class RompakEntry:
	offset: int
	payload: bytes


def parse_toc(rom: bytes) -> dict[str, RompakEntry]:
	candidates: list[tuple[int, int]] = []
	for offset in range(0x1000, min(0x5000, len(rom) - 15), 16):
		if rom[offset : offset + 4] != b"TOC0":
			continue
		_, _, toc_size, entry_size, count = struct.unpack_from(">4sIIHH", rom, offset)
		if toc_size == 1024 and entry_size == 64 and count <= 15 and offset + toc_size <= len(rom):
			candidates.append((offset, count))
	assert len(candidates) == 1, f"expected one valid rompak TOC, found {candidates}"

	toc_offset, count = candidates[0]
	entries: dict[str, RompakEntry] = {}
	for index in range(count):
		offset, size, raw_name = struct.unpack_from(">II56s", rom, toc_offset + 16 + index * 64)
		assert b"\0" in raw_name, f"TOC entry {index} has no terminated name"
		assert offset + size <= len(rom), f"TOC entry {index} extends past the ROM"
		name = raw_name.split(b"\0", 1)[0].decode("utf-8")
		assert name not in entries, f"duplicate TOC entry {name!r}"
		entries[name] = RompakEntry(offset, rom[offset : offset + size])
	return entries


def verify_header(rom: bytes) -> None:
	assert rom[:4] == bytes.fromhex("80371240"), "ROM is not big-endian .z64"
	assert rom[0x20:0x34] == EXPECTED_TITLE.ljust(20, b"\0"), "unexpected ROM title"
	assert rom[0x34:0x38] == EXPECTED_CONTROLLERS, "unexpected controller declarations"
	assert rom[0x38] & 1, "extended metadata-present bit is not set"
	assert rom[0x3B] == EXPECTED_CATEGORY, "unexpected media category"
	assert rom[0x3C:0x3E] == b"ED", "advanced homebrew header marker is absent"
	assert rom[0x3E] == EXPECTED_REGION, "unexpected region byte"
	assert rom[0x3F] == EXPECTED_CONFIG, "unexpected save/RTC/region-free configuration"


def verify_rompak(entries: dict[str, RompakEntry], dfs_path: Path, sdk_root: Path) -> None:
	assert entries[dfs_path.name].payload == dfs_path.read_bytes(), "embedded DFS differs from retained build output"
	include_root = sdk_root / "mips64-elf/include"
	for name in VERSION_FILES:
		assert entries[name].payload == (include_root / name).read_bytes(), f"embedded {name} differs from pinned SDK"


def verify_metadata(rom: bytes, metadata_path: Path, rompak: dict[str, RompakEntry]) -> None:
	assert len(rom) % 16384 == 0, "metadata tool did not apply 16 KiB ROM padding"
	assert rom[-22:-18] == b"PK\x05\x06", "metadata ZIP end record is not at ROM EOF"
	with zipfile.ZipFile(io.BytesIO(rom)) as archive:
		infos = archive.infolist()
		assert infos and infos[-1].filename == "metadata.ini", "metadata.ini is not the final ZIP entry"
		assert len({info.filename for info in infos}) == len(infos), "metadata ZIP contains duplicate names"
		assert all(info.compress_type == zipfile.ZIP_STORED for info in infos), "metadata ZIP must be uncompressed"
		assert archive.testzip() is None, "metadata ZIP CRC validation failed"
		metadata = metadata_path.read_bytes()
		assert archive.read("metadata.ini") == metadata, "embedded metadata.ini differs from source"
		zip_start = min(info.header_offset for info in infos)
		assert all(entry.offset + len(entry.payload) <= zip_start for entry in rompak.values()), \
			"rompak payload overlaps metadata ZIP"

	parser = configparser.ConfigParser(interpolation=None)
	parser.read_string(metadata.decode("utf-8"))
	assert parser.has_section("meta.es"), "localized [meta.es] metadata is absent"


def verify(rom_path: Path, dfs_path: Path, metadata_path: Path, sdk_root: Path) -> None:
	rom = rom_path.read_bytes()
	verify_header(rom)
	rompak = parse_toc(rom)
	verify_rompak(rompak, dfs_path, sdk_root)
	verify_metadata(rom, metadata_path, rompak)


def main() -> None:
	parser = argparse.ArgumentParser()
	parser.add_argument("rom", type=Path)
	parser.add_argument("dfs", type=Path, help="retained build/odin-n64.dfs")
	parser.add_argument("metadata", type=Path)
	parser.add_argument("sdk", type=Path)
	args = parser.parse_args()
	verify(args.rom, args.dfs, args.metadata, args.sdk)
	print("DFS ROM package verification passed")


if __name__ == "__main__":
	main()
