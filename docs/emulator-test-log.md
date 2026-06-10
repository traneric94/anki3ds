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

## 2026-06-06 - Automation Precheck Still Blocked

Build: current dirty Citro2D clean-shell worktree after adding
`app_input_policy`
Command: `make run-emulator-m7-smoke`
Gate: automation precheck only
Sample prep: none; target failed before staging or launch
Decks: planned `limits-demo` plus reset `sample`
Steps:
- Retried the deterministic M7 smoke target.
- Extracted and tested the app-shell repeat-input mask locally after the
  automation precheck failed.
Observed:
- `tools/drive_azahar_m7_smoke.py --check-automation` failed because macOS
  denied `osascript` keystrokes.
- No Azahar launch/input or post-run artifact verification occurred.
- Local input-policy extraction verification passed separately with
  `make test-host`, `python3 tools/verify_app_theme.py`,
  `python3 -m unittest tests/test_verify_app_theme.py`, direct
  `make -C app-3ds`, and `git diff --check`.
Expected:
- Automated M7 smoke still requires Accessibility/Assistive Access permission
  for the terminal or Codex host process.
Evidence:
- `make run-emulator-m7-smoke`
- `make test-host`
- `python3 tools/verify_app_theme.py`
- `python3 -m unittest tests/test_verify_app_theme.py`
- `make -C app-3ds`
- `git diff --check`
Result: automation blocked before emulator input; local hardening pass
Notes:
- Manual M7 acceptance remains pending.

## 2026-06-06 - Direct Smoke Reset-Deck Artifact Preflight

Build: current dirty Citro2D clean-shell worktree after extending the
direct-control smoke path with a reset-deck leg
Command: focused smoke-driver tests, focused M7 artifact verifier tests,
`make test-host`, dry-run smoke sequence, `make test`, `git diff --check`,
direct `make -C app-3ds`, then `make verify-m7-preflight`
Gate:
- All listed commands passed.
Sample prep:
- Preflight staged fresh tracked `limits-demo` and `sample` decks in local SD,
  package SD, and Azahar SDMC.
- Preflight cleared root `session.tsv` / `theme.tsv` artifacts and tracked
  sample progress files before the next Azahar fresh-sample pass.
Decks:
- studied deck: `limits-demo`
- reset deck: `sample`
Steps:
- Extended `tools/drive_azahar_m7_smoke.py` after the relaunch persistence
  reopen: return to deck select with `SELECT`, open `sample`, reveal/rate one
  card, reset `sample` with `Y` then `X`, then final-exit with `START` then
  `A`.
- Updated `verify-azahar-m7-smoke-artifacts` to expect studied
  `limits-demo`, reset `sample`, and exact settings
  `limits-demo:5:10 sample:20:200`.
- Updated the synthetic verifier fixture and real-module host smoke test so
  the direct smoke proof includes reset cleanup with `sample` settings
  preserved.
Observed:
- `python3 -m unittest tests/test_drive_azahar_m7_smoke.py` passed.
- `python3 -m unittest tests/test_verify_m7_artifacts.py` passed.
- `make test-host` passed.
- `python3 tools/drive_azahar_m7_smoke.py --dry-run` now prints a 39-step
  sequence ending with `M7_DECKS=limits-demo M7_RESET_DECKS=sample
  M7_EXPECT_SETTINGS=limits-demo:5:10 sample:20:200`.
- Full `make test`, whitespace check, direct 3DS build, sample validation,
  package/local SD staging, fresh Azahar sample staging, and Azahar key-profile
  validation passed.
Expected:
- A successful future `make run-emulator-m7-smoke` should now prove the
  repeatable core path plus reset cleanup/preserved settings in the artifact
  verifier, assuming macOS Accessibility permits Azahar key automation.
Evidence:
- focused smoke-driver unit tests
- focused M7 artifact verifier unit tests
- `make test-host`
- dry-run driver output
- `make test`
- `git diff --check`
- `make -C app-3ds`
- `make verify-m7-preflight`
Result: pass for automated preflight only; pending actual Azahar input
Notes:
- This still does not replace the manual M7 acceptance pass. It strengthens the
  repeatable smoke path and post-run artifact contract while local key
  automation remains dependent on macOS Accessibility permission.

## 2026-06-06 - Status Text Save Warning Composition M7 Preflight

Build: current dirty Citro2D clean-shell worktree after centralizing warning
context plus low-battery suffix composition in `app_status_text`
Command: focused `tests/test_app_status_text.c` through `make test-host`,
direct `make -C app-3ds`, broader `make test`, `git diff --check`, then
`make verify-m7-preflight`
Gate:
- All listed commands passed.
Sample prep:
- Preflight staged fresh tracked `limits-demo` and `sample` decks in local SD,
  package SD, and Azahar SDMC.
- Preflight cleared root `session.tsv` / `theme.tsv` artifacts and tracked
  sample progress files before the Azahar fresh-sample check.
Decks:
- `limits-demo`
- `sample`
Steps:
- Added `app_status_text_copy_with_warning_context_and_low_battery_suffix()`
  so save status composition is tested outside `main.c`.
- Updated `main.c` to use that helper when setting successful save feedback
  that must retain prior warning context and append `; batt low` only when the
  sampled battery state requires it.
- Added host coverage for context-plus-suffix composition, duplicate suffix
  avoidance, and suffix suppression when the battery warning is not active.
Observed:
- Full host/tool tests, 3DS build, whitespace check, sample validation,
  package/local SD staging, fresh Azahar sample staging, and Azahar
  key-profile validation passed.
Expected:
- Save feedback should preserve warning context such as `Settings ignored` or
  `Review limit reached` while adding at most one low-battery save suffix.
Evidence:
- `make test-host`
- `make -C app-3ds`
- `make test`
- `git diff --check`
- `make verify-m7-preflight`
Result: pass for automated preflight only; pending manual M7 interaction acceptance
Notes:
- Azahar was not launched for this checkpoint. Manual emulator or hardware
  evidence is still required for M7 acceptance.

## 2026-06-06 - Deck Select Action Reducer M7 Preflight

Build: current dirty Citro2D clean-shell worktree after extracting deck-selector
input into `app_deck_select_action`
Command: focused `tests/test_app_deck_select_action.c`, `make test-host`,
`make test`, `python3 tools/verify_app_theme.py`, `make -C app-3ds`,
`git diff --check`, then `make verify-m7-preflight`
Gate:
- All listed commands passed.
Sample prep:
- Preflight staged fresh tracked `limits-demo` and `sample` decks in local SD,
  package SD, and Azahar SDMC.
- Preflight cleared root `session.tsv` / `theme.tsv` artifacts and tracked
  sample progress files before the Azahar fresh-sample check.
Decks:
- `limits-demo`
- `sample`
Steps:
- Extracted deck-selector button classification from `main.c` into the pure
  `app_deck_select_action_apply()` reducer.
- Added host coverage for mixed-button chord rejection, exit/rescan/open-deck
  command requests, Up/Down movement, Left/Right paging, and no-op navigation
  on empty deck lists.
Observed:
- Full host/tool tests, 3DS build, theme verifier, whitespace check, sample
  validation, package/local SD staging, fresh Azahar sample staging, and Azahar
  key-profile validation passed.
Expected:
- Deck-selector input should now be safer to change without emulator feedback
  because command classification and selection mutation are host-tested
  separately from SD scanning, deck opening, and session writes.
Evidence:
- focused `cc` compile/run for `tests/test_app_deck_select_action.c`
- `make test-host`
- `make test`
- `python3 tools/verify_app_theme.py`
- `make -C app-3ds`
- `git diff --check`
- `make verify-m7-preflight`
Result: pass for automated preflight only; pending manual M7 interaction acceptance
Notes:
- Azahar was not launched for this checkpoint. Manual emulator or hardware
  evidence is still required for M7 acceptance.

## 2026-06-06 - Battery Status Policy M7 Preflight

Build: current dirty Citro2D clean-shell worktree after extracting battery
status transitions into `app_battery_status`
Command: focused `tests/test_app_battery_status.c`, `make test-host`,
`make test`, `python3 tools/verify_app_theme.py`, `make -C app-3ds`,
`git diff --check`, then `make verify-m7-preflight`
Gate:
- All listed commands passed.
Sample prep:
- Preflight staged fresh tracked `limits-demo` and `sample` decks in local SD,
  package SD, and Azahar SDMC.
- Preflight cleared root `session.tsv` / `theme.tsv` artifacts and tracked
  sample progress files before the Azahar fresh-sample check.
Decks:
- `limits-demo`
- `sample`
Steps:
- Extracted battery status-string transitions from `main.c` into
  `app_battery_status_apply_sample()`.
- Fixed the repeated-low-sample behavior so the low warning remains visible
  while the sample is still low, instead of clearing to `Battery ok` after the
  one-shot latch suppresses duplicate announcements.
- Added host coverage for first low announcement, repeated still-low samples,
  charging/recovery/unavailable clearing, closed-shell no-op samples, and
  unrelated status preservation on normal samples.
Observed:
- Full host/tool tests, 3DS build, theme verifier, whitespace check, sample
  validation, package/local SD staging, fresh Azahar sample staging, and Azahar
  key-profile validation passed.
Expected:
- Low-battery status behavior should now be safer to change without emulator
  feedback because status transitions are host-tested separately from PTMU
  sampling and app drawing.
Evidence:
- focused `cc` compile/run for `tests/test_app_battery_status.c`
- `make test-host`
- `make test`
- `python3 tools/verify_app_theme.py`
- `make -C app-3ds`
- `git diff --check`
- `make verify-m7-preflight`
Result: pass for automated preflight only; pending manual M7 interaction acceptance
Notes:
- Azahar was not launched for this checkpoint. Manual emulator or hardware
  evidence is still required for M7 acceptance.

## 2026-06-06 - Confirm Action Reducer M7 Preflight

Build: current dirty Citro2D clean-shell worktree after extracting
suspend/restore/reset/exit confirmation input into `app_confirm_action`
Command: focused `tests/test_app_confirm_action.c`, `make test-host`,
`make test`, `python3 tools/verify_app_theme.py`, `make -C app-3ds`,
`git diff --check`, then `make verify-m7-preflight`
Gate:
- All listed commands passed.
Sample prep:
- Preflight staged fresh tracked `limits-demo` and `sample` decks in local SD,
  package SD, and Azahar SDMC.
- Preflight cleared root `session.tsv` / `theme.tsv` artifacts and tracked
  sample progress files before the Azahar fresh-sample check.
Decks:
- `limits-demo`
- `sample`
Steps:
- Extracted confirmation-screen button classification from `main.c` into the
  pure `app_confirm_action_apply()` reducer.
- Added host coverage for mixed-button chord rejection, `B`/`SELECT` cancel,
  `X` confirm, `START` opening nested exit from non-exit confirmations, inert
  `START` on exit confirmation, and cancel status labels.
Observed:
- Full host/tool tests, 3DS build, theme verifier, whitespace check, sample
  validation, package/local SD staging, fresh Azahar sample staging, and Azahar
  key-profile validation passed.
Expected:
- Confirmation input for destructive/reset/exit flows should now be safer to
  change without emulator feedback because command classification is
  host-tested and separated from file/session side effects.
Evidence:
- focused `cc` compile/run for `tests/test_app_confirm_action.c`
- `make test-host`
- `make test`
- `python3 tools/verify_app_theme.py`
- `make -C app-3ds`
- `git diff --check`
- `make verify-m7-preflight`
Result: pass for automated preflight only; pending manual M7 interaction acceptance
Notes:
- Azahar was not launched for this checkpoint. Manual emulator or hardware
  evidence is still required for M7 acceptance.

## 2026-06-06 - Settings Action Reducer M7 Preflight

Build: current dirty Citro2D clean-shell worktree after extracting daily-limit
editor input into `app_settings_action`
Command: focused `tests/test_app_settings_action.c`, `make test-host`,
`make test`, `python3 tools/verify_app_theme.py`, `make -C app-3ds`,
`git diff --check`, then `make verify-m7-preflight`
Gate:
- All listed commands passed.
Sample prep:
- Preflight staged fresh tracked `limits-demo` and `sample` decks in local SD,
  package SD, and Azahar SDMC.
- Preflight cleared root `session.tsv` / `theme.tsv` artifacts and tracked
  sample progress files before the Azahar fresh-sample check.
Decks:
- `limits-demo`
- `sample`
Steps:
- Extracted daily-limit editor button handling from `main.c` into the pure
  `app_settings_action_apply()` reducer.
- Added host coverage for mixed-button chord rejection, selected-field
  movement, preset value cycling, save/cancel/exit requests, and
  `no changes` / `unsaved changes` edit-status classification.
- Wired the clean shell to show `no changes` on settings entry,
  `unsaved changes` after value edits, `Limits canceled; discarded` after
  canceling an unsaved draft, and `Exit loses unsaved limits` before exit
  confirmation would discard unsaved settings.
