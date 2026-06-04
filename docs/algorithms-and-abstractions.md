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
9. Count non-hidden ignored entries with invalid ids, overlong paths, or no
   readable `cards.tsv`.
10. Keep the first `DECK_INDEX_MAX_DECKS` folder ids in sorted order.

`deck_index_scan` stores a compact `deck_entry` for each deck: folder id,
display name, cards path, state path, review-log path, settings path, and
metadata path. The folder id remains the stable runtime id. The display name
falls back to the folder id when `deck.json` is missing or malformed.

Current practical constraints:

- Discovery order is sorted by folder id.
- The app only reads the `name` string from deck metadata during deck scanning.
- The selector stores at most `DECK_INDEX_MAX_DECKS` decks, shows a bounded
  scroll window, and reports overflow as visible/total deck counts.
- SD rescan is explicit from the deck selector with `SELECT`.
- Rescan keeps the selected folder id highlighted when that deck still exists.
- Returning from review, summary, or load-error screens reuses the cached deck
  list and refreshes only the active deck's summary from the in-memory session.
- Successful saved ratings, undo actions, suspends, restores, daily-limit
  edits, and day changes refresh that cached active-deck summary immediately,
  so deck-selector due counts stay current without a full rescan.
- Startup and explicit rescans draw a scanning screen before SD traversal, then
  update progress before each deck summary load so large deck lists do not look
  like a blank or frozen app.
- The selector reports ignored entries so copied folders such as `My Deck/` or
  incomplete deck payloads do not fail silently.
- Missing deck root or zero valid decks is a recoverable deck-selector state.

This boundary is small enough to host-test without libctru: build one entry,
reject invalid ids, scan a temporary root, and verify only folders containing
`cards.tsv` are listed.

After discovery, the app builds a `deck_summary` for each visible deck. The
summary loads the deck, settings, and saved state into a temporary scheduler
session, then records card count, total due count, new/learning/review due
counts from scheduler policy, daily-limit-blocked new/review counts,
suspended-card count, and deck/settings/state load reports for the selector.
This keeps the deck list useful for daily study while
preserving the fixed `DECK_INDEX_MAX_DECKS` and `DECK_MAX_CARDS` limits. The
summary loader allocates its temporary deck and scheduler on the heap so larger
supported decks do not consume a large 3DS stack frame during deck scanning. If
the deck load fails, the selected-deck panel can show the same first-line parse
detail as the load-error screen. If the deck loads but every available state
file is malformed, the selector keeps the card count but suppresses due and
suspended counts and shows a state error instead of presenting bad progress as
a fresh review queue. If daily limits hide otherwise calendar-due cards, the
selector marks the deck with a limit warning and shows the hidden new/review
counts in the selected-deck details. If a valid state file has no rows matching
the current deck's card ids, the selector marks it as unmatched and the app
starts a fresh queue.

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
messages. The optional load report carries the first failing line number plus
the parser reason for malformed rows, empty decks, and over-cap decks. The
staged deck is heap allocated so the supported card limit does not create a
large stack frame while loading a deck.

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
10. If parsed rows are valid but none match the current deck, return an
    unmatched-state warning and leave the initialized scheduler fresh.
11. Reposition the scheduler to the first due card.

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
does not outrank a known previous save. Empty state files are treated as
malformed, so a truncated save cannot silently reset all progress and then be
overwritten as fresh state. Valid files with rows but no card ids matching the
current deck are treated as unmatched, which allows re-imports with changed ids
to start fresh while showing a warning. If every available state copy is
malformed, review-state saves are blocked until the user resets deck progress.
Opening that deck enters a reset-needed summary screen instead of a review
queue, and normal study controls such as undo remain disabled until reset
succeeds. The reset-needed screens show the first malformed state line or
aggregate state-file reason when available. Reset removes `state.tsv.tmp` and
`state.tsv.bak` before the primary `state.tsv` for the active deck. The shared
`storage` module owns the remove/rename order for both review state and
settings. This remains simple to inspect on the SD card while avoiding the
known remove-before-rename data-loss window.

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
- `CONFIRM_RESTORE`: require explicit `X` before restoring suspended cards.
- `CONFIRM_SUSPEND`: require explicit `X` before hiding the current card.
- `CONFIRM_RESET`: require explicit `X` before deleting saved review state.
- `CONFIRM_EXIT`: require explicit `A` before leaving the app.

