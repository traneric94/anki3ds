# anki3ds

A small Nintendo 3DS text flash-card reviewer for Anki-derived plain-text decks.

This repo contains a simple 3DS homebrew reviewer plus desktop import tools
that turn Anki-style tab-separated exports or read-only Anki collection copies
into a 3DS-friendly deck format.

## Scope

The first usable version is not a full Anki clone. Its product scope is text
flash cards only. It does:

- review text flash cards on a Nintendo 3DS
- read decks from the SD card
- save local review progress
- use a simple Anki-like scheduler
- import decks through desktop text-deck tools

The first version does not:

- sync with AnkiWeb
- parse arbitrary Anki templates on-device
- run JavaScript or full card CSS on-device
- support image, audio, or other media review
- modify a user's main Anki collection directly

## Components

```text
anki3ds/
  app-3ds/          3DS homebrew app in C/libctru
  converter/        tab-separated text export converter in Python
  docs/             design notes and test logs
  sample-decks/     tiny non-copyrighted sample decks
  tools/            verifiers, asset tools, and direct Anki DB importer
```

## Build

The 3DS app currently builds a text-card multi-deck reviewer.

```sh
make
```

This produces:

```text
app-3ds/anki3ds.3dsx
app-3ds/anki3ds.smdh
```

To run host-side parser, scheduler, review-state, converter, and verifier
tests:

```sh
make test
```

To run the portable CI gate locally:

```sh
make verify-ci
```

That runs host tests, converter tests, and tracked sample-deck verification.

To copy the build and tracked sample decks into the gitignored local SD mirror:

```sh
make install-local-sd
```

This target installs only the tracked sample deck set; personal Anki imports
stay in whatever SD deck folders you imported separately.

To copy and verify a fresh tracked-sample local SD mirror in one step:

```sh
make verify-local-sd
```

That target clears tracked sample progress before verification.

To stage a clean SD-card payload under `dist/sdmc/` for release or manual copy:

```sh
make package-sd
```

This includes `anki3ds.3dsx`, `anki3ds.smdh`, and the tracked sample decks
without generated progress files. `PACKAGE_SDMC` must remain under `dist/`
because this target cleans its packaged app directory before staging.
To build and verify that copy-ready payload in one step, run:

```sh
make verify-package-sd
```

That check confirms the staged payload has the app artifacts, only the default
text sample decks, valid five-field text-card rows, valid settings, no generated
progress files, and no media or extra files inside those text deck folders.

For a fresh sample-deck pass, use:

```sh
make prepare-local-samples-fresh
```

That installs the build and tracked sample decks, then removes only sample-deck
`state.tsv` files plus `review-log.tsv` recovery files from the local SD
mirror. During sample installation, tracked sample folders are cleaned down to
existing `state.tsv`/`review-log.tsv` progress and recovery artifacts before
source deck files are copied, so stale media, temp/backup files, and other
stray files do not require manual cleanup. It does not remove progress for
personal decks outside the tracked sample ids.

This also installs the tracked sample decks to:

```text
local/sdmc/3ds/anki3ds/decks/limits-demo/cards.tsv
local/sdmc/3ds/anki3ds/decks/sample/cards.tsv
```

To install the sample decks into Azahar's SD card data directory:

```sh
make install-azahar-sample-decks
```

This installs only the tracked sample deck set. It does not import, remove, or
reset personal Anki decks in the same Azahar SD directory.

For a fresh Azahar sample-deck pass, use:

```sh
make prepare-azahar-samples-fresh
```

To prepare and verify those fresh Azahar sample decks before launch:

```sh
make verify-azahar-fresh-samples
```

By default this uses:

```text
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/limits-demo/cards.tsv
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/sample/cards.tsv
```

Override `LOCAL_SDMC` or `AZAHAR_SDMC` if your local mirror or emulator data
directory lives somewhere else.

To verify the tracked sample decks without building the 3DS app:

```sh
make verify-sample-decks
```

Both `verify-sample-decks` and `verify-package-sd` use
`tools/verify_text_deck.py` for the text-deck checks. See
[sample-decks/README.md](sample-decks/README.md) for the sample-deck workflow
and what each tracked deck is meant to cover.
Successful verifier runs print a one-line summary for each deck with its name,
card count, and daily limits; pass `--quiet` to print only validation errors.

To launch the current `.3dsx` in Azahar from a normal macOS session:

```sh
make run-emulator
```

To update only Azahar's SD-installed app binary without touching decks,
themes, session files, or progress:

```sh
make install-azahar-app
```

To prepare Azahar for a manual daily-use pass on imported personal decks
without resetting progress, use:

