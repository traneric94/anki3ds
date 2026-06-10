# Checkpoints

Each checkpoint should produce a build or artifact the user can test directly.
Record hardware results in `docs/device-test-log.md`.
For current-session context before continuing active work, read
`docs/sleep-context-2026-06-08.md` first, then `docs/session-handoff.md` for
the full historical record.

## Checkpoint Rules

- Every checkpoint has one clear acceptance test.
- The user tests only checkpoint builds, not random intermediate states.
- Any failing checkpoint gets a short bug note before fixes begin.
- Working checkpoints should be tagged in git.
- Run `make verify-local` before a tagged checkpoint on a machine with the 3DS
  toolchain.

Suggested tag format:

```text
v0.1-toolchain-proof
v0.2-input-rendering
v0.3-sd-deck-read
```

## Local Emulator Loop

Most changes should be checked in a local emulator before moving files to real
hardware. Hardware remains the checkpoint gate, but emulator passes should catch
basic build, rendering, and input mistakes first.

Preferred emulator:

- Azahar, because it supports `.3dsx` homebrew files

See `docs/emulator-feedback-loop.md`.

## M0: Repo And Planning

Acceptance test:

- public GitHub repo exists
- planning docs are pushed

## M1: Toolchain Proof

Build:

```sh
make
make install-local-sd
make run-emulator
```

Local emulator acceptance test:

1. Build the `.3dsx`.
2. Launch it in Azahar with `make run-emulator`.
3. Confirm it displays a title and version string.
4. Press `START` and confirm help toggles.
5. Press `Y`, confirm the exit screen appears, then press `A`.
6. Confirm it exits cleanly.

Hardware acceptance test:

1. Copy the `.3dsx` build to the SD card.
2. Launch it from the Homebrew Launcher.
3. Confirm it displays a title and version string.
4. Press `START` and confirm help toggles.
5. Press `Y`, confirm the exit screen appears, then press `A`.
6. Confirm it exits cleanly.

Pass condition:

- app launches in the emulator and on hardware without hanging

## M2: Input And Rendering Proof

Acceptance test:

1. Launch the app.
2. Press `A`.
3. Press D-pad directions.
4. Press `START` to toggle help.
5. Press `Y` to open exit confirmation.
6. Press `A` to exit, or `B`/`SELECT` to cancel.

Pass condition:

- visible text updates in response to button presses

## M3: SD Deck Read

Acceptance test:

1. Install the sample deck with `make install-local-sample-deck` for the local
   SD mirror, or `make install-azahar-sample-deck` for Azahar.
2. Launch the app.
3. Open the sample deck.
4. Reveal at least one answer.

Pass condition:

- card text comes from `sdmc:/3ds/anki3ds/decks/sample/cards.tsv`, not from
  hardcoded app data

## M4: Review Loop

Acceptance test:

1. Review ten sample cards.
2. Use all four ratings at least once.
3. Confirm the app advances after each rating.

Pass condition:

- ten-card review session completes without a crash or stuck screen

## M5: Save State

Host test:

```sh
make test-host
```

Acceptance test:

1. Review several cards.
2. Exit the app.
3. Relaunch the app.
4. Confirm reviewed cards are no longer immediately due unless rated Again.
5. Press `Y` before reveal, then `X`, to reset progress.

Pass condition:

- local progress survives restart
- reset removes saved progress and starts the sample deck again

## M6: Converter MVP

Acceptance test:

1. Export a small Anki deck as plain text.
2. Run the converter.
3. Copy the output to the SD card.
4. Review converted cards on the 3DS.

Pass condition:

- real Anki-exported cards are usable on-device

## M7: Daily-Use MVP

Before acceptance:

- During the current code-complete pass, use local commits plus narrow sanity
  checks and manual emulator or hardware feedback; do not require CI polling
  before every change.
- Run `make verify-local` on a machine with the 3DS toolchain, or confirm the
  portable `make verify-ci` gate is green when only host verification is
  available before a tagged checkpoint or release artifact.
- For the full emulator preflight, run `make verify-m7-preflight` so local
  verification, copy-ready packaging, Azahar fresh-sample staging, and the
  local Azahar keyboard profile are all current before the manual pass.
- For emulator acceptance, launch with `make run-emulator-fresh-samples`.
- For hardware acceptance, copy the current `.3dsx`, `.smdh`, and tracked
  sample decks to `sdmc:/3ds/anki3ds/` with tracked sample progress cleared.

Acceptance test:

1. Copy two or more text decks to the SD card with `make install-local-sd` or
   the equivalent real SD copy. For tracked sample decks, use
   `make prepare-local-samples-fresh` so stale sample progress and root
   `session.tsv` diagnostics do not carry into the pass.
2. Review due cards from each deck.
3. Suspend one card.
4. Undo one rating.
5. Restore suspended cards from the completion screen with `R`, then `X`.
6. Set `new_limit` and `review_limit` from the direct daily-limit editor
   opened with `X` before reveal. If testing the scheduler-policy toggle, also
   change learning mode.
7. Relaunch and confirm state and daily limits persisted.
8. Reset progress on one tested deck with `Y` before reveal, then `X` to
   confirm, and confirm settings are preserved. If reset is the final state for
   that deck, include it in `M7_RESET_DECKS`.
9. After the session, run `make verify-m7-artifacts M7_SDMC=/path/to/sdmc`
   against the tested SD root. For Azahar, the default `M7_SDMC` is the
   configured `AZAHAR_SDMC`. To prove exact settings edits, pass expectations
   as `deck:new_limit:review_limit[:learning_mode]`, for example
   `M7_EXPECT_SETTINGS="sample:5:20:1 limits-demo:1:10"`; each
   expected-settings deck must be part of `M7_DECKS` or `M7_RESET_DECKS`. The
   optional `learning_mode` field is `0` for Due first and `1` for Cooldown.
   The post-run verifier fails if no expected settings are supplied.

Pass condition:

- a real study session works without manual file edits
- the post-run artifact verifier passes for the tested decks, or the log notes
  why a missing `review-log.tsv` is expected from an in-app `log skipped`
  status and verifies with `M7_ALLOW_MISSING_REVIEW_LOG=1` and
  `M7_NO_REQUIRED_EVENTS=1`
- saved-action session counters do not exceed matching review-log event counts
  unless the pass explicitly uses the `log skipped` allowance above or verifies
  a deck whose progress was reset at the end
- any deck reset at the end verifies with `M7_RESET_DECKS` and still has valid
  `settings.tsv`
- the root `session.tsv` proves the app scanned decks, opened the tested decks,
  has monotonic session time/day fields,
  has internally consistent deck-open outcome counters,
  preserved action evidence across relaunch with `launch_count` of at least 2,
  entered a review screen for rating-required passes, showed answers before
  ratings, saved daily-limit edits and the required daily-use actions, ended
  with `last_deck_id` inside the tested deck set, and exited through the
  confirmation flow with `last_event=exit_confirmed`
- evidence is recorded in `docs/emulator-test-log.md` or
  `docs/device-test-log.md`, including deck ids, sample-prep command or copy
  method, settings changed, and relaunch persistence result

## M8: Polish And Packaging

Acceptance test:

1. Install a text deck with long and short cards.
2. Review cards on hardware.
3. Confirm text wrapping, scrolling, colors, deck stats, and controls remain
   readable on the original 3DS screen.
4. Build any release package target added for this milestone.

Pass condition:

- text-card review feels polished enough for repeated daily use
