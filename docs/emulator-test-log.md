# Emulator Test Log

Emulator:

- name: Azahar
- version: 2125.1.2
- install path: `~/Applications/azahar-macos-arm64-2125.1.2/Azahar.app`

## Template

```text
## YYYY-MM-DD - Checkpoint Name

Build:
Command:
Gate:
Sample prep:
Decks:
Steps:
Observed:
Expected:
Evidence:
Result: pass/fail
Notes:
```

For M7 daily-use acceptance, include the deck ids tested, the exact fresh-sample
command if one was used, the settings values changed, and whether relaunch
persistence was confirmed.

## 2026-06-01 - M1 Toolchain Proof

Build: `5118fe6`
Command: `make run-emulator`
Steps:
- Built `app-3ds/anki3ds.3dsx`.
- Launched the build in Azahar.
- Checked for the M1 proof screen.
Observed:
- User confirmed the app screen is visible in Azahar.
- User confirmed the app exits when pressing the emulator key mapped to
  `START`.
Expected:
- The top screen displays `anki3ds`, `M1 Toolchain Proof`, version text, and
  the `START` exit prompt.
Result: pass
Notes:
- Azahar maps 3DS `START` to keyboard `M` in the default control profile on
  this machine.

## 2026-06-03 - First-Save Rollback Diagnosis

Build: `41edf65` before fix, local working tree after fix
Command: `make run-emulator`
Steps:
- Launched the review app in Azahar.
- User reported that rating cards did not advance.
- Checked Azahar's SD-card directory and log.
Observed:
- No `state.tsv` existed after attempted ratings.
- Azahar logged repeated missing-file rename failures for
  `limits-demo/state.tsv` to `limits-demo/state.tsv.bak`.
Expected:
- The first rating should create `state.tsv` without needing an existing
  primary state file.
Result: fail before fix
Notes:
- The storage transaction had relied on desktop-style `errno == ENOENT` after
  `rename` failed for a missing primary file.
- The fix checks file existence before remove/rename and adds a first-save
  regression test.
- The fixed build launches in Azahar, but button-level acceptance still needs a
  manual emulator or hardware pass because this session cannot automate Azahar
  keypresses.

## 2026-06-03 - Current Build Launch Check

Build: `3731716`
Command: `make run-emulator`
Steps:
- Built or reused the current `app-3ds/anki3ds.3dsx`.
- Launched the build through Azahar with macOS `open`.
Observed:
- The launch command completed successfully.
Expected:
- Azahar accepts the `.3dsx` and starts the app process.
Result: pass for launch command only
Notes:
- Button-level review, actions, reset, and controls acceptance was not run in
  this checkpoint.

## 2026-06-03 - Selector UI Launch Check

Build: `6f3f917`
Command: `make run-emulator`
Steps:
- Reused the current `app-3ds/anki3ds.3dsx`.
- Launched the build through Azahar with macOS `open`.
Observed:
- The launch command completed successfully.
Expected:
- Azahar accepts the `.3dsx` and starts the app process.
Result: pass for launch command only
Notes:
- D-pad responsiveness and color rendering still need a manual emulator or
  hardware pass.

## 2026-06-03 - Media Layout Launch Check

Build: `c74423d`
Command: `make run-emulator`
Steps:
- Reused the current `app-3ds/anki3ds.3dsx`.
- Launched the build through Azahar with macOS `open`.
Observed:
- The launch command completed successfully.
Expected:
- Azahar accepts the `.3dsx` and starts the app process.
Result: pass for launch command only
Notes:
- Media status-row layout is covered by host tests, but the rendered colors and
  spacing still need a visual emulator or hardware pass.

## 2026-06-03 - D-pad Hold Wake Launch Check

Build: `0fcf72a`
Command: `make run-emulator`
Steps:
- Reused the current `app-3ds/anki3ds.3dsx`.
- Launched the build through Azahar with macOS `open`.
Observed:
- The launch command completed successfully.
Expected:
- Azahar accepts the `.3dsx` and starts the app process.
Result: pass for launch command only
Notes:
- D-pad hold repeat now stays on a one-VBlank wait while held, but selector
  feel still needs a manual emulator or hardware pass.

## 2026-06-03 - D-pad Repeat Timing Launch Check

Build: `abd9344`
Command: `make run-emulator`
Steps:
- Reused the current `app-3ds/anki3ds.3dsx`.
- Launched the build through Azahar with macOS `open`.
Observed:
- The launch command completed successfully.
Expected:
- Azahar accepts the `.3dsx` and starts the app process.
Result: pass for launch command only
Notes:
- D-pad repeat now starts after about 300 ms and repeats about every 80 ms.
- Quick-tap selector behavior still needs a manual emulator or hardware pass.

## 2026-06-03 - Reviewed-Today Summary Launch Check

Build: `5539fd0`
Command: `make run-emulator`
Steps:
- Reused the current `app-3ds/anki3ds.3dsx`.
- Launched the build through Azahar with macOS `open`.
Observed:
- The launch command completed successfully.
Expected:
- Azahar accepts the `.3dsx` and starts the app process.
Result: pass for launch command only
Notes:
- Summary labels and reviewed-today counts are covered by host-side scheduler
  tests, but rendered spacing still needs a visual emulator or hardware pass.

## 2026-06-03 - Suspend Confirmation Launch Check

