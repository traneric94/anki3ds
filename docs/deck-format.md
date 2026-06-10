# Draft Deck Format

The 3DS app should read a simple format that avoids full Anki complexity.
Both `converter/anki3ds_convert.py` and `tools/import_anki_collection.py` write
this same on-device format.

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
Personal imports from a local Anki collection should be written to ignored
SD-card mirrors such as `local/sdmc/3ds/anki3ds/decks/` or Azahar's
`sdmc/3ds/anki3ds/decks/` directory, not to tracked sample-deck fixtures.

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

`deck.json` object keys must be unique.
`deck_id` must match the folder id. The converter defaults it from the output
folder name, the direct Anki importer derives portable ids from deck names, and
both writers reject mismatches so desktop output stays loadable on-device.
Folder ids are limited to 63 ASCII letters, numbers, `_`, or `-` characters,
which leaves space for the C string terminator in the 3DS app's fixed buffers.
The 3DS app reads the optional `name` string for deck-list and review-screen
display, falling back to the folder id when metadata is missing or malformed.
Converter and package verification require this name to be non-empty, free of
control characters, and no longer than 63 UTF-8 bytes so it fits the app's fixed
display buffer. Store `name` as literal UTF-8; the 3DS display parser only
accepts `\"`, `\\`, and `\/` JSON escapes. Other escapes, including `\u00e9`,
pass JSON parsing but fall back to the folder id on device.
The direct Anki collection importer refuses tracked repo output by default so
personal card text is staged under ignored local/Azahar SD-card mirrors unless
`--allow-tracked-output` is explicitly used for non-personal fixtures.
`tools/verify_text_deck.py` is strict by default and rejects app-owned progress
artifacts so tracked/package payloads stay clean. When validating a live
local/Azahar SD-card deck that may already contain `state.tsv`,
`review-log.tsv`, or temp/backup files, pass `--allow-progress-files`; this
still validates `deck.json`, `cards.tsv`, and `settings.tsv` and still rejects
unknown entries such as media folders.

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
- current 3DS builds support at most 1024 cards per deck
- converter `--split-large-decks` writes oversized exports as up to 64 numbered
  sibling deck folders within this limit
- split re-import removes stale converter-generated sibling chunks after the
  current output is written
- if a converter-generated single-folder deck grows into split chunks, matching
  saved state rows, review-log rows, and daily-limit settings are copied into
  the new chunk folders before the obsolete single folder is removed
- if converter-generated split chunks shrink back into one deck, matching saved
  state rows, review-log rows, and daily-limit settings are copied into the new
  single folder before the obsolete chunks are removed
- if split output remains split but card boundaries move between numbered
  chunks, matching saved state rows and review-log rows are rewritten into the
  chunk that now owns each card; chunks with no matching saved rows start fresh
  instead of keeping stale state from their previous card ranges, and chunks
  with no matching review-log rows drop stale diagnostic logs
- text-only re-import removes a stale `media/` directory from the active output
  folder while preserving progress and settings files
- `front` and `back` fields may use at most 383 UTF-8 bytes after unescaping
- long front fields can be scrolled with circle-pad Up/Down; long back fields
  can be scrolled with D-pad Up/Down after reveal
- `tags` may use at most 127 UTF-8 bytes after unescaping
- non-empty `tags` are shown on the revealed answer side as separate tag chips
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

- `new_limit`: cards whose answer may be revealed for the first time today in
  the active clean shell, `0` means unlimited. The app preserves lifetime
  introduced-card flags and resets only the per-day introduced counter when the
  calendar day changes.
- `review_limit`: accepted ratings for the current day in the active clean
  shell, `0` means unlimited.
- `learning_mode`: optional queue policy. `0` is due-first scheduling;
  `1` enables card-count cooldowns so same-session repeats are spaced by
  other cards.

Both limit rows must be present. If `learning_mode` is absent, the app uses
due-first scheduling. If `settings.tsv` is missing or invalid, the app uses
defaults. Invalid settings are shown as ignored on device:

```text
new_limit	20
review_limit	200
learning_mode	0
```

Required setting keys must appear exactly once, and optional setting keys may
appear at most once. Duplicate settings rows are malformed. Daily-limit values
must be non-negative integers no larger than `1000000`; `learning_mode` must be
`0` or `1`. Blank rows and rows beginning with `#` are ignored.

The active clean shell saves setting edits through `settings.tsv.tmp`,
rotates the previous primary file to `settings.tsv.bak`, and then renames the
temp file into place. If `settings.tsv` is missing or malformed after an
interrupted save, the loader can recover a valid `settings.tsv.tmp` or
`settings.tsv.bak`.

## state.tsv

The active clean Citro2D shell currently writes a compact progress state beside
the selected deck:

