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

## 2026-06-04 - Pre-Manual Daily-Use Gate

Build: `ccfb97f`
Command: `make verify-local`
Gate: pass
Sample prep: `make verify-local` ran `prepare-local-samples-fresh` for the
local SD mirror and `package-sd` for the copy-ready payload.
Decks: `limits-demo`, `sample`
Steps:
- Ran host C tests, converter tests, text-deck verifier tests, tracked
  sample-deck verification, local fresh-sample staging, and package payload
  verification.
Observed:
- Host tests passed, including the M7-shaped two-deck workflow.
- Converter and text-deck verifier tests passed.
- Tracked sample decks verified in source, local SD mirror, and `dist/sdmc/`.
- Input coverage now checks per-screen repeat axes: deck select repeats all
  D-pad directions, review/actions repeat Up/Down only, and daily limits repeat
  Left/Right value changes while keeping Up/Down field changes single-step.
Expected:
- Current code-complete artifact remains ready for the final manual M7
  emulator or hardware pass.
Evidence:
- `make verify-local` completed with exit code 0.
Result: pass for automated gate only
Notes:
- Button-level review, rendered colors, save-feedback visibility, SD-card
  behavior, and relaunch persistence still need the final manual emulator or
  hardware acceptance pass.

## 2026-06-04 - Palette And Storage Pre-Manual Gate

Build: `1bacb02`
Command: `make verify-local`
Gate: pass
Sample prep: `make verify-local` ran `prepare-local-samples-fresh` for the
local SD mirror and `package-sd` for the copy-ready payload.
Decks: `limits-demo`, `sample`
Steps:
- Ran host C tests, converter tests, text-deck verifier tests, tracked
  sample-deck verification, local fresh-sample staging, and package payload
  verification.
Observed:
- Host C tests passed, including the daily-use workflow, 3DS key translation,
  per-screen navigation repeat coverage, battery polling cadence, idle input
  wait tiers, and stale review-log repair cleanup.
- Converter and text-deck verifier tests passed.
- Tracked sample decks verified in source, local SD mirror, and `dist/sdmc/`.
- The current artifact includes the dark-console palette change that avoids
  blue/cyan accents in headings and Easy ratings.
Expected:
- Current code-complete artifact remains ready for the final manual M7
  emulator or hardware pass.
Evidence:
- `make verify-local` completed with exit code 0.
Result: pass for automated gate only
Notes:
- Button-level review, rendered palette contrast, save-feedback visibility,
  SD-card behavior, and relaunch persistence still need the final manual
  emulator or hardware acceptance pass.

## 2026-06-04 - Deck Helper Null-Safety Gate

Build: local working tree
Command: `git diff --check`, `make test-host`, `make -C app-3ds`
Gate: pass for host/build only
Sample prep: not run
Decks: not run
Steps:
- Added null-argument guards to exported deck index and deck summary helper
  functions used by the multi-deck selector flow.
- Added host coverage for null deck-index lookup and null deck-summary inputs.
Observed:
- Whitespace check passed.
- Host C tests passed.
- The 3DS target rebuilt successfully.
Expected:
- Future selector or summary callers should fail closed on missing helper
  inputs instead of crashing during deck discovery or summary refresh.
Result: pass for automated host/build gate only
Notes:
- Manual emulator or hardware review remains pending for end-to-end acceptance.

## 2026-06-04 - Restore Log Snapshot Gate

Build: local working tree
Command: `git diff --check`, `make test-host`, `make -C app-3ds`
Gate: pass for host/build only
Sample prep: not run
Decks: not run
Steps:
- Changed restore-all diagnostic logging to use the rollback session captured
  before `scheduler_unsuspend_all`.
- Avoided reconstructing the before-state from the already-restored card.
Observed:
- Whitespace check passed.
- Host C tests passed.
- The 3DS target rebuilt successfully.
Expected:
- Restore-all log rows should record each card's actual pre-restore scheduler
  state, including suspended and review-day fields.
Result: pass for automated host/build gate only
Notes:
- Manual emulator or hardware review remains pending for end-to-end acceptance.

## 2026-06-04 - Review Log Day-Fields Gate

Build: local working tree
Command: `git diff --check`, `make test-host`, `make -C app-3ds`
Gate: pass for host/build only
Sample prep: not run
Decks: not run
Steps:
- Extended diagnostic `review-log.tsv` transition rows with before/after
  `first_review_day` and `last_review_day` fields.
