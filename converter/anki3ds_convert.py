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
DECK_MAX_CARDS = 256
DECK_INDEX_MAX_DECKS = 64
DECK_MAX_ID_LENGTH = 32
DECK_MAX_TEXT_LENGTH = 384
DECK_MAX_TAGS_LENGTH = 128
DECK_MAX_MEDIA_NAME_LENGTH = 96
DECK_MAX_LINE_LENGTH = 1024
DECK_MAX_ROW_BYTES = DECK_MAX_LINE_LENGTH - 2
MEDIA_IMAGE_MAX_WIDTH = 160
MEDIA_IMAGE_MAX_HEIGHT = 72
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
    front_media: str = ""
    back_media: str = ""


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


def media_name_is_valid(value: str) -> bool:
    if not value:
        return True
    if len(value.encode("utf-8")) >= DECK_MAX_MEDIA_NAME_LENGTH:
        return False
    if value.startswith("."):
        return False

    return all(
        character.isascii()
        and (character.isalnum() or character in "_-.")
        for character in value
    )


def normalize_media_name(value: str) -> str:
    name = " ".join(value.split())
    if not media_name_is_valid(name):
        raise ValueError(
            "media filenames must be plain filenames under 96 UTF-8 bytes"
        )
    return name


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


def card_output_fields(
    card: ConvertedCard,
    media_names: dict[str, str],
    has_media: bool,
) -> list[str]:
    fields = [
        escape_tsv_field(card.card_id),
        escape_tsv_field(card.note_id),
        escape_tsv_field(card.front),
        escape_tsv_field(card.back),
        escape_tsv_field(card.tags),
    ]
    if has_media:
        fields.extend(
            [
                escape_tsv_field(media_names.get(card.front_media, card.front_media)),
                escape_tsv_field(media_names.get(card.back_media, card.back_media)),
            ]
        )

    return fields


def validate_device_cards(
    cards: list[ConvertedCard],
    media_names: dict[str, str],
    has_media: bool,
) -> None:
    for card_number, card in enumerate(cards, start=1):
        front_media = media_names.get(card.front_media, card.front_media)
        back_media = media_names.get(card.back_media, card.back_media)

        validate_device_field(card_number, "card_id", card.card_id, DECK_MAX_ID_LENGTH)
        validate_device_field(card_number, "note_id", card.note_id, DECK_MAX_ID_LENGTH)
        validate_device_field(card_number, "front", card.front, DECK_MAX_TEXT_LENGTH)
        validate_device_field(card_number, "back", card.back, DECK_MAX_TEXT_LENGTH)
        validate_device_field(card_number, "tags", card.tags, DECK_MAX_TAGS_LENGTH)
        validate_device_field(
            card_number,
            "front_media",
            front_media,
            DECK_MAX_MEDIA_NAME_LENGTH,
        )
        validate_device_field(
            card_number,
            "back_media",
            back_media,
            DECK_MAX_MEDIA_NAME_LENGTH,
        )

        fields = card_output_fields(card, media_names, has_media)
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
) -> list[ConvertedCard]:
    cards: list[ConvertedCard] = []
    seen_card_ids: dict[str, int] = {}
    seen_note_ids: dict[str, int] = {}

    for line_number, raw_line in enumerate(lines, start=1):
        line = raw_line.rstrip("\n\r")

        if not line:
            continue

        fields = line.split("\t")
        max_field = max(
            front_field,
            back_field,
            tags_field or 0,
            front_media_field or 0,
            back_media_field or 0,
            card_id_field or 0,
            note_id_field or 0,
        )
        if len(fields) <= max_field:
            raise ValueError(f"line {line_number}: expected at least {max_field + 1} fields")

        front = normalize_text(fields[front_field])
        back = normalize_text(fields[back_field])
        tags = normalize_text(fields[tags_field]) if tags_field is not None else ""
        front_media = (
            normalize_media_name(fields[front_media_field])
            if front_media_field is not None
            else ""
        )
        back_media = (
            normalize_media_name(fields[back_media_field])
            if back_media_field is not None
            else ""
        )

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
        cards.append(
            ConvertedCard(card_id, note_id, front, back, tags, front_media, back_media)
        )

    if not cards:
        raise ValueError("input did not contain any cards")

    return cards