Build: `2392290`
Command: `make run-emulator`
Steps:
- Reused the current `app-3ds/anki3ds.3dsx`.
- Launched the build through Azahar with macOS `open`.
Observed:
- The launch command completed successfully.
Expected:
- Azahar accepts the `.3dsx` and starts the app process.
Result: pass for launch command only
Notes:
- `R` now opens suspend confirmation and `X` performs suspend. Host tests cover
  the new modal controls rule; full button acceptance is deferred to the final
  manual emulator or hardware pass.

## 2026-06-03 - Restore Confirmation Launch Check

Build: `4f751c9`
Command: `make run-emulator`
Steps:
- Reused the current `app-3ds/anki3ds.3dsx`.
- Launched the build through Azahar with macOS `open`.
Observed:
- The launch command completed successfully.
Expected:
- Azahar accepts the `.3dsx` and starts the app process.
Result: pass for launch command only
Notes:
- Restoring suspended cards now requires `X` confirmation when suspended cards
  exist. Host tests cover the new modal controls rule; full button acceptance is
  deferred to the final manual emulator or hardware pass.

## 2026-06-03 - Restore Modal Day-Change Launch Check

Build: `7798acb`
Command: `make run-emulator`
Steps:
- Reused the current `app-3ds/anki3ds.3dsx`.
- Launched the build through Azahar with macOS `open`.
Observed:
- The launch command completed successfully.
Expected:
- Azahar accepts the `.3dsx` and starts the app process.
Result: pass for launch command only
Notes:
- Restore confirmation is now treated as a review-surface modal during local
  day changes, matching suspend confirmation. Automated tests and build passed;
  full button acceptance is deferred to the final manual pass.

## 2026-06-03 - Confirmation Exit Launch Check

Build: `342cbea`
Command: `make run-emulator`
Steps:
- Reused the current `app-3ds/anki3ds.3dsx`.
- Launched the build through Azahar with macOS `open`.
Observed:
- The launch command completed successfully.
Expected:
- Azahar accepts the `.3dsx` and starts the app process.
Result: pass for launch command only
Notes:
- Confirmed suspend and restore attempts now leave their confirmation screens
  after success or save failure. Automated tests and build passed; full button
  acceptance is deferred to the final manual pass.

## 2026-06-03 - Input And Persistence Hardening Launch Check

Build: local working tree
Command: `make run-emulator`
Steps:
- Ran `make test`, `make`, and `make install-local-sd`.
- Launched the build through Azahar with macOS `open`.
Observed:
- The launch command completed successfully.
Expected:
- Azahar accepts the `.3dsx` and starts the app process.
Result: pass for launch command only
Notes:
- Host tests cover held-button command rejection, clean D-pad hold waits,
  scheduler restore repositioning, mature-review daily-limit reloads, and
  review-log partial-row rejection. Full button/render acceptance is deferred to
  the final manual pass.

## 2026-06-03 - Review Log Visibility Launch Check

Build: local working tree
Command: `make run-emulator`
Steps:
- Ran `make test`, `make`, and `make install-local-sd`.
- Launched the build through Azahar with macOS `open`.
Observed:
- The launch command completed successfully.
Expected:
- Azahar accepts the `.3dsx` and starts the app process.
Result: pass for launch command only
Notes:
- Saved study actions now keep progress even if diagnostic `review-log.tsv`
  append fails, and the bottom status reports `log skipped`. Full button/render
  acceptance is deferred to the final manual pass.

## 2026-06-03 - Daily Workflow Log Coverage Launch Check

Build: local working tree
Command: `make run-emulator`
Steps:
- Ran `make test`, `make`, and `make install-local-sd`.
- Launched the build through Azahar with macOS `open`.
Observed:
- The launch command completed successfully.
Expected:
- Azahar accepts the `.3dsx` and starts the app process.
Result: pass for launch command only
Notes:
- Host tests now cover the M7-shaped two-deck workflow with per-deck
  review-log rows, plus reset cleanup after orphaned `.tmp`/`.bak` save
  artifacts. Status warnings for `log skipped` and `log kept` now use the
  caution color. Full button/render acceptance is deferred to the final manual
  pass.

## 2026-06-03 - UI Warning And Battery Retry Launch Check

Build: local working tree
Command: `make run-emulator`
Steps:
- Ran `make test`, `make`, and `make install-local-sd`.
- Launched the build through Azahar with macOS `open`.
Observed:
- The launch command completed successfully.
Expected:
- Azahar accepts the `.3dsx` and starts the app process.
Result: pass for launch command only
Notes:
- Host tests now cover ten-minute closed-shell battery scheduling, short retry
  scheduling for failed battery samples, and bad-settings deck summaries.
  Load-error paths are truncated, bad settings are surfaced as ignored in deck
  stats, and `reset state` status text uses the caution color. Full
  button/render acceptance is deferred to the final manual pass.

## 2026-06-03 - Contextual Controls Launch Check

Build: local working tree
Command: `make run-emulator`
Steps:
- Ran `make test`, `make`, and `make install-local-sd`.
- Launched the build through Azahar with macOS `open`.
Observed:
- The launch command completed successfully.
Expected:
- Azahar accepts the `.3dsx` and starts the app process.
Result: pass for launch command only
Notes:
- The controls screen now shows prompts for the screen that opened it, including
  separate review-front and review-rating variants. Full button/render
  acceptance is deferred to the final manual pass.