- Kept review logging best-effort and unread by the 3DS app; this only improves
  hardware-session diagnosis of daily-limit and migration behavior.
Observed:
- Whitespace check passed.
- Host C tests passed, including exact review-log row formatting, partial-row
  repair, pending-repair recovery, and capacity-boundary coverage.
- The 3DS target rebuilt successfully.
Expected:
- Manual M7 logs should now contain the review-day fields needed to explain why
  a card counted as new or review work after a saved study transition.
Result: pass for automated host/build gate only
Notes:
- Manual emulator or hardware review remains pending for end-to-end acceptance.

## 2026-06-04 - Migrated Learning Daily-Count Gate

Build: local working tree
Command: `git diff --check`, `make test-host`, `make -C app-3ds`
Gate: pass for host/build only
Sample prep: not run
Decks: not run
Steps:
- Classified migrated zero-day initial-learning rows as newly introduced after
  their next rating, even when older state already had `review_count > 0`.
- Preserved migrated normal review/relearning behavior where unknown first-day
  state remains unknown and the next rating counts as review work.
Observed:
- Whitespace check passed.
- Host C tests passed, including migrated initial-learning daily-count
  regression coverage and migrated-review accounting coverage.
- The 3DS target rebuilt successfully.
Expected:
- Older saved cards still inside the new-card learning loop should count
  against the new-card daily limit after rating and after reload, while older
  mature review cards keep counting against the review limit.
Result: pass for automated host/build gate only
Notes:
- Manual emulator or hardware review remains pending for end-to-end acceptance.

## 2026-06-04 - Latest Local Pre-Manual Gate

Build: `2fbc8ae`
Command: `make verify-local`
Gate: pass
Sample prep: `make verify-local` ran `prepare-local-samples-fresh` for the
local SD mirror and `package-sd` for the copy-ready payload.
Decks: `limits-demo`, `sample`
Steps:
- Ran host C tests, converter tests, text-deck verifier tests, tracked
  sample-deck verification, local fresh-sample staging, and package payload
  verification.
Observed:
- Host C tests passed, including the daily-use workflow, key translation,
  centralized command classification, migrated review-limit accounting, battery
  polling cadence, and idle input wait tiers.
- Converter and text-deck verifier tests passed.
- Tracked sample decks verified in source, local SD mirror, and `dist/sdmc/`.
- The local SD mirror contains only fresh text sample deck files and app
  artifacts for the default sample decks.
Expected:
- Current artifact remains ready for the final manual M7 emulator or hardware
  pass.
Evidence:
- `make verify-local` completed with exit code 0.
Result: pass for automated gate only
Notes:
- Button-level review, rendered palette contrast, save-feedback visibility,
  SD-card behavior, relaunch persistence, and long-idle input wake feel still
  need the final manual emulator or hardware acceptance pass.

## 2026-06-04 - Migrated Review Limit Gate

Build: local working tree
Command: `make test-host`, `make -C app-3ds`
Gate: pass for host/build only
Sample prep: not run
Decks: not run
Steps:
- Fixed migrated reviewed cards from older state formats so their next rating
  records a known `last_review_day` while keeping `first_review_day` unknown.
- Kept truly new cards recording both review-day fields on their first rating.
Observed:
- Host C tests passed, including migrated review save/reload behavior and daily
  review-limit accounting.
- The 3DS target rebuilt successfully.
Expected:
- A reviewed card loaded from older state rows should count against the review
  daily limit after rating, not the new-card daily limit.
Evidence:
- `make test-host` completed with exit code 0.
- `make -C app-3ds` completed with exit code 0.
Result: pass for automated gate only
Notes:
- Manual emulator or hardware acceptance is still needed for the full M7 path.

## 2026-06-04 - Longer Idle Backoff Gate

Build: local working tree
Command: `make test-host`, `make -C app-3ds`
Gate: pass for host/build only
Sample prep: not run
Decks: not run
Steps:
- Increased the unchanged-screen idle timeout ceiling from 0.5 seconds to
  2 seconds after the app has already been idle for several seconds.
Observed:
- Host C tests passed, including idle wait tier coverage.
- The 3DS target rebuilt successfully.
Expected:
- A stable idle screen should wake less often while HID input still wakes the
  app immediately and scheduled battery/day checks are delayed by at most the
  idle timeout.
Evidence:
- `make test-host` completed with exit code 0.
- `make -C app-3ds` completed with exit code 0.
Result: pass for automated gate only
Notes:
- Manual emulator or hardware testing still needs to confirm input wake feel
  after long idle, plus the broader M7 acceptance pass.