```sh
make prepare-azahar-daily-use
```

This installs the current app binary and FE assets, verifies the Azahar control
profile, and validates the configured personal deck folders with live progress
files allowed. To prepare and launch in one command:

```sh
make run-emulator-daily-use
```

To install the tracked sample decks into Azahar's SD directory and launch the
current build in one command:

```sh
make run-emulator-samples
```

This target is for sample-deck passes. Personal Anki decks must be imported
with `tools/import_anki_collection.py` before launching if you want them visible
in the selector.

For the same launch with tracked sample progress cleared first:

```sh
make run-emulator-fresh-samples
```

By default this expects Azahar at:

```text
~/Applications/azahar-macos-arm64-2125.1.2/Azahar.app
```

### Tab-Separated Export

To convert a simple tab-separated export into an anki3ds deck:

```sh
python3 converter/anki3ds_convert.py export.tsv sample-decks/my-deck \
  --deck-id my-deck \
  --deck-name "My Deck" \
  --text-only
```

The converter simplifies simple HTML in exported fields before writing
`cards.tsv`: formatting tags are removed, block tags and `<br>` become line
breaks, entities such as `&nbsp;` are decoded, and script/style content is
dropped.

Text flash cards are the only supported import path. `--text-only` is accepted
for compatibility with earlier commands; the converter always accepts plain
text front/back fields and rejects media options plus inline image tags.
Re-importing a deck also removes any stale `media/` directory from that output
folder while preserving saved review state, review logs, and daily-limit
settings.

Conversion failures print a concise `error: ...` message and exit nonzero so
the input can be fixed without reading a Python traceback.

### Direct Anki Collection Import

To import Basic-style text decks directly from a local Anki collection copy,
write them to an ignored SD-card mirror instead of `sample-decks/`:

```sh
cp "$HOME/Library/Application Support/Anki2/User 1/collection.anki2" \
  /private/tmp/anki3ds-collection.anki2
python3 tools/import_anki_collection.py /private/tmp/anki3ds-collection.anki2 --list
python3 tools/import_anki_collection.py /private/tmp/anki3ds-collection.anki2 \
  local/sdmc/3ds/anki3ds/decks --deck RecSys
python3 tools/verify_text_deck.py local/sdmc/3ds/anki3ds/decks/recsys
```

The direct importer opens the collection read-only, registers Anki's `unicase`
SQLite collation, reads deck/card/note/field rows, and reuses the same text
deck writer as the tab-separated converter. It supports text-only Front/Back
decks, carries note tags into the fifth `cards.tsv` field, rejects inline image
HTML and Anki sound/media markers, refuses tracked repo output unless
explicitly overridden, and preserves app progress/settings when re-importing
into an existing deck folder. To stage decks for Azahar, use:

```sh
python3 tools/import_anki_collection.py /private/tmp/anki3ds-collection.anki2 \
  "$HOME/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks"
python3 tools/verify_text_deck.py --allow-progress-files \
  "$HOME/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/recsys"
```

Use `--allow-progress-files` only for live local/Azahar SD-card deck folders
where the app may have already written `state.tsv`, `review-log.tsv`, or
temporary/backup files. Omit it for tracked fixtures, package payloads, and
release checks so progress artifacts are still rejected.
For the current local personal deck set, the equivalent Make targets are:

```sh
make verify-local-personal-decks
make verify-azahar-personal-decks
```

Override `PERSONAL_DECKS="deck-a deck-b"` when checking a different set.

Personal imports are intentionally written under ignored local/Azahar SD-card
directories. Do not commit those generated deck folders.

If your export includes durable source identifiers, pass them through so
re-imported card text keeps the same on-device review state:

```sh
python3 converter/anki3ds_convert.py export.tsv sample-decks/my-deck \
  --deck-id my-deck \
  --deck-name "My Deck" \
  --note-id-field 0 \
  --card-id-field 1 \
  --front-field 2 \
  --back-field 3 \
  --text-only
```

Current 3DS builds support 1024 cards per deck folder and store up to 64 deck
folders in the selector. To split a larger export into numbered sibling decks
such as `my-deck-01` and `my-deck-02`:

```sh
python3 converter/anki3ds_convert.py export.tsv sample-decks/my-deck \
  --deck-id my-deck \
  --deck-name "My Deck" \
  --text-only \
  --split-large-decks
```