```text
version<TAB>2
card_count<TAB>card_count
current_index<TAB>current_index
progress_day<TAB>day_number
reviewed_count<TAB>reviewed_count
reviewed_today_count<TAB>reviewed_today_count
introduced_count<TAB>introduced_count
introduced_today_count<TAB>introduced_today_count
introduced_index<TAB>card_index
schedule_index<TAB>card_index
schedule_due_day<TAB>day_number
schedule_interval_days<TAB>interval_days
completed_today_count<TAB>completed_today_count
completed_today_index<TAB>card_index
again_count<TAB>again_count
hard_count<TAB>hard_count
good_count<TAB>good_count
easy_count<TAB>easy_count
suspended_count<TAB>suspended_count
suspended_index<TAB>card_index
```

This checkpoint format is owned by `study_backend`. It restores the active card
index, total rating counters, current-day rating/introduced counters,
same-day completed-card indexes, cards whose answer has been revealed at least
once, per-card due-day schedules, and index-based suspended cards for the
selected deck's `state.tsv`;
one-step undo is session-local and intentionally not persisted. The active
queue offers due introduced cards that have not been completed today before
offering new cards; `Again` leaves the current card due, while Hard/Good/Easy
mark it completed for the day and schedule it into the future. The current
compact scheduler is day-based: Again is due today with interval `0`, Hard uses
the previous interval or starts at `1`, Good doubles the previous interval or
starts at `2`, and Easy triples the previous interval or starts at `4`.
Intervals cap at `36500` days. `progress_day` is local calendar days since
1970-01-01; when the loaded day is older than the current day, the app resets
`reviewed_today_count`, `introduced_today_count`, and the completed-today
bitmap, preserves total progress and introduced/suspended indexes, and rebuilds
the active queue so already introduced cards are reviewed before new cards
consume the new-card limit. Missing state is normal. Version `1` compact state
files load without schedule rows and treat introduced cards as due now. Older
compact state files
without `progress_day`, `reviewed_today_count`, or `introduced_today_count` load
with day unknown and today counters initialized from the existing totals; the
first current-day open establishes `progress_day` without rewinding the active
index. Older compact state files without `completed_today_count` infer same-day
completed cards before `current_index` when possible so same-day reloads do not
reopen already-rated cards. Older compact state files without `introduced_count`
infer introduced cards from existing progress. Older compact state files without
`suspended_count` load with no suspended cards. Schedule rows are optional in
version `2`; when present, each `schedule_index`, `schedule_due_day`, and
`schedule_interval_days` triple must refer to an introduced card.
Malformed state, mismatched
`card_count`, impossible today counters, invalid schedule triples,
completed-today indexes that are not introduced, duplicate
introduced/schedule/completed/suspended indexes, or indexes outside the deck
leaves the live backend on a fresh session and shows a bad-state status
string. The app saves this file after a successful answer reveal, rating, undo,
confirmed suspend, confirmed restore, or active day rollover, using
`state.tsv.tmp` and `state.tsv.bak` so the next load can recover from an
interrupted save. Confirmed reset clears the in-memory compact progress and
removes `state.tsv`, `state.tsv.tmp`, and `state.tsv.bak` for the selected deck
instead of writing a fresh empty state.

The full M7 daily-use state format below is the target shape for the rebuilt
scheduler/session flow. Those files are framed by a header and footer. The
number is the count of
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
- `first_review_day` may be `0` while `last_review_day` is known for reviewed
  cards migrated from older state formats
- if `first_review_day` is known, `last_review_day` must also be known and
  `first_review_day` must be less than or equal to `last_review_day`
- unknown card IDs are ignored when loading state
- if a valid state file has rows but none match current card IDs, the app starts
  a fresh queue and shows an unmatched-state warning
- duplicate rows for the same current deck `card_id` are malformed
- footerless ten-column state rows still load as migration data
- previous eight-column state rows still load with first/last review day as `0`;
  after the next rating, migrated initial-learning cards record both review
  days, while migrated normal review/relearning cards keep first day unknown
  and record the new last review day
- previous seven-column state rows still load with `suspended=0`
- old four-column state rows, `card_id done review_count last_rating`, still load
  as a migration path

`state.tsv` and `review-log.tsv` are owned by the 3DS app. The converter should
preserve them when updating card content. When split output changes shape, the
converter preserves complete `review-log.tsv` rows whose `card_id` still exists
in the target deck or chunk. Rows must be newline-terminated and use the
expected field count; incomplete final log rows are ignored, matching the app's
partial-row repair policy. Rewritten migrated logs keep the newest matching
rows whose newline-terminated UTF-8 encoding fits the same 262144-byte device
cap used by the app.