Observed:
- Full host/tool tests, 3DS build, theme verifier, whitespace check, sample
  validation, package/local SD staging, fresh Azahar sample staging, and Azahar
  key-profile validation passed.
Expected:
- Daily-limit editor input should now be safer to change without emulator
  feedback because command classification and draft mutation are host-tested.
Evidence:
- focused `cc` compile/run for `tests/test_app_settings_action.c`
- `make test-host`
- `make test`
- `python3 tools/verify_app_theme.py`
- `make -C app-3ds`
- `git diff --check`
- `make verify-m7-preflight`
Result: pass for automated preflight only; pending manual M7 interaction acceptance
Notes:
- Azahar was not launched for this checkpoint. Manual emulator or hardware
  evidence is still required for M7 acceptance.

## 2026-06-06 - Review Action Reducer M7 Preflight

Build: current dirty Citro2D clean-shell worktree after extracting review
action side effects into `app_review_action`
Command: `python3 -m unittest tests/test_verify_app_theme.py`, `make test`,
`python3 tools/verify_app_theme.py`, `make -C app-3ds`, `git diff --check`,
then `make verify-m7-preflight`
Gate:
- All listed commands passed.
Sample prep:
- Preflight staged fresh tracked `limits-demo` and `sample` decks in local SD,
  package SD, and Azahar SDMC.
- Preflight cleared root `session.tsv` / `theme.tsv` artifacts and tracked
  sample progress files before the Azahar fresh-sample check.
Decks:
- `limits-demo`
- `sample`
Steps:
- Extracted review action side effects from `main.c` into the pure
  `app_review_action_apply()` reducer.
- Added host coverage for scroll clamp/up/down, scroll reset on
  reveal/rating/undo, dirty flags, review-log metadata, and exit signaling.
- Updated the theme verifier so `main.c` may expose answer/rating transitions
  through `app_review_action_apply()` while `study_backend.c` remains excluded
  from app-shell transition evidence.
Observed:
- Focused verifier tests passed for both direct backend transition calls and
  the reducer-mediated transition path.
- Full host/tool tests, 3DS build, theme verifier, whitespace check, sample
  validation, package/local SD staging, fresh Azahar sample staging, and Azahar
  key-profile validation passed.
Expected:
- The current reducer split should preserve the same review behavior while
  keeping review action side effects directly host-testable and renderer-free.
Evidence:
- `python3 -m unittest tests/test_verify_app_theme.py`
- `make test`
- `python3 tools/verify_app_theme.py`
- `make -C app-3ds`
- `git diff --check`
- `make verify-m7-preflight`
Result: pass for automated preflight only; pending manual M7 interaction acceptance
Notes:
- Azahar was not launched for this checkpoint. Manual emulator or hardware
  evidence is still required for M7 acceptance.

## 2026-06-06 - Completed-Today Queue State M7 Preflight

Build: current dirty Citro2D clean-shell worktree after adding compact
`completed_today_count` / `completed_today_index` state rows
Command: focused backend and clean-shell tests, `python3 -m unittest
tests/test_verify_m7_artifacts.py`, `make test`, `make -C app-3ds`,
`python3 tools/verify_app_theme.py`, `git diff --check`, then
`make verify-m7-preflight`
Gate:
- All listed commands passed.
Sample prep:
- Preflight staged fresh tracked `limits-demo` and `sample` decks in local SD,
  package SD, and Azahar SDMC.
- Preflight cleared root `session.tsv` / `theme.tsv` artifacts and tracked
  sample progress files before the Azahar fresh-sample check.
Decks:
- `limits-demo`
- `sample`
Steps:
- Persisted same-day completed-card indexes in compact `state.tsv`.
- Changed the clean-shell backend queue to offer due introduced cards not
  completed today before offering new cards, including sparse introduced-card
  layouts where earlier cards are still new.
- Updated the M7 artifact verifier to validate completed-today compact rows.
Observed:
- Host C backend coverage passed for sparse introduced-before-new day rollover,
  same-day completed-today reload, malformed completed-today state rejection,
  and existing first-load dayless migration behavior.
- Clean-shell daily-use integration passed.
- M7 artifact verifier tests passed with completed-today row coverage.
- Full preflight passed and refreshed app/assets/sample decks in all non-launch
  SD roots.
Expected:
- Same-day relaunch should not reopen cards already completed today, and a true
  next-day rollover should review introduced cards before new cards consume
  `new_limit`.
Evidence:
- `make test`
- `make -C app-3ds`
- `python3 tools/verify_app_theme.py`
- `git diff --check`
- `make verify-m7-preflight`
Result: pass for automated preflight only; pending manual M7 interaction acceptance
Notes:
- Azahar was not launched for this checkpoint. Manual emulator or hardware
  evidence is still required for M7 acceptance.

## 2026-06-06 - Day-Aware Daily-Limit M7 Preflight

Build: current dirty Citro2D clean-shell worktree after adding day-aware
daily-limit counters, first-load progress-day migration persistence, and
compact-state progress-day artifact checks, then restoring local-calendar-day
daily rollover through `app_time`, and tightening root session day rollover
diagnostics
Command: `make test`, `make -C app-3ds`, `python3 tools/verify_app_theme.py`,
`git diff --check`, then `make verify-m7-preflight`
Gate:
- All listed commands passed.
Sample prep:
- Preflight staged fresh tracked `limits-demo` and `sample` decks in local SD,
  package SD, and Azahar SDMC.
- Preflight cleared root `session.tsv` / `theme.tsv` artifacts and tracked
  sample progress files before the Azahar fresh-sample check.
Decks:
- `limits-demo`
- `sample`
Steps:
- Ran the full host/tool/sample/package/local/Azahar preflight after compact
  `state.tsv` gained `progress_day`, `introduced_today_count`, and
  `reviewed_today_count`, then again after `main.c` switched session days and
  daily-limit rollover from `timestamp / 86400` to the local-calendar-day
  helper, then again after rollover-only session day updates were saved before
  mode-specific early continues.
Observed:
- Host C tests passed, including local-day daily-limit counters, day rollover,
  first-load day establishment for existing progress, compact state save/load
  validation, review-card footer strings, and the integrated clean-shell
  daily-use workflow.
- Host C tests also passed for `tests/test_app_time.c`, covering leap dates,
  max-day clamping, invalid timestamps, and UTC-evening versus local-day
  rollover.
- Host C tests also passed for `tests/test_study_session.c`, covering
  rollover-only `current_day` / `updated_at` updates that preserve the last
  user-visible event.
- Converter, text-deck verifier, app-theme verifier, M7 artifact verifier,
  Azahar smoke-driver planning tests, FE theme asset tests, and Azahar
  key-profile verification passed.
- M7 artifact verifier tests now reject stale compact `progress_day` evidence
  when it does not match root `session.tsv current_day`.
- The current `.3dsx`, `.smdh`, FE theme assets, and fresh tracked text sample
  decks are staged for local/package/Azahar SD roots.
Expected:
- Current day-aware daily-limit build should be ready for a manual M7 emulator
  pass without stale sample progress or root session diagnostics.
Evidence:
- `make test`
- `make -C app-3ds`
- `python3 tools/verify_app_theme.py`
- `git diff --check`
- `make verify-m7-preflight`
Result: pass for automated preflight only; pending manual M7 interaction acceptance
Notes:
- Azahar was not launched for this checkpoint. The next proof step is a manual
  `make run-emulator-fresh-samples` pass or a hardware pass, followed by
  `make verify-m7-artifacts` with exact `M7_EXPECT_SETTINGS` and any reset or
  log-skipped options matching the run.
- This preflight now covers day-counter rollover, first-load progress-day
  migration, and repeat daily-review rewind through host tests. It still does
  not replace manual M7 interaction evidence.

## 2026-06-06 - Raw Text-View Contract M7 Preflight

Build: current dirty Citro2D clean-shell worktree after parallel renderer/backend split
Command: `make verify-m7-preflight`
Gate:
- `make verify-m7-preflight` passed.
Sample prep:
- Preflight staged fresh tracked `limits-demo` and `sample` decks in local SD,
  package SD, and Azahar SDMC.
- Preflight cleared root `session.tsv` / `theme.tsv` artifacts and tracked
  sample progress files before the Azahar fresh-sample check.
Decks:
- `limits-demo`
- `sample`
Steps:
- Ran full host/tool/sample/package/local/Azahar preflight after replacing the
  legacy review adapter with the raw `app_flashcard_text_view_build()` contract.
Observed:
- Host C tests passed, including the raw review text-view contract, current
  clean-shell daily-use integration test, deck/settings/confirm contracts,
  backend, controls, key-map, power, status-text, review-log, and session
  coverage.
- Converter, text-deck verifier, app-theme verifier, M7 artifact verifier,
  Azahar smoke-driver planning tests, FE theme asset tests, and Azahar
  key-profile verification passed.
- The current `.3dsx`, `.smdh`, FE theme assets, and fresh tracked text sample
  decks are staged for local/package/Azahar SD roots.
Expected:
- Current raw-string Citro2D build should be ready for a manual M7 emulator
  pass without stale sample progress or root session diagnostics.
Evidence:
- `make verify-m7-preflight`
Result: pass for automated preflight only; pending manual M7 interaction acceptance
Notes:
- Azahar was not launched by this preflight. The next proof step is a manual
  `make run-emulator-fresh-samples` pass or a hardware pass, followed by
  `make verify-m7-artifacts` with exact `M7_EXPECT_SETTINGS` and any reset or
  log-skipped options matching the run.

## 2026-06-06 - Post-Contract M7 Preflight

Build: current dirty Citro2D clean-shell worktree after review/deck/settings/confirm string contract extraction
Command: `make verify-m7-preflight`
Gate:
- `make verify-m7-preflight` passed.
Sample prep:
- Preflight staged fresh tracked `limits-demo` and `sample` decks in local SD,
  package SD, and Azahar SDMC.
- Preflight cleared root `session.tsv` / `theme.tsv` artifacts and tracked
  sample progress files before the Azahar fresh-sample check.
Decks:
- `limits-demo`
- `sample`
Steps:
- Ran full host/tool/sample/package/local/Azahar preflight for the current app.
Observed:
- Host C tests passed, including `app_confirm_contract`,
  `app_deck_select_contract`, `app_settings_contract`, key mapping, backend,
  session, review-log, settings, power, status-text coverage, and the
  integrated clean-shell daily-use workflow test.
- Converter, text-deck verifier, app-theme verifier, M7 artifact verifier,
  Azahar smoke-driver planning tests, FE theme asset tests, and Azahar
  key-profile verification passed.
- The current `.3dsx`, `.smdh`, FE theme assets, and fresh tracked text sample
  decks are staged for local/package/Azahar SD roots.
Expected:
- Current post-contract build should be ready for a manual M7 emulator pass
  without stale sample progress or root session diagnostics.
Evidence:
- `make verify-m7-preflight`
Result: pass for automated preflight only; pending manual M7 interaction acceptance
Notes:
- Azahar was not launched by this preflight. The next proof step is a manual
  `make run-emulator-fresh-samples` pass or a hardware pass, followed by
  `make verify-m7-artifacts` with exact `M7_EXPECT_SETTINGS` and any reset or
  log-skipped options matching the run.

## 2026-06-06 - M7 Preflight And Smoke Automation Check

Build: current dirty Citro2D clean-shell worktree
Command: `make verify-m7-preflight`, then `make run-emulator-m7-smoke`
Gate:
- `make verify-m7-preflight` passed.
- `make run-emulator-m7-smoke` intentionally stopped at
  `check-azahar-smoke-automation`.
Sample prep:
- Preflight staged fresh tracked `limits-demo` and `sample` decks in local SD,
  package SD, and Azahar SDMC.
- Preflight cleared root `session.tsv` / `theme.tsv` artifacts and tracked
  sample progress files before the Azahar fresh-sample check.
Decks:
- `limits-demo`
- `sample`
Steps:
- Ran full host/tool/sample/package/local/Azahar preflight.
- Attempted deterministic Azahar M7 smoke automation.
Observed:
- Preflight passed, including Azahar key-profile validation.
- macOS denied `System Events` keystrokes for the smoke driver.
- The smoke target now fails before restaging samples or relaunching Azahar
  when that Accessibility permission is missing.
Expected:
- Preflight should pass before a manual or automated M7 session.
- Automated smoke requires Accessibility permission for the terminal/Codex host.
Evidence:
- `make verify-m7-preflight`
- `make run-emulator-m7-smoke`
- `python3 -m unittest tests/test_drive_azahar_m7_smoke.py`
- `make test-tools`
Result: preflight pass; automated smoke blocked by macOS Accessibility
Notes:
- No post-run `make verify-m7-artifacts` acceptance was run because no
  automated study actions were sent.
- Next M7 acceptance path is either grant Accessibility permission and rerun
  `make run-emulator-m7-smoke`, which now runs the multi-deck smoke artifact
  verifier after a successful driver pass, or run the manual checklist and
  verify the tested SD root afterward.

