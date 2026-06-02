#!/usr/bin/env python3
"""Convert a simple tab-separated Anki export into an anki3ds deck."""

from __future__ import annotations

import argparse
import hashlib
from html.parser import HTMLParser
import json
from dataclasses import dataclass
from pathlib import Path

DECK_ID_MAX_LENGTH = 64
DEFAULT_SETTINGS = "new_limit\t20\nreview_limit\t200\n"

BLOCK_TAGS = {
    "address",
    "article",
    "aside",
    "blockquote",
    "dd",
    "div",
    "dl",
    "dt",
    "figcaption",
    "figure",
    "footer",
    "h1",
    "h2",
    "h3",
    "h4",
    "h5",
    "h6",
    "header",
    "hr",
    "li",
    "main",
    "nav",
    "ol",
    "p",
    "pre",
    "section",
    "table",
    "tbody",
    "td",
    "tfoot",
    "th",
    "thead",
    "tr",
    "ul",
}
LINE_BREAK_TAGS = {"br"}
SKIP_CONTENT_TAGS = {"script", "style"}


@dataclass(frozen=True)
class ConvertedCard:
    card_id: str
    note_id: str
    front: str
    back: str
    tags: str


class HtmlTextExtractor(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.parts: list[str] = []
        self.skip_depth = 0

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        del attrs
        tag = tag.lower()

        if tag in SKIP_CONTENT_TAGS:
            self.skip_depth += 1
            return
        if self.skip_depth > 0:
            return

        if tag in LINE_BREAK_TAGS or tag in BLOCK_TAGS:
            self.parts.append("\n")

    def handle_endtag(self, tag: str) -> None:
        tag = tag.lower()

        if tag in SKIP_CONTENT_TAGS:
            if self.skip_depth > 0:
                self.skip_depth -= 1
            return
        if self.skip_depth > 0:
            return

        if tag in BLOCK_TAGS:
            self.parts.append("\n")

    def handle_data(self, data: str) -> None:
        if self.skip_depth == 0:
            self.parts.append(data)

    def text(self) -> str:
        return "".join(self.parts)


def escape_tsv_field(value: str) -> str:
    return value.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n")


def normalize_text(value: str) -> str:
    parser = HtmlTextExtractor()
    parser.feed(value)
    parser.close()

    lines = []
    for line in parser.text().replace("\r\n", "\n").replace("\r", "\n").split("\n"):
        collapsed = " ".join(line.split())
        if collapsed:
            lines.append(collapsed)

    return "\n".join(lines)


def stable_id(prefix: str, *parts: str) -> str:
    digest = hashlib.sha1("\x1f".join(parts).encode("utf-8")).hexdigest()
    return f"{prefix}-{digest[:12]}"


def unique_id(base_id: str, seen_counts: dict[str, int]) -> str:
    count = seen_counts.get(base_id, 0) + 1
    seen_counts[base_id] = count

    if count == 1:
        return base_id

    return f"{base_id}-{count}"


def deck_id_is_valid(value: str) -> bool:
    if not value or len(value) >= DECK_ID_MAX_LENGTH:
        return False

    return all(
        character.isascii() and (character.isalnum() or character in "_-")
        for character in value
    )


def convert_lines(
    lines: list[str],
    front_field: int,
    back_field: int,
    tags_field: int | None,
) -> list[ConvertedCard]:
    cards: list[ConvertedCard] = []
    seen_card_ids: dict[str, int] = {}
    seen_note_ids: dict[str, int] = {}

    for line_number, raw_line in enumerate(lines, start=1):
        line = raw_line.rstrip("\n\r")

        if not line:
            continue

        fields = line.split("\t")
        max_field = max(front_field, back_field, tags_field or 0)
        if len(fields) <= max_field:
            raise ValueError(f"line {line_number}: expected at least {max_field + 1} fields")

        front = normalize_text(fields[front_field])
        back = normalize_text(fields[back_field])
        tags = normalize_text(fields[tags_field]) if tags_field is not None else ""

        if not front:
            raise ValueError(f"line {line_number}: front field is empty")
        if not back:
            raise ValueError(f"line {line_number}: back field is empty")

        base_note_id = stable_id("note", front, back, tags)
        base_card_id = stable_id("card", base_note_id, front, back)
        note_id = unique_id(base_note_id, seen_note_ids)
        card_id = unique_id(base_card_id, seen_card_ids)
        cards.append(ConvertedCard(card_id, note_id, front, back, tags))

    if not cards:
        raise ValueError("input did not contain any cards")

    return cards


def write_deck(
    output_dir: Path,
    deck_id: str,
    deck_name: str,
    cards: list[ConvertedCard],
) -> None:
    if not deck_id_is_valid(output_dir.name):
        raise ValueError(
            "output deck folder name must use letters, numbers, '_' or '-'"
        )
    if not deck_id_is_valid(deck_id):
        raise ValueError("deck id must use letters, numbers, '_' or '-'")
    if deck_id != output_dir.name:
        raise ValueError("deck id must match output deck folder name")

    output_dir.mkdir(parents=True, exist_ok=True)

    deck_json = {
        "format_version": 1,
        "deck_id": deck_id,
        "name": deck_name,
        "created_by": "anki3ds-converter",
        "card_count": len(cards),
    }

    (output_dir / "deck.json").write_text(
        json.dumps(deck_json, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )

    with (output_dir / "cards.tsv").open("w", encoding="utf-8", newline="\n") as file:
        for card in cards:
            file.write(
                "\t".join(
                    [
                        escape_tsv_field(card.card_id),
                        escape_tsv_field(card.note_id),
                        escape_tsv_field(card.front),
                        escape_tsv_field(card.back),
                        escape_tsv_field(card.tags),
                    ]
                )
            )
            file.write("\n")

    settings_path = output_dir / "settings.tsv"
    if not settings_path.exists():
        settings_path.write_text(DEFAULT_SETTINGS, encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert a tab-separated Anki/plain-text export to an anki3ds deck."
    )
    parser.add_argument("input", type=Path, help="UTF-8 tab-separated input file")
    parser.add_argument("output", type=Path, help="output deck directory")
    parser.add_argument(
        "--deck-id",
        default=None,
        help="deck id written to deck.json; defaults to output folder name",
    )
    parser.add_argument(
        "--deck-name",
        default="Converted Deck",
        help="deck name written to deck.json",
    )
    parser.add_argument(
        "--front-field",
        type=int,
        default=0,
        help="zero-based front field index",
    )
    parser.add_argument(
        "--back-field",
        type=int,
        default=1,
        help="zero-based back field index",
    )
    parser.add_argument(
        "--tags-field",
        type=int,
        default=None,
        help="zero-based tags field index",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.front_field < 0 or args.back_field < 0:
        raise SystemExit("field indexes must be non-negative")
    if args.tags_field is not None and args.tags_field < 0:
        raise SystemExit("field indexes must be non-negative")

    lines = args.input.read_text(encoding="utf-8").splitlines()
    cards = convert_lines(lines, args.front_field, args.back_field, args.tags_field)
    deck_id = args.deck_id if args.deck_id is not None else args.output.name
    write_deck(args.output, deck_id, args.deck_name, cards)
    print(f"wrote {len(cards)} cards to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