def read_ppm_token(content: bytes, offset: int) -> tuple[str, int]:
    while offset < len(content):
        value = content[offset]
        if value == ord("#"):
            while offset < len(content) and content[offset] not in b"\r\n":
                offset += 1
        elif chr(value).isspace():
            offset += 1
        else:
            break

    start = offset
    while offset < len(content) and not chr(content[offset]).isspace():
        offset += 1

    if start == offset:
        raise ValueError("bad PPM header")

    return content[start:offset].decode("ascii"), offset


def load_ppm_rgb(path: Path) -> tuple[int, int, bytes]:
    content = path.read_bytes()
    magic, offset = read_ppm_token(content, 0)
    width_text, offset = read_ppm_token(content, offset)
    height_text, offset = read_ppm_token(content, offset)
    max_value_text, offset = read_ppm_token(content, offset)

    if magic != "P6":
        raise ValueError(f"{path}: only binary PPM P6 images are supported")

    width = int(width_text)
    height = int(height_text)
    max_value = int(max_value_text)
    if width <= 0 or height <= 0 or max_value != 255:
        raise ValueError(f"{path}: unsupported PPM dimensions or max value")

    if offset >= len(content) or not chr(content[offset]).isspace():
        raise ValueError(f"{path}: bad PPM header separator")
    offset += 1

    expected_size = width * height * 3
    pixels = content[offset:]
    if len(pixels) != expected_size:
        raise ValueError(f"{path}: pixel data length does not match header")

    return width, height, pixels


