#!/usr/bin/env python3
"""Host checks for the GX rounded mesh and RGBA8 artwork layout.

These tests intentionally do not depend on libogc or claim a Wii GPU capture.
They mirror the documented data contracts with an independent tile decoder so
asset dimension or pitch regressions are caught before the DOL is run.
"""

from __future__ import annotations

import argparse
import math
import pathlib
import unittest


def rounded_perimeter(x: float, y: float, width: float, height: float, radius: float,
                      segments: int = 6) -> list[tuple[float, float]]:
    if width <= 0 or height <= 0:
        return []
    radius = max(0.0, min(radius, width / 2.0, height / 2.0))
    if radius == 0:
        return [(x, y), (x + width, y), (x + width, y + height), (x, y + height)]
    centers = ((x + width - radius, y + radius),
               (x + width - radius, y + height - radius),
               (x + radius, y + height - radius),
               (x + radius, y + radius))
    points: list[tuple[float, float]] = []
    for corner, center in enumerate(centers):
        start = -math.pi / 2 + corner * math.pi / 2
        for segment in range(segments + 1):
            if corner and not segment:
                continue
            angle = start + segment / segments * math.pi / 2
            points.append((center[0] + math.cos(angle) * radius,
                           center[1] + math.sin(angle) * radius))
    return points


def polygon_area(points: list[tuple[float, float]]) -> float:
    return abs(sum(a[0] * b[1] - a[1] * b[0]
                   for a, b in zip(points, points[1:] + points[:1])) / 2.0)


def triangle_area(a: tuple[float, float], b: tuple[float, float], c: tuple[float, float]) -> float:
    return abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) / 2.0


def point_inside(point: tuple[float, float], polygon: list[tuple[float, float]]) -> bool:
    crossings = 0
    px, py = point
    for first, second in zip(polygon, polygon[1:] + polygon[:1]):
        if (first[1] > py) != (second[1] > py):
            x = first[0] + (py - first[1]) * (second[0] - first[0]) / (second[1] - first[1])
            if x > px:
                crossings += 1
    return bool(crossings & 1)


def align_tile(value: int) -> int:
    return (value + 3) & ~3