The console UI uses the top screen for deck/card content and the bottom screen
for mode-specific controls and a short status line. This keeps button prompts
and save feedback out of the review card area without introducing a graphics
framework yet. Rendering uses a compact dark-terminal palette: warm amber
headings/status labels, Hard ratings, learning counts, and cautions; bright
violet reverse-video selected/focused items; green Good ratings, review
counts, and successful, restored, or safe state; red reverse-video selected
destructive actions; red Again ratings, suspended counts, errors, and
destructive reset prompts; bright violet Easy ratings and new counts; and
bright white neutral status, with dim white separators. Blue and cyan are
intentionally avoided because they are hard to read on the dark 3DS console
background. Moving
through the deck selector keeps the status line aligned
with the selected row, including load errors,
ignored settings, unmatched state, and daily-limit-blocked decks. Controls
opened from deck-specific screens, and closed back to those screens, inherit
warning context from the selector or active deck. Returning to the selector
restores the selected-deck status instead of a generic navigation message.
Canceling exit confirmation restores the status context for the destination
screen; unsaved daily-limit edits stay highest priority, controls restore their
own contextual status, and active deck screens keep reset, ignored-settings,
unmatched-state, or daily-limit warnings visible.
Opening exit, restore, suspend, and reset confirmations follows the same status
suffix rule so the prompt does not hide the warning context it was opened from.

The review button map and app-level command priority live in the small
`app_controls` module so reveal/rating rules can be host-tested without
libctru. `main.c` still owns state transitions and side effects, but it asks
`app_controls` to classify global actions such as exit, controls, deck-list
return, actions, undo, suspend confirmation, reveal, and rating. The stable
review mapping is: front side `A` reveals; after reveal, `Y/X/B/A` choose
Again/Hard/Good/Easy. Ambiguous post-reveal face-button combinations are
ignored so a fat-fingered rating does not save the wrong answer.
Actions, daily-limit settings, restore, suspend, reset, and exit confirmations
use the same centralized confirm-or-cancel classifier: the confirm button must
be the only active input, and `B`/`SELECT` cancel only as clean single-button
presses. Mixed confirm/cancel, confirm/navigation, or command chords resolve to
no action.
D-pad hold repeat also lives in `app_controls`; repeatable axes are declared per
mode. Deck select repeats Up/Down movement and Left/Right paging; review
repeats only Up/Down text scrolling; actions repeat only Up/Down selection; and
settings repeats only Left/Right daily-limit value changes. Up/Down field
selection in settings remains single-step so a held button cannot bounce between
the two fields. In review mode, D-pad Up/Down scrolls the active text pane:
front before reveal, back after reveal. Ratings and destructive actions stay
single-press. Long active panes draw a compact `^ current/total v` cue in the
top-screen section header so scrollability is visible without moving the
controls off the bottom screen. Answer reveal and review-text scroll feedback
append active-deck warning context when present. Held input keeps the idle wait
counter short while a repeatable button is down, so selector movement, action
selection, value changes, and text scrolling do not slow down as if the app were
idle. This short wait only applies to a clean single-direction D-pad hold on a
repeatable axis; diagonal holds, non-repeatable axes, and command chords fall
back to the adaptive idle path. While a repeatable D-pad key was held on the
previous scan,
the unchanged-screen path waits for one VBlank instead of entering the longer
HID idle wait; after release it returns to the adaptive low-power idle path.
Repeat buttons stay in the abstract
`APP_CONTROL_BUTTON_*` layer after the initial HID scan instead of being
synthesized back into raw libctru key bits. Repeat starts after about 300 ms
and then fires about every 80 ms, keeping normal taps to one movement while
still making long deck lists usable.

To avoid unnecessary screen work, the main loop only flushes and swaps
framebuffers after drawing a changed screen. Redraws still wait for VBlank.
When the screen is unchanged, the app waits for HID input with an adaptive
timeout before scanning controls again. The wait starts short for responsive
input, then backs off while idle to avoid busy redraw/poll loops while still
letting `aptMainLoop` run regularly. After several seconds of no input, the
unchanged-screen timeout reaches two seconds; HID events still wake it
immediately, and scheduled battery/day checks can be delayed by at most that
timeout while the app is otherwise idle. Any held, newly pressed, or repeated
input resets the idle wait counter. The pure idle-backoff timing policy lives
in `app_power` so host tests cover the fast, mid, and max wait tiers plus the
capped wait counter.

