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
REQUIRED_SETTING_KEYS = frozenset(("new_limit", "review_limit"))
OPTIONAL_SETTING_KEYS = frozenset(("learning_mode",))
SETTING_KEYS = REQUIRED_SETTING_KEYS | OPTIONAL_SETTING_KEYS
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
COMPACT_STATE_REQUIRED_KEYS = (
    "version",
    "card_count",
    "current_index",
    "reviewed_count",
    "again_count",
    "hard_count",
    "good_count",
    "easy_count",
)
COMPACT_STATE_OPTIONAL_KEYS = (
    "progress_day",
    "reviewed_today_count",
    "introduced_count",
    "introduced_today_count",
    "completed_today_count",
    "suspended_count",
)
COMPACT_STATE_REPEATED_KEYS = (
    "introduced_index",
    "schedule_index",
    "schedule_due_day",
    "schedule_interval_days",
    "completed_today_index",
    "suspended_index",
)
REVIEW_LOG_FIELD_COUNT = 21
COMPACT_REVIEW_LOG_FIELD_COUNT = 6
SESSION_HEADER = "#anki3ds-session-v1"
SESSION_FOOTER = "#anki3ds-session-complete"
VALID_SESSION_EVENTS = frozenset(
    (
        "boot",
        "scan",
        "deck_open",
        "deck_review",
        "deck_summary",
        "deck_load_error",
        "answer_shown",
        "rating_saved",
        "undo_saved",
        "suspend_saved",
        "restore_saved",
        "settings_saved",
        "reset_progress",
        "exit_confirmed",
    )
)
SESSION_TIME_KEYS = frozenset(("started_at", "updated_at"))
SESSION_DAY_KEYS = frozenset(("started_day", "current_day"))
SESSION_BOOLEAN_KEYS = frozenset(("scan_completed", "exit_confirmed"))
SESSION_COUNTER_KEYS = frozenset(
    (
        "launch_count",
        "deck_count",
        "ignored_count",
        "deck_open_count",
        "review_screen_count",
        "summary_screen_count",
        "load_error_count",
        "answer_shown_count",
        "rating_saved_count",
        "undo_saved_count",
        "suspend_saved_count",
        "restore_saved_count",
        "settings_saved_count",
        "reset_progress_count",
    )
)
SESSION_TEXT_KEYS = frozenset(("last_deck_id", "last_event"))
SESSION_REQUIRED_KEYS = (
    "started_at",
    "updated_at",
    "launch_count",
    "started_day",
    "current_day",
    "scan_completed",
    "deck_count",
    "ignored_count",
    "deck_open_count",
    "review_screen_count",
    "summary_screen_count",
    "load_error_count",
    "answer_shown_count",
    "rating_saved_count",
    "undo_saved_count",
    "suspend_saved_count",
    "restore_saved_count",
    "settings_saved_count",
    "reset_progress_count",
    "exit_confirmed",
    "last_deck_id",
    "last_event",
)
SESSION_EVENT_COUNTER_KEYS = {
    "rating": "rating_saved_count",
    "undo": "undo_saved_count",
    "suspend": "suspend_saved_count",
    "restore": "restore_saved_count",
}
RESET_PROGRESS_FILES = (
    "state.tsv",
    "state.tsv.tmp",
    "state.tsv.bak",
    "review-log.tsv",
    "review-log.tsv.tmp",
    "review-log.tsv.bak",
)
ExpectedSettings = tuple[str, int, int] | tuple[str, int, int, int | None]


class DeckArtifacts:
    def __init__(
        self,
        deck_id: str,
        card_ids: set[str],
        settings: dict[str, int],
        reviewed_or_suspended_count: int,
        state_progress_day: int | None,
        review_log_event_counts: dict[str, int],
    ) -> None:
        self.deck_id = deck_id
        self.card_ids = card_ids
        self.settings = settings
        self.reviewed_or_suspended_count = reviewed_or_suspended_count
        self.state_progress_day = state_progress_day
        self.review_log_event_counts = review_log_event_counts


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
        elif key == "learning_mode" and value > 1:
            append_error(errors, path, f"line {line_number}: bad learning_mode")
        else:
            settings[key] = value

    for key in sorted(REQUIRED_SETTING_KEYS):
        if key not in settings:
            append_error(errors, path, f"missing {key}")

    return settings


