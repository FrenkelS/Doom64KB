#!/usr/bin/env python3
"""Generate Neo Geo FIX menu art and the sprite-backed TITLEPIC source.

The retail WAD is needed only when regenerating these checked-in assets. Normal
ROM builds consume the generated FIX fragment and C headers directly.
"""

from __future__ import annotations

import argparse
import re
import struct
from dataclasses import dataclass
from pathlib import Path


FIX_TILE_BYTES = 32
FIX_TILE_LIMIT = 4096
MENU_TILE_BASE = 3525
TITLE_FIX_WIDTH = 38
TITLE_FIX_HEIGHT = 28
TITLE_CROM_TILE_BASE = 272
SPRITE_TILE_HALF_BYTES = 64
WIPE_TILE_IDS = frozenset(slot << 8 for slot in range(16))

PATCH_NAMES = (
    "M_NGAME",
    "M_OPTION",
    "M_SKILL",
    "M_JKILL",
    "M_ROUGH",
    "M_HURT",
    "M_ULTRA",
    "M_NMARE",
    "M_SKULL1",
    "M_SKULL2",
    "M_OPTTTL",
    "M_ENDGAM",
    "M_MESSG",
    "M_MSGON",
    "M_MSGOFF",
    "M_DETAIL",
    "M_GDHIGH",
    "M_GDLOW",
)

FONT_FIRST = 33
FONT_LAST = 95


@dataclass(frozen=True)
class Lump:
    offset: int
    size: int


class Wad:
    def __init__(self, data: bytes, endian: str = "<") -> None:
        if endian == ">":
            ident, count, _filler, directory = struct.unpack_from(">4sHHI", data, 0)
        else:
            ident, count, directory = struct.unpack_from("<4sII", data, 0)
        if ident not in (b"IWAD", b"PWAD"):
            raise ValueError(f"not a Doom WAD: {ident!r}")

        self.data = data
        self.lumps: dict[str, Lump] = {}
        for index in range(count):
            entry = directory + index * 16
            if endian == ">":
                offset, size, _filler, raw_name = struct.unpack_from(">IHH8s", data, entry)
            else:
                offset, size, raw_name = struct.unpack_from("<II8s", data, entry)
            name = raw_name.rstrip(b"\0").decode("ascii", "replace").upper()
            self.lumps[name] = Lump(offset, size)

    def get(self, name: str) -> bytes:
        lump = self.lumps[name.upper()]
        return self.data[lump.offset : lump.offset + lump.size]


def decode_patch(data: bytes) -> list[list[int]]:
    width, height, _left, _top = struct.unpack_from("<hhhh", data, 0)
    columns = [struct.unpack_from("<I", data, 8 + x * 4)[0] for x in range(width)]
    pixels = [[-1] * width for _ in range(height)]

    for x, offset in enumerate(columns):
        pos = offset
        while data[pos] != 0xFF:
            top = data[pos]
            length = data[pos + 1]
            pos += 3
            for y, color in enumerate(data[pos : pos + length]):
                if top + y < height:
                    pixels[top + y][x] = color
            pos += length + 1
    return pixels


def encode_fix_tile(pixels: list[int]) -> bytes:
    result = bytearray()
    for xa, xb in ((4, 5), (6, 7), (0, 1), (2, 3)):
        for y in range(8):
            result.append((pixels[y * 8 + xb] << 4) | pixels[y * 8 + xa])
    return bytes(result)


def decode_fix_tile(data: bytes) -> list[int]:
    pixels = [0] * 64
    pos = 0
    for xa, xb in ((4, 5), (6, 7), (0, 1), (2, 3)):
        for y in range(8):
            value = data[pos]
            pos += 1
            pixels[y * 8 + xa] = value & 0x0F
            pixels[y * 8 + xb] = value >> 4
    return pixels


def encode_sprite_tile(pixels: list[int]) -> tuple[bytes, bytes]:
    if len(pixels) != 256:
        raise ValueError("Neo Geo sprite tiles must be 16x16")

    c1 = bytearray()
    c2 = bytearray()
    for block_x, block_y in ((8, 0), (8, 8), (0, 0), (0, 8)):
        for y in range(8):
            planes = [0, 0, 0, 0]
            for x in range(8):
                color = pixels[(block_y + y) * 16 + block_x + x]
                bit = 7 - x
                for plane in range(4):
                    if color & (1 << plane):
                        planes[plane] |= 1 << bit
            c1.extend((planes[0], planes[1]))
            c2.extend((planes[2], planes[3]))
    return bytes(c1), bytes(c2)


def color_distance(a: tuple[int, int, int], b: tuple[int, int, int]) -> int:
    return sum((a[channel] - b[channel]) ** 2 for channel in range(3))


