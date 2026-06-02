import io
import json
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest import mock

from converter.anki3ds_convert import (
    DECK_MAX_CARDS,
    DECK_MAX_MEDIA_NAME_LENGTH,
    DECK_MAX_TEXT_LENGTH,
    MEDIA_IMAGE_MAX_HEIGHT,
    MEDIA_IMAGE_MAX_WIDTH,
    convert_lines,
    deck_id_is_valid,
    escape_tsv_field,
    main,
    normalize_text,
    resize_rgb_nearest,
    write_a3i_image,
    write_deck,
    write_split_decks,
)


class ConverterTests(unittest.TestCase):
    def test_deck_id_validation(self):
        self.assertTrue(deck_id_is_valid("my-deck_01"))
        self.assertTrue(deck_id_is_valid("a" * 63))
        self.assertFalse(deck_id_is_valid(""))
        self.assertFalse(deck_id_is_valid("a" * 64))
        self.assertFalse(deck_id_is_valid("My Deck"))
        self.assertFalse(deck_id_is_valid("bad/id"))

    def test_escape_tsv_field(self):
        self.assertEqual(escape_tsv_field("a\\b\tc\nd"), "a\\\\b\\tc\\nd")

    def test_normalize_text_simplifies_html(self):
        self.assertEqual(
            normalize_text("<b>front</b><br>line&nbsp;two"),
            "front\nline two",
        )
        self.assertEqual(
            normalize_text("<div>back <i>text</i></div><script>ignored()</script>"),
            "back text",
        )

    def test_convert_lines(self):
        cards = convert_lines(
            [
                "front one\tback one\ttag1 tag2",
                "front two\tback two\ttag3",
            ],
            front_field=0,
            back_field=1,
            tags_field=2,
        )

        self.assertEqual(len(cards), 2)
        self.assertTrue(cards[0].card_id.startswith("card-"))
        self.assertTrue(cards[0].note_id.startswith("note-"))
        self.assertEqual(cards[0].front, "front one")
        self.assertEqual(cards[0].back, "back one")
        self.assertEqual(cards[0].tags, "tag1 tag2")

    def test_convert_lines_simplifies_html_fields(self):
        cards = convert_lines(
            ["<p>front<br>line</p>\t<div>back&nbsp;<b>text</b></div>\ttag1 tag2"],
            front_field=0,
            back_field=1,
            tags_field=2,
        )

        self.assertEqual(cards[0].front, "front\nline")
        self.assertEqual(cards[0].back, "back text")

    def test_convert_lines_reads_media_fields(self):
        cards = convert_lines(
            ["front\tback\ttag\tfront.ppm\tback.ppm"],
            front_field=0,
            back_field=1,
            tags_field=2,
            front_media_field=3,
            back_media_field=4,
        )

        self.assertEqual(cards[0].front_media, "front.ppm")
        self.assertEqual(cards[0].back_media, "back.ppm")

    def test_convert_lines_rejects_media_paths(self):
        with self.assertRaisesRegex(ValueError, "plain filenames"):
            convert_lines(
                ["front\tback\ttag\tbad/path.ppm"],
                front_field=0,
                back_field=1,
                tags_field=2,
                front_media_field=3,
            )

    def test_convert_lines_disambiguates_duplicate_ids(self):
        cards = convert_lines(
            [
                "front\tback\ttag",
                "front\tback\ttag",
                "front\tback\ttag",
            ],
            front_field=0,
            back_field=1,
            tags_field=2,
        )

        self.assertEqual(cards[1].note_id, f"{cards[0].note_id}-2")
        self.assertEqual(cards[2].note_id, f"{cards[0].note_id}-3")
        self.assertEqual(cards[1].card_id, f"{cards[0].card_id}-2")
        self.assertEqual(cards[2].card_id, f"{cards[0].card_id}-3")

    def test_rejects_missing_or_empty_fields(self):
        with self.assertRaisesRegex(ValueError, "expected at least"):
            convert_lines(["front only"], front_field=0, back_field=1, tags_field=None)

        with self.assertRaisesRegex(ValueError, "front field is empty"):
            convert_lines(["\tback"], front_field=0, back_field=1, tags_field=None)

        with self.assertRaisesRegex(ValueError, "back field is empty"):
            convert_lines(["front\t"], front_field=0, back_field=1, tags_field=None)

    def test_write_deck_preserves_existing_state(self):
        cards = convert_lines(["front\tback\ttag"], 0, 1, 2)

        with tempfile.TemporaryDirectory() as temp_dir:
            output = Path(temp_dir) / "sample"
            output.mkdir()
            state = output / "state.tsv"
            settings = output / "settings.tsv"
            state.write_text("existing-state\n", encoding="utf-8")
            settings.write_text("new_limit\t1\nreview_limit\t2\n", encoding="utf-8")

            write_deck(output, "sample", "Sample", cards)

            deck_json = json.loads((output / "deck.json").read_text(encoding="utf-8"))
            self.assertEqual(deck_json["deck_id"], "sample")
            self.assertEqual(deck_json["card_count"], 1)
            self.assertTrue(
                (output / "cards.tsv").read_text(encoding="utf-8").startswith("card-")
            )
            self.assertEqual(state.read_text(encoding="utf-8"), "existing-state\n")
            self.assertEqual(
                settings.read_text(encoding="utf-8"),
                "new_limit\t1\nreview_limit\t2\n",
            )

    def test_write_deck_creates_default_settings(self):
        cards = convert_lines(["front\tback\ttag"], 0, 1, 2)

        with tempfile.TemporaryDirectory() as temp_dir:
            output = Path(temp_dir) / "sample"

            write_deck(output, "sample", "Sample", cards)

            self.assertEqual(
                (output / "settings.tsv").read_text(encoding="utf-8"),
                "new_limit\t20\nreview_limit\t200\n",
            )

    def test_write_deck_rejects_cards_beyond_device_limit(self):
        cards = convert_lines(
            [f"front {index}\tback {index}\ttag" for index in range(DECK_MAX_CARDS + 1)],
            0,
            1,
            2,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            output = Path(temp_dir) / "sample"

            with self.assertRaisesRegex(ValueError, f"more than {DECK_MAX_CARDS} cards"):
                write_deck(output, "sample", "Sample", cards)

    def test_write_deck_accepts_device_card_limit(self):
        cards = convert_lines(
            [f"front {index}\tback {index}\ttag" for index in range(DECK_MAX_CARDS)],
            0,
            1,
            2,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            output = Path(temp_dir) / "sample"

            write_deck(output, "sample", "Sample", cards)

            deck_json = json.loads((output / "deck.json").read_text(encoding="utf-8"))
            self.assertEqual(deck_json["card_count"], DECK_MAX_CARDS)

    def test_write_deck_rejects_empty_direct_card_list(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            output = Path(temp_dir) / "sample"

            with self.assertRaisesRegex(ValueError, "at least one card"):
                write_deck(output, "sample", "Sample", [])

    def test_write_deck_rejects_text_beyond_device_limit(self):
        cards = convert_lines(
            ["a" * DECK_MAX_TEXT_LENGTH + "\tback\ttag"],
            0,
            1,
            2,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            output = Path(temp_dir) / "sample"

            with self.assertRaisesRegex(ValueError, "front exceeds"):
                write_deck(output, "sample", "Sample", cards)

    def test_write_deck_validates_utf8_byte_lengths(self):
        valid_cards = convert_lines(
            ["é" * ((DECK_MAX_TEXT_LENGTH - 2) // 2) + "\tback\ttag"],
            0,
            1,
            2,
        )
        invalid_cards = convert_lines(
            ["é" * (DECK_MAX_TEXT_LENGTH // 2) + "\tback\ttag"],
            0,
            1,
            2,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)

            write_deck(root / "valid", "valid", "Valid", valid_cards)
            with self.assertRaisesRegex(ValueError, "UTF-8 bytes"):
                write_deck(root / "invalid", "invalid", "Invalid", invalid_cards)

    def test_write_deck_rejects_escaped_row_beyond_device_line_limit(self):
        cards = convert_lines(
            ["\\" * (DECK_MAX_TEXT_LENGTH - 1) + "\t" +
             "\\" * (DECK_MAX_TEXT_LENGTH - 1) + "\ttag"],
            0,
            1,
            2,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            output = Path(temp_dir) / "sample"

            with self.assertRaisesRegex(ValueError, "row exceeds"):
                write_deck(output, "sample", "Sample", cards)

    def test_convert_lines_rejects_media_name_beyond_device_limit(self):
        long_media_name = "a" * (DECK_MAX_MEDIA_NAME_LENGTH - len(".ppm")) + ".ppm"

        with self.assertRaisesRegex(ValueError, "under 96"):
            convert_lines(
                [f"front\tback\ttag\t{long_media_name}"],
                0,
                1,
                2,
                front_media_field=3,
            )

    def test_write_deck_writes_existing_media_names(self):
        cards = convert_lines(
            ["front\tback\ttag\tfront.a3i\t"],
            0,
            1,
            2,
            front_media_field=3,
            back_media_field=4,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            output = Path(temp_dir) / "sample"

            write_deck(output, "sample", "Sample", cards)

            line = (output / "cards.tsv").read_text(encoding="utf-8").splitlines()[0]
            fields = line.split("\t")
            self.assertEqual(len(fields), 7)
            self.assertEqual(fields[5], "front.a3i")
            self.assertEqual(fields[6], "")

    def test_write_deck_converts_ppm_media(self):
        cards = convert_lines(
            ["front\tback\ttag\tfront.ppm\t"],
            0,
            1,
            2,
            front_media_field=3,
            back_media_field=4,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            media_root = root / "source-media"
            output = root / "sample"
            media_root.mkdir()
            (media_root / "front.ppm").write_bytes(
                b"P6\n2 1\n255\n" +
                bytes([255, 0, 0, 0, 255, 0])
            )

            write_deck(output, "sample", "Sample", cards, media_root=media_root)

            line = (output / "cards.tsv").read_text(encoding="utf-8").splitlines()[0]
            self.assertEqual(line.split("\t")[5], "front.a3i")
            media = (output / "media" / "front.a3i").read_bytes()
            self.assertEqual(media[:8], b"A3I1\x02\x00\x01\x00")
            self.assertEqual(media[8:12], b"\x00\xf8\xe0\x07")

    def test_write_deck_rejects_media_output_collision(self):
        cards = convert_lines(
            [
                "front 1\tback 1\ttag\timage.ppm",
                "front 2\tback 2\ttag\timage.png",
            ],
            0,
            1,
            2,
            front_media_field=3,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            media_root = root / "source-media"
            output = root / "sample"
            media_root.mkdir()

            with self.assertRaisesRegex(ValueError, "collision"):
                write_deck(output, "sample", "Sample", cards, media_root=media_root)

    def test_write_a3i_image_writes_header_and_pixels(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            output = Path(temp_dir) / "image.a3i"

            write_a3i_image(output, 1, 1, bytes([0, 0, 255]))

            self.assertEqual(output.read_bytes(), b"A3I1\x01\x00\x01\x00\x1f\x00")

    def test_write_a3i_image_rejects_invalid_image_data(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            output = Path(temp_dir) / "image.a3i"

            with self.assertRaisesRegex(ValueError, "positive"):
                write_a3i_image(output, 0, 1, b"")

            with self.assertRaisesRegex(ValueError, "bounds"):
                write_a3i_image(
                    output,
                    MEDIA_IMAGE_MAX_WIDTH + 1,
                    1,
                    bytes((MEDIA_IMAGE_MAX_WIDTH + 1) * 3),
                )

            with self.assertRaisesRegex(ValueError, "length"):
                write_a3i_image(output, 1, 1, bytes([0, 0]))

    def test_resize_rgb_nearest_keeps_large_images_within_media_bounds(self):
        source_width = MEDIA_IMAGE_MAX_WIDTH * 2
        source_height = MEDIA_IMAGE_MAX_HEIGHT * 2
        source_pixels = bytes(source_width * source_height * 3)

        width, height, pixels = resize_rgb_nearest(
            source_width,
            source_height,
            source_pixels,
        )

        self.assertLessEqual(width, MEDIA_IMAGE_MAX_WIDTH)
        self.assertLessEqual(height, MEDIA_IMAGE_MAX_HEIGHT)
        self.assertEqual(len(pixels), width * height * 3)

    def test_write_deck_rejects_invalid_folder_id(self):
        cards = convert_lines(["front\tback"], 0, 1, None)

        with tempfile.TemporaryDirectory() as temp_dir:
            with self.assertRaisesRegex(ValueError, "folder name"):
                write_deck(Path(temp_dir) / "My Deck", "my-deck", "My Deck", cards)

            with self.assertRaisesRegex(ValueError, "deck id"):
                write_deck(Path(temp_dir) / "my-deck", "My Deck", "My Deck", cards)

            with self.assertRaisesRegex(ValueError, "match"):
                write_deck(Path(temp_dir) / "my-deck", "other-deck", "My Deck", cards)

    def test_cli_defaults_deck_id_to_output_folder(self):
        cards_input = "front\tback\n"

        with tempfile.TemporaryDirectory() as temp_dir:
            input_path = Path(temp_dir) / "export.tsv"
            output = Path(temp_dir) / "my-deck"
            input_path.write_text(cards_input, encoding="utf-8")

            with io.StringIO() as stdout:
                with redirect_stdout(stdout), mock.patch(
                    "sys.argv",
                    ["anki3ds_convert.py", str(input_path), str(output)],
                ):
                    self.assertEqual(main(), 0)

            deck_json = json.loads((output / "deck.json").read_text(encoding="utf-8"))
            self.assertEqual(deck_json["deck_id"], "my-deck")

    def test_write_split_decks_writes_numbered_sibling_decks(self):
        cards = convert_lines(
            [f"front {index}\tback {index}" for index in range(DECK_MAX_CARDS + 1)],
            0,
            1,
            None,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            output = root / "large"

            written_paths = write_split_decks(output, "large", "Large", cards)

            self.assertEqual(written_paths, [root / "large-01", root / "large-02"])
            self.assertFalse(output.exists())

            first_json = json.loads(
                (root / "large-01" / "deck.json").read_text(encoding="utf-8")
            )
            second_json = json.loads(
                (root / "large-02" / "deck.json").read_text(encoding="utf-8")
            )
            self.assertEqual(first_json["deck_id"], "large-01")
            self.assertEqual(first_json["name"], "Large 1/2")
            self.assertEqual(first_json["card_count"], DECK_MAX_CARDS)
            self.assertEqual(second_json["deck_id"], "large-02")
            self.assertEqual(second_json["name"], "Large 2/2")
            self.assertEqual(second_json["card_count"], 1)

    def test_write_split_decks_preserves_existing_chunk_state_and_settings(self):
        cards = convert_lines(
            [f"front {index}\tback {index}" for index in range(DECK_MAX_CARDS + 1)],
            0,
            1,
            None,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            first_chunk = root / "large-01"
            first_chunk.mkdir()
            state = first_chunk / "state.tsv"
            settings = first_chunk / "settings.tsv"
            state.write_text("existing-state\n", encoding="utf-8")
            settings.write_text("new_limit\t3\nreview_limit\t4\n", encoding="utf-8")

            write_split_decks(root / "large", "large", "Large", cards)

            self.assertEqual(state.read_text(encoding="utf-8"), "existing-state\n")
            self.assertEqual(
                settings.read_text(encoding="utf-8"),
                "new_limit\t3\nreview_limit\t4\n",
            )

    def test_write_split_decks_rejects_too_long_chunk_ids(self):
        cards = convert_lines(
            [f"front {index}\tback {index}" for index in range(DECK_MAX_CARDS + 1)],
            0,
            1,
            None,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            deck_id = "a" * 62

            with self.assertRaisesRegex(ValueError, "too long"):
                write_split_decks(
                    Path(temp_dir) / deck_id,
                    deck_id,
                    "Large",
                    cards,
                )

    def test_cli_split_large_decks(self):
        cards_input = "\n".join(
            f"front {index}\tback {index}" for index in range(DECK_MAX_CARDS + 1)
        ) + "\n"

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            input_path = root / "export.tsv"
            output = root / "large"
            input_path.write_text(cards_input, encoding="utf-8")

            stdout = io.StringIO()
            with stdout:
                with redirect_stdout(stdout), mock.patch(
                    "sys.argv",
                    [
                        "anki3ds_convert.py",
                        str(input_path),
                        str(output),
                        "--split-large-decks",
                    ],
                ):
                    self.assertEqual(main(), 0)
                output_text = stdout.getvalue()

            self.assertIn("2 deck folder", output_text)
            self.assertTrue((root / "large-01" / "cards.tsv").exists())
            self.assertTrue((root / "large-02" / "cards.tsv").exists())


if __name__ == "__main__":
    unittest.main()