def state_row_is_reviewed_or_suspended(fields: list[str]) -> bool:
    review_count = parse_unsigned(fields[1], MAX_REVIEW_COUNT)
    suspended = fields[7]

    return (review_count is not None and review_count > 0) or suspended == "1"


def load_scheduler_state_rows(
    path: Path,
    rows: list[str],
    card_ids: set[str],
    errors: list[str],
) -> tuple[int, int | None]:
    seen: set[str] = set()
    reviewed_or_suspended_count = 0

    if len(rows) < 2:
        append_error(errors, path, "must include header, rows, and footer")
        return 0, None

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

    return reviewed_or_suspended_count, None


def load_compact_state_rows(
    path: Path,
    rows: list[str],
    card_ids: set[str],
    errors: list[str],
) -> tuple[int, int | None]:
    values: dict[str, int] = {}
    introduced_indices: list[int] = []
    schedule_indices: list[int] = []
    schedule_due_days: list[int] = []
    schedule_interval_days: list[int] = []
    completed_today_indices: list[int] = []
    suspended_indices: list[int] = []
    allowed_single_keys = set(COMPACT_STATE_REQUIRED_KEYS) | set(
        COMPACT_STATE_OPTIONAL_KEYS
    )
    allowed_repeated_keys = set(COMPACT_STATE_REPEATED_KEYS)

    for line_number, row in enumerate(rows, start=1):
        if row == "" or row.startswith("#"):
            continue

        fields = row.split("\t")
        if len(fields) != 2:
            append_error(errors, path, f"line {line_number}: expected 2 fields")
            continue

        key, value_text = fields
        value = parse_unsigned(value_text, None)
        if key in allowed_single_keys:
            if key in values:
                append_error(errors, path, f"line {line_number}: duplicate {key}")
            elif value is None:
                append_error(errors, path, f"line {line_number}: bad {key}")
            else:
                values[key] = value
        elif key in allowed_repeated_keys:
            if value is None:
                append_error(errors, path, f"line {line_number}: bad {key}")
            elif key == "introduced_index":
                introduced_indices.append(value)
            elif key == "schedule_index":
                schedule_indices.append(value)
            elif key == "schedule_due_day":
                schedule_due_days.append(value)
            elif key == "schedule_interval_days":
                schedule_interval_days.append(value)
            elif key == "completed_today_index":
                completed_today_indices.append(value)
            else:
                suspended_indices.append(value)
        else:
            append_error(errors, path, f"line {line_number}: unknown state key")

    for key in COMPACT_STATE_REQUIRED_KEYS:
        if key not in values:
            append_error(errors, path, f"missing {key}")

    version = values.get("version")
    card_count = values.get("card_count")
    current_index = values.get("current_index")
    progress_day = values.get("progress_day")
    reviewed_count = values.get("reviewed_count")
    reviewed_today_count = values.get("reviewed_today_count")
    introduced_count = values.get("introduced_count")
    introduced_today_count = values.get("introduced_today_count")
    completed_today_count = values.get("completed_today_count")
    again_count = values.get("again_count")
    hard_count = values.get("hard_count")
    good_count = values.get("good_count")
    easy_count = values.get("easy_count")
    suspended_count = values.get("suspended_count")

    if version is not None and version not in (1, 2):
        append_error(errors, path, "bad compact state version")
    if version == 1 and (
        schedule_indices or schedule_due_days or schedule_interval_days
    ):
        append_error(errors, path, "compact state schedule requires version 2")
    if card_count is not None and card_count != len(card_ids):
        append_error(errors, path, "compact state card_count mismatch")
    if (
        card_count is not None
        and current_index is not None
        and current_index > card_count
    ):
        append_error(errors, path, "compact state current_index outside deck")
    if progress_day is not None and progress_day > MAX_DAY:
        append_error(errors, path, "compact state progress_day too high")
    if reviewed_count is not None and reviewed_count > MAX_REVIEW_COUNT:
        append_error(errors, path, "compact state reviewed_count too high")
    if reviewed_today_count is not None:
        if reviewed_today_count > MAX_REVIEW_COUNT:
            append_error(errors, path, "compact state reviewed_today_count too high")
        if reviewed_count is not None and reviewed_today_count > reviewed_count:
            append_error(errors, path, "compact state reviewed_today_count above total")
    if introduced_count is not None:
        if card_count is not None and introduced_count > card_count:
            append_error(errors, path, "compact state introduced_count outside deck")
        if introduced_count != len(introduced_indices):
            append_error(errors, path, "compact state introduced_count mismatch")
    elif introduced_indices:
        append_error(errors, path, "compact state introduced_index without count")

    has_schedule_rows = (
        bool(schedule_indices)
        or bool(schedule_due_days)
        or bool(schedule_interval_days)
    )
    if has_schedule_rows:
        if introduced_count is None:
            append_error(errors, path, "compact state schedule without introduced_count")
        if not (
            len(schedule_indices)
            == len(schedule_due_days)
            == len(schedule_interval_days)
        ):
            append_error(errors, path, "compact state schedule count mismatch")

    if introduced_today_count is not None:
        if card_count is not None and introduced_today_count > card_count:
            append_error(
                errors,
                path,
                "compact state introduced_today_count outside deck",
            )
        if introduced_count is not None and introduced_today_count > introduced_count:
            append_error(
                errors,
                path,
                "compact state introduced_today_count above total",
            )

    if completed_today_count is not None:
        if introduced_count is None:
            append_error(
                errors,
                path,
                "compact state completed_today_count without introduced_count",
            )
        if card_count is not None and completed_today_count > card_count:
            append_error(
                errors,
                path,
                "compact state completed_today_count outside deck",
            )
        if completed_today_count != len(completed_today_indices):
            append_error(
                errors,
                path,
                "compact state completed_today_count mismatch",
            )
        if introduced_count is not None and completed_today_count > introduced_count:
            append_error(
                errors,
                path,
                "compact state completed_today_count above introduced",
            )
        if (
            reviewed_today_count is not None
            and completed_today_count > reviewed_today_count
        ):
            append_error(
                errors,
                path,
                "compact state completed_today_count above reviewed today",
            )
    elif completed_today_indices:
        append_error(errors, path, "compact state completed_today_index without count")

    if suspended_count is not None:
        if card_count is not None and suspended_count > card_count:
            append_error(errors, path, "compact state suspended_count outside deck")
        if suspended_count != len(suspended_indices):
            append_error(errors, path, "compact state suspended_count mismatch")
    elif suspended_indices:
        append_error(errors, path, "compact state suspended_index without count")

    if reviewed_count is not None and None not in (
        again_count,
        hard_count,
        good_count,
        easy_count,
    ):
        rating_total = (
            int(again_count)
            + int(hard_count)
            + int(good_count)
            + int(easy_count)
        )
        if rating_total != reviewed_count:
            append_error(errors, path, "compact state rating counts mismatch")

    seen_introduced_indices: set[int] = set()
    for card_index in introduced_indices:
        if card_count is not None and card_index >= card_count:
            append_error(errors, path, "compact state introduced_index outside deck")
        elif card_index in seen_introduced_indices:
            append_error(errors, path, "compact state duplicate introduced_index")
        else:
            seen_introduced_indices.add(card_index)

    seen_schedule_indices: set[int] = set()
    for card_index in schedule_indices:
        if card_count is not None and card_index >= card_count:
            append_error(errors, path, "compact state schedule_index outside deck")
        elif card_index in seen_schedule_indices:
            append_error(errors, path, "compact state duplicate schedule_index")
        else:
            seen_schedule_indices.add(card_index)

        if introduced_count is not None and card_index not in seen_introduced_indices:
            append_error(errors, path, "compact state schedule_index not introduced")

    for due_day in schedule_due_days:
        if due_day > MAX_DAY:
            append_error(errors, path, "compact state schedule_due_day too high")

    for interval_days in schedule_interval_days:
        if interval_days > MAX_INTERVAL_DAYS:
            append_error(errors, path, "compact state schedule_interval_days too high")

    seen_completed_today_indices: set[int] = set()
    for card_index in completed_today_indices:
        if card_count is not None and card_index >= card_count:
            append_error(errors, path, "compact state completed_today_index outside deck")
        elif card_index in seen_completed_today_indices:
            append_error(errors, path, "compact state duplicate completed_today_index")
        elif introduced_count is not None and card_index not in seen_introduced_indices:
            append_error(
                errors,
                path,
                "compact state completed_today_index not introduced",
            )
        else:
            seen_completed_today_indices.add(card_index)

    seen_suspended_indices: set[int] = set()
    for card_index in suspended_indices:
        if card_count is not None and card_index >= card_count:
            append_error(errors, path, "compact state suspended_index outside deck")
        elif card_index in seen_suspended_indices:
            append_error(errors, path, "compact state duplicate suspended_index")
        else:
            seen_suspended_indices.add(card_index)

    reviewed_or_suspended_count = (
        (reviewed_count or 0)
        + len(introduced_indices)
        + len(suspended_indices)
    )
    if reviewed_or_suspended_count == 0:
        append_error(
            errors,
            path,
            "no reviewed, introduced, or suspended cards found",
        )

    return reviewed_or_suspended_count, progress_day


