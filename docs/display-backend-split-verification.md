# Display/Backend Split Verification

The split should make Azahar a final integration check, not the main feedback
loop. App behavior should be provable from host tests that do not initialize
libctru, open the emulator, or inspect screenshots.

## Current Fast Baseline

Keep these checks as the default development loop:

```sh
make test-host
make test-tools
make -C app-3ds
```

`make test-host` already covers the flashcard display string contract, portable
power policy, study backend transitions and state recovery, logical control
classification, the physical 3DS key bridge, deck indexing, settings
persistence, review-log writes, and root session diagnostics.
`make test-tools` covers deck import validation, Azahar key-profile validation,
M7 artifact validation, theme startup/fallback invariants, smoke-driver
planning, and FE asset generation. `make -C app-3ds` remains the fast compile
gate for the 3DS shell.

## Split Target

Move app behavior out of `app-3ds/source/main.c` behind two portable layers:

- `app_core`: owns `app_state`, modes, selected deck/action/settings indices,
  active deck paths, status messages, diagnostics counters, current day,
  battery display state, and action application.
- `app_view_model`: builds screen-ready data from `app_core`, such as title,
  selected rows, status text, battery text, due legends, flashcard text,
  scroll metadata, theme id, and a list of logical draw primitives.

The 3DS shell should keep only platform concerns:

- `gfxInitDefault`, console/framebuffer setup, and frame presentation.
- `hidScanInput`, raw 3DS key translation, and repeat timing.
- framebuffer asset loading/copying.
- calls from `app_core` state into `app_view_model`, then into the renderer.

## Host Test Targets

Add focused C host tests before relying on Azahar:

- `tests/test_app_core.c`: initialize from a fake deck root, scan decks, select
  decks, open a deck, reveal answer, rate cards, undo, suspend, restore,
  change daily limits, reset progress, rescan, confirm exit, and verify the
  resulting mode, status text, diagnostics counters, save files, review log,
  and deck summaries.
- `tests/test_app_view_model.c`: feed known `app_core` states and assert the
  screen model for deck select, review front, revealed answer, summary,
  actions, settings, confirmations, controls/help, load errors, low-battery
  warnings, and no-due states.
- `tests/test_app_theme_model.c`: verify Plain startup, opt-in FE themes,
  theme cycling order, null background behavior for Plain, and the selected
  logical chrome/background ids. Keep pixel/color conversion in
  `tests/test_fe_theme_assets.py`.
- `tests/test_app_core_day_power.c`: verify day rollover and low-battery status
  latching at the app-core level, using injected day/time and battery samples.

These tests should consume portable button masks from `app_controls`, not 3DS
key constants. Keep `tests/test_app_controls_3ds_keys.c` as the thin proof that
physical 3DS keys map to those portable buttons.

## Suggested Make Targets

Keep emulator-free work visible with separate targets:

```make
test-app-core
test-view-model
test-display-assets
verify-fast: test-app-core test-view-model test-host test-tools app-3ds
```

`verify-fast` should be the normal loop while building UI behavior. It should
not open Azahar.

## Minimal Emulator Checks

Use Azahar only when platform behavior or visual integration is actually under
test:

- After input-shell changes: `make verify-azahar-controls`, then one
  `make run-emulator-m7-smoke`.
- After renderer/framebuffer changes: launch the smallest relevant surface,
  preferably the isolated FE background viewer or a dedicated display harness,
  and capture/manual-check only background, frame, legend, and font layering.
- Before M7 acceptance: `make run-emulator-m7-smoke` for the automated
  multi-deck smoke gate, or run the manual checklist and then
  `make verify-m7-artifacts` with exact `M7_DECKS`, `M7_RESET_DECKS`, and
  `M7_EXPECT_SETTINGS` for the tested decks.
- Before checkpoint/hardware handoff: `make verify-m7-preflight`, then a
  manual hardware pass recorded in `docs/device-test-log.md`.

Do not use full emulator launches to prove scheduler, save/load, review-log,
settings, status-message, or screen-text decisions. Those belong in host tests
against `app_core` and `app_view_model`.