def build_fix_quantizer(playpal: list[tuple[int, int, int]]) -> tuple[list[list[int]], list[list[int]]]:
    slots: list[list[int]] = []
    errors: list[list[int]] = []
    for palette in range(16):
        candidates = playpal[palette * 16 + 1 : palette * 16 + 16]
        palette_slots = []
        palette_errors = []
        for rgb in playpal:
            slot = min(range(15), key=lambda index: color_distance(rgb, candidates[index]))
            palette_slots.append(slot + 1)
            palette_errors.append(color_distance(rgb, candidates[slot]))
        slots.append(palette_slots)
        errors.append(palette_errors)
    return slots, errors


def quantize_fix_tile(source: list[int], quantizer: tuple[list[list[int]], list[list[int]]]) -> tuple[int, bytes]:
    best_error: int | None = None
    best_palette = 0
    best_tile = bytes(FIX_TILE_BYTES)
    slots, errors = quantizer

    for palette in range(16):
        output = [0 if color < 0 else slots[palette][color] for color in source]
        error = sum(errors[palette][color] for color in source if color >= 0)

        if best_error is None or error < best_error:
            best_error = error
            best_palette = palette
            best_tile = encode_fix_tile(output)

    return best_palette, best_tile


def patch_tiles(patch: list[list[int]], quantizer: tuple[list[list[int]], list[list[int]]]) -> tuple[int, int, list[tuple[int, bytes]]]:
    height = len(patch)
    width = len(patch[0]) if height else 0
    cols = max(1, (width + 7) // 8)
    rows = max(1, (height + 7) // 8)
    tiles: list[tuple[int, bytes]] = []

    for tile_y in range(rows):
        for tile_x in range(cols):
            source = []
            for y in range(8):
                src_y = tile_y * 8 + y
                for x in range(8):
                    src_x = tile_x * 8 + x
                    source.append(patch[src_y][src_x] if src_y < height and src_x < width else -1)
            tiles.append(quantize_fix_tile(source, quantizer))
    return cols, rows, tiles


def c_name(name: str) -> str:
    return name.lower().replace("m_", "doom_menu_")


def write_menu_assets(iwad: Wad, base_srom_path: Path, output_fix: Path, output_header: Path) -> None:
    playpal_data = iwad.get("PLAYPAL")
    playpal = [tuple(playpal_data[index : index + 3]) for index in range(0, 768, 3)]
    quantizer = build_fix_quantizer(playpal)
    blank = bytes(FIX_TILE_BYTES)
    unique_tiles: list[tuple[int, bytes]] = []
    tile_ids: dict[bytes, int] = {}
    patches: list[tuple[str, int, int, list[int]]] = []

    def next_tile_id() -> int:
        candidate = MENU_TILE_BASE + len(unique_tiles)
        while candidate in WIPE_TILE_IDS:
            candidate += 1
        if unique_tiles:
            candidate = unique_tiles[-1][0] + 1
            while candidate in WIPE_TILE_IDS:
                candidate += 1
        return candidate

    def tile_id(tile: bytes) -> int:
        if tile == blank:
            return 0
        if tile not in tile_ids:
            tile_ids[tile] = next_tile_id()
            unique_tiles.append((tile_ids[tile], tile))
        return tile_ids[tile]

    for name in PATCH_NAMES:
        cols, rows, tiles = patch_tiles(decode_patch(iwad.get(name)), quantizer)
        entries = [(palette << 12) | tile_id(tile) for palette, tile in tiles]
        patches.append((c_name(name), cols, rows, entries))

    font_entries: list[int] = []
    for code in range(FONT_FIRST, FONT_LAST + 1):
        glyph = decode_patch(iwad.get(f"STCFN{code:03d}"))
        if len(glyph[0]) > 8:
            width = len(glyph[0])
            glyph = [[row[(x * (width - 1) + 3) // 7] for x in range(8)] for row in glyph]
        cols, rows, tiles = patch_tiles(glyph, quantizer)
        if cols != 1 or rows != 1:
            raise ValueError(f"STCFN{code:03d} does not fit in one FIX tile")
        palette, tile = tiles[0]
        font_entries.append((palette << 12) | tile_id(tile))

    if unique_tiles and unique_tiles[-1][0] >= FIX_TILE_LIMIT:
        raise ValueError(
            f"menu needs {len(unique_tiles)} unique tiles at {MENU_TILE_BASE}, "
            "but exceeds the FIX tile address space after wipe reservations"
        )

    output_fix.parent.mkdir(parents=True, exist_ok=True)
    base = base_srom_path.read_bytes()
    last_tile = unique_tiles[-1][0]
    overlay = bytearray(base[MENU_TILE_BASE * FIX_TILE_BYTES : (last_tile + 1) * FIX_TILE_BYTES])
    for tile, data in unique_tiles:
        offset = (tile - MENU_TILE_BASE) * FIX_TILE_BYTES
        overlay[offset : offset + FIX_TILE_BYTES] = data
    output_fix.write_bytes(overlay)

    lines = [
        "/* Generated by tools/gen_neogeo_fix_menu.py. */",
        "#ifndef DOOM_NEO_GEO_FIX_MENU_ASSETS_H",
        "#define DOOM_NEO_GEO_FIX_MENU_ASSETS_H",
        "",
        "#include <stdint.h>",
        "",
        f"#define DOOM_MENU_FIX_TILE_BASE {MENU_TILE_BASE}u",
        f"#define DOOM_MENU_FIX_TILE_COUNT {len(unique_tiles)}u",
        f"#define DOOM_MENU_FONT_FIRST {FONT_FIRST}u",
        f"#define DOOM_MENU_FONT_COUNT {len(font_entries)}u",
        "",
    ]

    for name, cols, rows, entries in patches:
        lines.extend(
            [
                f"#define {name.upper()}_COLS {cols}u",
                f"#define {name.upper()}_ROWS {rows}u",
                f"static const uint16_t {name}[{len(entries)}] = {{",
            ]
        )
        for offset in range(0, len(entries), 12):
            values = ", ".join(f"0x{entry:04x}" for entry in entries[offset : offset + 12])
            lines.append(f"    {values},")
        lines.extend(["};", ""])

    lines.append(f"static const uint16_t doom_menu_font[{len(font_entries)}] = {{")
    for offset in range(0, len(font_entries), 12):
        values = ", ".join(f"0x{entry:04x}" for entry in font_entries[offset : offset + 12])
        lines.append(f"    {values},")
    lines.extend(["};", "", "#endif", ""])
    output_header.parent.mkdir(parents=True, exist_ok=True)
    output_header.write_text("\n".join(lines), encoding="ascii")

    print(f"FIX menu: {len(unique_tiles)} unique tiles, IDs {MENU_TILE_BASE}..{last_tile} (wipe tiles preserved)")


def parse_embedded_wad(header: Path) -> Wad:
    source = header.read_text(encoding="ascii")
    values = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", source))
    return Wad(values, endian=">")


def write_title_crom(embedded_wad: Wad, srom_path: Path, output_c1: Path, output_c2: Path) -> None:
    tilemap = embedded_wad.get("TITLEPIC")
    if len(tilemap) != TITLE_FIX_WIDTH * TITLE_FIX_HEIGHT * 2:
        raise ValueError(f"unexpected compact TITLEPIC size: {len(tilemap)}")

    srom = srom_path.read_bytes()
    c1 = bytearray()
    c2 = bytearray()
    for tile_y in range(TITLE_FIX_HEIGHT):
        for tile_x in range(TITLE_FIX_WIDTH):
            entry = struct.unpack_from(">H", tilemap, (tile_y * TITLE_FIX_WIDTH + tile_x) * 2)[0]
            tile = entry & 0x0FFF
            offset = tile * FIX_TILE_BYTES
            pixels = decode_fix_tile(srom[offset : offset + FIX_TILE_BYTES])
            scaled = [pixels[(y // 2) * 8 + (x // 2)] for y in range(16) for x in range(16)]
            tile_c1, tile_c2 = encode_sprite_tile(scaled)
            c1.extend(tile_c1)
            c2.extend(tile_c2)

    output_c1.parent.mkdir(parents=True, exist_ok=True)
    output_c2.parent.mkdir(parents=True, exist_ok=True)
    output_c1.write_bytes(c1)
    output_c2.write_bytes(c2)
    print(f"TITLEPIC C-ROM: {TITLE_FIX_WIDTH}x{TITLE_FIX_HEIGHT} tiles at {TITLE_CROM_TILE_BASE}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iwad", type=Path, required=True)
    parser.add_argument("--base-srom", type=Path, required=True)
    parser.add_argument("--embedded-wad-header", type=Path, required=True)
    parser.add_argument("--out-fix", type=Path, required=True)
    parser.add_argument("--out-menu-header", type=Path, required=True)
    parser.add_argument("--out-title-c1", type=Path, required=True)
    parser.add_argument("--out-title-c2", type=Path, required=True)
    args = parser.parse_args()

    write_menu_assets(Wad(args.iwad.read_bytes()), args.base_srom, args.out_fix, args.out_menu_header)
    write_title_crom(parse_embedded_wad(args.embedded_wad_header), args.base_srom, args.out_title_c1, args.out_title_c2)


if __name__ == "__main__":
    main()