## 2026-06-04 - Local Command Classifier Gate

Build: local working tree
Command: `make test-host`, `make -C app-3ds`
Gate: pass for host/build only
Sample prep: not run
Decks: not run
Steps:
- Centralized actions, daily-limit settings, restore, suspend, reset, and exit
  confirm-or-cancel commands in the app-controls classifier.
- Removed duplicated save/cancel/confirm command handling from the app-local
  action, settings, and confirmation handlers.
Observed:
- Host C tests passed, including centralized local command classification and
  mixed-button chord rejection.
- The 3DS target rebuilt successfully.
Expected:
- Save, choose, restore, suspend, reset, and cancel commands should only fire
  on clean single-button presses, while screen-specific movement remains local.
Evidence:
- `make test-host` completed with exit code 0.
- `make -C app-3ds` completed with exit code 0.
Result: pass for automated gate only
Notes:
- Button-level review, rendered palette contrast, save-feedback visibility,
  SD-card behavior, and relaunch persistence still need the final manual
  emulator or hardware acceptance pass.

## 2026-06-04 - Complementary Dark Palette Focus Gate

Build: local working tree
Command: `make -C app-3ds`
Steps:
- Switched normal selected/focused rows from green reverse video to bright
  violet reverse video.
- Kept warm amber for app chrome and preserved green/yellow/red for review
  meaning: Good/review/safe, Hard/learning/caution, and Again/error/danger.
Observed:
- The 3DS target rebuilt successfully with the selected/focused-row palette
  change.
Expected:
- Selected deck rows, menu rows, and setting rows should read as navigation
  focus instead of success state, while the dark background keeps a
  complementary amber/violet palette without blue or cyan.
Result: pass for build only; pending manual render check
Notes:
- Manual emulator or hardware rendering is still needed for true 3DS screen
  contrast.

## 2026-06-04 - Warm Dark Palette Refresh

Build: local working tree
Command: `make -C app-3ds`
Steps:
- Replaced the blue-purple heading/status accent with warm amber.
- Moved the normal selected-row highlight to green reverse video so selected
  rows remain distinct from caution text.
Observed:
- The 3DS target rebuilt successfully.
- Not manually rendered yet.
Expected:
- Headings, status labels, selected rows, and rating labels should stay readable
  on a dark 3DS console without relying on blue or cyan.
Result: pass for build only; pending manual render check
Notes:
- The app still needs the final emulator or hardware pass for rendered palette
  contrast.

## 2026-06-04 - Limit-Blocked Summary Local Gate

Build: `feb223b`
Command: `make verify-local`
Gate: pass
Sample prep: `make verify-local` ran `prepare-local-samples-fresh` for the
local SD mirror and `package-sd` for the copy-ready payload.
Decks: `limits-demo`, `sample`
Steps:
- Ran host C tests, converter tests, text-deck verifier tests, tracked
  sample-deck verification, local fresh-sample staging, and package payload
  verification.
Observed:
- Host C tests passed, including the M7-shaped daily-use workflow, 3DS key
  translation, action and confirmation key priority, day-change status
  formatting, battery polling cadence, per-screen navigation repeat coverage,
  idle input wait tiers, and deck-summary limit-blocked count coverage.
- Converter and text-deck verifier tests passed.
- Tracked sample decks verified in source, local SD mirror, and `dist/sdmc/`.
- The current artifact includes selector-visible daily-limit warnings and
  selected-deck hidden new/review counts when limits hide otherwise due cards.
Expected:
- Current code-complete artifact remains ready for the final manual M7
  emulator or hardware pass.
Evidence:
- `make verify-local` completed with exit code 0.
Result: pass for automated gate only
Notes:
- Button-level review, rendered palette contrast, save-feedback visibility,
  SD-card behavior, and relaunch persistence still need the final manual
  emulator or hardware acceptance pass.

## 2026-06-04 - Current Daily-Use Local Gate

Build: `97f7a9d`
Command: `make verify-local`
Gate: pass
Sample prep: `make verify-local` ran `prepare-local-samples-fresh` for the
local SD mirror and `package-sd` for the copy-ready payload.
Decks: `limits-demo`, `sample`
Steps:
- Ran host C tests, converter tests, text-deck verifier tests, tracked
  sample-deck verification, local fresh-sample staging, and package payload
  verification.