## 2026-06-04 - FE Background Viewer Visual Pass

Build: `5d941d1`
Command: `make install-azahar-fe-bg-viewer`, then launch
`tools/fe-bg-viewer-3ds/fe-bg-viewer.3dsx` in Azahar
Steps:
- Regenerated FE framebuffer assets from the `[F2E]` WAve source backgrounds.
- Built and installed the standalone FE background viewer into Azahar SDMC.
- Launched the viewer through `Azahar.app`.
- Inspected the live emulator window.
Observed:
- User-provided screenshot showed the same FE background rendered upright on
  both top and bottom screens.
- The image was dimmed, centered, and legible enough for a background test.
Expected:
- The direct-copy `*_bgr888_fb.bin` assets display upright on both 3DS
  framebuffers without scrambled or rotated output.
Result: pass
Notes:
- This proves the isolated renderer path, not integration into the main
  `anki3ds` review UI.
- An attempted Azahar `--dump-video` capture path did not produce a `.webm`, so
  the verified evidence for this checkpoint is the live emulator screenshot.

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

## 2026-06-04 - M7 Preflight Target Gate

Build: `0959159`
Command: `make verify-m7-preflight`
Gate: pass for automated pre-manual gate
Sample prep: `verify-local` plus `prepare-azahar-samples-fresh`
Decks: `limits-demo`, `sample`
Steps:
- Added `make verify-m7-preflight` as a combined non-launching M7 setup gate.
- Ran host C tests, converter tests, text-deck verifier tests, tracked
  sample-deck verification, local fresh-sample staging, package payload
  verification, and Azahar fresh-sample staging.
Observed:
- Host C tests passed.
- Converter and text-deck verifier tests passed.
- Tracked sample decks verified in source, local SD mirror, `dist/sdmc/`, and
  Azahar's SDMC directory.
- Azahar sample progress and diagnostic files were cleared for the tracked
  sample decks.
Expected:
- Running `make verify-m7-preflight` immediately before manual M7 emulator
  acceptance should leave the app artifact, package payload, local SD mirror,
  and Azahar sample decks in a known-good state without launching the emulator.
Evidence:
- `make verify-m7-preflight` completed with exit code 0.
Result: pass for automated pre-manual gate only
Notes:
- The emulator app was not launched in this gate. Button-level review,
  rendered palette contrast, save-feedback visibility, SD-card behavior,
  relaunch persistence, and long-idle input wake feel still need the final
  manual emulator or hardware acceptance pass.

## 2026-06-04 - Azahar Fresh Samples Preflight

Build: `515f995`
Command: `make verify-azahar-fresh-samples`
Gate: pass for emulator SD staging only
Sample prep: `prepare-azahar-samples-fresh`
Decks: `limits-demo`, `sample`
Steps:
- Verified tracked sample decks in source.
- Installed tracked sample decks into Azahar's SDMC directory.
- Removed stale progress and diagnostic files for the tracked sample decks.
- Verified the Azahar SDMC sample deck folders after staging.
Observed:
- `limits-demo` verified as text-only with 6 cards, new limit 2/day, review
  limit 5/day.
- `sample` verified as text-only with 11 cards, new limit 20/day, review limit
  200/day.
- `media-demo` was removed from Azahar's sample deck root if present.
Expected:
- `make run-emulator-fresh-samples` should launch against clean tracked sample
  deck progress for the final manual M7 emulator pass.
Evidence:
- `make verify-azahar-fresh-samples` completed with exit code 0.
Result: pass for emulator SD staging only
Notes:
- The emulator app was not launched in this preflight. Button-level review,
  rendered palette contrast, save-feedback visibility, SD-card behavior, and
  relaunch persistence still need the final manual emulator or hardware
  acceptance pass.

## 2026-06-04 - Current Local Pre-Manual Gate

Build: `3dd47b3`
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
  command classification, scheduler migration accounting, review-log day
  fields, deck-summary helper null-safety, battery polling cadence, and idle
  input wait tiers.
- Converter and text-deck verifier tests passed.
- Tracked sample decks verified in source, local SD mirror, and `dist/sdmc/`.
- The local SD mirror was prepared with fresh tracked-sample progress cleared.
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

## 2026-06-04 - Deck Metadata Duplicate-Key Gate

Build: local working tree
Command: `python3 -m unittest tests/test_verify_text_deck.py`
Steps:
- Tightened the text-deck verifier to reject duplicate JSON keys in
  `deck.json`.
Observed:
- Text-deck verifier tests passed.
Expected:
- Preflight verification should reject ambiguous metadata instead of letting
  Python's JSON parser silently choose one key occurrence.
Result: pass for verifier gate
Notes:
- Converter output already writes unique keys, so normal converted decks are
  unaffected.

## 2026-06-04 - Deck Name Preflight Escape Gate

Build: local working tree
Command: `python3 -m unittest tests/test_verify_text_deck.py`
Steps:
- Tightened the text-deck verifier to reject deck display names that rely on
  JSON escapes unsupported by the 3DS display-name parser.
Observed:
- Text-deck verifier tests passed.
Expected:
- Preflight verification should catch deck names that Python JSON accepts but
  the app would display as the folder id.
Result: pass for verifier gate
Notes:
- Converter output already writes literal UTF-8 names, so normal converted
  decks are unaffected.

## 2026-06-04 - Converter Review-Log Migration Gate

Build: local working tree
Command: `python3 -m unittest tests/test_converter.py`
Steps:
- Added converter migration support for complete `review-log.tsv` rows when a
  converter-generated deck changes between single-folder and split output, or
  when split chunk boundaries move.
- Review-log rows are filtered by the target card IDs and expected field count
  so stale chunk logs and truncated diagnostic rows do not follow unrelated
  cards.
Observed:
- Converter tests passed.
Expected:
- Updating large text decks should preserve diagnostic study history for cards
  that still exist, while dropping rows for removed cards.
Result: pass for converter gate
Notes:
- The review log remains diagnostic; `state.tsv` is still the source of review
  progress loaded by the 3DS app.

## 2026-06-04 - Warm High-Contrast Palette Gate

Build: local working tree
Command: `make -C app-3ds`
Steps:
- Replaced violet/magenta focus and Easy/New accents with a warmer
  high-contrast terminal palette.
- Selected/focused rows now use bright white reverse video; Easy ratings use
  the same positive green as Good; new counts use bright white neutral text.
Observed:
- The 3DS target rebuilt successfully.
Expected:
- The UI should stay readable on the dark 3DS console without relying on blue,
  cyan, or violet-like colors.
Result: pass for build only; pending manual render check
Notes:
- Manual emulator or hardware rendering is still needed for true 3DS screen
  contrast.

## 2026-06-04 - Current M7 Preflight Gate

Build: `0d2b636`
Command: `make verify-m7-preflight`
Gate: pass for automated pre-manual gate
Sample prep: `verify-local` plus `prepare-azahar-samples-fresh`
Decks: `limits-demo`, `sample`
Steps:
- Ran host C tests, converter tests, text-deck verifier tests, tracked
  sample-deck verification, local fresh-sample staging, package payload
  verification, and Azahar fresh-sample staging.
- Built or reused the current `app-3ds/anki3ds.3dsx` and `anki3ds.smdh`.
- Staged the copy-ready SD-card payload in `dist/sdmc/3ds/anki3ds/`.
- Cleared tracked sample progress and diagnostic files in the local SD mirror
  and Azahar sample folders.
Observed:
- Host C tests passed, including the 3DS key-translation branch.
- Converter and text-deck verifier tests passed.
- `limits-demo` verified as 6 cards with `new_limit` 2/day and `review_limit`
  5/day in source, local SD, package payload, and Azahar SDMC staging.
- `sample` verified as 11 cards with `new_limit` 20/day and `review_limit`
  200/day in source, local SD, package payload, and Azahar SDMC staging.
- `dist/sdmc/3ds/anki3ds/` contains only the app artifacts and tracked sample
  deck source files for the current manual-copy payload.
Expected:
- The current artifact is ready for `make run-emulator-fresh-samples` or a
  CTR-001 SD-card copy using `dist/sdmc/3ds/anki3ds/`.
Evidence:
- `make verify-m7-preflight` completed with exit code 0.
Result: pass for automated pre-manual gate only
Notes:
- The emulator app was not launched in this gate. Button-level review,
  rendered palette contrast, save-feedback visibility, SD-card behavior,
  relaunch persistence, and CTR-001 battery-line rendering still need the final
  manual emulator or hardware acceptance pass.

## 2026-06-04 - Current Azahar Launch Check

Build: `636b605`
Command: `make run-emulator-fresh-samples`
Gate: launch only; not acceptance
Sample prep: `verify-azahar-fresh-samples`
Decks: `limits-demo`, `sample`
Steps:
- Staged fresh tracked sample decks in Azahar's SDMC directory.
- Cleared tracked sample progress and diagnostic files in the Azahar sample
  folders.
- Built or reused the current `app-3ds/anki3ds.3dsx`.
- Launched the `.3dsx` through Azahar.
Observed:
- The first sandboxed launch attempt staged the samples but failed at `open`
  with a macOS Launch Services communication error.
- Rerunning the same target with GUI escalation succeeded.
- `osascript -e 'application "Azahar" is running'` returned `true`.
- Azahar SDMC sample folders contained only `cards.tsv`, `deck.json`, and
  `settings.tsv` for the tracked sample decks after staging.
- A window-count check was blocked because System Events does not have
  assistive access on this machine.
Expected:
- The current artifact should be ready for manual Azahar input testing against
  fresh sample-deck progress.
Evidence:
- `make run-emulator-fresh-samples` completed with exit code 0 after GUI
  escalation.
- Azahar reported running through macOS application state.
Result: pass for launch command only
Notes:
- This does not prove the M7 acceptance path. Button-level review, rendered
  palette contrast, save-feedback visibility, SD-card behavior, relaunch
  persistence, and CTR-001 battery-line rendering still need manual emulator or
  hardware acceptance.

## 2026-06-04 - Post-Launch Azahar SD Evidence

Build: `b66ec48` repo state; app code unchanged since `0d2b636`
Command: read-only inspection of Azahar SDMC files
Gate: evidence only; not acceptance
Sample prep: previous `make run-emulator-fresh-samples`
Decks: `limits-demo`, `sample`
Steps:
- Inspected app-owned files in Azahar's SDMC sample deck folders after the
  current launch check.
- Read `limits-demo/state.tsv` and `limits-demo/review-log.tsv` without
  resetting sample progress.
Observed:
- `limits-demo/state.tsv`, `limits-demo/state.tsv.bak`, and
  `limits-demo/review-log.tsv` existed with June 4, 2026 timestamps around
  10:50.
- `limits-demo/state.tsv` contained a complete 6-card state file.
- `limits-demo/review-log.tsv` contained 11 rating rows: one Hard rating for
  `limit-0001`, repeated Again ratings for `limit-0002`, and a final Easy
  rating for `limit-0002`.
- `sample` still contained only source deck files and no app-owned progress
  files.
Expected:
- Emulator interaction after launch should save per-deck progress beside the
  active deck and append diagnostic review-log rows without affecting unrelated
  decks.
Evidence:
- `wc -l` reported 8 lines in `limits-demo/state.tsv` and 11 lines in
  `limits-demo/review-log.tsv`.
- `sample` folder inspection listed only `cards.tsv`, `deck.json`, and
  `settings.tsv`.
Result: pass for SD evidence only
Notes:
- This is useful evidence that the launched app wrote emulator SD progress and
  diagnostic logs for `limits-demo`, but it does not prove which physical or
  emulator keys were pressed, rendered contrast, status readability, relaunch
  persistence, or the full two-deck M7 path.

## 2026-06-04 - Paper Flashcard Panel Local Gate

Build: local working tree
Commands:
- `make -C app-3ds`
- `make test`
- `make verify-package-sd`
Steps:
- Replaced the review screen's plain separator-line card area with an original
  light paper flashcard panel.
- Kept the surrounding app shell dark and console-native, with amber panel trim
  and black text on the card face.
- Reduced review text wrapping width to match the padded card panel so scroll
  hints and rendered text stay aligned.
Observed:
- The 3DS target rebuilt successfully.
- Host C tests, converter tests, and text-deck verifier tests passed through
  `make test`.
- `dist/sdmc/3ds/anki3ds/` was refreshed and verified with the current app
  artifact and tracked text sample decks.
Expected:
- The review surface should feel more like a physical flashcard while
  preserving the existing low-overhead console renderer and key workflow.
Result: pass for automated build/test only; pending manual render check
Notes:
- Manual emulator or hardware rendering is still needed for true 3DS screen
  contrast.

## 2026-06-04 - Contextual Key Prompt Palette Local Gate

Build: local working tree
Commands:
- `make -C app-3ds`
- `make test`
Steps:
- Added a small shared renderer for amber key prompts.
- Applied the key-prompt palette to contextual controls and confirmation
  screens.
- Changed deck paging prompts away from bare `L/R` wording so D-pad or Circle
  Pad left/right is not confused with shoulder buttons.