def load_state(
    deck_dir: Path,
    card_ids: set[str],
    errors: list[str],
) -> tuple[int, int | None]:
    path = deck_dir / "state.tsv"
    rows = read_required_rows(path, errors)
    if rows is None:
        return 0, None
    if rows and rows[0].split("\t")[0] == STATE_HEADER:
        return load_scheduler_state_rows(path, rows, card_ids, errors)

    return load_compact_state_rows(path, rows, card_ids, errors)


def validate_review_log_common_fields(
    path: Path,
    line_number: int,
    event: str,
    rating: str,
    errors: list[str],
) -> None:
    if event not in VALID_REVIEW_LOG_EVENTS:
        append_error(errors, path, f"line {line_number}: bad event")
        return

    if rating not in VALID_REVIEW_LOG_RATINGS:
        append_error(errors, path, f"line {line_number}: bad rating")
    if event == "rating" and rating == "-":
        append_error(errors, path, f"line {line_number}: rating event needs rating")
    if event != "rating" and rating != "-":
        append_error(errors, path, f"line {line_number}: non-rating event needs -")


def load_review_log(
    deck_dir: Path,
    card_ids: set[str],
    errors: list[str],
    allow_missing: bool,
) -> dict[str, int]:
    path = deck_dir / "review-log.tsv"
    if allow_missing and not path.exists():
        return {}

    rows = read_required_rows(path, errors)
    if rows is None:
        return {}
    event_counts: dict[str, int] = {}

    if len(rows) == 0:
        append_error(errors, path, "must contain at least one complete row")
        return event_counts

    for line_number, row in enumerate(rows, start=1):
        fields = row.split("\t")
        if len(fields) == REVIEW_LOG_FIELD_COUNT:
            event = fields[2]
            card_id = fields[3]
            rating = fields[4]
            validate_review_log_common_fields(
                path,
                line_number,
                event,
                rating,
                errors,
            )
            if event in VALID_REVIEW_LOG_EVENTS:
                event_counts[event] = event_counts.get(event, 0) + 1
            if card_id not in card_ids:
                append_error(errors, path, f"line {line_number}: unknown card id")

            if parse_unsigned(fields[0], MAX_TIMESTAMP) is None:
                append_error(errors, path, f"line {line_number}: bad timestamp")
            if parse_unsigned(fields[1], MAX_DAY) is None:
                append_error(errors, path, f"line {line_number}: bad day")
            validate_scheduler_snapshot(fields, 5, path, line_number, errors)
            validate_scheduler_snapshot(fields, 13, path, line_number, errors)
            continue

        if len(fields) == COMPACT_REVIEW_LOG_FIELD_COUNT:
            event = fields[1]
            rating = fields[2]
            validate_review_log_common_fields(
                path,
                line_number,
                event,
                rating,
                errors,
            )
            if event in VALID_REVIEW_LOG_EVENTS:
                event_counts[event] = event_counts.get(event, 0) + 1

            if parse_unsigned(fields[0], MAX_TIMESTAMP) is None:
                append_error(errors, path, f"line {line_number}: bad timestamp")
            card_index = parse_unsigned(fields[3])
            if (
                card_index is None
                or card_index >= len(card_ids)
                or len(card_ids) == 0
            ):
                append_error(errors, path, f"line {line_number}: bad card_index")
            if parse_unsigned(fields[4], MAX_REVIEW_COUNT) is None:
                append_error(errors, path, f"line {line_number}: bad reviewed_count")
            suspended_count = parse_unsigned(fields[5])
            if suspended_count is None or suspended_count > len(card_ids):
                append_error(errors, path, f"line {line_number}: bad suspended_count")
            continue

        append_error(
            errors,
            path,
            f"line {line_number}: expected 21 or 6 fields",
        )

    return event_counts


