# anki3ds

A small Nintendo 3DS flashcard reviewer for Anki-derived decks.

This repo starts as a project plan and feedback loop. The intended shape is a
simple 3DS homebrew app plus a desktop converter that turns Anki exports into a
3DS-friendly deck format.

## Scope

The first usable version will not be a full Anki clone. It will:

- review text-only cards on a Nintendo 3DS
- read decks from the SD card
- save local review progress
- use a simple Anki-like scheduler
- import decks through a desktop converter

The first version will not:

- sync with AnkiWeb
- parse arbitrary Anki templates on-device
- run JavaScript or full card CSS on-device
- support arbitrary media formats
- modify a user's main Anki collection directly

## Planned Components

```text
anki3ds/
  app-3ds/          3DS homebrew app, planned for C/libctru
  converter/        desktop converter, planned for Python
  docs/             design notes and test logs
  sample-decks/     tiny non-copyrighted sample decks
  tools/            helper scripts
```

## Build

The 3DS app currently builds a text-only multi-deck reviewer.

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

This also installs the tracked sample text decks to:

```text
local/sdmc/3ds/anki3ds/decks/limits-demo/cards.tsv
local/sdmc/3ds/anki3ds/decks/sample/cards.tsv
```

To install the sample decks into Azahar's SD card data directory:

```sh
make install-azahar-sample-deck
```

By default this uses:

```text
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/limits-demo/cards.tsv
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/sample/cards.tsv
```

Override `LOCAL_SDMC` or `AZAHAR_SDMC` if your local mirror or emulator data
directory lives somewhere else.

To launch the current `.3dsx` in Azahar from a normal macOS session:

```sh
make run-emulator
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

## Current Status

Multi-deck text review works at build level: the app scans
`sdmc:/3ds/anki3ds/decks`, lets you select a deck folder containing `cards.tsv`,
shows due/new counts in the deck selector, loads optional per-deck
`settings.tsv` daily limits, can edit those daily limits from the `SELECT`
actions screen, reveals answers, records ratings, schedules cards with a
day-level spaced repetition algorithm, saves local `state.tsv` progress beside
that deck, and can undo the last rating or suspend action with `L`, suspend
cards with `R`, restore suspended cards from the actions screen, or reset saved
progress after selecting reset from that actions screen. The top screen shows
deck/card content, while the bottom screen shows controls and review status.

See:

- [PROJECT_PLAN.md](PROJECT_PLAN.md)
- [CHECKPOINTS.md](CHECKPOINTS.md)
- [TESTING.md](TESTING.md)
- [docs/algorithms-and-abstractions.md](docs/algorithms-and-abstractions.md)
- [docs/deck-format.md](docs/deck-format.md)
- [docs/c-style-and-architecture.md](docs/c-style-and-architecture.md)
- [docs/emulator-feedback-loop.md](docs/emulator-feedback-loop.md)
- [docs/emulator-test-log.md](docs/emulator-test-log.md)
- [docs/device-test-log.md](docs/device-test-log.md)

## Non-Goals

This project is not for distributing copyrighted games, BIOS files, ROMs, or
commercial Anki content. Test decks should be original or openly licensed.