Observed:
- The 3DS target rebuilt successfully.
- Host C tests, converter tests, and text-deck verifier tests passed through
  `make test`.
Expected:
- The in-app controls screen should better match the dark-console palette and
  make the daily-use key map easier to follow during manual acceptance.
Result: pass for automated build/test only; pending manual render check
Notes:
- Manual emulator or hardware rendering is still needed for true 3DS screen
  contrast.

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

## 2026-06-04 - Amber Focus Palette Local Gate

Build: local working tree
Commands:
- `make -C app-3ds`
- `make test`
Steps:
- Switched normal selected/focused rows to warm amber reverse video instead of
  white or violet reverse video.
- Kept app chrome on warm amber, Good/review/safe cues on green,
  Hard/learning/warning cues on amber, Again/suspended/error cues on red, and
  Easy/new/neutral text on bright white.
- Updated palette docs so the code and project notes describe the same
  dark-terminal color system.
Observed:
- The 3DS target rebuilt successfully.
- Host C tests, converter tests, and text-deck verifier tests passed.
Expected:
- The app should avoid hard-to-read blue, cyan, and violet-like colors while
  giving deck focus, review actions, and status severities distinct cues on a
  dark console background.
Result: pass for automated build/test only; pending manual render check
Notes:
- Manual emulator or hardware rendering is still needed for true 3DS screen
  contrast.

## 2026-06-04 - Migrated Review Log Cap Local Gate

Build: local working tree
Commands:
- `python3 -m unittest tests/test_converter.py`
- `make test`
Steps:
- Added the 262144-byte device review-log cap to converter-rewritten
  `review-log.tsv` files.
- Kept the newest complete matching migrated log rows that fit the cap when a
  converter-generated deck changes between single-folder and split-folder
  output.
- Updated deck-format and converter algorithm docs with the cap behavior.
Observed:
- Focused converter tests passed, including the oversized migrated-log case.
- Host C tests, converter tests, and text-deck verifier tests passed through
  `make test`.
Expected:
- Reimporting a text deck should not strand migrated diagnostic logs above the
  size where the 3DS app can continue appending study transitions.
Result: pass for automated gate
Notes:
- This is converter-side behavior only; the app's append cap remains covered by
  host C review-log tests.

## 2026-06-04 - Dark Console Palette Hierarchy Local Gate

Build: local working tree
Commands:
- `make -C app-3ds`
- `make test`
Steps:
- Kept the app off blue, cyan, and violet terminal colors.
- Refined the palette into amber/chalk/green/red semantics: amber for app
  chrome, labels, key prompts, and focus; green for safe progress; red for
  destructive/error state; bright white for card text and neutral values.
- Added color to review headers, deck due counts, summary metrics, and bottom
  key prompts without changing input behavior.
Observed:
- The 3DS target rebuilt successfully.
- Host C tests, converter tests, and text-deck verifier tests passed through
  `make test`.
Expected:
- The dark-console UI should be easier to scan on 3DS LCDs, especially at low
  brightness, while preserving the existing review-button color meanings.
Result: pass for automated build/test only; pending manual render check
Notes:
- Manual emulator or hardware rendering is still needed for true 3DS screen
  contrast.

## 2026-06-04 - Paper Card And Rating Chip M7 Preflight

Build: `bead593`
Command: `make verify-m7-preflight`
Gate: pass
Sample prep: `verify-local` plus `prepare-azahar-samples-fresh`
Decks: `limits-demo`, `sample`
Steps:
- Ran host C tests, converter tests, text-deck verifier tests, tracked
  sample-deck verification, local fresh-sample staging, package payload
  verification, and Azahar fresh-sample staging.
- Used the current artifact with light paper review cards, amber terminal
  chrome, and bracketed bottom-screen rating chips.
Observed:
- Host C tests passed, including daily-use workflow persistence, 3DS key
  translation, command chord rejection, per-screen navigation repeat coverage,
  battery polling cadence, idle input wait tiers, scheduler behavior,
  state/settings persistence, and review-log recovery/capping behavior.
- Converter and text-deck verifier tests passed.
- Source sample decks, local SD sample decks, `dist/sdmc`, and Azahar sample
  decks all verified as text-only decks with fresh tracked-sample progress
  cleared where applicable.
- `dist/sdmc/3ds/anki3ds/` contains the copy-ready `.3dsx`, `.smdh`, and
  tracked sample decks for manual copy or release staging.
Expected:
- The current build is ready for `make run-emulator-fresh-samples` or hardware
  copy before the final manual M7 study-session acceptance pass.
Evidence:
- `make verify-m7-preflight` completed with exit code 0.
Result: pass for automated preflight only
Notes:
- Full M7 acceptance still requires manual emulator or hardware interaction:
  review due cards from both sample decks, suspend a card, undo a rating,
  restore suspended cards, save changed daily limits, relaunch, and confirm
  state plus settings persistence.

## 2026-06-04 - Split Card Help Theme Local Gate

Build: local working tree
Commands:
- `make -C app-3ds`
- `make test`
Steps:
- Kept the review front on the top screen and moved the revealed back to the
  bottom screen.
- Reframed the contextual controls UI as a help screen and added help-page `X`
  theme cycling for the card panel trim.
- Added an FE asset note documenting candidate local asset folders and why no
  FE assets are vendored into the public repo yet.
Observed:
- The 3DS target rebuilt successfully.
- Host C tests, converter tests, and text-deck verifier tests passed through
  `make test`.
Expected:
- Review should feel more like a two-screen flashcard: prompt/front on top,
  answer/rating strip on bottom after reveal, and help available before reveal.
Result: pass for automated build/test only; pending manual render check
Notes:
- Manual emulator or hardware rendering is still needed to confirm bottom-screen
  answer fit, panel theme contrast, and control prompts on real 3DS dimensions.

## 2026-06-04 - 4096-Card State Loader Boot Fix

Build: local working tree
Commands:
- `make test`
- `make -C app-3ds`
- `make verify-package-sd`
- `make run-emulator-fresh-samples`
Steps:
- Reproduced the Azahar 0 FPS boot failure as an immediate `HW.Memory` loop at
  `PC 0x00000000`.
- Mapped the crash address back to `review_state_load_file`.
- Kept the 4096-card cap and moved the temporary state-load scheduler copy plus
  card-match bitmap off the 3DS stack.
- Updated the emulator launcher to pass Azahar an absolute `.3dsx` path.
Observed:
- Host tests, the 3DS build, and package verification passed.
- The rebuilt `review_state_load_file` prologue allocates 364 bytes on the
  stack instead of copying a full 4096-card scheduler session.
- A fresh-sample Azahar launch no longer logs the immediate `HW.Memory` crash
  loop.
Expected:
- Fresh sample decks should boot without the 0 FPS startup stall while keeping
  the high per-deck card cap.
Result: pass for boot-crash regression; pending manual screen interaction
Notes:
- macOS assistive-access restrictions still prevented automated Azahar window
  inspection in this session.

## 2026-06-04 - Azahar Control Profile Preflight Gate

Build: local working tree
Commands:
- `make verify-azahar-controls`
- `make test`
- `make verify-m7-preflight`
Steps:
- Added a local Azahar control-profile verifier for the anki3ds emulator key
  map.
- Verified face buttons map to keyboard `A`/`B`/`X`/`Y`, shoulder buttons map
  to `L`/`R`, `SELECT`/`START` map to `N`/`M`, and D-pad plus Circle Pad
  directions map to keyboard arrows.
- Ran the full M7 preflight after wiring the control-profile check into it.
Observed:
- `tools/verify_azahar_controls.py` accepted
  `~/Library/Application Support/Azahar/config/qt-config.ini`.
- Host C tests, 3DS key translation tests, converter tests, text-deck verifier
  tests, and Azahar-control verifier tests passed.
- Local SD, `dist/sdmc`, and Azahar sample-deck staging all verified with
  fresh tracked-sample progress.
Expected:
- M7 emulator preflight now fails early if Azahar rewrites the profile away
  from the documented intuitive keyboard bindings.
Evidence:
- `make verify-m7-preflight` completed with exit code 0.
Result: pass for automated preflight only
Notes:
- Full M7 acceptance still requires manual emulator or hardware interaction for
  rating, suspend, undo, daily-limit editing, relaunch persistence, and visual
  readability.

## 2026-06-04 - M7 Post-Run Artifact Verifier

Build: local working tree
Commands:
- `python3 -m py_compile tools/verify_m7_artifacts.py tests/test_verify_m7_artifacts.py`
- `python3 -m unittest tests/test_verify_m7_artifacts.py`
- `python3 -m unittest tests/test_verify_azahar_controls.py tests/test_verify_text_deck.py`
Steps:
- Added a post-run SD artifact verifier for the M7 daily-use checklist.
- Validated two-deck fixture output containing framed `state.tsv`,
  `settings.tsv`, and 21-field `review-log.tsv` rows with `rating`, `undo`,
  `suspend`, and `restore` evidence.
Observed:
- The verifier accepts normal Unix timestamps, checks scheduler-field bounds by
  field type, and reports missing or malformed artifacts without tracebacks.
Expected:
- After a manual emulator or hardware pass, `make verify-m7-artifacts` should
  provide a quick sanity check that the tested SD root contains durable study
  evidence.
Evidence:
- Focused verifier and existing tool tests passed.
Result: pass for automated post-run artifact gate only
Notes:
- This does not replace manual M7 interaction acceptance. It verifies SD-card
  evidence after that pass has already created progress files.

## 2026-06-04 - M7 Artifact Target Settings Parameters

Build: local working tree
Commands:
- `python3 -m unittest tests/test_verify_m7_artifacts.py`
- `make -n verify-m7-artifacts M7_SDMC=/tmp/sd M7_DECKS="sample limits-demo"
  M7_REQUIRED_EVENTS="rating undo"
  M7_EXPECT_SETTINGS="sample:5:20 limits-demo:1:10"`
Steps:
- Parameterized `make verify-m7-artifacts` so manual passes can declare the
  tested deck ids, required review-log events, and exact daily-limit settings.
Observed:
- The verifier CLI rejects a saved setting that does not match
  `--expect-settings`.
Expected:
- M7 acceptance can now prove daily-limit persistence with the same Make target
  used for post-run SD artifact verification.
Result: pass for automated target wiring only
Notes:
- Manual M7 interaction acceptance remains pending.

## 2026-06-04 - Plain Theme Fallback Verifier

Build: local working tree
Commands:
- `python3 -m unittest tests/test_verify_app_theme.py`
- `python3 tools/verify_app_theme.py`
- `make test-tools`
- `make verify-local`
Steps:
- Added a source verifier for the theme fallback invariant used by manual M7
  testing.
- The verifier checks that `Plain` is first/default, invalid theme values
  sanitize to `Plain`, and the `Plain` background entry has null framebuffer
  paths so it cannot load FE assets.
- Wired the verifier into `make test-tools`.
- Updated the CTR-001 M7 checklist to expect startup in `Plain` and a
  five-theme help-screen cycle.
Observed:
- Focused theme-verifier tests passed.
- The verifier accepted the current `app-3ds/source/main.c`.
- `make verify-local` passed with the new verifier included.
Expected:
- Future changes should fail the local gate if they accidentally remove the
  high-contrast Plain startup path needed for reliable daily-use testing.
Result: pass for automated verifier guard only
Notes:
- Manual M7 interaction acceptance remains pending.

## 2026-06-04 - Plain Theme Default Escape Hatch

Build: local working tree
Commands:
- `make -C app-3ds`
- `make verify-local`
- `make run-emulator-fresh-samples`
Steps:
- Added a `Plain` session theme as the startup default.
- Kept FE themes available in the help-screen `X` cycle after Plain.
- Made Plain skip FE framebuffer loading entirely, preserving the original
  black console background for high-contrast manual M7 testing.
- Updated theme docs to describe the five-theme cycle and default behavior.
Observed:
- The 3DS app rebuilt successfully.
- `make verify-local` passed.
- The fresh-sample Azahar target cleared tracked sample progress/session,
  installed current FE framebuffers, and opened the current `.3dsx`.
Expected:
- Manual study testing can start from the non-FE high-contrast UI, while FE
  backdrops remain available for optional visual checks.
Result: pass for automated build/staging gates; pending user visual check
Notes:
- Manual M7 interaction acceptance remains pending.

## 2026-06-04 - Subdued FE Theme Chrome Pass

Build: local working tree
Commands:
- `python3 -m unittest tests/test_fe_theme_assets.py`
- `make verify-fe-theme-assets`
- `make run-emulator-fresh-samples`
- `make verify-local`
Steps:
- Replaced the full-screen battle-frame overlay with quieter generated
  top/bottom banner bands.
- Kept the FE battle-frame source as a trim-palette input only, instead of
  pasting the whole frame over the app background.
- Darkened only the screen-sized app backdrops more strongly while preserving
  the tracked low-resolution raw background darkening.
