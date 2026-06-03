import io
import json
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest import mock

from converter.anki3ds_convert import (
    DECK_INDEX_MAX_DECKS,
    DECK_MAX_CARDS,
    DECK_MAX_MEDIA_NAME_LENGTH,
    DECK_MAX_TEXT_LENGTH,
    MEDIA_IMAGE_MAX_HEIGHT,
    MEDIA_IMAGE_MAX_WIDTH,
    convert_lines,
    deck_id_is_valid,
    escape_tsv_field,
    load_ppm_rgb,
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

    def test_convert_lines_requires_media_field_for_front_image_tags(self):
        with self.assertRaisesRegex(ValueError, "front image tags"):
            convert_lines(
                ['front text <img src="front.ppm">\tback\ttag'],
                front_field=0,
                back_field=1,
                tags_field=2,
            )

        with self.assertRaisesRegex(ValueError, "front image tags"):
            convert_lines(
                ['<img src="front.ppm">\tback\ttag'],
                front_field=0,
                back_field=1,
                tags_field=2,
            )

    def test_convert_lines_requires_media_field_for_back_image_tags(self):
        with self.assertRaisesRegex(ValueError, "back image tags"):
            convert_lines(
                ['front\tback text <img src="back.ppm">\ttag'],
                front_field=0,
                back_field=1,
                tags_field=2,
            )

    def test_convert_lines_requires_non_empty_media_for_image_tags(self):
        with self.assertRaisesRegex(ValueError, "non-empty media field"):
            convert_lines(
                ['front text <img src="front.ppm">\tback\ttag\t'],
                front_field=0,
                back_field=1,
                tags_field=2,
                front_media_field=3,
            )

        with self.assertRaisesRegex(ValueError, "non-empty media field"):
            convert_lines(
                ['front\tback text <img src="back.ppm">\ttag\t'],
                front_field=0,
                back_field=1,
                tags_field=2,
                back_media_field=3,
            )

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

    def test_convert_lines_uses_source_card_id_for_reimport_identity(self):
        original = convert_lines(
            ["anki-card-1\tfront\tback"],
            front_field=1,
            back_field=2,
            tags_field=None,
            card_id_field=0,
        )
        edited = convert_lines(
            ["anki-card-1\tfront edited\tback"],
            front_field=1,
            back_field=2,
            tags_field=None,
            card_id_field=0,
        )

        self.assertEqual(original[0].card_id, edited[0].card_id)
        self.assertEqual(original[0].note_id, edited[0].note_id)
        self.assertNotEqual(original[0].front, edited[0].front)

    def test_convert_lines_uses_source_note_id_when_card_id_is_missing(self):
        original = convert_lines(
            ["anki-note-1\tfront\tback"],
            front_field=1,
            back_field=2,
            tags_field=None,
            note_id_field=0,
        )
        edited = convert_lines(
            ["anki-note-1\tfront edited\tback"],
            front_field=1,
            back_field=2,
            tags_field=None,
            note_id_field=0,
        )

        self.assertEqual(original[0].card_id, edited[0].card_id)
        self.assertEqual(original[0].note_id, edited[0].note_id)

    def test_convert_lines_rejects_empty_source_ids(self):
        with self.assertRaisesRegex(ValueError, "card id field is empty"):
            convert_lines(
                ["\tfront\tback"],
                front_field=1,
                back_field=2,
                tags_field=None,
                card_id_field=0,
            )

        with self.assertRaisesRegex(ValueError, "note id field is empty"):
            convert_lines(
                ["\tfront\tback"],
                front_field=1,
                back_field=2,
                tags_field=None,
                note_id_field=0,
            )

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
            review_log = output / "review-log.tsv"
            settings = output / "settings.tsv"
            state.write_text("existing-state\n", encoding="utf-8")
            review_log.write_text("existing-log\n", encoding="utf-8")
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
                review_log.read_text(encoding="utf-8"),
                "existing-log\n",
            )
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

    def test_write_deck_payload_commit_failure_rolls_back_existing_files(self):
        cards = convert_lines(["front\tback\ttag"], 0, 1, 2)

        with tempfile.TemporaryDirectory() as temp_dir:
            output = Path(temp_dir) / "sample"
            output.mkdir()
            old_deck_json = '{"format_version":1,"deck_id":"sample","card_count":9}\n'
            old_cards = "old-card\told-note\told front\told back\told\n"
            old_settings = "new_limit\t3\nreview_limit\t4\n"
            (output / "deck.json").write_text(old_deck_json, encoding="utf-8")
            (output / "cards.tsv").write_text(old_cards, encoding="utf-8")
            (output / "settings.tsv").write_text(old_settings, encoding="utf-8")
            original_replace = Path.replace

            def fail_cards_replace(source: Path, target: Path) -> Path:
                if source.name == "cards.tsv.tmp":
                    raise OSError("simulated cards replace failure")
                return original_replace(source, target)

            with self.assertRaisesRegex(OSError, "simulated cards replace failure"):
                with mock.patch.object(Path, "replace", fail_cards_replace):
                    write_deck(output, "sample", "Sample", cards)

            self.assertEqual(
                (output / "deck.json").read_text(encoding="utf-8"),
                old_deck_json,
            )
            self.assertEqual(
                (output / "cards.tsv").read_text(encoding="utf-8"),
                old_cards,
            )
            self.assertEqual(
                (output / "settings.tsv").read_text(encoding="utf-8"),
                old_settings,
            )
            self.assertFalse((output / "deck.json.tmp").exists())
            self.assertFalse((output / "cards.tsv.tmp").exists())
            self.assertFalse((output / "deck.json.bak").exists())
            self.assertFalse((output / "cards.tsv.bak").exists())

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
            media_dir = output / "media"
            media_content = b"A3I1\x01\x00\x01\x00\x00\xf8"
            media_dir.mkdir(parents=True)
            (media_dir / "front.a3i").write_bytes(media_content)

            write_deck(output, "sample", "Sample", cards)

            line = (output / "cards.tsv").read_text(encoding="utf-8").splitlines()[0]
            fields = line.split("\t")
            self.assertEqual(len(fields), 7)
            self.assertEqual(fields[5], "front.a3i")
            self.assertEqual(fields[6], "")
            self.assertEqual((media_dir / "front.a3i").read_bytes(), media_content)

    def test_write_deck_rejects_missing_passthrough_media(self):
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

            with self.assertRaisesRegex(ValueError, "passthrough media is missing"):
                write_deck(output, "sample", "Sample", cards)

            self.assertFalse((output / "deck.json").exists())
            self.assertFalse((output / "cards.tsv").exists())

    def test_write_deck_rejects_invalid_passthrough_media(self):
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
            media_dir = output / "media"
            media_dir.mkdir(parents=True)
            (media_dir / "front.a3i").write_bytes(b"A3I1\x02\x00\x01\x00\x00\xf8")

            with self.assertRaisesRegex(ValueError, "A3I pixel data"):
                write_deck(output, "sample", "Sample", cards)

            self.assertFalse((output / "deck.json").exists())

    def test_write_deck_rejects_unconverted_media_names(self):
        cards = convert_lines(
            ["front\tback\ttag\tfront.ppm\t"],
            0,
            1,
            2,
            front_media_field=3,
            back_media_field=4,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            output = Path(temp_dir) / "sample"

            with self.assertRaisesRegex(ValueError, "must be .a3i"):
                write_deck(output, "sample", "Sample", cards)

    def test_write_deck_rejects_missing_media_root(self):
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

            with self.assertRaisesRegex(ValueError, "media root must be"):
                write_deck(
                    root / "sample",
                    "sample",
                    "Sample",
                    cards,
                    media_root=root / "missing-media",
                )

    def test_write_deck_rejects_unsupported_media_extension(self):
        cards = convert_lines(
            ["front\tback\ttag\tfront.png\t"],
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

            with self.assertRaisesRegex(ValueError, "media must be .ppm or .a3i"):
                write_deck(output, "sample", "Sample", cards, media_root)

            self.assertFalse((output / "deck.json").exists())

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

    def test_write_deck_converts_inline_image_with_media_field(self):
        cards = convert_lines(
            ['front <img src="front.ppm">\tback\ttag\tfront.ppm'],
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
            (media_root / "front.ppm").write_bytes(
                b"P6\n1 1\n255\n" + bytes([255, 0, 0])
            )

            write_deck(output, "sample", "Sample", cards, media_root=media_root)

            line = (output / "cards.tsv").read_text(encoding="utf-8").splitlines()[0]
            self.assertEqual(line.split("\t")[2], "front")
            self.assertEqual(line.split("\t")[5], "front.a3i")
            self.assertTrue((output / "media" / "front.a3i").exists())

    def test_load_ppm_rgb_accepts_crlf_header(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "front.ppm"
            pixels = bytes([255, 0, 0, 0, 255, 0])
            source.write_bytes(b"P6\r\n2 1\r\n255\r\n" + pixels)

            width, height, loaded_pixels = load_ppm_rgb(source)

            self.assertEqual(width, 2)
            self.assertEqual(height, 1)
            self.assertEqual(loaded_pixels, pixels)

    def test_write_deck_media_failure_does_not_write_deck_files(self):
        cards = convert_lines(
            ["front\tback\ttag\tmissing.ppm\t"],
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

            with self.assertRaises(FileNotFoundError):
                write_deck(output, "sample", "Sample", cards, media_root=media_root)

            self.assertFalse((output / "deck.json").exists())
            self.assertFalse((output / "cards.tsv").exists())
            self.assertFalse((output / "settings.tsv").exists())

    def test_write_deck_media_failure_preserves_existing_media(self):
        cards = convert_lines(
            ["front\tback\ttag\tfront.ppm\tmissing.ppm"],
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
            media_dir = output / "media"
            old_media = b"A3I1\x01\x00\x01\x00\x00\xf8"
            media_root.mkdir()
            media_dir.mkdir(parents=True)
            (media_root / "front.ppm").write_bytes(
                b"P6\n1 1\n255\n" + bytes([0, 0, 255])
            )
            (media_dir / "front.a3i").write_bytes(old_media)
            (output / "cards.tsv").write_text("old cards\n", encoding="utf-8")

            with self.assertRaises(FileNotFoundError):
                write_deck(output, "sample", "Sample", cards, media_root=media_root)

            self.assertEqual((media_dir / "front.a3i").read_bytes(), old_media)
            self.assertEqual(
                (output / "cards.tsv").read_text(encoding="utf-8"),
                "old cards\n",
            )
            self.assertFalse((output / ".anki3ds-media.tmp").exists())

    def test_write_deck_media_commit_failure_rolls_back_existing_media(self):
        cards = convert_lines(
            ["front\tback\ttag\tone.ppm\ttwo.ppm"],
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
            media_dir = output / "media"
            old_one = b"A3I1\x01\x00\x01\x00\x00\xf8"
            old_two = b"A3I1\x01\x00\x01\x00\xe0\x07"
            original_replace = Path.replace

            media_root.mkdir()
            media_dir.mkdir(parents=True)
            (media_root / "one.ppm").write_bytes(
                b"P6\n1 1\n255\n" + bytes([0, 0, 255])
            )
            (media_root / "two.ppm").write_bytes(
                b"P6\n1 1\n255\n" + bytes([255, 0, 0])
            )
            (media_dir / "one.a3i").write_bytes(old_one)
            (media_dir / "two.a3i").write_bytes(old_two)
            (output / "cards.tsv").write_text("old cards\n", encoding="utf-8")

            def fail_second_temp_replace(source: Path, target: Path) -> Path:
                if source.name == "two.a3i" and source.parent.name == ".anki3ds-media.tmp":
                    raise OSError("simulated media replace failure")
                return original_replace(source, target)

            with self.assertRaisesRegex(OSError, "simulated media replace failure"):
                with mock.patch.object(Path, "replace", fail_second_temp_replace):
                    write_deck(
                        output,
                        "sample",
                        "Sample",
                        cards,
                        media_root=media_root,
                    )

            self.assertEqual((media_dir / "one.a3i").read_bytes(), old_one)
            self.assertEqual((media_dir / "two.a3i").read_bytes(), old_two)
            self.assertEqual(
                (output / "cards.tsv").read_text(encoding="utf-8"),
                "old cards\n",
            )
            self.assertFalse((output / ".anki3ds-media.tmp").exists())
            self.assertFalse((output / ".anki3ds-media.bak").exists())

    def test_write_deck_copies_existing_a3i_media(self):
        cards = convert_lines(
            ["front\tback\ttag\tfront.a3i\t"],
            0,
            1,
            2,
            front_media_field=3,
            back_media_field=4,
        )
        media_content = b"A3I1\x01\x00\x01\x00\x00\xf8"

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            media_root = root / "source-media"
            output = root / "sample"
            media_root.mkdir()
            (media_root / "front.a3i").write_bytes(media_content)

            write_deck(output, "sample", "Sample", cards, media_root=media_root)

            line = (output / "cards.tsv").read_text(encoding="utf-8").splitlines()[0]
            self.assertEqual(line.split("\t")[5], "front.a3i")
            self.assertEqual(
                (output / "media" / "front.a3i").read_bytes(),
                media_content,
            )

    def test_write_deck_rejects_invalid_a3i_media(self):
        cards = convert_lines(
            ["front\tback\ttag\tfront.a3i\t"],
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
            (media_root / "front.a3i").write_bytes(b"A3I1\x02\x00\x01\x00\x00\xf8")

            with self.assertRaisesRegex(ValueError, "A3I pixel data"):
                write_deck(output, "sample", "Sample", cards, media_root=media_root)

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

    def test_cli_accepts_source_id_fields(self):
        cards_input = "note-1\tcard-1\tfront\tback\n"

        with tempfile.TemporaryDirectory() as temp_dir:
            input_path = Path(temp_dir) / "export.tsv"
            output = Path(temp_dir) / "my-deck"
            input_path.write_text(cards_input, encoding="utf-8")

            with io.StringIO() as stdout:
                with redirect_stdout(stdout), mock.patch(
                    "sys.argv",
                    [
                        "anki3ds_convert.py",
                        str(input_path),
                        str(output),
                        "--note-id-field",
                        "0",
                        "--card-id-field",
                        "1",
                        "--front-field",
                        "2",
                        "--back-field",
                        "3",
                    ],
                ):
                    self.assertEqual(main(), 0)

            card_id, note_id, front, back, tags = (
                output / "cards.tsv"
            ).read_text(encoding="utf-8").splitlines()[0].split("\t")
            self.assertTrue(card_id.startswith("card-"))
            self.assertTrue(note_id.startswith("note-"))
            self.assertEqual(front, "front")
            self.assertEqual(back, "back")
            self.assertEqual(tags, "")

    def test_cli_reports_conversion_errors_without_traceback(self):
        cards_input = "\tback\n"

        with tempfile.TemporaryDirectory() as temp_dir:
            input_path = Path(temp_dir) / "export.tsv"
            output = Path(temp_dir) / "my-deck"
            input_path.write_text(cards_input, encoding="utf-8")

            stdout = io.StringIO()
            stderr = io.StringIO()
            with redirect_stdout(stdout), redirect_stderr(stderr), mock.patch(
                "sys.argv",
                ["anki3ds_convert.py", str(input_path), str(output)],
            ):
                self.assertEqual(main(), 1)

            self.assertEqual(stdout.getvalue(), "")
            self.assertIn("error: line 1: front field is empty", stderr.getvalue())
            self.assertNotIn("Traceback", stderr.getvalue())

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
            review_log = first_chunk / "review-log.tsv"
            settings = first_chunk / "settings.tsv"
            state.write_text("existing-state\n", encoding="utf-8")
            review_log.write_text("existing-log\n", encoding="utf-8")
            settings.write_text("new_limit\t3\nreview_limit\t4\n", encoding="utf-8")

            write_split_decks(root / "large", "large", "Large", cards)

            self.assertEqual(state.read_text(encoding="utf-8"), "existing-state\n")
            self.assertEqual(
                review_log.read_text(encoding="utf-8"),
                "existing-log\n",
            )
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

    def test_write_split_decks_rejects_more_chunks_than_device_deck_list(self):
        cards = convert_lines(
            [
                f"front {index}\tback {index}"
                for index in range(DECK_MAX_CARDS * DECK_INDEX_MAX_DECKS + 1)
            ],
            0,
            1,
            None,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            with self.assertRaisesRegex(ValueError, "more than 64 deck folders"):
                write_split_decks(
                    Path(temp_dir) / "huge",
                    "huge",
                    "Huge",
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
