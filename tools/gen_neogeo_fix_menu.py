#!/usr/bin/env python3
"""Generate Neo Geo FIX menu art and the sprite-backed TITLEPIC source.

The retail WAD is needed only when regenerating these checked-in assets. Normal
ROM builds consume the generated FIX fragment and C headers directly.
"""

from __future__ import annotations

import argparse
from collections import Counter
import struct
from dataclasses import dataclass
from pathlib import Path


FIX_TILE_BYTES = 32
FIX_TILE_LIMIT = 4096
MENU_TILE_BASE = 3525
TITLE_CROM_TILE_BASE = 272
TITLE_SOURCE_WIDTH = 320
TITLE_WIDTH = 304
TITLE_HEIGHT = 200
TITLE_SCREEN_HEIGHT = 224
TITLE_COLUMNS = 38
TITLE_ROWS = 28
TITLE_PALETTE_LIMIT = 222
SPRITE_TILE_HALF_BYTES = 64
WIPE_TILE_IDS = frozenset(slot << 8 for slot in range(16))

PATCH_NAMES = (
    "M_NGAME",
    "M_OPTION",
    "M_NEWG",
    "M_SKILL",
    "M_JKILL",
    "M_ROUGH",
    "M_HURT",
    "M_ULTRA",
    "M_NMARE",
    "M_SKULL1",
    "M_SKULL2",
    "M_OPTTTL",
    "M_SVOL",
    "M_SFXVOL",
    "M_MUSVOL",
    "M_THERML",
    "M_THERMM",
    "M_THERMR",
    "M_THERMO",
)

COMPACT_PATCHES = frozenset((
    "M_NEWG",
    "M_SKILL",
    "M_JKILL",
    "M_ROUGH",
    "M_HURT",
    "M_ULTRA",
    "M_NMARE",
    "M_SVOL",
    "M_SFXVOL",
    "M_MUSVOL",
))

FONT_FIRST = 33
FONT_LAST = 95


def neo_color(rgb: tuple[int, int, int]) -> int:
    """Match jWadUtil's Neo Geo color packing, including the dark bit."""
    r8, g8, b8 = rgb
    r, g, b = r8 // 8, g8 // 8, b8 // 8
    dark = 1 ^ (int(54.213 * r8 + 182.376 * g8 + 18.411 * b8) & 1)
    return (
        (dark << 15)
        | ((r & 1) << 14)
        | ((g & 1) << 13)
        | ((b & 1) << 12)
        | ((r & 0x1E) << 7)
        | ((g & 0x1E) << 3)
        | (b >> 1)
    )


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


