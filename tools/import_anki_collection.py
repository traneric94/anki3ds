#!/usr/bin/env python3
"""Import text-only Basic Anki decks from collection.anki2 into anki3ds decks."""

from __future__ import annotations

import argparse
import re
import sqlite3
import sys
from dataclasses import dataclass
from html.parser import HTMLParser
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from converter.anki3ds_convert import (  # noqa: E402
    DECK_ID_MAX_LENGTH,
    DECK_MAX_NAME_LENGTH,
    ConvertedCard,
    normalize_text,
    stable_id,
    text_contains_image_tag,
    write_split_decks,
)


FIELD_SEPARATOR = "\x1f"
DEFAULT_EMPTY_BACK_PLACEHOLDER = "(No answer)"
UNSUPPORTED_MEDIA_TAGS = frozenset(
    ("audio", "embed", "iframe", "img", "object", "source", "video")
)
SOUND_MARKER_RE = re.compile(r"\[sound:[^\]]+\]", re.IGNORECASE)


@dataclass(frozen=True)
class AnkiDeck:
    deck_id: int
    name: str
    card_count: int


@dataclass(frozen=True)
class FieldInfo:
    ordinal: int
    name: str


@dataclass(frozen=True)
class ImportedDeck:
    source_id: int
    source_name: str
    deck_id: str
    card_count: int
    empty_back_count: int
    written_paths: tuple[Path, ...]


