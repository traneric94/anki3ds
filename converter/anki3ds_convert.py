#!/usr/bin/env python3
"""Convert a simple tab-separated Anki export into an anki3ds deck."""

from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class ConvertedCard:
    card_id: str
    note_id: str
    front: str
    back: str
    tags: str


def escape_tsv_field(value: str) -> str:
    return value.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n")


def stable_id(prefix: str, *parts: str) -> str:
    digest = hashlib.sha1("\x1f".join(parts).encode("utf-8")).hexdigest()
    return f"{prefix}-{digest[:12]}"


def convert_lines(
    lines: list[str],
    front_field: int,
    back_field: int,
    tags_field: int | None,
) -> list[ConvertedCard]:
    cards: list[ConvertedCard] = []

    for line_number, raw_line in enumerate(lines, start=1):
        line = raw_line.rstrip("\n\r")

        if not line:
            continue

        fields = line.split("\t")
        max_field = max(front_field, back_field, tags_field or 0)
        if len(fields) <= max_field:
            raise ValueError(f"line {line_number}: expected at least {max_field + 1} fields")

        front = fields[front_field].strip()
        back = fields[back_field].strip()
        tags = fields[tags_field].strip() if tags_field is not None else ""

        if not front:
            raise ValueError(f"line {line_number}: front field is empty")
        if not back:
            raise ValueError(f"line {line_number}: back field is empty")

        note_id = stable_id("note", front, back, tags)
        card_id = stable_id("card", note_id, front, back)
        cards.append(ConvertedCard(card_id, note_id, front, back, tags))

    if not cards:
        raise ValueError("input did not contain any cards")

    return cards


def write_deck(output_dir: Path, deck_id: str, deck_name: str, cards: list[ConvertedCard]) -> None:
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


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert a tab-separated Anki/plain-text export to an anki3ds deck."
    )
    parser.add_argument("input", type=Path, help="UTF-8 tab-separated input file")
    parser.add_argument("output", type=Path, help="output deck directory")
    parser.add_argument("--deck-id", default="converted", help="deck id written to deck.json")
    parser.add_argument("--deck-name", default="Converted Deck", help="deck name written to deck.json")
    parser.add_argument("--front-field", type=int, default=0, help="zero-based front field index")
    parser.add_argument("--back-field", type=int, default=1, help="zero-based back field index")
    parser.add_argument("--tags-field", type=int, default=None, help="zero-based tags field index")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.front_field < 0 or args.back_field < 0:
        raise SystemExit("field indexes must be non-negative")
    if args.tags_field is not None and args.tags_field < 0:
        raise SystemExit("field indexes must be non-negative")

    lines = args.input.read_text(encoding="utf-8").splitlines()
    cards = convert_lines(lines, args.front_field, args.back_field, args.tags_field)
    write_deck(args.output, args.deck_id, args.deck_name, cards)
    print(f"wrote {len(cards)} cards to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
