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


def read_bmp_dimensions(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise AssertionError(f"{path} is not a BMP")
    return struct.unpack_from("<ii", data, 18)


class FeThemeAssetTests(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
