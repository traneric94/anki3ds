import importlib.util
import re
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

APP_RENDERER_C2D_PATH = ROOT / "app-3ds/source/app_renderer_c2d.c"


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


def renderer_rect(source: str, name: str) -> tuple[float, float, float, float]:
    pattern = re.compile(
        rf"static const struct app_render_rect {re.escape(name)}\s*=\s*"
        r"\{\s*"
        r"([0-9.]+)f,\s*"
        r"([0-9.]+)f,\s*"
        r"([0-9.]+)f,\s*"
        r"([0-9.]+)f,\s*"
        r"\};",
        re.MULTILINE,
    )
    match = pattern.search(source)
    if match is None:
        raise AssertionError(f"missing renderer rect: {name}")
    return tuple(float(group) for group in match.groups())


def write_solid_palette_png(
    path: Path,
    width: int,
    height: int,
    palette: list[tuple[int, int, int]],
    indexes: list[int],
) -> None:
    if len(indexes) != width * height:
        raise AssertionError("index count must match dimensions")

    scanlines = bytearray()
    for y in range(height):
        scanlines.append(0)
        scanlines.extend(indexes[y * width:(y + 1) * width])

    palette_bytes = bytearray()
    for red, green, blue in palette:
        palette_bytes.extend((red, green, blue))

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        import_fe_theme_assets.PNG_SIGNATURE
        + png_chunk(
            b"IHDR",
            struct.pack(">IIBBBBB", width, height, 8, 3, 0, 0, 0),
        )
        + png_chunk(b"PLTE", bytes(palette_bytes))
        + png_chunk(b"IDAT", zlib.compress(bytes(scanlines)))
        + png_chunk(b"IEND", b"")
    )


def write_packed_palette_png(
    path: Path,
    width: int,
    height: int,
    bit_depth: int,
    palette: list[tuple[int, int, int]],
    indexes: list[int],
) -> None:
    if len(indexes) != width * height:
        raise AssertionError("index count must match dimensions")

    scanlines = bytearray()
    mask = (1 << bit_depth) - 1
    for y in range(height):
        scanlines.append(0)
        row = indexes[y * width:(y + 1) * width]
        byte_value = 0
        bits_used = 0
        for palette_index in row:
            byte_value = (byte_value << bit_depth) | (palette_index & mask)
            bits_used += bit_depth
            if bits_used == 8:
                scanlines.append(byte_value)
                byte_value = 0
                bits_used = 0
        if bits_used > 0:
            scanlines.append(byte_value << (8 - bits_used))

    palette_bytes = bytearray()
    for red, green, blue in palette:
        palette_bytes.extend((red, green, blue))

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        import_fe_theme_assets.PNG_SIGNATURE
        + png_chunk(
            b"IHDR",
            struct.pack(">IIBBBBB", width, height, bit_depth, 3, 0, 0, 0),
        )
        + png_chunk(b"PLTE", bytes(palette_bytes))
        + png_chunk(b"IDAT", zlib.compress(bytes(scanlines)))
        + png_chunk(b"IEND", b"")
    )


