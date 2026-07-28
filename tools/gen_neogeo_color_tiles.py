#!/usr/bin/env python3
"""Generate Neo Geo C-ROM solid-color microframebuffer tiles for Doom64KB.

The Neo Geo sprite backend displays an 80x56 logical framebuffer as 4x4
hardware-sprite cells.  Each logical pixel is represented by one 16x16 sprite
tile shrunk horizontally and vertically to 4x4.

Tile code usage in i_neogev.c:

    tile = MICROFB_TILE_BASE + color_slot

where color_slot is 1..15.  Sprite pixel index 0 is transparent, so tile 0 is a
blank tile and tiles 1..15 are solid tiles using sprite color indices 1..15.
Actual Doom PLAYPAL colors are selected through the SCB1 palette attribute.
"""

from __future__ import annotations

import argparse
from pathlib import Path

VISIBLE_COLOR_SLOTS = 15
TILE_HALF_SIZE = 64       # bytes per C-ROM half per 16x16 tile
START_TILE = 256          # keep the BIOS/ngdevkit eyecatcher area intact
TITLE_TILE = START_TILE + VISIBLE_COLOR_SLOTS + 1
DEFAULT_CROM_SIZE = 2 * 1024 * 1024


def encode_tile(color_index: int) -> tuple[bytes, bytes]:
    """Encode one solid 16x16 tile into Neo Geo C1/C2 halves."""
    color_index = max(0, min(15, color_index))

    # Neo Geo block order as documented by the NeoGeo dev wiki:
    # t1=(x 8..15,y 0..7), t2=(x 8..15,y 8..15),
    # t3=(x 0..7,y 0..7),  t4=(x 0..7,y 8..15).
    c1 = bytearray()
    c2 = bytearray()

    for _block in range(4):
        for _row in range(8):
            plane = [0, 0, 0, 0]
            for px in range(8):
                bit = px
                for bp in range(4):
                    if color_index & (1 << bp):
                        plane[bp] |= 1 << bit
            c1.append(plane[0])
            c1.append(plane[1])
            c2.append(plane[2])
            c2.append(plane[3])

    assert len(c1) == TILE_HALF_SIZE
    assert len(c2) == TILE_HALF_SIZE
    return bytes(c1), bytes(c2)


def load_base(path: Path) -> bytearray:
    data = bytearray(path.read_bytes())
    required = START_TILE * TILE_HALF_SIZE
    if len(data) > required:
        raise ValueError(f"{path} is larger than the reserved {required} bytes")
    data.extend(b"\0" * (required - len(data)))
    return data


def generate(
    base_c1: Path,
    base_c2: Path,
    title_c1: Path,
    title_c2: Path,
    out_c1: Path,
    out_c2: Path,
    crom_size: int,
) -> None:
    c1 = load_base(base_c1)
    c2 = load_base(base_c2)

    # tile START_TILE + 0: fully transparent/blank
    # tile START_TILE + 1..15: solid sprite color index 1..15
    for color_index in range(VISIBLE_COLOR_SLOTS + 1):
        tile_c1, tile_c2 = encode_tile(color_index)
        c1.extend(tile_c1)
        c2.extend(tile_c2)

    if len(c1) != TITLE_TILE * TILE_HALF_SIZE or len(c2) != TITLE_TILE * TILE_HALF_SIZE:
        raise ValueError("solid microtile layout no longer ends at the TITLEPIC tile base")

    title_data_c1 = title_c1.read_bytes()
    title_data_c2 = title_c2.read_bytes()
    if len(title_data_c1) != len(title_data_c2) or len(title_data_c1) % TILE_HALF_SIZE:
        raise ValueError("TITLEPIC C-ROM halves must contain the same number of complete tiles")
    c1.extend(title_data_c1)
    c2.extend(title_data_c2)

    if len(c1) > crom_size or len(c2) > crom_size:
        raise ValueError(
            f"generated C-ROM exceeds {crom_size} bytes per half: "
            f"C1={len(c1)}, C2={len(c2)}"
        )

    c1.extend(b"\0" * (crom_size - len(c1)))
    c2.extend(b"\0" * (crom_size - len(c2)))

    out_c1.parent.mkdir(parents=True, exist_ok=True)
    out_c2.parent.mkdir(parents=True, exist_ok=True)
    out_c1.write_bytes(c1)
    out_c2.write_bytes(c2)

    print(
        f"generated {VISIBLE_COLOR_SLOTS + 1} solid microtiles at tile {START_TILE} "
        f"({len(c1)} bytes per C-ROM half)"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-c1", type=Path, required=True)
    parser.add_argument("--base-c2", type=Path, required=True)
    parser.add_argument("--title-c1", type=Path, required=True)
    parser.add_argument("--title-c2", type=Path, required=True)
    parser.add_argument("--out-c1", type=Path, required=True)
    parser.add_argument("--out-c2", type=Path, required=True)
    parser.add_argument("--crom-size", type=int, default=DEFAULT_CROM_SIZE)
    args = parser.parse_args()

    generate(
        args.base_c1,
        args.base_c2,
        args.title_c1,
        args.title_c2,
        args.out_c1,
        args.out_c2,
        args.crom_size,
    )


if __name__ == "__main__":
    main()
