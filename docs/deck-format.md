# Draft Deck Format

The 3DS app should read a simple format that avoids full Anki complexity.

## Folder Layout

```text
/3ds/anki3ds/decks/<deck-id>/
  deck.json
  cards.tsv
  settings.tsv
  state.tsv
  media/
```

Folder ids should use only letters, numbers, `_`, and `-`. The current app uses
the folder id as the stable runtime id and reads:

```text
sdmc:/3ds/anki3ds/decks/<deck-id>/cards.tsv
```

The tracked sample decks live in `sample-decks/` and install to:

```text
sdmc:/3ds/anki3ds/decks/limits-demo/cards.tsv
sdmc:/3ds/anki3ds/decks/media-demo/cards.tsv
sdmc:/3ds/anki3ds/decks/sample/cards.tsv
```

Each sample deck also includes:

```text
sdmc:/3ds/anki3ds/decks/limits-demo/settings.tsv
sdmc:/3ds/anki3ds/decks/media-demo/settings.tsv
sdmc:/3ds/anki3ds/decks/sample/settings.tsv
```

## deck.json

Draft:

```json
{
  "format_version": 1,
  "deck_id": "sample",
  "name": "Sample Deck",
  "created_by": "anki3ds-converter",
  "card_count": 2
}
```

`deck_id` must match the folder id. The converter defaults it from the output
folder name, and rejects mismatches so desktop output stays loadable on-device.
Folder ids are limited to 63 ASCII letters, numbers, `_`, or `-` characters,
which leaves space for the C string terminator in the 3DS app's fixed buffers.
The 3DS app reads the optional `name` string for deck-list and review-screen
display, falling back to the folder id when metadata is missing or malformed.

## cards.tsv

Draft columns:

```text
card_id<TAB>note_id<TAB>front<TAB>back<TAB>tags
card_id<TAB>note_id<TAB>front<TAB>back<TAB>tags<TAB>front_media<TAB>back_media
```

Rules:

- UTF-8 text
- one card per line
- the final card line may omit its trailing newline
- tabs separate fields
- `card_id` values must be unique within the deck
- `card_id` values may not contain control characters after unescaping; this
  includes tabs and newlines because `state.tsv` stores raw card IDs
- current 3DS builds support at most 256 cards per deck
- converter `--split-large-decks` writes oversized exports as up to 64 numbered
  sibling deck folders within this limit
- `front` and `back` fields may use at most 383 UTF-8 bytes after unescaping
- `tags` may use at most 127 UTF-8 bytes after unescaping
- converter-generated `card_id` and `note_id` values are short stable hashes
  with numeric suffixes for duplicates; by default they come from normalized
  front/back/tags text, but `--card-id-field` and `--note-id-field` can seed
  them from durable source IDs so edited card text preserves review progress
- embedded newlines are encoded as `\n`
- literal backslashes are escaped as `\\`
- converter output simplifies simple HTML to text before writing these fields
- `front_media` and `back_media` are optional plain filenames under `media/`
- media filenames may use only letters, numbers, `_`, `-`, and `.`, and may not
  start with `.`
- media filenames may use at most 95 UTF-8 bytes
- each physical `cards.tsv` row must fit in the current 1024-byte parser buffer

Example:

```text
card-0001	note-0001	front text	back text	tag1 tag2
card-0002	note-0002	What is 2 + 2?	4	math
card-0003	note-0003	What is shown?	diagram explanation	media	front-diagram.a3i	back-diagram.a3i
```

## settings.tsv

Optional per-deck settings use two tab-separated columns:

```text
setting<TAB>value
```

Supported settings:

- `new_limit`: new cards introduced per day, `0` means unlimited
- `review_limit`: review cards introduced per day, `0` means unlimited

Both settings rows must be present. If `settings.tsv` is missing or invalid,
the app uses defaults:

```text
new_limit	20
review_limit	200
```

The app may briefly create `settings.tsv.tmp` and `settings.tsv.bak` while
saving daily limits from the actions screen. If `settings.tsv` is missing or
malformed after an interrupted save, the app can load a valid
`settings.tsv.tmp` or `settings.tsv.bak`.

## state.tsv

Current columns:

```text
card_id<TAB>review_count<TAB>last_rating<TAB>due_day<TAB>interval_days<TAB>ease_permille<TAB>lapses<TAB>suspended<TAB>first_review_day<TAB>last_review_day
```

Rules:

- `review_count` is a non-negative integer
- `last_rating` is numeric: `0` Again, `1` Hard, `2` Good, `3` Easy
- `due_day` is local calendar days since 1970-01-01
- `interval_days` is the current review interval
- `ease_permille` is the ease factor scaled by 1000, such as `2500` for 2.5
- `lapses` counts review-card Again ratings
- `suspended` is `0` for active cards and `1` for cards skipped by review
- `first_review_day` is the first day this card was reviewed, or `0` if unknown
- `last_review_day` is the most recent review day, or `0` if unknown
- unknown card IDs are ignored when loading state
- duplicate rows for the same current deck `card_id` are malformed
- previous eight-column state rows still load with first/last review day as `0`
- previous seven-column state rows still load with `suspended=0`
- old four-column state rows, `card_id done review_count last_rating`, still load
  as a migration path

`state.tsv` is owned by the 3DS app. The converter should preserve it when
updating card content.

The app may briefly create `state.tsv.tmp` and `state.tsv.bak` while saving.
If `state.tsv` is missing or malformed after an interrupted save, the app can
load a valid `state.tsv.tmp` or `state.tsv.bak`.

This is an early day-level spaced repetition format. Minute-level learning
steps, single-card unsuspend UI, burying, filtered decks, and review logs are
planned later.
Older app builds that only accept seven- or eight-column rows will reject state
saved by this version.

## media/*.a3i

The 3DS app can display bounded raw `.a3i` images referenced by `front_media`
or `back_media`.

File layout:

```text
bytes 0-3:  A3I1
bytes 4-5:  little-endian width
bytes 6-7:  little-endian height
bytes 8-:   little-endian RGB565 pixels, row-major
```

Limits:

- maximum width: 160 pixels
- maximum height: 72 pixels
- zero dimensions are invalid
- extra or missing pixel data is invalid

The converter can currently convert binary PPM `P6` images into `.a3i` files.
Common image formats such as PNG/JPEG should be converted to PPM first or added
through a future optional desktop dependency. The 3DS app intentionally does no
general-purpose image decoding.

## Review Log

Later versions may write an append-only review log:

```text
review_id<TAB>card_id<TAB>timestamp<TAB>rating<TAB>old_state<TAB>new_state
```

This can support multi-step undo, debugging, and possible desktop import later.
The current one-step undo is an in-memory scheduler snapshot saved back to
`state.tsv`.

## Compatibility Policy

Once the 3DS app reads a released format version, future converter versions
should either keep writing that version or include a migration path.