The app may briefly create `state.tsv.tmp` and `state.tsv.bak` while saving.
If `state.tsv` is missing or malformed after an interrupted save, the app can
load a valid `state.tsv.tmp` or `state.tsv.bak`. If all available state copies
are malformed, normal review-state saves are blocked until deck progress is
reset. The reset-needed screens show the first malformed state line or
state-file reason when available.

This is an early day-level spaced repetition format. Minute-level learning
steps, single-card unsuspend UI, burying, and filtered decks are planned later.
Older app builds that only accept unframed seven- or eight-column rows will
reject state saved by this version.

## Review Log

The active clean shell appends compact study transitions to `review-log.tsv`
beside `state.tsv` after a rating, suspend, undo, or restore-suspended action
has been accepted and `state.tsv` has saved. The log is diagnostic and normally
appended; repairing an interrupted final row may rewrite the complete prefix
before appending. A failed append, or an intentional low-battery diagnostic
skip, does not roll back the review state or block the study action. The app
reports `log skipped` in the status line when a saved action was not logged.

```text
timestamp<TAB>event<TAB>rating<TAB>card_index<TAB>reviewed_count<TAB>suspended_count
```

Current clean-shell rules:

- `timestamp` is Unix time in seconds, or `0` if the clock is unavailable
- `event` is `rating`, `suspend`, `undo`, or `restore`
- `rating` is `again`, `hard`, `good`, `easy`, or `-` for non-rating events
- `card_index` is the zero-based active index after the accepted transition
- `reviewed_count` is the clean-shell total reviewed counter after the accepted
  transition; `suspended_count` is the active suspended-card count
- the app stops appending when the next row would exceed 262144 bytes
- if the existing log does not end in a newline, the app rewrites the complete
  prefix before appending so a new row is not concatenated onto a partial
  interrupted row

`tools/verify_m7_artifacts.py` accepts this compact state/log pair as current
clean-shell checkpoint evidence. It still also accepts the fuller scheduler
state/log formats below, so the post-run evidence gate can bridge the current
Citro2D shell and the later full scheduler rebuild without weakening either
format's validation rules.

The fuller M7 scheduler log target is:

```text
timestamp<TAB>day<TAB>event<TAB>card_id<TAB>rating<TAB>old_review_count<TAB>old_first_review_day<TAB>old_last_review_day<TAB>old_due_day<TAB>old_interval_days<TAB>old_ease_permille<TAB>old_lapses<TAB>old_suspended<TAB>new_review_count<TAB>new_first_review_day<TAB>new_last_review_day<TAB>new_due_day<TAB>new_interval_days<TAB>new_ease_permille<TAB>new_lapses<TAB>new_suspended
```

Target scheduler-log rules:

- `timestamp` is Unix time in seconds, or `0` if the clock is unavailable
- `day` is local calendar days since 1970-01-01
- `event` is `rating`, `suspend`, `undo`, or `restore`
- `card_id` must not include tabs or newlines
- `rating` is `again`, `hard`, `good`, `easy`, or `-` for non-rating events
- old/new scheduler fields use the same meanings as `state.tsv`
This can support debugging and possible desktop import later. The current
one-step undo is still an in-memory snapshot saved back to `state.tsv`; the
review log is not read by the 3DS app.

Resetting deck progress removes `state.tsv.tmp` and `state.tsv.bak` before the
primary `state.tsv`, then removes `review-log.tsv`, `review-log.tsv.tmp`, and
`review-log.tsv.bak` as diagnostic cleanup. A log delete failure does not
restore review progress.

## session.tsv

The active clean shell writes root diagnostics at:

```text
sdmc:/3ds/anki3ds/session.tsv
```

The file is framed so interrupted writes can be rejected or recovered from
`session.tsv.tmp` / `session.tsv.bak`:

```text
#anki3ds-session-v1
key<TAB>value
#anki3ds-session-complete
```

Current keys match the post-run verifier shape: `started_at`, `updated_at`,
`launch_count`, `started_day`, `current_day`, `scan_completed`, `deck_count`,
`ignored_count`, `deck_open_count`, `review_screen_count`,
`summary_screen_count`, `load_error_count`, `answer_shown_count`,
`rating_saved_count`, `undo_saved_count`, `suspend_saved_count`,
`restore_saved_count`, `settings_saved_count`, `reset_progress_count`,
`exit_confirmed`, `last_deck_id`, and `last_event`.

The clean shell loads an existing complete `session.tsv` on boot, preserves
saved-action counters across relaunches, increments `launch_count`, records
scan/deck-open/review events, and writes `last_event=exit_confirmed` only when
the exit confirmation is accepted with `A`.

## Compatibility Policy

Once the 3DS app reads a released format version, future converter versions
should either keep writing that version or include a migration path.
