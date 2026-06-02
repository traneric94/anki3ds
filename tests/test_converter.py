import io
import json
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest import mock

from converter.anki3ds_convert import (
    convert_lines,
    deck_id_is_valid,
    escape_tsv_field,
    main,
    normalize_text,
    write_deck,
)


class ConverterTests(unittest.TestCase):
    def test_deck_id_validation(self):
        self.assertTrue(deck_id_is_valid("my-deck_01"))
        self.assertFalse(deck_id_is_valid(""))
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


if __name__ == "__main__":
    unittest.main()