On re-import with `--split-large-decks`, the converter removes stale
converter-generated split siblings for that deck after the current output is
written, so old chunks do not remain visible on the 3DS after a deck shrinks or
changes between single-folder and split-folder output.
When a converter-generated single-folder deck grows into split chunks, matching
saved state rows and daily-limit settings are copied into the new chunk folders
before the obsolete single folder is removed.
When split chunks shrink back into a single deck, matching state rows from the
obsolete chunks and daily-limit settings are copied into the new single folder
before the chunks are removed.
When split output remains split but card boundaries move between numbered
chunks, matching state rows are rewritten into the chunk that now owns each
card. A chunk with no matching saved rows starts fresh instead of keeping stale
state from its previous card range.

## Current Status

Multi-deck text review works at build level: the app scans
`sdmc:/3ds/anki3ds/decks`, lets you select a deck folder containing `cards.tsv`,
shows the selected deck position plus explicit `Due`, `New`, and `Susp`
counts in the multi-deck selector, reports ignored non-hidden entries during
deck scans, and summarizes all visible deck stats in the bottom footer as
`All decks: Due ... | New ... | Susp ...`,
loads optional per-deck `settings.tsv` study settings, can
edit Study Settings with `X` before reveal, including daily limits and learning
mode, reveals answers,
records ratings, schedules cards with a
day-level spaced repetition algorithm, saves local `state.tsv` progress beside
that deck, and can undo the last rating with `B`, suspend
cards after opening suspend confirmation with `R` and confirming with `X`, see
suspended-card counts, restore suspended cards from the completion screen with
`R` then `X`, review introduced cards again from the completion screen with
`A` without resetting progress, or reset saved progress by pressing `Y` before
reveal and confirming with `X`.

Startup uses the centralized Citro2D FE shell: generated Forest background
art, parchment dialogue panels, and renderer-owned text drawn from
backend/display strings at one shared text scale. Before reveal, the active
prompt is shown on the review parchment; after reveal, the answer and rating
surface are shown from the same renderer-facing screen model. Revealed cards
show tags as separate answer-side chips when the deck row has tags; daily
limits are otherwise labeled as `Today New` and `Review`. The bottom review
body shows help only when explicitly opened before reveal, then reveal closes
that help surface so answer text has the bottom parchment. `START` expands or
hides detailed `button: action` help on the deck selector and review screens.
`B` is undo in review and cancel on settings/confirmation screens; `L` is the
Hard rating after reveal. Deck and
review status, save feedback, and battery state are also shown, including an
unavailable state before the first valid sample plus charging and low-battery
states once sampled. All active drawing is in the renderer; backend/study
modules emit strings and state only.
Answer reveal now persists introduced-card progress for the clean-shell
`new_limit`; the clean shell stores local-day new/review counters and resets
only those daily counters when the local calendar day changes. Ratings now save
compact day-based due schedules: Again stays due today, Hard starts at one day,
Good starts at two days and doubles, and Easy starts at four days and triples.
Version-1 compact states still load as due-now progress. On a true
day rollover after a saved `progress_day` exists, it clears same-day completed
card markers and rebuilds the queue so due introduced cards are reviewed before
new cards consume `new_limit`. Legacy compact states opened without
`progress_day` only establish the current day on first load and do not rewind.
Successful ratings,
suspend actions, undo actions, and
restore-suspended actions append best-effort `review-log.tsv` rows for
debugging when battery state allows the optional write; progress state still
saves if that diagnostic append is skipped. The no-due summary separates
current-session rating counts from the
persisted count of cards reviewed today and distinguishes daily-limit exhaustion
from a fully clear deck. Valid saved state with no matching current card ids
starts fresh with a visible unmatched-state warning. Deck load errors show the
first failing line and parser reason when available. Direct no-op commands give
visible feedback: `B` with no undo history reports `Nothing to undo`, and `R`
on a completed deck with no suspended cards reports `Nothing suspended`.

See:

- [PROJECT_PLAN.md](PROJECT_PLAN.md)
- [CHECKPOINTS.md](CHECKPOINTS.md)
- [TESTING.md](TESTING.md)
- [docs/control-map.md](docs/control-map.md)
- [docs/algorithms-and-abstractions.md](docs/algorithms-and-abstractions.md)
- [docs/theme-assets.md](docs/theme-assets.md)
- [docs/deck-format.md](docs/deck-format.md)
- [docs/c-style-and-architecture.md](docs/c-style-and-architecture.md)
- [docs/emulator-feedback-loop.md](docs/emulator-feedback-loop.md)
- [docs/emulator-test-log.md](docs/emulator-test-log.md)
- [docs/device-test-log.md](docs/device-test-log.md)

## Non-Goals

This project is not for distributing copyrighted games, BIOS files, ROMs, or
commercial Anki content. Test decks should be original or openly licensed.
