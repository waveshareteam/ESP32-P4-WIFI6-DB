"""Small dependency-free renderer for original 112x112 launcher icons."""

from __future__ import annotations

import binascii
import math
import struct
import zlib
from pathlib import Path
from typing import Callable, Iterable, Sequence


Color = tuple[int, int, int, int]
Paint = Color | Callable[[float, float], Color]
Point = tuple[float, float]


def _clamp_u8(value: float) -> int:
    return max(0, min(255, int(round(value))))


def vertical_gradient(top: Color, bottom: Color, y0: float, y1: float) -> Paint:
    """Return a vertical RGBA gradient paint."""

    span = max(1.0, y1 - y0)

    def paint(_x: float, y: float) -> Color:
        ratio = max(0.0, min(1.0, (y - y0) / span))
        return tuple(
            _clamp_u8(start + (end - start) * ratio)
            for start, end in zip(top, bottom)
        )  # type: ignore[return-value]

    return paint


def highlighted_gradient(
    top: Color,
    bottom: Color,
    y0: float,
    y1: float,
    highlight_center: Point,
    highlight_radius: float,
    highlight_strength: int,
) -> Paint:
    """Return a vertical gradient with a restrained radial highlight."""

    base = vertical_gradient(top, bottom, y0, y1)
    center_x, center_y = highlight_center

    def paint(x: float, y: float) -> Color:
        red, green, blue, alpha = base(x, y)
        distance = math.hypot(x - center_x, y - center_y)
        lift = max(0.0, 1.0 - distance / highlight_radius) * highlight_strength
        return (
            _clamp_u8(red + lift),
            _clamp_u8(green + lift),
            _clamp_u8(blue + lift),
            alpha,
        )

    return paint


class Canvas:
    """Supersampled premultiplied-alpha raster canvas."""

    def __init__(self, size: int = 112, scale: int = 4) -> None:
        self.size = size
        self.scale = scale
        self.high_size = size * scale
        self._pixels = [0.0] * (self.high_size * self.high_size * 4)

    def draw(
        self,
        predicate: Callable[[float, float], bool],
        bounds: tuple[float, float, float, float],
        paint: Paint,
    ) -> None:
        x0, y0, x1, y1 = bounds
        start_x = max(0, int(math.floor(x0 * self.scale)))
        start_y = max(0, int(math.floor(y0 * self.scale)))
        end_x = min(self.high_size, int(math.ceil(x1 * self.scale)))
        end_y = min(self.high_size, int(math.ceil(y1 * self.scale)))
        paint_fn = paint if callable(paint) else lambda _x, _y: paint

        for pixel_y in range(start_y, end_y):
            y = (pixel_y + 0.5) / self.scale
            for pixel_x in range(start_x, end_x):
                x = (pixel_x + 0.5) / self.scale
                if not predicate(x, y):
                    continue

                red, green, blue, alpha_u8 = paint_fn(x, y)
                source_alpha = alpha_u8 / 255.0
                if source_alpha <= 0.0:
                    continue

                offset = (pixel_y * self.high_size + pixel_x) * 4
                inverse_alpha = 1.0 - source_alpha
                self._pixels[offset] = (
                    red / 255.0 * source_alpha
                    + self._pixels[offset] * inverse_alpha
                )
                self._pixels[offset + 1] = (
                    green / 255.0 * source_alpha
                    + self._pixels[offset + 1] * inverse_alpha
                )
                self._pixels[offset + 2] = (
                    blue / 255.0 * source_alpha
                    + self._pixels[offset + 2] * inverse_alpha
                )
                self._pixels[offset + 3] = (
                    source_alpha + self._pixels[offset + 3] * inverse_alpha
                )

    def rounded_rect(
        self,
        bounds: tuple[float, float, float, float],
        radius: float,
        paint: Paint,
    ) -> None:
        x0, y0, x1, y1 = bounds
        radius = min(radius, (x1 - x0) / 2.0, (y1 - y0) / 2.0)

        def predicate(x: float, y: float) -> bool:
            closest_x = min(max(x, x0 + radius), x1 - radius)
            closest_y = min(max(y, y0 + radius), y1 - radius)
            delta_x = x - closest_x
            delta_y = y - closest_y
            return delta_x * delta_x + delta_y * delta_y <= radius * radius

        self.draw(predicate, bounds, paint)

    def ellipse(
        self,
        bounds: tuple[float, float, float, float],
        paint: Paint,
    ) -> None:
        x0, y0, x1, y1 = bounds
        center_x = (x0 + x1) / 2.0
        center_y = (y0 + y1) / 2.0
        radius_x = max(0.001, (x1 - x0) / 2.0)
        radius_y = max(0.001, (y1 - y0) / 2.0)

        def predicate(x: float, y: float) -> bool:
            delta_x = (x - center_x) / radius_x
            delta_y = (y - center_y) / radius_y
            return delta_x * delta_x + delta_y * delta_y <= 1.0

        self.draw(predicate, bounds, paint)

    def polygon(self, points: Sequence[Point], paint: Paint) -> None:
        if len(points) < 3:
            raise ValueError("a polygon needs at least three points")

        xs = [point[0] for point in points]
        ys = [point[1] for point in points]
        bounds = (min(xs), min(ys), max(xs), max(ys))

        def predicate(x: float, y: float) -> bool:
            inside = False
            previous_x, previous_y = points[-1]
            for current_x, current_y in points:
                crosses = (current_y > y) != (previous_y > y)
                if crosses:
                    intersection_x = (
                        (previous_x - current_x)
                        * (y - current_y)
                        / (previous_y - current_y)
                        + current_x
                    )
                    if x < intersection_x:
                        inside = not inside
                previous_x, previous_y = current_x, current_y
            return inside

        self.draw(predicate, bounds, paint)

    def rgba_bytes(self) -> bytes:
        """Downsample the premultiplied high-resolution buffer to RGBA8888."""

        output = bytearray(self.size * self.size * 4)
        sample_count = self.scale * self.scale
        for y in range(self.size):
            for x in range(self.size):
                premultiplied = [0.0, 0.0, 0.0, 0.0]
                for sample_y in range(self.scale):
                    high_y = y * self.scale + sample_y
                    for sample_x in range(self.scale):
                        high_x = x * self.scale + sample_x
                        offset = (high_y * self.high_size + high_x) * 4
                        for channel in range(4):
                            premultiplied[channel] += self._pixels[offset + channel]

                premultiplied = [
                    value / sample_count for value in premultiplied
                ]
                alpha = premultiplied[3]
                output_offset = (y * self.size + x) * 4
                if alpha > 0.0:
                    output[output_offset] = _clamp_u8(
                        premultiplied[0] / alpha * 255.0
                    )
                    output[output_offset + 1] = _clamp_u8(
                        premultiplied[1] / alpha * 255.0
                    )
                    output[output_offset + 2] = _clamp_u8(
                        premultiplied[2] / alpha * 255.0
                    )
                output[output_offset + 3] = _clamp_u8(alpha * 255.0)
        return bytes(output)


