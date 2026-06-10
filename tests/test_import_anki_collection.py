import importlib.util
import json
import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
IMPORT_ANKI_COLLECTION_PATH = ROOT / "tools" / "import_anki_collection.py"
IMPORT_ANKI_COLLECTION_SPEC = importlib.util.spec_from_file_location(
    "import_anki_collection",
    IMPORT_ANKI_COLLECTION_PATH,
)
import_anki_collection = importlib.util.module_from_spec(IMPORT_ANKI_COLLECTION_SPEC)
assert IMPORT_ANKI_COLLECTION_SPEC.loader is not None
sys.modules["import_anki_collection"] = import_anki_collection
IMPORT_ANKI_COLLECTION_SPEC.loader.exec_module(import_anki_collection)

VERIFY_TEXT_DECK_PATH = ROOT / "tools" / "verify_text_deck.py"
VERIFY_TEXT_DECK_SPEC = importlib.util.spec_from_file_location(
    "verify_text_deck",
    VERIFY_TEXT_DECK_PATH,
)
verify_text_deck = importlib.util.module_from_spec(VERIFY_TEXT_DECK_SPEC)
assert VERIFY_TEXT_DECK_SPEC.loader is not None
sys.modules["verify_text_deck"] = verify_text_deck
VERIFY_TEXT_DECK_SPEC.loader.exec_module(verify_text_deck)


def write_collection(path: Path) -> None:
    connection = sqlite3.connect(path)
    connection.create_collation(
        "unicase",
        import_anki_collection.unicase_compare,
    )
    connection.executescript(
        """
        create table decks (
          id integer primary key,
          name text not null collate unicase
        );
        create table cards (
          id integer primary key,
          nid integer not null,
          did integer not null,
          ord integer not null default 0
        );
        create table notes (
          id integer primary key,
          mid integer not null,
          flds text not null,
          tags text not null
        );
        create table fields (
          ntid integer not null,
          ord integer not null,
          name text not null collate unicase
        );
        """
    )
    connection.executemany(
        "insert into fields (ntid, ord, name) values (?, ?, ?)",
        [(10, 0, "Front"), (10, 1, "Back")],
    )
    connection.execute(
        "insert into decks (id, name) values (?, ?)",
        (1, "RecSys"),
    )
    connection.execute(
        "insert into decks (id, name) values (?, ?)",
        (2, "SAT Vocabulary"),
    )
    connection.executemany(
        "insert into notes (id, mid, flds, tags) values (?, ?, ?, ?)",
        [
            (100, 10, "<b>front</b>\x1f<div>back&nbsp;text</div>", " recsys ch1 "),
            (101, 10, "empty back\x1f", " recsys "),
            (200, 10, "word\x1fmeaning", " vocab "),
        ],
    )
    connection.executemany(
        "insert into cards (id, nid, did) values (?, ?, ?)",
        [(1000, 100, 1), (1001, 101, 1), (2000, 200, 2)],
    )
    connection.commit()
    connection.close()