class UnsupportedMediaDetector(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.found = False

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        del attrs
        if tag.lower() in UNSUPPORTED_MEDIA_TAGS:
            self.found = True


def unicase_compare(left: str, right: str) -> int:
    folded_left = left.casefold()
    folded_right = right.casefold()
    return (folded_left > folded_right) - (folded_left < folded_right)


def connect_collection(path: Path) -> sqlite3.Connection:
    connection = sqlite3.connect(f"file:{path}?mode=ro", uri=True)
    connection.create_collation("unicase", unicase_compare)
    return connection


def path_is_relative_to(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def validate_output_root(output_root: Path, allow_tracked_output: bool = False) -> None:
    if allow_tracked_output:
        return

    resolved_output = output_root.expanduser().resolve()
    resolved_repo = ROOT.resolve()
    resolved_local = (ROOT / "local").resolve()

    if path_is_relative_to(resolved_output, resolved_repo) and not path_is_relative_to(
        resolved_output,
        resolved_local,
    ):
        raise ValueError(
            "refusing to write personal Anki import inside a tracked repo path; "
            "use local/sdmc/3ds/anki3ds/decks, an emulator SD directory, or "
            "--allow-tracked-output"
        )


def text_contains_unsupported_media(value: str) -> bool:
    if SOUND_MARKER_RE.search(value) is not None:
        return True

    parser = UnsupportedMediaDetector()
    parser.feed(value)
    parser.close()
    return parser.found


def portable_deck_id(name: str) -> str:
    base = re.sub(r"[^A-Za-z0-9_-]+", "-", name.strip().lower()).strip("-_")
    if not base:
        base = "deck"

    if len(base.encode("utf-8")) >= DECK_ID_MAX_LENGTH:
        suffix = stable_id("deck", name).replace("deck-", "")
        max_prefix_bytes = DECK_ID_MAX_LENGTH - len(suffix) - 2
        prefix = fit_utf8_bytes(base, max_prefix_bytes).strip("-_")
        if not prefix:
            prefix = "deck"
        base = f"{prefix}-{suffix}"

    return base


def unique_deck_id(base_id: str, source_id: int, used_ids: set[str]) -> str:
    candidate = base_id
    if candidate not in used_ids:
        used_ids.add(candidate)
        return candidate

    suffix = stable_id("deck", str(source_id)).replace("deck-", "")
    max_prefix_bytes = DECK_ID_MAX_LENGTH - len(suffix) - 2
    prefix = fit_utf8_bytes(base_id, max_prefix_bytes).strip("-_") or "deck"
    candidate = f"{prefix}-{suffix}"

    counter = 2
    while candidate in used_ids:
        numeric_suffix = f"{suffix}-{counter}"
        max_prefix_bytes = DECK_ID_MAX_LENGTH - len(numeric_suffix) - 2
        prefix = fit_utf8_bytes(base_id, max_prefix_bytes).strip("-_") or "deck"
        candidate = f"{prefix}-{numeric_suffix}"
        counter += 1

    used_ids.add(candidate)
    return candidate


def deck_id_with_source_suffix(
    base_id: str,
    source_id: int,
    used_ids: set[str],
) -> str:
    suffix = stable_id("deck", str(source_id)).replace("deck-", "")
    numeric_suffix = suffix
    counter = 2

    while True:
        max_prefix_bytes = DECK_ID_MAX_LENGTH - len(numeric_suffix) - 2
        prefix = fit_utf8_bytes(base_id, max_prefix_bytes).strip("-_") or "deck"
        candidate = f"{prefix}-{numeric_suffix}"
        if candidate not in used_ids:
            used_ids.add(candidate)
            return candidate
        numeric_suffix = f"{suffix}-{counter}"
        counter += 1


def fit_utf8_bytes(value: str, max_bytes: int) -> str:
    if max_bytes <= 0:
        return ""

    output = value
    while len(output.encode("utf-8")) > max_bytes:
        output = output[:-1]
    return output


def device_deck_name(name: str) -> str:
    cleaned = " ".join(name.split())
    if not cleaned:
        cleaned = "Imported Anki Deck"
    return fit_utf8_bytes(cleaned, DECK_MAX_NAME_LENGTH - 1)


def list_decks(connection: sqlite3.Connection) -> list[AnkiDeck]:
    rows = connection.execute(
        """
        select d.id, d.name, count(c.id)
        from decks d
        left join cards c on c.did = d.id
        group by d.id
        order by d.id
        """
    )
    return [AnkiDeck(int(row[0]), str(row[1]), int(row[2])) for row in rows]


def load_fields(connection: sqlite3.Connection) -> dict[int, list[FieldInfo]]:
    fields: dict[int, list[FieldInfo]] = {}
    rows = connection.execute(
        "select ntid, ord, name from fields order by ntid, ord"
    )
    for notetype_id, ordinal, name in rows:
        fields.setdefault(int(notetype_id), []).append(
            FieldInfo(int(ordinal), str(name))
        )
    return fields


def select_decks(
    decks: list[AnkiDeck],
    selectors: list[str],
) -> dict[int, str]:
    populated_decks = [deck for deck in decks if deck.card_count > 0]
    base_counts: dict[str, int] = {}
    for deck in populated_decks:
        base = portable_deck_id(deck.name)
        base_counts[base] = base_counts.get(base, 0) + 1

    all_output_ids: dict[int, str] = {}
    used_ids: set[str] = set()
    for deck in populated_decks:
        base = portable_deck_id(deck.name)
        if base_counts[base] > 1:
            output_id = deck_id_with_source_suffix(base, deck.deck_id, used_ids)
        else:
            output_id = unique_deck_id(base, deck.deck_id, used_ids)
        all_output_ids[deck.deck_id] = output_id

    if not selectors:
        selected = populated_decks
    else:
        selected = []
        for selector in selectors:
            matches = [
                deck
                for deck in populated_decks
                if str(deck.deck_id) == selector
                or deck.name == selector
                or all_output_ids[deck.deck_id] == selector
            ]
            if len(matches) != 1:
                raise ValueError(
                    f"deck selector {selector!r} matched {len(matches)} decks"
                )
            selected.append(matches[0])

    output_ids: dict[int, str] = {}
    for deck in selected:
        output_ids[deck.deck_id] = all_output_ids[deck.deck_id]
    return output_ids


def paired_field_indexes(fields: list[FieldInfo]) -> tuple[int, int]:
    candidate_pairs = (
        ("front", "back"),
        ("question", "answer"),
        ("prompt", "answer"),
        ("term", "definition"),
        ("word", "meaning"),
    )
    by_name = {field.name.casefold(): field.ordinal for field in fields}

    for front_name, back_name in candidate_pairs:
        if front_name in by_name and back_name in by_name:
            return by_name[front_name], by_name[back_name]

    raise ValueError(
        "notetype must have a supported text field pair "
        "(Front/Back, Question/Answer, Term/Definition, or Word/Meaning)"
    )


def field_value(values: list[str], index: int) -> str:
    if index < 0 or index >= len(values):
        return ""
    return values[index]


def tags_text(raw_tags: str) -> str:
    return " ".join(raw_tags.split())


def build_cards_for_deck(
    connection: sqlite3.Connection,
    source_deck: AnkiDeck,
    fields_by_notetype: dict[int, list[FieldInfo]],
    empty_back_placeholder: str,
) -> tuple[list[ConvertedCard], int]:
    cards: list[ConvertedCard] = []
    empty_back_count = 0
    rows = connection.execute(
        """
        select c.id, c.ord, n.id, n.mid, n.flds, n.tags
        from cards c
        join notes n on n.id = c.nid
        where c.did = ?
        order by c.id
        """,
        (source_deck.deck_id,),
    )

    for source_card_id, card_ord, source_note_id, notetype_id, raw_fields, raw_tags in rows:
        if int(card_ord) != 0:
            raise ValueError(
                f"{source_deck.name}: card {source_card_id} uses template ord "
                f"{card_ord}; only forward text cards are supported"
            )

        fields = fields_by_notetype.get(int(notetype_id), [])
        if len(fields) < 2:
            raise ValueError(
                f"{source_deck.name}: notetype {notetype_id} has fewer than two fields"
            )

        values = str(raw_fields).split(FIELD_SEPARATOR)
        try:
            front_index, back_index = paired_field_indexes(fields)
        except ValueError as error:
            raise ValueError(f"{source_deck.name}: {error}") from error
        raw_front = field_value(values, front_index)
        raw_back = field_value(values, back_index)

        if text_contains_image_tag(raw_front) or text_contains_image_tag(raw_back):
            raise ValueError(
                f"{source_deck.name}: card {source_card_id} contains image HTML"
            )
        if text_contains_unsupported_media(raw_front) or text_contains_unsupported_media(
            raw_back
        ):
            raise ValueError(
                f"{source_deck.name}: card {source_card_id} contains unsupported media"
            )

        front = normalize_text(raw_front)
        back = normalize_text(raw_back)
        if not front:
            raise ValueError(f"{source_deck.name}: card {source_card_id} has empty front")
        if not back:
            if not empty_back_placeholder:
                raise ValueError(
                    f"{source_deck.name}: card {source_card_id} has empty back"
                )
            back = empty_back_placeholder
            empty_back_count += 1

        cards.append(
            ConvertedCard(
                card_id=stable_id("card", str(source_card_id)),
                note_id=stable_id("note", str(source_note_id)),
                front=front,
                back=back,
                tags=tags_text(str(raw_tags)),
            )
        )

    return cards, empty_back_count


def import_collection(
    collection_path: Path,
    output_root: Path,
    deck_selectors: list[str] | None = None,
    empty_back_placeholder: str = DEFAULT_EMPTY_BACK_PLACEHOLDER,
    allow_tracked_output: bool = False,
) -> list[ImportedDeck]:
    if deck_selectors is None:
        deck_selectors = []

    validate_output_root(output_root, allow_tracked_output)

    with connect_collection(collection_path) as connection:
        source_decks = list_decks(connection)
        fields_by_notetype = load_fields(connection)
        selected_output_ids = select_decks(source_decks, deck_selectors)
        selected_by_id = {deck.deck_id: deck for deck in source_decks}

        imported: list[ImportedDeck] = []
        for source_id, output_id in selected_output_ids.items():
            source_deck = selected_by_id[source_id]
            cards, empty_back_count = build_cards_for_deck(
                connection,
                source_deck,
                fields_by_notetype,
                empty_back_placeholder,
            )
            written_paths = write_split_decks(
                output_root / output_id,
                output_id,
                device_deck_name(source_deck.name),
                cards,
                None,
            )
            imported.append(
                ImportedDeck(
                    source_id=source_deck.deck_id,
                    source_name=source_deck.name,
                    deck_id=output_id,
                    card_count=len(cards),
                    empty_back_count=empty_back_count,
                    written_paths=tuple(written_paths),
                )
            )

    return imported


def print_decks(collection_path: Path) -> None:
    with connect_collection(collection_path) as connection:
        for deck in list_decks(connection):
            if deck.card_count <= 0:
                continue
            print(f"{deck.deck_id}\t{portable_deck_id(deck.name)}\t{deck.card_count}\t{deck.name}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Import text-only Basic Anki decks into anki3ds deck folders."
    )
    parser.add_argument("collection", type=Path, help="path to collection.anki2")
    parser.add_argument(
        "output_root",
        type=Path,
        nargs="?",
        help="output root, e.g. local/sdmc/3ds/anki3ds/decks",
    )
    parser.add_argument(
        "--deck",
        action="append",
        default=[],
        help="deck name, Anki deck id, or generated anki3ds deck id to import",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="list importable source decks without writing output",
    )
    parser.add_argument(
        "--empty-back-placeholder",
        default=DEFAULT_EMPTY_BACK_PLACEHOLDER,
        help="answer text used when an Anki note has an empty Back field",
    )
    parser.add_argument(
        "--allow-tracked-output",
        action="store_true",
        help="allow output under tracked repo paths; use only for non-personal fixtures",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    try:
        if args.list:
            print_decks(args.collection)
            return 0

        if args.output_root is None:
            raise ValueError("output_root is required unless --list is used")

        imported = import_collection(
            args.collection,
            args.output_root,
            args.deck,
            args.empty_back_placeholder,
            args.allow_tracked_output,
        )
    except (OSError, sqlite3.Error, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    for deck in imported:
        chunk_text = (
            f" in {len(deck.written_paths)} chunks"
            if len(deck.written_paths) != 1
            else ""
        )
        empty_back_text = (
            f"; {deck.empty_back_count} empty backs used placeholder"
            if deck.empty_back_count
            else ""
        )
        print(
            f"imported {deck.source_name} -> {deck.deck_id}: "
            f"{deck.card_count} cards{chunk_text}{empty_back_text}"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
