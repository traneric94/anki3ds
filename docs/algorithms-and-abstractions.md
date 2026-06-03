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
  -> app reveals, rates, saves state, and appends accepted transitions
```

Imported card content is owned by the converter and stored in `cards.tsv`.
Local review progress is owned by the 3DS app and stored in `state.tsv`.
Diagnostic study history is appended to `review-log.tsv`. Those files should
stay separate so a deck can be re-imported without losing local progress.

## Deck Discovery

Current discovery lives in `app-3ds/source/deck_index.c`; the app owns only the
selected index and active paths.

Algorithm:

1. Open `sdmc:/3ds/anki3ds/decks`.
2. Iterate directory entries.
3. Reject ids outside the portable allowlist: letters, numbers, `_`, and `-`.
4. Build `deck.json`, `cards.tsv`, `state.tsv`, `review-log.tsv`, and
   `settings.tsv` paths from the folder name.
5. Reject entries whose folder name or paths exceed fixed limits.
6. Probe `cards.tsv` with `fopen`; only entries with readable cards are listed.
7. Optionally read the deck display name from `deck.json`.
8. Count every valid deck folder.
9. Keep the first `DECK_INDEX_MAX_DECKS` folder ids in sorted order.

`deck_index_scan` stores a compact `deck_entry` for each deck: folder id,
display name, cards path, state path, review-log path, settings path, media
path, and metadata path. The folder id remains the stable runtime id. The
display name falls back to the folder id when `deck.json` is missing or
malformed.

Current practical constraints:

- Discovery order is sorted by folder id.
- The app only reads the `name` string from deck metadata during deck scanning.
- The selector stores at most `DECK_INDEX_MAX_DECKS` decks, shows a bounded
  scroll window, and reports overflow as visible/total deck counts.
- SD rescan is explicit from the deck selector with `SELECT`.
- Rescan keeps the selected folder id highlighted when that deck still exists.
- Returning from review, summary, or load-error screens reuses the cached deck
  list and refreshes only the active deck's summary from the in-memory session.
- Startup and explicit rescans draw a scanning screen before SD traversal, then
  update progress before each deck summary load so large deck lists do not look
  like a blank or frozen app.
- Missing deck root or zero valid decks is a recoverable deck-selector state.

This boundary is small enough to host-test without libctru: build one entry,
reject invalid ids, scan a temporary root, and verify only folders containing
`cards.tsv` are listed.

After discovery, the app builds a `deck_summary` for each visible deck. The
summary loads the deck, settings, and saved state into a temporary scheduler
session, then records card count, total due count, new/learning/review due
counts, and suspended-card count for the selector. This keeps the deck list
useful for daily study while preserving the fixed `DECK_INDEX_MAX_DECKS` and
`DECK_MAX_CARDS` limits. The summary loader allocates its temporary deck and
scheduler on the heap so larger supported decks do not consume a large 3DS stack
frame during deck scanning. If the deck loads but every available state file is
malformed, the selector keeps the card count but suppresses due and suspended
counts and shows a state error instead of presenting bad progress as a fresh
review queue.

## Deck Loading

Current loading is in `deck_load_cards`.

Algorithm:

1. Open the active `cards.tsv`.
2. Allocate and initialize a temporary `struct deck` using the selected deck
   name.
3. Read one bounded line at a time.
4. Reject an overlong physical line and consume the remainder.
5. Parse exactly five tab-separated fields:
   `card_id`, `note_id`, `front`, `back`, `tags`.
6. Unescape supported sequences into fixed buffers.
7. Skip empty lines.
8. Reject malformed lines, duplicate `card_id` values, missing required
   `card_id`/`front`/`back`, or too many cards.
9. Commit the temporary deck to the caller only after a successful full load.

The parser currently accepts `\n`, `\t`, and `\\` escapes. The deck owns card
text in fixed-size buffers, with `DECK_MAX_CARDS` as the current hard limit.
Card IDs reject control characters after unescaping so `state.tsv` can store
raw IDs without ambiguity.
Load errors are returned as small enums so the app can show concise on-device
messages. The staged deck is heap allocated so the supported card limit does
not create a large stack frame while loading a deck.

The deck module should stay about deck data and card parsing. It should not know
about 3DS input, screens, review progress, or SD-card discovery.

## Review-State Load/Save

Current persistence is in `review_state.c`.

`state.tsv` columns are:

```text
card_id<TAB>review_count<TAB>last_rating<TAB>due_day<TAB>interval_days<TAB>ease_permille<TAB>lapses<TAB>suspended<TAB>first_review_day<TAB>last_review_day
```

Load algorithm:

1. Treat a missing file as normal new-deck state.
2. Read bounded lines.
3. If a state header is present, require the matching completion footer and row
   count before committing the staged scheduler.
4. Parse the current ten-field row, previous eight- or seven-field row, or old
   four-field row.
5. Validate review count, numeric rating, due day, interval, ease, lapses, and
   suspended/review-day fields. Restore rejects impossible scheduler states
   such as lapses greater than review count, unreviewed rows with review-day
   state, or first review day after last review day.
6. Find the matching card by `card_id`.
7. Ignore unknown card IDs so re-imported decks can drop cards without breaking
   the saved state.
8. Reject duplicate rows for the same current deck card.
9. Restore matching per-card scheduler state into a staged scheduler.
10. Reposition the scheduler to the first due card.

Old rows migrate `done=0` to due today and `done=1` to tomorrow with a one-day
interval. Bad rows are rejected before the staged scheduler is committed, so a
bad state file leaves the live session unchanged.

Save algorithm:

1. Reject mismatched deck/session card counts before opening a temp file.
2. Build `state.tsv.tmp`.
3. Write a state header with the row count.
4. Write one state row per loaded card.
5. Write a completion footer with the same row count.
6. Close the temp file and check close errors.
7. Ask `storage` to replace the primary file through the shared
   temp/backup transaction.

When replacing an existing primary file, `storage` keeps that previous primary
as `.bak` after the new temp file becomes primary. First saves still leave no
backup. This gives the load path a known previous save to recover from if the
new primary is corrupted later.

If the main state file is missing on load, the app tries `state.tsv.tmp`, then
`state.tsv.bak`. The temp fallback covers interrupted first saves where the
temp file was fully written but not yet renamed into place. If the main state
file is malformed, the app tries the backup before temp so a stale temp file
does not outrank a known previous save. Empty state files and files with no rows
matching the current deck are treated as malformed, so a truncated save cannot
silently reset all progress and then be overwritten as fresh state. If every
available state copy is malformed, review-state saves are blocked until the
user resets deck progress. Reset removes `state.tsv`, `state.tsv.tmp`, and
`state.tsv.bak` for the active deck. The shared `storage` module owns the
remove/rename order for both review state and settings. This remains simple to
inspect on the SD card while avoiding the known remove-before-rename data-loss
window.

The storage transaction checks whether the primary or backup file exists before
removing or renaming it. Azahar/libctru SD-card operations do not behave exactly
like desktop POSIX in missing-file cases, so first-save behavior must not rely
on `errno == ENOENT` after a failed `rename`.

## Review Loop

The app loop is a small mode machine:

- `DECK_SELECT`: list discovered deck folders, move selection, rescan, open.
- `LOAD_ERROR`: show the active path and load result, return to deck list.
- `REVIEW`: show front, reveal back, accept a rating.
- `SUMMARY`: show counts when no cards are due today.
- `ACTIONS`: choose deck-level actions such as restoring suspended cards or
  resetting progress.
- `SETTINGS`: edit per-deck daily limits.
- `CONTROLS`: show the in-app key map, then return to the previous mode.
- `CONFIRM_RESET`: require explicit `X` before deleting saved review state.
- `CONFIRM_EXIT`: require explicit `A` before leaving the app.

The console UI uses the top screen for deck/card content and the bottom screen
for mode-specific controls and a short status line. This keeps button prompts
and save feedback out of the review card area without introducing a graphics
framework yet.

The review button map lives in the small `app_controls` module so the
reveal/rating rules can be host-tested without libctru. `main.c` still owns
state transitions and side effects, but the stable review mapping is: front
side `A` reveals; after reveal, `Y/X/B/A` choose Again/Hard/Good/Easy.
Ambiguous post-reveal face-button combinations are ignored so a fat-fingered
rating does not save the wrong answer.
D-pad hold repeat also lives in `app_controls`; `main.c` applies it only in
deck select, actions, and settings modes, so ratings and destructive actions
stay single-press.

To avoid unnecessary screen work, the main loop only flushes and swaps
framebuffers after drawing a changed screen. Redraws still wait for VBlank.
When the screen is unchanged, the app waits for HID input with an adaptive
timeout before scanning controls again. The wait starts short for responsive
input, then backs off while idle to avoid busy redraw/poll loops while still
letting `aptMainLoop` run regularly.

The app tracks the current local calendar day while it is open. The main loop
checks for a local-day change at most once per minute, using the loop's existing
`time()` result so it does not run calendar conversion on every idle tick. If a
valid day change is observed, deck-select rescans summaries, and an active
review session updates the scheduler's `today`, clears one-step undo,
recomputes daily counts, and repositions to the next due card. If a review or
summary screen is visible, the screen redraws immediately; modal return targets
are updated so canceling a modal lands on the correct review or summary screen
for the new day.

The app samples PTMU battery state at startup, then at most once every ten
minutes. Periodic checks first ask PTMU whether the shell is open; battery level
and charging state are read only when the shell reports open. The app does not
call the battery service on every button press. If the system clock is briefly
unavailable, the periodic poll timer arms itself when a valid clock reading
appears. Closed-shell checks also schedule the next ten-minute poll instead of
remaining immediately due. The bottom screen shows the last valid open-shell
battery sample as a compact `level/5` line, including charging and low-battery
states. Battery status changes redraw the screen only when that visible status
changes.

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

`R` suspends the current card without counting a review. Suspended cards are
saved in `state.tsv`, treated as not due, skipped by queue advancement, and
included in total card counts. The selector, actions screen, and summary screen
show suspended-card counts so a hidden queue is visible before restoring cards.

The actions screen can restore all suspended cards in the active deck. This is
the default selected action so opening actions and pressing `A` does not reset
progress. Daily limits can be edited from the same actions screen. Reset
remains available by moving the action selection first, then confirming on a
separate reset screen with `X`.

Cards may reference front/back `.a3i` media. The app keeps a two-slot media
cache for the active deck, enough for the current card's front and back images.
This avoids repeated SD reads on redraws while keeping memory bounded. The
review-screen image positions live in `app_layout`, and host tests check that
maximum-size front and back images fit within the top screen. Loading a deck
clears the cache.

`L` undoes the most recent rating or suspend action in the active session. The
scheduler stores a single snapshot of the affected card plus queue/session
counters before applying the action. Undo restores that snapshot, clears the
undo slot, returns to review mode, and saves the restored `state.tsv`. Loading a
deck or restoring saved card state clears the undo slot.

The app keeps a scheduler rollback snapshot before review actions that mutate
state and then save `state.tsv`. If the save fails after a rating, suspend,
undo, or restore-suspended action, the app restores that snapshot and leaves the
user on the current workflow screen with the save error visible. This keeps the
in-memory review queue from advancing past the durable SD-card state.

After a rating, suspend, undo, or restore-suspended action saves `state.tsv`,
the app appends diagnostic rows to `review-log.tsv` with the before/after
scheduler fields for the affected cards. Review logging is best-effort and
append-only until the next row would exceed the configured size cap: a log
append failure does not roll back a saved study action.

The bottom status line reports successful ratings with the next card index, and
reports save failures as non-advancing actions. This is intentionally redundant
with the top-screen state string because SD-card save failures are otherwise
easy to mistake for scheduler bugs during emulator or hardware testing.

`SELECT` opens an actions screen from review and summary modes. Choosing reset
opens a confirmation screen. Pressing `X` there removes the active
`review-log.tsv` and `state.tsv`, then reloads the selected deck. If reload
succeeds, the bottom status confirms `Progress reset`; if removal fails, the
app leaves the current session in place and shows `reset failed`.

`START` opens an exit confirmation screen from every normal app mode. Pressing
`A` there exits the app; `B` or `SELECT` cancels back to the previous mode. This
keeps the Homebrew-style exit path available while avoiding accidental exits
during review or settings edits.

`rating_counts` are live session counters. Restored state contributes to
per-card `review_count`, but not to the current session's rating-count totals.

The scheduler's day number is derived from the device's local calendar date.
It is not `time() / 86400`, because UTC rollover can make due cards and daily
limits advance early in western time zones. The date conversion lives in
`app_time` so local-day behavior can be host-tested without libctru.

## Settings

Each deck may include `settings.tsv` beside `cards.tsv`. Missing settings use
defaults of `new_limit=20` and `review_limit=200`. A value of `0` means
unlimited. A present settings file must include both `new_limit` and
`review_limit` exactly once; incomplete or duplicate rows are malformed so
interrupted writes can fall back to temp or backup files. The app can write
`settings.tsv` from the daily
limits screen using the same temp/backup save pattern as review state. If
`settings.tsv` is missing on load, settings try `settings.tsv.tmp`, then
`settings.tsv.bak`. If `settings.tsv` is malformed, backup is tried before temp
so a stale temp file does not outrank a known previous save. If all available
settings files are malformed, the app uses defaults.
Settings save feedback stays in the settings/status messages and does not
overwrite the review-state status line.

The scheduler stores `first_review_day` and `last_review_day` in `state.tsv` so
daily limits survive relaunch. New-card limits apply to unstarted new cards.
Review limits apply to unstarted normal review cards. Zero-day
learning/relearning cards remain due even when the review limit is full, and a
card already started today is allowed to remain due. This lets same-day Again
loops finish instead of hiding half-reviewed cards behind a limit.

Changing daily limits clears the one-step undo slot. The undo snapshot contains
queue counters from the previous limit configuration, so keeping it after a
limit change could restore a stale visible queue.

## Scheduler

The first spaced repetition algorithm is day-level and SM-2 inspired, not FSRS.
It stores enough state to replace the algorithm later without changing card IDs.

New cards start due today with ease `2500` and interval `0`. A card stays in
the initial learning path while its interval is `0` and it has no lapses, so
repeated new-card `Again` ratings do not count as review lapses.

Review cards rated `Again` enter a simple relearning path with interval `0` and
at least one lapse. In relearning, `Hard` and `Good` schedule the card for
tomorrow, while `Easy` schedules it for four days later. This prevents a lapsed
card from jumping forward by multiplying a zero-day interval by its ease.

Rating behavior:

- `Again`: due today, interval `0`, ease decreases by `200`.
- `Hard`: due after roughly `interval * 1.2`, at least one day, ease decreases
  by `150`.
- `Good`: due after `interval * ease / 1000`.
- `Easy`: due after `interval * (ease + 300) / 1000`, ease increases by `150`.

First successful reviews are special-cased so new cards become usable quickly:
`Hard` and `Good` start at one day, while `Easy` starts at four days. Ease is
clamped between `1300` and `3500`, and intervals are clamped to 100 years.
Review and lapse counters are capped at `1000000` so a card at the file-format
boundary can still be reviewed, saved, and loaded again.

Due selection prefers cards that are already in progress before introducing new
cards. The priority order is learning/relearning cards, then review cards by
oldest due day, then new cards. Learning/relearning cards bypass the unstarted
review limit while they are due, but once reviewed they count toward today's
review limit like other review cards. During an active session, advancement
starts after the current card so a failed card is not immediately reselected
while other due cards remain; among those candidates, rotation is the
tie-breaker when priority and due day are equal.

## Converter Flow

The converter is desktop Python and owns import-time normalization.

Algorithm:

1. Read UTF-8 tab-separated input.
2. Skip empty lines.
3. Select front, back, and optional tags by zero-based field indexes.
4. Strip outer whitespace from selected fields.
5. Reject missing fields and empty front/back text.
6. Generate stable IDs from SHA-1 digests:
   `note_id` and `card_id` from `--note-id-field`/`--card-id-field` when
   durable source IDs are provided, otherwise `note_id` from front/back/tags
   and `card_id` from note/front/back.
7. Write `deck.json` with format version, deck id, deck name, creator, and
   card count.
8. Write `cards.tsv`, escaping backslashes, tabs, and newlines.
9. If media fields and `--media-root` are provided, convert referenced binary
   PPM `P6` files into bounded raw `.a3i` files under `media/`.
10. Write default `settings.tsv` if it does not already exist.

The converter deliberately does not open or rewrite existing `state.tsv`,
`review-log.tsv`, or `settings.tsv`, so review progress, diagnostic history,
and deck-specific daily limits survive re-imports into the same deck folder. To
keep progress attached to edited card text, pass stable source ID fields during
conversion; otherwise content-derived fallback IDs change when the normalized
front/back/tags content changes. The folder id is the runtime deck id on the
3DS, so the converter defaults `deck_id` from the output folder name and
rejects mismatches. Current converter media support is deliberately narrow: PPM
`P6` in, `.a3i` out. Rich HTML/template rendering and Anki collection parsing
still belong on the desktop side rather than on the 3DS.

## C Boundaries We Want

Keep the portable logic separate from the libctru shell:

| Boundary | Owns | Should Not Own |
| --- | --- | --- |
| `deck_index` | deck folder scan, deck id validation, deck-local path construction | card parsing, review state parsing, rendering |
| `deck` | `cards.tsv` parsing, card/deck structs, parse/load errors | input handling, review progress, UI |
| `scheduler` | per-card session state, rating transitions, current-card selection | file paths, card text parsing, rendering |
| `review_state` | `state.tsv` load/save, card-id matching, persistence errors | deck discovery, button mapping, screens |
| `review_log` | append-only study transition rows | scheduler decisions, rollback policy, rendering |
| `storage` | temp/backup save-file replacement and cleanup | TSV formatting, scheduler state, settings parsing |
| `app_layout` | screen geometry constants and pure fit checks | rendering side effects, text wrapping |
| `app_power` | battery status thresholds and poll scheduling policy | libctru PTMU calls, rendering |
| `app_text` | UTF-8 character stepping for wrapping/truncation | font shaping, rich text layout |
| `media_image` | bounded `.a3i` validation and pixel loading | PNG/JPEG decoding, deck parsing, scheduler state |
| `media_cache` | bounded reuse of loaded media images by path | rendering, deck selection, SD path construction |
| `app` | top-level mode machine, libctru input/render loop, active deck selection | TSV parsing details, scheduler internals |
| converter | desktop import, stable IDs, deck folder writes | local 3DS progress mutation |

Battery sampling is intentionally coarse. The main loop samples PTMU at startup
and then at most every ten minutes while the shell is open. A closed-shell skip
does not move the next real sample time forward, and transient PTMU read
failures keep the last valid battery display instead of clearing it.

The console renderer still uses a simple one-column-per-character model, but
`app_text` keeps valid UTF-8 byte sequences together during wrapping and
ellipsis truncation so language-deck text is not split in the middle of a
character.

Likely next boundaries:

- `ui`: screen drawing and button labels, once the app has more than a few
  screens.

Avoid generic containers, callback-heavy APIs, object systems, and global
service locators. Fixed limits and explicit enums are still the right shape
until a real deck or hardware test proves otherwise.
