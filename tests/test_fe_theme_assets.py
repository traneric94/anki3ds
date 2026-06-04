import importlib.util
import struct
import tempfile
import unittest
import zlib
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
IMPORT_FE_THEME_ASSETS_PATH = ROOT / "tools" / "import_fe_theme_assets.py"
IMPORT_FE_THEME_ASSETS_SPEC = importlib.util.spec_from_file_location(
    "import_fe_theme_assets",
    IMPORT_FE_THEME_ASSETS_PATH,
)
import_fe_theme_assets = importlib.util.module_from_spec(IMPORT_FE_THEME_ASSETS_SPEC)
assert IMPORT_FE_THEME_ASSETS_SPEC.loader is not None
IMPORT_FE_THEME_ASSETS_SPEC.loader.exec_module(import_fe_theme_assets)


def png_chunk(chunk_type: bytes, payload: bytes) -> bytes:
    crc = zlib.crc32(chunk_type)
    crc = zlib.crc32(payload, crc) & 0xFFFFFFFF
    return (
        struct.pack(">I", len(payload))
        + chunk_type
        + payload
        + struct.pack(">I", crc)
    )


def write_gradient_png(path: Path, width: int, height: int) -> None:
    scanlines = bytearray()
    for y in range(height):
        scanlines.append(0)
        for x in range(width):
            scanlines.extend((x & 0xFF, y & 0xFF, (x + y) & 0xFF))

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        import_fe_theme_assets.PNG_SIGNATURE
        + png_chunk(
            b"IHDR",
            struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0),
        )
        + png_chunk(b"IDAT", zlib.compress(bytes(scanlines)))
        + png_chunk(b"IEND", b"")
    )


def write_palette_png(path: Path) -> None:
    palette = bytes(
        (
            0x10,
            0x20,
            0x30,
            0x40,
            0x50,
            0x60,
            0x70,
            0x80,
            0x90,
        )
    )
    scanlines = bytes(
        (
            0,
            0,
            1,
            2,
            0,
            2,
            1,
            0,
        )
    )

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        import_fe_theme_assets.PNG_SIGNATURE
        + png_chunk(
            b"IHDR",
            struct.pack(">IIBBBBB", 3, 2, 8, 3, 0, 0, 0),
        )
        + png_chunk(b"PLTE", palette)
        + png_chunk(b"IDAT", zlib.compress(scanlines))
        + png_chunk(b"IEND", b"")
    )


