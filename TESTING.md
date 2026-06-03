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
make install-local-sample-decks
make install-azahar-sample-deck
make install-azahar-sample-decks
make run-emulator
make run-emulator-samples
```

The local SD mirror lives at:

```text
local/sdmc/3ds/anki3ds/
```

The tracked sample decks install to:

```text
local/sdmc/3ds/anki3ds/decks/limits-demo/cards.tsv
local/sdmc/3ds/anki3ds/decks/limits-demo/settings.tsv
local/sdmc/3ds/anki3ds/decks/media-demo/cards.tsv
local/sdmc/3ds/anki3ds/decks/media-demo/media/colors.a3i
local/sdmc/3ds/anki3ds/decks/media-demo/settings.tsv
local/sdmc/3ds/anki3ds/decks/sample/cards.tsv
local/sdmc/3ds/anki3ds/decks/sample/settings.tsv
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/limits-demo/cards.tsv
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/limits-demo/settings.tsv
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/media-demo/cards.tsv
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/media-demo/media/colors.a3i
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/media-demo/settings.tsv
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/sample/cards.tsv
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/sample/settings.tsv
```

The app writes review progress beside the selected deck:

```text
sdmc:/3ds/anki3ds/decks/<deck-id>/state.tsv
```

During saves, the app may also use `state.tsv.tmp` and `state.tsv.bak`.
On load, a valid temp state file can recover an interrupted first save. If all
available state copies are malformed, normal review-state saves are blocked
until deck progress is reset.

The app also appends diagnostic study transitions beside the selected deck:

```text
sdmc:/3ds/anki3ds/decks/<deck-id>/review-log.tsv
```

This log is not required for loading progress; it records accepted rating,
suspend, undo, and restore-suspended transitions after `state.tsv` saves
successfully. The app caps it at 262144 bytes, and resetting deck progress
removes the log before removing `state.tsv`.

Deck settings live beside the selected deck:

```text
sdmc:/3ds/anki3ds/decks/<deck-id>/settings.tsv
```

During settings saves, the app may also use `settings.tsv.tmp` and
`settings.tsv.bak`.
On load, a valid temp settings file can recover an interrupted first save.

Default Azahar keyboard controls are documented in
`docs/control-map.md`. The app displays Nintendo 3DS button names on screen;
for example, the default Azahar key for 3DS `START` is `M`.

The app uses the top screen for deck/card content and the bottom screen for
current controls.

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

The host C suite includes an M7-shaped daily-use workflow covering two decks,
rating undo, suspension restore, daily limits, relaunch persistence, and the
review key map for reveal and Again/Hard/Good/Easy ratings.

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
- converts optional media fields into bounded `.a3i` files
- writes expected deck folder layout
- preserves existing review state on re-import
- preserves card IDs across text edits when stable source ID fields are supplied
- reports conversion errors without Python tracebacks

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
- sample decks are tiny and original
- no personal Anki data is committed
- no copyrighted media is committed
- build instructions are current
