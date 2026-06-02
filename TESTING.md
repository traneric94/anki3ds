# Testing

## Hardware Feedback Loop

The project depends on real 3DS testing. Emulator testing can help, but hardware
is the source of truth.

For each checkpoint:

1. Build the artifact.
2. Copy it to the SD card.
3. Test only the documented acceptance path.
4. Record the result in `docs/device-test-log.md`.
5. Fix the smallest confirmed issue.
6. Repeat until the checkpoint passes.

## Emulator Feedback Loop

Use the emulator for quick iteration between checkpoint builds:

1. Build the `.3dsx`.
2. Launch it in the emulator.
3. Check the narrow behavior under development.
4. Fix obvious rendering, crash, or input problems.
5. Move to hardware only when the local behavior is stable.

The emulator loop is allowed to be faster and rougher than the hardware loop.
Hardware test results should still be recorded in `docs/device-test-log.md`.

Current build commands:

```sh
make
make test
make test-host
make test-converter
make install-local-sd
make install-local-sample-deck
make install-azahar-sample-deck
make run-emulator
```

The local SD mirror lives at:

```text
local/sdmc/3ds/anki3ds/
```

The tracked sample deck installs to:

```text
local/sdmc/3ds/anki3ds/decks/sample/cards.tsv
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/sample/cards.tsv
```

The app writes review progress to:

```text
sdmc:/3ds/anki3ds/decks/sample/state.tsv
```

Default Azahar keyboard controls used by the sample reviewer:

```text
A key = reveal / Easy
S key = Good
Z key = Hard
X key = Again
N key = reset progress
M key = exit
```

Record emulator runs in `docs/emulator-test-log.md`.

## Initial Target Device

Known user device:

- model: Nintendo 3DS, `CTR-001`
- custom firmware: Luma3DS present
- GodMode9 available
- SD card recently repaired or replaced

## Manual Test Log Format

Use this format in `docs/device-test-log.md`:

```text
## YYYY-MM-DD - Checkpoint Name

Build:
SD path:
Steps:
Observed:
Expected:
Result: pass/fail
Notes:
```

## Desktop Tests

Run current host-side C tests with:

```sh
make test-host
```

Run converter tests with:

```sh
make test-converter
```

The converter should have automated tests because it handles user data.

Minimum converter tests:

- parses plain-text Anki export
- preserves stable card IDs
- handles tabs and line breaks
- strips or simplifies simple HTML
- writes expected deck folder layout
- preserves existing review state on re-import

## Save-State Tests

Save files should be tested with:

- normal write and reload
- interrupted write simulation
- missing state file
- corrupted state file
- deck update with existing state

## Release Checklist

Before any tagged checkpoint:

- docs match the current artifact
- sample deck is tiny and original
- no personal Anki data is committed
- no copyrighted media is committed
- build instructions are current
