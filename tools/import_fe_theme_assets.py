#!/usr/bin/env python3
"""Convert selected FE-Repo backgrounds into raw BGR assets and previews."""

from __future__ import annotations

import argparse
import os
import struct
import subprocess
import tempfile
import zlib
from pathlib import Path
from typing import NamedTuple, Optional


SOURCE_IMAGE_WIDTH = 256
SOURCE_IMAGE_HEIGHT = 160
RAW_WIDTH = 128
RAW_HEIGHT = 80
# FE battle backgrounds are 240px-wide GBA art with 16px of right-side padding
# inside the 256px source PNG.
VISIBLE_CROP_WIDTH = 240
VISIBLE_CROP_HEIGHT = 160
DARKEN_PERMILLE = 430
SCREEN_DARKEN_PERMILLE = 850
TOP_PREVIEW_WIDTH = 400
BOTTOM_PREVIEW_WIDTH = 320
PREVIEW_HEIGHT = 240
PREVIEW_GRID_COLUMNS = 2
LAYER_PREVIEW_GRID_COLUMNS = 4
PREVIEW_GRID_GAP = 8
UI_DIALOG_SHADOW_BGR = bytes((0x25, 0x36, 0x38))
UI_DIALOG_OUTLINE_BGR = bytes((0x1E, 0x2E, 0x2F))
UI_DIALOG_BORDER_DARK_BGR = bytes((0x24, 0x48, 0x52))
UI_DIALOG_BORDER_BGR = bytes((0x42, 0x76, 0x78))
UI_DIALOG_BORDER_MID_BGR = bytes((0x58, 0x85, 0x82))
UI_DIALOG_BORDER_LIGHT_BGR = bytes((0x99, 0xE6, 0xF4))
UI_DIALOG_FILL_BGR = bytes((0x8A, 0xDA, 0xE6))
UI_DIALOG_FILL_ALT_BGR = bytes((0x7C, 0xCF, 0xDB))
UI_DIALOG_FILL_LIGHT_BGR = bytes((0xA8, 0xEA, 0xF6))
UI_DIALOG_FILL_SHADE_BGR = bytes((0x67, 0xB3, 0xB8))
UI_DIALOG_SCROLL_SHADE_BGR = bytes((0x58, 0x93, 0x98))
UI_DIALOG_TEXT_BGR = bytes((0x08, 0x12, 0x20))
UI_DIALOG_TEXT_SHADOW_BGR = bytes((0x9C, 0xE2, 0xEE))
UI_DIALOG_REFERENCE_PALETTE_BGR = (
    UI_DIALOG_OUTLINE_BGR,
    UI_DIALOG_BORDER_DARK_BGR,
    UI_DIALOG_BORDER_BGR,
    UI_DIALOG_BORDER_MID_BGR,
    UI_DIALOG_BORDER_LIGHT_BGR,
    UI_DIALOG_FILL_SHADE_BGR,
    UI_DIALOG_FILL_BGR,
    UI_DIALOG_FILL_LIGHT_BGR,
)
UI_TITLE_BGR = bytes((0xB8, 0xD8, 0xF8))
UI_TITLE_SHADOW_BGR = bytes((0x18, 0x20, 0x38))
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
UI_FRAME_RELATIVE_PATH = (
    "BGs, Interface Elements/Battle Frames & Backgrounds/"
    "{Cynon} Chrono Trigger Inspired Battle Frames {F2E}/"
    "Full Battle Screen (Variant 1).png"
)
UI_CARD_FRAME_RELATIVE_PATH = (
    "BGs, Interface Elements/Battle Frames & Backgrounds/"
    "Sokaballa's Battle Screen/Full Battle Screen.png"
)
UI_FONT_RELATIVE_PATH = (
    "BGs, Interface Elements/Vanilla Fonts & Logos & Save Slots/"
    "FE7-FE8/Checkmate by Bob Quinzel/Checkmate.otf"
)
FE8_FONT_MAP_RELATIVE_PATH = Path(
    "assets/fe-themes/font/fe8_vanilla_fontMap.png"
)
FE8_FONT_MAP_SOURCE_URL = (
    "https://feuniverse.us/t/fe8u-editing-menu-dialogue-font-glyphs-draft/6716"
)
UI_DIALOG_REFERENCE_RELATIVE_PATH = Path(
    "assets/fe-themes/reference/fe_dialogue_warning_275x183.png"
)
UI_DIALOG_REFERENCE_PANEL_RECT = (55, 37, 166, 99)
UI_DIALOG_REFERENCE_SLICE_LEFT = 9
UI_DIALOG_REFERENCE_SLICE_TOP = 7
UI_DIALOG_REFERENCE_SLICE_RIGHT = 9
UI_DIALOG_REFERENCE_SLICE_BOTTOM = 8
UI_OLD_PARCHMENT_RELATIVE_PATH = Path(
    "assets/fe-themes/external/opengameart/old-parchment-paper/"
    "parchment_alpha.png"
)
UI_PARCHMENT_GUI_RELATIVE_PATH = Path(
    "assets/fe-themes/external/opengameart/parchment-gui/panels.png"
)
UI_PARCHMENT_GUI_PANEL_RECT = (48, 0, 48, 48)
UI_PARCHMENT_GUI_SLICE = 16
FE8_FONT_GRID_BGR = bytes((0x8C, 0x61, 0x51))
FE8_FONT_HEADER_BGR = bytes((0x80, 0xA0, 0x80))
FE8_FONT_BACKGROUND_MIN = 232
FE8_FONT_COLUMNS = 16
FE8_FONT_FIRST_ROW = 0x2
FE8_FONT_ROWS = 14
FE8_FONT_SOURCE_GLYPH_WIDTH = 16
FE8_FONT_SOURCE_GLYPH_HEIGHT = 16
FE8_FONT_TEXT_SHADE_BGR = bytes((0x72, 0x72, 0x72))
TERMINAL_FONT_FAMILY = "Hack Nerd Font Mono"
TERMINAL_FONT_SIZE = 14
TERMINAL_FONT_KEY_RGB = "ffffff"
TERMINAL_FONT_KEY_BGR = bytes((0xFF, 0xFF, 0xFF))
TERMINAL_FONT_PATH_CANDIDATES = (
    Path("~/Library/Fonts/HackNerdFontMono-Regular.ttf"),
    Path("/Library/Fonts/HackNerdFontMono-Regular.ttf"),
    Path("/System/Library/Fonts/HackNerdFontMono-Regular.ttf"),
    Path("/opt/homebrew/share/fonts/HackNerdFontMono-Regular.ttf"),
)
UI_ICON_RELATIVE_PATHS = (
    "BGs, Interface Elements/Text Characters/{JeyTheCount} Sword Icon [F2E].png",
    "BGs, Interface Elements/Text Characters/{JeyTheCount} Staff Icon [F2E].png",
    "BGs, Interface Elements/Text Characters/{JeyTheCount} Anima Icon [F2E].png",
)
UI_FONT_KEY_RGB = "b400b4"
UI_FONT_KEY_BGR = bytes((0xB4, 0x00, 0xB4))
UI_FONT_ATLAS_FILENAME = "fe_font_review_8x14_alpha.bin"
UI_FONT_ATLAS_FIRST_CHAR = 32
UI_FONT_ATLAS_CHAR_COUNT = 95
UI_FONT_ATLAS_GLYPH_WIDTH = 8
UI_FONT_ATLAS_GLYPH_HEIGHT = 14
UI_FONT_ATLAS_RENDER_SIZE = 14
UI_CARD_TEXT_BGR = bytes((0xC8, 0xF0, 0xF8))
UI_CARD_TEXT_SHADOW_BGR = bytes((0x18, 0x20, 0x38))
UI_CARD_SHADOW_BGR = bytes((0x0D, 0x1F, 0x4A))
UI_CARD_DARK_BGR = bytes((0x10, 0x33, 0x77))
UI_CARD_FILL_BGR = bytes((0x16, 0x58, 0xB9))
UI_CARD_FILL_ALT_BGR = bytes((0x1C, 0x68, 0xC9))
UI_CARD_HIGHLIGHT_BGR = bytes((0x28, 0x92, 0xE8))
UI_CARD_TRIM_BGR = bytes((0x32, 0x63, 0x7A))
UI_CARD_TRIM_BRIGHT_BGR = bytes((0xB0, 0xF0, 0xFF))
UI_CARD_TRIM_SHADOW_BGR = bytes((0x18, 0x36, 0x4A))
UI_CARD_TRIM_GOLD_BGR = bytes((0x40, 0xC8, 0xF0))
UI_CARD_TRIM_DARK_BGR = bytes((0x10, 0x1C, 0x2A))
UI_PIXEL_FONT = {
    "0": ("111", "101", "101", "101", "111"),
    "1": ("010", "110", "010", "010", "111"),
    "2": ("111", "001", "111", "100", "111"),
    "3": ("111", "001", "111", "001", "111"),
    "A": ("01110", "10001", "11111", "10001", "10001"),
    "B": ("11110", "10001", "11110", "10001", "11110"),
    "C": ("01111", "10000", "10000", "10000", "01111"),
    "D": ("11110", "10001", "10001", "10001", "11110"),
    "E": ("11111", "10000", "11110", "10000", "11111"),
    "F": ("11111", "10000", "11110", "10000", "10000"),
    "H": ("10001", "10001", "11111", "10001", "10001"),
    "I": ("111", "010", "010", "010", "111"),
    "K": ("10001", "10010", "11100", "10010", "10001"),
    "N": ("10001", "11001", "10101", "10011", "10001"),
    "O": ("01110", "10001", "10001", "10001", "01110"),
    "R": ("11110", "10001", "11110", "10010", "10001"),
    "S": ("11111", "10000", "11110", "00001", "11110"),
    "T": ("11111", "00100", "00100", "00100", "00100"),
    "W": ("10001", "10001", "10101", "10101", "01010"),
}
UI_PIXEL_FALLBACK_GLYPH = ("111", "001", "010", "000", "010")
UI_PIXEL_FONT.update(
    {
        "G": ("01111", "10000", "10111", "10001", "01111"),
        "J": ("00111", "00010", "00010", "10010", "01100"),
        "L": ("10000", "10000", "10000", "10000", "11111"),
        "M": ("10001", "11011", "10101", "10001", "10001"),
        "P": ("11110", "10001", "11110", "10000", "10000"),
        "Q": ("01110", "10001", "10001", "10011", "01111"),
        "U": ("10001", "10001", "10001", "10001", "01110"),
        "V": ("10001", "10001", "10001", "01010", "00100"),
        "X": ("10001", "01010", "00100", "01010", "10001"),
        "Y": ("10001", "01010", "00100", "00100", "00100"),
        "Z": ("11111", "00010", "00100", "01000", "11111"),
        "4": ("101", "101", "111", "001", "001"),
        "5": ("111", "100", "111", "001", "111"),
        "6": ("111", "100", "111", "101", "111"),
        "7": ("111", "001", "010", "010", "010"),
        "8": ("111", "101", "111", "101", "111"),
        "9": ("111", "101", "111", "001", "111"),
        "a": ("0000", "0110", "0001", "0111", "0111"),
        "b": ("1000", "1000", "1110", "1001", "1110"),
        "c": ("0000", "0111", "1000", "1000", "0111"),
        "d": ("0001", "0001", "0111", "1001", "0111"),
        "e": ("0000", "0110", "1111", "1000", "0111"),
        "f": ("0011", "0100", "1110", "0100", "0100"),
        "g": ("0000", "0111", "1001", "0111", "0001", "1110"),
        "h": ("1000", "1000", "1110", "1001", "1001"),
        "i": ("010", "000", "110", "010", "111"),
        "j": ("001", "000", "001", "001", "101", "010"),
        "k": ("1000", "1001", "1010", "1100", "1010", "1001"),
        "l": ("110", "010", "010", "010", "111"),
        "m": ("00000", "11010", "10101", "10101", "10101"),
        "n": ("0000", "1110", "1001", "1001", "1001"),
        "o": ("0000", "0110", "1001", "1001", "0110"),
        "p": ("0000", "1110", "1001", "1110", "1000", "1000"),
        "q": ("0000", "0111", "1001", "0111", "0001", "0001"),
        "r": ("0000", "1011", "1100", "1000", "1000"),
        "s": ("0000", "0111", "1100", "0011", "1110"),
        "t": ("0100", "1110", "0100", "0100", "0011"),
        "u": ("0000", "1001", "1001", "1001", "0111"),
        "v": ("0000", "1001", "1001", "0110", "0110"),
        "w": ("00000", "10101", "10101", "10101", "01010"),
        "x": ("0000", "1001", "0110", "0110", "1001"),
        "y": ("0000", "1001", "1001", "0111", "0001", "1110"),
        "z": ("0000", "1111", "0010", "0100", "1111"),
        ".": ("0", "0", "0", "0", "1"),
        ",": ("0", "0", "0", "1", "1", "10"),
        ":": ("0", "1", "0", "1", "0"),
        ";": ("0", "1", "0", "1", "1", "10"),
        "!": ("1", "1", "1", "0", "1"),
        "?": ("111", "001", "011", "000", "010"),
        "'": ("1", "1", "0"),
        "\"": ("101", "101", "000"),
        "-": ("000", "000", "111", "000", "000"),
        "/": ("001", "001", "010", "100", "100"),
        "(": ("01", "10", "10", "10", "01"),
        ")": ("10", "01", "01", "01", "10"),
        "[": ("11", "10", "10", "10", "11"),
        "]": ("11", "01", "01", "01", "11"),
    }
)