def load_session(sdmc: Path, errors: list[str]) -> dict[str, str]:
    path = sdmc / APP_SD_DIR / "session.tsv"
    rows = read_required_rows(path, errors)
    if rows is None:
        return {}
    values: dict[str, str] = {}

    if len(rows) < 2:
        append_error(errors, path, "must include header, rows, and footer")
        return values
    if rows[0] != SESSION_HEADER:
        append_error(errors, path, "bad session header")
    if rows[-1] != SESSION_FOOTER:
        append_error(errors, path, "bad session footer")

    valid_keys = (
        SESSION_TIME_KEYS
        | SESSION_DAY_KEYS
        | SESSION_BOOLEAN_KEYS
        | SESSION_COUNTER_KEYS
        | SESSION_TEXT_KEYS
    )
    for line_number, row in enumerate(rows[1:-1], start=2):
        fields = row.split("\t")
        if len(fields) != 2:
            append_error(errors, path, f"line {line_number}: expected 2 fields")
            continue

        key, value = fields
        if key not in valid_keys:
            append_error(errors, path, f"line {line_number}: unknown session key")
            continue
        if key in values:
            append_error(errors, path, f"line {line_number}: duplicate session key")
            continue

        values[key] = value
        if key in SESSION_TIME_KEYS:
            if parse_unsigned(value, MAX_TIMESTAMP) is None:
                append_error(errors, path, f"line {line_number}: bad {key}")
        elif key in SESSION_DAY_KEYS:
            if parse_unsigned(value, MAX_DAY) is None:
                append_error(errors, path, f"line {line_number}: bad {key}")
        elif key in SESSION_BOOLEAN_KEYS:
            if parse_unsigned(value, 1) is None:
                append_error(errors, path, f"line {line_number}: bad {key}")
        elif key in SESSION_COUNTER_KEYS:
            if parse_unsigned(value, MAX_LIMIT) is None:
                append_error(errors, path, f"line {line_number}: bad {key}")
        elif key == "last_event" and value not in VALID_SESSION_EVENTS:
            append_error(errors, path, f"line {line_number}: bad last_event")
        elif key == "last_deck_id" and value == "":
            append_error(errors, path, f"line {line_number}: empty last_deck_id")

    for key in SESSION_REQUIRED_KEYS:
        if key not in values:
            append_error(errors, path, f"missing {key}")

    return values