- Kept the small pixel-font title mark and constrained the icon-strip legend to
  the bottom-sized screen assets.
Observed:
- Desktop contact sheets show darkened FE backgrounds with subdued trim and no
  large competing battle-frame panels.
- The fresh-sample Azahar target cleared tracked sample progress/session,
  installed the regenerated FE framebuffer assets, and opened the current
  `.3dsx`.
- `make verify-local` passed after the compositor change.
Expected:
- The FE layer should read as a background skin instead of fighting the console
  study UI.
Result: pass for automated generation/build gates; pending user visual check
Notes:
- Manual M7 interaction acceptance remains pending.

## 2026-06-04 - M7 Session Deck Evidence Guard

Build: local working tree
Commands:
- `python3 -m py_compile tools/verify_m7_artifacts.py tests/test_verify_m7_artifacts.py`
- `python3 -m unittest tests/test_verify_m7_artifacts.py`
- `make test-tools`
- `make verify-m7-preflight`
- `make run-emulator-fresh-samples`
Steps:
- Hardened the M7 artifact verifier so root `session.tsv` evidence must end
  with `last_deck_id` inside the set of study or reset decks being checked.
- Added a regression test for stale or mismatched session evidence from an
  unchecked deck.
Observed:
- The focused verifier suite passed with 20 tests.
- `make test-tools` passed, including text-deck, Azahar-control, M7-artifact,
  and FE-theme asset verifier tests.
- `make verify-m7-preflight` passed, including host tests, converter tests,
  FE theme generation, package/local SD verification, fresh Azahar sample
  staging, and Azahar control-profile verification.
- `make run-emulator-fresh-samples` cleared tracked Azahar sample progress and
  root session diagnostics, installed the current FE framebuffer assets, and
  opened the current `.3dsx` in Azahar.
Expected:
- Post-run M7 verification should now fail if deck artifacts and root session
  evidence clearly come from different manual passes.
Result: pass for automated verifier guard only
Notes:
- Manual M7 interaction acceptance remains pending.

## 2026-06-04 - M7 Artifact Log-Skipped Parameters

Build: local working tree
Commands:
- `python3 -m unittest tests/test_verify_m7_artifacts.py`
- `make -n verify-m7-artifacts M7_SDMC=/tmp/sd
  M7_ALLOW_MISSING_REVIEW_LOG=1 M7_NO_REQUIRED_EVENTS=1`
Steps:
- Added an explicit verifier mode for the documented `log skipped` exception.
- Wired the Make target to pass `--allow-missing-review-log` and
  `--no-required-events` only when requested.
Observed:
- The verifier can still check deck state and settings when review logs are
  absent because the app reported diagnostic logging failure.
- The CLI rejects combining `--no-required-events` with explicit
  `--require-event` flags.
Expected:
- Manual M7 acceptance can now distinguish a real missing-log failure from the
  known in-app `log skipped` exception without bypassing all artifact checks.
Result: pass for automated target wiring only
Notes:
- Manual M7 interaction acceptance remains pending.

## 2026-06-04 - M7 Reset Artifact Verification

Build: local working tree
Commands:
- `python3 -m unittest tests/test_verify_m7_artifacts.py`
- `make -n verify-m7-artifacts M7_SDMC=/tmp/sd M7_RESET_DECKS=sample
  M7_REQUIRED_EVENTS="suspend restore"`
- `make -n verify-m7-artifacts M7_SDMC=/tmp/sd
  M7_RESET_DECKS="sample limits-demo" M7_NO_REQUIRED_EVENTS=1`
Steps:
- Added reset-deck artifact checks to the M7 verifier.
- Reset decks must keep valid `cards.tsv` and `settings.tsv` while removing
  `state.tsv`, `state.tsv.tmp`, `state.tsv.bak`, `review-log.tsv`,
  `review-log.tsv.tmp`, and `review-log.tsv.bak`.
Observed:
- The Make target removes reset decks from the normal study-deck list.
- The verifier can check one reset deck beside one studied deck, or reset-only
  decks with `--no-study-decks` and no required events.
Expected:
- Manual M7 acceptance can verify reset-progress cleanup without contradicting
  the normal post-study state/log artifact checks.
Result: pass for automated target wiring only
Notes:
- Manual M7 interaction acceptance remains pending.

## 2026-06-04 - M7 Settings Expectation Guard

Build: local working tree
Commands:
- `python3 -m unittest tests/test_verify_m7_artifacts.py`
- `make -n verify-m7-artifacts M7_SDMC=/tmp/sd M7_DECKS="sample limits-demo"
  M7_EXPECT_SETTINGS="sample:5:20 limits-demo:1:10"`
Steps:
- Hardened the M7 artifact verifier so `--expect-settings` must name a deck
  that is actually being verified as either a study deck or a reset deck.
- Added a guard against empty deck selections.
Observed:
- A typoed expected-settings deck id now fails instead of being silently
  skipped.
Expected:
- Manual M7 commands that claim to verify exact daily-limit edits must now
  prove those settings on a selected deck.
Result: pass for automated verifier guard only
Notes:
- Manual M7 interaction acceptance remains pending.

## 2026-06-04 - M7 Plain Default Fresh Relaunch

Build: local working tree
Commands:
- `make verify-m7-preflight`
- `make run-emulator-fresh-samples`
Steps:
- Ran the full M7 preflight after making the startup theme `Plain` and leaving
  the Fire Emblem framebuffers opt-in from the help-screen theme cycle.
- Relaunched Azahar with fresh `limits-demo` and `sample` decks.
Observed:
- Preflight passed host C tests, converter tests, tool tests, sample deck
  verification, app build/package checks, Azahar sample staging cleanup, and
  Azahar control-profile verification.
- The fresh-sample launcher cleared root session files and tracked sample deck
  state/log artifacts before opening the current `.3dsx`.
Expected:
- Manual testing should start from the plain console-style UI, with FE themes
  available only after cycling the theme.
Result: ready for manual M7 interaction testing
Notes:
- Manual M7 interaction acceptance remains pending.

## 2026-06-04 - FE Layer Viewer Relaunch

Build: local working tree
Commands:
- `python3 -m unittest tests/test_fe_theme_assets.py`
- `make verify-fe-theme-assets`
- `make fe-bg-viewer-3ds`
- `make test-tools`
- `make run-emulator-fe-bg-viewer`
Steps:
- Added generated 3DS framebuffer binaries for the FE layer pipeline:
  background, legend, and font.
- Updated the isolated FE background viewer so Left/Right cycles themes and
  Up/Down cycles layers.
- Installed the viewer and layer assets into Azahar SDMC and opened the viewer.
Observed:
- FE asset tests passed with direct coverage for layer framebuffer output.
- `make verify-fe-theme-assets` regenerated final and layer framebuffers.
- `make fe-bg-viewer-3ds` rebuilt after clearing stale pre-move dependencies.
- Azahar SDMC contains 24 layer framebuffer files.
Expected:
- Manual viewer testing can now inspect each theme as background-only, then
  legend/chrome, then font.
Result: launch only; pending manual layer visual acceptance
Notes:
- Main app FE themes still use the final framebuffer filenames; layer files are
  for the isolated viewer.

## 2026-06-05 - FE Card Layer Viewer Relaunch

Build: local working tree
Commands:
- `python3 -m unittest tests/test_fe_theme_assets.py`
- `make test-tools`
- `make verify-fe-theme-assets`
- `make test-host`
- `make -C app-3ds`
- `make fe-bg-viewer-3ds`
- `make run-emulator-fe-bg-viewer`
Steps:
- Replaced the subdued legend/chrome layer with a generated FE-style
  red/orange flashcard card layer.
- Used Sokaballa's battle-screen art as the card palette source and the local
  FE7-FE8 Checkmate OTF for baked card labels/title when `hb-view` is
  available.
- Let FE themes draw bright review text over the baked card while Plain keeps
  the original protected black-on-white paper card.
- Installed the regenerated viewer/app theme assets into Azahar SDMC and opened
  the layer viewer.
Observed:
- Tool and host tests passed after the card/font changes.
- App and viewer 3DSX builds passed.
- Regenerated framebuffers passed exact-size checks with no all-black outer
  edges.
- Azahar SDMC contains 8 `card` layer framebuffer files, one per theme/screen.
Expected:
- Manual viewer testing can now inspect each theme as background-only, then
  card, then font.
Result: launch only; pending manual card visual acceptance
Notes:
- The baked FE font currently covers fixed labels/title only; full flashcard
  body text still uses console rendering.

## 2026-06-05 - M7 Readiness Docs And Preflight

Build: local working tree
Commands:
- `python3 -m unittest tests/test_verify_app_theme.py tests/test_verify_m7_artifacts.py tests/test_verify_text_deck.py`
- `make test-tools`
- `make verify-m7-preflight`
Steps:
- Updated top-level README current-status wording to match the current
  Plain-first five-theme cycle and FE red/orange card-panel behavior.
- Added the FE background viewer targets to the testing command list and
  documented their Background/Card/Font render-check role.
- Removed stale architecture wording that described only paper-card trim and
  black card text.
- Ran the full non-launching M7 preflight after the docs sync.
Observed:
- Focused verifier tests passed.
- `make test-tools` passed.
- `make verify-m7-preflight` passed, including host C tests, converter tests,
  verifier-tool tests, sample-deck verification, app build/package checks,
  FE theme generation, local SD staging, package SD verification, fresh Azahar
  sample staging, and Azahar control-profile verification.
Expected:
- The next manual emulator or hardware pass can follow current docs without
  conflicting theme/card instructions.
Result: pass for automated preflight only; pending manual M7 interaction acceptance
Notes:
- Azahar fresh samples were staged by the preflight; the emulator was not
  launched by this target.

## 2026-06-05 - M7 Exact Settings Verifier Requirement

Build: local working tree
Commands:
- `python3 -m py_compile tools/verify_m7_artifacts.py tests/test_verify_m7_artifacts.py`
- `python3 -m unittest tests/test_verify_m7_artifacts.py`
- `make test-tools`
- `make verify-m7-preflight`
Steps:
- Tightened the post-run M7 artifact verifier so it fails unless exact
  `--expect-settings deck_id:new_limit:review_limit` values are supplied.
- Added focused coverage for the missing-expectations failure.
- Updated the M7 checkpoint, testing, and device-test docs to call out that
  `M7_EXPECT_SETTINGS` is required for post-run acceptance.
Observed:
- Focused verifier compile and unit tests passed.
- `make test-tools` passed.
- `make verify-m7-preflight` passed and staged fresh tracked samples into
  Azahar SDMC.
Expected:
- Manual M7 artifact checks can no longer pass daily-limit acceptance with only
  a generic `settings_saved_count`; they must prove the exact saved limits.
Result: pass for automated verifier/preflight only; pending manual M7 interaction acceptance
Notes:
- This strengthens the post-run proof gate. It does not replace the manual
  study-session pass.

## 2026-06-05 - M7 Expanded Physical-Key Safety Coverage

Build: local working tree
Commands:
- `cc -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iapp-3ds/include -D__3DS__ -Itests/stubs tests/test_app_controls_3ds_keys.c app-3ds/source/app_controls.c -o /tmp/anki3ds-test-app-controls-3ds-keys`
- `/tmp/anki3ds-test-app-controls-3ds-keys`
- `make test-host`
Steps:
- Extended the direct 3DS-key regression test beyond ratings, exit, and reset.
- Added translated-key coverage for `KEY_SELECT` opening review actions,
  `KEY_L` undo gating, `KEY_R` suspend confirmation opening, action choose and
  cancel keys, daily-limit save and cancel keys, and suspend/restore
  confirmation keys.
- Added mixed-command/navigation/face-button chord checks for those flows so
  noisy physical input maps to `APP_CONTROL_ACTION_NONE`.
Observed:
- The direct 3DS-key test compiled and passed.
- `make test-host` passed, including deck/scheduler, diagnostics, and the
  expanded 3DS-key input suite.
Expected:
- Future key-map changes should fail host tests if actions, settings, undo,
  suspend, or restore start accepting noisy multi-button input.
Result: pass for automated key-safety coverage only; pending manual M7 interaction acceptance
Notes:
- This remains automated coverage of the classifier and translated 3DS key
  path. Manual emulator or hardware testing is still required for the full M7
  study session.

## 2026-06-05 - M7 Post-Key-Coverage Preflight

Build: local working tree
Commands:
- `make verify-m7-preflight`
Steps:
- Ran the full M7 preflight after expanding the translated 3DS physical-key
  safety tests for actions, settings, undo, suspend, and restore.
Observed:
- Host C tests passed, including deck/scheduler, diagnostics, and the expanded
  3DS-key input suite.
- Converter, text-deck verifier, Azahar-control verifier, app-theme verifier,
  M7 artifact verifier, and FE theme asset tests passed.
- Tracked sample decks verified from source, local SD, packaged SD, and Azahar
  SDMC.
- The app package was current, FE theme framebuffers regenerated, and Azahar
  controls matched the documented anki3ds keyboard profile.
