# Checkpoints

Each checkpoint should produce a build or artifact the user can test directly.
Record hardware results in `docs/device-test-log.md`.

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
4. Press `START`, confirm the exit screen appears, then press `A`.
5. Confirm it exits cleanly.

Hardware acceptance test:

1. Copy the `.3dsx` build to the SD card.
2. Launch it from the Homebrew Launcher.
3. Confirm it displays a title and version string.
4. Press `START`, confirm the exit screen appears, then press `A`.
5. Confirm it exits cleanly.

Pass condition:

- app launches in the emulator and on hardware without hanging

## M2: Input And Rendering Proof

Acceptance test:

1. Launch the app.
2. Press `A`.
3. Press D-pad directions.
4. Press `START` to open exit confirmation.
5. Press `A` to exit, or `B`/`SELECT` to cancel.

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
5. Press `SELECT` to reset progress.

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

Acceptance test:

1. Copy two or more decks to the SD card with `make install-local-sd` or the
   equivalent real SD copy. For tracked sample decks, use
   `make prepare-local-samples-fresh` so stale sample progress does not carry
   into the pass.
2. Review due cards from each deck.
3. Suspend one card.
4. Undo one rating.
5. Restore suspended cards from the actions screen.
6. Set `new_limit` and `review_limit` from the actions screen.
7. Relaunch and confirm state and daily limits persisted.

Pass condition:

- a real study session works without manual file edits

## M8: Media And Polish

Acceptance test:

1. Convert a deck with small images.
2. Install `sample-decks/media-demo` or the converted deck to SD.
3. Review cards on hardware.
4. Confirm images fit the screen and text remains readable.

Pass condition:

- image cards are useful on the original 3DS screen