def read_bmp_dimensions(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise AssertionError(f"{path} is not a BMP")
    return struct.unpack_from("<ii", data, 18)


class FeThemeAssetTests(unittest.TestCase):
    def test_read_png_8bit_bgr_expands_indexed_palette(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "indexed.png"
            write_palette_png(source)

            width, height, pixels = import_fe_theme_assets.read_png_8bit_bgr(source)

            self.assertEqual((width, height), (3, 2))
            self.assertEqual(
                pixels,
                bytes(
                    (
                        0x30,
                        0x20,
                        0x10,
                        0x60,
                        0x50,
                        0x40,
                        0x90,
                        0x80,
                        0x70,
                        0x90,
                        0x80,
                        0x70,
                        0x60,
                        0x50,
                        0x40,
                        0x30,
                        0x20,
                        0x10,
                    )
                ),
            )

    def test_convert_source_outputs_darken_scaled_bgr888(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "source.png"
            write_gradient_png(
                source,
                import_fe_theme_assets.SOURCE_IMAGE_WIDTH,
                import_fe_theme_assets.SOURCE_IMAGE_HEIGHT,
            )

            raw_pixels = import_fe_theme_assets.convert_source(source)

            self.assertEqual(
                len(raw_pixels),
                import_fe_theme_assets.RAW_WIDTH * import_fe_theme_assets.RAW_HEIGHT * 3,
            )
            offset = (30 * import_fe_theme_assets.RAW_WIDTH + 50) * 3
            self.assertEqual(raw_pixels[offset:offset + 3], bytes((68, 25, 43)))

    def test_3ds_framebuffer_layout_round_trips_screen_pixels(self):
        pixels = bytes(
            (
                0x00,
                0x01,
                0x02,
                0x10,
                0x11,
                0x12,
                0x20,
                0x21,
                0x22,
                0x30,
                0x31,
                0x32,
                0x40,
                0x41,
                0x42,
                0x50,
                0x51,
                0x52,
            )
        )

        framebuffer = import_fe_theme_assets.bgr_to_3ds_framebuffer(pixels, 3, 2)

        self.assertEqual(len(framebuffer), len(pixels))
        self.assertEqual(framebuffer[0:3], pixels[9:12])
        self.assertEqual(framebuffer[3:6], pixels[0:3])
        self.assertEqual(framebuffer[12:15], pixels[15:18])
        self.assertEqual(framebuffer[15:18], pixels[6:9])
        self.assertEqual(
            import_fe_theme_assets.bgr_from_3ds_framebuffer(framebuffer, 3, 2),
            pixels,
        )

    def test_write_theme_previews_uses_screen_dimensions(self):
        raw_pixels = bytes(
            (
                (index * 3) & 0xFF
                for index in range(
                    import_fe_theme_assets.RAW_WIDTH
                    * import_fe_theme_assets.RAW_HEIGHT
                    * 3
                )
            )
        )

        with tempfile.TemporaryDirectory() as temp_dir, mock.patch.object(
            import_fe_theme_assets,
            "write_png_copy",
        ):
            output = Path(temp_dir)

            top_pixels, bottom_pixels = import_fe_theme_assets.write_theme_previews(
                output,
                "sample",
                raw_pixels,
            )

            self.assertEqual(
                len(top_pixels),
                import_fe_theme_assets.TOP_PREVIEW_WIDTH
                * import_fe_theme_assets.PREVIEW_HEIGHT
                * 3,
            )
            self.assertEqual(
                len(bottom_pixels),
                import_fe_theme_assets.BOTTOM_PREVIEW_WIDTH
                * import_fe_theme_assets.PREVIEW_HEIGHT
                * 3,
            )
            self.assertEqual(
                read_bmp_dimensions(output / "sample_raw_128x80.bmp"),
                (
                    import_fe_theme_assets.RAW_WIDTH,
                    -import_fe_theme_assets.RAW_HEIGHT,
                ),
            )
            self.assertEqual(
                read_bmp_dimensions(output / "sample_top_400x240.bmp"),
                (
                    import_fe_theme_assets.TOP_PREVIEW_WIDTH,
                    -import_fe_theme_assets.PREVIEW_HEIGHT,
                ),
            )
            self.assertEqual(
                read_bmp_dimensions(output / "sample_bottom_320x240.bmp"),
                (
                    import_fe_theme_assets.BOTTOM_PREVIEW_WIDTH,
                    -import_fe_theme_assets.PREVIEW_HEIGHT,
                ),
            )

    def test_write_theme_framebuffers_outputs_direct_copy_bins(self):
        top_pixels = bytes(
            (
                (index * 5) & 0xFF
                for index in range(
                    import_fe_theme_assets.TOP_PREVIEW_WIDTH
                    * import_fe_theme_assets.PREVIEW_HEIGHT
                    * 3
                )
            )
        )
        bottom_pixels = bytes(
            (
                (index * 7) & 0xFF
                for index in range(
                    import_fe_theme_assets.BOTTOM_PREVIEW_WIDTH
                    * import_fe_theme_assets.PREVIEW_HEIGHT
                    * 3
                )
            )
        )

        with tempfile.TemporaryDirectory() as temp_dir, mock.patch.object(
            import_fe_theme_assets,
            "write_png_copy",
        ):
            output = Path(temp_dir)
            framebuffer_out = output / "framebuffers"
            preview_out = output / "previews"
            preview_out.mkdir()

            import_fe_theme_assets.write_theme_framebuffers(
                framebuffer_out,
                preview_out,
                "sample",
                top_pixels,
                bottom_pixels,
            )

            top_bin = (
                framebuffer_out / "fe_bg_sample_top_400x240_bgr888_fb.bin"
            )
            bottom_bin = (
                framebuffer_out / "fe_bg_sample_bottom_320x240_bgr888_fb.bin"
            )
            self.assertEqual(
                top_bin.stat().st_size,
                import_fe_theme_assets.TOP_PREVIEW_WIDTH
                * import_fe_theme_assets.PREVIEW_HEIGHT
                * 3,
            )
            self.assertEqual(
                bottom_bin.stat().st_size,
                import_fe_theme_assets.BOTTOM_PREVIEW_WIDTH
                * import_fe_theme_assets.PREVIEW_HEIGHT
                * 3,
            )
            self.assertEqual(
                read_bmp_dimensions(preview_out / "sample_top_fb_240x400.bmp"),
                (
                    import_fe_theme_assets.PREVIEW_HEIGHT,
                    -import_fe_theme_assets.TOP_PREVIEW_WIDTH,
                ),
            )
            self.assertEqual(
                read_bmp_dimensions(
                    preview_out / "sample_top_fb_roundtrip_400x240.bmp"
                ),
                (
                    import_fe_theme_assets.TOP_PREVIEW_WIDTH,
                    -import_fe_theme_assets.PREVIEW_HEIGHT,
                ),
            )
            self.assertEqual(
                read_bmp_dimensions(preview_out / "sample_bottom_fb_240x320.bmp"),
                (
                    import_fe_theme_assets.PREVIEW_HEIGHT,
                    -import_fe_theme_assets.BOTTOM_PREVIEW_WIDTH,
                ),
            )
            self.assertEqual(
                read_bmp_dimensions(
                    preview_out / "sample_bottom_fb_roundtrip_320x240.bmp"
                ),
                (
                    import_fe_theme_assets.BOTTOM_PREVIEW_WIDTH,
                    -import_fe_theme_assets.PREVIEW_HEIGHT,
                ),
            )


if __name__ == "__main__":
    unittest.main()