def read_bmp_dimensions(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise AssertionError(f"{path} is not a BMP")
    return struct.unpack_from("<ii", data, 18)


def pixel_at_bgr(pixels: bytes, width: int, x: int, y: int) -> bytes:
    offset = (y * width + x) * 3
    return pixels[offset:offset + 3]


def bgr_luminance(pixel: bytes) -> float:
    return 0.114 * pixel[0] + 0.587 * pixel[1] + 0.299 * pixel[2]


def rgba_alpha_at(pixels: bytes, width: int, x: int, y: int) -> int:
    return pixels[(y * width + x) * 4 + 3]


def count_bgr_pixels(pixels: bytes, color: bytes) -> int:
    return sum(
        1
        for offset in range(0, len(pixels), 3)
        if pixels[offset:offset + 3] == color
    )


def write_ui_fixture_assets(fe_repo: Path) -> None:
    frame_path = fe_repo / import_fe_theme_assets.UI_FRAME_RELATIVE_PATH
    frame_indexes = [0] * 16
    frame_indexes[5] = 1
    write_solid_palette_png(
        frame_path,
        4,
        4,
        [(0x20, 0x30, 0x40), (0xA0, 0x80, 0x20)],
        frame_indexes,
    )

    card_frame_path = fe_repo / import_fe_theme_assets.UI_CARD_FRAME_RELATIVE_PATH
    card_indexes = [1] * 256
    for index in range(16):
        card_indexes[index] = 3
        card_indexes[15 * 16 + index] = 0
    write_solid_palette_png(
        card_frame_path,
        16,
        16,
        [
            (0x38, 0x20, 0x18),
            (0x90, 0x48, 0x28),
            (0xC8, 0x78, 0x38),
            (0xF8, 0xC0, 0x40),
        ],
        card_indexes,
    )

    for relative_path in import_fe_theme_assets.UI_ICON_RELATIVE_PATHS:
        icon_indexes = [0] * 16
        icon_indexes[5] = 1
        write_solid_palette_png(
            fe_repo / relative_path,
            4,
            4,
            [(0xFF, 0xFF, 0xFF), (0x10, 0x90, 0xD0)],
            icon_indexes,
        )


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

    def test_read_png_bgr_unpacks_low_bit_depth_indexed_palette(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "indexed-2bit.png"
            write_packed_palette_png(
                source,
                4,
                1,
                2,
                [
                    (0x00, 0x10, 0x20),
                    (0x30, 0x40, 0x50),
                    (0x60, 0x70, 0x80),
                    (0x90, 0xA0, 0xB0),
                ],
                [0, 1, 2, 3],
            )

            width, height, pixels = import_fe_theme_assets.read_png_8bit_bgr(source)

            self.assertEqual((width, height), (4, 1))
            self.assertEqual(
                pixels,
                bytes(
                    (
                        0x20,
                        0x10,
                        0x00,
                        0x50,
                        0x40,
                        0x30,
                        0x80,
                        0x70,
                        0x60,
                        0xB0,
                        0xA0,
                        0x90,
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
            self.assertEqual(raw_pixels[offset:offset + 3], bytes((65, 25, 39)))

    def test_convert_source_outputs_scales_screens_from_visible_art_crop(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "source.png"
            write_gradient_png(
                source,
                import_fe_theme_assets.SOURCE_IMAGE_WIDTH,
                import_fe_theme_assets.SOURCE_IMAGE_HEIGHT,
            )

            raw_pixels, top_pixels, bottom_pixels = (
                import_fe_theme_assets.convert_source_outputs(source)
            )
            low_res_top_pixels, low_res_bottom_pixels = (
                import_fe_theme_assets.scale_raw_for_screens(raw_pixels)
            )

            top_offset = (119 * import_fe_theme_assets.TOP_PREVIEW_WIDTH + 199) * 3
            bottom_offset = (
                119 * import_fe_theme_assets.BOTTOM_PREVIEW_WIDTH + 159
            ) * 3
            self.assertEqual(top_pixels[top_offset:top_offset + 3], bytes((168, 67, 101)))
            self.assertEqual(
                bottom_pixels[bottom_offset:bottom_offset + 3],
                bytes((168, 67, 101)),
            )
            self.assertNotEqual(
                top_pixels[top_offset:top_offset + 3],
                low_res_top_pixels[top_offset:top_offset + 3],
            )
            self.assertNotEqual(
                bottom_pixels[bottom_offset:bottom_offset + 3],
                low_res_bottom_pixels[bottom_offset:bottom_offset + 3],
            )

    def test_screen_background_crop_omits_source_right_gutter(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "source.png"
            write_gradient_png(
                source,
                import_fe_theme_assets.SOURCE_IMAGE_WIDTH,
                import_fe_theme_assets.SOURCE_IMAGE_HEIGHT,
            )

            _, top_pixels, bottom_pixels = (
                import_fe_theme_assets.convert_source_outputs(source)
            )

            top_right_offset = (import_fe_theme_assets.TOP_PREVIEW_WIDTH - 1) * 3
            bottom_right_offset = (
                import_fe_theme_assets.BOTTOM_PREVIEW_WIDTH - 1
            ) * 3
            self.assertEqual(
                top_pixels[top_right_offset:top_right_offset + 3],
                bytes((203, 0, 203)),
            )
            self.assertEqual(
                bottom_pixels[bottom_right_offset:bottom_right_offset + 3],
                bytes((203, 0, 203)),
            )

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

    def test_apply_app_ui_chrome_composes_card_title_and_icons(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            fe_repo = Path(temp_dir) / "FE-Repo"
            write_ui_fixture_assets(fe_repo)

            screen_width = 100
            screen_height = import_fe_theme_assets.PREVIEW_HEIGHT
            base = bytes([0] * screen_width * screen_height * 3)
            output = import_fe_theme_assets.apply_app_ui_chrome(
                base,
                screen_width,
                screen_height,
                fe_repo,
            )

            self.assertEqual(len(output), len(base))
            self.assertNotEqual(output, base)
            panel_x, panel_y, panel_width, panel_height = (
                import_fe_theme_assets.legend_dialogue_rect(
                    screen_width,
                    screen_height,
                )
            )
            self.assertGreaterEqual(
                bgr_luminance(
                    pixel_at_bgr(
                        output,
                        screen_width,
                        panel_x + panel_width // 2,
                        panel_y + panel_height // 2,
                    )
                ),
                160,
            )
            self.assertIn(import_fe_theme_assets.UI_DIALOG_TEXT_BGR, output)
            self.assertNotIn(import_fe_theme_assets.UI_CARD_FILL_BGR, output)
            self.assertNotIn(import_fe_theme_assets.UI_CARD_TEXT_BGR, output)
            self.assertNotIn(bytes((0xD0, 0x90, 0x10)), output)
            self.assertNotIn(bytes((0xB8, 0xD8, 0xF8)), output)

    def test_compose_app_ui_layers_keeps_font_last(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            fe_repo = Path(temp_dir) / "FE-Repo"
            write_ui_fixture_assets(fe_repo)

            base = bytes([0] * import_fe_theme_assets.TOP_PREVIEW_WIDTH * 80 * 3)
            (
                background,
                legend,
                card,
                font,
            ) = import_fe_theme_assets.compose_app_ui_layers(
                base,
                import_fe_theme_assets.TOP_PREVIEW_WIDTH,
                80,
                fe_repo,
            )

            self.assertEqual(background, base)
            self.assertNotEqual(legend, background)
            self.assertNotIn(import_fe_theme_assets.UI_CARD_FILL_BGR, legend)
            self.assertNotIn(import_fe_theme_assets.UI_CARD_TEXT_BGR, legend)
            panel_x, panel_y, _, _ = import_fe_theme_assets.legend_dialogue_rect(
                import_fe_theme_assets.TOP_PREVIEW_WIDTH,
                80,
            )
            center_x = import_fe_theme_assets.TOP_PREVIEW_WIDTH // 2
            center_y = panel_y + 80 // 4
            self.assertEqual(
                pixel_at_bgr(legend, 400, panel_x, panel_y),
                pixel_at_bgr(background, 400, panel_x, panel_y),
            )
            self.assertNotEqual(
                pixel_at_bgr(legend, 400, center_x, center_y),
                pixel_at_bgr(background, 400, center_x, center_y),
            )
            self.assertEqual(card, legend)
            self.assertNotIn(import_fe_theme_assets.UI_CARD_TEXT_BGR, card)
            self.assertNotIn(bytes((0xB8, 0xD8, 0xF8)), card)
            self.assertNotIn(import_fe_theme_assets.UI_CARD_FILL_BGR, card)
            self.assertNotIn(import_fe_theme_assets.UI_CARD_FILL_ALT_BGR, card)
            self.assertEqual(
                count_bgr_pixels(card, import_fe_theme_assets.UI_DIALOG_TEXT_BGR),
                0,
            )
            self.assertNotIn(bytes((0xB8, 0xD8, 0xF8)), font)
            self.assertIn(import_fe_theme_assets.UI_DIALOG_TEXT_BGR, font)
            self.assertGreater(
                count_bgr_pixels(font, import_fe_theme_assets.UI_DIALOG_TEXT_BGR),
                0,
            )
            self.assertNotEqual(font, card)

    def test_dialogue_panel_uses_old_parchment_before_fallbacks(self):
        width = 96
        height = 64
        destination = bytearray(bytes((0x01, 0x02, 0x03)) * width * height)

        with mock.patch.object(
            import_fe_theme_assets,
            "draw_dialogue_panel_from_old_parchment",
            return_value=True,
        ) as old_parchment:
            with mock.patch.object(
                import_fe_theme_assets,
                "draw_dialogue_panel_from_parchment_gui",
                return_value=True,
            ) as parchment_gui:
                with mock.patch.object(
                    import_fe_theme_assets,
                    "draw_dialogue_panel_from_reference",
                    return_value=True,
                ) as reference:
                    import_fe_theme_assets.draw_dialogue_panel(
                        destination,
                        width,
                        height,
                        8,
                        8,
                        80,
                        44,
                        bytes((0x10, 0x20, 0x30)),
                    )

        old_parchment.assert_called_once()
        parchment_gui.assert_not_called()
        reference.assert_not_called()

    def test_dialogue_panel_falls_back_after_old_parchment(self):
        width = 96
        height = 64
        destination = bytearray(bytes((0x01, 0x02, 0x03)) * width * height)

        with mock.patch.object(
            import_fe_theme_assets,
            "draw_dialogue_panel_from_old_parchment",
            return_value=False,
        ) as old_parchment:
            with mock.patch.object(
                import_fe_theme_assets,
                "draw_dialogue_panel_from_parchment_gui",
                return_value=False,
            ) as parchment_gui:
                with mock.patch.object(
                    import_fe_theme_assets,
                    "draw_dialogue_panel_from_reference",
                    return_value=True,
                ) as reference:
                    import_fe_theme_assets.draw_dialogue_panel(
                        destination,
                        width,
                        height,
                        8,
                        8,
                        80,
                        44,
                        bytes((0x10, 0x20, 0x30)),
                    )

        old_parchment.assert_called_once()
        parchment_gui.assert_called_once()
        reference.assert_called_once()

    def test_card_layer_is_inert_for_dialogue_checkpoint(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            fe_repo = Path(temp_dir) / "FE-Repo"
            write_ui_fixture_assets(fe_repo)

            width = import_fe_theme_assets.TOP_PREVIEW_WIDTH
            height = import_fe_theme_assets.PREVIEW_HEIGHT
            base_color = bytes((0x04, 0x05, 0x06))
            base = base_color * width * height

            card = import_fe_theme_assets.apply_card_layer(
                base,
                width,
                height,
                fe_repo,
            )

            self.assertEqual(card, base)
            self.assertEqual(pixel_at_bgr(card, width, 0, 0), base_color)
            self.assertNotIn(import_fe_theme_assets.UI_CARD_TRIM_BRIGHT_BGR, card)
            self.assertNotIn(import_fe_theme_assets.UI_CARD_TEXT_BGR, card)

            legend = import_fe_theme_assets.apply_legend_layer(
                card,
                width,
                height,
                fe_repo,
            )
            font = import_fe_theme_assets.apply_font_layer(
                legend,
                width,
                height,
                fe_repo,
            )
            self.assertIn(import_fe_theme_assets.UI_DIALOG_TEXT_BGR, font)

    def test_old_parchment_source_drives_active_dialogue_panel(self):
        self.assertEqual(
            import_fe_theme_assets.UI_OLD_PARCHMENT_RELATIVE_PATH,
            Path(
                "assets/fe-themes/external/opengameart/"
                "old-parchment-paper/parchment_alpha.png"
            ),
        )
        parchment_path = (
            ROOT / import_fe_theme_assets.UI_OLD_PARCHMENT_RELATIVE_PATH
        )
        if not parchment_path.is_file():
            self.skipTest("old parchment source is not present")

        source_width, source_height, source_rgba = (
            import_fe_theme_assets.read_old_parchment_rgba()
        )
        self.assertEqual((source_width, source_height), (640, 480))
        for x, y in (
            (0, 0),
            (source_width - 1, 0),
            (0, source_height - 1),
            (source_width - 1, source_height - 1),
        ):
            self.assertEqual(rgba_alpha_at(source_rgba, source_width, x, y), 0)
        center_offset = ((source_height // 2) * source_width + source_width // 2) * 4
        self.assertEqual(source_rgba[center_offset + 3], 255)

        width = import_fe_theme_assets.TOP_PREVIEW_WIDTH
        height = import_fe_theme_assets.PREVIEW_HEIGHT
        base_color = bytes((0x04, 0x05, 0x06))
        destination = bytearray(base_color * width * height)
        panel_x, panel_y, panel_width, panel_height = (
            import_fe_theme_assets.legend_dialogue_rect(width, height)
        )

        self.assertTrue(
            import_fe_theme_assets.draw_dialogue_panel_from_old_parchment(
                destination,
                width,
                height,
                panel_x,
                panel_y,
                panel_width,
                panel_height,
            )
        )
        for x, y in (
            (panel_x, panel_y),
            (panel_x + panel_width - 1, panel_y),
            (panel_x, panel_y + panel_height - 1),
            (panel_x + panel_width - 1, panel_y + panel_height - 1),
        ):
            self.assertEqual(pixel_at_bgr(destination, width, x, y), base_color)

        center_pixels = []
        for sample_y in (-12, -6, 0, 6, 12):
            for sample_x in (-20, -10, 0, 10, 20):
                center_pixels.append(
                    pixel_at_bgr(
                        destination,
                        width,
                        panel_x + panel_width // 2 + sample_x,
                        panel_y + panel_height // 2 + sample_y,
                    )
                )
        self.assertGreaterEqual(
            min(bgr_luminance(pixel) for pixel in center_pixels),
            180,
        )
        self.assertGreater(
            bgr_luminance(
                pixel_at_bgr(
                    destination,
                    width,
                    panel_x + panel_width // 2,
                    panel_y + panel_height // 2,
                )
            )
            - bgr_luminance(import_fe_theme_assets.UI_DIALOG_TEXT_BGR),
            150,
        )
        self.assertNotEqual(
            pixel_at_bgr(
                destination,
                width,
                panel_x + panel_width // 2,
                panel_y + panel_height // 2,
            ),
            base_color,
        )

    def test_downloaded_dialogue_reference_drives_panel_border(self):
        reference_path = (
            ROOT / import_fe_theme_assets.UI_DIALOG_REFERENCE_RELATIVE_PATH
        )
        if not reference_path.is_file():
            self.skipTest("downloaded FE dialogue reference is not present")

        source_width, source_height, source_pixels = (
            import_fe_theme_assets.read_dialogue_reference_panel_bgr()
        )
        _, _, panel_width, panel_height = (
            import_fe_theme_assets.UI_DIALOG_REFERENCE_PANEL_RECT
        )
        self.assertEqual((source_width, source_height), (panel_width, panel_height))

        destination = bytearray(
            bytes((0x01, 0x02, 0x03)) * source_width * source_height
        )
        self.assertTrue(
            import_fe_theme_assets.draw_dialogue_panel_from_reference(
                destination,
                source_width,
                source_height,
                0,
                0,
                source_width,
                source_height,
            )
        )

        left = import_fe_theme_assets.UI_DIALOG_REFERENCE_SLICE_LEFT
        top = import_fe_theme_assets.UI_DIALOG_REFERENCE_SLICE_TOP
        right = import_fe_theme_assets.UI_DIALOG_REFERENCE_SLICE_RIGHT
        bottom = import_fe_theme_assets.UI_DIALOG_REFERENCE_SLICE_BOTTOM
        self.assertEqual(
            pixel_at_bgr(destination, source_width, 0, 0),
            pixel_at_bgr(source_pixels, source_width, 0, 0),
        )
        self.assertEqual(
            pixel_at_bgr(destination, source_width, left + 2, 0),
            pixel_at_bgr(source_pixels, source_width, left + 2, 0),
        )
        self.assertEqual(
            pixel_at_bgr(destination, source_width, 0, top + 2),
            pixel_at_bgr(source_pixels, source_width, 0, top + 2),
        )
        self.assertEqual(
            pixel_at_bgr(destination, source_width, source_width - 1, 0),
            pixel_at_bgr(source_pixels, source_width, source_width - 1, 0),
        )
        for border_y in range(top):
            for border_x in range(source_width):
                self.assertEqual(
                    pixel_at_bgr(destination, source_width, border_x, border_y),
                    pixel_at_bgr(source_pixels, source_width, border_x, border_y),
                )
        for border_y in range(source_height - bottom, source_height):
            for border_x in range(source_width):
                self.assertEqual(
                    pixel_at_bgr(destination, source_width, border_x, border_y),
                    pixel_at_bgr(source_pixels, source_width, border_x, border_y),
                )
        for border_y in range(top, source_height - bottom):
            for border_x in range(left):
                self.assertEqual(
                    pixel_at_bgr(destination, source_width, border_x, border_y),
                    pixel_at_bgr(source_pixels, source_width, border_x, border_y),
                )
            for border_x in range(source_width - right, source_width):
                self.assertEqual(
                    pixel_at_bgr(destination, source_width, border_x, border_y),
                    pixel_at_bgr(source_pixels, source_width, border_x, border_y),
                )

    def test_fe8_font_map_renders_dialogue_text(self):
        output = bytearray(bytes((0xFF, 0xFF, 0xFF)) * 96 * 32)

        rendered = import_fe_theme_assets.draw_fe8_dialogue_text(
            output,
            96,
            32,
            None,
            4,
            6,
            "Anki?",
            import_fe_theme_assets.UI_DIALOG_TEXT_BGR,
        )

        self.assertTrue(rendered)
        self.assertIn(import_fe_theme_assets.UI_DIALOG_TEXT_BGR, output)
        self.assertIn(import_fe_theme_assets.FE8_FONT_TEXT_SHADE_BGR, output)

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

    def test_write_preview_sheet_uses_grid_dimensions(self):
        image_width = 5
        image_height = 3
        images = [
            bytes([index] * image_width * image_height * 3)
            for index in range(5)
        ]

        with tempfile.TemporaryDirectory() as temp_dir, mock.patch.object(
            import_fe_theme_assets,
            "write_png_copy",
        ):
            output = Path(temp_dir)

            import_fe_theme_assets.write_preview_sheet(
                output,
                "layers",
                images,
                image_width,
                image_height,
                3,
            )

            self.assertEqual(
                read_bmp_dimensions(output / "layers.bmp"),
                (
                    3 * image_width + 2 * import_fe_theme_assets.PREVIEW_GRID_GAP,
                    -(2 * image_height + import_fe_theme_assets.PREVIEW_GRID_GAP),
                ),
            )

    def test_write_theme_layer_framebuffers_outputs_layer_bins(self):
        top_background = bytes(
            [1]
            * import_fe_theme_assets.TOP_PREVIEW_WIDTH
            * import_fe_theme_assets.PREVIEW_HEIGHT
            * 3
        )
        top_legend = bytes(
            [5]
            * import_fe_theme_assets.TOP_PREVIEW_WIDTH
            * import_fe_theme_assets.PREVIEW_HEIGHT
            * 3
        )
        top_font = bytes(
            [2]
            * import_fe_theme_assets.TOP_PREVIEW_WIDTH
            * import_fe_theme_assets.PREVIEW_HEIGHT
            * 3
        )
        bottom_background = bytes(
            [3]
            * import_fe_theme_assets.BOTTOM_PREVIEW_WIDTH
            * import_fe_theme_assets.PREVIEW_HEIGHT
            * 3
        )
        bottom_legend = bytes(
            [6]
            * import_fe_theme_assets.BOTTOM_PREVIEW_WIDTH
            * import_fe_theme_assets.PREVIEW_HEIGHT
            * 3
        )
        bottom_font = bytes(
            [4]
            * import_fe_theme_assets.BOTTOM_PREVIEW_WIDTH
            * import_fe_theme_assets.PREVIEW_HEIGHT
            * 3
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            output = Path(temp_dir)

            import_fe_theme_assets.write_theme_layer_framebuffers(
                output,
                "sample",
                (
                    ("background", top_background),
                    ("legend", top_legend),
                    ("font", top_font),
                ),
                (
                    ("background", bottom_background),
                    ("legend", bottom_legend),
                    ("font", bottom_font),
                ),
            )

            top_background_bin = (
                output
                / "layers"
                / "fe_bg_sample_top_layer_background_400x240_bgr888_fb.bin"
            )
            top_legend_bin = (
                output
                / "layers"
                / "fe_bg_sample_top_layer_legend_400x240_bgr888_fb.bin"
            )
            top_font_bin = (
                output
                / "layers"
                / "fe_bg_sample_top_layer_font_400x240_bgr888_fb.bin"
            )
            bottom_background_bin = (
                output
                / "layers"
                / "fe_bg_sample_bottom_layer_background_320x240_bgr888_fb.bin"
            )
            bottom_legend_bin = (
                output
                / "layers"
                / "fe_bg_sample_bottom_layer_legend_320x240_bgr888_fb.bin"
            )
            bottom_font_bin = (
                output
                / "layers"
                / "fe_bg_sample_bottom_layer_font_320x240_bgr888_fb.bin"
            )

            self.assertEqual(
                top_background_bin.stat().st_size,
                import_fe_theme_assets.TOP_PREVIEW_WIDTH
                * import_fe_theme_assets.PREVIEW_HEIGHT
                * 3,
            )
            self.assertEqual(
                top_legend_bin.stat().st_size,
                top_background_bin.stat().st_size,
            )
            self.assertEqual(
                top_font_bin.stat().st_size,
                top_background_bin.stat().st_size,
            )
            self.assertEqual(
                bottom_background_bin.stat().st_size,
                import_fe_theme_assets.BOTTOM_PREVIEW_WIDTH
                * import_fe_theme_assets.PREVIEW_HEIGHT
                * 3,
            )
            self.assertEqual(
                bottom_legend_bin.stat().st_size,
                bottom_background_bin.stat().st_size,
            )
            self.assertEqual(
                bottom_font_bin.stat().st_size,
                bottom_background_bin.stat().st_size,
            )

    def test_build_font_atlas_uses_fallback_when_font_missing(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            with mock.patch.object(
                import_fe_theme_assets,
                "find_terminal_font_path",
                return_value=None,
            ), mock.patch.object(
                import_fe_theme_assets,
                "find_fe8_font_map_path",
                return_value=None,
            ), mock.patch.object(
                import_fe_theme_assets,
                "render_fe_font_text_bgr",
                return_value=None,
            ):
                atlas = import_fe_theme_assets.build_font_atlas(Path(temp_dir))

            glyph_size = (
                import_fe_theme_assets.UI_FONT_ATLAS_GLYPH_WIDTH
                * import_fe_theme_assets.UI_FONT_ATLAS_GLYPH_HEIGHT
            )
            self.assertEqual(
                len(atlas),
                import_fe_theme_assets.UI_FONT_ATLAS_CHAR_COUNT * glyph_size,
            )

            space_offset = import_fe_theme_assets.font_atlas_glyph_offset(" ")
            a_offset = import_fe_theme_assets.font_atlas_glyph_offset("A")
            question_offset = import_fe_theme_assets.font_atlas_glyph_offset("?")

            self.assertEqual(
                atlas[space_offset:space_offset + glyph_size],
                bytes(glyph_size),
            )
            self.assertIn(0xFF, atlas[a_offset:a_offset + glyph_size])
            self.assertIn(0xFF, atlas[question_offset:question_offset + glyph_size])

    def test_write_font_atlas_outputs_alpha_bin(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            output = Path(temp_dir)

            atlas_path = import_fe_theme_assets.write_font_atlas(output, output)

            self.assertEqual(
                atlas_path,
                output / import_fe_theme_assets.UI_FONT_ATLAS_FILENAME,
            )
            self.assertEqual(
                atlas_path.stat().st_size,
                import_fe_theme_assets.UI_FONT_ATLAS_CHAR_COUNT
                * import_fe_theme_assets.UI_FONT_ATLAS_GLYPH_WIDTH
                * import_fe_theme_assets.UI_FONT_ATLAS_GLYPH_HEIGHT,
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

    def test_embedded_forest_legend_pngs_match_generated_previews(self):
        expected_pairs = (
            (
                ROOT
                / "build/fe-theme-previews/forest_top_layer_legend_400x240.png",
                ROOT / "app-3ds/gfx/fe_forest_top_legend.png",
            ),
            (
                ROOT
                / "build/fe-theme-previews/forest_bottom_layer_legend_320x240.png",
                ROOT / "app-3ds/gfx/fe_forest_bottom_legend.png",
            ),
        )

        for generated, embedded in expected_pairs:
            with self.subTest(embedded=embedded.name):
                if not generated.is_file():
                    self.skipTest(f"generated preview missing: {generated}")
                self.assertTrue(embedded.is_file(), f"{embedded} is missing")
                self.assertEqual(
                    embedded.read_bytes(),
                    generated.read_bytes(),
                    (
                        f"{embedded} is stale; run "
                        "`make sync-app-fe-ui-assets`"
                    ),
                )

    def test_renderer_panels_match_generated_dialogue_rects(self):
        source = APP_RENDERER_C2D_PATH.read_text(encoding="utf-8")

        self.assertEqual(
            renderer_rect(source, "app_render_top_panel"),
            tuple(
                float(value)
                for value in import_fe_theme_assets.legend_dialogue_rect(
                    import_fe_theme_assets.TOP_PREVIEW_WIDTH,
                    import_fe_theme_assets.PREVIEW_HEIGHT,
                )
            ),
        )
        self.assertEqual(
            renderer_rect(source, "app_render_bottom_panel"),
            tuple(
                float(value)
                for value in import_fe_theme_assets.legend_dialogue_rect(
                    import_fe_theme_assets.BOTTOM_PREVIEW_WIDTH,
                    import_fe_theme_assets.PREVIEW_HEIGHT,
                )
            ),
        )


if __name__ == "__main__":
    unittest.main()