The app tracks the current local calendar day while it is open. The main loop
checks for a local-day change at most once per minute, using the loop's existing
`time()` result so it does not run calendar conversion on every idle tick. If a
valid day change is observed, deck-select rescans summaries, and an active
review session updates the scheduler's `today`, clears one-step undo,
recomputes daily counts, and repositions to the next due card. If a review,
summary, suspend-confirmation, or restore-confirmation screen is visible, the
screen redraws immediately on the correct review or summary surface for the new
day. Other deck-specific screens, such as actions, daily limits, controls, and
exit confirmation, redraw in place after the scheduler update and keep their
current modal context. Their return targets are updated so canceling a modal
lands on the correct review or summary screen for the new day. The status line
also reports the new-day queue result on those in-place screens, with
ignored-settings or unmatched-state context appended when present; redundant
daily-limit and reset-state suffixes are omitted. Unsaved daily-limit and
exit-loses-unsaved-limits warnings stay visible. A deck with malformed saved
state stays on the reset-needed summary across day changes instead of moving
into the review queue.

The app samples PTMU battery state at startup, then normally at most once every
ten minutes. Startup and periodic samples use the same scheduling policy, so a
transient startup read failure gets the short retry interval instead of waiting
for a full ten-minute poll. If PTMU service initialization is unavailable at
startup, the app retries initialization only at the normal battery-poll cadence.
Periodic checks first ask PTMU whether the shell is open; battery level and
charging state are read only when the shell reports open. The app does not call
the battery service on every button press. If the system clock is briefly
unavailable, the periodic poll timer arms itself when a valid clock reading
appears. Closed-shell skips keep the normal ten-minute cadence and avoid
battery-level reads until the shell reports open; transient read failures
schedule a short retry instead of leaving stale status for a full interval. The
bottom screen shows `Battery: unavailable` until a valid open-shell sample is
available, then keeps the last valid sample as a compact `level/5` line,
including charging and low-battery states. Battery status changes redraw the
screen only when that visible status changes.

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

`R` opens a confirmation screen for suspending the current card; `X` on that
screen performs the suspend. This keeps an accidental `R` press from hiding a
card during normal rating flow. Suspending does not count as a review.
Suspended cards are saved in `state.tsv`, treated as not due, skipped by queue
advancement, and included in total card counts. The selector, actions screen,
and summary screen show suspended-card counts so a hidden queue is visible
before restoring cards.

The actions screen can restore all suspended cards in the active deck. This is
the default selected action so opening actions and pressing `A` does not reset
progress during normal study. If suspended cards exist, restore opens a
confirmation screen and `X` performs the restore because restore-all clears the
one-step undo slot. Opening actions preserves active-deck warning context in
the status line, including reset-needed state, ignored settings, unmatched
state, and daily-limit exhaustion. Moving through action items and canceling
actions or action confirmation screens preserves the same warning context.
Selecting reset keeps a warning status even when no active-deck warning is
present. If a deck opens with malformed saved state, reset is selected by
default so the recovery path is direct. Daily limits can be edited from the
same actions screen. Reset remains available by moving the action selection
first, then confirming on a separate reset screen with `X`.
No-op study actions such as undo without an undo slot, suspend without a
current card, or restore with no suspended cards preserve active-deck warning
context in the status line.
If malformed settings are present at the same time, the reset-needed screens
still show the settings warning so fixing state does not hide a second setup
problem.

`L` undoes the most recent rating or suspend action in the active session. The
scheduler stores a single snapshot of the affected card plus queue/session
counters before applying the action. Undo restores that snapshot, clears the
undo slot, returns to review mode, and saves the restored `state.tsv`. Successful
undo feedback preserves active-deck warning context. Loading a deck or restoring
saved card state clears the undo slot.

The app keeps a scheduler rollback snapshot before review actions that mutate
state and then save `state.tsv`. If the save fails after a rating, suspend,
undo, or restore-suspended action, the app restores that snapshot and leaves the
user on a normal workflow screen with the save error visible. Ratings and
failed suspends return to review with the original card active; failed
restore-suspended actions return to the actions screen. This keeps the in-memory
review queue from advancing past the durable SD-card state without leaving a
stale confirmation prompt open.