Expected:
- Azahar SDMC is freshly staged for the manual M7 interaction pass with
  `limits-demo` and `sample`, tracked sample progress cleared, and root
  `session.tsv` diagnostics cleared.
Result: pass for automated preflight only; pending manual M7 interaction acceptance
Notes:
- This does not prove the full M7 acceptance checklist. The next proof step is
  manual emulator or hardware study, followed by `make verify-m7-artifacts`
  with exact `M7_EXPECT_SETTINGS` and any reset/log-skipped allowances matching
  the pass.

## 2026-06-05 - M7 Fresh Azahar Manual-Pass Launch

Build: local working tree
Commands:
- `make run-emulator-fresh-samples`
- `osascript -e 'application "Azahar" is running'`
- `pgrep -fl Azahar`
- `sed -n '1,120p' "/Users/eric/Library/Application Support/Azahar/sdmc/3ds/anki3ds/session.tsv"`
- `find "/Users/eric/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks" -maxdepth 2 -type f \( -name 'state.tsv' -o -name 'review-log.tsv' -o -name 'settings.tsv' \) | sort`
Steps:
- Staged fresh tracked `limits-demo` and `sample` decks into Azahar SDMC.
- Cleared tracked sample progress files and root session diagnostics before
  launch.
- Regenerated and installed FE theme framebuffers.
- Opened the current `app-3ds/anki3ds.3dsx` in Azahar.
Observed:
- Azahar was running after launch.
- Root `session.tsv` was freshly created with `launch_count=1`,
  `scan_completed=1`, `deck_count=2`, `deck_open_count=0`, and
  `last_event=scan`.
- Only `settings.tsv` files existed under the tested sample deck directories;
  no `state.tsv` or `review-log.tsv` progress files were present yet.
- System Events window inspection was blocked by macOS assistive-access
  permissions, so process state and SDMC diagnostics are the launch evidence.
Expected:
- The emulator is ready for the manual M7 study-session pass from a clean
  sample state.
Result: launch ready for manual M7 interaction acceptance
Notes:
- This is launch evidence only. The full M7 gate still requires completing the
  manual study flow, exiting through confirmation, and running
  `make verify-m7-artifacts` with exact settings expectations and any reset or
  log-skipped allowances from the actual pass.

## 2026-06-05 - FE Viewer Legend Layer Checkpoint

Build: local working tree
Commands:
- `python3 -m unittest tests/test_fe_theme_assets.py`
- `make verify-fe-theme-assets`
- `make -C tools/fe-bg-viewer-3ds`
- `make run-emulator-fe-bg-viewer`
Steps:
- Split the generated FE viewer composition into four layers: background,
  legend, card, and font.
- Added `legend` layer framebuffers for top and bottom screens before any
  Anki-specific integration work.
- Moved the bottom-screen icon strip into the legend layer; the final font
  layer now only adds baked labels/title.
Observed:
- FE asset tests passed.
- FE asset generation produced eight `*_layer_legend_*_bgr888_fb.bin` files.
- Azahar launched the standalone FE background viewer with refreshed layer
  assets installed to SDMC.
Expected:
- In the viewer, first screen is background; pressing Down once shows the
  legend layer, Down again shows card, Down again shows font.
Result: pass for isolated legend layer; user confirmed the legend is visible in Azahar

## 2026-06-05 - M7 Review Screen Evidence Guard

Build: local working tree
Commands:
- `python3 -m py_compile tools/verify_m7_artifacts.py tests/test_verify_m7_artifacts.py`
- `python3 -m unittest tests/test_verify_m7_artifacts.py`
- `make test-tools`
- `make verify-m7-preflight`
Steps:
- Tightened the post-run M7 artifact verifier so rating-required passes must
  have root `session.tsv` review-screen evidence.
- Added focused verifier coverage for `rating` evidence with
  `review_screen_count=0`.
- Updated checkpoint/device-test docs to make review-screen diagnostics part
  of the expected root session proof.
Observed:
- Focused verifier compile and unit tests passed.
- `make test-tools` passed, including the expanded M7 artifact suite.
- `make verify-m7-preflight` passed and staged fresh tracked samples into
  Azahar SDMC.
Expected:
- Manual M7 artifact checks can no longer pass a rating-required session whose
  root diagnostics never prove that the app entered a review screen.
Result: pass for automated verifier/preflight only; pending manual M7 interaction acceptance
Notes:
- This strengthens post-run workflow proof. It does not replace the manual
  study-session pass.

## 2026-06-05 - M7 Session And Review-Log Action Consistency Guard

Build: local working tree
Commands:
- `python3 -m py_compile tools/verify_m7_artifacts.py tests/test_verify_m7_artifacts.py`
- `python3 -m unittest tests/test_verify_m7_artifacts.py`
- `make test-tools`
- `make verify-m7-preflight`
Steps:
- Tightened the post-run M7 artifact verifier so positive saved-action session
  counters must have matching review-log events in non-reset, non-log-skipped
  study passes.
- Preserved the explicit `log skipped` allowance and reset-deck exception,
  because those flows can intentionally leave saved-action counters without
  retained review-log rows.
- Added focused verifier coverage for `rating_saved_count` without a matching
  review-log `rating` event.
Observed:
- Focused verifier compile and unit tests passed.
- `make test-tools` passed, including the expanded M7 artifact suite and the
  existing reset/log-skipped acceptance cases.
- `make verify-m7-preflight` passed and staged fresh tracked samples into
  Azahar SDMC.
Expected:
- Manual M7 artifact checks can no longer pass a normal study run where root
  session diagnostics claim saved actions that are absent from retained
  review logs.
Result: pass for automated verifier/preflight only; pending manual M7 interaction acceptance
Notes:
- This strengthens post-run workflow proof. It does not replace the manual
  study-session pass.

## 2026-06-05 - M7 Session Chronology Guard

Build: local working tree
Commands:
- `python3 -m py_compile tools/verify_m7_artifacts.py tests/test_verify_m7_artifacts.py`
- `python3 -m unittest tests/test_verify_m7_artifacts.py`
- `make test-tools`
- `make verify-m7-preflight`
Steps:
- Tightened the post-run M7 artifact verifier so root `session.tsv` rejects
  backward session chronology: `updated_at < started_at` or
  `current_day < started_day`.
- Added focused verifier coverage for both backward timestamp and backward day
  diagnostics.
- Updated checkpoint/device-test docs to require monotonic session time/day
  fields in root session diagnostics.
Observed:
- Focused verifier compile and unit tests passed.
- `make test-tools` passed, including the expanded M7 artifact suite.
- `make verify-m7-preflight` passed and staged fresh tracked samples into
  Azahar SDMC.
Expected:
- Manual M7 artifact checks can no longer pass root session diagnostics whose
  time/day fields contradict a forward-moving daily-use pass.
Result: pass for automated verifier/preflight only; pending manual M7 interaction acceptance
Notes:
- This strengthens post-run workflow proof. It does not replace the manual
  study-session pass.

## 2026-06-05 - M7 Deck-Open Counter Consistency Guard

Build: local working tree
Commands:
- `python3 -m py_compile tools/verify_m7_artifacts.py tests/test_verify_m7_artifacts.py`
- `python3 -m unittest tests/test_verify_m7_artifacts.py`
- `make test-tools`
- `make verify-m7-preflight`
Steps:
- Tightened the post-run M7 artifact verifier so root `session.tsv` rejects
  impossible deck-open outcome counters.
- Added focused verifier coverage for a session where review/summary/load-error
  outcome counters exceed `deck_open_count`.
- Updated checkpoint/device-test docs to require internally consistent
  deck-open outcome counters in root session diagnostics.
Observed:
- Focused verifier compile and unit tests passed.
- `make test-tools` passed, including the expanded M7 artifact suite.
- `make verify-m7-preflight` passed and staged fresh tracked samples into
  Azahar SDMC.
Expected:
- Manual M7 artifact checks can no longer pass a root session whose deck-open
  outcome counters could not have been produced by the app's one-outcome-per-open
  diagnostics path.
Result: pass for automated verifier/preflight only; pending manual M7 interaction acceptance
Notes:
- This strengthens post-run workflow proof. It does not replace the manual
  study-session pass.

## 2026-06-05 - M7 PTMU Init Retry Policy

Build: local working tree
Commands:
- `make test-host`
- `make test`
- `make verify-m7-preflight`
- `make test-tools`
- `make -C app-3ds`
- `make verify-m7-preflight`
Steps:
- Changed battery-poll scheduling so `APP_POWER_BATTERY_SAMPLE_UNAVAILABLE`
  uses the short retry interval, matching transient PTMU read failures.
- Added host coverage proving unavailable PTMU service retries on the short
  interval and retries as soon as the system clock becomes available.
- Updated architecture docs so startup PTMU initialization failures no longer
  describe the old ten-minute retry cadence.
Observed:
- `make test-host` passed, including the updated battery retry policy.
- `make test-tools` passed.
- `make -C app-3ds` rebuilt `anki3ds.3dsx`.
- `make verify-m7-preflight` passed and staged fresh tracked samples into
  Azahar SDMC.
Expected:
- A transient PTMU initialization failure should leave the UI battery line as
  unavailable briefly, then retry after the short battery retry interval instead
  of waiting for the full ten-minute poll.
Result: pass for automated power/preflight coverage only; pending manual M7 interaction acceptance
Notes:
- Battery sampling remains coarse and scheduled; this does not add battery
  service calls to per-button input handling.

## 2026-06-05 - M7 3DS Key Chord Safety Coverage

Build: local working tree
Commands:
- `cc -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iapp-3ds/include -D__3DS__ -Itests/stubs tests/test_app_controls_3ds_keys.c app-3ds/source/app_controls.c -o /tmp/anki3ds-test-app-controls-3ds-keys`
- `/tmp/anki3ds-test-app-controls-3ds-keys`
- `make test-host`
- `make test-tools`
- `make verify-m7-preflight`
Steps:
- Added 3DS-key regression coverage for `KEY_START` opening the exit
  confirmation only when pressed alone.
- Added 3DS-key regression coverage for exit and reset confirmation chords, so
  `KEY_A` or `KEY_X` combined with cancel/navigation/START inputs does not
  confirm a modal action.
Observed:
- The direct 3DS-key test compiled and passed.
- `make test-host` passed, including deck/scheduler, diagnostics, and 3DS-key
  input tests.
- `make test-tools` passed.
- `make verify-m7-preflight` passed and staged fresh tracked samples into
  Azahar SDMC.
Expected:
- Future key-map changes should fail host tests if START, reset, or exit
  confirmation starts accepting noisy multi-button input.
Result: pass for automated key/preflight coverage only; pending manual M7 interaction acceptance
Notes:
- Runtime input behavior was already chord-gated; this pass added
  physical-key regression coverage for the M7 safety path.

## 2026-06-05 - M7 Confirmed Exit Terminal Event Guard

Build: local working tree
Commands:
- `python3 -m py_compile tools/verify_m7_artifacts.py tests/test_verify_m7_artifacts.py`
- `python3 -m unittest tests/test_verify_m7_artifacts.py`
- `make test-tools`
- `make verify-m7-preflight`
Steps:
- Tightened the post-run M7 artifact verifier so root `session.tsv` must end
  with `last_event=exit_confirmed`, not merely contain a historical
  `exit_confirmed=1` flag.
- Added focused verifier coverage for sessions that save daily-use evidence
  but are last updated by another event such as `rating_saved`.
- Updated the M7 checklist docs to tell manual testers to use the confirm-exit
  flow after the final checked action.
Observed:
- Focused verifier compile and unit tests passed.
- `make test-tools` passed, including text-deck, Azahar-control, app-theme,
  M7-artifact, and FE-theme asset verifier suites.
- `make verify-m7-preflight` passed and staged fresh tracked samples into
  Azahar SDMC.
Expected:
- Manual M7 artifact checks now fail if a run has save evidence but does not
  finish through the confirmed-exit path.
Result: pass for automated verifier/preflight only; pending manual M7 interaction acceptance
Notes:
- This strengthens the post-run proof gate. It does not replace the manual
  study-session pass.

## 2026-06-05 - Plain Startup Restored After FE Color Regression

Build: local working tree
Commands:
- `make test-tools`
- `make -C app-3ds`
- `make run-emulator-fresh-samples`
- `make verify-m7-preflight`
Steps:
- Restored startup to the `Plain` theme so the app no longer boots directly
  into the experimental FE framebuffer compositor when FE assets are installed.
- Kept FE themes available through the Help-screen `X` cycle.
- Updated the app-theme verifier to reject future FE-first startup behavior.
- Relaunched Azahar with fresh tracked sample decks and a cleared root
  `session.tsv`.
Observed:
- Azahar relaunched and wrote a fresh session with `launch_count=1`,
  `scan_completed=1`, `deck_count=2`, `deck_open_count=0`, and
  `last_event=scan`.
