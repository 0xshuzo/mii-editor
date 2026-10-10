#!/usr/bin/env python3
"""Crop the reference-only Mii artwork into build-time GX source textures."""

from __future__ import annotations

import argparse
import pathlib
import struct
import zlib


def read_png(path: pathlib.Path) -> tuple[int, int, bytearray]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path} is not a PNG file")
    position = 8
    width = height = color_type = bit_depth = None
    compressed = bytearray()
    while position < len(data):
        size = struct.unpack(">I", data[position:position + 4])[0]
        chunk = data[position + 4:position + 8]
        payload = data[position + 8:position + 8 + size]
        position += size + 12
        if chunk == b"IHDR":
            width, height, bit_depth, color_type, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
            if (bit_depth, color_type, compression, filtering, interlace) not in ((8, 6, 0, 0, 0), (8, 2, 0, 0, 0)):
                raise ValueError("only non-interlaced RGB/RGBA PNGs are supported")
        elif chunk == b"IDAT":
            compressed.extend(payload)
        elif chunk == b"IEND":
            break
    if width is None or height is None or color_type is None:
        raise ValueError("PNG header missing")
    channels = 4 if color_type == 6 else 3
    stride = width * channels
    filtered = zlib.decompress(compressed)
    if len(filtered) != (stride + 1) * height:
        raise ValueError("unexpected PNG data length")
    rows = bytearray(height * stride)
    previous = bytearray(stride)
    offset = 0
    for y in range(height):
        filter_type = filtered[offset]
        offset += 1
        row = bytearray(filtered[offset:offset + stride])
        offset += stride
        for x in range(stride):
            left = row[x - channels] if x >= channels else 0
            up = previous[x]
            up_left = previous[x - channels] if x >= channels else 0
            if filter_type == 1:
                row[x] = (row[x] + left) & 0xFF
            elif filter_type == 2:
                row[x] = (row[x] + up) & 0xFF
            elif filter_type == 3:
                row[x] = (row[x] + ((left + up) >> 1)) & 0xFF
            elif filter_type == 4:
                p = left + up - up_left
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - up_left)
                row[x] = (row[x] + (left if pa <= pb and pa <= pc else up if pb <= pc else up_left)) & 0xFF
            elif filter_type != 0:
                raise ValueError(f"unsupported PNG filter {filter_type}")
        rows[y * stride:(y + 1) * stride] = row
        previous = row
    rgba = bytearray(width * height * 4)
    for pixel in range(width * height):
        rgba[pixel * 4:pixel * 4 + 3] = rows[pixel * channels:pixel * channels + 3]
        rgba[pixel * 4 + 3] = 255 if channels == 3 else rows[pixel * channels + 3]
    return width, height, rgba


def crop_resize(source: bytearray, source_width: int, left: int, top: int, right: int, bottom: int,
                output_width: int, output_height: int, radius: int) -> bytearray:
    output = bytearray(output_width * output_height * 4)
    crop_width, crop_height = right - left, bottom - top
    for y in range(output_height):
        source_y = top + min(crop_height - 1, int((y + 0.5) * crop_height / output_height))
        for x in range(output_width):
            source_x = left + min(crop_width - 1, int((x + 0.5) * crop_width / output_width))
            target = (y * output_width + x) * 4
            origin = (source_y * source_width + source_x) * 4
            output[target:target + 4] = source[origin:origin + 4]
            if radius and ((x < radius and y < radius and (x - radius) ** 2 + (y - radius) ** 2 > radius ** 2) or
                           (x >= output_width - radius and y < radius and (x - (output_width - radius - 1)) ** 2 + (y - radius) ** 2 > radius ** 2) or
                           (x < radius and y >= output_height - radius and (x - radius) ** 2 + (y - (output_height - radius - 1)) ** 2 > radius ** 2) or
                           (x >= output_width - radius and y >= output_height - radius and (x - (output_width - radius - 1)) ** 2 + (y - (output_height - radius - 1)) ** 2 > radius ** 2)):
                output[target + 3] = 0
    return output


