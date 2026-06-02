# Algorithms And Abstractions

This note describes the small algorithms that are emerging in the current
anki3ds app and the C boundaries we want to keep. It is intentionally concrete:
the goal is to make the current behavior easy to reason about on hardware, not
to design a larger framework.

## Data Flow

```text
desktop export
  -> converter writes deck.json + cards.tsv
  -> deck_index scans deck folders
  -> deck loads cards.tsv
  -> scheduler starts a session
  -> review_state overlays state.tsv when present
  -> app reveals, rates, saves after each rating
```

Imported card content is owned by the converter and stored in `cards.tsv`.
Local review progress is owned by the 3DS app and stored in `state.tsv`.
Those files should stay separate so a deck can be re-imported without losing
local progress.

## Deck Discovery

Current discovery lives in `app-3ds/source/deck_index.c`; the app owns only the
selected index and active paths.

Algorithm:

1. Open `sdmc:/3ds/anki3ds/decks`.
2. Iterate directory entries.
3. Reject ids outside the portable allowlist: letters, numbers, `_`, and `-`.
4. Build `cards.tsv` and `state.tsv` paths from the folder name.
5. Reject entries whose folder name or paths exceed fixed limits.
6. Probe `cards.tsv` with `fopen`; only entries with readable cards are listed.
7. Keep the first `DECK_INDEX_MAX_DECKS` folder ids in sorted order.

`deck_index_scan` stores a compact `deck_entry` for each deck: folder id, cards
path, and state path. The current display name is the folder id. `deck.json` is
written by the converter but is not currently read by the 3DS app.

Current practical constraints:

- Discovery order is sorted by folder id.
- The app does not need all deck metadata to show the deck list.
- The selector shows at most `DECK_INDEX_MAX_DECKS` decks and reports overflow.
- Rescan is explicit from the deck selector with `SELECT`.
- Missing deck root or zero valid decks is a recoverable deck-selector state.

This boundary is small enough to host-test without libctru: build one entry,
reject invalid ids, scan a temporary root, and verify only folders containing
`cards.tsv` are listed.

## Deck Loading

Current loading is in `deck_load_cards`.

Algorithm:

1. Open the active `cards.tsv`.
2. Initialize a temporary `struct deck` using the selected deck name.
3. Read one bounded line at a time.
4. Reject an overlong physical line and consume the remainder.
5. Parse exactly five tab-separated fields:
   `card_id`, `note_id`, `front`, `back`, `tags`.
6. Unescape supported sequences into fixed buffers.
7. Skip empty lines.
8. Reject malformed lines, missing required `card_id`/`front`/`back`, or too
   many cards.
9. Commit the temporary deck to the caller only after a successful full load.

The parser currently accepts `\n`, `\t`, and `\\` escapes. The deck owns card
text in fixed-size buffers, with `DECK_MAX_CARDS` as the current hard limit.
Load errors are returned as small enums so the app can show concise on-device
messages.

The deck module should stay about deck data and card parsing. It should not know
about 3DS input, screens, review progress, or SD-card discovery.

## Review-State Load/Save

Current persistence is in `review_state.c`.

`state.tsv` columns are:

```text
card_id<TAB>review_count<TAB>last_rating<TAB>due_day<TAB>interval_days<TAB>ease_permille<TAB>lapses
```

Load algorithm:

1. Treat a missing file as normal new-deck state.
2. Read bounded lines.
3. Parse either the current seven-field row or the old four-field row.
4. Validate review count, numeric rating, due day, interval, ease, and lapses.
5. Find the matching card by `card_id`.
6. Ignore unknown card IDs so re-imported decks can drop cards without breaking
   the saved state.
7. Restore matching per-card scheduler state into a staged scheduler.
8. Reposition the scheduler to the first due card.

Old rows migrate `done=0` to due today and `done=1` to tomorrow with a one-day
interval. Bad rows are rejected before the staged scheduler is committed, so a
bad state file leaves the live session unchanged.

Save algorithm:

1. Build `state.tsv.tmp`.
2. Write one state row per loaded card.
3. Close the temp file and check close errors.
4. Remove the old `state.tsv`.
5. Rename the temp file to `state.tsv`.

