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
    DECK_MAX_NAME_LENGTH,
    DECK_MAX_TEXT_LENGTH,
    convert_lines,
    deck_id_is_valid,
    escape_tsv_field,
    main,
    normalize_text,
    write_deck,
    write_split_decks,
)


def review_log_row(card_id: str, timestamp: int) -> str:
    return (
        f"{timestamp}\t20000\trating\t{card_id}\tgood\t"
        "0\t0\t0\t20000\t0\t2500\t0\t0\t"
        "1\t20000\t20000\t20001\t1\t2500\t0\t0\n"
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

    def test_convert_lines_rejects_image_tags(self):
        with self.assertRaisesRegex(ValueError, "cannot include image tags"):
            convert_lines(
                ['front text <img src="front.ppm">\tback\ttag'],
                front_field=0,
                back_field=1,
                tags_field=2,
            )

        with self.assertRaisesRegex(ValueError, "cannot include image tags"):
            convert_lines(
                ['front\tback text <img src="back.ppm">\ttag'],
                front_field=0,
                back_field=1,
                tags_field=2,
            )

    def test_convert_lines_rejects_media_fields(self):
        with self.assertRaisesRegex(ValueError, "cannot use media fields"):
            convert_lines(
                ["front\tback\ttag\tfront.a3i"],
                front_field=0,
                back_field=1,
                tags_field=2,
                front_media_field=3,
                text_only=True,
            )

        with self.assertRaisesRegex(ValueError, "cannot use media fields"):
            convert_lines(
                ["front\tback\ttag\tback.a3i"],
                front_field=0,
                back_field=1,
                tags_field=2,
                back_media_field=3,
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

    def test_write_deck_removes_stale_media_without_touching_progress(self):
        cards = convert_lines(["front\tback\ttag"], 0, 1, 2)

        with tempfile.TemporaryDirectory() as temp_dir:
            output = Path(temp_dir) / "sample"
            output.mkdir()
            media = output / "media"
            media.mkdir()
            (media / "old.bin").write_bytes(b"old media")
            state = output / "state.tsv"
            review_log = output / "review-log.tsv"
            settings = output / "settings.tsv"
            state.write_text("existing-state\n", encoding="utf-8")
            review_log.write_text("existing-log\n", encoding="utf-8")
            settings.write_text("new_limit\t5\nreview_limit\t6\n", encoding="utf-8")

            write_deck(output, "sample", "Sample", cards)

            self.assertFalse(media.exists())
            self.assertEqual(state.read_text(encoding="utf-8"), "existing-state\n")
            self.assertEqual(
                review_log.read_text(encoding="utf-8"),
                "existing-log\n",
            )
            self.assertEqual(
                settings.read_text(encoding="utf-8"),
                "new_limit\t5\nreview_limit\t6\n",
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

    def test_write_deck_rejects_unloadable_display_name(self):
        cards = convert_lines(["front\tback\ttag"], 0, 1, 2)

        with tempfile.TemporaryDirectory() as temp_dir:
            output = Path(temp_dir) / "sample"

            with self.assertRaisesRegex(ValueError, "deck name is required"):
                write_deck(output, "sample", "", cards)
            with self.assertRaisesRegex(ValueError, "control characters"):
                write_deck(output, "sample", "Bad\nName", cards)
            with self.assertRaisesRegex(ValueError, "deck name exceeds"):
                write_deck(output, "sample", "a" * DECK_MAX_NAME_LENGTH, cards)

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

    def test_write_deck_rejects_media_root(self):
        cards = convert_lines(["front\tback\ttag"], 0, 1, 2)

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            media_root = root / "source-media"
            media_root.mkdir()

            with self.assertRaisesRegex(ValueError, "cannot use media options"):
                write_deck(root / "sample", "sample", "Sample", cards, media_root)

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

    def test_cli_text_only_writes_plain_text_deck(self):
        cards_input = "front\tback\ttag\n"

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
                        "--text-only",
                    ],
                ):
                    self.assertEqual(main(), 0)

            line = (output / "cards.tsv").read_text(encoding="utf-8").splitlines()[0]
            self.assertEqual(len(line.split("\t")), 5)
            self.assertFalse((output / "media").exists())

    def test_cli_text_only_rejects_media_options(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            input_path = Path(temp_dir) / "export.tsv"
            output = Path(temp_dir) / "my-deck"
            input_path.write_text("front\tback\ttag\tfront.a3i\n", encoding="utf-8")

            with mock.patch(
                "sys.argv",
                [
                    "anki3ds_convert.py",
                    str(input_path),
                    str(output),
                    "--text-only",
                    "--front-media-field",
                    "3",
                ],
            ):
                with self.assertRaisesRegex(SystemExit, "text flash-card decks"):
                    main()

    def test_cli_text_only_reports_image_tags_without_traceback(self):
        cards_input = 'front <img src="front.ppm">\tback\n'

        with tempfile.TemporaryDirectory() as temp_dir:
            input_path = Path(temp_dir) / "export.tsv"
            output = Path(temp_dir) / "my-deck"
            input_path.write_text(cards_input, encoding="utf-8")

            stdout = io.StringIO()
            stderr = io.StringIO()
            with redirect_stdout(stdout), redirect_stderr(stderr), mock.patch(
                "sys.argv",
                [
                    "anki3ds_convert.py",
                    str(input_path),
                    str(output),
                    "--text-only",
                ],
            ):
                self.assertEqual(main(), 1)

            self.assertEqual(stdout.getvalue(), "")
            self.assertIn(
                "text flash-card decks cannot include image tags",
                stderr.getvalue(),
            )
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

    def test_write_split_decks_removes_stale_converter_chunks_after_shrink(self):
        old_cards = convert_lines(
            [
                f"old front {index}\told back {index}"
                for index in range(DECK_MAX_CARDS * 2 + 1)
            ],
            0,
            1,
            None,
        )
        new_cards = convert_lines(
            [
                f"new front {index}\tnew back {index}"
                for index in range(DECK_MAX_CARDS + 1)
            ],
            0,
            1,
            None,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            output = root / "large"
            write_split_decks(output, "large", "Large", old_cards)
            stale_state = root / "large-02" / "state.tsv"
            stale_state.write_text("existing-state\n", encoding="utf-8")
            self.assertTrue((root / "large-03").exists())

            written_paths = write_split_decks(output, "large", "Large", new_cards)

            self.assertEqual(written_paths, [root / "large-01", root / "large-02"])
            self.assertTrue((root / "large-01").exists())
            self.assertTrue((root / "large-02").exists())
            self.assertFalse((root / "large-03").exists())
            self.assertFalse(stale_state.exists())

    def test_write_split_decks_keeps_matching_state_after_shrink(self):
        old_cards = convert_lines(
            [
                f"source-{index}\tnote-{index}\told front {index}\told back {index}"
                for index in range(DECK_MAX_CARDS * 2 + 1)
            ],
            2,
            3,
            None,
            card_id_field=0,
            note_id_field=1,
        )
        new_cards = convert_lines(
            [
                f"fresh-{index}\tfresh-note-{index}\tfront {index}\tback {index}"
                for index in range(DECK_MAX_CARDS)
            ] + [
                (
                    f"source-{DECK_MAX_CARDS}\tnote-{DECK_MAX_CARDS}"
                    f"\tnew front {DECK_MAX_CARDS}\tnew back {DECK_MAX_CARDS}"
                )
            ],
            2,
            3,
            None,
            card_id_field=0,
            note_id_field=1,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            output = root / "large"
            matching_state = (
                "#anki3ds-state-v1\t1\n"
                f"{old_cards[DECK_MAX_CARDS].card_id}\t1\t3\t20004\t4\t2500\t0\t0\t20000\t20000\n"
                "#anki3ds-state-complete\t1\n"
            )
            write_split_decks(output, "large", "Large", old_cards)
            state = root / "large-02" / "state.tsv"
            state.write_text(matching_state, encoding="utf-8")
            self.assertTrue((root / "large-03").exists())

            written_paths = write_split_decks(output, "large", "Large", new_cards)

            self.assertEqual(written_paths, [root / "large-01", root / "large-02"])
            self.assertFalse((root / "large-03").exists())
            self.assertEqual(
                state.read_text(encoding="utf-8"),
                matching_state,
            )

    def test_write_split_decks_removes_stale_chunks_when_deck_becomes_single(self):
        old_cards = convert_lines(
            [
                f"old front {index}\told back {index}"
                for index in range(DECK_MAX_CARDS + 1)
            ],
            0,
            1,
            None,
        )
        new_cards = convert_lines(
            [f"new front {index}\tnew back {index}" for index in range(10)],
            0,
            1,
            None,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            output = root / "large"
            write_split_decks(output, "large", "Large", old_cards)
            self.assertTrue((root / "large-01").exists())
            self.assertTrue((root / "large-02").exists())

            written_paths = write_split_decks(output, "large", "Large", new_cards)

            self.assertEqual(written_paths, [output])
            self.assertTrue(output.exists())
            self.assertFalse((root / "large-01").exists())
            self.assertFalse((root / "large-02").exists())

            deck_json = json.loads((output / "deck.json").read_text(encoding="utf-8"))
            self.assertEqual(deck_json["deck_id"], "large")
            self.assertEqual(deck_json["card_count"], 10)

    def test_write_split_decks_migrates_split_progress_when_deck_becomes_single(self):
        old_cards = convert_lines(
            [
                f"source-{index}\tnote-{index}\told front {index}\told back {index}"
                for index in range(DECK_MAX_CARDS + 1)
            ],
            2,
            3,
            None,
            card_id_field=0,
            note_id_field=1,
        )
        new_cards = convert_lines(
            [
                "source-0\tnote-0\tnew front 0\tnew back 0",
                (
                    f"source-{DECK_MAX_CARDS}\tnote-{DECK_MAX_CARDS}"
                    f"\tnew front {DECK_MAX_CARDS}\tnew back {DECK_MAX_CARDS}"
                ),
            ] + [
                f"new-source-{index}\tnew-note-{index}\tfront {index}\tback {index}"
                for index in range(8)
            ],
            2,
            3,
            None,
            card_id_field=0,
            note_id_field=1,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            output = root / "large"
            write_split_decks(output, "large", "Large", old_cards)
            first_chunk = root / "large-01"
            second_chunk = root / "large-02"
            (first_chunk / "state.tsv").write_text(
                (
                    "#anki3ds-state-v1\t1\n"
                    f"{old_cards[0].card_id}\t1\t2\t20001\t1\t2500\t0\t0\t20000\t20000\n"
                    "#anki3ds-state-complete\t1\n"
                ),
                encoding="utf-8",
            )
            (second_chunk / "state.tsv").write_text(
                (
                    "#anki3ds-state-v1\t1\n"
                    f"{old_cards[DECK_MAX_CARDS].card_id}\t1\t3\t20004\t4\t2500\t0\t0\t20000\t20000\n"
                    "#anki3ds-state-complete\t1\n"
                ),
                encoding="utf-8",
            )
            (first_chunk / "review-log.tsv").write_text(
                review_log_row(old_cards[0].card_id, 1),
                encoding="utf-8",
            )
            (second_chunk / "review-log.tsv").write_text(
                review_log_row(old_cards[DECK_MAX_CARDS].card_id, 2),
                encoding="utf-8",
            )
            (first_chunk / "settings.tsv").write_text(
                "new_limit\t3\nreview_limit\t4\n",
                encoding="utf-8",
            )

            written_paths = write_split_decks(output, "large", "Large", new_cards)

            self.assertEqual(written_paths, [output])
            self.assertTrue(output.exists())
            self.assertFalse(first_chunk.exists())
            self.assertFalse(second_chunk.exists())
            self.assertEqual(
                (output / "state.tsv").read_text(encoding="utf-8"),
                (
                    "#anki3ds-state-v1\t2\n"
                    f"{old_cards[0].card_id}\t1\t2\t20001\t1\t2500\t0\t0\t20000\t20000\n"
                    f"{old_cards[DECK_MAX_CARDS].card_id}\t1\t3\t20004\t4\t2500\t0\t0\t20000\t20000\n"
                    "#anki3ds-state-complete\t2\n"
                ),
            )
            self.assertEqual(
                (output / "review-log.tsv").read_text(encoding="utf-8"),
                (
                    review_log_row(old_cards[0].card_id, 1)
                    + review_log_row(old_cards[DECK_MAX_CARDS].card_id, 2)
                ),
            )
            self.assertEqual(
                (output / "settings.tsv").read_text(encoding="utf-8"),
                "new_limit\t3\nreview_limit\t4\n",
            )

    def test_write_split_decks_migrates_progress_when_chunk_boundaries_shift(self):
        old_cards = convert_lines(
            [
                f"source-{index}\tnote-{index}\told front {index}\told back {index}"
                for index in range(DECK_MAX_CARDS + 1)
            ],
            2,
            3,
            None,
            card_id_field=0,
            note_id_field=1,
        )
        new_cards = convert_lines(
            [
                "new-front\tnote-new\tinserted front\tinserted back",
            ] + [
                f"source-{index}\tnote-{index}\tnew front {index}\tnew back {index}"
                for index in range(DECK_MAX_CARDS + 1)
            ],
            2,
            3,
            None,
            card_id_field=0,
            note_id_field=1,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            output = root / "large"
            write_split_decks(output, "large", "Large", old_cards)
            first_chunk = root / "large-01"
            second_chunk = root / "large-02"
            (first_chunk / "state.tsv").write_text(
                (
                    "#anki3ds-state-v1\t2\n"
                    f"{old_cards[0].card_id}\t1\t2\t20001\t1\t2500\t0\t0\t20000\t20000\n"
                    f"{old_cards[255].card_id}\t1\t1\t20002\t2\t2400\t0\t0\t20000\t20000\n"
                    "#anki3ds-state-complete\t2\n"
                ),
                encoding="utf-8",
            )
            (first_chunk / "review-log.tsv").write_text(
                (
                    review_log_row(old_cards[0].card_id, 1)
                    + review_log_row(old_cards[255].card_id, 2)
                ),
                encoding="utf-8",
            )
            (second_chunk / "state.tsv").write_text(
                (
                    "#anki3ds-state-v1\t1\n"
                    f"{old_cards[256].card_id}\t1\t3\t20004\t4\t2500\t0\t0\t20000\t20000\n"
                    "#anki3ds-state-complete\t1\n"
                ),
                encoding="utf-8",
            )
            (second_chunk / "review-log.tsv").write_text(
                review_log_row(old_cards[256].card_id, 3),
                encoding="utf-8",
            )

            written_paths = write_split_decks(output, "large", "Large", new_cards)

            self.assertEqual(written_paths, [first_chunk, second_chunk])
            self.assertEqual(
                (first_chunk / "state.tsv").read_text(encoding="utf-8"),
                (
                    "#anki3ds-state-v1\t1\n"
                    f"{old_cards[0].card_id}\t1\t2\t20001\t1\t2500\t0\t0\t20000\t20000\n"
                    "#anki3ds-state-complete\t1\n"
                ),
            )
            self.assertEqual(
                (first_chunk / "review-log.tsv").read_text(encoding="utf-8"),
                review_log_row(old_cards[0].card_id, 1),
            )
            self.assertEqual(
                (second_chunk / "state.tsv").read_text(encoding="utf-8"),
                (
                    "#anki3ds-state-v1\t2\n"
                    f"{old_cards[255].card_id}\t1\t1\t20002\t2\t2400\t0\t0\t20000\t20000\n"
                    f"{old_cards[256].card_id}\t1\t3\t20004\t4\t2500\t0\t0\t20000\t20000\n"
                    "#anki3ds-state-complete\t2\n"
                ),
            )
            self.assertEqual(
                (second_chunk / "review-log.tsv").read_text(encoding="utf-8"),
                (
                    review_log_row(old_cards[255].card_id, 2)
                    + review_log_row(old_cards[256].card_id, 3)
                ),
            )

    def test_write_split_decks_removes_stale_single_when_deck_becomes_split(self):
        old_cards = convert_lines(
            [f"old front {index}\told back {index}" for index in range(10)],
            0,
            1,
            None,
        )
        new_cards = convert_lines(
            [
                f"new front {index}\tnew back {index}"
                for index in range(DECK_MAX_CARDS + 1)
            ],
            0,
            1,
            None,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            output = root / "large"
            write_split_decks(output, "large", "Large", old_cards)
            self.assertTrue(output.exists())

            written_paths = write_split_decks(output, "large", "Large", new_cards)

            self.assertEqual(written_paths, [root / "large-01", root / "large-02"])
            self.assertFalse(output.exists())
            self.assertTrue((root / "large-01").exists())
            self.assertTrue((root / "large-02").exists())

    def test_write_split_decks_migrates_single_progress_when_deck_becomes_split(self):
        old_cards = convert_lines(
            [
                f"source-{index}\tnote-{index}\told front {index}\told back {index}"
                for index in range(2)
            ],
            2,
            3,
            None,
            card_id_field=0,
            note_id_field=1,
        )
        new_cards = convert_lines(
            [
                (
                    f"source-{index}\tnote-{index}\tnew front {index}"
                    f"\tnew back {index}"
                )
                for index in range(2)
            ] + [
                f"new-source-{index}\tnew-note-{index}\tfront {index}\tback {index}"
                for index in range(DECK_MAX_CARDS - 1)
            ],
            2,
            3,
            None,
            card_id_field=0,
            note_id_field=1,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            output = root / "large"
            write_split_decks(output, "large", "Large", old_cards)
            (output / "state.tsv").write_text(
                (
                    "#anki3ds-state-v1\t2\n"
                    f"{old_cards[0].card_id}\t1\t2\t20001\t1\t2500\t0\t0\t20000\t20000\n"
                    f"{old_cards[1].card_id}\t1\t3\t20004\t4\t2500\t0\t0\t20000\t20000\n"
                    "#anki3ds-state-complete\t2\n"
                ),
                encoding="utf-8",
            )
            (output / "review-log.tsv").write_text(
                (
                    review_log_row(old_cards[0].card_id, 1)
                    + review_log_row(old_cards[1].card_id, 2)
                    + review_log_row("missing-card", 3)
                    + review_log_row(old_cards[0].card_id, 4).rstrip("\n")
                ),
                encoding="utf-8",
            )
            (output / "settings.tsv").write_text(
                "new_limit\t3\nreview_limit\t4\n",
                encoding="utf-8",
            )

            written_paths = write_split_decks(output, "large", "Large", new_cards)

            first_chunk = root / "large-01"
            second_chunk = root / "large-02"
            self.assertEqual(written_paths, [first_chunk, second_chunk])
            self.assertFalse(output.exists())
            self.assertEqual(
                (first_chunk / "state.tsv").read_text(encoding="utf-8"),
                (
                    "#anki3ds-state-v1\t2\n"
                    f"{old_cards[0].card_id}\t1\t2\t20001\t1\t2500\t0\t0\t20000\t20000\n"
                    f"{old_cards[1].card_id}\t1\t3\t20004\t4\t2500\t0\t0\t20000\t20000\n"
                    "#anki3ds-state-complete\t2\n"
                ),
            )
            self.assertFalse((second_chunk / "state.tsv").exists())
            self.assertEqual(
                (first_chunk / "review-log.tsv").read_text(encoding="utf-8"),
                (
                    review_log_row(old_cards[0].card_id, 1)
                    + review_log_row(old_cards[1].card_id, 2)
                ),
            )
            self.assertFalse((second_chunk / "review-log.tsv").exists())
            self.assertEqual(
                (first_chunk / "settings.tsv").read_text(encoding="utf-8"),
                "new_limit\t3\nreview_limit\t4\n",
            )
            self.assertEqual(
                (second_chunk / "settings.tsv").read_text(encoding="utf-8"),
                "new_limit\t3\nreview_limit\t4\n",
            )

    def test_write_split_decks_keeps_non_converter_matching_sibling(self):
        cards = convert_lines(
            [f"front {index}\tback {index}" for index in range(DECK_MAX_CARDS + 1)],
            0,
            1,
            None,
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            sibling = root / "large-99"
            sibling.mkdir()
            (sibling / "deck.json").write_text(
                json.dumps(
                    {
                        "format_version": 1,
                        "deck_id": "large-99",
                        "created_by": "someone-else",
                    }
                ),
                encoding="utf-8",
            )
            (sibling / "cards.tsv").write_text("custom\n", encoding="utf-8")

            write_split_decks(root / "large", "large", "Large", cards)

            self.assertTrue(sibling.exists())
            self.assertEqual(
                (sibling / "cards.tsv").read_text(encoding="utf-8"),
                "custom\n",
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
