#!/usr/bin/env python3
"""Verify an anki3ds text-only deck fixture or staged deck payload."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path


PROGRESS_FILES = (
    "state.tsv",
    "state.tsv.tmp",
    "state.tsv.bak",
    "review-log.tsv",
    "review-log.tsv.tmp",
    "review-log.tsv.bak",
    "settings.tsv.tmp",
    "settings.tsv.bak",
)


def append_file_error(errors: list[str], path: Path, message: str) -> None:
    errors.append(f"{path}: {message}")


def load_deck_json(deck_dir: Path, errors: list[str]) -> dict[str, object] | None:
    deck_json_path = deck_dir / "deck.json"

    if not deck_json_path.is_file():
        append_file_error(errors, deck_json_path, "missing")
        return None

    try:
        loaded = json.loads(deck_json_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        append_file_error(errors, deck_json_path, str(error))
        return None

    if not isinstance(loaded, dict):
        append_file_error(errors, deck_json_path, "must contain a JSON object")
        return None

    return loaded


def read_text_rows(path: Path, errors: list[str]) -> list[str]:
    if not path.is_file():
        append_file_error(errors, path, "missing")
        return []

    try:
        return path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        append_file_error(errors, path, str(error))
        return []


def verify_deck_json(
    deck_dir: Path,
    deck_json: dict[str, object],
    card_rows: list[str],
    errors: list[str],
) -> None:
    deck_json_path = deck_dir / "deck.json"
    deck_id = deck_dir.name

    if deck_json.get("format_version") != 1:
        append_file_error(errors, deck_json_path, "format_version must be 1")
    if deck_json.get("deck_id") != deck_id:
        append_file_error(errors, deck_json_path, "deck_id must match folder name")
    if not isinstance(deck_json.get("name"), str) or deck_json.get("name") == "":
        append_file_error(errors, deck_json_path, "name is required")
    if deck_json.get("card_count") != len(card_rows):
        append_file_error(
            errors,
            deck_json_path,
            "card_count must match cards.tsv row count",
        )


def verify_cards(deck_dir: Path, card_rows: list[str], errors: list[str]) -> None:
    cards_path = deck_dir / "cards.tsv"
    seen_card_ids: set[str] = set()

    for line_number, row in enumerate(card_rows, start=1):
        fields = row.split("\t")
        if len(fields) != 5:
            append_file_error(
                errors,
                cards_path,
                (
                    f"line {line_number}: expected 5 text-card fields, "
                    f"got {len(fields)}"
                ),
            )
            continue
        card_id = fields[0]
        if card_id == "":
            append_file_error(
                errors,
                cards_path,
                f"line {line_number}: card_id is required",
            )
        elif card_id in seen_card_ids:
            append_file_error(
                errors,
                cards_path,
                f"line {line_number}: duplicate card_id {card_id}",
            )
        seen_card_ids.add(card_id)


def verify_settings(
    deck_dir: Path,
    settings_rows: list[str],
    errors: list[str],
) -> None:
    settings_path = deck_dir / "settings.tsv"
    setting_counts = {"new_limit": 0, "review_limit": 0}

    for line_number, row in enumerate(settings_rows, start=1):
        fields = row.split("\t")
        if len(fields) != 2:
            append_file_error(
                errors,
                settings_path,
                f"line {line_number}: expected setting and value",
            )
            continue

        key, value = fields
        if key not in setting_counts or re.fullmatch(r"[0-9]+", value) is None:
            append_file_error(
                errors,
                settings_path,
                (
                    f"line {line_number}: expected new_limit or review_limit "
                    "with a non-negative integer value"
                ),
            )
            continue

        setting_counts[key] += 1

    if (
        setting_counts["new_limit"] != 1
        or setting_counts["review_limit"] != 1
        or len(settings_rows) != 2
    ):
        append_file_error(
            errors,
            settings_path,
            "expected exactly one new_limit row and one review_limit row",
        )


def verify_no_progress_files(deck_dir: Path, errors: list[str]) -> None:
    for progress_file in PROGRESS_FILES:
        path = deck_dir / progress_file
        if path.exists():
            append_file_error(errors, path, "must not be committed or packaged")


def verify_text_deck(deck_dir: Path) -> list[str]:
    errors: list[str] = []

    if not deck_dir.is_dir():
        append_file_error(errors, deck_dir, "missing")
        return errors

    deck_json = load_deck_json(deck_dir, errors)
    card_rows = read_text_rows(deck_dir / "cards.tsv", errors)
    settings_rows = read_text_rows(deck_dir / "settings.tsv", errors)

    if deck_json is not None:
        verify_deck_json(deck_dir, deck_json, card_rows, errors)
    verify_cards(deck_dir, card_rows, errors)
    verify_settings(deck_dir, settings_rows, errors)
    verify_no_progress_files(deck_dir, errors)

    return errors


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: verify_text_deck.py DECK_DIR...", file=sys.stderr)
        return 2

    errors: list[str] = []
    for raw_deck_dir in sys.argv[1:]:
        errors.extend(verify_text_deck(Path(raw_deck_dir)))

    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
