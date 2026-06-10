#!/usr/bin/env python3
"""Verify an anki3ds text-only deck fixture or staged deck payload."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import NamedTuple


DECK_MAX_CARDS = 1024
DECK_MAX_ID_LENGTH = 32
DECK_MAX_TEXT_LENGTH = 384
DECK_MAX_TAGS_LENGTH = 128
DECK_MAX_NAME_LENGTH = 64
DECK_MAX_LINE_LENGTH = 1024
DECK_MAX_ROW_BYTES = DECK_MAX_LINE_LENGTH - 2
APP_SETTINGS_MAX_DAILY_LIMIT = 1000000
APP_SETTINGS_MAX_LEARNING_MODE = 1

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
RUNTIME_SUPPORTED_JSON_NAME_ESCAPES = frozenset(('"', "\\", "/"))


class DuplicateJsonKeyError(ValueError):
    pass


class DeckSummary(NamedTuple):
    deck_dir: Path
    deck_name: str
    card_count: int
    new_limit: int
    review_limit: int


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
        loaded = json.loads(
            deck_json_path.read_text(encoding="utf-8"),
            object_pairs_hook=json_object_without_duplicate_keys,
        )
    except (OSError, json.JSONDecodeError, DuplicateJsonKeyError) as error:
        append_file_error(errors, deck_json_path, str(error))
        return None

    if not isinstance(loaded, dict):
        append_file_error(errors, deck_json_path, "must contain a JSON object")
        return None

    return loaded


def json_object_without_duplicate_keys(
    pairs: list[tuple[str, object]],
) -> dict[str, object]:
    loaded: dict[str, object] = {}

    for key, value in pairs:
        if key in loaded:
            raise DuplicateJsonKeyError(f"duplicate JSON key: {key}")

        loaded[key] = value

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


def raw_json_string_value(document: str, key: str) -> str | None:
    match = re.search(
        r'"' + re.escape(key) + r'"\s*:\s*"((?:[^"\\]|\\.)*)"',
        document,
        re.DOTALL,
    )

    if match is None:
        return None

    return match.group(1)


def runtime_supports_raw_json_name(raw_name: str) -> bool:
    index = 0

    while index < len(raw_name):
        if raw_name[index] != "\\":
            index += 1
            continue

        index += 1
        if index >= len(raw_name):
            return False
        if raw_name[index] not in RUNTIME_SUPPORTED_JSON_NAME_ESCAPES:
            return False

        index += 1

    return True


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
    raw_deck_name: str | None = None

    try:
        raw_deck_name = raw_json_string_value(
            deck_json_path.read_text(encoding="utf-8"),
            "name",
        )
    except OSError:
        raw_deck_name = None

    if deck_json.get("format_version") != 1:
        append_file_error(errors, deck_json_path, "format_version must be 1")
    if deck_json.get("deck_id") != deck_id:
        append_file_error(errors, deck_json_path, "deck_id must match folder name")
    if not isinstance(deck_name, str) or deck_name == "":
        append_file_error(errors, deck_json_path, "name is required")
    elif any(ord(character) < 32 or ord(character) == 127 for character in deck_name):
        append_file_error(
            errors,
            deck_json_path,
            "name cannot contain control characters",
        )
    elif utf8_length(deck_name) >= DECK_MAX_NAME_LENGTH:
        append_file_error(
            errors,
            deck_json_path,
            f"name exceeds {DECK_MAX_NAME_LENGTH - 1} UTF-8 bytes",
        )
    elif raw_deck_name is not None and not runtime_supports_raw_json_name(raw_deck_name):
        append_file_error(
            errors,
            deck_json_path,
            "name uses a JSON escape unsupported by the 3DS display parser",
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
        if utf8_length(row) > DECK_MAX_ROW_BYTES:
            append_file_error(
                errors,
                cards_path,
                f"line {line_number}: row exceeds {DECK_MAX_ROW_BYTES} bytes",
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
    optional_setting_counts = {"learning_mode": 0}

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
        if (
            (
                key not in setting_counts
                and key not in optional_setting_counts
            )
            or re.fullmatch(r"[0-9]+", value) is None
        ):
            append_file_error(
                errors,
                settings_path,
                (
                    f"line {line_number}: expected a supported setting "
                    "with a non-negative integer value"
                ),
            )
            continue

        parsed_value = int(value)
        max_value = (
            APP_SETTINGS_MAX_DAILY_LIMIT
            if key in setting_counts
            else APP_SETTINGS_MAX_LEARNING_MODE
        )
        if parsed_value > max_value:
            limit_label = "limit" if key in setting_counts else key
            append_file_error(
                errors,
                settings_path,
                (
                    f"line {line_number}: {limit_label} must be at most "
                    f"{max_value}"
                ),
            )
            continue

        if key in setting_counts:
            setting_counts[key] += 1
        else:
            optional_setting_counts[key] += 1

    for key, count in optional_setting_counts.items():
        if count > 1:
            append_file_error(errors, settings_path, f"duplicate {key} row")

    if (
        setting_counts["new_limit"] != 1
        or setting_counts["review_limit"] != 1
    ):
        append_file_error(
            errors,
            settings_path,
            "expected exactly one new_limit row and one review_limit row",
        )


def verify_no_progress_files(
    deck_dir: Path,
    errors: list[str],
    allow_progress_files: bool = False,
) -> None:
    if allow_progress_files:
        return

    for progress_file in PROGRESS_FILES:
        path = deck_dir / progress_file
        if path.exists():
            append_file_error(errors, path, "must not be committed or packaged")


def verify_text_deck_entries(
    deck_dir: Path,
    errors: list[str],
    allow_progress_files: bool = False,
) -> None:
    try:
        entries = sorted(deck_dir.iterdir(), key=lambda entry: entry.name)
    except OSError as error:
        append_file_error(errors, deck_dir, str(error))
        return

    expected_files = set(TEXT_DECK_FILES)
    if allow_progress_files:
        expected_files.update(PROGRESS_FILES)

    for entry in entries:
        if entry.name not in expected_files:
            append_file_error(
                errors,
                entry,
                "unexpected entry in text-only deck",
            )


def verify_text_deck(
    deck_dir: Path,
    allow_progress_files: bool = False,
) -> list[str]:
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
    verify_no_progress_files(deck_dir, errors, allow_progress_files)
    verify_text_deck_entries(deck_dir, errors, allow_progress_files)

    return errors


def verified_deck_summary(deck_dir: Path) -> DeckSummary:
    deck_json = json.loads((deck_dir / "deck.json").read_text(encoding="utf-8"))
    settings: dict[str, int] = {}

    for row in (deck_dir / "settings.tsv").read_text(encoding="utf-8").splitlines():
        if row == "" or row.startswith("#"):
            continue
        key, value = row.split("\t")
        settings[key] = int(value)

    return DeckSummary(
        deck_dir=deck_dir,
        deck_name=str(deck_json["name"]),
        card_count=len(
            (deck_dir / "cards.tsv").read_text(encoding="utf-8").splitlines()
        ),
        new_limit=settings["new_limit"],
        review_limit=settings["review_limit"],
    )


def format_limit(limit: int) -> str:
    if limit == 0:
        return "unlimited"
    return f"{limit}/day"


def format_summary(summary: DeckSummary) -> str:
    return (
        f"{summary.deck_dir}: ok - {summary.deck_name} "
        f"({summary.card_count} cards, "
        f"new {format_limit(summary.new_limit)}, "
        f"review {format_limit(summary.review_limit)})"
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Verify an anki3ds text-only deck fixture or staged deck payload."
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="only print validation errors",
    )
    parser.add_argument(
        "--allow-progress-files",
        action="store_true",
        help=(
            "allow app-owned state/log/temp/backup files when validating a "
            "live SD-card deck directory"
        ),
    )
    parser.add_argument("deck_dirs", nargs="+", type=Path, metavar="DECK_DIR")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    errors: list[str] = []
    summaries: list[DeckSummary] = []
    for deck_dir in args.deck_dirs:
        deck_errors = verify_text_deck(deck_dir, args.allow_progress_files)
        errors.extend(deck_errors)
        if not deck_errors:
            summaries.append(verified_deck_summary(deck_dir))

    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1

    if not args.quiet:
        for summary in summaries:
            print(format_summary(summary))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