def session_unsigned(session: dict[str, str], key: str) -> int:
    value = session.get(key)
    if value is None:
        return 0
    parsed = parse_unsigned(value, MAX_LIMIT)
    return 0 if parsed is None else parsed


def session_parsed_unsigned(
    session: dict[str, str],
    key: str,
    maximum: int,
) -> int | None:
    value = session.get(key)
    if value is None:
        return None

    return parse_unsigned(value, maximum)


def verify_session(
    session: dict[str, str],
    checked_deck_ids: set[str],
    required_events: list[str],
    review_log_event_counts: dict[str, int],
    allow_missing_log: bool,
    expected_settings: list[ExpectedSettings],
    reset_deck_ids: list[str],
    errors: list[str],
) -> None:
    if not session:
        return

    started_at = session_parsed_unsigned(session, "started_at", MAX_TIMESTAMP)
    updated_at = session_parsed_unsigned(session, "updated_at", MAX_TIMESTAMP)
    if started_at is not None and updated_at is not None and updated_at < started_at:
        errors.append("session.tsv: updated_at before started_at")

    started_day = session_parsed_unsigned(session, "started_day", MAX_DAY)
    current_day = session_parsed_unsigned(session, "current_day", MAX_DAY)
    if started_day is not None and current_day is not None and current_day < started_day:
        errors.append("session.tsv: current_day before started_day")

    if session.get("scan_completed") != "1":
        errors.append("session.tsv: scan_completed must be 1")
    if session.get("exit_confirmed") != "1":
        errors.append("session.tsv: exit_confirmed must be 1")
    if session.get("last_event") != "exit_confirmed":
        errors.append("session.tsv: last_event must prove confirmed exit")
    if session_unsigned(session, "launch_count") < 2:
        errors.append("session.tsv: launch_count must prove relaunch")
    if session_unsigned(session, "deck_open_count") < len(checked_deck_ids):
        errors.append("session.tsv: deck_open_count below checked deck count")
    if session_unsigned(session, "deck_count") < len(checked_deck_ids):
        errors.append("session.tsv: deck_count below checked deck count")
    deck_open_count = session_unsigned(session, "deck_open_count")
    deck_open_outcome_count = (
        session_unsigned(session, "review_screen_count")
        + session_unsigned(session, "summary_screen_count")
        + session_unsigned(session, "load_error_count")
    )
    if deck_open_outcome_count > deck_open_count:
        errors.append("session.tsv: deck-open outcome counters exceed deck_open_count")
    last_deck_id = session.get("last_deck_id")
    if last_deck_id is not None and last_deck_id not in checked_deck_ids:
        errors.append("session.tsv: last_deck_id outside checked decks")

    for required_event in required_events:
        counter_key = SESSION_EVENT_COUNTER_KEYS.get(required_event)
        if counter_key is None:
            continue
        if session_unsigned(session, counter_key) == 0:
            errors.append(f"session.tsv: missing {counter_key}")

    if not allow_missing_log and not reset_deck_ids:
        for event, counter_key in SESSION_EVENT_COUNTER_KEYS.items():
            session_count = session_unsigned(session, counter_key)
            log_count = review_log_event_counts.get(event, 0)
            if session_count > log_count:
                errors.append(
                    f"session.tsv: {counter_key} exceeds review-log {event} events"
                )

    if "rating" in required_events:
        if session_unsigned(session, "review_screen_count") == 0:
            errors.append("session.tsv: missing review_screen_count")
        if session_unsigned(session, "answer_shown_count") == 0:
            errors.append("session.tsv: missing answer_shown_count")
        if (
            session_unsigned(session, "answer_shown_count") <
            session_unsigned(session, "rating_saved_count")
        ):
            errors.append("session.tsv: answer_shown_count below rating_saved_count")

    if session_unsigned(session, "settings_saved_count") == 0:
        errors.append("session.tsv: missing settings_saved_count")
    if (
        reset_deck_ids
        and session_unsigned(session, "reset_progress_count") < len(reset_deck_ids)
    ):
        errors.append("session.tsv: reset_progress_count below reset deck count")


