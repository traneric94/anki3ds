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
make verify-m7-preflight
make verify-local-sd
make verify-azahar-fresh-samples
make package-sd
make verify-package-sd
make install-local-sd
make install-local-sample-deck
make install-local-sample-decks
make reset-local-sample-progress
make prepare-local-samples-fresh
make install-azahar-app
make install-azahar-sample-deck
make install-azahar-sample-decks
make prepare-azahar-daily-use
make reset-azahar-sample-progress
make prepare-azahar-samples-fresh
make run-emulator
make run-emulator-samples
make run-emulator-fresh-samples
make run-emulator-daily-use
make fe-bg-viewer-3ds
make fix-azahar-controls
make install-azahar-fe-bg-viewer
make run-emulator-fe-bg-viewer
```

Verification gates:

- `make verify-ci` is the portable CI gate. It runs `make test`, including
  host C tests, converter tests, and verifier-tool tests, then runs
  `make verify-sample-decks`.
- `make test-host` includes `tests/test_app_battery_status.c`, which covers
  the battery status transition policy used by `main.c`: first low-sample
  announcement, repeated still-low samples preserving the warning, clearing
  after charging/recovery/unavailable evidence, closed-shell samples staying
  quiet, and unrelated status text staying intact on normal samples.
- `make test-host` includes `tests/test_app_battery_monitor.c`, which covers
  battery monitor state and PTMU sampling behavior through the host 3DS stub:
  unavailable startup, open-sample change detection, unchanged samples,
  closed-shell samples, read failures preserving the last good state,
  low-battery status/save-warning policy, and one-shot PTMU shutdown.
- `make test-host` includes `tests/test_app_flashcard_contract.c`, which covers
  the review-card string contract, including owned raw front/back/display,
  word-aware wrapped text windows, revealed front/top and back/bottom string
  separation, status, compact and expanded help legends, progress, and footer
  strings from `app_flashcard_text_view_build()`.
- `make test-host` includes `tests/test_app_screen_model.c`, which covers the
  aggregate renderer-facing screen model used by `app_shell`: review flashcard
  strings, deck-selector strings, settings draft strings, confirmation strings,
  backend status propagation, legend visibility handoff, and scroll-offset
  handoff without renderer geometry.
- `make test-host` includes `tests/test_app_shell.c`, which covers the
  app-shell boundary above platform input/rendering: startup deck scan and root
  session evidence, renderer-facing screen-model construction, opening a deck
  into review, raw review scroll-text handoff, failed root-session save retry,
  per-screen help reset on deck/review mode changes, shell-level `START+A`
  chord safety for help/open/reveal/rating commands, low-battery rating and
  undo state saves with optional review-log skipping,
  failed reveal/rating state-save rollback, confirmed-exit session-save gating,
  rollback if that final save fails and is then canceled, and one-shot battery
  shutdown.
- `make test-host` includes `tests/test_app_input_frame.c`, which covers
  app-frame input assembly above the physical key map: initial repeatable
  presses, delayed held-navigation repeats, chord-safe held context, non-repeat
  confirmation screens, and per-screen held-wait masks.
- `make test-host` includes `tests/test_app_deck_select_contract.c`, which
  covers deck-selector display strings, compact and expanded help legends,
  empty-list messaging, visible-window list text, scan status pluralization,
  and selected-index meta clamping.
- `make test-host` includes `tests/test_app_deck_select_action.c`, which covers
  the deck-selector input reducer used by `main.c`: mixed-button chord
  rejection, exit/rescan/open-deck requests, Up/Down movement, Left/Right
  paging, and no-op navigation/open on empty deck lists.
- `make test-host` includes `tests/test_app_deck_flow.c`, which covers
  deck-selector workflow side effects: startup scan/session diagnostics,
  missing-root startup diagnostics, rescan preserving the selected deck,
  manual scan session diagnostics, successful open with active
  paths/settings/day rollover, load-time rollover state-save rollback, and
  bad-deck load-error diagnostics without enabling state saves.
- `make test-host` includes `tests/test_app_settings_contract.c`, which covers
  Study Settings display strings, selected row markers, `All` formatting,
  default settings text, learning-mode labels, and selected-index clamping.
- `make test-host` includes `tests/test_app_settings_action.c`, which covers
  the Study Settings input reducer used by `main.c`: mixed-button chord
  rejection, ignored-input no-op behavior, command requests without selection
  or draft side effects, selected-field movement, preset value cycling,
  learning-mode toggling, and `no changes` / `unsaved changes` edit status.
- `make test-host` includes `tests/test_app_settings_flow.c`, which covers
  settings-screen workflow orchestration: field updates, cancel/exit
  transitions, warning-context statuses, successful save persistence/session
  diagnostics, and save failure staying in the editor with the draft preserved
  for retry.
- `make test-host` includes `tests/test_study_controls.c`, which covers the
  platform-neutral review control contract used by `main.c`: reveal, undo
  gating, answer-side Again/Hard/Good/Easy rating dispatch, navigation
  commands, mixed-button chord rejection, held-button chord safety, and
  frame-counted single-direction repeat for caller-selected navigation axes.
- `make test-host` includes `tests/test_app_confirm_action.c`, which covers
  the confirmation-screen input reducer used by `main.c`: mixed-button chord
  rejection, cancel buttons, `X` confirmation for suspend/restore/reset, `A`
  confirmation for exit, nested exit opening from non-exit confirmations,
  inert `START` on exit confirmation, invalid confirmation kinds failing
  closed for confirm/open-exit actions, and cancel status labels.
- `make test-host` includes `tests/test_app_confirm_contract.c`, which covers
  suspend/restore/reset/exit confirmation strings, restore pluralization, and
  null-status handling.
- `make test-host` includes `tests/test_app_confirm_flow.c`, which covers
  confirmation workflow side effects: nested exit opening, cancel mode returns
  with warning context, stale output flag clearing, confirmed exit session
  evidence, confirmed suspend/restore state-log dirtiness, confirmed reset cleanup/session
  diagnostics with low-battery suffix handling, and reset session evidence when
  only diagnostic log cleanup fails.
- `make test-host` includes `tests/test_app_reset_action.c`, which covers reset
  action cleanup/status behavior, primary-state cleanup failure preserving
  in-memory progress, and diagnostic log cleanup failure leaving progress reset
  with a `log kept` status.
- `make test-host` includes `tests/test_app_state_save.c`, which covers
  saved-action state persistence, optional review-log appends, root session
  saved-action counters, primary state-write failure reporting, ordinary
  successful-save redraw signaling, and low-battery skipping of optional
  review-log writes while preserving progress.
- `make test-host` includes `tests/test_app_review_action.c`, which covers the
  pure review action reducer used by `main.c`: scroll movement and clamped
  scroll no-ops, scroll reset on reveal/rating/undo, log metadata, dirty flags,
  stale exit-output clearing, and exit signaling.
- `make test-host` includes `tests/test_app_review_flow.c`, which covers
  review-screen workflow orchestration: hidden-answer help, Study Settings
  entry, unavailable undo/suspend feedback, reset/deck/exit/suspend/restore
  mode transitions, new/review daily-limit gates, answer-shown session
  diagnostics, stale exit-output clearing on non-exit transitions, and
  state/log dirty handoff from the reducer.
- `make test-host` includes `tests/test_app_day_rollover_flow.c`, which covers
  local-day rollover workflow side effects: same-day no-op behavior, root
  session day updates, backend daily-limit resets, rollover state persistence,
  disabled-state behavior, low-battery save suffix handling, and state-save
  failure rollback/status behavior.
- `make test-host` includes `tests/test_app_session_save.c`, which covers root
  session save policy: clean no-op behavior, dirty write success without
  status churn, backup preservation on overwrite, explicit save outcomes, and
  save failure status feedback. `tests/test_app_shell.c` also covers retrying a
  failed startup root-session save on a later frame, so scan diagnostics are not
  dropped after a transient `session.tsv` write failure.
- `make test-host` includes `tests/test_app_time.c`, which covers the
  device-local calendar day helper used by session diagnostics and day-aware
  daily limits, including the case where UTC has rolled over but local time has
  not.
- `make test-host` includes `tests/test_clean_shell_daily_use.c`, which covers
  the current clean-shell modules together across two decks: scan, load,
  reveal/rate/undo, local-day daily-limit counters, settings save/reload,
  true next-day rollover back to introduced review cards, suspend/restore,
  state/log writes, reset cleanup, and root session diagnostics.
- `make test-host` includes `tests/test_m7_direct_smoke_flow.c`, which drives
  the planned direct-control smoke path through the real study modules:
  `limits-demo` settings save, reveal/rate/undo, suspend-all, restore-all,
  relaunch session evidence, persisted compact state, review log, then a
  `sample` reveal/rating/reset leg that verifies progress/log cleanup while
  preserving settings before final confirmed exit on the reset deck.
- `make test-host` includes `tests/test_study_backend.c`, which covers compact
  state save/load, introduced-card state, completed-today queue state,
  current-day counters, and day rollover resetting daily counts without clearing
  lifetime progress, prioritizing due introduced cards before new cards, and
  reopening a completed deck for repeat review. It also covers first-load day
  establishment for an existing-progress compact state without clearing that
  day's counters or rewinding the active index.
- `make test-host` includes `tests/test_study_session.c`, which covers root
  session diagnostics, relaunch counter preservation, artifact recovery,
  rejection of impossible counter/event combinations, and rollover-only day
  updates that preserve the last recorded user-visible event.
- `make test-tools` includes `tests/test_verify_m7_artifacts.py`, which covers
  compact M7 artifacts with `progress_day`, current-day counters, the
  direct-control smoke artifact shape with studied `limits-demo` plus reset
  `sample`, and rejection of stale compact progress-day evidence against root
  `session.tsv`.
- `make test-tools` includes `tests/test_verify_app_theme.py`, which verifies
  the clean FE shell can place Citro2D/Citro3D calls in `app_renderer_c2d`
  while `app_shell` owns workflow state and builds `app_screen_model`, the
  backend remains renderer-free, the renderer public header stays opaque, and
  the renderer draws live display/status strings from the screen model. It also
  guards the fixed footer/status baselines: top deck/review meta stay aligned
  at `y=178`, and the bottom footer stays fixed at `y=174` so status text does
  not jump between screens. The active live font path is Citro2D shared-font
  text; Hack, Checkmate, Emporio, and FE8 glyph-map assets remain atlas and
  sprite-font inputs until the renderer grows a live bitmap-font path.
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
- `make verify-m7-preflight` runs `make verify-local`, then stages and verifies
  fresh tracked sample decks inside Azahar's SD data directory. It does not
  launch the emulator; run it immediately before the manual M7 emulator pass.
- `make package-sd` builds a clean SD-card payload under `dist/sdmc/` with the
  app artifact and tracked sample decks, but without generated progress files.
- `make run-emulator-fe-bg-viewer` builds the isolated FE background viewer,
  regenerates FE theme framebuffers, installs final and layer assets into
  Azahar's SD data directory, and launches the viewer for manual
  Background/Card/Font render inspection.

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
new row. If that repair or append fails, or if low battery intentionally skips
the optional diagnostic append, the status line reports `log skipped`.
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
for example, the local Azahar key for 3DS `START` is Enter and `SELECT` is
Backspace. Run `make fix-azahar-controls` if Azahar rewrites the profile.

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

The current clean-shell host C suite covers the flashcard string contract,
deck-selector string contract, deck-selector navigation/windowing, deck
scan/open workflow side effects, low-battery status suffix formatting,
low-battery monitor sampling, Study Settings string contract, backend card
flow including same-day daily counters and day rollover, confirmation string
contract, confirmation workflow side effects, review-screen workflow side
effects, failed reveal/rating state-save rollback at the app shell,
settings-screen workflow side effects, local-day rollover workflow side effects,
root session save policy, a clean-shell daily-use integration flow, logical
controls, the libctru 3DS key bridge, deck discovery, daily-limit settings,
compact review logs, root
session diagnostics, and power policy. The control coverage checks
reveal, undo gating, answer-side
Again/Hard/Good/Easy rating dispatch, navigation commands, held-button chord
safety, frame-counted navigation repeat, the app-shell per-screen repeat mask,
app-mode mapping to screen/confirmation behavior, scroll clamp/reset behavior,
confirmed reset cleanup/status behavior, daily-limit settings-save
persistence, saved-action state/log/session persistence, log metadata, dirty
flags, exit signaling, and mixed-button chord rejection.
`tests/test_study_3ds_key_map.c` compiles
`study_3ds_key_map` against `tests/stubs/3ds.h` so the `KEY_A`/`KEY_B`/
`KEY_X`/`KEY_Y`, shoulder, D-pad, Circle Pad, `SELECT`, and `START`
translation branch is covered outside hardware builds. The power coverage
checks battery polling cadence, retry scheduling, clock-rollback poll recovery,
the one-shot low-battery status warning latch, and the adaptive idle input wait
tiers used to avoid busy polling while the screen is unchanged.
`tests/test_app_status_text.c` verifies the `; batt low`
save-status suffix is appended once and preserved during truncation, and that
successful workflow feedback can retain warning context such as ignored
settings, daily-limit exhaustion, and low-battery suffixes.
Tracked sample fixture coverage is limited to the default text decks,
`sample` and `limits-demo`.

GitHub Actions runs `make verify-ci` on pushes and pull requests. That covers
the portable C host suite, converter tests, and tracked sample-deck
verification; emulator and hardware checks remain manual checkpoint steps. The
C host compiler can be overridden with `HOST_CC` and `HOST_CFLAGS`; the default
flags include the POSIX feature level needed by the timezone and filesystem
tests.

Run the full local M7 preflight gate with:

```sh
make verify-m7-preflight
```

That target runs the local pre-checkpoint gate, then prepares and verifies
fresh tracked sample decks inside Azahar's SD data directory without launching
the emulator. It does not remove personal imported decks from the same Azahar
deck root; the M7 smoke driver discovers sorted deck-folder order and navigates
to `limits-demo` and `sample` by id before sending study keys.

Run the local pre-checkpoint gate with:

```sh
make verify-local
```

That target runs `make test`, builds the 3DS app, and stages the local SD mirror
with sample decks. It requires the local 3DS toolchain, so CI uses the portable
`make verify-ci` gate instead. `make verify-local` prepares fresh tracked
sample decks in the local SD mirror but does not stage Azahar's SD data
directory; run `make verify-m7-preflight` or `make run-emulator-fresh-samples`
before a fresh manual emulator acceptance pass.

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
The direct Anki collection importer also lives in `make test-tools`; its tests
use a synthetic SQLite collection and must not depend on or print personal
Anki card content.

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
  and confirm reveal/scroll feedback preserves active warning context when
  present
- suspend one card, then restore suspended cards with the direct `R` flow
- confirm pressing `R` on a completed deck with no suspended cards reports
  `Nothing suspended` and preserves active warning context when present
- press `A` on a completed deck and confirm introduced cards are available
  again without resetting lifetime review counts or deck settings
- undo one rating and confirm the queue/status updates sensibly; saved and
  no-op undo feedback keeps active warning context when present
- confirm saved rating, suspend, and restore feedback also keeps active warning
  context when present, except when save failure or daily-limit-complete
  feedback takes priority
- change `new_limit`, `review_limit`, and learning mode from Study Settings
- confirm settings edits show an unsaved-changes cue before saving and
  return to no-changes state after save or cancel, with active warning context
  preserved after cancel
- confirm opening Study Settings and moving clean fields/values from reset-needed,
  ignored-settings, or limit-blocked decks preserves that warning context in
  the status line
- confirm `START` from Study Settings warns before losing unsaved edits
- confirm opening exit, restore, suspend, and reset confirmations preserves
  selected-deck, load-error, or active-deck warning context
- confirm canceling exit from deck select, review, Study Settings, confirmations,
  and warning surfaces preserves the relevant status-line warning context
- confirm canceling restore/suspend/reset confirmations preserves active deck
  warning context in the status line
- confirm Study Settings, restore, suspend, and reset screens show the
  expected active deck before taking deck-specific actions
- exit, relaunch, and confirm review state and settings persisted
- check that no manual file edits were needed during the session

Record evidence in `docs/emulator-test-log.md` or `docs/device-test-log.md`:

- command or copy method used for sample prep
- deck ids covered
- before/after `new_limit`, `review_limit`, and learning-mode values when
  changed
- whether `state.tsv`, `settings.tsv`, and `review-log.tsv` appeared beside
  the tested decks after app actions
- any render, input, save-feedback, or SD-card issues observed

The post-run artifact verifier requires exact settings expectations. Pass them
through the Make target as `deck:new_limit:review_limit[:learning_mode]`; the
optional learning-mode field is `0` for Due first and `1` for Cooldown.
For example:

```sh
make verify-m7-artifacts M7_EXPECT_SETTINGS="sample:5:20:1 limits-demo:1:10"
```

If the app reported `Saved; log skipped` because the battery was low or the
diagnostic append failed, keep exact settings/deck expectations but pass
`M7_ALLOW_MISSING_REVIEW_LOG=1`. If required event types are not all present in
the remaining diagnostic rows, also pass `M7_NO_REQUIRED_EVENTS=1`.

The verifier accepts both current clean-shell artifacts and the fuller target
scheduler artifacts: compact key/value `state.tsv`, six-field
`review-log.tsv`, framed scheduler `state.tsv`, and 21-field scheduler logs.
Compact artifacts prove the current checkpoint's saved actions, counters,
settings, reset cleanup, relaunch diagnostics, and confirmed-exit evidence;
they do not by themselves prove the future full scheduler implementation.

It also requires the root `session.tsv` to end with
`last_event=exit_confirmed`, so run the app through the confirm-exit flow after
the final checked action.

## Recent Local Verification

2026-06-07 UI/tag/review-log cleanup:

- `make test-host`
- `python3 tools/verify_app_theme.py`
- `make -C app-3ds`
- `git diff --check`

These covered the grouped `button: action` legends, explicit `Today New` /
`Review` footer labels, deck-selector `All decks` aggregate footer,
revealed-card tag footer, backend tag parsing, and review-log
validation/recovery hardening.

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
- live local/Azahar personal decks, if checked directly, use
  `tools/verify_text_deck.py --allow-progress-files` so app-owned
  `state.tsv`/`review-log.tsv` artifacts are preserved; do not use that flag
  for tracked fixtures or package payloads
- `make verify-local-personal-decks` and `make verify-azahar-personal-decks`
  are available for the current ignored personal deck set; override
  `PERSONAL_DECKS="..."` for a different local set
- `make verify-package-sd` stages and verifies the expected SD payload when
  preparing files for manual copy or release
- no personal Anki data is committed
- no copyrighted media is committed
- build instructions are current
- release payload is `app-3ds/anki3ds.3dsx` plus `app-3ds/anki3ds.smdh`;
  `.cia` packaging remains future work unless documented separately
