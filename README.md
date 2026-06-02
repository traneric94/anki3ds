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

The 3DS app currently builds a minimal toolchain proof.

```sh
make
```

This produces:

```text
app-3ds/anki3ds.3dsx
app-3ds/anki3ds.smdh
```

To copy the build into the gitignored local SD mirror:

```sh
make install-local-sd
```

This also installs the tracked sample text deck to:

```text
local/sdmc/3ds/anki3ds/decks/sample/cards.tsv
```

To install only the sample deck into Azahar's SD card data directory:

```sh
make install-azahar-sample-deck
```

By default this uses:

```text
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

## Current Status

M1 toolchain proof builds locally. Emulator and hardware verification are next.

See:

- [PROJECT_PLAN.md](PROJECT_PLAN.md)
- [CHECKPOINTS.md](CHECKPOINTS.md)
- [TESTING.md](TESTING.md)
- [docs/deck-format.md](docs/deck-format.md)
- [docs/c-style-and-architecture.md](docs/c-style-and-architecture.md)
- [docs/emulator-feedback-loop.md](docs/emulator-feedback-loop.md)
- [docs/emulator-test-log.md](docs/emulator-test-log.md)
- [docs/device-test-log.md](docs/device-test-log.md)

## Non-Goals

This project is not for distributing copyrighted games, BIOS files, ROMs, or
commercial Anki content. Test decks should be original or openly licensed.
