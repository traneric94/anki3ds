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

## Current Code-Complete Loop

For the current daily-use push, keep the inner loop local and manual-first:

1. Make the app/converter change.
2. Run a narrow local sanity check such as `make`, `python3 -m py_compile`, or
   a deck verifier when it directly covers the changed surface.
3. Commit locally.
4. Defer CI polling and broad automated tests until the manual pass exposes a
   concrete issue worth shrinking with a targeted test.

This does not replace the stronger pre-checkpoint gates below; it just keeps
the code-complete pass moving while manual emulator or hardware feedback is
still pending.

Current build commands:

```sh
make
make test
make test-host
make test-converter
make test-tools
make verify-ci
make verify-sample-decks
make verify-local
make verify-local-sd
make verify-azahar-fresh-samples
make package-sd
make verify-package-sd
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

- `make verify-ci` is the portable CI gate. It runs `make test`, including
  host C tests, converter tests, and verifier-tool tests, then runs
  `make verify-sample-decks`.
- `make verify-sample-decks` checks the tracked sample decks for required
  files, matching `deck.json` metadata, valid `settings.tsv`, five-field
  text-card rows, duplicate card IDs, and accidentally committed progress
  files. The default tracked sample set is text-only.
- `make verify-package-sd` builds the copy-ready `dist/sdmc/` payload and
  verifies that it contains the app artifacts, the default text decks, valid
  five-field text-card rows, valid settings, no generated progress files, and
  no media or extra files inside those text deck folders.
- `make verify-local-sd` builds a fresh tracked-sample local SD mirror and
  verifies that it contains the app artifacts, default text decks, and no
  progress or stray files inside those text deck folders.
- `make verify-azahar-fresh-samples` installs fresh tracked sample decks into
  Azahar's SD data directory and verifies that the emulator sample folders are
  text-only and progress-free.
- `make verify-local` is the local pre-checkpoint gate. It runs tests,
  sample-deck verification, local SD staging and verification, and
  package-payload verification. It requires the local 3DS toolchain.
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
`cards.tsv` and `settings.tsv`, clean tracked sample folders of stale
non-progress files, and preserve `state.tsv`/`review-log.tsv` progress and
recovery artifacts; fresh targets clear those files so stale progress should
not carry into a pass. Default sample installs also remove old `media-demo`
folders from the sample root so a text-only acceptance pass shows exactly the
text decks.
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
successfully. The app caps it at 262144 bytes. If an interrupted write leaves a
partial final row, the next append rewrites the complete prefix before adding a
new row. If that repair or append fails, the status line reports `log skipped`.
Resetting deck progress removes state recovery files before the primary
`state.tsv`, then removes the log as diagnostic cleanup.

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
rating undo, suspension restore, daily limits, reset progress while preserving
deck settings, relaunch persistence, and the review key map for reveal and
Again/Hard/Good/Easy ratings. It also checks
app-level input priority for controls, exit confirmation, deck-list return,
actions, undo gating, suspend confirmation, review text scrolling, reveal, and
rating dispatch. The input coverage also checks which screens accept held-D-pad
repeat and which D-pad axes repeat on those screens. The power coverage checks
battery polling cadence and the adaptive idle input wait tiers used to avoid
busy polling while the screen is unchanged. That
workflow checks per-deck review-log rows for rating, undo, suspend, and restore
actions. Status-feedback coverage checks the warning/success/error classes
used by daily-use messages such as unsaved limits, missing decks, restored
cards, and no-due summaries.
`make test-host` also compiles the app-controls module with a small host stub of
libctru's `<3ds.h>` so the `KEY_A`/`KEY_B`/`KEY_X`/`KEY_Y`, D-pad,
`SELECT`, and `START` translation branch is covered outside hardware builds.
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
Run verifier-tool tests with:

```sh
make test-tools
```

The text-deck verifier is part of the package safety net because it rejects
source or staged sample decks that the 3DS app would fail to load, plus stray
media folders or extra files in the default text-only deck folders.

Minimum converter tests:

- parses plain-text Anki export
- preserves stable card IDs
- handles tabs and line breaks
- strips or simplifies simple HTML
- keeps the daily-use import path focused on text cards
- writes expected deck folder layout
- preserves existing review state on re-import
- removes stale converter-generated split chunks on re-import
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

During the current code-complete pass, run this checklist as the manual feedback
step after fresh sample prep. Before a tagged checkpoint or release artifact,
also run the relevant automated gate:

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
- scroll the long text card in the tracked `sample` deck with D-pad Up/Down
- suspend one card, then restore suspended cards from the actions screen
- confirm restore with no suspended cards stays on the actions screen
- undo one rating and confirm the queue/status updates sensibly
- change `new_limit` and `review_limit` from the actions screen
- confirm daily-limit edits show an unsaved-changes cue before saving and
  return to no-changes state after save or cancel
- confirm opening daily limits from reset-needed, ignored-settings, or
  limit-blocked decks preserves that warning context in the status line
- confirm controls opened from unsaved daily-limit edits show an unsaved warning
- confirm controls opened from or closed back to reset-needed, ignored-settings,
  limit-blocked, or load-error screens preserve that warning context in the
  status line
- confirm exit from daily limits and from its controls screen warns before
  losing unsaved edits
- confirm opening exit, restore, suspend, and reset confirmations preserves
  selected-deck, load-error, controls, or active-deck warning context
- confirm canceling exit from controls, actions, confirmations, and warning
  surfaces preserves the relevant status-line warning context
- confirm opening actions and moving through action items from reset-needed,
  ignored-settings, or limit-blocked decks preserves that warning context in
  the status line
- confirm canceling actions or restore/suspend/reset confirmations preserves
  active deck warning context in the status line
- confirm action, daily-limit, restore, suspend, and reset screens show the
  expected active deck before taking deck-specific actions
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
- `make verify-package-sd` stages and verifies the expected SD payload when
  preparing files for manual copy or release
- no personal Anki data is committed
- no copyrighted media is committed
- build instructions are current
- release payload is `app-3ds/anki3ds.3dsx` plus `app-3ds/anki3ds.smdh`;
  `.cia` packaging remains future work unless documented separately