THEMES = (
    (
        "amber",
        "Ballroom",
        "WAve",
        "BGs, Interface Elements/Background CGs/"
        "WAve's BGs {WAve} [F2E]/Ballroom.png",
    ),
    (
        "forest",
        "Grassland",
        "WAve",
        "BGs, Interface Elements/Background CGs/"
        "WAve's BGs {WAve} [F2E]/Grassland.png",
    ),
    (
        "ruby",
        "Red Castle",
        "WAve",
        "BGs, Interface Elements/Background CGs/"
        "WAve's BGs {WAve} [F2E]/Red Castle.png",
    ),
    (
        "chalk",
        "Blue Castle",
        "WAve",
        "BGs, Interface Elements/Background CGs/"
        "WAve's BGs {WAve} [F2E]/Blue Castle.png",
    ),
)


def paeth_predictor(left: int, above: int, upper_left: int) -> int:
    estimate = left + above - upper_left
    left_distance = abs(estimate - left)
    above_distance = abs(estimate - above)
    upper_left_distance = abs(estimate - upper_left)

    if left_distance <= above_distance and left_distance <= upper_left_distance:
        return left
    if above_distance <= upper_left_distance:
        return above

    return upper_left


def unfilter_png_scanline(
    filter_type: int,
    scanline: bytes,
    previous_scanline: bytes,
    bytes_per_pixel: int,
) -> bytes:
    output = bytearray(scanline)

    for index, value in enumerate(output):
        left = output[index - bytes_per_pixel] if index >= bytes_per_pixel else 0
        above = previous_scanline[index] if previous_scanline else 0
        upper_left = (
            previous_scanline[index - bytes_per_pixel]
            if previous_scanline and index >= bytes_per_pixel
            else 0
        )

        if filter_type == 0:
            continue
        if filter_type == 1:
            output[index] = (value + left) & 0xFF
        elif filter_type == 2:
            output[index] = (value + above) & 0xFF
        elif filter_type == 3:
            output[index] = (value + ((left + above) // 2)) & 0xFF
        elif filter_type == 4:
            output[index] = (value + paeth_predictor(left, above, upper_left)) & 0xFF
        else:
            raise ValueError(f"unsupported PNG filter type {filter_type}")

    return bytes(output)


def read_png_8bit_bgr(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if data[:len(PNG_SIGNATURE)] != PNG_SIGNATURE:
        raise ValueError(f"{path} is not a PNG file")

    offset = len(PNG_SIGNATURE)
    width = 0
    height = 0
    bit_depth = 0
    color_type = 0
    compression_method = 0
    filter_method = 0
    interlace_method = 0
    palette: list[tuple[int, int, int]] = []
    compressed = bytearray()

    while offset + 12 <= len(data):
        chunk_length = struct.unpack_from(">I", data, offset)[0]
        chunk_type = data[offset + 4:offset + 8]
        chunk_start = offset + 8
        chunk_end = chunk_start + chunk_length
        if chunk_end + 4 > len(data):
            raise ValueError(f"{path} has a truncated PNG chunk")
        chunk_data = data[chunk_start:chunk_end]
        offset = chunk_end + 4

        if chunk_type == b"IHDR":
            (
                width,
                height,
                bit_depth,
                color_type,
                compression_method,
                filter_method,
                interlace_method,
            ) = struct.unpack(">IIBBBBB", chunk_data)
        elif chunk_type == b"PLTE":
            if len(chunk_data) % 3 != 0:
                raise ValueError(f"{path} has a malformed PNG palette")
            palette = [
                (chunk_data[index], chunk_data[index + 1], chunk_data[index + 2])
                for index in range(0, len(chunk_data), 3)
            ]
        elif chunk_type == b"IDAT":
            compressed.extend(chunk_data)
        elif chunk_type == b"IEND":
            break

    if width <= 0 or height <= 0:
        raise ValueError(f"{path} is missing a PNG IHDR chunk")
    if compression_method != 0 or filter_method != 0 or interlace_method != 0:
        raise ValueError(f"{path} uses unsupported PNG compression/filter/interlace")

    if color_type == 3:
        if bit_depth not in (1, 2, 4, 8):
            raise ValueError(f"{path} uses unsupported indexed bit depth {bit_depth}")
        bytes_per_pixel = 1
    elif color_type == 2:
        if bit_depth != 8:
            raise ValueError(f"{path} must be an 8-bit truecolor PNG")
        bytes_per_pixel = 3
    elif color_type == 6:
        if bit_depth != 8:
            raise ValueError(f"{path} must be an 8-bit truecolor alpha PNG")
        bytes_per_pixel = 4
    else:
        raise ValueError(f"{path} uses unsupported PNG color type {color_type}")

    raw = zlib.decompress(bytes(compressed))
    if color_type == 3:
        scanline_length = (width * bit_depth + 7) // 8
    else:
        scanline_length = width * bytes_per_pixel
    expected_length = height * (1 + scanline_length)
    if len(raw) != expected_length:
        raise ValueError(
            f"{path} decoded to {len(raw)} bytes, expected {expected_length}"
        )

    pixels_bgr = bytearray(width * height * 3)
    previous_scanline = b""
    raw_offset = 0
    for y in range(height):
        filter_type = raw[raw_offset]
        raw_offset += 1
        filtered_scanline = raw[raw_offset:raw_offset + scanline_length]
        raw_offset += scanline_length
        scanline = unfilter_png_scanline(
            filter_type,
            filtered_scanline,
            previous_scanline,
            bytes_per_pixel,
        )

        for x in range(width):
            pixel_offset = (y * width + x) * 3
            if color_type == 3:
                if bit_depth == 8:
                    palette_index = scanline[x]
                else:
                    packed_value = scanline[(x * bit_depth) // 8]
                    shift = 8 - bit_depth - ((x * bit_depth) % 8)
                    palette_index = (packed_value >> shift) & ((1 << bit_depth) - 1)
                if palette_index >= len(palette):
                    raise ValueError(f"{path} references missing palette index")
                red, green, blue = palette[palette_index]
            elif color_type == 2:
                source_offset = x * 3
                red = scanline[source_offset]
                green = scanline[source_offset + 1]
                blue = scanline[source_offset + 2]
            else:
                source_offset = x * 4
                red = scanline[source_offset]
                green = scanline[source_offset + 1]
                blue = scanline[source_offset + 2]

            pixels_bgr[pixel_offset] = blue
            pixels_bgr[pixel_offset + 1] = green
            pixels_bgr[pixel_offset + 2] = red

        previous_scanline = scanline

    return width, height, bytes(pixels_bgr)


def read_png_8bit_rgba(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if data[:len(PNG_SIGNATURE)] != PNG_SIGNATURE:
        raise ValueError(f"{path} is not a PNG file")

    offset = len(PNG_SIGNATURE)
    width = 0
    height = 0
    bit_depth = 0
    color_type = 0
    compression_method = 0
    filter_method = 0
    interlace_method = 0
    compressed = bytearray()

    while offset + 12 <= len(data):
        chunk_length = struct.unpack_from(">I", data, offset)[0]
        chunk_type = data[offset + 4:offset + 8]
        chunk_start = offset + 8
        chunk_end = chunk_start + chunk_length
        if chunk_end + 4 > len(data):
            raise ValueError(f"{path} has a truncated PNG chunk")
        chunk_data = data[chunk_start:chunk_end]
        offset = chunk_end + 4

        if chunk_type == b"IHDR":
            (
                width,
                height,
                bit_depth,
                color_type,
                compression_method,
                filter_method,
                interlace_method,
            ) = struct.unpack(">IIBBBBB", chunk_data)
        elif chunk_type == b"IDAT":
            compressed.extend(chunk_data)
        elif chunk_type == b"IEND":
            break

    if width <= 0 or height <= 0:
        raise ValueError(f"{path} is missing a PNG IHDR chunk")
    if compression_method != 0 or filter_method != 0 or interlace_method != 0:
        raise ValueError(f"{path} uses unsupported PNG compression/filter/interlace")
    if bit_depth != 8:
        raise ValueError(f"{path} must be an 8-bit PNG")
    if color_type not in (2, 6):
        raise ValueError(f"{path} must be truecolor or truecolor alpha PNG")

    bytes_per_pixel = 4 if color_type == 6 else 3
    scanline_length = width * bytes_per_pixel
    raw = zlib.decompress(bytes(compressed))
    expected_length = height * (1 + scanline_length)
    if len(raw) != expected_length:
        raise ValueError(
            f"{path} decoded to {len(raw)} bytes, expected {expected_length}"
        )

    pixels_rgba = bytearray(width * height * 4)
    previous_scanline = b""
    raw_offset = 0
    for y in range(height):
        filter_type = raw[raw_offset]
        raw_offset += 1
        filtered_scanline = raw[raw_offset:raw_offset + scanline_length]
        raw_offset += scanline_length
        scanline = unfilter_png_scanline(
            filter_type,
            filtered_scanline,
            previous_scanline,
            bytes_per_pixel,
        )

        for x in range(width):
            source_offset = x * bytes_per_pixel
            destination_offset = (y * width + x) * 4
            pixels_rgba[destination_offset] = scanline[source_offset]
            pixels_rgba[destination_offset + 1] = scanline[source_offset + 1]
            pixels_rgba[destination_offset + 2] = scanline[source_offset + 2]
            pixels_rgba[destination_offset + 3] = (
                scanline[source_offset + 3]
                if color_type == 6 else
                0xFF
            )

        previous_scanline = scanline

    return width, height, bytes(pixels_rgba)


def write_bmp_24(path: Path, width: int, height: int, pixels_bgr: bytes) -> None:
    expected_size = width * height * 3
    if len(pixels_bgr) != expected_size:
        raise ValueError(
            f"{path} expected {expected_size} BGR bytes, got {len(pixels_bgr)}"
        )

    row_stride = ((width * 3 + 3) // 4) * 4
    image_size = row_stride * height
    file_size = 14 + 40 + image_size
    padding = b"\0" * (row_stride - width * 3)

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as output:
        output.write(struct.pack("<2sIHHI", b"BM", file_size, 0, 0, 54))
        output.write(
            struct.pack(
                "<IiiHHIIiiII",
                40,
                width,
                -height,
                1,
                24,
                0,
                image_size,
                0,
                0,
                0,
                0,
            )
        )
        for y in range(height):
            row_start = y * width * 3
            output.write(pixels_bgr[row_start:row_start + width * 3])
            output.write(padding)


def bgr_to_3ds_framebuffer(
    pixels_bgr: bytes,
    screen_width: int,
    screen_height: int,
) -> bytes:
    """Return BGR888 bytes in libctru's default sideways framebuffer layout."""
    expected_size = screen_width * screen_height * 3
    if len(pixels_bgr) != expected_size:
        raise ValueError(
            f"expected {expected_size} screen BGR bytes, got {len(pixels_bgr)}"
        )

    framebuffer = bytearray(expected_size)
    for y in range(screen_height):
        for x in range(screen_width):
            source_offset = (y * screen_width + x) * 3
            framebuffer_offset = (x * screen_height + (screen_height - 1 - y)) * 3
            framebuffer[framebuffer_offset:framebuffer_offset + 3] = pixels_bgr[
                source_offset:source_offset + 3
            ]

    return bytes(framebuffer)


def bgr_from_3ds_framebuffer(
    framebuffer_bgr: bytes,
    screen_width: int,
    screen_height: int,
) -> bytes:
    """Convert libctru's default sideways framebuffer layout back to screen order."""
    expected_size = screen_width * screen_height * 3
    if len(framebuffer_bgr) != expected_size:
        raise ValueError(
            f"expected {expected_size} framebuffer BGR bytes, got {len(framebuffer_bgr)}"
        )

    pixels_bgr = bytearray(expected_size)
    for y in range(screen_height):
        for x in range(screen_width):
            framebuffer_offset = (x * screen_height + (screen_height - 1 - y)) * 3
            output_offset = (y * screen_width + x) * 3
            pixels_bgr[output_offset:output_offset + 3] = framebuffer_bgr[
                framebuffer_offset:framebuffer_offset + 3
            ]

    return bytes(pixels_bgr)


def write_png_copy(bmp_path: Path) -> None:
    subprocess.run(
        [
            "sips",
            "-s",
            "format",
            "png",
            str(bmp_path),
            "--out",
            str(bmp_path.with_suffix(".png")),
        ],
        check=True,
        stdout=subprocess.DEVNULL,
    )


def darken_bgr(pixels_bgr: bytes, permille: int = DARKEN_PERMILLE) -> bytes:
    permille = max(0, min(1000, permille))
    return bytes((value * permille) // 1000 for value in pixels_bgr)


def scale_bgr_nearest(
    pixels_bgr: bytes,
    source_width: int,
    source_height: int,
    output_width: int,
    output_height: int,
) -> bytes:
    output = bytearray(output_width * output_height * 3)

    for y in range(output_height):
        source_y = (y * source_height) // output_height
        for x in range(output_width):
            source_x = (x * source_width) // output_width
            source_offset = (source_y * source_width + source_x) * 3
            output_offset = (y * output_width + x) * 3
            output[output_offset:output_offset + 3] = pixels_bgr[
                source_offset:source_offset + 3
            ]

    return bytes(output)


def scale_rgba_nearest(
    pixels_rgba: bytes,
    source_width: int,
    source_height: int,
    target_width: int,
    target_height: int,
) -> bytes:
    output = bytearray(target_width * target_height * 4)
    for y in range(target_height):
        source_y = (y * source_height) // target_height
        for x in range(target_width):
            source_x = (x * source_width) // target_width
            source_offset = (source_y * source_width + source_x) * 4
            destination_offset = (y * target_width + x) * 4
            output[destination_offset:destination_offset + 4] = (
                pixels_rgba[source_offset:source_offset + 4]
            )
    return bytes(output)


def composite_rgba_over_bgr(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    source_rgba: bytes,
    source_width: int,
    source_height: int,
    destination_x: int,
    destination_y: int,
    opacity_permille: int = 1000,
) -> None:
    opacity_permille = max(0, min(1000, opacity_permille))
    if opacity_permille == 0:
        return

    for source_y in range(source_height):
        output_y = destination_y + source_y
        if output_y < 0 or output_y >= destination_height:
            continue
        for source_x in range(source_width):
            output_x = destination_x + source_x
            if output_x < 0 or output_x >= destination_width:
                continue

            source_offset = (source_y * source_width + source_x) * 4
            red = source_rgba[source_offset]
            green = source_rgba[source_offset + 1]
            blue = source_rgba[source_offset + 2]
            alpha = (source_rgba[source_offset + 3] * opacity_permille) // 1000
            if alpha == 0:
                continue

            destination_offset = (output_y * destination_width + output_x) * 3
            destination[destination_offset] = (
                blue * alpha +
                destination[destination_offset] * (255 - alpha)
            ) // 255
            destination[destination_offset + 1] = (
                green * alpha +
                destination[destination_offset + 1] * (255 - alpha)
            ) // 255
            destination[destination_offset + 2] = (
                red * alpha +
                destination[destination_offset + 2] * (255 - alpha)
            ) // 255


def color_key_from_top_left(pixels_bgr: bytes) -> bytes:
    if len(pixels_bgr) < 3:
        raise ValueError("source image has no top-left pixel")

    return pixels_bgr[:3]


def color_distance_squared(pixel: bytes, color_key: bytes) -> int:
    return sum((pixel[index] - color_key[index]) ** 2 for index in range(3))


def pixel_matches_key(pixel: bytes, color_key: bytes, tolerance: int = 0) -> bool:
    return color_distance_squared(pixel, color_key) <= tolerance * tolerance


def blit_scaled_bgr_colorkey(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    source_pixels: bytes,
    source_width: int,
    source_height: int,
    target_x: int,
    target_y: int,
    target_width: int,
    target_height: int,
    color_key: bytes,
    tolerance: int = 0,
    opacity_permille: int = 1000,
) -> None:
    opacity_permille = max(0, min(1000, opacity_permille))
    for y in range(target_height):
        destination_y = target_y + y
        if destination_y < 0 or destination_y >= destination_height:
            continue

        source_y = (y * source_height) // target_height
        for x in range(target_width):
            destination_x = target_x + x
            if destination_x < 0 or destination_x >= destination_width:
                continue

            source_x = (x * source_width) // target_width
            source_offset = (source_y * source_width + source_x) * 3
            pixel = source_pixels[source_offset:source_offset + 3]
            if pixel_matches_key(pixel, color_key, tolerance):
                continue

            destination_offset = (destination_y * destination_width + destination_x) * 3
            if opacity_permille == 1000:
                destination[destination_offset:destination_offset + 3] = pixel
            else:
                for channel in range(3):
                    old_value = destination[destination_offset + channel]
                    new_value = pixel[channel]
                    destination[destination_offset + channel] = (
                        old_value * (1000 - opacity_permille) +
                        new_value * opacity_permille
                    ) // 1000


def draw_pixel_text(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    x: int,
    y: int,
    text: str,
    color_bgr: bytes,
    shadow_bgr: bytes,
    scale: int = 2,
) -> None:
    cursor_x = x

    for character in text.upper():
        if character == " ":
            cursor_x += 3 * scale
            continue

        glyph = UI_PIXEL_FONT.get(character)
        if glyph is None:
            cursor_x += 4 * scale
            continue

        glyph_width = max(len(row) for row in glyph)
        for row_index, row in enumerate(glyph):
            for column_index, value in enumerate(row):
                if value != "1":
                    continue

                for offset_x, offset_y, pixel_color in (
                    (1, 1, shadow_bgr),
                    (0, 0, color_bgr),
                ):
                    for scaled_y in range(scale):
                        destination_y = (
                            y + row_index * scale + scaled_y + offset_y
                        )
                        if destination_y < 0 or destination_y >= destination_height:
                            continue
                        for scaled_x in range(scale):
                            destination_x = (
                                cursor_x
                                + column_index * scale
                                + scaled_x
                                + offset_x
                            )
                            if (
                                destination_x < 0
                                or destination_x >= destination_width
                            ):
                                continue
                            destination_offset = (
                                destination_y * destination_width + destination_x
                            ) * 3
                            destination[
                                destination_offset:destination_offset + 3
                            ] = pixel_color

        cursor_x += (glyph_width + 1) * scale


def blend_rect_bgr(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    x: int,
    y: int,
    width: int,
    height: int,
    color_bgr: bytes,
    opacity_permille: int,
) -> None:
    opacity_permille = max(0, min(1000, opacity_permille))
    if opacity_permille == 0:
        return

    left = max(0, x)
    top = max(0, y)
    right = min(destination_width, x + width)
    bottom = min(destination_height, y + height)
    if left >= right or top >= bottom:
        return

    for destination_y in range(top, bottom):
        row_offset = destination_y * destination_width * 3
        for destination_x in range(left, right):
            destination_offset = row_offset + destination_x * 3
            for channel in range(3):
                destination[destination_offset + channel] = (
                    destination[destination_offset + channel]
                    * (1000 - opacity_permille)
                    + color_bgr[channel] * opacity_permille
                ) // 1000


def fill_rect_bgr(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    x: int,
    y: int,
    width: int,
    height: int,
    color_bgr: bytes,
) -> None:
    left = max(0, x)
    top = max(0, y)
    right = min(destination_width, x + width)
    bottom = min(destination_height, y + height)
    if left >= right or top >= bottom:
        return

    row_pixels = color_bgr * (right - left)
    for destination_y in range(top, bottom):
        destination_offset = (destination_y * destination_width + left) * 3
        destination[destination_offset:destination_offset + len(row_pixels)] = (
            row_pixels
        )


def sample_bgr_clamped(
    pixels_bgr: bytes,
    width: int,
    height: int,
    x: int,
    y: int,
) -> bytes:
    source_x = max(0, min(width - 1, x))
    source_y = max(0, min(height - 1, y))
    offset = (source_y * width + source_x) * 3
    return pixels_bgr[offset:offset + 3]


class Fe8FontMap(NamedTuple):
    width: int
    height: int
    pixels_bgr: bytes
    vertical_lines: tuple[int, ...]
    horizontal_lines: tuple[int, ...]


class Fe8Glyph(NamedTuple):
    mask: bytes
    left: int
    right: int


_FE8_FONT_MAP_CACHE: dict[Path, Fe8FontMap] = {}


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def fe8_font_map_candidates(fe_repo: Optional[Path] = None) -> tuple[Path, ...]:
    candidates: list[Path] = []
    env_path = os.environ.get("FE8_FONT_MAP_PATH")
    if env_path:
        candidates.append(Path(env_path).expanduser())

    candidates.append(repo_root() / FE8_FONT_MAP_RELATIVE_PATH)
    candidates.append(Path.cwd() / FE8_FONT_MAP_RELATIVE_PATH)

    if fe_repo is not None:
        candidates.append(fe_repo / FE8_FONT_MAP_RELATIVE_PATH)

    deduped: list[Path] = []
    seen: set[Path] = set()
    for candidate in candidates:
        resolved = candidate.expanduser()
        if resolved in seen:
            continue
        seen.add(resolved)
        deduped.append(resolved)

    return tuple(deduped)


def find_fe8_font_map_path(fe_repo: Optional[Path] = None) -> Optional[Path]:
    for candidate in fe8_font_map_candidates(fe_repo):
        if candidate.is_file():
            return candidate
    return None


def fe8_pixel_matches(pixel: bytes, color_bgr: bytes, tolerance: int = 0) -> bool:
    return (
        abs(pixel[0] - color_bgr[0]) <= tolerance
        and abs(pixel[1] - color_bgr[1]) <= tolerance
        and abs(pixel[2] - color_bgr[2]) <= tolerance
    )


def fe8_font_map_pixel(
    pixels_bgr: bytes,
    width: int,
    x: int,
    y: int,
) -> bytes:
    offset = (y * width + x) * 3
    return pixels_bgr[offset:offset + 3]


def fe8_font_grid_line_positions(
    pixels_bgr: bytes,
    width: int,
    height: int,
    *,
    vertical: bool,
) -> tuple[int, ...]:
    span = height if vertical else width
    limit = width if vertical else height
    threshold = max(8, span // 4)
    positions: list[int] = []

    for position in range(limit):
        matches = 0
        for offset in range(span):
            x = position if vertical else offset
            y = offset if vertical else position
            pixel = fe8_font_map_pixel(pixels_bgr, width, x, y)
            if fe8_pixel_matches(pixel, FE8_FONT_GRID_BGR, tolerance=2):
                matches += 1

        if matches >= threshold:
            positions.append(position)

    if not positions:
        return ()

    grouped: list[int] = []
    group_start = positions[0]
    previous = positions[0]
    for position in positions[1:]:
        if position == previous + 1:
            previous = position
            continue
        grouped.append((group_start + previous) // 2)
        group_start = position
        previous = position
    grouped.append((group_start + previous) // 2)

    return tuple(grouped)


def fallback_fe8_font_grid(width: int, height: int) -> tuple[tuple[int, ...], tuple[int, ...]]:
    left_header = 39
    top_header = 38
    vertical_lines = tuple(
        left_header + round(index * (width - left_header) / FE8_FONT_COLUMNS)
        for index in range(FE8_FONT_COLUMNS)
    )
    horizontal_lines = tuple(
        top_header + round(index * (height - top_header) / FE8_FONT_ROWS)
        for index in range(FE8_FONT_ROWS)
    )
    return vertical_lines, horizontal_lines


def load_fe8_font_map(fe_repo: Optional[Path] = None) -> Optional[Fe8FontMap]:
    path = find_fe8_font_map_path(fe_repo)
    if path is None:
        return None

    cached = _FE8_FONT_MAP_CACHE.get(path)
    if cached is not None:
        return cached

    width, height, pixels_bgr = read_png_8bit_bgr(path)
    vertical_lines = fe8_font_grid_line_positions(
        pixels_bgr,
        width,
        height,
        vertical=True,
    )
    horizontal_lines = fe8_font_grid_line_positions(
        pixels_bgr,
        width,
        height,
        vertical=False,
    )

    if (
        len(vertical_lines) < FE8_FONT_COLUMNS
        or len(horizontal_lines) < FE8_FONT_ROWS
    ):
        vertical_lines, horizontal_lines = fallback_fe8_font_grid(width, height)

    font_map = Fe8FontMap(
        width,
        height,
        pixels_bgr,
        vertical_lines[:FE8_FONT_COLUMNS],
        horizontal_lines[:FE8_FONT_ROWS],
    )
    _FE8_FONT_MAP_CACHE[path] = font_map
    return font_map


def fe8_font_character_cell(
    font_map: Fe8FontMap,
    character: str,
) -> Optional[tuple[int, int, int, int]]:
    if len(character) != 1:
        return None

    value = ord(character)
    row = value >> 4
    column = value & 0xF
    row_index = row - FE8_FONT_FIRST_ROW
    if row_index < 0 or row_index >= FE8_FONT_ROWS:
        return None
    if column >= len(font_map.vertical_lines):
        return None
    if row_index >= len(font_map.horizontal_lines):
        return None

    x0 = font_map.vertical_lines[column] + 1
    x1 = (
        font_map.vertical_lines[column + 1]
        if column + 1 < len(font_map.vertical_lines)
        else font_map.width
    )
    y0 = font_map.horizontal_lines[row_index] + 1
    y1 = (
        font_map.horizontal_lines[row_index + 1]
        if row_index + 1 < len(font_map.horizontal_lines)
        else font_map.height
    )
    if x1 <= x0 or y1 <= y0:
        return None
    return x0, y0, x1, y1


def fe8_font_pixel_ink_level(pixel: bytes) -> int:
    if fe8_pixel_matches(pixel, FE8_FONT_GRID_BGR, tolerance=3):
        return 0
    if fe8_pixel_matches(pixel, FE8_FONT_HEADER_BGR, tolerance=3):
        return 0

    brightness = (pixel[0] + pixel[1] + pixel[2]) // 3
    if brightness >= FE8_FONT_BACKGROUND_MIN:
        return 0
    if brightness <= 96:
        return 2
    return 1


def fe8_glyph_from_font_map(
    font_map: Fe8FontMap,
    character: str,
) -> Optional[Fe8Glyph]:
    if character == " ":
        return Fe8Glyph(bytes(FE8_FONT_SOURCE_GLYPH_WIDTH * FE8_FONT_SOURCE_GLYPH_HEIGHT), 0, 3)

    cell = fe8_font_character_cell(font_map, character)
    if cell is None:
        return None

    x0, y0, x1, y1 = cell
    cell_width = x1 - x0
    cell_height = y1 - y0
    mask = bytearray(FE8_FONT_SOURCE_GLYPH_WIDTH * FE8_FONT_SOURCE_GLYPH_HEIGHT)

    for target_y in range(FE8_FONT_SOURCE_GLYPH_HEIGHT):
        source_y0 = y0 + (target_y * cell_height) // FE8_FONT_SOURCE_GLYPH_HEIGHT
        source_y1 = y0 + ((target_y + 1) * cell_height + FE8_FONT_SOURCE_GLYPH_HEIGHT - 1) // FE8_FONT_SOURCE_GLYPH_HEIGHT
        source_y1 = max(source_y0 + 1, min(y1, source_y1))
        for target_x in range(FE8_FONT_SOURCE_GLYPH_WIDTH):
            source_x0 = x0 + (target_x * cell_width) // FE8_FONT_SOURCE_GLYPH_WIDTH
            source_x1 = x0 + ((target_x + 1) * cell_width + FE8_FONT_SOURCE_GLYPH_WIDTH - 1) // FE8_FONT_SOURCE_GLYPH_WIDTH
            source_x1 = max(source_x0 + 1, min(x1, source_x1))
            dark_count = 0
            shade_count = 0

            for source_y in range(source_y0, source_y1):
                for source_x in range(source_x0, source_x1):
                    pixel = fe8_font_map_pixel(
                        font_map.pixels_bgr,
                        font_map.width,
                        source_x,
                        source_y,
                    )
                    ink_level = fe8_font_pixel_ink_level(pixel)
                    if ink_level == 2:
                        dark_count += 1
                    elif ink_level == 1:
                        shade_count += 1

            if dark_count > 0:
                mask[target_y * FE8_FONT_SOURCE_GLYPH_WIDTH + target_x] = 2
            elif shade_count > 0:
                mask[target_y * FE8_FONT_SOURCE_GLYPH_WIDTH + target_x] = 1

    ink_x = [
        x
        for y in range(FE8_FONT_SOURCE_GLYPH_HEIGHT)
        for x in range(FE8_FONT_SOURCE_GLYPH_WIDTH)
        if mask[y * FE8_FONT_SOURCE_GLYPH_WIDTH + x] != 0
    ]
    if not ink_x:
        return Fe8Glyph(bytes(mask), 0, 3)

    return Fe8Glyph(bytes(mask), min(ink_x), max(ink_x))


def draw_fe8_dialogue_text(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    fe_repo: Optional[Path],
    x: int,
    y: int,
    text: str,
    color_bgr: bytes,
    shade_bgr: bytes = FE8_FONT_TEXT_SHADE_BGR,
    scale: int = 1,
) -> bool:
    font_map = load_fe8_font_map(fe_repo)
    if font_map is None:
        return False

    cursor_x = x
    for character in text:
        glyph = fe8_glyph_from_font_map(font_map, character)
        if glyph is None:
            cursor_x += 5 * scale
            continue

        for source_y in range(FE8_FONT_SOURCE_GLYPH_HEIGHT):
            for source_x in range(glyph.left, glyph.right + 1):
                ink_level = glyph.mask[
                    source_y * FE8_FONT_SOURCE_GLYPH_WIDTH + source_x
                ]
                if ink_level == 0:
                    continue

                pixel_color = color_bgr if ink_level == 2 else shade_bgr
                destination_x0 = cursor_x + (source_x - glyph.left) * scale
                destination_y0 = y + source_y * scale
                for scaled_y in range(scale):
                    destination_y = destination_y0 + scaled_y
                    if destination_y < 0 or destination_y >= destination_height:
                        continue
                    for scaled_x in range(scale):
                        destination_x = destination_x0 + scaled_x
                        if (
                            destination_x < 0
                            or destination_x >= destination_width
                        ):
                            continue
                        destination_offset = (
                            destination_y * destination_width + destination_x
                        ) * 3
                        destination[
                            destination_offset:destination_offset + 3
                        ] = pixel_color

        cursor_x += (glyph.right - glyph.left + 2) * scale

    return True


def glyph_mask_from_fe8_font_map(
    font_map: Fe8FontMap,
    character: str,
) -> Optional[bytes]:
    glyph = fe8_glyph_from_font_map(font_map, character)
    if glyph is None:
        return None

    output = bytearray(UI_FONT_ATLAS_GLYPH_WIDTH * UI_FONT_ATLAS_GLYPH_HEIGHT)
    for target_y in range(UI_FONT_ATLAS_GLYPH_HEIGHT):
        source_y0 = (target_y * FE8_FONT_SOURCE_GLYPH_HEIGHT) // UI_FONT_ATLAS_GLYPH_HEIGHT
        source_y1 = ((target_y + 1) * FE8_FONT_SOURCE_GLYPH_HEIGHT + UI_FONT_ATLAS_GLYPH_HEIGHT - 1) // UI_FONT_ATLAS_GLYPH_HEIGHT
        source_y1 = max(source_y0 + 1, min(FE8_FONT_SOURCE_GLYPH_HEIGHT, source_y1))
        for target_x in range(UI_FONT_ATLAS_GLYPH_WIDTH):
            source_x0 = (target_x * FE8_FONT_SOURCE_GLYPH_WIDTH) // UI_FONT_ATLAS_GLYPH_WIDTH
            source_x1 = ((target_x + 1) * FE8_FONT_SOURCE_GLYPH_WIDTH + UI_FONT_ATLAS_GLYPH_WIDTH - 1) // UI_FONT_ATLAS_GLYPH_WIDTH
            source_x1 = max(source_x0 + 1, min(FE8_FONT_SOURCE_GLYPH_WIDTH, source_x1))
            sample_count = 0
            ink_score = 0

            for source_y in range(source_y0, source_y1):
                for source_x in range(source_x0, source_x1):
                    sample_count += 1
                    ink_level = glyph.mask[
                        source_y * FE8_FONT_SOURCE_GLYPH_WIDTH + source_x
                    ]
                    if ink_level == 2:
                        ink_score += 2
                    elif ink_level == 1:
                        ink_score += 1

            if ink_score == 0:
                continue
            output[target_y * UI_FONT_ATLAS_GLYPH_WIDTH + target_x] = min(
                255,
                max(48, (ink_score * 128) // sample_count),
            )

    return bytes(output)


def bgr_to_rgb_hex(color_bgr: bytes) -> str:
    return f"{color_bgr[2]:02x}{color_bgr[1]:02x}{color_bgr[0]:02x}"


def terminal_font_path_candidates() -> tuple[Path, ...]:
    candidates: list[Path] = []
    env_path = os.environ.get("ANKI3DS_UI_FONT_PATH")
    if env_path:
        candidates.append(Path(env_path).expanduser())

    for candidate in TERMINAL_FONT_PATH_CANDIDATES:
        candidates.append(candidate.expanduser())

    deduped: list[Path] = []
    seen: set[Path] = set()
    for candidate in candidates:
        if candidate in seen:
            continue
        seen.add(candidate)
        deduped.append(candidate)

    return tuple(deduped)


def find_terminal_font_path() -> Optional[Path]:
    for candidate in terminal_font_path_candidates():
        if candidate.is_file():
            return candidate
    return None


def render_font_file_text_bgr(
    font_path: Path,
    text: str,
    font_size: int,
    foreground_bgr: bytes,
    background_rgb: str = UI_FONT_KEY_RGB,
) -> Optional[tuple[int, int, bytes]]:
    if not font_path.is_file():
        return None

    with tempfile.TemporaryDirectory() as temp_dir:
        output_path = Path(temp_dir) / "text.png"
        try:
            subprocess.run(
                [
                    "hb-view",
                    f"--font-size={font_size}",
                    "--margin=0",
                    f"--background={background_rgb}",
                    f"--foreground={bgr_to_rgb_hex(foreground_bgr)}",
                    "-o",
                    str(output_path),
                    str(font_path),
                    text,
                ],
                check=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            return read_png_8bit_bgr(output_path)
        except (FileNotFoundError, subprocess.CalledProcessError, ValueError):
            return None


def render_terminal_font_text_bgr(
    text: str,
    font_size: int,
    foreground_bgr: bytes,
) -> Optional[tuple[int, int, bytes]]:
    font_path = find_terminal_font_path()
    if font_path is None:
        return None
    return render_font_file_text_bgr(
        font_path,
        text,
        font_size,
        foreground_bgr,
        TERMINAL_FONT_KEY_RGB,
    )


def render_fe_font_text_bgr(
    fe_repo: Path,
    text: str,
    font_size: int,
    foreground_bgr: bytes,
) -> Optional[tuple[int, int, bytes]]:
    return render_font_file_text_bgr(
        fe_repo / UI_FONT_RELATIVE_PATH,
        text,
        font_size,
        foreground_bgr,
    )


def blit_text_image(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    rendered_text: tuple[int, int, bytes],
    x: int,
    y: int,
    color_key: bytes = UI_FONT_KEY_BGR,
    tolerance: int = 150,
) -> None:
    text_width, text_height, text_pixels = rendered_text
    blit_scaled_bgr_colorkey(
        destination,
        destination_width,
        destination_height,
        text_pixels,
        text_width,
        text_height,
        x,
        y,
        text_width,
        text_height,
        color_key,
        tolerance=tolerance,
    )


def draw_terminal_font_text(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    x: int,
    y: int,
    text: str,
    font_size: int,
    color_bgr: bytes,
    shadow_bgr: bytes,
) -> bool:
    rendered_text = render_terminal_font_text_bgr(text, font_size, color_bgr)
    if rendered_text is None:
        return False

    rendered_shadow = render_terminal_font_text_bgr(text, font_size, shadow_bgr)
    if rendered_shadow is not None:
        blit_text_image(
            destination,
            destination_width,
            destination_height,
            rendered_shadow,
            x + 1,
            y + 1,
            TERMINAL_FONT_KEY_BGR,
            tolerance=4,
        )
    blit_text_image(
        destination,
        destination_width,
        destination_height,
        rendered_text,
        x,
        y,
        TERMINAL_FONT_KEY_BGR,
        tolerance=4,
    )
    return True


def draw_fe_font_text(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    fe_repo: Optional[Path],
    x: int,
    y: int,
    text: str,
    font_size: int,
    color_bgr: bytes,
    shadow_bgr: bytes,
) -> bool:
    if fe_repo is None:
        return False

    rendered_text = render_fe_font_text_bgr(fe_repo, text, font_size, color_bgr)
    if rendered_text is None:
        return False

    rendered_shadow = render_fe_font_text_bgr(fe_repo, text, font_size, shadow_bgr)
    if rendered_shadow is not None:
        blit_text_image(
            destination,
            destination_width,
            destination_height,
            rendered_shadow,
            x + 2,
            y + 2,
        )
    blit_text_image(
        destination,
        destination_width,
        destination_height,
        rendered_text,
        x,
        y,
    )
    return True


def draw_layer_text(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    fe_repo: Optional[Path],
    x: int,
    y: int,
    text: str,
    font_size: int,
    color_bgr: bytes,
    shadow_bgr: bytes,
    fallback_scale: int = 2,
) -> None:
    if draw_terminal_font_text(
        destination,
        destination_width,
        destination_height,
        x,
        y,
        text,
        TERMINAL_FONT_SIZE,
        color_bgr,
        shadow_bgr,
    ):
        return

    if draw_fe8_dialogue_text(
        destination,
        destination_width,
        destination_height,
        fe_repo,
        x,
        y,
        text,
        color_bgr,
        FE8_FONT_TEXT_SHADE_BGR,
        scale=1,
    ):
        return

    if draw_fe_font_text(
        destination,
        destination_width,
        destination_height,
        fe_repo,
        x,
        y,
        text,
        font_size,
        color_bgr,
        shadow_bgr,
    ):
        return

    draw_pixel_text(
        destination,
        destination_width,
        destination_height,
        x,
        y,
        text,
        color_bgr,
        shadow_bgr,
        scale=fallback_scale,
    )


def font_atlas_glyph_offset(character: str) -> int:
    if len(character) != 1:
        raise ValueError("font atlas character must be one codepoint")

    value = ord(character)
    if (
        value < UI_FONT_ATLAS_FIRST_CHAR or
        value >= UI_FONT_ATLAS_FIRST_CHAR + UI_FONT_ATLAS_CHAR_COUNT
    ):
        raise ValueError("font atlas character is outside printable ASCII")

    return (
        (value - UI_FONT_ATLAS_FIRST_CHAR)
        * UI_FONT_ATLAS_GLYPH_WIDTH
        * UI_FONT_ATLAS_GLYPH_HEIGHT
    )


def glyph_mask_from_pixel_font(character: str) -> bytes:
    if character == " ":
        return bytes(UI_FONT_ATLAS_GLYPH_WIDTH * UI_FONT_ATLAS_GLYPH_HEIGHT)

    glyph = UI_PIXEL_FONT.get(character.upper(), UI_PIXEL_FALLBACK_GLYPH)
    glyph_width = max(len(row) for row in glyph)
    glyph_height = len(glyph)
    x_offset = max(0, (UI_FONT_ATLAS_GLYPH_WIDTH - glyph_width) // 2)
    y_offset = max(0, (UI_FONT_ATLAS_GLYPH_HEIGHT - glyph_height) // 2)
    mask = bytearray(UI_FONT_ATLAS_GLYPH_WIDTH * UI_FONT_ATLAS_GLYPH_HEIGHT)

    for source_y, row in enumerate(glyph):
        destination_y = y_offset + source_y
        if destination_y >= UI_FONT_ATLAS_GLYPH_HEIGHT:
            continue
        for source_x, value in enumerate(row):
            destination_x = x_offset + source_x
            if destination_x >= UI_FONT_ATLAS_GLYPH_WIDTH:
                continue
            if value == "1":
                mask[
                    destination_y * UI_FONT_ATLAS_GLYPH_WIDTH + destination_x
                ] = 0xFF

    return bytes(mask)


def rendered_font_pixel_is_ink(pixel: bytes) -> bool:
    return not pixel_matches_key(pixel, UI_FONT_KEY_BGR, tolerance=150)


def glyph_mask_from_rendered_text(
    rendered_text: tuple[int, int, bytes],
) -> Optional[bytes]:
    source_width, source_height, source_pixels = rendered_text
    ink_pixels: list[tuple[int, int]] = []

    if source_width <= 0 or source_height <= 0:
        return None

    for y in range(source_height):
        for x in range(source_width):
            offset = (y * source_width + x) * 3
            if rendered_font_pixel_is_ink(source_pixels[offset:offset + 3]):
                ink_pixels.append((x, y))

    if not ink_pixels:
        return None

    left = min(x for x, _ in ink_pixels)
    right = max(x for x, _ in ink_pixels) + 1
    top = min(y for _, y in ink_pixels)
    bottom = max(y for _, y in ink_pixels) + 1
    box_width = right - left
    box_height = bottom - top
    scale = min(
        UI_FONT_ATLAS_GLYPH_WIDTH / box_width,
        UI_FONT_ATLAS_GLYPH_HEIGHT / box_height,
    )
    target_width = max(1, min(UI_FONT_ATLAS_GLYPH_WIDTH, round(box_width * scale)))
    target_height = max(
        1,
        min(UI_FONT_ATLAS_GLYPH_HEIGHT, round(box_height * scale)),
    )
    x_offset = (UI_FONT_ATLAS_GLYPH_WIDTH - target_width) // 2
    y_offset = (UI_FONT_ATLAS_GLYPH_HEIGHT - target_height) // 2
    mask = bytearray(UI_FONT_ATLAS_GLYPH_WIDTH * UI_FONT_ATLAS_GLYPH_HEIGHT)

    for target_y in range(target_height):
        source_y0 = top + (target_y * box_height) // target_height
        source_y1 = top + ((target_y + 1) * box_height + target_height - 1) // target_height
        source_y1 = max(source_y0 + 1, min(bottom, source_y1))
        for target_x in range(target_width):
            source_x0 = left + (target_x * box_width) // target_width
            source_x1 = left + ((target_x + 1) * box_width + target_width - 1) // target_width
            source_x1 = max(source_x0 + 1, min(right, source_x1))
            sample_count = 0
            ink_count = 0

            for source_y in range(source_y0, source_y1):
                for source_x in range(source_x0, source_x1):
                    sample_count += 1
                    offset = (source_y * source_width + source_x) * 3
                    if rendered_font_pixel_is_ink(source_pixels[offset:offset + 3]):
                        ink_count += 1

            if ink_count == 0:
                continue
            mask[
                (y_offset + target_y) * UI_FONT_ATLAS_GLYPH_WIDTH
                + x_offset
                + target_x
            ] = max(32, min(255, (ink_count * 255) // sample_count))

    return bytes(mask)


def build_font_atlas(fe_repo: Path) -> bytes:
    atlas = bytearray(
        UI_FONT_ATLAS_CHAR_COUNT
        * UI_FONT_ATLAS_GLYPH_WIDTH
        * UI_FONT_ATLAS_GLYPH_HEIGHT
    )
    fe8_font_map = load_fe8_font_map(fe_repo)
    terminal_font_path = find_terminal_font_path()

    for value in range(
        UI_FONT_ATLAS_FIRST_CHAR,
        UI_FONT_ATLAS_FIRST_CHAR + UI_FONT_ATLAS_CHAR_COUNT,
    ):
        character = chr(value)
        glyph_mask: Optional[bytes] = None

        if terminal_font_path is not None and character != " ":
            rendered_text = render_font_file_text_bgr(
                terminal_font_path,
                character,
                UI_FONT_ATLAS_RENDER_SIZE,
                UI_DIALOG_TEXT_BGR,
            )
            if rendered_text is not None:
                glyph_mask = glyph_mask_from_rendered_text(rendered_text)

        if glyph_mask is None and fe8_font_map is not None:
            glyph_mask = glyph_mask_from_fe8_font_map(fe8_font_map, character)

        if glyph_mask is None and character != " ":
            rendered_text = render_fe_font_text_bgr(
                fe_repo,
                character,
                UI_FONT_ATLAS_RENDER_SIZE,
                UI_DIALOG_TEXT_BGR,
            )
            if rendered_text is not None:
                glyph_mask = glyph_mask_from_rendered_text(rendered_text)

        if glyph_mask is None:
            glyph_mask = glyph_mask_from_pixel_font(character)

        offset = font_atlas_glyph_offset(character)
        atlas[offset:offset + len(glyph_mask)] = glyph_mask

    return bytes(atlas)


def write_font_atlas(framebuffer_out: Path, fe_repo: Path) -> Path:
    framebuffer_out.mkdir(parents=True, exist_ok=True)
    output_path = framebuffer_out / UI_FONT_ATLAS_FILENAME
    output_path.write_bytes(build_font_atlas(fe_repo))
    return output_path


def average_non_key_color(
    pixels_bgr: bytes,
    color_key: bytes,
    tolerance: int = 0,
) -> bytes:
    totals = [0, 0, 0]
    count = 0

    for offset in range(0, len(pixels_bgr), 3):
        pixel = pixels_bgr[offset:offset + 3]
        if pixel_matches_key(pixel, color_key, tolerance):
            continue

        for channel in range(3):
            totals[channel] += pixel[channel]
        count += 1

    if count == 0:
        return bytes((0x40, 0x60, 0x78))

    return bytes(total // count for total in totals)


def legend_dialogue_rect(
    screen_width: int,
    screen_height: int,
) -> tuple[int, int, int, int]:
    if screen_width >= TOP_PREVIEW_WIDTH:
        width = min(350, screen_width - 50)
        height = min(176, max(28, screen_height - 26))
        y = max(18, min(34, (screen_height - height) // 2))
        return (screen_width - width) // 2, y, width, height

    width = min(292, screen_width - 24)
    height = min(174, max(28, screen_height - 38))
    y = max(18, min(28, (screen_height - height) // 2))
    return (screen_width - width) // 2, y, width, height


def set_pixel_bgr(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    x: int,
    y: int,
    color_bgr: bytes,
) -> None:
    if x < 0 or x >= destination_width or y < 0 or y >= destination_height:
        return

    destination_offset = (y * destination_width + x) * 3
    destination[destination_offset:destination_offset + 3] = color_bgr


def nearest_palette_bgr(pixel: bytes, palette: tuple[bytes, ...]) -> bytes:
    best_color = palette[0]
    best_distance: int | None = None

    for color in palette:
        distance = 0
        for channel in range(3):
            delta = pixel[channel] - color[channel]
            distance += delta * delta
        if best_distance is None or distance < best_distance:
            best_distance = distance
            best_color = color

    return best_color


def quantize_bgr_pixels(pixels_bgr: bytes, palette: tuple[bytes, ...]) -> bytes:
    output = bytearray(len(pixels_bgr))
    for offset in range(0, len(pixels_bgr), 3):
        output[offset:offset + 3] = nearest_palette_bgr(
            pixels_bgr[offset:offset + 3],
            palette,
        )
    return bytes(output)


def read_dialogue_reference_panel_bgr() -> tuple[int, int, bytes] | None:
    reference_path = (
        Path(__file__).resolve().parents[1] /
        UI_DIALOG_REFERENCE_RELATIVE_PATH
    )
    if not reference_path.is_file():
        return None

    source_width, source_height, source_pixels = read_png_8bit_bgr(reference_path)
    panel_x, panel_y, panel_width, panel_height = UI_DIALOG_REFERENCE_PANEL_RECT
    if (
        panel_x < 0 or
        panel_y < 0 or
        panel_x + panel_width > source_width or
        panel_y + panel_height > source_height
    ):
        return None

    panel_pixels = bytearray(panel_width * panel_height * 3)
    for row in range(panel_height):
        source_offset = ((panel_y + row) * source_width + panel_x) * 3
        destination_offset = row * panel_width * 3
        panel_pixels[destination_offset:destination_offset + panel_width * 3] = (
            source_pixels[source_offset:source_offset + panel_width * 3]
        )

    return (
        panel_width,
        panel_height,
        quantize_bgr_pixels(bytes(panel_pixels), UI_DIALOG_REFERENCE_PALETTE_BGR),
    )


def read_parchment_gui_panel_bgr() -> tuple[int, int, bytes] | None:
    source_path = (
        Path(__file__).resolve().parents[1] /
        UI_PARCHMENT_GUI_RELATIVE_PATH
    )
    if not source_path.is_file():
        return None

    source_width, source_height, source_pixels = read_png_8bit_bgr(source_path)
    panel_x, panel_y, panel_width, panel_height = UI_PARCHMENT_GUI_PANEL_RECT
    if (
        panel_x < 0 or
        panel_y < 0 or
        panel_x + panel_width > source_width or
        panel_y + panel_height > source_height
    ):
        return None

    panel_pixels = bytearray(panel_width * panel_height * 3)
    for row in range(panel_height):
        source_offset = ((panel_y + row) * source_width + panel_x) * 3
        destination_offset = row * panel_width * 3
        panel_pixels[destination_offset:destination_offset + panel_width * 3] = (
            source_pixels[source_offset:source_offset + panel_width * 3]
        )

    return panel_width, panel_height, bytes(panel_pixels)


def read_old_parchment_rgba() -> tuple[int, int, bytes] | None:
    source_path = (
        Path(__file__).resolve().parents[1] /
        UI_OLD_PARCHMENT_RELATIVE_PATH
    )
    if not source_path.is_file():
        return None

    return read_png_8bit_rgba(source_path)


def blit_bgr(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    source_pixels: bytes,
    source_width: int,
    source_height: int,
    source_x: int,
    source_y: int,
    width: int,
    height: int,
    destination_x: int,
    destination_y: int,
) -> None:
    if width <= 0 or height <= 0:
        return

    for local_y in range(height):
        output_y = destination_y + local_y
        input_y = source_y + local_y
        if (
            output_y < 0 or
            output_y >= destination_height or
            input_y < 0 or
            input_y >= source_height
        ):
            continue
        for local_x in range(width):
            output_x = destination_x + local_x
            input_x = source_x + local_x
            if (
                output_x < 0 or
                output_x >= destination_width or
                input_x < 0 or
                input_x >= source_width
            ):
                continue

            source_offset = (input_y * source_width + input_x) * 3
            destination_offset = (output_y * destination_width + output_x) * 3
            destination[destination_offset:destination_offset + 3] = (
                source_pixels[source_offset:source_offset + 3]
            )


def tile_bgr(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    source_pixels: bytes,
    source_width: int,
    source_height: int,
    source_x: int,
    source_y: int,
    tile_width: int,
    tile_height: int,
    destination_x: int,
    destination_y: int,
    width: int,
    height: int,
) -> None:
    if width <= 0 or height <= 0 or tile_width <= 0 or tile_height <= 0:
        return

    for local_y in range(height):
        output_y = destination_y + local_y
        if output_y < 0 or output_y >= destination_height:
            continue
        input_y = source_y + local_y % tile_height
        if input_y < 0 or input_y >= source_height:
            continue

        for local_x in range(width):
            output_x = destination_x + local_x
            if output_x < 0 or output_x >= destination_width:
                continue
            input_x = source_x + local_x % tile_width
            if input_x < 0 or input_x >= source_width:
                continue

            source_offset = (input_y * source_width + input_x) * 3
            destination_offset = (output_y * destination_width + output_x) * 3
            destination[destination_offset:destination_offset + 3] = (
                source_pixels[source_offset:source_offset + 3]
            )


def draw_panel_nine_slice_bgr(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    x: int,
    y: int,
    width: int,
    height: int,
    source_pixels: bytes,
    source_width: int,
    source_height: int,
    left: int,
    top: int,
    right: int,
    bottom: int,
) -> bool:
    if width < left + right or height < top + bottom:
        return False

    center_width = source_width - left - right
    center_height = source_height - top - bottom
    if center_width <= 0 or center_height <= 0:
        return False

    tile_bgr(
        destination,
        destination_width,
        destination_height,
        source_pixels,
        source_width,
        source_height,
        left,
        top,
        center_width,
        center_height,
        x + left,
        y + top,
        width - left - right,
        height - top - bottom,
    )
    tile_bgr(
        destination,
        destination_width,
        destination_height,
        source_pixels,
        source_width,
        source_height,
        left,
        0,
        center_width,
        top,
        x + left,
        y,
        width - left - right,
        top,
    )
    tile_bgr(
        destination,
        destination_width,
        destination_height,
        source_pixels,
        source_width,
        source_height,
        left,
        source_height - bottom,
        center_width,
        bottom,
        x + left,
        y + height - bottom,
        width - left - right,
        bottom,
    )
    tile_bgr(
        destination,
        destination_width,
        destination_height,
        source_pixels,
        source_width,
        source_height,
        0,
        top,
        left,
        center_height,
        x,
        y + top,
        left,
        height - top - bottom,
    )
    tile_bgr(
        destination,
        destination_width,
        destination_height,
        source_pixels,
        source_width,
        source_height,
        source_width - right,
        top,
        right,
        center_height,
        x + width - right,
        y + top,
        right,
        height - top - bottom,
    )

    blit_bgr(
        destination,
        destination_width,
        destination_height,
        source_pixels,
        source_width,
        source_height,
        0,
        0,
        left,
        top,
        x,
        y,
    )
    blit_bgr(
        destination,
        destination_width,
        destination_height,
        source_pixels,
        source_width,
        source_height,
        source_width - right,
        0,
        right,
        top,
        x + width - right,
        y,
    )
    blit_bgr(
        destination,
        destination_width,
        destination_height,
        source_pixels,
        source_width,
        source_height,
        0,
        source_height - bottom,
        left,
        bottom,
        x,
        y + height - bottom,
    )
    blit_bgr(
        destination,
        destination_width,
        destination_height,
        source_pixels,
        source_width,
        source_height,
        source_width - right,
        source_height - bottom,
        right,
        bottom,
        x + width - right,
        y + height - bottom,
    )
    return True


def draw_dialogue_panel_from_parchment_gui(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    x: int,
    y: int,
    width: int,
    height: int,
) -> bool:
    source = read_parchment_gui_panel_bgr()
    if source is None:
        return False

    source_width, source_height, source_pixels = source
    slice_size = UI_PARCHMENT_GUI_SLICE
    return draw_panel_nine_slice_bgr(
        destination,
        destination_width,
        destination_height,
        x,
        y,
        width,
        height,
        source_pixels,
        source_width,
        source_height,
        slice_size,
        slice_size,
        slice_size,
        slice_size,
    )


def draw_dialogue_panel_from_old_parchment(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    x: int,
    y: int,
    width: int,
    height: int,
) -> bool:
    source = read_old_parchment_rgba()
    if source is None:
        return False

    source_width, source_height, source_rgba = source
    scaled_rgba = scale_rgba_nearest(
        source_rgba,
        source_width,
        source_height,
        width,
        height,
    )
    composite_rgba_over_bgr(
        destination,
        destination_width,
        destination_height,
        scaled_rgba,
        width,
        height,
        x,
        y,
    )
    return True


def draw_dialogue_panel_from_reference(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    x: int,
    y: int,
    width: int,
    height: int,
) -> bool:
    reference = read_dialogue_reference_panel_bgr()
    if reference is None:
        return False

    source_width, source_height, source_pixels = reference
    left = UI_DIALOG_REFERENCE_SLICE_LEFT
    top = UI_DIALOG_REFERENCE_SLICE_TOP
    right = UI_DIALOG_REFERENCE_SLICE_RIGHT
    bottom = UI_DIALOG_REFERENCE_SLICE_BOTTOM
    return draw_panel_nine_slice_bgr(
        destination,
        destination_width,
        destination_height,
        x,
        y,
        width,
        height,
        source_pixels,
        source_width,
        source_height,
        left,
        top,
        right,
        bottom,
    )


def draw_dialogue_panel_texture(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    x: int,
    y: int,
    width: int,
    height: int,
) -> None:
    inner_x = x + 13
    inner_y = y + 13
    inner_width = width - 26
    inner_height = height - 26
    if inner_width <= 0 or inner_height <= 0:
        return

    for destination_y in range(inner_y, inner_y + inner_height):
        if destination_y < 0 or destination_y >= destination_height:
            continue
        row_offset = destination_y * destination_width * 3
        for destination_x in range(inner_x, inner_x + inner_width):
            if destination_x < 0 or destination_x >= destination_width:
                continue
            local_x = destination_x - inner_x
            local_y = destination_y - inner_y
            color_bgr = UI_DIALOG_FILL_BGR
            grain = (
                (local_x * 37) ^
                (local_y * 73) ^
                ((local_x + 11) * (local_y + 17))
            ) & 255
            if local_y <= 1 or local_x <= 1:
                color_bgr = UI_DIALOG_FILL_LIGHT_BGR
            elif local_y >= inner_height - 3 or local_x >= inner_width - 3:
                color_bgr = UI_DIALOG_FILL_SHADE_BGR
            elif grain == 0:
                color_bgr = UI_DIALOG_FILL_LIGHT_BGR
            elif grain == 255:
                color_bgr = UI_DIALOG_FILL_ALT_BGR
            destination_offset = row_offset + destination_x * 3
            destination[destination_offset:destination_offset + 3] = color_bgr


def fill_chamfered_rect_bgr(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    x: int,
    y: int,
    width: int,
    height: int,
    chamfer: int,
    color_bgr: bytes,
) -> None:
    if width <= 0 or height <= 0:
        return

    chamfer = max(0, min(chamfer, width // 2, height // 2))
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + chamfer,
        y,
        width - chamfer * 2,
        height,
        color_bgr,
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x,
        y + chamfer,
        width,
        height - chamfer * 2,
        color_bgr,
    )


def draw_dialogue_corner_cap(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    x: int,
    y: int,
) -> None:
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x,
        y + 4,
        12,
        8,
        UI_DIALOG_BORDER_DARK_BGR,
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 4,
        y,
        8,
        12,
        UI_DIALOG_BORDER_DARK_BGR,
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 2,
        y + 5,
        9,
        5,
        UI_DIALOG_BORDER_BGR,
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 5,
        y + 2,
        5,
        9,
        UI_DIALOG_BORDER_BGR,
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 5,
        y + 5,
        4,
        4,
        UI_DIALOG_BORDER_LIGHT_BGR,
    )
    set_pixel_bgr(
        destination,
        destination_width,
        destination_height,
        x + 7,
        y + 7,
        UI_DIALOG_FILL_LIGHT_BGR,
    )


def draw_dialogue_scroll_edge_detail(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    x: int,
    y: int,
    width: int,
    height: int,
    accent_bgr: bytes,
) -> None:
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 7,
        y,
        width - 14,
        1,
        UI_DIALOG_BORDER_DARK_BGR,
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 7,
        y + height - 1,
        width - 14,
        1,
        UI_DIALOG_OUTLINE_BGR,
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 15,
        y + 3,
        width - 30,
        2,
        accent_bgr,
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 15,
        y + height - 5,
        width - 30,
        2,
        UI_DIALOG_OUTLINE_BGR,
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 14,
        y + 7,
        width - 28,
        1,
        UI_DIALOG_BORDER_LIGHT_BGR,
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 14,
        y + height - 8,
        width - 28,
        1,
        UI_DIALOG_BORDER_DARK_BGR,
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 4,
        y + 15,
        2,
        height - 30,
        UI_DIALOG_BORDER_LIGHT_BGR,
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + width - 6,
        y + 15,
        2,
        height - 30,
        UI_DIALOG_OUTLINE_BGR,
    )

    for local_x in range(18, max(18, width - 18), 9):
        if ((local_x * 5) & 15) < 8:
            set_pixel_bgr(
                destination,
                destination_width,
                destination_height,
                x + local_x,
                y + 6,
                UI_DIALOG_BORDER_MID_BGR,
            )
        else:
            set_pixel_bgr(
                destination,
                destination_width,
                destination_height,
                x + local_x,
                y + height - 7,
                UI_DIALOG_BORDER_LIGHT_BGR,
            )

    mid_y = y + height // 2
    for side_x in (x + 2, x + width - 8):
        fill_rect_bgr(
            destination,
            destination_width,
            destination_height,
            side_x,
            mid_y - 11,
            6,
            22,
            UI_DIALOG_BORDER_DARK_BGR,
        )
        fill_rect_bgr(
            destination,
            destination_width,
            destination_height,
            side_x + 1,
            mid_y - 9,
            4,
            18,
            UI_DIALOG_BORDER_BGR,
        )
        fill_rect_bgr(
            destination,
            destination_width,
            destination_height,
            side_x + 2,
            mid_y - 6,
            2,
            12,
            UI_DIALOG_BORDER_LIGHT_BGR,
        )


def draw_dialogue_panel(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    x: int,
    y: int,
    width: int,
    height: int,
    trim_bgr: bytes,
) -> None:
    if width < 40 or height < 24:
        return
    if draw_dialogue_panel_from_old_parchment(
        destination,
        destination_width,
        destination_height,
        x,
        y,
        width,
        height,
    ):
        return
    if draw_dialogue_panel_from_parchment_gui(
        destination,
        destination_width,
        destination_height,
        x,
        y,
        width,
        height,
    ):
        return
    if draw_dialogue_panel_from_reference(
        destination,
        destination_width,
        destination_height,
        x,
        y,
        width,
        height,
    ):
        return

    accent_bgr = trim_bgr if len(trim_bgr) == 3 else UI_DIALOG_BORDER_MID_BGR

    fill_chamfered_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 4,
        y + 4,
        width - 1,
        height - 1,
        9,
        UI_DIALOG_SHADOW_BGR,
    )
    fill_chamfered_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x,
        y,
        width,
        height,
        9,
        UI_DIALOG_OUTLINE_BGR,
    )
    fill_chamfered_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 1,
        y + 1,
        width - 2,
        height - 2,
        8,
        UI_DIALOG_BORDER_DARK_BGR,
    )
    fill_chamfered_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 3,
        y + 3,
        width - 6,
        height - 6,
        6,
        UI_DIALOG_BORDER_BGR,
    )
    fill_chamfered_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 5,
        y + 5,
        width - 10,
        height - 10,
        5,
        UI_DIALOG_BORDER_LIGHT_BGR,
    )
    fill_chamfered_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 8,
        y + 8,
        width - 16,
        height - 16,
        4,
        UI_DIALOG_BORDER_DARK_BGR,
    )
    fill_chamfered_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 11,
        y + 11,
        width - 22,
        height - 22,
        2,
        UI_DIALOG_FILL_BGR,
    )
    draw_dialogue_panel_texture(
        destination,
        destination_width,
        destination_height,
        x,
        y,
        width,
        height,
    )

    draw_dialogue_scroll_edge_detail(
        destination,
        destination_width,
        destination_height,
        x,
        y,
        width,
        height,
        accent_bgr,
    )

    for corner_x in (x + 2, x + width - 14):
        for corner_y in (y + 2, y + height - 14):
            draw_dialogue_corner_cap(
                destination,
                destination_width,
                destination_height,
                corner_x,
                corner_y,
            )


def ui_trim_color(fe_repo: Path) -> bytes:
    frame_path = fe_repo / UI_FRAME_RELATIVE_PATH
    if not frame_path.is_file():
        raise FileNotFoundError(frame_path)

    _, _, frame_pixels = read_png_8bit_bgr(frame_path)
    return average_non_key_color(
        frame_pixels,
        color_key_from_top_left(frame_pixels),
        tolerance=2,
    )


def ui_card_palette(fe_repo: Path) -> dict[str, bytes]:
    frame_path = fe_repo / UI_CARD_FRAME_RELATIVE_PATH
    if not frame_path.is_file():
        raise FileNotFoundError(frame_path)

    return {
        "shadow": UI_CARD_SHADOW_BGR,
        "dark": UI_CARD_DARK_BGR,
        "fill": UI_CARD_FILL_BGR,
        "fill_alt": UI_CARD_FILL_ALT_BGR,
        "highlight": UI_CARD_HIGHLIGHT_BGR,
        "trim": UI_CARD_TRIM_BGR,
        "trim_bright": UI_CARD_TRIM_BRIGHT_BGR,
        "trim_shadow": UI_CARD_TRIM_SHADOW_BGR,
        "trim_gold": UI_CARD_TRIM_GOLD_BGR,
        "trim_dark": UI_CARD_TRIM_DARK_BGR,
    }


def draw_card_texture(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    x: int,
    y: int,
    width: int,
    height: int,
    fill_bgr: bytes,
    fill_alt_bgr: bytes,
    dark_bgr: bytes,
) -> None:
    left = max(0, x)
    top = max(0, y)
    right = min(destination_width, x + width)
    bottom = min(destination_height, y + height)
    if left >= right or top >= bottom:
        return

    for destination_y in range(top, bottom):
        row_offset = destination_y * destination_width * 3
        for destination_x in range(left, right):
            local_x = destination_x - x
            local_y = destination_y - y
            if ((local_x // 8) + (local_y // 8)) % 2 == 0:
                color_bgr = fill_alt_bgr
            else:
                color_bgr = fill_bgr
            destination_offset = row_offset + destination_x * 3
            destination[destination_offset:destination_offset + 3] = color_bgr


def draw_fe_flashcard_panel(
    destination: bytearray,
    destination_width: int,
    destination_height: int,
    x: int,
    y: int,
    width: int,
    height: int,
    palette: dict[str, bytes],
) -> None:
    if width < 32 or height < 32:
        return

    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 3,
        y + 3,
        width - 3,
        height - 3,
        palette["shadow"],
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x,
        y,
        width,
        height,
        palette["trim_dark"],
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 1,
        y + 1,
        width - 2,
        height - 2,
        palette["trim_shadow"],
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 3,
        y + 3,
        width - 6,
        height - 6,
        palette["trim_bright"],
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 5,
        y + 5,
        width - 10,
        height - 10,
        palette["trim"],
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 7,
        y + 7,
        width - 14,
        height - 14,
        palette["trim_gold"],
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 9,
        y + 9,
        width - 18,
        height - 18,
        palette["dark"],
    )
    draw_card_texture(
        destination,
        destination_width,
        destination_height,
        x + 11,
        y + 11,
        width - 22,
        height - 22,
        palette["fill"],
        palette["fill_alt"],
        palette["shadow"],
    )

    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 12,
        y + 12,
        width - 24,
        2,
        palette["highlight"],
    )
    fill_rect_bgr(
        destination,
        destination_width,
        destination_height,
        x + 12,
        y + height - 14,
        width - 24,
        2,
        palette["dark"],
    )
    for corner_x in (x + 3, x + width - 9):
        for corner_y in (y + 3, y + height - 9):
            fill_rect_bgr(
                destination,
                destination_width,
                destination_height,
                corner_x,
                corner_y,
                6,
                6,
                palette["trim_bright"],
            )
            fill_rect_bgr(
                destination,
                destination_width,
                destination_height,
                corner_x + 2,
                corner_y + 2,
                2,
                2,
                palette["trim_gold"],
            )


def draw_bottom_icon_strip(
    output: bytearray,
    screen_width: int,
    screen_height: int,
    fe_repo: Path,
) -> None:
    icon_size = 20
    icon_gap = 5
    total_width = len(UI_ICON_RELATIVE_PATHS) * icon_size + (
        len(UI_ICON_RELATIVE_PATHS) - 1
    ) * icon_gap
    icon_x = max(8, screen_width - total_width - 20)
    icon_y = screen_height - 42
    for icon_index, relative_path in enumerate(UI_ICON_RELATIVE_PATHS):
        icon_path = fe_repo / relative_path
        if not icon_path.is_file():
            raise FileNotFoundError(icon_path)

        icon_width, icon_height, icon_pixels = read_png_8bit_bgr(icon_path)
        blit_scaled_bgr_colorkey(
            output,
            screen_width,
            screen_height,
            icon_pixels,
            icon_width,
            icon_height,
            icon_x + icon_index * (icon_size + icon_gap),
            icon_y,
            icon_size,
            icon_size,
            color_key_from_top_left(icon_pixels),
            tolerance=6,
        )


def apply_card_layer(
    pixels_bgr: bytes,
    screen_width: int,
    screen_height: int,
    fe_repo: Path,
) -> bytes:
    _ = (screen_width, screen_height, fe_repo)
    return bytes(pixels_bgr)


def apply_legend_layer(
    pixels_bgr: bytes,
    screen_width: int,
    screen_height: int,
    fe_repo: Path,
) -> bytes:
    output = bytearray(pixels_bgr)
    trim_bgr = ui_trim_color(fe_repo)
    panel_x, panel_y, panel_width, panel_height = legend_dialogue_rect(
        screen_width,
        screen_height,
    )

    draw_dialogue_panel(
        output,
        screen_width,
        screen_height,
        panel_x,
        panel_y,
        panel_width,
        panel_height,
        trim_bgr,
    )
    return bytes(output)


def draw_dialogue_sample_text(
    output: bytearray,
    screen_width: int,
    screen_height: int,
    fe_repo: Optional[Path],
) -> None:
    panel_x, panel_y, _, _ = legend_dialogue_rect(screen_width, screen_height)
    if screen_width >= TOP_PREVIEW_WIDTH:
        lines = ("Be warned: if Anki is", "ignored, your cards", "will return tomorrow.")
        font_size = 18
        line_height = 19
        text_x = panel_x + 24
        text_y = panel_y + 20
    else:
        lines = ("Press A to answer.",)
        font_size = 18
        line_height = 19
        text_x = panel_x + 20
        text_y = panel_y + 18

    for line_index, line in enumerate(lines):
        draw_layer_text(
            output,
            screen_width,
            screen_height,
            fe_repo,
            text_x,
            text_y + line_index * line_height,
            line,
            font_size,
            UI_DIALOG_TEXT_BGR,
            UI_DIALOG_TEXT_SHADOW_BGR,
            fallback_scale=2,
        )


def apply_font_layer(
    pixels_bgr: bytes,
    screen_width: int,
    screen_height: int,
    fe_repo: Optional[Path] = None,
) -> bytes:
    output = bytearray(pixels_bgr)
    draw_dialogue_sample_text(output, screen_width, screen_height, fe_repo)
    return bytes(output)


def compose_app_ui_layers(
    pixels_bgr: bytes,
    screen_width: int,
    screen_height: int,
    fe_repo: Path,
) -> tuple[bytes, bytes, bytes, bytes]:
    background_layer = bytes(pixels_bgr)
    legend_layer = apply_legend_layer(
        background_layer,
        screen_width,
        screen_height,
        fe_repo,
    )
    card_layer = apply_card_layer(
        legend_layer,
        screen_width,
        screen_height,
        fe_repo,
    )
    font_layer = apply_font_layer(
        card_layer,
        screen_width,
        screen_height,
        fe_repo,
    )
    return background_layer, legend_layer, card_layer, font_layer


def apply_app_ui_chrome(
    pixels_bgr: bytes,
    screen_width: int,
    screen_height: int,
    fe_repo: Path,
) -> bytes:
    _, _, _, final_layer = compose_app_ui_layers(
        pixels_bgr,
        screen_width,
        screen_height,
        fe_repo,
    )
    return final_layer


def crop_top_left_bgr(
    pixels_bgr: bytes,
    source_width: int,
    crop_width: int,
    crop_height: int,
) -> bytes:
    output = bytearray(crop_width * crop_height * 3)

    for y in range(crop_height):
        source_offset = y * source_width * 3
        output_offset = y * crop_width * 3
        output[output_offset:output_offset + crop_width * 3] = pixels_bgr[
            source_offset:source_offset + crop_width * 3
        ]

    return bytes(output)


def make_grid(
    images: list[bytes],
    image_width: int,
    image_height: int,
    columns: int,
    gap: int,
) -> tuple[int, int, bytes]:
    rows = (len(images) + columns - 1) // columns
    grid_width = columns * image_width + (columns - 1) * gap
    grid_height = rows * image_height + (rows - 1) * gap
    grid = bytearray(grid_width * grid_height * 3)

    for image_index, image in enumerate(images):
        column = image_index % columns
        row = image_index // columns
        target_x = column * (image_width + gap)
        target_y = row * (image_height + gap)
        for y in range(image_height):
            source_offset = y * image_width * 3
            target_offset = ((target_y + y) * grid_width + target_x) * 3
            grid[target_offset:target_offset + image_width * 3] = image[
                source_offset:source_offset + image_width * 3
            ]

    return grid_width, grid_height, bytes(grid)


def write_preview_sheet(
    preview_out: Path,
    sheet_name: str,
    images: list[bytes],
    image_width: int,
    image_height: int,
    columns: int,
) -> None:
    sheet_width, sheet_height, sheet_pixels = make_grid(
        images,
        image_width,
        image_height,
        columns,
        PREVIEW_GRID_GAP,
    )
    sheet_path = preview_out / f"{sheet_name}.bmp"
    write_bmp_24(sheet_path, sheet_width, sheet_height, sheet_pixels)
    write_png_copy(sheet_path)


def remove_generated_files(directory: Path, patterns: tuple[str, ...]) -> None:
    if not directory.is_dir():
        return

    for pattern in patterns:
        for path in directory.glob(pattern):
            if path.is_file():
                path.unlink()


def read_source_visible_bgr(source: Path) -> bytes:
    width, height, full_pixels = read_png_8bit_bgr(source)
    if width != SOURCE_IMAGE_WIDTH or height != SOURCE_IMAGE_HEIGHT:
        raise ValueError(
            f"{source} must be {SOURCE_IMAGE_WIDTH}x{SOURCE_IMAGE_HEIGHT}"
        )

    return crop_top_left_bgr(
        full_pixels,
        SOURCE_IMAGE_WIDTH,
        VISIBLE_CROP_WIDTH,
        VISIBLE_CROP_HEIGHT,
    )


def make_raw_theme_pixels(visible_pixels: bytes) -> bytes:
    raw_pixels = scale_bgr_nearest(
        visible_pixels,
        VISIBLE_CROP_WIDTH,
        VISIBLE_CROP_HEIGHT,
        RAW_WIDTH,
        RAW_HEIGHT,
    )
    return darken_bgr(raw_pixels)


def make_screen_theme_pixels(visible_pixels: bytes) -> tuple[bytes, bytes]:
    top_pixels = scale_bgr_nearest(
        visible_pixels,
        VISIBLE_CROP_WIDTH,
        VISIBLE_CROP_HEIGHT,
        TOP_PREVIEW_WIDTH,
        PREVIEW_HEIGHT,
    )
    bottom_pixels = scale_bgr_nearest(
        visible_pixels,
        VISIBLE_CROP_WIDTH,
        VISIBLE_CROP_HEIGHT,
        BOTTOM_PREVIEW_WIDTH,
        PREVIEW_HEIGHT,
    )
    return (
        darken_bgr(top_pixels, SCREEN_DARKEN_PERMILLE),
        darken_bgr(bottom_pixels, SCREEN_DARKEN_PERMILLE),
    )


def convert_source(source: Path) -> bytes:
    return make_raw_theme_pixels(read_source_visible_bgr(source))


def convert_source_outputs(
    source: Path,
    fe_repo: Optional[Path] = None,
) -> tuple[bytes, bytes, bytes]:
    raw_pixels, top_layers, bottom_layers = convert_source_layer_outputs(
        source,
        fe_repo,
    )
    return raw_pixels, top_layers[-1][1], bottom_layers[-1][1]


def convert_source_layer_outputs(
    source: Path,
    fe_repo: Optional[Path] = None,
) -> tuple[bytes, tuple[tuple[str, bytes], ...], tuple[tuple[str, bytes], ...]]:
    visible_pixels = read_source_visible_bgr(source)
    raw_pixels = make_raw_theme_pixels(visible_pixels)
    top_pixels, bottom_pixels = make_screen_theme_pixels(visible_pixels)
    top_layers: tuple[tuple[str, bytes], ...] = (("background", top_pixels),)
    bottom_layers: tuple[tuple[str, bytes], ...] = (("background", bottom_pixels),)
    if fe_repo is not None:
        top_background, top_legend, top_card, top_pixels = compose_app_ui_layers(
            top_pixels,
            TOP_PREVIEW_WIDTH,
            PREVIEW_HEIGHT,
            fe_repo,
        )
        (
            bottom_background,
            bottom_legend,
            bottom_card,
            bottom_pixels,
        ) = compose_app_ui_layers(
            bottom_pixels,
            BOTTOM_PREVIEW_WIDTH,
            PREVIEW_HEIGHT,
            fe_repo,
        )
        top_layers = (
            ("background", top_background),
            ("legend", top_legend),
            ("card", top_card),
            ("font", top_pixels),
        )
        bottom_layers = (
            ("background", bottom_background),
            ("legend", bottom_legend),
            ("card", bottom_card),
            ("font", bottom_pixels),
        )
    return raw_pixels, top_layers, bottom_layers


def scale_raw_for_screens(raw_pixels: bytes) -> tuple[bytes, bytes]:
    top_pixels = scale_bgr_nearest(
        raw_pixels,
        RAW_WIDTH,
        RAW_HEIGHT,
        TOP_PREVIEW_WIDTH,
        PREVIEW_HEIGHT,
    )
    bottom_pixels = scale_bgr_nearest(
        raw_pixels,
        RAW_WIDTH,
        RAW_HEIGHT,
        BOTTOM_PREVIEW_WIDTH,
        PREVIEW_HEIGHT,
    )
    return top_pixels, bottom_pixels


def write_theme_previews_from_pixels(
    preview_out: Path,
    theme_id: str,
    raw_pixels: bytes,
    top_pixels: bytes,
    bottom_pixels: bytes,
) -> None:
    raw_preview = preview_out / f"{theme_id}_raw_{RAW_WIDTH}x{RAW_HEIGHT}.bmp"
    top_preview = preview_out / f"{theme_id}_top_{TOP_PREVIEW_WIDTH}x{PREVIEW_HEIGHT}.bmp"
    bottom_preview = (
        preview_out / f"{theme_id}_bottom_{BOTTOM_PREVIEW_WIDTH}x{PREVIEW_HEIGHT}.bmp"
    )

    write_bmp_24(raw_preview, RAW_WIDTH, RAW_HEIGHT, raw_pixels)
    write_bmp_24(top_preview, TOP_PREVIEW_WIDTH, PREVIEW_HEIGHT, top_pixels)
    write_bmp_24(bottom_preview, BOTTOM_PREVIEW_WIDTH, PREVIEW_HEIGHT, bottom_pixels)
    write_png_copy(raw_preview)
    write_png_copy(top_preview)
    write_png_copy(bottom_preview)


def write_theme_layer_previews(
    preview_out: Path,
    theme_id: str,
    screen_name: str,
    screen_width: int,
    screen_height: int,
    layers: tuple[tuple[str, bytes], ...],
) -> None:
    for layer_name, layer_pixels in layers:
        layer_preview = (
            preview_out
            / f"{theme_id}_{screen_name}_layer_{layer_name}_{screen_width}x"
            f"{screen_height}.bmp"
        )
        write_bmp_24(layer_preview, screen_width, screen_height, layer_pixels)
        write_png_copy(layer_preview)


def write_theme_previews(
    preview_out: Path,
    theme_id: str,
    raw_pixels: bytes,
) -> tuple[bytes, bytes]:
    top_pixels, bottom_pixels = scale_raw_for_screens(raw_pixels)
    write_theme_previews_from_pixels(
        preview_out,
        theme_id,
        raw_pixels,
        top_pixels,
        bottom_pixels,
    )
    return top_pixels, bottom_pixels


def write_framebuffer_preview_pair(
    preview_out: Path,
    theme_id: str,
    screen_name: str,
    screen_width: int,
    screen_height: int,
    framebuffer_pixels: bytes,
) -> None:
    sideways_preview = (
        preview_out
        / f"{theme_id}_{screen_name}_fb_{screen_height}x{screen_width}.bmp"
    )
    roundtrip_preview = (
        preview_out
        / f"{theme_id}_{screen_name}_fb_roundtrip_{screen_width}x{screen_height}.bmp"
    )
    roundtrip_pixels = bgr_from_3ds_framebuffer(
        framebuffer_pixels,
        screen_width,
        screen_height,
    )

    write_bmp_24(sideways_preview, screen_height, screen_width, framebuffer_pixels)
    write_bmp_24(roundtrip_preview, screen_width, screen_height, roundtrip_pixels)
    write_png_copy(sideways_preview)
    write_png_copy(roundtrip_preview)


def write_theme_framebuffers(
    framebuffer_out: Path,
    preview_out: Optional[Path],
    theme_id: str,
    top_pixels: bytes,
    bottom_pixels: bytes,
) -> None:
    outputs = (
        ("top", TOP_PREVIEW_WIDTH, PREVIEW_HEIGHT, top_pixels),
        ("bottom", BOTTOM_PREVIEW_WIDTH, PREVIEW_HEIGHT, bottom_pixels),
    )

    framebuffer_out.mkdir(parents=True, exist_ok=True)
    for screen_name, screen_width, screen_height, screen_pixels in outputs:
        framebuffer_pixels = bgr_to_3ds_framebuffer(
            screen_pixels,
            screen_width,
            screen_height,
        )
        raw_path = (
            framebuffer_out
            / f"fe_bg_{theme_id}_{screen_name}_{screen_width}x{screen_height}"
            "_bgr888_fb.bin"
        )
        raw_path.write_bytes(framebuffer_pixels)

        if preview_out is not None:
            write_framebuffer_preview_pair(
                preview_out,
                theme_id,
                screen_name,
                screen_width,
                screen_height,
                framebuffer_pixels,
            )


def write_theme_layer_framebuffers(
    framebuffer_out: Path,
    theme_id: str,
    top_layers: tuple[tuple[str, bytes], ...],
    bottom_layers: tuple[tuple[str, bytes], ...],
) -> None:
    layer_out = framebuffer_out / "layers"
    outputs = (
        ("top", TOP_PREVIEW_WIDTH, PREVIEW_HEIGHT, top_layers),
        ("bottom", BOTTOM_PREVIEW_WIDTH, PREVIEW_HEIGHT, bottom_layers),
    )

    layer_out.mkdir(parents=True, exist_ok=True)
    for screen_name, screen_width, screen_height, layers in outputs:
        for layer_name, layer_pixels in layers:
            framebuffer_pixels = bgr_to_3ds_framebuffer(
                layer_pixels,
                screen_width,
                screen_height,
            )
            raw_path = (
                layer_out
                / f"fe_bg_{theme_id}_{screen_name}_layer_{layer_name}_"
                f"{screen_width}x{screen_height}_bgr888_fb.bin"
            )
            raw_path.write_bytes(framebuffer_pixels)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--fe-repo",
        type=Path,
        default=Path(os.environ.get("FE_REPO_PATH", "~/codebase/FE-Repo")).expanduser(),
        help="Path to a local Klokinator/FE-Repo clone.",
    )
    parser.add_argument(
        "--raw-out",
        type=Path,
        default=Path("assets/fe-themes/raw"),
        help="Directory for generated raw BGR888 files.",
    )
    parser.add_argument(
        "--preview-out",
        type=Path,
        default=Path("build/fe-theme-previews"),
        help="Directory for generated BMP previews.",
    )
    parser.add_argument(
        "--framebuffer-out",
        type=Path,
        default=Path("build/fe-theme-framebuffers"),
        help="Directory for generated 3DS framebuffer-ready BGR888 files.",
    )
    parser.add_argument(
        "--no-previews",
        action="store_true",
        help="Skip generated BMP/PNG previews.",
    )
    parser.add_argument(
        "--no-framebuffers",
        action="store_true",
        help="Skip generated 3DS framebuffer-ready BGR888 files.",
    )
    args = parser.parse_args()

    args.raw_out.mkdir(parents=True, exist_ok=True)
    remove_generated_files(args.raw_out, ("fe_bg_*_bgr888.bin",))
    if not args.no_framebuffers:
        args.framebuffer_out.mkdir(parents=True, exist_ok=True)
        remove_generated_files(
            args.framebuffer_out,
            ("fe_bg_*_bgr888_fb.bin", "fe_font_*_alpha.bin"),
        )
        remove_generated_files(
            args.framebuffer_out / "layers",
            ("fe_bg_*_layer_*_bgr888_fb.bin",),
        )
    if not args.no_previews:
        args.preview_out.mkdir(parents=True, exist_ok=True)
        remove_generated_files(args.preview_out, ("*.bmp", "*.png"))

    top_previews: list[bytes] = []
    bottom_previews: list[bytes] = []
    top_layer_previews: list[bytes] = []
    bottom_layer_previews: list[bytes] = []

    if not args.no_framebuffers:
        write_font_atlas(args.framebuffer_out, args.fe_repo)

    for theme_id, title, credit, relative_path in THEMES:
        source = args.fe_repo / relative_path
        if not source.is_file():
            raise FileNotFoundError(source)

        raw_pixels, top_layers, bottom_layers = convert_source_layer_outputs(
            source,
            args.fe_repo,
        )
        top_pixels = top_layers[-1][1]
        bottom_pixels = bottom_layers[-1][1]
        raw_path = args.raw_out / f"fe_bg_{theme_id}_{RAW_WIDTH}x{RAW_HEIGHT}_bgr888.bin"
        raw_path.write_bytes(raw_pixels)

        if not args.no_previews:
            write_theme_previews_from_pixels(
                args.preview_out,
                theme_id,
                raw_pixels,
                top_pixels,
                bottom_pixels,
            )
            write_theme_layer_previews(
                args.preview_out,
                theme_id,
                "top",
                TOP_PREVIEW_WIDTH,
                PREVIEW_HEIGHT,
                top_layers,
            )
            write_theme_layer_previews(
                args.preview_out,
                theme_id,
                "bottom",
                BOTTOM_PREVIEW_WIDTH,
                PREVIEW_HEIGHT,
                bottom_layers,
            )
            top_previews.append(top_pixels)
            bottom_previews.append(bottom_pixels)
            top_layer_previews.extend(layer_pixels for _, layer_pixels in top_layers)
            bottom_layer_previews.extend(
                layer_pixels for _, layer_pixels in bottom_layers
            )

        if not args.no_framebuffers:
            write_theme_framebuffers(
                args.framebuffer_out,
                None if args.no_previews else args.preview_out,
                theme_id,
                top_pixels,
                bottom_pixels,
            )
            write_theme_layer_framebuffers(
                args.framebuffer_out,
                theme_id,
                top_layers,
                bottom_layers,
            )

        print(f"{theme_id}: {title}, {credit}, {raw_path}")

    if not args.no_previews:
        write_preview_sheet(
            args.preview_out,
            "themes_top_sheet",
            top_previews,
            TOP_PREVIEW_WIDTH,
            PREVIEW_HEIGHT,
            PREVIEW_GRID_COLUMNS,
        )
        write_preview_sheet(
            args.preview_out,
            "themes_bottom_sheet",
            bottom_previews,
            BOTTOM_PREVIEW_WIDTH,
            PREVIEW_HEIGHT,
            PREVIEW_GRID_COLUMNS,
        )
        write_preview_sheet(
            args.preview_out,
            "themes_top_layers_sheet",
            top_layer_previews,
            TOP_PREVIEW_WIDTH,
            PREVIEW_HEIGHT,
            LAYER_PREVIEW_GRID_COLUMNS,
        )
        write_preview_sheet(
            args.preview_out,
            "themes_bottom_layers_sheet",
            bottom_layer_previews,
            BOTTOM_PREVIEW_WIDTH,
            PREVIEW_HEIGHT,
            LAYER_PREVIEW_GRID_COLUMNS,
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
