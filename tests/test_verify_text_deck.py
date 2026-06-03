import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VERIFY_TEXT_DECK_PATH = ROOT / "tools" / "verify_text_deck.py"
VERIFY_TEXT_DECK_SPEC = importlib.util.spec_from_file_location(
    "verify_text_deck",
    VERIFY_TEXT_DECK_PATH,
)
verify_text_deck = importlib.util.module_from_spec(VERIFY_TEXT_DECK_SPEC)
assert VERIFY_TEXT_DECK_SPEC.loader is not None
VERIFY_TEXT_DECK_SPEC.loader.exec_module(verify_text_deck)


class VerifyTextDeckTests(unittest.TestCase):
    def write_deck(
        self,
        root: Path,
        deck_id: str = "sample",
        cards: str = "card-1\tnote-1\tfront\tback\ttag\n",
        card_count: int = 1,
        name: str = "Sample",
    ) -> Path:
        deck_dir = root / deck_id
        deck_dir.mkdir()
        (deck_dir / "deck.json").write_text(
            (
                "{"
                '"format_version":1,'
                f'"deck_id":"{deck_id}",'
                f'"name":"{name}",'
                f'"card_count":{card_count}'
                "}\n"
            ),
            encoding="utf-8",
        )
        (deck_dir / "cards.tsv").write_text(cards, encoding="utf-8")
        (deck_dir / "settings.tsv").write_text(
            "new_limit\t20\nreview_limit\t200\n",
            encoding="utf-8",
        )
        return deck_dir

    def verify(self, deck_dir: Path) -> list[str]:
        return verify_text_deck.verify_text_deck(deck_dir)

    def assert_error_contains(self, errors: list[str], expected: str) -> None:
        self.assertTrue(
            any(expected in error for error in errors),
            f"{expected!r} not found in {errors!r}",
        )

    def test_accepts_valid_text_deck(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            deck_dir = self.write_deck(
                Path(temp_dir),
                cards="card-1\tnote-1\tfront\\nline\tback\\ttab\ttag\n",
            )

            self.assertEqual(self.verify(deck_dir), [])

    def test_accepts_settings_comments_and_blank_lines(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            deck_dir = self.write_deck(Path(temp_dir))
            (deck_dir / "settings.tsv").write_text(
                "# daily limits\n\nnew_limit\t20\nreview_limit\t200\n# end\n",
                encoding="utf-8",
            )

            self.assertEqual(self.verify(deck_dir), [])

    def test_rejects_empty_deck(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            deck_dir = self.write_deck(Path(temp_dir), cards="", card_count=0)

            self.assert_error_contains(
                self.verify(deck_dir),
                "must contain at least one card",
            )

    def test_rejects_app_incompatible_card_rows(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            cards = (
                "#bad\tnote-1\tfront\tback\ttag\n"
                "card-2\tnote-2\t\tback\ttag\n"
                "card-3\tnote-3\tfront\t\ttag\n"
                "card-4\tnote-4\tbad\\qescape\tback\ttag\n"
                f"card-5\tnote-5\t{'a' * 384}\tback\ttag\n"
            )
            deck_dir = self.write_deck(Path(temp_dir), cards=cards, card_count=5)
            errors = self.verify(deck_dir)

            self.assert_error_contains(errors, "card_id is invalid")
            self.assert_error_contains(errors, "front is required")
            self.assert_error_contains(errors, "back is required")
            self.assert_error_contains(errors, "front has a bad escape")
            self.assert_error_contains(errors, "front exceeds 383 UTF-8 bytes")

    def test_rejects_bad_deck_metadata(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            deck_dir = self.write_deck(Path(temp_dir), deck_id="Bad Deck")
            errors = self.verify(deck_dir)

            self.assert_error_contains(errors, "deck folder id is invalid")

            (deck_dir / "deck.json").write_text(
                '{"format_version":1,"deck_id":"sample","name":"","card_count":"1"}\n',
                encoding="utf-8",
            )
            errors = self.verify(deck_dir)

            self.assert_error_contains(errors, "deck_id must match folder name")
            self.assert_error_contains(errors, "name is required")
            self.assert_error_contains(errors, "card_count must be an integer")

            long_name = "a" * 64
            (deck_dir / "deck.json").write_text(
                (
                    '{"format_version":1,'
                    '"deck_id":"Bad Deck",'
                    f'"name":"{long_name}",'
                    '"card_count":1}\n'
                ),
                encoding="utf-8",
            )
            errors = self.verify(deck_dir)

            self.assert_error_contains(errors, "name exceeds 63 UTF-8 bytes")

            (deck_dir / "deck.json").write_text(
                json.dumps(
                    {
                        "format_version": 1,
                        "deck_id": "Bad Deck",
                        "name": "Bad\nName",
                        "card_count": 1,
                    }
                ) + "\n",
                encoding="utf-8",
            )
            errors = self.verify(deck_dir)

            self.assert_error_contains(errors, "name cannot contain control characters")

    def test_rejects_progress_files(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            deck_dir = self.write_deck(Path(temp_dir))
            (deck_dir / "state.tsv").write_text("progress\n", encoding="utf-8")

            self.assert_error_contains(
                self.verify(deck_dir),
                "must not be committed or packaged",
            )

    def test_rejects_media_or_extra_files_in_text_deck(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            deck_dir = self.write_deck(Path(temp_dir))
            (deck_dir / "media").mkdir()
            (deck_dir / "cards.tsv.tmp").write_text("partial\n", encoding="utf-8")
            errors = self.verify(deck_dir)

            self.assert_error_contains(errors, "media: unexpected entry")
            self.assert_error_contains(errors, "cards.tsv.tmp: unexpected entry")

    def test_rejects_settings_beyond_app_limit(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            deck_dir = self.write_deck(Path(temp_dir))
            (deck_dir / "settings.tsv").write_text(
                "new_limit\t1000001\nreview_limit\t200\n",
                encoding="utf-8",
            )

            self.assert_error_contains(
                self.verify(deck_dir),
                "limit must be at most 1000000",
            )


if __name__ == "__main__":
    unittest.main()