def _png_chunk(chunk_type: bytes, data: bytes) -> bytes:
    checksum = binascii.crc32(chunk_type + data) & 0xFFFFFFFF
    return (
        struct.pack(">I", len(data))
        + chunk_type
        + data
        + struct.pack(">I", checksum)
    )


def write_rgba_png(path: Path, width: int, height: int, rgba: bytes) -> None:
    """Write an 8-bit non-interlaced RGBA PNG using only the standard library."""

    expected_size = width * height * 4
    if len(rgba) != expected_size:
        raise ValueError(f"expected {expected_size} RGBA bytes, got {len(rgba)}")

    stride = width * 4
    rows = [
        b"\x00" + rgba[row * stride : (row + 1) * stride]
        for row in range(height)
    ]
    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png.extend(
        _png_chunk(
            b"IHDR",
            struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0),
        )
    )
    png.extend(
        _png_chunk(
            b"tEXt",
            b"Comment\x00Original procedural geometry; no external image assets",
        )
    )
    png.extend(_png_chunk(b"IDAT", zlib.compress(b"".join(rows), level=9)))
    png.extend(_png_chunk(b"IEND", b""))
    path.write_bytes(bytes(png))


def _format_c_bytes(data: Iterable[int]) -> str:
    values = list(data)
    lines = []
    for offset in range(0, len(values), 16):
        line = ", ".join(f"0x{value:02x}" for value in values[offset : offset + 16])
        lines.append(f"  {line},")
    return "\n".join(lines)


def write_lvgl_argb8888_c(
    path: Path,
    symbol: str,
    attribute: str,
    width: int,
    height: int,
    rgba: bytes,
) -> None:
    """Write LVGL 9 ARGB8888 data in the little-endian B,G,R,A byte order."""

    bgra = bytearray(len(rgba))
    bgra[0::4] = rgba[2::4]
    bgra[1::4] = rgba[1::4]
    bgra[2::4] = rgba[0::4]
    bgra[3::4] = rgba[3::4]

    source = f"""/* Generated from original procedural geometry by tools/gen_icon.py. */
#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef {attribute}
#define {attribute}
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST {attribute} uint8_t {symbol}_map[] = {{
{_format_c_bytes(bgra)}
}};

const lv_image_dsc_t {symbol} = {{
  .header.cf = LV_COLOR_FORMAT_ARGB8888,
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.w = {width},
  .header.h = {height},
  .data_size = {len(bgra)},
  .data = {symbol}_map,
}};
"""
    path.write_text(source, encoding="utf-8", newline="\n")