After a rating, suspend, undo, or restore-suspended action saves `state.tsv`,
the app appends diagnostic rows to `review-log.tsv` with the before/after
scheduler fields for the affected cards, including first/last review day so
daily-limit and migration behavior can be diagnosed from hardware logs. Review
logging is best-effort and normally appended until the next row would exceed
the configured size cap. If the existing file ends with a partial non-newline
row, the log writer rewrites the complete prefix before appending the new row.
A log append or repair failure does not roll back a saved study action; the
bottom status reports `log skipped` so the diagnostic gap is visible.

The bottom status line reports successful ratings with the next card index, and
reports save failures as non-advancing actions. This is intentionally redundant
with the top-screen state string because SD-card save failures are otherwise
easy to mistake for scheduler bugs during emulator or hardware testing.
Successful rating, suspend, restore, undo, reset, and daily-limit save feedback
appends active-deck warning context unless a save-failure or redundant
daily-limit/reset-state message has priority.

`SELECT` opens an actions screen from review and summary modes. Choosing reset
opens a confirmation screen. Pressing `X` there removes active state recovery
files before the primary `state.tsv`, then removes `review-log.tsv` as
diagnostic cleanup and reloads the selected deck. If state removal and reload
succeed, the bottom status confirms `Progress reset`; if only the diagnostic
log cleanup fails, progress still stays reset and the status reports that the
log was kept. Reset success feedback appends active-deck warning context when
present, such as ignored settings or daily-limit exhaustion after reload. If
state removal fails, the app leaves the current session in place and shows
`reset failed`.

`START` opens an exit confirmation screen from every normal app mode. Pressing
`A` there exits the app; `B` or `SELECT` cancels back to the previous mode. This
keeps the Homebrew-style exit path available while avoiding accidental exits
during review or settings edits.

`reviewed_count` and `rating_counts` are live session counters. Restored state
contributes to per-card `review_count`, but not to the current session's
rating-count totals. The summary screen separately derives a persisted cards
reviewed-today count from each card's `last_review_day`, so relaunching the app
does not make today's hidden cards look untouched.

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
Deck selector stats still show default-based counts in that case, but mark the
settings as ignored and show the first settings parse reason so the fallback is
visible.
Settings save feedback stays in the settings/status messages and does not
overwrite the review-state status line. Saving settings after a malformed
review-state load still leaves the deck on the reset-needed summary; settings
changes do not make an unsafe review queue visible. Limits-save success
feedback preserves active deck warning context, while omitting redundant
reset-state or daily-limit suffixes. Opening the daily-limits screen, moving
between fields before edits, and cycling a value back to its saved value
preserve active deck warning context; unsaved edit feedback takes
priority until save or cancel replaces it. Canceling daily-limit edits returns
to actions with discarded-edit feedback and the active deck warning suffix.

The scheduler stores `first_review_day` and `last_review_day` in `state.tsv` so
daily limits survive relaunch. New-card limits apply to unstarted new cards in
deck order. Review limits apply to unstarted normal review cards by review
priority: older due days first, then deck order as the tiebreaker. Zero-day
learning/relearning cards remain due even when the review limit is full, and a
card already started today is allowed to remain due. This lets same-day Again
loops finish instead of hiding half-reviewed cards behind a limit.
Previous state formats can load reviewed cards with unknown first/last review
days. If the migrated card is still in initial learning (`interval_days=0`,
`lapses=0`), the next rating initializes both review-day fields and counts the
card as newly introduced for the day. If the migrated card is a normal review
or relearning card, the next rating keeps `first_review_day` unknown and
records `last_review_day` as the current local day. That preserves
review-limit accounting: migrated reviewed cards count as reviews, not as newly
introduced cards. Truly new cards still initialize both review-day fields to
the current local day on their first rating.
When daily limits hide otherwise calendar-due cards and the visible queue is
empty, the summary shows `Daily limit reached` plus the count of new and review
cards past the limit instead of presenting the deck as simply done.

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

Restoring saved card rows recomputes daily counters, due count, and current-card
position immediately. This keeps direct scheduler callers from leaving
`current_index` on a hidden or future card while another card is due.

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
7. Reject media options and inline `<img>` tags so image-bearing exports do not
   silently become text-only cards.