def resize_rgb_nearest(
    width: int,
    height: int,
    pixels: bytes,
    max_width: int = MEDIA_IMAGE_MAX_WIDTH,
    max_height: int = MEDIA_IMAGE_MAX_HEIGHT,
) -> tuple[int, int, bytes]:
    if width <= max_width and height <= max_height:
        return width, height, pixels

    scale = min(max_width / width, max_height / height)
    output_width = max(1, int(width * scale))
    output_height = max(1, int(height * scale))
    output = bytearray(output_width * output_height * 3)

    for y in range(output_height):
        source_y = min(height - 1, (y * height) // output_height)
        for x in range(output_width):
            source_x = min(width - 1, (x * width) // output_width)
            source_offset = (source_y * width + source_x) * 3
            output_offset = (y * output_width + x) * 3
            output[output_offset:output_offset + 3] = pixels[
                source_offset:source_offset + 3
            ]

    return output_width, output_height, bytes(output)


def validate_rgb_image(width: int, height: int, pixels: bytes) -> None:
    if width <= 0 or height <= 0:
        raise ValueError("image dimensions must be positive")
    if width > MEDIA_IMAGE_MAX_WIDTH or height > MEDIA_IMAGE_MAX_HEIGHT:
        raise ValueError("image exceeds anki3ds media bounds")
    if len(pixels) != width * height * 3:
        raise ValueError("pixel data length does not match dimensions")


def rgb_to_rgb565(red: int, green: int, blue: int) -> int:
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def write_a3i_image(path: Path, width: int, height: int, pixels: bytes) -> None:
    validate_rgb_image(width, height, pixels)

    with path.open("wb") as file:
        file.write(b"A3I1")
        file.write(width.to_bytes(2, "little"))
        file.write(height.to_bytes(2, "little"))

        for offset in range(0, len(pixels), 3):
            pixel = rgb_to_rgb565(pixels[offset], pixels[offset + 1], pixels[offset + 2])
            file.write(pixel.to_bytes(2, "little"))


def converted_media_name(source_name: str) -> str:
    return f"{Path(source_name).stem}.a3i"


def convert_media_file(media_root: Path, source_name: str, output_media_dir: Path) -> str:
    output_name = converted_media_name(source_name)
    if not media_name_is_valid(output_name):
        raise ValueError(f"{source_name}: converted media name is invalid")

    source_path = media_root / source_name
    output_path = output_media_dir / output_name
    width, height, pixels = load_ppm_rgb(source_path)
    width, height, pixels = resize_rgb_nearest(width, height, pixels)
    output_media_dir.mkdir(parents=True, exist_ok=True)
    write_a3i_image(output_path, width, height, pixels)
    return output_name


def write_deck(
    output_dir: Path,
    deck_id: str,
    deck_name: str,
    cards: list[ConvertedCard],
    media_root: Path | None = None,
) -> None:
    if not deck_id_is_valid(output_dir.name):
        raise ValueError(
            "output deck folder name must use letters, numbers, '_' or '-'"
        )
    if not deck_id_is_valid(deck_id):
        raise ValueError("deck id must use letters, numbers, '_' or '-'")
    if deck_id != output_dir.name:
        raise ValueError("deck id must match output deck folder name")
    if not cards:
        raise ValueError("deck must contain at least one card")
    if len(cards) > DECK_MAX_CARDS:
        raise ValueError(f"deck has more than {DECK_MAX_CARDS} cards")

    media_names: dict[str, str] = {}
    media_outputs: dict[str, str] = {}
    media_sources: set[str] = set()
    if media_root is not None:
        for card in cards:
            for source_name in (card.front_media, card.back_media):
                if source_name and source_name not in media_sources:
                    media_sources.add(source_name)
                    output_name = converted_media_name(source_name)
                    existing_source = media_outputs.get(output_name)
                    if existing_source is not None and existing_source != source_name:
                        raise ValueError(
                            f"media output name collision: {existing_source} and {source_name}"
                        )
                    media_outputs[output_name] = source_name

        for source_name in media_outputs.values():
            media_names[source_name] = converted_media_name(source_name)

    has_media = any(card.front_media or card.back_media for card in cards)
    validate_device_cards(cards, media_names, has_media)

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

    if media_root is not None:
        for source_name in media_outputs.values():
            media_names[source_name] = convert_media_file(
                media_root,
                source_name,
                output_dir / "media",
            )

    with (output_dir / "cards.tsv").open("w", encoding="utf-8", newline="\n") as file:
        for card in cards:
            fields = card_output_fields(card, media_names, has_media)
            file.write("\t".join(fields))
            file.write("\n")

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


def write_split_decks(
    output_dir: Path,
    deck_id: str,
    deck_name: str,
    cards: list[ConvertedCard],
    media_root: Path | None = None,
) -> list[Path]:
    if len(cards) <= DECK_MAX_CARDS:
        write_deck(output_dir, deck_id, deck_name, cards, media_root)
        return [output_dir]

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

    written_paths: list[Path] = []

    for chunk_index in range(chunk_count):
        chunk_number = chunk_index + 1
        chunk_id = split_deck_id(deck_id, chunk_number, chunk_count)
        chunk_output = output_dir.parent / chunk_id
        chunk_cards = cards[
            chunk_index * DECK_MAX_CARDS:
            (chunk_index + 1) * DECK_MAX_CARDS
        ]

        write_deck(
            chunk_output,
            chunk_id,
            split_deck_name(deck_name, chunk_number, chunk_count),
            chunk_cards,
            media_root,
        )
        written_paths.append(chunk_output)

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
        help="zero-based optional front media filename field index",
    )
    parser.add_argument(
        "--back-media-field",
        type=int,
        default=None,
        help="zero-based optional back media filename field index",
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
        help="directory containing PPM P6 media files to convert into media/*.a3i",
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

    lines = args.input.read_text(encoding="utf-8").splitlines()
    cards = convert_lines(
        lines,
        args.front_field,
        args.back_field,
        args.tags_field,
        args.front_media_field,
        args.back_media_field,
        args.card_id_field,
        args.note_id_field,
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
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
