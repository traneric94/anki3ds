# Draft Deck Format

The 3DS app should read a simple format that avoids full Anki complexity.

## Folder Layout

```text
/3ds/anki3ds/decks/<deck-id>/
  deck.json
  cards.tsv
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

## state.tsv

Current sample-app columns:

```text
card_id<TAB>done<TAB>review_count<TAB>last_rating
```

Rules:

- `done` is `0` or `1`
- `review_count` is a non-negative integer
- `last_rating` is numeric: `0` Again, `1` Hard, `2` Good, `3` Easy
- unknown card IDs are ignored when loading state

`state.tsv` is owned by the 3DS app. The converter should preserve it when
updating card content.

This is an early persistence format for the sample reviewer. Full spaced
repetition fields such as due date, interval, ease, lapses, and review log are
planned later.

## Review Log

Later versions may write an append-only review log:

```text
review_id<TAB>card_id<TAB>timestamp<TAB>rating<TAB>old_state<TAB>new_state
```

This can support undo, debugging, and possible desktop import.

## Compatibility Policy

Once the 3DS app reads a released format version, future converter versions
should either keep writing that version or include a migration path.
