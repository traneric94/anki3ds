#!/usr/bin/env python3
"""Convert a simple tab-separated Anki export into an anki3ds deck."""

from __future__ import annotations

import argparse
import hashlib
from html.parser import HTMLParser
import json
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path

DECK_ID_MAX_LENGTH = 64
DECK_MAX_CARDS = 256
DECK_INDEX_MAX_DECKS = 64
DECK_MAX_ID_LENGTH = 32
DECK_MAX_NAME_LENGTH = 64
DECK_MAX_TEXT_LENGTH = 384
DECK_MAX_TAGS_LENGTH = 128
DECK_MAX_LINE_LENGTH = 1024
DECK_MAX_ROW_BYTES = DECK_MAX_LINE_LENGTH - 2
DEFAULT_SETTINGS = "new_limit\t20\nreview_limit\t200\n"
TEXT_ONLY_OBSOLETE_DIRECTORIES = ("media",)
STATE_FILE_HEADER = "#anki3ds-state-v1"
STATE_FILE_FOOTER = "#anki3ds-state-complete"
REVIEW_STATE_FILES = ("state.tsv", "state.tsv.tmp", "state.tsv.bak")
SETTINGS_FILES = ("settings.tsv", "settings.tsv.tmp", "settings.tsv.bak")

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