Observed:
- Host C tests passed, including the M7-shaped daily-use workflow, 3DS key
  translation, action and confirmation key priority, day-change status
  formatting, battery polling cadence, per-screen navigation repeat coverage,
  and idle input wait tiers.
- Converter and text-deck verifier tests passed.
- Tracked sample decks verified in source, local SD mirror, and `dist/sdmc/`.
- The current artifact includes the dark-console palette, ignored-deck scan
  warnings, modal new-day status feedback, and centralized active-deck summary
  refresh after saved study-state changes.
Expected:
- Current code-complete artifact remains ready for the final manual M7
  emulator or hardware pass.
Evidence:
- `make verify-local` completed with exit code 0.
Result: pass for automated gate only
Notes:
- Button-level review, rendered palette contrast, save-feedback visibility,
  SD-card behavior, and relaunch persistence still need the final manual
  emulator or hardware acceptance pass.

## 2026-06-04 - Daily-Limit Cleanup Local Gate

Build: `445a0a5`
Command: `make verify-local`
Gate: pass
Sample prep: `make verify-local` ran `prepare-local-samples-fresh` for the
local SD mirror and `package-sd` for the copy-ready payload.
Decks: `limits-demo`, `sample`
Steps:
- Ran host C tests, converter tests, text-deck verifier tests, tracked
  sample-deck verification, local fresh-sample staging, and package payload
  verification.
Observed:
- Host C tests passed, including the daily-use workflow, 3DS key translation,
  expanded escape-key coverage, battery retry after missing-clock coverage,
  per-screen navigation repeat coverage, and idle input wait tiers.
- Converter and text-deck verifier tests passed.
- Tracked sample decks verified in source, local SD mirror, and `dist/sdmc/`.
- The current artifact includes daily-limit cancel cleanup so discarded edits
  reset the scratch settings buffer before returning to actions.
Expected:
- Current code-complete artifact remains ready for the final manual M7
  emulator or hardware pass.
Evidence:
- `make verify-local` completed with exit code 0.
Result: pass for automated gate only
Notes:
- Button-level review, rendered palette contrast, save-feedback visibility,
  SD-card behavior, and relaunch persistence still need the final manual
  emulator or hardware acceptance pass.

## 2026-06-04 - Summary Refresh Local Gate

Build: `b4e45a2`
Command: `make verify-local`
Gate: pass
Sample prep: `make verify-local` ran `prepare-local-samples-fresh` for the
local SD mirror and `package-sd` for the copy-ready payload.
Decks: `limits-demo`, `sample`
Steps:
- Ran host C tests, converter tests, text-deck verifier tests, tracked
  sample-deck verification, local fresh-sample staging, and package payload
  verification.
Observed:
- Host C tests passed, including the daily-use workflow, 3DS key translation,
  escape-key coverage, battery retry policy, per-screen navigation repeat
  coverage, and idle input wait tiers.
- Converter and text-deck verifier tests passed.
- Tracked sample decks verified in source, local SD mirror, and `dist/sdmc/`.
- The current artifact includes immediate selected-deck summary refresh after
  saving daily limits, so selector due counts do not stay stale.
Expected:
- Current code-complete artifact remains ready for the final manual M7
  emulator or hardware pass.
Evidence:
- `make verify-local` completed with exit code 0.
Result: pass for automated gate only
Notes:
- Button-level review, rendered palette contrast, save-feedback visibility,
  SD-card behavior, and relaunch persistence still need the final manual
  emulator or hardware acceptance pass.

## 2026-06-04 - Live Summary Coverage Local Gate

Build: `702a7b3`
Command: `make verify-local`
Gate: pass
Sample prep: `make verify-local` ran `prepare-local-samples-fresh` for the
local SD mirror and `package-sd` for the copy-ready payload.
Decks: `limits-demo`, `sample`
Steps:
- Ran host C tests, converter tests, text-deck verifier tests, tracked
  sample-deck verification, local fresh-sample staging, and package payload
  verification.
Observed:
- Host C tests passed, including the daily-use workflow, live deck-summary
  coverage after daily-limit changes, 3DS key translation, escape-key coverage,
  battery retry policy, per-screen navigation repeat coverage, and idle input
  wait tiers.
- Converter and text-deck verifier tests passed.
- Tracked sample decks verified in source, local SD mirror, and `dist/sdmc/`.
Expected:
- Current code-complete artifact remains ready for the final manual M7
  emulator or hardware pass.