8. Remove obsolete `media/` output left by older non-text builds.
9. Stage `deck.json` and `cards.tsv` in temporary files, then replace both
   final files with rollback if either commit step fails.
10. Write default `settings.tsv` if it does not already exist.
11. For `--split-large-decks`, remove obsolete converter-generated sibling
    chunk folders after the current single-folder or split-folder output has
    been written.
12. When a converter-generated single-folder deck becomes split output, copy
    matching `state.tsv` rows and daily-limit settings into new chunk folders
    before deleting the obsolete single folder.
13. When converter-generated split chunks become one single-folder output,
    combine matching `state.tsv` rows and copy daily-limit settings into the
    new single folder before deleting the obsolete chunk folders.
14. When split output stays split but card boundaries move between numbered
    chunks, rewrite matching `state.tsv` rows into the chunk that now owns each
    card. If a rewritten chunk has no matching rows, remove stale state files so
    the chunk starts fresh.

During same-folder re-import, the converter deliberately does not open or
rewrite existing `state.tsv`, `review-log.tsv`, or `settings.tsv`, so review
progress, diagnostic history, and deck-specific daily limits survive updates in
that deck folder.
The old `media/` subdirectory and obsolete split chunks are different: once the
current import can no longer use them, those stale outputs are removed so they
do not remain part of the 3DS text-card workflow. To keep progress attached to
edited card text, pass stable source ID fields during conversion; otherwise
content-derived fallback IDs change when the normalized front/back/tags content
changes. The folder id is the runtime deck id on the 3DS, so the converter
defaults `deck_id` from the output folder name and rejects mismatches. Rich
HTML/template rendering, media conversion, and Anki collection parsing still
belong outside this 3DS text-card scope.

## C Boundaries We Want

Keep the portable logic separate from the libctru shell:

| Boundary | Owns | Should Not Own |
| --- | --- | --- |
| `deck_index` | deck folder scan, deck id validation, deck-local path construction | card parsing, review state parsing, rendering |
| `deck` | `cards.tsv` parsing, card/deck structs, parse/load errors | input handling, review progress, UI |
| `scheduler` | per-card session state, rating transitions, due counts, current-card selection | file paths, card text parsing, rendering |
| `review_state` | `state.tsv` load/save, card-id matching, persistence errors | deck discovery, button mapping, screens |
| `review_log` | diagnostic study transition rows and partial-row repair | scheduler decisions, rollback policy, rendering |
| `storage` | temp/backup save-file replacement and cleanup | TSV formatting, scheduler state, settings parsing |
| `app_layout` | screen geometry constants and pure fit checks | rendering side effects, text wrapping |
| `app_power` | battery status thresholds, poll scheduling policy, idle input wait tiers | libctru PTMU calls, rendering |
| `app_review` | pure review-queue eligibility from state-load result and scheduler due state | rendering, button mapping, file I/O |
| `app_status` | pure status-message classification for console color semantics | libctru color escapes, rendering |
| `app_controls` | abstract button bits, repeat timing, app command classification | scheduler mutation, file I/O, rendering |
| `app_text` | UTF-8 character stepping for wrapping/truncation | font shaping, rich text layout |
| `app` | top-level mode machine, libctru input/render loop, active deck selection | TSV parsing details, scheduler internals |
| converter | desktop import, stable IDs, deck folder writes | local 3DS progress mutation |

Battery sampling is intentionally coarse. The main loop samples PTMU at startup
and then normally performs at most one shell-state check every ten minutes.
Battery level and charging state are read only when the shell reports open.
Closed-shell skips keep the ten-minute cadence. Transient PTMU read failures,
including startup failures, schedule a short retry and keep the last valid
battery display, or the explicit unavailable display before the first valid
sample, instead of clearing it.

The console renderer still uses a simple one-column-per-character model, but
`app_text` keeps valid UTF-8 byte sequences together during wrapping and
ellipsis truncation so language-deck text is not split in the middle of a
character. Long bottom status messages compact the left side first when a
semicolon-delimited warning suffix is present, keeping context such as ignored
settings or unmatched state visible within the one-line status area.

Likely next boundaries:

- `ui`: screen drawing and button labels, once the app has more than a few
  screens.

Avoid generic containers, callback-heavy APIs, object systems, and global
service locators. Fixed limits and explicit enums are still the right shape
until a real deck or hardware test proves otherwise.