def resize_patch(patch: list[list[int]], width: int, height: int) -> list[list[int]]:
    source_height = len(patch)
    source_width = len(patch[0]) if source_height else 0
    if source_width == 0 or width <= 0 or height <= 0:
        raise ValueError("cannot resize an empty Doom patch")

    return [
        [
            patch[min(source_height - 1, ((2 * y + 1) * source_height) // (2 * height))]
                 [min(source_width - 1, ((2 * x + 1) * source_width) // (2 * width))]
            for x in range(width)
        ]
        for y in range(height)
    ]


def compact_patch(patch: list[list[int]]) -> list[list[int]]:
    height = len(patch)
    width = len(patch[0]) if height else 0
    return resize_patch(
        patch,
        max(1, (width * 4 + 2) // 5),
        max(1, (height * 4 + 2) // 5),
    )


def normalize_font_glyph(glyph: list[list[int]]) -> list[list[int]]:
    width = len(glyph[0])
    if width > 8:
        return resize_patch(glyph, 8, len(glyph))
    if width == 8:
        return glyph

    left = (8 - width) // 2
    return [[-1] * left + row + [-1] * (8 - width - left) for row in glyph]


def menu_patch(iwad: Wad, name: str) -> list[list[int]]:
    patch = decode_patch(iwad.get(name))
    if name in COMPACT_PATCHES:
        patch = compact_patch(patch)

    if name == "M_THERMM":
        # Vanilla draws the 9-pixel middle patch every 8 pixels. Cropping the
        # overlapping column preserves that pitch when each FIX cell is 8px.
        patch = [row[:8] for row in patch]
    elif name == "M_THERMO":
        middle = [row[:8] for row in decode_patch(iwad.get("M_THERMM"))]
        knob = patch
        patch = [row[:] for row in middle]
        for y, row in enumerate(knob):
            for x, color in enumerate(row):
                if color >= 0:
                    patch[y][x] = color

    return patch


def encode_fix_tile(pixels: list[int]) -> bytes:
    result = bytearray()
    for xa, xb in ((4, 5), (6, 7), (0, 1), (2, 3)):
        for y in range(8):
            result.append((pixels[y * 8 + xb] << 4) | pixels[y * 8 + xa])
    return bytes(result)


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


def patch_tile_sources(patch: list[list[int]]) -> tuple[int, int, list[list[int]]]:
    height = len(patch)
    width = len(patch[0]) if height else 0
    cols = max(1, (width + 7) // 8)
    rows = max(1, (height + 7) // 8)
    tiles: list[list[int]] = []

    for tile_y in range(rows):
        for tile_x in range(cols):
            source = []
            for y in range(8):
                src_y = tile_y * 8 + y
                for x in range(8):
                    src_x = tile_x * 8 + x
                    source.append(patch[src_y][src_x] if src_y < height and src_x < width else -1)
            tiles.append(source)
    return cols, rows, tiles


def optimize_palette(
    histogram: Counter[int],
    playpal: list[tuple[int, int, int]],
    distances: list[list[int]],
) -> list[int]:
    colors = list(histogram)
    if not colors:
        return []
    if len(colors) <= 15:
        return sorted(colors, key=lambda color: (-histogram[color], color))

    palette = sorted(colors, key=lambda color: (-histogram[color], color))[:15]
    for _ in range(4):
        clusters: list[list[int]] = [[] for _ in palette]
        for color in colors:
            slot = min(
                range(len(palette)),
                key=lambda index: (distances[color][palette[index]], index),
            )
            clusters[slot].append(color)

        updated: list[int] = []
        for old, cluster in zip(palette, clusters):
            if not cluster:
                updated.append(old)
                continue
            updated.append(min(
                cluster,
                key=lambda candidate: (
                    sum(
                        histogram[color] * distances[color][candidate]
                        for color in cluster
                    ),
                    candidate,
                ),
            ))
        if updated == palette:
            break
        palette = updated
    return palette


def palette_error(
    histogram: Counter[int],
    palette: list[int],
    distances: list[list[int]],
) -> int:
    if not histogram:
        return 0
    if not palette:
        return sum(histogram.values()) * 255 * 255 * 3
    return sum(
        count * min(distances[color][candidate] for candidate in palette)
        for color, count in histogram.items()
    )


def build_palette_set(
    sources: list[list[int]],
    playpal: list[tuple[int, int, int]],
    limit: int,
) -> tuple[list[list[int]], list[list[int]]]:
    distances = [
        [color_distance(source, target) for target in playpal]
        for source in playpal
    ]
    histograms = [Counter(color for color in source if color >= 0) for source in sources]
    combined: Counter[int] = Counter()
    for histogram in histograms:
        combined.update(histogram)

    palettes = [optimize_palette(combined, playpal, distances)]
    candidates = [optimize_palette(histogram, playpal, distances) for histogram in histograms]
    selected = {tuple(palettes[0])}
    best_errors = [palette_error(histogram, palettes[0], distances) for histogram in histograms]

    while len(palettes) < limit:
        candidate_index = None
        for index in sorted(range(len(histograms)), key=best_errors.__getitem__, reverse=True):
            key = tuple(candidates[index])
            if key and key not in selected:
                candidate_index = index
                break
        if candidate_index is None:
            break

        palette = candidates[candidate_index]
        palettes.append(palette)
        selected.add(tuple(palette))
        for index, histogram in enumerate(histograms):
            best_errors[index] = min(
                best_errors[index],
                palette_error(histogram, palette, distances),
            )

    for _ in range(3):
        assignments = [
            min(
                range(len(palettes)),
                key=lambda index: palette_error(histogram, palettes[index], distances),
            )
            for histogram in histograms
        ]
        groups = [Counter() for _ in palettes]
        for histogram, assignment in zip(histograms, assignments):
            groups[assignment].update(histogram)
        updated = [
            optimize_palette(group, playpal, distances) if group else palette
            for group, palette in zip(groups, palettes)
        ]
        if updated == palettes:
            break
        palettes = updated

    return palettes, distances


def quantize_tile(
    source: list[int],
    palettes: list[list[int]],
    distances: list[list[int]],
) -> tuple[int, bytes]:
    histogram = Counter(color for color in source if color >= 0)
    palette_index = min(
        range(len(palettes)),
        key=lambda index: palette_error(histogram, palettes[index], distances),
    )
    palette = palettes[palette_index]
    output = [
        0 if color < 0 else 1 + min(
            range(len(palette)),
            key=lambda index: distances[color][palette[index]],
        )
        for color in source
    ]
    return palette_index, encode_fix_tile(output)


def c_name(name: str) -> str:
    return name.lower().replace("m_", "doom_menu_")


def write_menu_assets(
    iwad: Wad,
    base_srom_path: Path,
    output_fix: Path,
    output_header: Path,
    output_palette_header: Path,
) -> None:
    playpal_data = iwad.get("PLAYPAL")
    playpal = [tuple(playpal_data[index : index + 3]) for index in range(0, 768, 3)]
    blank = bytes(FIX_TILE_BYTES)
    unique_tiles: list[tuple[int, bytes]] = []
    tile_ids: dict[bytes, int] = {}
    patches: list[tuple[str, int, int, list[int]]] = []

    patch_sources: list[tuple[str, int, int, list[list[int]]]] = []
    all_sources: list[list[int]] = []
    for name in PATCH_NAMES:
        cols, rows, sources = patch_tile_sources(menu_patch(iwad, name))
        patch_sources.append((c_name(name), cols, rows, sources))
        all_sources.extend(sources)

    font_sources: list[list[int]] = []
    for code in range(FONT_FIRST, FONT_LAST + 1):
        glyph = normalize_font_glyph(decode_patch(iwad.get(f"STCFN{code:03d}")))
        cols, rows, sources = patch_tile_sources(glyph)
        if cols != 1 or rows != 1:
            raise ValueError(f"STCFN{code:03d} does not fit in one FIX tile")
        font_sources.append(sources[0])
        all_sources.append(sources[0])

    palettes, distances = build_palette_set(all_sources, playpal, 16)

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

    for name, cols, rows, sources in patch_sources:
        tiles = [quantize_tile(source, palettes, distances) for source in sources]
        entries = [(palette << 12) | tile_id(tile) for palette, tile in tiles]
        patches.append((name, cols, rows, entries))

    font_entries: list[int] = []
    for source in font_sources:
        palette, tile = quantize_tile(source, palettes, distances)
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

    palette_lines = [
        "/* Generated by tools/gen_neogeo_fix_menu.py. */",
        "#ifndef DOOM_NEO_GEO_FIX_MENU_PALETTE_H",
        "#define DOOM_NEO_GEO_FIX_MENU_PALETTE_H",
        "",
        "#include <stdint.h>",
        "",
        "static const uint16_t doom_menu_palettes[16][16] = {",
    ]
    for palette in palettes:
        values = [0x8000]
        values.extend(neo_color(playpal[color]) for color in palette)
        values.extend([0x8000] * (16 - len(values)))
        palette_lines.append(
            "    {" + ", ".join(f"0x{value:04x}" for value in values) + "},"
        )
    for _ in range(16 - len(palettes)):
        palette_lines.append("    {" + ", ".join(["0x8000"] * 16) + "},")
    palette_lines.extend(["};", "", "#endif", ""])
    output_palette_header.parent.mkdir(parents=True, exist_ok=True)
    output_palette_header.write_text("\n".join(palette_lines), encoding="ascii")

    print(f"FIX menu: {len(unique_tiles)} unique tiles, IDs {MENU_TILE_BASE}..{last_tile} (wipe tiles preserved)")

def write_title_crom(
    iwad: Wad,
    output_c1: Path,
    output_c2: Path,
    output_header: Path,
) -> None:
    source = decode_patch(iwad.get("TITLEPIC"))
    if len(source) != TITLE_HEIGHT or len(source[0]) != TITLE_SOURCE_WIDTH:
        raise ValueError(
            f"unexpected retail TITLEPIC size: {len(source[0])}x{len(source)}"
        )
    source = resize_patch(source, TITLE_WIDTH, TITLE_HEIGHT)

    playpal_data = iwad.get("PLAYPAL")
    playpal = [tuple(playpal_data[index : index + 3]) for index in range(0, 768, 3)]
    canvas = [[-1] * TITLE_WIDTH for _ in range(TITLE_SCREEN_HEIGHT)]
    y_offset = (TITLE_SCREEN_HEIGHT - TITLE_HEIGHT) // 2
    for y, row in enumerate(source):
        canvas[y_offset + y] = row

    columns, rows, tile_sources = patch_tile_sources(canvas)
    if columns != TITLE_COLUMNS or rows != TITLE_ROWS:
        raise ValueError(f"unexpected TITLEPIC tile grid: {columns}x{rows}")
    palettes, distances = build_palette_set(tile_sources, playpal, TITLE_PALETTE_LIMIT)

    c1 = bytearray()
    c2 = bytearray()
    palette_map: list[int] = []
    for source_tile in tile_sources:
        histogram = Counter(color for color in source_tile if color >= 0)
        palette_index = min(
            range(len(palettes)),
            key=lambda index: palette_error(histogram, palettes[index], distances),
        )
        palette_map.append(palette_index)
        palette = palettes[palette_index]
        pixels = [
            0 if color < 0 else 1 + min(
                range(len(palette)),
                key=lambda index: distances[color][palette[index]],
            )
            for color in source_tile
        ]
        scaled = [pixels[(y // 2) * 8 + (x // 2)] for y in range(16) for x in range(16)]
        tile_c1, tile_c2 = encode_sprite_tile(scaled)
        c1.extend(tile_c1)
        c2.extend(tile_c2)

    output_c1.parent.mkdir(parents=True, exist_ok=True)
    output_c2.parent.mkdir(parents=True, exist_ok=True)
    output_c1.write_bytes(c1)
    output_c2.write_bytes(c2)

    lines = [
        "/* Generated by tools/gen_neogeo_fix_menu.py. */",
        "#ifndef DOOM_NEO_GEO_TITLE_ASSETS_H",
        "#define DOOM_NEO_GEO_TITLE_ASSETS_H",
        "",
        "#include <stdint.h>",
        "",
        f"#define DOOM_TITLE_TILE_BASE {TITLE_CROM_TILE_BASE}u",
        f"#define DOOM_TITLE_COLUMNS {TITLE_COLUMNS}u",
        f"#define DOOM_TITLE_ROWS {TITLE_ROWS}u",
        f"#define DOOM_TITLE_PALETTE_COUNT {len(palettes)}u",
        "",
        f"static const uint16_t doom_title_palettes[{len(palettes)}][16] = {{",
    ]
    for palette in palettes:
        values = [0x8000]
        values.extend(neo_color(playpal[color]) for color in palette)
        values.extend([0x8000] * (16 - len(values)))
        lines.append("    {" + ", ".join(f"0x{value:04x}" for value in values) + "},")
    lines.extend(["};", "", f"static const uint8_t doom_title_palette_map[{len(palette_map)}] = {{"])
    for offset in range(0, len(palette_map), 20):
        values = ", ".join(f"{value}u" for value in palette_map[offset : offset + 20])
        lines.append("    " + values + ",")
    lines.extend(["};", "", "#endif", ""])
    output_header.parent.mkdir(parents=True, exist_ok=True)
    output_header.write_text("\n".join(lines), encoding="ascii")

    print(
        f"TITLEPIC C-ROM: retail {TITLE_SOURCE_WIDTH}x{TITLE_HEIGHT} fit to "
        f"{TITLE_WIDTH}x{TITLE_HEIGHT}, "
        f"{TITLE_COLUMNS}x{TITLE_ROWS} tiles, {len(palettes)} palettes at tile "
        f"{TITLE_CROM_TILE_BASE}"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iwad", type=Path, required=True)
    parser.add_argument("--base-srom", type=Path, required=True)
    parser.add_argument("--out-fix", type=Path, required=True)
    parser.add_argument("--out-menu-header", type=Path, required=True)
    parser.add_argument("--out-menu-palette-header", type=Path, required=True)
    parser.add_argument("--out-title-c1", type=Path, required=True)
    parser.add_argument("--out-title-c2", type=Path, required=True)
    parser.add_argument("--out-title-header", type=Path, required=True)
    args = parser.parse_args()

    write_menu_assets(
        Wad(args.iwad.read_bytes()),
        args.base_srom,
        args.out_fix,
        args.out_menu_header,
        args.out_menu_palette_header,
    )
    write_title_crom(
        Wad(args.iwad.read_bytes()),
        args.out_title_c1,
        args.out_title_c2,
        args.out_title_header,
    )


if __name__ == "__main__":
    main()
