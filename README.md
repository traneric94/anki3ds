# anki3ds

A small Nintendo 3DS flashcard reviewer for Anki-derived decks.

This repo contains a simple 3DS homebrew reviewer plus a desktop converter that
turns Anki-style tab-separated exports into a 3DS-friendly deck format.

## Scope

The first usable version is not a full Anki clone. It does:

- review text flash cards on a Nintendo 3DS
- read decks from the SD card
- save local review progress
- use a simple Anki-like scheduler
- import decks through a desktop converter

The first version does not:

- sync with AnkiWeb
- parse arbitrary Anki templates on-device
- run JavaScript or full card CSS on-device
- treat image/media review as optional non-MVP work
- modify a user's main Anki collection directly

## Components

```text
anki3ds/
  app-3ds/          3DS homebrew app in C/libctru
  converter/        desktop converter in Python
  docs/             design notes and test logs
  sample-decks/     tiny non-copyrighted sample decks
```

## Build

The 3DS app currently builds a text-card multi-deck reviewer. Existing media
helpers are optional fixtures, not part of the daily-use MVP.

```sh
make
```

This produces:

```text
app-3ds/anki3ds.3dsx
app-3ds/anki3ds.smdh
```

To run host-side parser, scheduler, and review-state tests:

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
text sample decks, valid five-field text-card rows, valid settings, and no
generated progress files.

For a fresh sample-deck pass, use:

```sh
make prepare-local-samples-fresh
```

That installs the build and tracked sample decks, then removes only sample-deck
`state.tsv` files plus `review-log.tsv` recovery files from the local SD
mirror. It does not remove progress for personal decks outside the tracked
sample ids.

This also installs the tracked sample decks to:

```text
local/sdmc/3ds/anki3ds/decks/limits-demo/cards.tsv
local/sdmc/3ds/anki3ds/decks/sample/cards.tsv
```

To install the sample decks into Azahar's SD card data directory:

```sh
make install-azahar-sample-deck
```

For a fresh Azahar sample-deck pass, use:

```sh
make prepare-azahar-samples-fresh
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

To launch the current `.3dsx` in Azahar from a normal macOS session:

```sh
make run-emulator
```

To install the tracked sample decks into Azahar's SD directory and launch the
current build in one command:

```sh
make run-emulator-samples
```

For the same launch with tracked sample progress cleared first:

```sh
make run-emulator-fresh-samples
```

By default this expects Azahar at:

```text
~/Applications/azahar-macos-arm64-2125.1.2/Azahar.app
```

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

`--text-only` is the recommended daily-use mode: it accepts plain text front
and back fields, but rejects media options and inline image tags.

Conversion failures print a concise `error: ...` message and exit nonzero so
the input can be fixed without reading a Python traceback.

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

Current 3DS builds support 256 cards per deck folder and store up to 64 deck
folders in the selector. To split a larger export into numbered sibling decks
such as `my-deck-01` and `my-deck-02`:

```sh
python3 converter/anki3ds_convert.py export.tsv sample-decks/my-deck \
  --deck-id my-deck \
  --deck-name "My Deck" \
  --text-only \
  --split-large-decks
```

Text cards are the supported daily-use scope. Optional media fields can be
copied from existing `.a3i` images or converted from binary PPM `P6` images
into the device-side `.a3i` format only when `--text-only` is omitted:

```sh
python3 converter/anki3ds_convert.py export.tsv sample-decks/my-deck \
  --deck-id my-deck \
  --deck-name "My Deck" \
  --front-media-field 3 \
  --media-root path/to/media
```

Use `--media-root` for new media decks. If media fields are used without
`--media-root`, each referenced `.a3i` file must already exist under the output
deck's `media/` directory and pass `.a3i` validation before `cards.tsv` is
written.

If exported front/back HTML contains an inline `<img>` tag, map that side to a
non-empty `--front-media-field` or `--back-media-field`. The converter rejects
inline images without an explicit media filename so images are not silently
dropped.

## Current Status

Multi-deck text review works at build level: the app scans
`sdmc:/3ds/anki3ds/decks`, lets you select a deck folder containing `cards.tsv`,
shows the selected deck position plus new/learning/review due counts in the
multi-deck selector, loads optional per-deck `settings.tsv` daily limits, can
edit those daily limits from the `SELECT` actions screen, reveals answers,
records ratings, schedules cards with a
day-level spaced repetition algorithm, saves local `state.tsv` progress beside
that deck, and can undo the last rating or suspend action with `L`, suspend
cards after opening suspend confirmation with `R` and confirming with `X`, see
suspended-card counts in deck, action, and summary views, restore suspended
cards from the actions screen after confirming with `X`, or reset saved
progress after opening the reset action and confirming with `X`. Optional
media references remain non-MVP. The top screen shows
deck/card content with basic terminal-style color cues, while the bottom screen
shows controls, review status, save feedback, and the last valid sampled
battery level, including charging and low battery states. Successful ratings,
suspend actions, undo actions, and
restore-suspended actions append best-effort `review-log.tsv` rows for
debugging. The no-due summary separates current-session rating counts from the
persisted count of cards reviewed today. An in-app controls screen is available
with `Y` from non-rating screens and unrevealed review cards.

See:

- [PROJECT_PLAN.md](PROJECT_PLAN.md)
- [CHECKPOINTS.md](CHECKPOINTS.md)
- [TESTING.md](TESTING.md)
- [docs/control-map.md](docs/control-map.md)
- [docs/algorithms-and-abstractions.md](docs/algorithms-and-abstractions.md)
- [docs/deck-format.md](docs/deck-format.md)
- [docs/c-style-and-architecture.md](docs/c-style-and-architecture.md)
- [docs/emulator-feedback-loop.md](docs/emulator-feedback-loop.md)
- [docs/emulator-test-log.md](docs/emulator-test-log.md)
- [docs/device-test-log.md](docs/device-test-log.md)

## Non-Goals

This project is not for distributing copyrighted games, BIOS files, ROMs, or
commercial Anki content. Test decks should be original or openly licensed.
