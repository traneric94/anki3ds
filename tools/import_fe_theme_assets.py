#!/usr/bin/env python3
"""Convert selected FE-Repo backgrounds into raw BGR assets and previews."""

from __future__ import annotations

import argparse
import os
import struct
import subprocess
import zlib
from pathlib import Path


SOURCE_IMAGE_WIDTH = 256
SOURCE_IMAGE_HEIGHT = 160
RAW_WIDTH = 128
RAW_HEIGHT = 80
VISIBLE_CROP_WIDTH = 256
VISIBLE_CROP_HEIGHT = 160
DARKEN_PERMILLE = 430
TOP_PREVIEW_WIDTH = 400
BOTTOM_PREVIEW_WIDTH = 320
PREVIEW_HEIGHT = 240
PREVIEW_GRID_COLUMNS = 2
PREVIEW_GRID_GAP = 8
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"

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
    if bit_depth != 8:
        raise ValueError(f"{path} must be an 8-bit PNG")
    if compression_method != 0 or filter_method != 0 or interlace_method != 0:
        raise ValueError(f"{path} uses unsupported PNG compression/filter/interlace")

    if color_type == 3:
        bytes_per_pixel = 1
    elif color_type == 2:
        bytes_per_pixel = 3
    elif color_type == 6:
        bytes_per_pixel = 4
    else:
        raise ValueError(f"{path} uses unsupported PNG color type {color_type}")

    raw = zlib.decompress(bytes(compressed))
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
                palette_index = scanline[x]
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


def darken_bgr(pixels_bgr: bytes) -> bytes:
    return bytes((value * DARKEN_PERMILLE) // 1000 for value in pixels_bgr)


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


def remove_generated_files(directory: Path, patterns: tuple[str, ...]) -> None:
    if not directory.is_dir():
        return

    for pattern in patterns:
        for path in directory.glob(pattern):
            if path.is_file():
                path.unlink()


def convert_source(source: Path) -> bytes:
    width, height, full_pixels = read_png_8bit_bgr(source)
    if width != SOURCE_IMAGE_WIDTH or height != SOURCE_IMAGE_HEIGHT:
        raise ValueError(
            f"{source} must be {SOURCE_IMAGE_WIDTH}x{SOURCE_IMAGE_HEIGHT}"
        )

    cropped_pixels = crop_top_left_bgr(
        full_pixels,
        SOURCE_IMAGE_WIDTH,
        VISIBLE_CROP_WIDTH,
        VISIBLE_CROP_HEIGHT,
    )
    raw_pixels = scale_bgr_nearest(
        cropped_pixels,
        VISIBLE_CROP_WIDTH,
        VISIBLE_CROP_HEIGHT,
        RAW_WIDTH,
        RAW_HEIGHT,
    )
    return darken_bgr(raw_pixels)


def write_theme_previews(
    preview_out: Path,
    theme_id: str,
    raw_pixels: bytes,
) -> tuple[bytes, bytes]:
    raw_preview = preview_out / f"{theme_id}_raw_{RAW_WIDTH}x{RAW_HEIGHT}.bmp"
    top_preview = preview_out / f"{theme_id}_top_{TOP_PREVIEW_WIDTH}x{PREVIEW_HEIGHT}.bmp"
    bottom_preview = (
        preview_out / f"{theme_id}_bottom_{BOTTOM_PREVIEW_WIDTH}x{PREVIEW_HEIGHT}.bmp"
    )
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

    write_bmp_24(raw_preview, RAW_WIDTH, RAW_HEIGHT, raw_pixels)
    write_bmp_24(top_preview, TOP_PREVIEW_WIDTH, PREVIEW_HEIGHT, top_pixels)
    write_bmp_24(bottom_preview, BOTTOM_PREVIEW_WIDTH, PREVIEW_HEIGHT, bottom_pixels)
    write_png_copy(raw_preview)
    write_png_copy(top_preview)
    write_png_copy(bottom_preview)
    return top_pixels, bottom_pixels


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
        "--no-previews",
        action="store_true",
        help="Only write raw BGR888 files.",
    )
    args = parser.parse_args()

    args.raw_out.mkdir(parents=True, exist_ok=True)
    remove_generated_files(args.raw_out, ("fe_bg_*_bgr888.bin",))
    if not args.no_previews:
        args.preview_out.mkdir(parents=True, exist_ok=True)
        remove_generated_files(args.preview_out, ("*.bmp", "*.png"))

    top_previews: list[bytes] = []
    bottom_previews: list[bytes] = []

    for theme_id, title, credit, relative_path in THEMES:
        source = args.fe_repo / relative_path
        if not source.is_file():
            raise FileNotFoundError(source)

        raw_pixels = convert_source(source)
        raw_path = args.raw_out / f"fe_bg_{theme_id}_{RAW_WIDTH}x{RAW_HEIGHT}_bgr888.bin"
        raw_path.write_bytes(raw_pixels)

        if not args.no_previews:
            top_pixels, bottom_pixels = write_theme_previews(
                args.preview_out,
                theme_id,
                raw_pixels,
            )
            top_previews.append(top_pixels)
            bottom_previews.append(bottom_pixels)

        print(f"{theme_id}: {title}, {credit}, {raw_path}")

    if not args.no_previews:
        top_width, top_height, top_grid = make_grid(
            top_previews,
            TOP_PREVIEW_WIDTH,
            PREVIEW_HEIGHT,
            PREVIEW_GRID_COLUMNS,
            PREVIEW_GRID_GAP,
        )
        bottom_width, bottom_height, bottom_grid = make_grid(
            bottom_previews,
            BOTTOM_PREVIEW_WIDTH,
            PREVIEW_HEIGHT,
            PREVIEW_GRID_COLUMNS,
            PREVIEW_GRID_GAP,
        )
        top_sheet = args.preview_out / "themes_top_sheet.bmp"
        bottom_sheet = args.preview_out / "themes_bottom_sheet.bmp"
        write_bmp_24(top_sheet, top_width, top_height, top_grid)
        write_bmp_24(
            bottom_sheet,
            bottom_width,
            bottom_height,
            bottom_grid,
        )
        write_png_copy(top_sheet)
        write_png_copy(bottom_sheet)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