Evidence:
- `make verify-local` completed with exit code 0.
Result: pass for automated gate only
Notes:
- Button-level review, rendered palette contrast, save-feedback visibility,
  SD-card behavior, and relaunch persistence still need the final manual
  emulator or hardware acceptance pass.

## 2026-06-04 - Current Warning-Status Local Gate

Build: `54f8060`
Command: `make verify-local`
Gate: pass
Sample prep: `make verify-local` ran `prepare-local-samples-fresh` for the
local SD mirror and `package-sd` for the copy-ready payload.
Decks: `limits-demo`, `sample`
Steps:
- Ran host C tests, converter tests, text-deck verifier tests, tracked
  sample-deck verification, local fresh-sample staging, and package payload
  verification.
Observed:
- Host C tests passed, including the daily-use workflow, 3DS key translation,
  action and confirmation key priority, status-color classification for deck,
  controls, and actions warning context, battery polling cadence, per-screen
  navigation repeat coverage, and idle input wait tiers.
- Converter and text-deck verifier tests passed.
- Tracked sample decks verified in source, local SD mirror, and `dist/sdmc/`.
- The current artifact includes selector, controls, and actions status feedback
  that preserves load-error, reset-needed, ignored-settings, unmatched-state,
  and daily-limit warning context.
Expected:
- Current code-complete artifact remains ready for the final manual M7
  emulator or hardware pass.
Evidence:
- `make verify-local` completed with exit code 0.
Result: pass for automated gate only
Notes:
- Button-level review, rendered palette contrast, save-feedback visibility,
  SD-card behavior, and relaunch persistence still need the final manual
  emulator or hardware acceptance pass.

## 2026-06-04 - Controls Return Warning Local Gate

Build: `fad1f8f`
Command: `make verify-local`
Gate: pass
Sample prep: `make verify-local` ran `prepare-local-samples-fresh` for the
local SD mirror and `package-sd` for the copy-ready payload.
Decks: `limits-demo`, `sample`
Steps:
- Ran host C tests, converter tests, text-deck verifier tests, tracked
  sample-deck verification, local fresh-sample staging, and package payload
  verification.
Observed:
- Host C tests passed, including the daily-use workflow, 3DS key translation,
  action and confirmation key priority, status-color classification for deck,
  controls, actions, and controls-return warning context, battery polling
  cadence, per-screen navigation repeat coverage, and idle input wait tiers.
- Converter and text-deck verifier tests passed.
- Tracked sample decks verified in source, local SD mirror, and `dist/sdmc/`.
- The current artifact includes controls status feedback that preserves
  load-error, reset-needed, ignored-settings, unmatched-state, and daily-limit
  warning context when controls are opened and when they close back to
  deck-specific screens.
Expected:
- Current code-complete artifact remains ready for the final manual M7
  emulator or hardware pass.
Evidence:
- `make verify-local` completed with exit code 0.
Result: pass for automated gate only
Notes:
- Button-level review, rendered palette contrast, save-feedback visibility,
  SD-card behavior, and relaunch persistence still need the final manual
  emulator or hardware acceptance pass.

## 2026-06-04 - Action Cancel Warning Local Gate

Build: `f339186`
Command: `make verify-local`
Gate: pass
Sample prep: `make verify-local` ran `prepare-local-samples-fresh` for the
local SD mirror and `package-sd` for the copy-ready payload.
Decks: `limits-demo`, `sample`
Steps:
- Ran host C tests, converter tests, text-deck verifier tests, tracked
  sample-deck verification, local fresh-sample staging, and package payload
  verification.
Observed:
- Host C tests passed, including the daily-use workflow, 3DS key translation,
  action and confirmation key priority, status-color classification for deck,
  controls, actions, controls-return, and action-cancel warning context,
  battery polling cadence, per-screen navigation repeat coverage, and idle
  input wait tiers.
- Converter and text-deck verifier tests passed.
- Tracked sample decks verified in source, local SD mirror, and `dist/sdmc/`.
- The current artifact includes action and confirmation cancel status feedback
  that preserves reset-needed, ignored-settings, unmatched-state, and
  daily-limit warning context.
Expected:
- Current code-complete artifact remains ready for the final manual M7
  emulator or hardware pass.
Evidence:
- `make verify-local` completed with exit code 0.
Result: pass for automated gate only
Notes:
- Button-level review, rendered palette contrast, save-feedback visibility,
  SD-card behavior, and relaunch persistence still need the final manual
  emulator or hardware acceptance pass.