def pack_rgba8(source: bytes, width: int, height: int) -> tuple[bytes, int, int]:
    """Pack a tight RGBA source into padded, tiled GX RGBA8 storage."""
    storage_width = align_tile(width + 2)
    storage_height = align_tile(height + 2)
    packed = bytearray(storage_width * storage_height * 4)
    for y in range(storage_height):
        source_y = min(max(y - 1, 0), height - 1)
        for x in range(storage_width):
            source_x = min(max(x - 1, 0), width - 1)
            rgba = source[(source_y * width + source_x) * 4:(source_y * width + source_x + 1) * 4]
            tile_base = ((y // 4) * (storage_width // 4) + x // 4) * 64
            lane = (y % 4) * 4 + x % 4
            packed[tile_base + 2 * lane] = rgba[3]
            packed[tile_base + 2 * lane + 1] = rgba[0]
            packed[tile_base + 32 + 2 * lane] = rgba[1]
            packed[tile_base + 32 + 2 * lane + 1] = rgba[2]
    return bytes(packed), storage_width, storage_height


def unpack_rgba8(packed: bytes, storage_width: int, storage_height: int) -> bytes:
    """Independent block-first RGBA8 decoder used for round-trip validation."""
    decoded = bytearray(storage_width * storage_height * 4)
    offset = 0
    for tile_y in range(storage_height // 4):
        for tile_x in range(storage_width // 4):
            ar = packed[offset:offset + 32]
            gb = packed[offset + 32:offset + 64]
            offset += 64
            for lane in range(16):
                x = tile_x * 4 + lane % 4
                y = tile_y * 4 + lane // 4
                destination = (y * storage_width + x) * 4
                decoded[destination:destination + 4] = bytes((ar[lane * 2 + 1],
                                                                gb[lane * 2],
                                                                gb[lane * 2 + 1],
                                                                ar[lane * 2]))
    return bytes(decoded)


def marker_image(width: int, height: int) -> bytes:
    return bytes(component for y in range(height) for x in range(width)
                 for component in ((x * 31) & 0xFF, (y * 47) & 0xFF,
                                   ((x + y) * 19) & 0xFF, 255 - ((x * 7 + y * 11) & 0x7F)))


class RoundedMeshTests(unittest.TestCase):
    def test_closed_perimeters_cover_the_left_middle(self) -> None:
        for width, height, radius in ((100, 60, 12), (24, 120, 40), (320, 26, 40), (52, 52, 0), (52, 52, 30)):
            points = rounded_perimeter(10, 20, width, height, radius)
            self.assertEqual(len(points), len(set(points)), "perimeter contains duplicate arc joins")
            self.assertTrue(all(10 <= x <= 10 + width and 20 <= y <= 20 + height for x, y in points))
            center = (10 + width / 2, 20 + height / 2)
            fan_area = sum(triangle_area(center, point, points[(index + 1) % len(points)])
                           for index, point in enumerate(points))
            self.assertAlmostEqual(fan_area, polygon_area(points), places=4)
            self.assertTrue(point_inside((10 + min(radius or 1, width / 2), 20 + height / 2), points))

    def test_zero_or_negative_extents_are_rejected(self) -> None:
        self.assertEqual(rounded_perimeter(0, 0, 0, 12, 4), [])
        self.assertEqual(rounded_perimeter(0, 0, 12, -1, 4), [])


class Rgba8TextureTests(unittest.TestCase):
    def test_known_tile_offsets(self) -> None:
        width = 8
        x, y = 6, 5
        tile_base = ((y // 4) * (width // 4) + x // 4) * 64
        lane = (y % 4) * 4 + x % 4
        self.assertEqual((tile_base, lane, tile_base + 2 * lane, tile_base + 32 + 2 * lane),
                         (192, 6, 204, 236))

    def test_round_trip_markers_and_non_multiple_sizes(self) -> None:
        for width, height in ((42, 42), (65, 37), (64, 64), (128, 128)):
            source = marker_image(width, height)
            packed, storage_width, storage_height = pack_rgba8(source, width, height)
            self.assertEqual(storage_width % 4, 0)
            self.assertEqual(storage_height % 4, 0)
            self.assertEqual(len(packed), storage_width * storage_height * 4)
            decoded = unpack_rgba8(packed, storage_width, storage_height)
            for y in range(height):
                for x in range(width):
                    expected = source[(y * width + x) * 4:(y * width + x + 1) * 4]
                    actual = decoded[((y + 1) * storage_width + x + 1) * 4:
                                     ((y + 1) * storage_width + x + 2) * 4]
                    self.assertEqual(actual, expected)

    def test_generated_artwork_round_trips_when_available(self) -> None:
        if ARTWORK_DIRECTORY is None:
            self.skipTest("generated assets were not supplied")
        for name, width, height in ARTWORK_SIZES:
            source = (ARTWORK_DIRECTORY / f"{name}.rgba").read_bytes()
            packed, storage_width, storage_height = pack_rgba8(source, width, height)
            decoded = unpack_rgba8(packed, storage_width, storage_height)
            reconstructed = bytearray()
            for y in range(height):
                start = ((y + 1) * storage_width + 1) * 4
                reconstructed.extend(decoded[start:start + width * 4])
            self.assertEqual(bytes(reconstructed), source, name)


ARTWORK_DIRECTORY: pathlib.Path | None = None
ARTWORK_SIZES = (("hero", 260, 316), ("luca", 42, 42), ("sora", 42, 42),
                 ("mika", 42, 42), ("rex", 42, 42), ("nina", 42, 42), ("tobi", 42, 42))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--artwork-dir", type=pathlib.Path)
    args, remaining = parser.parse_known_args()
    global ARTWORK_DIRECTORY
    if args.artwork_dir:
        ARTWORK_DIRECTORY = args.artwork_dir
        for name, width, height in ARTWORK_SIZES:
            raw = args.artwork_dir / f"{name}.rgba"
            if not raw.exists() or raw.stat().st_size != width * height * 4:
                raise SystemExit(f"invalid generated artwork asset: {raw}")
    unittest.main(argv=[__file__, *remaining])


if __name__ == "__main__":
    main()