def parse_expected_settings(value: str) -> ExpectedSettings:
    parts = value.split(":")
    if len(parts) not in (3, 4):
        raise argparse.ArgumentTypeError(
            "use deck_id:new_limit:review_limit[:learning_mode]"
        )
    if parts[0] == "":
        raise argparse.ArgumentTypeError("deck_id must not be empty")

    new_limit = parse_unsigned(parts[1])
    review_limit = parse_unsigned(parts[2])
    if new_limit is None or review_limit is None:
        raise argparse.ArgumentTypeError("settings values must be unsigned")

    if len(parts) == 3:
        return parts[0], new_limit, review_limit, None

    learning_mode = parse_unsigned(parts[3], 1)
    if learning_mode is None:
        raise argparse.ArgumentTypeError("learning_mode must be 0 or 1")

    return parts[0], new_limit, review_limit, learning_mode


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
    reviewed_or_suspended_count, state_progress_day = load_state(
        deck_dir,
        card_ids,
        errors,
    )
    events = load_review_log(deck_dir, card_ids, errors, allow_missing_log)

    return DeckArtifacts(
        deck_id,
        card_ids,
        settings,
        reviewed_or_suspended_count,
        state_progress_day,
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
        None,
        {},
    )


def verify_compact_state_days(
    session: dict[str, str],
    decks: dict[str, DeckArtifacts],
    errors: list[str],
) -> None:
    current_day = session_parsed_unsigned(session, "current_day", MAX_DAY)
    if current_day is None:
        return

    for deck_id, artifacts in sorted(decks.items()):
        if artifacts.state_progress_day is None:
            continue
        if artifacts.state_progress_day != current_day:
            errors.append(
                f"{deck_id}/state.tsv: compact state progress_day "
                "does not match session current_day"
            )