- The fresh SDMC sample decks contained only `settings.tsv`, with no
  `state.tsv` or `review-log.tsv` yet.
- Manual visual check from the emulator confirmed the deck selector had no FE
  colors after relaunch.
Expected:
- Daily-use startup remains the stable black-console Plain UI.
- FE backgrounds/card/legend/font work should stay opt-in or isolated until the
  color/composition path is fixed.
Result: pass for Plain startup regression coverage; pending manual M7 interaction acceptance
Notes:
- This intentionally pauses automatic FE-theme startup. It does not remove the
  FE asset pipeline or Help-screen theme cycle.

## 2026-06-06 - Opaque Citro2D Renderer Boundary Preflight

Build: local working tree
Commands:
- `python3 -m unittest tests/test_verify_app_theme.py`
- `python3 tools/verify_app_theme.py`
- `make test`
- `make -C app-3ds`
- `git diff --check`
- `make verify-m7-preflight`
Steps:
- Moved the active Citro2D FE renderer into `app_renderer_c2d.{h,c}` behind an
  opaque stack-allocatable public handle.
- Kept `main.c` on app state/input/persistence/screen-model construction and
  removed direct Citro2D types, calls, and renderer geometry constants from it.
- Hardened `tools/verify_app_theme.py` so the renderer can own Citro2D while
  rejecting direct Citro2D in `main.c`, Citro2D leakage from the renderer
  public header, renderer/backend reach-through, and missing display/status
  text draws.
Observed:
- The app-theme verifier suite passed with 46 tests.
- The direct app build passed.
- `make verify-m7-preflight` passed and staged fresh tracked
  `limits-demo`/`sample` decks, the current app, and FE assets into local,
  package, and Azahar SDMC roots.
Expected:
- Fresh sessions should render through the clean FE shell path:
  `main.c -> app_screen_model_build() -> app_renderer_c2d_draw()`.
Result: pass for automated renderer-boundary/preflight coverage only; pending manual M7 interaction acceptance
Notes:
- This did not launch Azahar or verify live readability/layout in the emulator.

## 2026-06-06 - M7 Smoke Blocked By Accessibility; Exit Confirm Aligned

Build: local working tree
Commands:
- `make run-emulator-m7-smoke`
- `make test-host`
- `make -C app-3ds`
- `git diff --check`
- `make verify-m7-preflight`
Steps:
- Tried the deterministic Azahar M7 smoke driver after the renderer-boundary
  preflight.
- The driver stopped at its macOS Automation/Accessibility precheck before
  launching Azahar because `osascript` could not send keystrokes.
- Audited the smoke key sequence against current confirmation handling and
  found the driver/checkpoints use `A` to confirm exit while the clean-shell
  reducer and prompt still required `X`.
- Changed exit confirmation to accept `A`, kept `X` as the confirm key for
  suspend/restore/reset, and updated focused reducer/prompt tests plus control
  docs.
Observed:
- `make run-emulator-m7-smoke` failed before app launch with:
  `macOS denied osascript keystrokes`.
- `make test-host` passed after the exit-confirm correction.
- The direct app build passed.
- `make verify-m7-preflight` passed and restaged fresh tracked
  `limits-demo`/`sample` decks, the current app, and FE assets into local,
  package, and Azahar SDMC roots.
Expected:
- Once Accessibility permission is granted to the terminal/Codex host, the
  smoke driver should be able to press `A` on exit confirmation and continue to
  the artifact verifier instead of hanging on the exit prompt.
Result: pass for automated build/preflight coverage; emulator smoke blocked by local Accessibility permission before launch
Notes:
- This was not a manual M7 acceptance pass.

## 2026-06-06 - Warning Context Status Preflight

Build: local working tree
Commands:
- `make test-host`
- `make -C app-3ds`
- `make test`
- `python3 tools/verify_app_theme.py`
- `git diff --check`
- `make verify-m7-preflight`
Steps:
- Added `app_status_text_copy_with_warning_context()` for preserving warning
  status segments such as `Settings ignored`, daily-limit exhaustion, and
  `; batt low` across successful workflow feedback.
- Wired the helper into the clean-shell app paths for review reveal/rating/undo,
  suspend/restore/reset, daily-limit open/update/cancel/save, confirmation
  cancel, and daily-limit-blocked feedback.
- Kept save/session failure statuses as priority messages instead of appending
  older warning context over them.
Observed:
- Focused host coverage for status text and workflow reducers passed.
- `make test` passed.
- The app-theme verifier passed.
- `make verify-m7-preflight` passed and staged fresh tracked
  `limits-demo`/`sample` decks, the current app, and FE assets into local,
  package, and Azahar SDMC roots.
Expected:
- Manual/emulator review flows should keep active warning context visible after
  benign feedback, while still surfacing save or session write failures plainly.
Result: pass for automated status-context/preflight coverage only; pending manual M7 interaction acceptance
Notes:
- This did not launch Azahar because the smoke driver remains blocked by local
  Accessibility permission until the terminal/Codex host can send keystrokes.

## 2026-06-06 - Parallel Renderer Readability And Contract Preflight

Build: local working tree
Commands:
- `python3 -m py_compile tools/verify_app_theme.py`
- `python3 tools/verify_app_theme.py`
- `make -C app-3ds`
- `git diff --check`
- `make test`
- `make verify-m7-preflight`
Steps:
- Dispatched two parallel agents with disjoint scopes: one audited the
  renderer/backend contract and verifier coverage, and one worked only on FE
  renderer readability.
- Kept backend/deck/learning code out of the renderer pass.
- Added a thin Citro2D inset trim over the parchment, darker/thicker ink text,
  larger text scales, and tighter 31x4 review body wrapping.
- Expanded the app-theme verifier coverage to guard public-header Citro2D
  leaks, non-renderer draw APIs, backend display-contract/geometry coupling,
  and renderer backend forward declarations.
Observed:
- `python3 tools/verify_app_theme.py` passed in the main workspace.
- `make test` passed, including 50 app-theme verifier tests.
- `make verify-m7-preflight` passed and staged fresh tracked
  `limits-demo`/`sample` decks, the current app, and FE assets into local,
  package, and Azahar SDMC roots.
Expected:
- The active FE shell should now be more readable in Azahar while preserving
  the clean contract: renderer consumes strings/screen model only; backend does
  not draw.
Result: pass for automated renderer-readability/contract/preflight coverage only; pending manual M7 interaction acceptance
Notes:
- Azahar was not launched here. The deterministic smoke run still requires
  macOS Accessibility permission for the terminal/Codex host.

## 2026-06-06 - Fresh Samples Launched For Manual FE Readability Check

Build: local working tree after renderer readability and contract-verifier pass
Commands:
- `make run-emulator-fresh-samples`
- `osascript -e 'application "Azahar" is running'`
Steps:
- Staged fresh tracked `limits-demo` and `sample` decks into Azahar SDMC.
- Reinstalled current FE theme assets.
- Launched Azahar with `app-3ds/anki3ds.3dsx`.
Observed:
- `make run-emulator-fresh-samples` completed with exit code 0.
- Azahar was running after launch.
Expected:
- The emulator is ready for manual inspection of the current Forest FE
  background, parchment inset trim, and thicker readable Citro2D text.
Result: launched for manual visual acceptance; automated preflight already passed
Notes:
- This is not a completed M7 smoke or artifact acceptance pass.

## 2026-06-06 - Deck Selector Geometry Contract Cleanup

Build: local working tree
Commands:
- `make test-host`
- `python3 -m unittest tests/test_verify_app_theme.py`
- `python3 tools/verify_app_theme.py`
- `make -C app-3ds`
- `git diff --check`
- `make test`
- `make verify-m7-preflight`
Steps:
- Removed `APP_DECK_SELECT_VISIBLE_ROWS` from the public deck-selector display
  contract header.
- Kept deck-selector list windowing private to `app_deck_select_contract.c`.
- Kept deck-selector paging private to `app_deck_select_action.c`, so `main.c`
  no longer passes a view row count into the action reducer.
- Added an app-theme verifier regression that rejects public non-renderer
  `APP_*VISIBLE_ROWS`-style display geometry constants.
Observed:
- Focused host tests and the direct app build passed.
- The app-theme verifier suite passed with 51 tests.
- `make verify-m7-preflight` passed and staged fresh tracked
  `limits-demo`/`sample` decks, the current app, and FE assets into local,
  package, and Azahar SDMC roots.
Expected:
- The display/backend boundary is tighter: public app display contracts expose
  strings/state, not renderer/list geometry.
Result: pass for automated contract/preflight coverage only; pending manual M7 interaction acceptance
Notes:
- This did not perform a new manual emulator acceptance pass.

## 2026-06-06 - Suspend Restore No-Op Feedback Preflight

Build: local working tree
Commands:
- `make test-host`
- `python3 tools/verify_app_theme.py`
- `make -C app-3ds`
- `git diff --check`
- `python3 -m unittest tests/test_verify_app_theme.py`
- `make test`
- `make verify-m7-preflight`
Steps:
- Added `app_review_action_suspend_restore_target()` so the direct `R`
  suspend/restore command is classified in a host-tested app-shell helper.
- Kept active-card `R` opening suspend confirmation and completed-with-
  suspended `R` opening restore confirmation.
- Added visible no-op feedback for completed decks with no suspended cards:
  `Nothing suspended`, preserving any active warning context through the
  existing status helper.
- Updated current controls and M7 checklist docs away from the stale actions-
  screen wording for suspend/restore and daily-limit edits.
Observed:
- Focused host tests passed.
- The direct app build passed.
- The app-theme verifier and full test suite passed.
- `make verify-m7-preflight` passed and staged fresh tracked
  `limits-demo`/`sample` decks, the current app, and FE assets into local,
  package, and Azahar SDMC roots.
Expected:
- During manual M7, pressing `R` on a completed deck with no suspended cards
  should visibly report `Nothing suspended` instead of silently doing nothing.
Result: pass for automated workflow/preflight coverage only; pending manual M7 interaction acceptance
Notes:
- This did not launch or drive Azahar.

## 2026-06-06 - Undo No-Op Feedback Preflight

Build: local working tree
Commands:
- `make test-host`
- `python3 tools/verify_app_theme.py`
- `make -C app-3ds`
- `git diff --check`
- `python3 -m unittest tests/test_verify_app_theme.py`
- `make test`
- `make verify-m7-preflight`
Steps:
- Added `app_review_action_undo_target()` so the direct `L` undo command has a
  host-tested app-shell target classifier.
- Kept real undo behavior unchanged when `undo_available` is true.
- Added visible no-op feedback when a deck has no undo history:
  `Nothing to undo`, preserving any active warning context through the existing
  status helper.
- Documented the no-op `L` behavior in the control map.
Observed:
- Focused host tests passed.
- The direct app build passed.
- The app-theme verifier and full test suite passed.
- `make verify-m7-preflight` passed and staged fresh tracked
  `limits-demo`/`sample` decks, the current app, and FE assets into local,
  package, and Azahar SDMC roots.
Expected:
- During manual M7, pressing `L` when no undo is available should visibly
  report `Nothing to undo` instead of silently doing nothing.
Result: pass for automated workflow/preflight coverage only; pending manual M7 interaction acceptance
Notes:
- This did not launch or drive Azahar.

## 2026-06-06 - Held Input Safety And Repeat Preflight

Build: local working tree
Commands:
- `make test-host`
- `python3 tools/verify_app_theme.py`
- `python3 -m unittest tests/test_verify_app_theme.py`
- `make -C app-3ds`
- `git diff --check`
- `make test`
- `make verify-m7-preflight`
Steps:
- Split clean-shell input handling into key-down and key-held logical button
  snapshots in `main.c`.
- Added chord-safe held context so a command pressed while another button is
  held, such as `A` while holding Down, reaches reducers as a mixed chord and
  is ignored.
- Added pure frame-counted navigation repeat in `study_controls`: deck select
  repeats Up/Down/Left/Right, review repeats Up/Down scroll, settings repeats
  Left/Right value changes, and confirmation screens do not repeat.
- Kept held navigation on one-VBlank waits so repeat input does not sit behind
  the adaptive idle backoff.
Observed:
- Focused host controls and full host/tool/converter tests passed.
- The app-theme verifier suite passed with 51 tests.
- The direct app build passed.
- `make verify-m7-preflight` passed and staged fresh tracked
  `limits-demo`/`sample` decks, the current app, and FE assets into local,
  package, and Azahar SDMC roots.
Expected:
- During manual M7, holding a pure navigation direction should repeat only on
  the documented axes, while command/navigation/face-button chords should
  remain inert even when the command is pressed after the navigation hold.
Result: pass for automated workflow/preflight coverage only; pending manual M7 interaction acceptance
Notes:
- This did not launch or drive Azahar.

## 2026-06-06 - Fresh Samples Launched After Manual-Checklist Cleanup

