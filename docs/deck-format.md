# Draft Deck Format

The 3DS app should read a simple format that avoids full Anki complexity.

## Folder Layout

```text
/3ds/anki3ds/decks/Deck Name/
  deck.json
  cards.tsv
  state.tsv
  media/
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

Draft columns:

```text
card_id<TAB>state<TAB>due<TAB>interval<TAB>ease<TAB>lapses<TAB>reviews<TAB>last_review
```

State values:

- `new`
- `learning`
- `review`
- `suspended`

`state.tsv` is owned by the 3DS app. The converter should preserve it when
updating card content.

## Review Log

Later versions may write an append-only review log:

```text
review_id<TAB>card_id<TAB>timestamp<TAB>rating<TAB>old_state<TAB>new_state
```

This can support undo, debugging, and possible desktop import.

## Compatibility Policy

Once the 3DS app reads a released format version, future converter versions
should either keep writing that version or include a migration path.