def write_png(path: pathlib.Path, width: int, height: int, pixels: bytearray) -> None:
    def chunk(kind: bytes, payload: bytes) -> bytes:
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    rows = b"".join(b"\0" + bytes(pixels[y * width * 4:(y + 1) * width * 4]) for y in range(height))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
                     chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def write_metadata(path: pathlib.Path, assets: list[tuple[str, int, int]]) -> None:
    """Emit dimensions beside the raw data that the linker embeds.

    Keeping this generated avoids a hand-maintained C++ width/height table that
    can silently disagree with a changed crop or conversion output.
    """
    entries = ",\n".join(
        f"    ArtworkMetadata{{{width}, {height}, {width * height * 4}U}}"
        for _, width, height in assets
    )
    path.write_text(
        "#pragma once\n\n"
        "#include <array>\n#include <cstddef>\n\n"
        "namespace mii::ui::generated {\n"
        "struct ArtworkMetadata { int width; int height; std::size_t byteCount; };\n"
        f"inline constexpr std::array<ArtworkMetadata, {len(assets)}> kArtworkMetadata{{{{\n"
        f"{entries}\n"
        "}};\n}  // namespace mii::ui::generated\n",
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=pathlib.Path, required=True)
    parser.add_argument("--out", type=pathlib.Path, required=True)
    parser.add_argument("--cursor-source", type=pathlib.Path, required=True)
    parser.add_argument("--cursor-shape", type=pathlib.Path, required=True)
    args = parser.parse_args()
    width, height, pixels = read_png(args.source)
    if (width, height) != (1672, 941):
        raise ValueError(f"expected the supplied 1672x941 target, got {width}x{height}")
    args.out.mkdir(parents=True, exist_ok=True)
    crops = {
        "hero": (623, 176, 1132, 792, 260, 316, 12),
        "luca": (86, 182, 168, 264, 42, 42, 7),
        "sora": (86, 288, 168, 370, 42, 42, 7),
        "mika": (86, 394, 168, 476, 42, 42, 7),
        "rex": (86, 500, 168, 582, 42, 42, 7),
        "nina": (86, 606, 168, 688, 42, 42, 7),
        "tobi": (86, 712, 168, 794, 42, 42, 7),
    }
    generated_assets: list[tuple[str, int, int]] = []
    for name, dimensions in crops.items():
        artwork = crop_resize(pixels, width, *dimensions)
        args.out.joinpath(f"{name}.rgba").write_bytes(artwork)
        write_png(args.out.joinpath(f"{name}_preview.png"), dimensions[4], dimensions[5], artwork)
        generated_assets.append((name, dimensions[4], dimensions[5]))

    shape_width, shape_height, _ = read_png(args.cursor_shape)
    if (shape_width, shape_height) != (500, 500):
        raise ValueError(f"expected the supplied 500x500 cursor silhouette, got {shape_width}x{shape_height}")
    cursor_width, cursor_height, cursor_pixels = read_png(args.cursor_source)
    if (cursor_width, cursor_height) != (1672, 941):
        raise ValueError(f"expected the supplied 1672x941 cursor sheet, got {cursor_width}x{cursor_height}")
    # The default panel is used as the single master: black sheet pixels become transparent.
    hand = crop_resize(cursor_pixels, cursor_width, 62, 72, 370, 452, 128, 128, 0)
    mask = bytearray(len(hand))
    for index in range(0, len(hand), 4):
        brightest = max(hand[index], hand[index + 1], hand[index + 2])
        if brightest < 18:
            hand[index + 3] = 0
            continue
        hand[index + 3] = 255
        mask[index:index + 4] = bytes((255, 255, 255, 255))
    args.out.joinpath("cursor_hand.rgba").write_bytes(hand)
    args.out.joinpath("cursor_mask.rgba").write_bytes(mask)
    generated_assets.extend((("cursor_hand", 128, 128), ("cursor_mask", 128, 128)))
    write_png(args.out.joinpath("cursor_hand_preview.png"), 128, 128, hand)
    write_png(args.out.joinpath("cursor_mask_preview.png"), 128, 128, mask)
    write_metadata(args.out.joinpath("ui_artwork_metadata.hpp"), generated_assets)


if __name__ == "__main__":
    main()
