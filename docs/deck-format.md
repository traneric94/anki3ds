# Draft Deck Format

The 3DS app should read a simple format that avoids full Anki complexity.

## Folder Layout

```text
/3ds/anki3ds/decks/<deck-id>/
  deck.json
  cards.tsv
  settings.tsv
  state.tsv
  review-log.tsv
```

Folder ids should use only letters, numbers, `_`, and `-`. The current app uses
the folder id as the stable runtime id and reads:

```text
sdmc:/3ds/anki3ds/decks/<deck-id>/cards.tsv
```

The default tracked text sample decks live in `sample-decks/` and install to:

```text
sdmc:/3ds/anki3ds/decks/limits-demo/cards.tsv
sdmc:/3ds/anki3ds/decks/sample/cards.tsv
```

See `sample-decks/README.md` for the tracked deck purposes, fresh-pass targets,
and `make verify-sample-decks` workflow.

Each sample deck also includes:

```text
sdmc:/3ds/anki3ds/decks/limits-demo/settings.tsv
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
Converter and package verification require this name to be non-empty, free of
control characters, and no longer than 63 UTF-8 bytes so it fits the app's fixed
display buffer.

## cards.tsv

Draft columns:

```text
card_id<TAB>note_id<TAB>front<TAB>back<TAB>tags
```

Rules:

- UTF-8 text
- one card per line
- the final card line may omit its trailing newline
- tabs separate fields
- `card_id` values must be unique within the deck
- `card_id` values may not contain control characters after unescaping; this
  includes tabs and newlines because `state.tsv` stores raw card IDs
- `card_id` values may not start with `#`, which is reserved for app-owned
  state-file metadata
- current 3DS builds support at most 256 cards per deck
- converter `--split-large-decks` writes oversized exports as up to 64 numbered
  sibling deck folders within this limit
- split re-import removes stale converter-generated sibling chunks after the
  current output is written
- if a converter-generated single-folder deck grows into split chunks, matching
  saved state rows and daily-limit settings are copied into the new chunk
  folders before the obsolete single folder is removed
- if converter-generated split chunks shrink back into one deck, matching saved
  state rows and daily-limit settings are copied into the new single folder
  before the obsolete chunks are removed
- if split output remains split but card boundaries move between numbered
  chunks, matching saved state rows are rewritten into the chunk that now owns
  each card; chunks with no matching saved rows start fresh instead of keeping
  stale state from their previous card ranges
- text-only re-import removes a stale `media/` directory from the active output
  folder while preserving progress and settings files
- `front` and `back` fields may use at most 383 UTF-8 bytes after unescaping
- long front/back fields can be scrolled on the review screen with D-pad
  Up/Down
- `tags` may use at most 127 UTF-8 bytes after unescaping
- converter-generated `card_id` and `note_id` values are short stable hashes
  with numeric suffixes for duplicates; by default they come from normalized
  front/back/tags text, but `--card-id-field` and `--note-id-field` can seed
  them from durable source IDs so edited card text preserves review progress
- embedded newlines are encoded as `\n`
- literal backslashes are escaped as `\\`
- converter output simplifies simple HTML to text before writing these fields
- media fields, inline image tags, audio, and arbitrary template output are not
  part of the supported deck format
- each physical `cards.tsv` row may use at most 1022 UTF-8 bytes before the
  optional trailing newline, so it fits in the current 1024-byte parser buffer

Example:

```text
card-0001	note-0001	front text	back text	tag1 tag2
card-0002	note-0002	What is 2 + 2?	4	math
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

Each setting key must appear exactly once. Duplicate settings rows are
malformed. Values must be non-negative integers no larger than `1000000`.
Blank rows and rows beginning with `#` are ignored.

The app may briefly create `settings.tsv.tmp` and `settings.tsv.bak` while
saving daily limits from the actions screen. If `settings.tsv` is missing or
malformed after an interrupted save, the app can load a valid
`settings.tsv.tmp` or `settings.tsv.bak`.

## state.tsv

Current files are framed by a header and footer. The number is the count of
state rows written between them:

```text
#anki3ds-state-v1<TAB>row_count
...
#anki3ds-state-complete<TAB>row_count
```