This is temp-file backed and simple to inspect on the SD card. Because the
current code removes the old file before rename, power-loss behavior should be
tested on hardware before calling the save fully atomic.

## Review Loop

The app loop is a small mode machine:

- `DECK_SELECT`: list discovered deck folders, move selection, rescan, open.
- `LOAD_ERROR`: show the active path and load result, return to deck list.
- `REVIEW`: show front, reveal back, accept a rating.
- `SUMMARY`: show counts when no cards are due today.

Review algorithm:

1. Start with a loaded deck and initialized scheduler session.
2. Overlay saved state when present.
3. Show the current card front.
4. `A` reveals the answer.
5. Ratings are accepted only after reveal.
6. Ratings update interval, ease, due day, lapses, and review count.
7. `Again` keeps a card due today; other ratings schedule it into the future.
8. After every rating, save `state.tsv`.
9. Advance to the next due card, wrapping through the fixed card array.
10. Enter summary when no cards remain due today.

`SELECT` resets progress for the active deck by removing the active `state.tsv`
and reloading the selected deck. If removal fails, the app leaves the current
session in place and shows `reset failed`.

`rating_counts` are live session counters. Restored state contributes to
per-card `review_count`, but not to the current session's rating-count totals.

## Scheduler

The first spaced repetition algorithm is day-level and SM-2 inspired, not FSRS.
It stores enough state to replace the algorithm later without changing card IDs.

New cards start due today with ease `2500` and interval `0`. A card stays in
the initial learning path while its interval is `0` and it has no lapses, so
repeated new-card `Again` ratings do not count as review lapses.

Rating behavior:

- `Again`: due today, interval `0`, ease decreases by `200`.
- `Hard`: due after roughly `interval * 1.2`, at least one day, ease decreases
  by `150`.
- `Good`: due after `interval * ease / 1000`.
- `Easy`: due after `interval * (ease + 300) / 1000`, ease increases by `150`.

First successful reviews are special-cased so new cards become usable quickly:
`Hard` and `Good` start at one day, while `Easy` starts at four days. Ease is
clamped between `1300` and `3500`, and intervals are clamped to 100 years.

## Converter Flow

The converter is desktop Python and owns import-time normalization.

Algorithm:

1. Read UTF-8 tab-separated input.
2. Skip empty lines.
3. Select front, back, and optional tags by zero-based field indexes.
4. Strip outer whitespace from selected fields.
5. Reject missing fields and empty front/back text.
6. Generate stable IDs from SHA-1 digests:
   `note_id` from front/back/tags, then `card_id` from note/front/back.
7. Write `deck.json` with format version, deck id, deck name, creator, and
   card count.
8. Write `cards.tsv`, escaping backslashes, tabs, and newlines.

The converter deliberately does not open or rewrite `state.tsv`, so existing
review progress survives re-imports into the same deck folder. The folder id is
the runtime deck id on the 3DS, so the converter defaults `deck_id` from the
output folder name and rejects mismatches. Current converter support is
text-only; media, HTML/template rendering, and Anki collection parsing belong on
the desktop side rather than on the 3DS.

## C Boundaries We Want

Keep the portable logic separate from the libctru shell:

| Boundary | Owns | Should Not Own |
| --- | --- | --- |
| `deck_index` | deck folder scan, deck id validation, cards/state path construction | card parsing, review state parsing, rendering |
| `deck` | `cards.tsv` parsing, card/deck structs, parse/load errors | input handling, review progress, UI |
| `scheduler` | per-card session state, rating transitions, current-card selection | file paths, card text parsing, rendering |
| `review_state` | `state.tsv` load/save, card-id matching, persistence errors | deck discovery, button mapping, screens |
| `app` | top-level mode machine, libctru input/render loop, active deck selection | TSV parsing details, scheduler internals |
| converter | desktop import, stable IDs, deck folder writes | local 3DS progress mutation |

Likely next boundaries:

- `storage`: temp-file replace and remove/rename behavior that can be
  hardware-tested in one place.
- `ui`: screen drawing and button labels, once the app has more than a few
  screens.

Avoid generic containers, callback-heavy APIs, object systems, and global
service locators. Fixed limits and explicit enums are still the right shape
until a real deck or hardware test proves otherwise.