class ImportAnkiCollectionTests(unittest.TestCase):
    def test_empty_back_placeholder_is_display_marker(self):
        self.assertEqual(
            import_anki_collection.DEFAULT_EMPTY_BACK_PLACEHOLDER,
            "(No answer)",
        )

    def test_portable_deck_id(self):
        self.assertEqual(
            import_anki_collection.portable_deck_id("SAT Vocabulary"),
            "sat-vocabulary",
        )
        self.assertEqual(
            import_anki_collection.portable_deck_id("RecSys"),
            "recsys",
        )
        self.assertEqual(
            import_anki_collection.portable_deck_id("!!!"),
            "deck",
        )

    def test_rejects_tracked_repo_output_by_default(self):
        with self.assertRaisesRegex(ValueError, "tracked repo path"):
            import_anki_collection.validate_output_root(ROOT / "sample-decks")

        import_anki_collection.validate_output_root(
            ROOT / "local" / "sdmc" / "3ds" / "anki3ds" / "decks"
        )

    def test_imports_selected_deck_to_verified_text_deck(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            collection = Path(temp_dir) / "collection.anki2"
            output_root = Path(temp_dir) / "decks"
            write_collection(collection)

            imported = import_anki_collection.import_collection(
                collection,
                output_root,
                ["RecSys"],
            )

            self.assertEqual(len(imported), 1)
            self.assertEqual(imported[0].deck_id, "recsys")
            self.assertEqual(imported[0].card_count, 2)
            self.assertEqual(imported[0].empty_back_count, 1)

            deck_dir = output_root / "recsys"
            self.assertEqual(verify_text_deck.verify_text_deck(deck_dir), [])
            deck_json = json.loads((deck_dir / "deck.json").read_text())
            self.assertEqual(deck_json["name"], "RecSys")
            rows = (deck_dir / "cards.tsv").read_text(encoding="utf-8").splitlines()
            self.assertIn("\tfront\tback text\trecsys ch1", rows[0])
            self.assertIn(
                import_anki_collection.DEFAULT_EMPTY_BACK_PLACEHOLDER,
                rows[1],
            )

    def test_imports_all_populated_decks(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            collection = Path(temp_dir) / "collection.anki2"
            output_root = Path(temp_dir) / "decks"
            write_collection(collection)

            imported = import_anki_collection.import_collection(
                collection,
                output_root,
            )

            self.assertEqual(
                [deck.deck_id for deck in imported],
                ["recsys", "sat-vocabulary"],
            )
            self.assertEqual(verify_text_deck.verify_text_deck(output_root / "recsys"), [])
            self.assertEqual(
                verify_text_deck.verify_text_deck(output_root / "sat-vocabulary"),
                [],
            )

    def test_empty_back_can_be_rejected(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            collection = Path(temp_dir) / "collection.anki2"
            output_root = Path(temp_dir) / "decks"
            write_collection(collection)

            with self.assertRaisesRegex(ValueError, "empty back"):
                import_anki_collection.import_collection(
                    collection,
                    output_root,
                    ["RecSys"],
                    empty_back_placeholder="",
                )

    def test_duplicate_deck_ids_are_stable_for_selected_imports(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            collection = Path(temp_dir) / "collection.anki2"
            all_output = Path(temp_dir) / "all"
            selected_output = Path(temp_dir) / "selected"
            write_collection(collection)
            connection = sqlite3.connect(collection)
            connection.execute(
                "insert into decks (id, name) values (?, ?)",
                (3, "RecSys"),
            )
            connection.execute(
                "insert into notes (id, mid, flds, tags) values (?, ?, ?, ?)",
                (300, 10, "front two\x1fback two", " recsys "),
            )
            connection.execute(
                "insert into cards (id, nid, did) values (?, ?, ?)",
                (3000, 300, 3),
            )
            connection.commit()
            connection.close()

            all_imported = import_anki_collection.import_collection(
                collection,
                all_output,
            )
            selected_imported = import_anki_collection.import_collection(
                collection,
                selected_output,
                ["1"],
            )

            all_recsys_ids = [
                deck.deck_id for deck in all_imported if deck.source_name == "RecSys"
            ]
            self.assertIn(selected_imported[0].deck_id, all_recsys_ids)

    def test_rejects_non_forward_card_ord(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            collection = Path(temp_dir) / "collection.anki2"
            output_root = Path(temp_dir) / "decks"
            write_collection(collection)
            connection = sqlite3.connect(collection)
            connection.execute(
                "insert into cards (id, nid, did, ord) values (?, ?, ?, ?)",
                (1002, 100, 1, 1),
            )
            connection.commit()
            connection.close()

            with self.assertRaisesRegex(ValueError, "template ord"):
                import_anki_collection.import_collection(
                    collection,
                    output_root,
                    ["RecSys"],
                )

    def test_rejects_sound_media_marker(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            collection = Path(temp_dir) / "collection.anki2"
            output_root = Path(temp_dir) / "decks"
            write_collection(collection)
            connection = sqlite3.connect(collection)
            connection.execute(
                "update notes set flds = ? where id = ?",
                ("front\x1f[sound:answer.mp3]", 100),
            )
            connection.commit()
            connection.close()

            with self.assertRaisesRegex(ValueError, "unsupported media"):
                import_anki_collection.import_collection(
                    collection,
                    output_root,
                    ["RecSys"],
                )


if __name__ == "__main__":
    unittest.main()
