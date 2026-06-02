import json
import tempfile
import unittest
from pathlib import Path

from converter.anki3ds_convert import convert_lines, escape_tsv_field, write_deck


class ConverterTests(unittest.TestCase):
    def test_escape_tsv_field(self):
        self.assertEqual(escape_tsv_field("a\\b\tc\nd"), "a\\\\b\\tc\\nd")

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
            state.write_text("existing-state\n", encoding="utf-8")

            write_deck(output, "sample", "Sample", cards)

            deck_json = json.loads((output / "deck.json").read_text(encoding="utf-8"))
            self.assertEqual(deck_json["deck_id"], "sample")
            self.assertEqual(deck_json["card_count"], 1)
            self.assertTrue((output / "cards.tsv").read_text(encoding="utf-8").startswith("card-"))
            self.assertEqual(state.read_text(encoding="utf-8"), "existing-state\n")


if __name__ == "__main__":
    unittest.main()