Current row columns:

```text
card_id<TAB>review_count<TAB>last_rating<TAB>due_day<TAB>interval_days<TAB>ease_permille<TAB>lapses<TAB>suspended<TAB>first_review_day<TAB>last_review_day
```

Rules:

- `review_count` is a non-negative integer, maximum `1000000`
- `last_rating` is numeric: `0` Again, `1` Hard, `2` Good, `3` Easy
- `due_day` is local calendar days since 1970-01-01
- `interval_days` is the current review interval
- `ease_permille` is the ease factor scaled by 1000, such as `2500` for 2.5
- `lapses` counts review-card Again ratings, maximum `1000000`
- `suspended` is `0` for active cards and `1` for cards skipped by review
- `first_review_day` is the first day this card was reviewed, or `0` if unknown
- `last_review_day` is the most recent review day, or `0` if unknown
- marked files must include matching header/footer row counts
- `lapses` must be less than or equal to `review_count`
- unreviewed cards must have `interval_days=0`, `lapses=0`, and no
  first/last review day
- if first/last review days are known, `first_review_day` must be less than or
  equal to `last_review_day`
- unknown card IDs are ignored when loading state
- if a valid state file has rows but none match current card IDs, the app starts
  a fresh queue and shows an unmatched-state warning
- duplicate rows for the same current deck `card_id` are malformed
- footerless ten-column state rows still load as migration data
- previous eight-column state rows still load with first/last review day as `0`
- previous seven-column state rows still load with `suspended=0`
- old four-column state rows, `card_id done review_count last_rating`, still load
  as a migration path

`state.tsv` and `review-log.tsv` are owned by the 3DS app. The converter should
preserve them when updating card content.

The app may briefly create `state.tsv.tmp` and `state.tsv.bak` while saving.
If `state.tsv` is missing or malformed after an interrupted save, the app can
load a valid `state.tsv.tmp` or `state.tsv.bak`. If all available state copies
are malformed, normal review-state saves are blocked until deck progress is
reset.

This is an early day-level spaced repetition format. Minute-level learning
steps, single-card unsuspend UI, burying, and filtered decks are planned later.
Older app builds that only accept unframed seven- or eight-column rows will
reject state saved by this version.

## Review Log

The app appends study transitions to `review-log.tsv` beside `state.tsv` after
a rating, suspend, undo, or restore-suspended action has been accepted and
`state.tsv` has saved. The log is diagnostic and normally appended; repairing
an interrupted final row may rewrite the complete prefix before appending. A
failed append does not roll back the review state or block the study action.
The app reports `log skipped` in the status line when a saved action could not
be logged.

```text
timestamp<TAB>day<TAB>event<TAB>card_id<TAB>rating<TAB>old_review_count<TAB>old_due_day<TAB>old_interval_days<TAB>old_ease_permille<TAB>old_lapses<TAB>old_suspended<TAB>new_review_count<TAB>new_due_day<TAB>new_interval_days<TAB>new_ease_permille<TAB>new_lapses<TAB>new_suspended
```

Rules:

- `timestamp` is Unix time in seconds, or `0` if the clock is unavailable
- `day` is local calendar days since 1970-01-01
- `event` is `rating`, `suspend`, `undo`, or `restore`
- `card_id` must not include tabs or newlines
- `rating` is `again`, `hard`, `good`, `easy`, or `-` for non-rating events
- old/new scheduler fields use the same meanings as `state.tsv`
- the app stops appending when the next row would exceed 262144 bytes
- if the existing log does not end in a newline, the app rewrites the complete
  prefix before appending so a new row is not concatenated onto a partial
  interrupted row

This can support debugging and possible desktop import later. The current
one-step undo is still an in-memory scheduler snapshot saved back to
`state.tsv`; the review log is not read by the 3DS app.

Resetting deck progress removes `state.tsv.tmp` and `state.tsv.bak` before the
primary `state.tsv`, then removes `review-log.tsv` as diagnostic cleanup. A log
delete failure does not restore review progress.

## Compatibility Policy

Once the 3DS app reads a released format version, future converter versions
should either keep writing that version or include a migration path.
