#!/usr/bin/env python3
"""Generate deduplicated, precomposed Doom wall columns for Neo Geo ROM."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

from gen_neogeo_sprite_defs import Wad, read_embedded_wad_header


MAX_COLUMN_HEIGHT = 128
INVALID_REF_BASE = 0xFFFF


def lump_data(wad: Wad, name: str) -> bytes:
    lump = wad.lumps[wad.index(name)]
    return wad.data[lump.offset : lump.offset + lump.size]


def compose_column(
    wad: Wad,
    texture_data: bytes,
    texture_offset: int,
    column: int,
    height: int,
    patch_count: int,
) -> bytes:
    pixels = bytearray(MAX_COLUMN_HEIGHT)
    covered = bytearray(height)

    for patch_index in range(patch_count):
        patch_offset = texture_offset + 8 + patch_index * 8
        origin_x, origin_y, lump_index, declared_width = struct.unpack_from(
            ">hhhh", texture_data, patch_offset
        )
        if not origin_x <= column < origin_x + declared_width:
            continue

        lump = wad.lumps[lump_index]
        patch = wad.data[lump.offset : lump.offset + lump.size]
        if len(patch) < 8:
            raise ValueError(f"patch lump {lump.name} is truncated")

        width = struct.unpack_from(">h", patch, 0)[0]
        if width != declared_width:
            raise ValueError(
                f"patch lump {lump.name} width {width} != {declared_width}"
            )

        local_column = column - origin_x
        column_table_offset = 8 + local_column * 4
        if column_table_offset + 4 > len(patch):
            raise ValueError(f"patch lump {lump.name} column table is truncated")
        post_offset = struct.unpack_from(">I", patch, column_table_offset)[0]

        while True:
            if post_offset >= len(patch):
                raise ValueError(f"patch lump {lump.name} post lies outside lump")
            top_delta = patch[post_offset]
            if top_delta == 0xFF:
                break
            if post_offset + 4 > len(patch):
                raise ValueError(f"patch lump {lump.name} post header is truncated")

            count = patch[post_offset + 1]
            source_offset = post_offset + 3
            position = origin_y + top_delta

            if source_offset + count > len(patch):
                raise ValueError(f"patch lump {lump.name} post data is truncated")
            if position < 0:
                source_offset -= position
                count += position
                position = 0
            if position + count > height:
                count = height - position

            if count > 0:
                pixels[position : position + count] = patch[
                    source_offset : source_offset + count
                ]
                covered[position : position + count] = b"\1" * count

            post_offset += patch[post_offset + 1] + 4

    if 0 in covered:
        raise ValueError(
            f"texture column {column} has uncovered pixels after composition"
        )

    return bytes(pixels)


def generate(wad: Wad) -> tuple[list[int], list[int], list[bytes]]:
    texture1 = lump_data(wad, "TEXTURE1")
    texture_data = lump_data(wad, "TEXTUREP")
    if len(texture1) < 4:
        raise ValueError("TEXTURE1 is truncated")

    texture_count = struct.unpack_from(">I", texture1)[0]
    if texture_count <= 0 or texture_count > 0xFF:
        raise ValueError(f"invalid texture count {texture_count}")
    if len(texture_data) < texture_count * 2:
        raise ValueError("TEXTUREP offset table is truncated")

    texture_offsets = struct.unpack_from(f">{texture_count}H", texture_data)
    ref_bases = [INVALID_REF_BASE] * texture_count
    refs: list[int] = []
    columns: list[bytes] = []
    column_ids: dict[bytes, int] = {}

    for texture_index, texture_offset in enumerate(texture_offsets):
        if texture_offset + 8 > len(texture_data):
            raise ValueError(f"texture {texture_index} header is truncated")
        _, width, height, overlapped, patch_count = struct.unpack_from(
            ">HHHBB", texture_data, texture_offset
        )
        if width <= 0 or width > 0x100:
            raise ValueError(f"texture {texture_index} has unsupported width {width}")
        if height <= 0 or height > MAX_COLUMN_HEIGHT:
            raise ValueError(
                f"texture {texture_index} has unsupported height {height}"
            )
        if texture_offset + 8 + patch_count * 8 > len(texture_data):
            raise ValueError(f"texture {texture_index} patch table is truncated")
        if not overlapped:
            continue

        ref_bases[texture_index] = len(refs)
        for column in range(width):
            pixels = compose_column(
                wad,
                texture_data,
                texture_offset,
                column,
                height,
                patch_count,
            )
            column_id = column_ids.get(pixels)
            if column_id is None:
                column_id = len(columns)
                if column_id >= INVALID_REF_BASE:
                    raise ValueError("too many unique wall columns")
                column_ids[pixels] = column_id
                columns.append(pixels)
            refs.append(column_id)

    if len(refs) >= INVALID_REF_BASE:
        raise ValueError("too many wall-column references")
    return ref_bases, refs, columns


def format_u16_array(name: str, values: list[int]) -> list[str]:
    lines = [
        f"static const uint16_t {name}[{len(values)}] "
        'DOOM_WALL_ROM __attribute__((aligned(2))) = {'
    ]
    for offset in range(0, len(values), 12):
        lines.append(
            "    "
            + ", ".join(f"0x{value:04x}u" for value in values[offset : offset + 12])
            + ","
        )
    lines.extend(["};", ""])
    return lines


def write_header(
    output: Path,
    source: Path,
    ref_bases: list[int],
    refs: list[int],
    columns: list[bytes],
) -> None:
    source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
    lines = [
        "/* Generated by tools/gen_neogeo_wall_columns.py. */",
        "#ifndef __DOOM_NEOGEO_WALL_COLUMNS__",
        "#define __DOOM_NEOGEO_WALL_COLUMNS__",
        "",
        '#define DOOM_WALL_ROM __attribute__((section(".text2")))',
        f"#define DOOM_WALL_TEXTURE_COUNT {len(ref_bases)}u",
        f"#define DOOM_WALL_REF_COUNT {len(refs)}u",
        f"#define DOOM_WALL_COLUMN_COUNT {len(columns)}u",
        f'#define DOOM_WALL_SOURCE_SHA256 "{source_hash}"',
        "",
    ]
    lines.extend(format_u16_array("doom_wall_ref_bases", ref_bases))
    lines.extend(format_u16_array("doom_wall_refs", refs))
    lines.append(
        "static const uint8_t "
        f"doom_wall_columns[{len(columns)}][{MAX_COLUMN_HEIGHT}] "
        'DOOM_WALL_ROM __attribute__((aligned(4))) = {'
    )
    for column in columns:
        lines.append("    {")
        for offset in range(0, len(column), 16):
            values = ", ".join(
                f"0x{value:02x}" for value in column[offset : offset + 16]
            )
            lines.append(f"        {values},")
        lines.append("    },")
    lines.extend(["};", "", "#undef DOOM_WALL_ROM", "", "#endif", ""])

    text = "\n".join(lines)
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_text(encoding="ascii") != text:
        output.write_text(text, encoding="ascii")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--embedded-wad-header", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    wad = read_embedded_wad_header(args.embedded_wad_header)
    ref_bases, refs, columns = generate(wad)
    write_header(
        args.output,
        args.embedded_wad_header,
        ref_bases,
        refs,
        columns,
    )
    data_bytes = len(columns) * MAX_COLUMN_HEIGHT
    lookup_bytes = (len(ref_bases) + len(refs)) * 2
    print(
        f"wall columns: {len(refs)} references, {len(columns)} unique, "
        f"{data_bytes + lookup_bytes} ROM bytes"
    )


if __name__ == "__main__":
    main()
