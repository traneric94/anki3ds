# anki3ds

A small Nintendo 3DS flashcard reviewer for Anki-derived decks.

This repo contains a simple 3DS homebrew reviewer plus a desktop converter that
turns Anki-style tab-separated exports into a 3DS-friendly deck format.

## Scope

The first usable version is not a full Anki clone. It does:

- review text cards and bounded `.a3i` images on a Nintendo 3DS
- read decks from the SD card
- save local review progress
- use a simple Anki-like scheduler
- import decks through a desktop converter

The first version does not:

- sync with AnkiWeb
- parse arbitrary Anki templates on-device
- run JavaScript or full card CSS on-device
- support arbitrary media formats
- modify a user's main Anki collection directly

## Components

```text
anki3ds/
  app-3ds/          3DS homebrew app in C/libctru
  converter/        desktop converter in Python
  docs/             design notes and test logs
  sample-decks/     tiny non-copyrighted sample decks
  tools/            helper scripts
```

## Build

The 3DS app currently builds a text and small-image multi-deck reviewer.

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

To copy the build and tracked sample decks into the gitignored local SD mirror:

```sh
make install-local-sd
```

This also installs the tracked sample decks to:

```text
local/sdmc/3ds/anki3ds/decks/limits-demo/cards.tsv
local/sdmc/3ds/anki3ds/decks/media-demo/cards.tsv
local/sdmc/3ds/anki3ds/decks/sample/cards.tsv
```

To install the sample decks into Azahar's SD card data directory:

```sh
make install-azahar-sample-deck
```

By default this uses:

```text
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/limits-demo/cards.tsv
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/media-demo/cards.tsv
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/sample/cards.tsv
```

Override `LOCAL_SDMC` or `AZAHAR_SDMC` if your local mirror or emulator data
directory lives somewhere else.

To launch the current `.3dsx` in Azahar from a normal macOS session:

```sh
make run-emulator
```

To install the tracked sample decks into Azahar's SD directory and launch the
current build in one command:

```sh
make run-emulator-samples
```

By default this expects Azahar at:

```text
~/Applications/azahar-macos-arm64-2125.1.2/Azahar.app
```

To convert a simple tab-separated export into an anki3ds deck:

```sh
python3 converter/anki3ds_convert.py export.tsv sample-decks/my-deck \
  --deck-id my-deck \
  --deck-name "My Deck"
```

The converter simplifies simple HTML in exported fields before writing
`cards.tsv`: formatting tags are removed, block tags and `<br>` become line
breaks, entities such as `&nbsp;` are decoded, and script/style content is
dropped.

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
  --back-field 3
```

Current 3DS builds support 256 cards per deck folder and store up to 64 deck
folders in the selector. To split a larger export into numbered sibling decks
such as `my-deck-01` and `my-deck-02`:

```sh
python3 converter/anki3ds_convert.py export.tsv sample-decks/my-deck \
  --deck-id my-deck \
  --deck-name "My Deck" \
  --split-large-decks
```

Optional media fields can be copied from existing `.a3i` images or converted
from binary PPM `P6` images into the device-side `.a3i` format:

```sh
python3 converter/anki3ds_convert.py export.tsv sample-decks/my-deck \
  --deck-id my-deck \
  --deck-name "My Deck" \
  --front-media-field 3 \
  --media-root path/to/media
```

## Current Status

Multi-deck text and small-image review works at build level: the app scans
`sdmc:/3ds/anki3ds/decks`, lets you select a deck folder containing `cards.tsv`,
shows new/learning/review due counts in the deck selector, loads optional per-deck
`settings.tsv` daily limits, can edit those daily limits from the `SELECT`
actions screen, reveals answers, records ratings, schedules cards with a
day-level spaced repetition algorithm, saves local `state.tsv` progress beside
that deck, and can undo the last rating or suspend action with `L`, suspend
cards with `R`, see suspended-card counts in deck, action, and summary views,
restore suspended cards from the actions screen, or reset saved progress after
opening the reset action and confirming with `X`. Cards may optionally reference
bounded `.a3i` images under the deck's `media/` folder. The top screen shows
deck/card content, while the bottom screen shows controls, review status, save
feedback, and the last valid sampled battery level, including charging and low
battery states. Successful ratings, suspend actions, undo actions, and
restore-suspended actions append best-effort `review-log.tsv` rows for
debugging. An in-app controls screen is available with `Y` from non-rating
screens and unrevealed review cards.

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
