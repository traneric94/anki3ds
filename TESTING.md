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
make verify-ci
make verify-sample-decks
make verify-local
make package-sd
make install-local-sd
make install-local-sample-deck
make install-local-sample-decks
make reset-local-sample-progress
make prepare-local-samples-fresh
make install-azahar-sample-deck
make install-azahar-sample-decks
make reset-azahar-sample-progress
make prepare-azahar-samples-fresh
make run-emulator
make run-emulator-samples
make run-emulator-fresh-samples
```

Verification gates:

- `make verify-ci` is the portable CI gate. It runs `make test` and
  `make verify-sample-decks`.
- `make verify-sample-decks` checks the tracked sample decks for required
  files, matching `deck.json` metadata, valid `settings.tsv`, five-field
  text-card rows, duplicate card IDs, and accidentally committed progress
  files. The default tracked sample set is text-only.
- `make verify-local` is the local pre-checkpoint gate. It runs tests and
  sample-deck verification, then builds the 3DS app and stages the local SD
  mirror with sample decks. It requires the local 3DS toolchain.
- `make package-sd` builds a clean SD-card payload under `dist/sdmc/` with the
  app artifact and tracked sample decks, but without generated progress files.

The local SD mirror lives at:

```text
local/sdmc/3ds/anki3ds/
```

The tracked sample decks install to:

```text
local/sdmc/3ds/anki3ds/decks/limits-demo/cards.tsv
local/sdmc/3ds/anki3ds/decks/limits-demo/settings.tsv
local/sdmc/3ds/anki3ds/decks/sample/cards.tsv
local/sdmc/3ds/anki3ds/decks/sample/settings.tsv
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/limits-demo/cards.tsv
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/limits-demo/settings.tsv
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/sample/cards.tsv
~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/sample/settings.tsv
```

The app writes review progress beside the selected deck:

```text
sdmc:/3ds/anki3ds/decks/<deck-id>/state.tsv
```

Use `make reset-local-sample-progress` or `make reset-azahar-sample-progress`
to clear only tracked sample-deck progress files before a fresh manual pass.
The `prepare-local-samples-fresh` and `prepare-azahar-samples-fresh` targets
install the tracked sample decks first, then perform that progress reset.
Sample install targets replace source-owned files such as `deck.json`,
`cards.tsv` and `settings.tsv` while preserving `state.tsv` and
`review-log.tsv`; fresh targets clear those files plus `state.tsv.tmp`,
`state.tsv.bak`, `review-log.tsv.tmp`, and `review-log.tsv.bak` so stale
progress should not carry into a pass. Default sample installs also remove the
old optional `media-demo` fixture from the sample root so a text-only acceptance
pass shows exactly the text decks.
The singular `install-local-sample-deck` and `install-azahar-sample-deck`
targets are compatibility aliases for the plural targets; the plural names
describe the current multi-deck sample workflow more accurately.

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
successfully. The app caps it at 262144 bytes and stops appending if an
interrupted write leaves a partial final row. If a saved action cannot be
logged, the status line reports `log skipped`. Resetting deck progress removes
state recovery files before the primary `state.tsv`, then removes the log as
diagnostic cleanup.

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
Gate:
SD path:
Sample prep:
Decks:
Steps:
Observed:
Expected:
Evidence:
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
review key map for reveal and Again/Hard/Good/Easy ratings. That workflow also
checks per-deck review-log rows for rating, undo, suspend, and restore actions.
It also checks that malformed review state blocks normal saves and suppresses
selector due counts until deck progress is reset. Opening a deck with malformed
state should land on a reset-needed summary instead of the normal review queue.
Tracked sample fixture coverage is limited to the default text decks,
`sample` and `limits-demo`.

GitHub Actions runs `make verify-ci` on pushes and pull requests. That covers
the portable C host suite, converter tests, and tracked sample-deck
verification; emulator and hardware checks remain manual checkpoint steps. The
C host compiler can be overridden with `HOST_CC` and `HOST_CFLAGS`; the default
flags include the POSIX feature level needed by the timezone and filesystem
tests.

Run the local pre-checkpoint gate with:

```sh
make verify-local
```

That target runs `make test`, builds the 3DS app, and stages the local SD mirror
with sample decks. It requires the local 3DS toolchain, so CI uses the portable
`make verify-ci` gate instead. `make verify-local` does not clear existing
sample-deck progress; run `make prepare-local-samples-fresh`,
`make prepare-azahar-samples-fresh`, or `make run-emulator-fresh-samples`
before a fresh manual acceptance pass.

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
- keeps the daily-use import path focused on text cards
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

## M7 Daily-Use Acceptance Checklist

Run this checklist after the relevant automated gate passes:

- CI or toolchain-limited machine: `make verify-ci`
- local checkpoint machine: `make verify-local`
- emulator fresh sample pass: `make run-emulator-fresh-samples`
- hardware fresh sample pass: copy the current `.3dsx`, `.smdh`, and tracked
  sample decks to `sdmc:/3ds/anki3ds/`, with tracked sample progress cleared

Use at least two text decks. The tracked `sample` and `limits-demo` decks define
the daily-use path.

Acceptance steps:

- open the deck selector and confirm at least two sample decks appear
- review due cards from two different decks
- reveal and rate with each rating path needed for the pass
- suspend one card, then restore suspended cards from the actions screen
- undo one rating and confirm the queue/status updates sensibly
- change `new_limit` and `review_limit` from the actions screen
- exit, relaunch, and confirm review state and daily limits persisted
- check that no manual file edits were needed during the session

Record evidence in `docs/emulator-test-log.md` or `docs/device-test-log.md`:

- command or copy method used for sample prep
- deck ids covered
- before/after `new_limit` and `review_limit` values
- whether `state.tsv`, `settings.tsv`, and `review-log.tsv` appeared beside
  the tested decks after app actions
- any render, input, save-feedback, or SD-card issues observed

## Release Checklist

Before any tagged checkpoint:

- latest CI run is green, or `make verify-ci` passes locally
- `make verify-local` passes
- M7 or checkpoint-specific emulator evidence is recorded, or explicitly
  deferred with a reason
- hardware evidence is recorded in `docs/device-test-log.md`, or explicitly
  deferred to the user with the date
- docs match the current artifact
- sample decks are tiny and original
- `make verify-sample-decks` passes
- `make package-sd` stages the expected SD payload when preparing files for
  manual copy or release
- no personal Anki data is committed
- no copyrighted media is committed
- build instructions are current
- release payload is `app-3ds/anki3ds.3dsx` plus `app-3ds/anki3ds.smdh`;
  `.cia` packaging remains future work unless documented separately
