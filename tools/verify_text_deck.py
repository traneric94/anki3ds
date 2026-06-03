#!/usr/bin/env python3
"""Verify an anki3ds text-only deck fixture or staged deck payload."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path


DECK_MAX_CARDS = 256
DECK_MAX_ID_LENGTH = 32
DECK_MAX_TEXT_LENGTH = 384
DECK_MAX_TAGS_LENGTH = 128
DECK_MAX_NAME_LENGTH = 64
DECK_MAX_LINE_LENGTH = 1024
APP_SETTINGS_MAX_DAILY_LIMIT = 1000000

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

TEXT_DECK_FILES = frozenset(("deck.json", "cards.tsv", "settings.tsv"))


def utf8_length(value: str) -> int:
    return len(value.encode("utf-8"))


def append_file_error(errors: list[str], path: Path, message: str) -> None:
    errors.append(f"{path}: {message}")


def deck_id_is_valid(deck_id: str) -> bool:
    if deck_id == "" or deck_id.startswith("."):
        return False
    if len(deck_id) >= DECK_MAX_NAME_LENGTH:
        return False

    return all(
        ("a" <= character <= "z")
        or ("A" <= character <= "Z")
        or ("0" <= character <= "9")
        or character in "-_"
        for character in deck_id
    )


def card_id_is_valid(card_id: str) -> bool:
    if card_id.startswith("#"):
        return False

    return all(ord(character) >= 32 and ord(character) != 127 for character in card_id)


def unescape_card_field(value: str) -> str | None:
    output: list[str] = []
    index = 0

    while index < len(value):
        character = value[index]
        if character != "\\":
            output.append(character)
            index += 1
            continue

        index += 1
        if index >= len(value):
            return None

        escaped = value[index]
        if escaped == "n":
            output.append("\n")
        elif escaped == "t":
            output.append("\t")
        elif escaped == "\\":
            output.append("\\")
        else:
            return None

        index += 1

    return "".join(output)


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
    card_count = deck_json.get("card_count")
    deck_name = deck_json.get("name")

    if deck_json.get("format_version") != 1:
        append_file_error(errors, deck_json_path, "format_version must be 1")
    if deck_json.get("deck_id") != deck_id:
        append_file_error(errors, deck_json_path, "deck_id must match folder name")
    if not isinstance(deck_name, str) or deck_name == "":
        append_file_error(errors, deck_json_path, "name is required")
    elif utf8_length(deck_name) >= DECK_MAX_NAME_LENGTH:
        append_file_error(
            errors,
            deck_json_path,
            f"name exceeds {DECK_MAX_NAME_LENGTH - 1} UTF-8 bytes",
        )
    if not isinstance(card_count, int) or isinstance(card_count, bool):
        append_file_error(errors, deck_json_path, "card_count must be an integer")
    elif card_count != len(card_rows):
        append_file_error(
            errors,
            deck_json_path,
            "card_count must match cards.tsv row count",
        )


def validate_field_length(
    errors: list[str],
    path: Path,
    line_number: int,
    field_name: str,
    value: str,
    field_size: int,
) -> None:
    if utf8_length(value) >= field_size:
        append_file_error(
            errors,
            path,
            (
                f"line {line_number}: {field_name} exceeds "
                f"{field_size - 1} UTF-8 bytes"
            ),
        )


def verify_cards(deck_dir: Path, card_rows: list[str], errors: list[str]) -> None:
    cards_path = deck_dir / "cards.tsv"
    seen_card_ids: set[str] = set()

    if len(card_rows) == 0:
        append_file_error(errors, cards_path, "must contain at least one card")
    if len(card_rows) > DECK_MAX_CARDS:
        append_file_error(
            errors,
            cards_path,
            f"must contain at most {DECK_MAX_CARDS} cards",
        )

    for line_number, row in enumerate(card_rows, start=1):
        if utf8_length(row) >= DECK_MAX_LINE_LENGTH:
            append_file_error(
                errors,
                cards_path,
                f"line {line_number}: row exceeds {DECK_MAX_LINE_LENGTH - 1} bytes",
            )

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
        unescaped_fields: list[str] = []
        field_names = ("card_id", "note_id", "front", "back", "tags")
        for field_name, field in zip(field_names, fields):
            unescaped = unescape_card_field(field)
            if unescaped is None:
                append_file_error(
                    errors,
                    cards_path,
                    f"line {line_number}: {field_name} has a bad escape",
                )
                unescaped = ""
            unescaped_fields.append(unescaped)

        card_id, note_id, front, back, tags = unescaped_fields
        if card_id == "":
            append_file_error(
                errors,
                cards_path,
                f"line {line_number}: card_id is required",
            )
        elif not card_id_is_valid(card_id):
            append_file_error(
                errors,
                cards_path,
                f"line {line_number}: card_id is invalid",
            )
        elif card_id in seen_card_ids:
            append_file_error(
                errors,
                cards_path,
                f"line {line_number}: duplicate card_id {card_id}",
            )
        seen_card_ids.add(card_id)

        if front == "":
            append_file_error(
                errors,
                cards_path,
                f"line {line_number}: front is required",
            )
        if back == "":
            append_file_error(
                errors,
                cards_path,
                f"line {line_number}: back is required",
            )

        validate_field_length(
            errors,
            cards_path,
            line_number,
            "card_id",
            card_id,
            DECK_MAX_ID_LENGTH,
        )
        validate_field_length(
            errors,
            cards_path,
            line_number,
            "note_id",
            note_id,
            DECK_MAX_ID_LENGTH,
        )
        validate_field_length(
            errors,
            cards_path,
            line_number,
            "front",
            front,
            DECK_MAX_TEXT_LENGTH,
        )
        validate_field_length(
            errors,
            cards_path,
            line_number,
            "back",
            back,
            DECK_MAX_TEXT_LENGTH,
        )
        validate_field_length(
            errors,
            cards_path,
            line_number,
            "tags",
            tags,
            DECK_MAX_TAGS_LENGTH,
        )


def verify_settings(
    deck_dir: Path,
    settings_rows: list[str],
    errors: list[str],
) -> None:
    settings_path = deck_dir / "settings.tsv"
    setting_counts = {"new_limit": 0, "review_limit": 0}

    for line_number, row in enumerate(settings_rows, start=1):
        if row == "" or row.startswith("#"):
            continue

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

        if int(value) > APP_SETTINGS_MAX_DAILY_LIMIT:
            append_file_error(
                errors,
                settings_path,
                (
                    f"line {line_number}: limit must be at most "
                    f"{APP_SETTINGS_MAX_DAILY_LIMIT}"
                ),
            )
            continue

        setting_counts[key] += 1

    if (
        setting_counts["new_limit"] != 1
        or setting_counts["review_limit"] != 1
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


def verify_text_deck_entries(deck_dir: Path, errors: list[str]) -> None:
    try:
        entries = sorted(deck_dir.iterdir(), key=lambda entry: entry.name)
    except OSError as error:
        append_file_error(errors, deck_dir, str(error))
        return

    for entry in entries:
        if entry.name not in TEXT_DECK_FILES:
            append_file_error(
                errors,
                entry,
                "unexpected entry in text-only deck",
            )


def verify_text_deck(deck_dir: Path) -> list[str]:
    errors: list[str] = []

    if not deck_dir.is_dir():
        append_file_error(errors, deck_dir, "missing")
        return errors
    if not deck_id_is_valid(deck_dir.name):
        append_file_error(errors, deck_dir, "deck folder id is invalid")

    deck_json = load_deck_json(deck_dir, errors)
    card_rows = read_text_rows(deck_dir / "cards.tsv", errors)
    settings_rows = read_text_rows(deck_dir / "settings.tsv", errors)

    if deck_json is not None:
        verify_deck_json(deck_dir, deck_json, card_rows, errors)
    verify_cards(deck_dir, card_rows, errors)
    verify_settings(deck_dir, settings_rows, errors)
    verify_no_progress_files(deck_dir, errors)
    verify_text_deck_entries(deck_dir, errors)

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