def verify_m7_artifacts(
    sdmc: Path,
    deck_ids: list[str],
    required_events: list[str],
    expected_settings: list[ExpectedSettings],
    allow_missing_log: bool = False,
    reset_deck_ids: list[str] | None = None,
) -> list[str]:
    errors: list[str] = []
    deck_root = sdmc / APP_SD_DIR / "decks"
    decks: dict[str, DeckArtifacts] = {}
    review_log_event_counts: dict[str, int] = {}
    reset_deck_ids = reset_deck_ids or []
    checked_deck_ids = set(deck_ids) | set(reset_deck_ids)
    session = load_session(sdmc, errors)

    if len(checked_deck_ids) == 0:
        errors.append("no decks selected for verification")
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
        for event, count in artifacts.review_log_event_counts.items():
            review_log_event_counts[event] = (
                review_log_event_counts.get(event, 0) + count
            )

    for deck_id in reset_deck_ids:
        artifacts = verify_reset_deck_artifacts(deck_root, deck_id, errors)
        if artifacts is not None:
            decks[deck_id] = artifacts

    for required_event in required_events:
        if required_event not in VALID_REVIEW_LOG_EVENTS:
            errors.append(f"{required_event}: unknown required event")
        elif review_log_event_counts.get(required_event, 0) == 0:
            errors.append(f"review-log.tsv: missing required {required_event} event")

    if checked_deck_ids and not expected_settings:
        errors.append(
            "settings expectations required; pass --expect-settings "
            "deck_id:new_limit:review_limit[:learning_mode] for the saved "
            "daily-limit edits"
        )

    for expected in expected_settings:
        deck_id, new_limit, review_limit = expected[:3]
        learning_mode = expected[3] if len(expected) > 3 else None
        if deck_id not in checked_deck_ids:
            errors.append(f"{deck_id}/settings.tsv: expected settings deck not selected")
            continue
        artifacts = decks.get(deck_id)
        if artifacts is None:
            continue
        if artifacts.settings.get("new_limit") != new_limit:
            errors.append(f"{deck_id}/settings.tsv: expected new_limit {new_limit}")
        if artifacts.settings.get("review_limit") != review_limit:
            errors.append(
                f"{deck_id}/settings.tsv: expected review_limit {review_limit}"
            )
        if learning_mode is not None:
            actual_learning_mode = artifacts.settings.get("learning_mode", 0)
            if actual_learning_mode != learning_mode:
                errors.append(
                    f"{deck_id}/settings.tsv: expected learning_mode {learning_mode}"
                )

    verify_session(
        session,
        checked_deck_ids,
        required_events,
        review_log_event_counts,
        allow_missing_log,
        expected_settings,
        reset_deck_ids,
        errors,
    )
    verify_compact_state_days(session, decks, errors)

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
        help=(
            "Expected saved settings as "
            "deck_id:new_limit:review_limit[:learning_mode]."
        ),
    )
    parser.add_argument(
        "--allow-missing-review-log",
        action="store_true",
        help=(
            "Allow incomplete diagnostic review-log evidence, such as missing "
            "review-log.tsv or in-app 'log skipped' gaps."
        ),
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