class HtmlImageDetector(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.found = False

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        del attrs
        if tag.lower() == "img":
            self.found = True


def escape_tsv_field(value: str) -> str:
    return value.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n")


def text_contains_image_tag(value: str) -> bool:
    parser = HtmlImageDetector()
    parser.feed(value)
    parser.close()
    return parser.found


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


def normalize_source_id(line_number: int, label: str, value: str) -> str:
    normalized = " ".join(value.split())
    if not normalized:
        raise ValueError(f"line {line_number}: {label} field is empty")

    return normalized


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


def validate_device_field(
    card_number: int,
    label: str,
    value: str,
    max_length: int,
) -> None:
    if len(value.encode("utf-8")) >= max_length:
        raise ValueError(
            f"card {card_number}: {label} exceeds {max_length - 1} UTF-8 bytes"
        )


def validate_deck_name(deck_name: str) -> None:
    if not deck_name:
        raise ValueError("deck name is required")
    if any(ord(character) < 32 or ord(character) == 127 for character in deck_name):
        raise ValueError("deck name cannot contain control characters")
    if len(deck_name.encode("utf-8")) >= DECK_MAX_NAME_LENGTH:
        raise ValueError(
            f"deck name exceeds {DECK_MAX_NAME_LENGTH - 1} UTF-8 bytes"
        )


def card_output_fields(card: ConvertedCard) -> list[str]:
    return [
        escape_tsv_field(card.card_id),
        escape_tsv_field(card.note_id),
        escape_tsv_field(card.front),
        escape_tsv_field(card.back),
        escape_tsv_field(card.tags),
    ]


def validate_device_cards(cards: list[ConvertedCard]) -> None:
    for card_number, card in enumerate(cards, start=1):
        validate_device_field(card_number, "card_id", card.card_id, DECK_MAX_ID_LENGTH)
        validate_device_field(card_number, "note_id", card.note_id, DECK_MAX_ID_LENGTH)
        validate_device_field(card_number, "front", card.front, DECK_MAX_TEXT_LENGTH)
        validate_device_field(card_number, "back", card.back, DECK_MAX_TEXT_LENGTH)
        validate_device_field(card_number, "tags", card.tags, DECK_MAX_TAGS_LENGTH)

        fields = card_output_fields(card)
        row = "\t".join(fields)
        if len(row.encode("utf-8")) > DECK_MAX_ROW_BYTES:
            raise ValueError(
                f"card {card_number}: row exceeds {DECK_MAX_ROW_BYTES} UTF-8 bytes"
            )


def convert_lines(
    lines: list[str],
    front_field: int,
    back_field: int,
    tags_field: int | None,
    front_media_field: int | None = None,
    back_media_field: int | None = None,
    card_id_field: int | None = None,
    note_id_field: int | None = None,
    text_only: bool = False,
) -> list[ConvertedCard]:
    cards: list[ConvertedCard] = []
    seen_card_ids: dict[str, int] = {}
    seen_note_ids: dict[str, int] = {}

    del text_only

    if front_media_field is not None or back_media_field is not None:
        raise ValueError("text flash-card decks cannot use media fields")

    for line_number, raw_line in enumerate(lines, start=1):
        line = raw_line.rstrip("\n\r")

        if not line:
            continue

        fields = line.split("\t")
        max_field = max(
            front_field,
            back_field,
            tags_field or 0,
            card_id_field or 0,
            note_id_field or 0,
        )
        if len(fields) <= max_field:
            raise ValueError(f"line {line_number}: expected at least {max_field + 1} fields")

        front_has_image = text_contains_image_tag(fields[front_field])
        back_has_image = text_contains_image_tag(fields[back_field])

        if front_has_image or back_has_image:
            raise ValueError(
                f"line {line_number}: text flash-card decks cannot include image tags"
            )

        front = normalize_text(fields[front_field])
        back = normalize_text(fields[back_field])
        tags = normalize_text(fields[tags_field]) if tags_field is not None else ""

        if not front:
            raise ValueError(f"line {line_number}: front field is empty")
        if not back:
            raise ValueError(f"line {line_number}: back field is empty")

        if card_id_field is not None:
            source_card_id = normalize_source_id(
                line_number,
                "card id",
                fields[card_id_field],
            )
            base_card_id = stable_id("card", source_card_id)
        else:
            source_card_id = ""

        if note_id_field is not None:
            source_note_id = normalize_source_id(
                line_number,
                "note id",
                fields[note_id_field],
            )
            base_note_id = stable_id("note", source_note_id)
        elif source_card_id:
            base_note_id = stable_id("note", source_card_id)
        else:
            base_note_id = stable_id("note", front, back, tags)

        if card_id_field is None:
            if note_id_field is not None:
                base_card_id = stable_id("card", source_note_id)
            else:
                base_card_id = stable_id("card", base_note_id, front, back)

        note_id = unique_id(base_note_id, seen_note_ids)
        card_id = unique_id(base_card_id, seen_card_ids)
        cards.append(ConvertedCard(card_id, note_id, front, back, tags))

    if not cards:
        raise ValueError("input did not contain any cards")

    return cards


def remove_path_if_present(path: Path) -> None:
    if path.is_symlink():
        path.unlink()
    elif path.is_dir():
        shutil.rmtree(path)
    elif path.exists():
        path.unlink()


def remove_obsolete_text_only_artifacts(output_dir: Path) -> None:
    for directory_name in TEXT_ONLY_OBSOLETE_DIRECTORIES:
        remove_path_if_present(output_dir / directory_name)


def commit_deck_payload_files(
    output_dir: Path,
    deck_json: dict[str, object],
    cards: list[ConvertedCard],
) -> None:
    deck_path = output_dir / "deck.json"
    cards_path = output_dir / "cards.tsv"
    temp_paths = {
        deck_path: output_dir / "deck.json.tmp",
        cards_path: output_dir / "cards.tsv.tmp",
    }
    backup_paths = {
        deck_path: output_dir / "deck.json.bak",
        cards_path: output_dir / "cards.tsv.bak",
    }
    committed_paths: list[Path] = []
    backed_up_paths: list[Path] = []
    cleanup_backups = True

    for path in list(temp_paths.values()) + list(backup_paths.values()):
        remove_path_if_present(path)

    try:
        temp_paths[deck_path].write_text(
            json.dumps(deck_json, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )

        with temp_paths[cards_path].open(
            "w",
            encoding="utf-8",
            newline="\n",
        ) as file:
            for card in cards:
                fields = card_output_fields(card)
                file.write("\t".join(fields))
                file.write("\n")

        for final_path in (deck_path, cards_path):
            temp_path = temp_paths[final_path]
            backup_path = backup_paths[final_path]

            if final_path.exists():
                final_path.replace(backup_path)
                backed_up_paths.append(final_path)

            temp_path.replace(final_path)
            committed_paths.append(final_path)
    except OSError:
        rollback_failed = False
        cleanup_backups = False

        for final_path in reversed(committed_paths):
            try:
                remove_path_if_present(final_path)
            except OSError:
                rollback_failed = True
        for final_path in reversed(backed_up_paths):
            backup_path = backup_paths[final_path]
            if backup_path.exists():
                try:
                    backup_path.replace(final_path)
                except OSError:
                    rollback_failed = True
        cleanup_backups = not rollback_failed
        raise
    finally:
        for path in temp_paths.values():
            remove_path_if_present(path)
        if cleanup_backups:
            for path in backup_paths.values():
                remove_path_if_present(path)

def write_deck(
    output_dir: Path,
    deck_id: str,
    deck_name: str,
    cards: list[ConvertedCard],
    media_root: Path | None = None,
) -> None:
    if media_root is not None:
        raise ValueError("text flash-card decks cannot use media options")
    if not deck_id_is_valid(output_dir.name):
        raise ValueError(
            "output deck folder name must use letters, numbers, '_' or '-'"
        )
    if not deck_id_is_valid(deck_id):
        raise ValueError("deck id must use letters, numbers, '_' or '-'")
    if deck_id != output_dir.name:
        raise ValueError("deck id must match output deck folder name")
    validate_deck_name(deck_name)
    if not cards:
        raise ValueError("deck must contain at least one card")
    if len(cards) > DECK_MAX_CARDS:
        raise ValueError(f"deck has more than {DECK_MAX_CARDS} cards")

    validate_device_cards(cards)

    output_dir.mkdir(parents=True, exist_ok=True)
    remove_obsolete_text_only_artifacts(output_dir)

    deck_json = {
        "format_version": 1,
        "deck_id": deck_id,
        "name": deck_name,
        "created_by": "anki3ds-converter",
        "card_count": len(cards),
    }

    commit_deck_payload_files(output_dir, deck_json, cards)

    settings_path = output_dir / "settings.tsv"
    if not settings_path.exists():
        settings_path.write_text(DEFAULT_SETTINGS, encoding="utf-8")


def split_deck_id(base_deck_id: str, chunk_index: int, chunk_count: int) -> str:
    width = max(2, len(str(chunk_count)))
    deck_id = f"{base_deck_id}-{chunk_index:0{width}d}"

    if not deck_id_is_valid(deck_id):
        raise ValueError(f"split deck id is too long: {deck_id}")

    return deck_id


def split_deck_name(deck_name: str, chunk_index: int, chunk_count: int) -> str:
    return f"{deck_name} {chunk_index}/{chunk_count}"


def split_chunk_name_is_for_base(name: str, base_deck_id: str) -> bool:
    prefix = f"{base_deck_id}-"

    if not name.startswith(prefix):
        return False

    suffix = name[len(prefix):]
    return suffix != "" and suffix.isdigit()


def converter_deck_dir_is_removable(deck_dir: Path) -> bool:
    deck_json_path = deck_dir / "deck.json"

    if not deck_dir.is_dir() or not deck_json_path.is_file():
        return False

    try:
        deck_json = json.loads(deck_json_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return False

    return (
        isinstance(deck_json, dict)
        and deck_json.get("created_by") == "anki3ds-converter"
        and deck_json.get("deck_id") == deck_dir.name
    )


def state_row_card_id(row: str) -> str:
    return row.split("\t", 1)[0]


def state_metadata_row_count(line: str, marker: str) -> int | None:
    fields = line.split("\t")

    if len(fields) != 2 or fields[0] != marker or not fields[1].isdigit():
        return None

    row_count = int(fields[1])
    if row_count > DECK_MAX_CARDS:
        return None

    return row_count


def parse_state_rows(state_text: str) -> tuple[bool, list[str]] | None:
    lines = state_text.splitlines()
    framed = (
        len(lines) >= 2
        and lines[0].startswith(f"{STATE_FILE_HEADER}\t")
        and lines[-1].startswith(f"{STATE_FILE_FOOTER}\t")
    )

    if framed:
        rows = lines[1:-1]
        header_row_count = state_metadata_row_count(lines[0], STATE_FILE_HEADER)
        footer_row_count = state_metadata_row_count(lines[-1], STATE_FILE_FOOTER)
        if (
            header_row_count is None
            or footer_row_count is None
            or header_row_count != len(rows)
            or footer_row_count != len(rows)
        ):
            return None
    else:
        rows = [line for line in lines if line and not line.startswith("#")]

    return framed, rows


def format_state_rows(rows: list[str], framed: bool) -> str:
    if framed:
        row_count = len(rows)
        return (
            "\n".join(
                [
                    f"{STATE_FILE_HEADER}\t{row_count}",
                    *rows,
                    f"{STATE_FILE_FOOTER}\t{row_count}",
                ]
            )
            + "\n"
        )

    return "\n".join(rows) + "\n"


def matching_state_rows_for_cards(
    state_text: str,
    target_card_ids: set[str],
) -> tuple[bool, list[str]] | None:
    parsed_state = parse_state_rows(state_text)

    if parsed_state is None:
        return None

    framed, rows = parsed_state
    return (
        framed,
        [row for row in rows if state_row_card_id(row) in target_card_ids],
    )


def filter_state_rows_for_cards(
    state_text: str,
    target_card_ids: set[str],
) -> str | None:
    matched_state = matching_state_rows_for_cards(state_text, target_card_ids)

    if matched_state is None:
        return None

    framed, matching_rows = matched_state
    if not matching_rows:
        return None

    return format_state_rows(matching_rows, framed)


def migrate_review_state_file(
    source_path: Path,
    target_path: Path,
    target_card_ids: set[str],
) -> None:
    if target_path.exists() or not source_path.is_file():
        return

    try:
        migrated_state = filter_state_rows_for_cards(
            source_path.read_text(encoding="utf-8"),
            target_card_ids,
        )
    except (OSError, UnicodeDecodeError):
        return

    if migrated_state is not None:
        target_path.write_text(migrated_state, encoding="utf-8")


def collect_matching_state_rows(
    source_dirs: list[Path],
    state_filename: str,
    target_card_ids: set[str],
) -> list[str]:
    matching_rows: list[str] = []
    seen_card_ids: set[str] = set()

    for source_dir in source_dirs:
        source_path = source_dir / state_filename

        if not source_path.is_file():
            continue

        try:
            matched_state = matching_state_rows_for_cards(
                source_path.read_text(encoding="utf-8"),
                target_card_ids,
            )
        except (OSError, UnicodeDecodeError):
            continue

        if matched_state is None:
            continue

        for row in matched_state[1]:
            card_id = state_row_card_id(row)

            if card_id in seen_card_ids:
                continue

            seen_card_ids.add(card_id)
            matching_rows.append(row)

    return matching_rows


def collect_state_rows(source_dirs: list[Path], state_filename: str) -> list[str]:
    rows: list[str] = []
    seen_card_ids: set[str] = set()

    for source_dir in source_dirs:
        source_path = source_dir / state_filename

        if not source_path.is_file():
            continue

        try:
            parsed_state = parse_state_rows(source_path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError):
            continue

        if parsed_state is None:
            continue

        for row in parsed_state[1]:
            card_id = state_row_card_id(row)

            if card_id in seen_card_ids:
                continue

            seen_card_ids.add(card_id)
            rows.append(row)

    return rows


def migrate_combined_review_state_file(
    source_dirs: list[Path],
    target_path: Path,
    state_filename: str,
    target_card_ids: set[str],
) -> None:
    matching_rows = collect_matching_state_rows(
        source_dirs,
        state_filename,
        target_card_ids,
    )
    if matching_rows:
        target_path.write_text(format_state_rows(matching_rows, True), encoding="utf-8")


def copy_settings_file(
    source_path: Path,
    target_path: Path,
    replace_existing: bool,
) -> None:
    if not source_path.is_file():
        return
    if target_path.exists() and not replace_existing:
        return

    shutil.copy2(source_path, target_path)


def migrate_single_deck_progress_to_split_outputs(
    source_dir: Path,
    chunk_cards: dict[Path, list[ConvertedCard]],
    existing_outputs: set[Path],
) -> None:
    if not converter_deck_dir_is_removable(source_dir):
        return

    for chunk_output, cards in chunk_cards.items():
        target_card_ids = {card.card_id for card in cards}
        replace_default_settings = chunk_output not in existing_outputs

        for state_filename in REVIEW_STATE_FILES:
            migrate_review_state_file(
                source_dir / state_filename,
                chunk_output / state_filename,
                target_card_ids,
            )

        for settings_filename in SETTINGS_FILES:
            copy_settings_file(
                source_dir / settings_filename,
                chunk_output / settings_filename,
                replace_default_settings,
            )


def migrate_split_progress_to_split_outputs(
    chunk_cards: dict[Path, list[ConvertedCard]],
    source_dirs: list[Path],
    existing_outputs: set[Path],
) -> None:
    if not source_dirs:
        return

    source_rows = {
        state_filename: collect_state_rows(source_dirs, state_filename)
        for state_filename in REVIEW_STATE_FILES
    }

    for chunk_output, cards in chunk_cards.items():
        target_card_ids = {card.card_id for card in cards}
        replace_default_settings = chunk_output not in existing_outputs

        for state_filename in REVIEW_STATE_FILES:
            matching_rows = [
                row
                for row in source_rows[state_filename]
                if state_row_card_id(row) in target_card_ids
            ]

            if matching_rows:
                (chunk_output / state_filename).write_text(
                    format_state_rows(matching_rows, True),
                    encoding="utf-8",
                )

        for settings_filename in SETTINGS_FILES:
            for source_dir in source_dirs:
                source_path = source_dir / settings_filename

                if not source_path.is_file():
                    continue

                copy_settings_file(
                    source_path,
                    chunk_output / settings_filename,
                    replace_default_settings,
                )
                break


def migrate_split_progress_to_single_output(
    output_dir: Path,
    cards: list[ConvertedCard],
    source_dirs: list[Path],
    replace_default_settings: bool,
) -> None:
    target_card_ids = {card.card_id for card in cards}

    if not source_dirs:
        return

    for state_filename in REVIEW_STATE_FILES:
        migrate_combined_review_state_file(
            source_dirs,
            output_dir / state_filename,
            state_filename,
            target_card_ids,
        )

    for settings_filename in SETTINGS_FILES:
        for source_dir in source_dirs:
            source_path = source_dir / settings_filename

            if not source_path.is_file():
                continue

            copy_settings_file(
                source_path,
                output_dir / settings_filename,
                replace_default_settings,
            )
            break


def obsolete_converter_split_outputs(
    output_dir: Path,
    deck_id: str,
    current_outputs: set[Path],
) -> list[Path]:
    parent = output_dir.parent
    candidates: list[Path] = []

    if output_dir not in current_outputs:
        candidates.append(output_dir)
    if parent.is_dir():
        for entry in parent.iterdir():
            if entry in current_outputs:
                continue
            if split_chunk_name_is_for_base(entry.name, deck_id):
                candidates.append(entry)

    return sorted(
        (
            candidate
            for candidate in candidates
            if converter_deck_dir_is_removable(candidate)
        ),
        key=lambda path: path.name,
    )


def existing_converter_split_outputs(output_dir: Path, deck_id: str) -> list[Path]:
    parent = output_dir.parent

    if not parent.is_dir():
        return []

    return sorted(
        (
            entry
            for entry in parent.iterdir()
            if split_chunk_name_is_for_base(entry.name, deck_id)
            and converter_deck_dir_is_removable(entry)
        ),
        key=lambda path: path.name,
    )


def remove_obsolete_split_outputs(
    output_dir: Path,
    deck_id: str,
    current_outputs: set[Path],
) -> None:
    for candidate in obsolete_converter_split_outputs(
        output_dir,
        deck_id,
        current_outputs,
    ):
        shutil.rmtree(candidate)


def write_split_decks(
    output_dir: Path,
    deck_id: str,
    deck_name: str,
    cards: list[ConvertedCard],
    media_root: Path | None = None,
) -> list[Path]:
    if len(cards) <= DECK_MAX_CARDS:
        old_split_outputs = obsolete_converter_split_outputs(
            output_dir,
            deck_id,
            {output_dir},
        )
        output_existed = output_dir.exists()
        write_deck(output_dir, deck_id, deck_name, cards, media_root)
        migrate_split_progress_to_single_output(
            output_dir,
            cards,
            old_split_outputs,
            not output_existed,
        )
        remove_obsolete_split_outputs(output_dir, deck_id, {output_dir})
        return [output_dir]

    old_split_outputs = existing_converter_split_outputs(output_dir, deck_id)
    if not deck_id_is_valid(output_dir.name):
        raise ValueError(
            "output deck folder name must use letters, numbers, '_' or '-'"
        )
    if not deck_id_is_valid(deck_id):
        raise ValueError("deck id must use letters, numbers, '_' or '-'")
    if deck_id != output_dir.name:
        raise ValueError("deck id must match output deck folder name")

    chunk_count = (len(cards) + DECK_MAX_CARDS - 1) // DECK_MAX_CARDS
    if chunk_count > DECK_INDEX_MAX_DECKS:
        raise ValueError(
            f"split output would create more than {DECK_INDEX_MAX_DECKS} deck folders"
        )

    chunk_cards_by_path: dict[Path, list[ConvertedCard]] = {}
    existing_outputs: set[Path] = set()
    written_paths: list[Path] = []

    for chunk_index in range(chunk_count):
        chunk_number = chunk_index + 1
        chunk_id = split_deck_id(deck_id, chunk_number, chunk_count)
        chunk_output = output_dir.parent / chunk_id
        chunk_cards = cards[
            chunk_index * DECK_MAX_CARDS:
            (chunk_index + 1) * DECK_MAX_CARDS
        ]

        if chunk_output.exists():
            existing_outputs.add(chunk_output)
        write_deck(
            chunk_output,
            chunk_id,
            split_deck_name(deck_name, chunk_number, chunk_count),
            chunk_cards,
            media_root,
        )
        chunk_cards_by_path[chunk_output] = chunk_cards
        written_paths.append(chunk_output)

    migrate_split_progress_to_split_outputs(
        chunk_cards_by_path,
        old_split_outputs,
        existing_outputs,
    )
    migrate_single_deck_progress_to_split_outputs(
        output_dir,
        chunk_cards_by_path,
        existing_outputs,
    )
    remove_obsolete_split_outputs(output_dir, deck_id, set(written_paths))

    return written_paths


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
    parser.add_argument(
        "--front-media-field",
        type=int,
        default=None,
        help=argparse.SUPPRESS,
    )
    parser.add_argument(
        "--back-media-field",
        type=int,
        default=None,
        help=argparse.SUPPRESS,
    )
    parser.add_argument(
        "--card-id-field",
        type=int,
        default=None,
        help="zero-based stable source card id field for preserving progress",
    )
    parser.add_argument(
        "--note-id-field",
        type=int,
        default=None,
        help="zero-based stable source note id field for preserving progress",
    )
    parser.add_argument(
        "--media-root",
        type=Path,
        default=None,
        help=argparse.SUPPRESS,
    )
    parser.add_argument(
        "--text-only",
        action="store_true",
        help="accepted for compatibility; text-only import is always enforced",
    )
    parser.add_argument(
        "--split-large-decks",
        action="store_true",
        help="write oversized exports as numbered sibling deck folders",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.front_field < 0 or args.back_field < 0:
        raise SystemExit("field indexes must be non-negative")
    if args.tags_field is not None and args.tags_field < 0:
        raise SystemExit("field indexes must be non-negative")
    if args.front_media_field is not None and args.front_media_field < 0:
        raise SystemExit("field indexes must be non-negative")
    if args.back_media_field is not None and args.back_media_field < 0:
        raise SystemExit("field indexes must be non-negative")
    if args.card_id_field is not None and args.card_id_field < 0:
        raise SystemExit("field indexes must be non-negative")
    if args.note_id_field is not None and args.note_id_field < 0:
        raise SystemExit("field indexes must be non-negative")
    if (
        args.front_media_field is not None
        or args.back_media_field is not None
        or args.media_root is not None
    ):
        raise SystemExit("text flash-card decks cannot use media options")

    try:
        lines = args.input.read_text(encoding="utf-8").splitlines()
        cards = convert_lines(
            lines,
            args.front_field,
            args.back_field,
            args.tags_field,
            None,
            None,
            args.card_id_field,
            args.note_id_field,
            True,
        )
        deck_id = args.deck_id if args.deck_id is not None else args.output.name
        if args.split_large_decks:
            written_paths = write_split_decks(
                args.output,
                deck_id,
                args.deck_name,
                cards,
                args.media_root,
            )
            print(f"wrote {len(cards)} cards to {len(written_paths)} deck folder(s)")
        else:
            write_deck(args.output, deck_id, args.deck_name, cards, args.media_root)
            print(f"wrote {len(cards)} cards to {args.output}")
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
