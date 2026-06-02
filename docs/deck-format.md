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
the folder id as the selector display name and reads:

```text
sdmc:/3ds/anki3ds/decks/<deck-id>/cards.tsv
```

The tracked text-only sample deck lives in `sample-decks/sample/` and installs
to:

```text
sdmc:/3ds/anki3ds/decks/sample/cards.tsv
```

The sample deck also includes:

```text
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

## cards.tsv

Draft columns:

```text
card_id<TAB>note_id<TAB>front<TAB>back<TAB>tags
```

Rules:

- UTF-8 text
- one card per line
- tabs separate fields
- embedded newlines are encoded as `\n`
- literal backslashes are escaped as `\\`
- media references are plain filenames, introduced later

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

If `settings.tsv` is missing or invalid, the app uses defaults:

```text
new_limit	20
review_limit	200
```

## state.tsv

Current columns:

```text
card_id<TAB>review_count<TAB>last_rating<TAB>due_day<TAB>interval_days<TAB>ease_permille<TAB>lapses<TAB>suspended<TAB>first_review_day<TAB>last_review_day
```

Rules:

- `review_count` is a non-negative integer
- `last_rating` is numeric: `0` Again, `1` Hard, `2` Good, `3` Easy
- `due_day` is days since the Unix epoch
- `interval_days` is the current review interval
- `ease_permille` is the ease factor scaled by 1000, such as `2500` for 2.5
- `lapses` counts review-card Again ratings
- `suspended` is `0` for active cards and `1` for cards skipped by review
- `first_review_day` is the first day this card was reviewed, or `0` if unknown
- `last_review_day` is the most recent review day, or `0` if unknown
- unknown card IDs are ignored when loading state
- previous eight-column state rows still load with first/last review day as `0`
- previous seven-column state rows still load with `suspended=0`
- old four-column state rows, `card_id done review_count last_rating`, still load
  as a migration path

`state.tsv` is owned by the 3DS app. The converter should preserve it when
updating card content.

This is an early day-level spaced repetition format. Minute-level learning
steps, unsuspend UI, burying, filtered decks, and review logs are planned later.
Older app builds that only accept seven- or eight-column rows will reject state
saved by this version.

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
