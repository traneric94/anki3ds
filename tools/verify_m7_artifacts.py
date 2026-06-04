#!/usr/bin/env python3
"""Verify SD-card artifacts after an M7 daily-use study pass."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path


APP_SD_DIR = Path("3ds/anki3ds")
DEFAULT_SDMC = Path(
    os.environ.get(
        "M7_SDMC",
        os.environ.get(
            "AZAHAR_SDMC",
            "~/Library/Application Support/Azahar/sdmc",
        ),
    )
).expanduser()
DEFAULT_DECKS = ("limits-demo", "sample")
DEFAULT_REQUIRED_EVENTS = ("rating", "undo", "suspend", "restore")
SETTING_KEYS = frozenset(("new_limit", "review_limit"))
VALID_STATE_RATINGS = frozenset(("0", "1", "2", "3"))
VALID_REVIEW_LOG_EVENTS = frozenset(("rating", "suspend", "undo", "restore"))
VALID_REVIEW_LOG_RATINGS = frozenset(("again", "hard", "good", "easy", "-"))
MAX_LIMIT = 1000000
MAX_REVIEW_COUNT = 1000000
MAX_DAY = 1000000
MAX_INTERVAL_DAYS = 36500
MIN_EASE_PERMILLE = 1300
MAX_EASE_PERMILLE = 3500
MAX_LAPSES = 1000000
MAX_TIMESTAMP = 9223372036854775807
STATE_HEADER = "#anki3ds-state-v1"
STATE_FOOTER = "#anki3ds-state-complete"
STATE_FIELD_COUNT = 10
REVIEW_LOG_FIELD_COUNT = 21
RESET_PROGRESS_FILES = (
    "state.tsv",
    "state.tsv.tmp",
    "state.tsv.bak",
    "review-log.tsv",
    "review-log.tsv.tmp",
    "review-log.tsv.bak",
)


class DeckArtifacts:
    def __init__(
        self,
        deck_id: str,
        card_ids: set[str],
        settings: dict[str, int],
        reviewed_or_suspended_count: int,
        review_log_events: set[str],
    ) -> None:
        self.deck_id = deck_id
        self.card_ids = card_ids
        self.settings = settings
        self.reviewed_or_suspended_count = reviewed_or_suspended_count
        self.review_log_events = review_log_events


def append_error(errors: list[str], path: Path, message: str) -> None:
    errors.append(f"{path}: {message}")


def parse_unsigned(value: str, maximum: int | None = MAX_LIMIT) -> int | None:
    if value == "" or not value.isdigit():
        return None

    parsed = int(value)
    if maximum is not None and parsed > maximum:
        return None

    return parsed


def validate_scheduler_snapshot(
    fields: list[str],
    base_index: int,
    path: Path,
    line_number: int,
    errors: list[str],
    field_numbers: tuple[int, int, int, int, int, int, int, int] | None = None,
) -> None:
    review_count = parse_unsigned(fields[base_index], MAX_REVIEW_COUNT)
    first_review_day = parse_unsigned(fields[base_index + 1], MAX_DAY)
    last_review_day = parse_unsigned(fields[base_index + 2], MAX_DAY)
    due_day = parse_unsigned(fields[base_index + 3], MAX_DAY)
    interval_days = parse_unsigned(fields[base_index + 4], MAX_INTERVAL_DAYS)
    ease_permille = parse_unsigned(fields[base_index + 5], MAX_EASE_PERMILLE)
    lapses = parse_unsigned(fields[base_index + 6], MAX_LAPSES)
    suspended = parse_unsigned(fields[base_index + 7], 1)

    field_names = (
        "review_count",
        "first_review_day",
        "last_review_day",
        "due_day",
        "interval_days",
        "ease_permille",
        "lapses",
        "suspended",
    )
    values = (
        review_count,
        first_review_day,
        last_review_day,
        due_day,
        interval_days,
        ease_permille,
        lapses,
        suspended,
    )
    for offset, (field_name, value) in enumerate(zip(field_names, values)):
        if value is None:
            display_field = (
                field_numbers[offset]
                if field_numbers is not None
                else base_index + offset + 1
            )
            append_error(
                errors,
                path,
                f"line {line_number}: bad {field_name} field {display_field}",
            )

    if ease_permille is not None and ease_permille < MIN_EASE_PERMILLE:
        append_error(errors, path, f"line {line_number}: ease_permille too low")
    if (
        review_count is not None
        and lapses is not None
        and lapses > review_count
    ):
        append_error(errors, path, f"line {line_number}: lapses exceed review_count")
    if (
        review_count == 0
        and (
            interval_days not in (None, 0)
            or lapses not in (None, 0)
            or first_review_day not in (None, 0)
            or last_review_day not in (None, 0)
        )
    ):
        append_error(errors, path, f"line {line_number}: unreviewed card has review data")
    if (
        first_review_day not in (None, 0)
        and last_review_day not in (None, 0)
        and first_review_day > last_review_day
    ):
        append_error(errors, path, f"line {line_number}: first_review_day after last")
    if first_review_day not in (None, 0) and last_review_day == 0:
        append_error(errors, path, f"line {line_number}: first_review_day without last")


def read_required_rows(path: Path, errors: list[str]) -> list[str] | None:
    if not path.is_file():
        append_error(errors, path, "missing")
        return None

    try:
        text = path.read_text(encoding="utf-8")
    except OSError as error:
        append_error(errors, path, str(error))
        return None

    if text != "" and not text.endswith("\n"):
        append_error(errors, path, "must end with a newline")

    return text.splitlines()


def load_card_ids(deck_dir: Path, errors: list[str]) -> set[str]:
    path = deck_dir / "cards.tsv"
    rows = read_required_rows(path, errors)
    if rows is None:
        return set()
    card_ids: set[str] = set()

    for line_number, row in enumerate(rows, start=1):
        fields = row.split("\t")
        if len(fields) != 5:
            append_error(errors, path, f"line {line_number}: expected 5 fields")
            continue
        card_id = fields[0]
        if card_id == "" or card_id in card_ids:
            append_error(errors, path, f"line {line_number}: bad card id")
            continue

        card_ids.add(card_id)

    if len(card_ids) == 0:
        append_error(errors, path, "must contain at least one card")

    return card_ids


def load_settings(deck_dir: Path, errors: list[str]) -> dict[str, int]:
    path = deck_dir / "settings.tsv"
    rows = read_required_rows(path, errors)
    if rows is None:
        return {}
    settings: dict[str, int] = {}

    for line_number, row in enumerate(rows, start=1):
        if row == "" or row.startswith("#"):
            continue

        fields = row.split("\t")
        if len(fields) != 2:
            append_error(errors, path, f"line {line_number}: expected 2 fields")
            continue

        key, value_text = fields
        value = parse_unsigned(value_text)
        if key not in SETTING_KEYS:
            append_error(errors, path, f"line {line_number}: unknown setting")
        elif key in settings:
            append_error(errors, path, f"line {line_number}: duplicate setting")
        elif value is None:
            append_error(errors, path, f"line {line_number}: bad setting value")
        else:
            settings[key] = value

    for key in sorted(SETTING_KEYS):
        if key not in settings:
            append_error(errors, path, f"missing {key}")

    return settings


def state_row_is_reviewed_or_suspended(fields: list[str]) -> bool:
    review_count = parse_unsigned(fields[1], MAX_REVIEW_COUNT)
    suspended = fields[7]

    return (review_count is not None and review_count > 0) or suspended == "1"


def load_state(
    deck_dir: Path,
    card_ids: set[str],
    errors: list[str],
) -> int:
    path = deck_dir / "state.tsv"
    rows = read_required_rows(path, errors)
    if rows is None:
        return 0
    seen: set[str] = set()
    reviewed_or_suspended_count = 0

    if len(rows) < 2:
        append_error(errors, path, "must include header, rows, and footer")
        return 0

    header = rows[0].split("\t")
    footer = rows[-1].split("\t")
    data_rows = rows[1:-1]
    if len(header) != 2 or header[0] != STATE_HEADER:
        append_error(errors, path, "bad state header")
    if len(footer) != 2 or footer[0] != STATE_FOOTER:
        append_error(errors, path, "bad state footer")

    expected_count = parse_unsigned(header[1]) if len(header) == 2 else None
    footer_count = parse_unsigned(footer[1]) if len(footer) == 2 else None
    if expected_count != len(data_rows) or footer_count != len(data_rows):
        append_error(errors, path, "state row count mismatch")
    if len(data_rows) != len(card_ids):
        append_error(errors, path, "state rows must match card count")

    for line_number, row in enumerate(data_rows, start=2):
        fields = row.split("\t")
        if len(fields) != STATE_FIELD_COUNT:
            append_error(errors, path, f"line {line_number}: expected 10 fields")
            continue

        card_id = fields[0]
        if card_id not in card_ids:
            append_error(errors, path, f"line {line_number}: unknown card id")
        elif card_id in seen:
            append_error(errors, path, f"line {line_number}: duplicate card id")
        else:
            seen.add(card_id)

        if fields[2] not in VALID_STATE_RATINGS:
            append_error(errors, path, f"line {line_number}: bad last_rating")
        validate_scheduler_snapshot(
            [
                fields[0],
                fields[1],
                fields[8],
                fields[9],
                fields[3],
                fields[4],
                fields[5],
                fields[6],
                fields[7],
            ],
            1,
            path,
            line_number,
            errors,
            (2, 9, 10, 4, 5, 6, 7, 8),
        )
        if state_row_is_reviewed_or_suspended(fields):
            reviewed_or_suspended_count += 1

    if reviewed_or_suspended_count == 0:
        append_error(errors, path, "no reviewed or suspended cards found")

    return reviewed_or_suspended_count


def load_review_log(
    deck_dir: Path,
    card_ids: set[str],
    errors: list[str],
    allow_missing: bool,
) -> set[str]:
    path = deck_dir / "review-log.tsv"
    if allow_missing and not path.exists():
        return set()

    rows = read_required_rows(path, errors)
    if rows is None:
        return set()
    events: set[str] = set()

    if len(rows) == 0:
        append_error(errors, path, "must contain at least one complete row")
        return events

    for line_number, row in enumerate(rows, start=1):
        fields = row.split("\t")
        if len(fields) != REVIEW_LOG_FIELD_COUNT:
            append_error(errors, path, f"line {line_number}: expected 21 fields")
            continue

        event = fields[2]
        card_id = fields[3]
        rating = fields[4]
        if event not in VALID_REVIEW_LOG_EVENTS:
            append_error(errors, path, f"line {line_number}: bad event")
        else:
            events.add(event)
        if card_id not in card_ids:
            append_error(errors, path, f"line {line_number}: unknown card id")
        if rating not in VALID_REVIEW_LOG_RATINGS:
            append_error(errors, path, f"line {line_number}: bad rating")
        if event == "rating" and rating == "-":
            append_error(errors, path, f"line {line_number}: rating event needs rating")
        if event != "rating" and rating != "-":
            append_error(errors, path, f"line {line_number}: non-rating event needs -")

        if parse_unsigned(fields[0], MAX_TIMESTAMP) is None:
            append_error(errors, path, f"line {line_number}: bad timestamp")
        if parse_unsigned(fields[1], MAX_DAY) is None:
            append_error(errors, path, f"line {line_number}: bad day")
        validate_scheduler_snapshot(fields, 5, path, line_number, errors)
        validate_scheduler_snapshot(fields, 13, path, line_number, errors)

    return events


def parse_expected_settings(value: str) -> tuple[str, int, int]:
    parts = value.split(":")
    if len(parts) != 3:
        raise argparse.ArgumentTypeError("use deck_id:new_limit:review_limit")
    if parts[0] == "":
        raise argparse.ArgumentTypeError("deck_id must not be empty")

    new_limit = parse_unsigned(parts[1])
    review_limit = parse_unsigned(parts[2])
    if new_limit is None or review_limit is None:
        raise argparse.ArgumentTypeError("settings values must be unsigned")

    return parts[0], new_limit, review_limit


def verify_deck_artifacts(
    deck_root: Path,
    deck_id: str,
    errors: list[str],
    allow_missing_log: bool,
) -> DeckArtifacts | None:
    deck_dir = deck_root / deck_id
    if not deck_dir.is_dir():
        append_error(errors, deck_dir, "missing deck directory")
        return None

    card_ids = load_card_ids(deck_dir, errors)
    settings = load_settings(deck_dir, errors)
    reviewed_or_suspended_count = load_state(deck_dir, card_ids, errors)
    events = load_review_log(deck_dir, card_ids, errors, allow_missing_log)

    return DeckArtifacts(
        deck_id,
        card_ids,
        settings,
        reviewed_or_suspended_count,
        events,
    )


def verify_reset_deck_artifacts(
    deck_root: Path,
    deck_id: str,
    errors: list[str],
) -> DeckArtifacts | None:
    deck_dir = deck_root / deck_id
    if not deck_dir.is_dir():
        append_error(errors, deck_dir, "missing deck directory")
        return None

    card_ids = load_card_ids(deck_dir, errors)
    settings = load_settings(deck_dir, errors)

    for name in RESET_PROGRESS_FILES:
        path = deck_dir / name
        if path.exists():
            append_error(errors, path, "must be absent after reset")

    return DeckArtifacts(
        deck_id,
        card_ids,
        settings,
        0,
        set(),
    )


def verify_m7_artifacts(
    sdmc: Path,
    deck_ids: list[str],
    required_events: list[str],
    expected_settings: list[tuple[str, int, int]],
    allow_missing_log: bool = False,
    reset_deck_ids: list[str] | None = None,
) -> list[str]:
    errors: list[str] = []
    deck_root = sdmc / APP_SD_DIR / "decks"
    decks: dict[str, DeckArtifacts] = {}
    all_events: set[str] = set()
    reset_deck_ids = reset_deck_ids or []

    for deck_id in sorted(set(deck_ids).intersection(reset_deck_ids)):
        errors.append(f"{deck_id}: cannot be both study and reset deck")

    for deck_id in deck_ids:
        artifacts = verify_deck_artifacts(
            deck_root,
            deck_id,
            errors,
            allow_missing_log,
        )
        if artifacts is None:
            continue

        decks[deck_id] = artifacts
        all_events.update(artifacts.review_log_events)

    for deck_id in reset_deck_ids:
        artifacts = verify_reset_deck_artifacts(deck_root, deck_id, errors)
        if artifacts is not None:
            decks[deck_id] = artifacts

    for required_event in required_events:
        if required_event not in VALID_REVIEW_LOG_EVENTS:
            errors.append(f"{required_event}: unknown required event")
        elif required_event not in all_events:
            errors.append(f"review-log.tsv: missing required {required_event} event")

    for deck_id, new_limit, review_limit in expected_settings:
        artifacts = decks.get(deck_id)
        if artifacts is None:
            continue
        if artifacts.settings.get("new_limit") != new_limit:
            errors.append(f"{deck_id}/settings.tsv: expected new_limit {new_limit}")
        if artifacts.settings.get("review_limit") != review_limit:
            errors.append(
                f"{deck_id}/settings.tsv: expected review_limit {review_limit}"
            )

    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--sdmc",
        type=Path,
        default=DEFAULT_SDMC,
        help="SDMC root containing 3ds/anki3ds/decks.",
    )
    parser.add_argument(
        "--deck",
        dest="decks",
        action="append",
        default=[],
        help="Deck id to verify. Can be passed more than once.",
    )
    parser.add_argument(
        "--no-study-decks",
        action="store_true",
        help="Do not default to the tracked sample study decks.",
    )
    parser.add_argument(
        "--expect-reset-deck",
        dest="reset_decks",
        action="append",
        default=[],
        help="Deck id expected to have reset progress while preserving settings.",
    )
    parser.add_argument(
        "--require-event",
        dest="required_events",
        action="append",
        default=[],
        choices=sorted(VALID_REVIEW_LOG_EVENTS),
        help="Review-log event that must appear across the checked decks.",
    )
    parser.add_argument(
        "--no-required-events",
        action="store_true",
        help="Do not require any review-log event types.",
    )
    parser.add_argument(
        "--expect-settings",
        action="append",
        default=[],
        type=parse_expected_settings,
        help="Expected saved settings as deck_id:new_limit:review_limit.",
    )
    parser.add_argument(
        "--allow-missing-review-log",
        action="store_true",
        help="Do not fail when review-log.tsv is missing.",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Print only validation errors.",
    )
    args = parser.parse_args()

    if args.no_required_events and args.required_events:
        parser.error("--no-required-events cannot be combined with --require-event")
    if args.no_study_decks and args.decks:
        parser.error("--no-study-decks cannot be combined with --deck")

    if args.no_study_decks:
        deck_ids = []
    else:
        deck_ids = args.decks if args.decks else list(DEFAULT_DECKS)
    if args.no_required_events:
        required_events = []
    else:
        required_events = (
            args.required_events if args.required_events else list(DEFAULT_REQUIRED_EVENTS)
        )
    sdmc = args.sdmc.expanduser()
    errors = verify_m7_artifacts(
        sdmc,
        deck_ids,
        required_events,
        args.expect_settings,
        args.allow_missing_review_log,
        args.reset_decks,
    )

    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1

    if not args.quiet:
        print(
            f"{sdmc}: ok - M7 artifacts verified for "
            f"{', '.join(deck_ids)}"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