Build: local working tree
Commands:
- `git diff --check`
- `python3 tools/verify_app_theme.py`
- `make verify-m7-preflight`
- `make run-emulator-fresh-samples`
- `osascript -e 'application "Azahar" is running'`
- `pgrep -fl Azahar`
Steps:
- Updated README, checkpoint, emulator checklist, hardware checklist, and
  handoff wording away from the removed actions/help/theme-cycle surfaces.
- Restaged fresh tracked `limits-demo` and `sample` decks into Azahar SDMC.
- Reinstalled current FE theme assets.
- Launched Azahar with the current `app-3ds/anki3ds.3dsx`.
Observed:
- `git diff --check` passed.
- `python3 tools/verify_app_theme.py` passed.
- `make verify-m7-preflight` passed and restaged local/package/Azahar payloads.
- `make run-emulator-fresh-samples` completed with exit code 0.
- `osascript` reported Azahar is running, and `pgrep` showed an Azahar process.
- Immediately after launch, root `session.tsv` was not yet present in Azahar
  SDMC, so this proves staging plus emulator launch only, not a completed app
  scan or manual M7 acceptance pass.
Expected:
- The emulator window is ready for manual FE readability and M7 daily-use
  inspection with current direct controls.
Result: launched for manual visual/workflow acceptance; automated preflight passed
Notes:
- Manual interaction and post-run `make verify-m7-artifacts` remain pending.

## 2026-06-06 - Fresh Samples First-Scan Session Save Fixed

Build: local working tree
Commands:
- `make test-host`
- `python3 tools/verify_app_theme.py`
- `git diff --check`
- `make -C app-3ds`
- `make run-emulator-fresh-samples`
- `sed -n '1,120p' "/Users/eric/Library/Application Support/Azahar/sdmc/3ds/anki3ds/session.tsv"`
- `tail -n 35 "/Users/eric/Library/Application Support/Azahar/log/azahar_log.txt"`
Steps:
- Removed the implicit review title/header draw from the FE top-screen card
  surface and moved review body text up into the freed parchment area.
- Kept the expanded parchment panels but pulled the visible inner readability
  frame safely inside the parchment edge.
- Hardened first-save persistence for `session.tsv`, deck `state.tsv`,
  `settings.tsv`, and `review-log.tsv` so missing primary files are not renamed
  to `.bak` on a clean first write.
- Skipped missing-backup cleanup before `remove()` to avoid noisy Azahar SDMC
  log lines on clean launches.
- Restaged fresh tracked `limits-demo` and `sample` decks, FE assets, and the
  rebuilt 3DSX into Azahar SDMC, then launched Azahar.
Observed:
- `make test-host` passed after the persistence changes.
- `python3 tools/verify_app_theme.py` passed after the renderer spacing/header
  update.
- `git diff --check` passed.
- `make -C app-3ds` rebuilt `anki3ds.3dsx`.
- Fresh Azahar launch wrote root `session.tsv` immediately after scan:
  `launch_count=1`, `scan_completed=1`, `deck_count=2`, `ignored_count=0`,
  `deck_open_count=0`, `last_deck_id=-`, and `last_event=scan`.
- The fresh Azahar log tail no longer included the previous missing
  `session.tsv.bak` cleanup error.
Expected:
- Manual FE readability and M7 daily-use testing can now start from a clean
  sample launch with root session evidence already persisted.
Result: pass for first-scan persistence and emulator launch evidence; pending manual M7 interaction acceptance
Notes:
- Manual interaction and post-run `make verify-m7-artifacts` remain pending.

## 2026-06-06 - Legend Toggle And Font Contract Docs

Build: local working tree
Commands:
- `git diff --check`
- `python3 tools/verify_app_theme.py`
- `python3 -m unittest tests/test_verify_app_theme.py`
- `make test-host`
- `make -C app-3ds`
- `make test`
Steps:
- Documented the current `B` behavior: compact/detailed help toggle on deck
  select and unrevealed/no-active review screens, Easy rating after reveal,
  and cancel on settings/confirmation screens.
- Documented that all live app text is currently drawn by the Citro2D renderer
  at one shared scale, using the shared/system font path.
- Clarified that Hack Nerd Font Mono, Checkmate, Emporio, and the FE8 glyph
  map are available font sources but require a renderer-side bitmap/sprite
  font path before they can replace live Citro2D text in the 3DS app.
- Updated testing docs for compact/expanded legend contract coverage and the
  current renderer/backend split invariant.
Observed:
- `git diff --check` passed.
- `python3 tools/verify_app_theme.py` passed.
- `python3 -m unittest tests/test_verify_app_theme.py` ran 51 tests and passed.
- `make test-host` passed all host C tests, including flashcard/deck selector
  contracts, screen-model handoff, key mapping, battery status, session
  persistence, and clean-shell daily-use coverage.
- `make -C app-3ds` reported the current `anki3ds.3dsx` target up to date.
- `make test` passed host C, converter, text-deck verifier, Azahar controls,
  app-theme verifier, M7 artifact verifier, smoke-driver, and FE-theme asset
  tests.
Expected:
- The current checkpoint should have one live text style and a documented,
  test-covered legend toggle without reintroducing backend drawing.
Result: pass for automated contract/build coverage; pending manual Azahar
readability and M7 interaction acceptance
Notes:
- This did not relaunch Azahar or reset the existing emulator SDMC state.

## 2026-06-06 - Direct-Control M7 Smoke Driver Updated

Build: local working tree
Commands:
- `python3 -m unittest tests/test_drive_azahar_m7_smoke.py`
- `make drive-azahar-m7-smoke-dry-run`
- `python3 tools/verify_text_deck.py sample-decks/limits-demo`
- `make run-emulator-m7-smoke`
- `make test-tools`
- `make verify-sample-decks`
- `git diff --check`
- `make verify-m7-preflight`
- `osascript -e 'tell application "Azahar" to quit'`
- `kill 67569`
- `kill 60875`
- `kill -9 60875`
- `make run-emulator-fresh-samples`
- `sed -n '1,120p' "/Users/eric/Library/Application Support/Azahar/sdmc/3ds/anki3ds/session.tsv"`
Steps:
- Updated the deterministic M7 smoke driver away from the removed actions
  screen. It now uses direct controls: `X` opens/saves daily limits, `A`
  reveals and rates Good, `L` undoes, `R` then `X` suspends each active card,
  `R` then `X` restores from the no-active suspended summary, and `START` then
  `A` confirms exit.
- Reopened `limits-demo` after the relaunch before final exit so
  `session.tsv` can end with `last_event=exit_confirmed` and
  `last_deck_id=limits-demo`, matching the M7 artifact verifier contract.
- Added regression coverage that the smoke driver no longer uses `SELECT` for
  deleted deck actions and saves daily limits with `X`.
- Corrected the `limits-demo` sample card that still taught `SELECT` as
  opening deck actions; it now describes returning to the deck list.
- Tried the full smoke target after the driver fix.
- Restaged a fresh Azahar sample launch after the smoke automation precheck
  failed, so manual testing can start from the current build and current sample
  decks.
Observed:
- Focused smoke-driver tests passed.
- The dry run now prints a 32-step direct-control sequence ending with reopen
  of `limits-demo`, final exit confirmation, and
  `make verify-azahar-m7-smoke-artifacts`.
- `sample-decks/limits-demo` and both tracked sample decks verified.
- `make run-emulator-m7-smoke` stopped before launch/input at the macOS
  Accessibility precheck:
  `macOS denied osascript keystrokes`.
- `make test-tools`, `git diff --check`, and `make verify-m7-preflight`
  passed after the driver/sample update.
- A graceful AppleScript quit of the already-running Azahar process hung; the
  helper `osascript` was killed, then the old Azahar process was terminated
  and finally SIGKILLed before relaunch.
- `make run-emulator-fresh-samples` completed and launched Azahar process
  `68581`.
- Fresh root `session.tsv` after relaunch showed `launch_count=1`,
  `scan_completed=1`, `deck_count=2`, `ignored_count=0`,
  `deck_open_count=0`, `exit_confirmed=0`, `last_deck_id=-`, and
  `last_event=scan`.
Expected:
- Once macOS Accessibility permission is available to the terminal/Codex host,
  `make run-emulator-m7-smoke` should drive the current direct-control app
  instead of following the deleted actions UI.
Result: pass for updated driver/preflight/fresh-launch evidence; automated
smoke execution remains blocked by local macOS Accessibility permission before
input
Notes:
- Visual screenshot capture still needs manual help or working window
  inspection permissions; activating Azahar showed the menu bar active, but
  screen capture did not include a visible emulator window in this run.
- After continuing from the interrupted turn, `pgrep -fl Azahar` and
  `pgrep -fl osascript` returned no processes. The fresh `session.tsv` scan
  evidence remains on disk, but no emulator window should be assumed open.

## 2026-06-06 - Review UI Layout Fresh Azahar Launch

Build: local working tree
Commands:
- `make run-emulator-fresh-samples`
- `sed -n '1,80p' "$AZAHAR_SDMC/3ds/anki3ds/renderer.tsv"`
- `sed -n '1,120p' "$AZAHAR_SDMC/3ds/anki3ds/session.tsv"`
Steps:
- Restaged fresh sample decks, rebuilt/installed current FE assets as needed,
  and launched the rebuilt `app-3ds/anki3ds.3dsx` in Azahar.
- Checked the runtime renderer fingerprint and root session diagnostics from
  Azahar SDMC.
Observed:
- Azahar process was running after launch.
- `renderer.tsv` was written at 2026-06-06 23:03 EDT.
- Runtime renderer fingerprint showed `renderer=citro2d`,
  `assets_loaded=1`, `review_front_window=40 7`,
  `review_answer_window=31 6`, and
  `review_bottom_answer x=34 y=72 scale=0.50 wrap=0`.
- Fresh root `session.tsv` showed `launch_count=1`, `scan_completed=1`,
  `deck_count=2`, `ignored_count=0`, `deck_open_count=0`, and
  `last_event=scan`.
Expected:
- The running Azahar build is the current Citro2D renderer with the new
  top-front/bottom-answer review layout and hidden default deck controls.
Result: pass for rebuilt-runtime fingerprint evidence; manual visual
acceptance of the new review layout remains pending
Notes:
- This does not prove final readability or physical 3DS behavior. It proves
  Azahar executed the rebuilt renderer and not a stale binary.

## 2026-06-06 - Direct-Control Smoke Artifact Shape Test

Build: local working tree
Commands:
- `python3 -m unittest tests/test_verify_m7_artifacts.py`
- `python3 -m unittest tests/test_drive_azahar_m7_smoke.py`
- `git diff --check`
Steps:
- Added a verifier-side compact artifact test for the updated direct-control
  `limits-demo` smoke shape.
- The synthetic artifact set uses the expected saved settings
  `limits-demo:5:10`, required `rating`, `undo`, `suspend`, and `restore`
  events, relaunch evidence, and final confirmed-exit evidence with
  `last_deck_id=limits-demo`.
Observed:
- `tests/test_verify_m7_artifacts.py` now runs 36 tests and passed.
- The smoke-driver planning tests still passed.
- Whitespace check passed.
Expected:
- While macOS Accessibility blocks actual Azahar key driving, the post-run
  artifact verifier still has a regression fixture for the exact direct-control
  smoke evidence shape it should accept after a successful run.
Result: pass for host artifact-verifier coverage; automated Azahar smoke input
remains blocked by local Accessibility permission
Notes:
- This is not a substitute for manual/emulator interaction evidence; it only
  tightens the artifact proof gate around the planned direct-control flow.

## 2026-06-06 - Direct-Control Smoke Real-Module Host Test

Build: local working tree
Commands:
- `make test-host`
Steps:
- Added `tests/test_m7_direct_smoke_flow.c` to the host C suite.
- The test builds a temporary SDMC-shaped `limits-demo` deck, then drives the
  planned direct-control smoke flow through real study modules:
  save settings as `5` new and `10` review, reveal/rate Good, undo, reveal
  again, suspend all six cards, restore them, save state/session/log artifacts,
  relaunch from saved session/state, reopen `limits-demo`, and confirm exit.
Observed:
- `make test-host` passed with the new direct smoke integration executable.
- `make test` passed, including host C, converter, app-theme, M7 artifact,
  smoke-driver, and FE-theme asset tests.
- `make verify-m7-preflight` passed and restaged fresh local/package/Azahar
  sample decks, current app artifacts, FE assets, and Azahar controls.
- The resulting assertions cover launch count, deck-open/review counters,
  answer/rating/undo/suspend/restore/settings counters, final
  `exit_confirmed`, final `last_deck_id=limits-demo`, saved settings,
  compact state presence, and nine review-log rows.
Expected:
- The planned M7 smoke evidence is now covered at two host levels: a real C
  module flow that writes artifacts, and a verifier-side compact artifact shape
  accepted by `tools/verify_m7_artifacts.py`.
Result: pass for host module integration coverage; automated Azahar smoke input
remains blocked by local Accessibility permission
Notes:
- This still does not prove real emulator rendering/input; it narrows the gap
  to the platform interaction layer and manual visual acceptance.
