# Sleep Context: 2026-06-08

This is the short resume file for picking `anki3ds` back up after a few days
away. Read this before digging through the longer historical
`docs/session-handoff.md`.

## Current State

- The active app is a Nintendo 3DS homebrew flashcard app with a Fire
  Emblem-inspired Citro2D UI, imported text/Anki decks, deck stats, reveal and
  rating flow, undo, suspend/restore, reset, per-deck settings, daily limits,
  compact progress saves, root session diagnostics, low-battery-aware save
  behavior, and local-day rollover.
- The old Anki/backend rendering attempt should stay gone. The app should not
  draw with console text or direct framebuffer overlays anymore.
- The current implementation is the clean-shell module set:
  `app_*` for shell/display/policy and `study_*` for backend/study data.
- The worktree is intentionally dirty. Many legacy files are deleted and many
  clean-shell files are untracked. Do not restore legacy files just to make
  `git status` smaller.
- Manual Azahar or hardware validation is the main unfinished checkpoint.

## Resume First

1. Read this file, then skim the top of `docs/session-handoff.md`.
2. Do not redesign rendering first. Start with current controls/UI validation.
3. The exact last control-map safety audit is closed: shell-level tests now
   prove `START+A` does not toggle help, reveal, open a deck, rate, or confirm
   anything.
4. Run focused tests, then `make test-host`, `make test-tools`,
   `make -C app-3ds`, `git diff --check`, and `make verify-m7-preflight`.
5. Launch or prepare Azahar only after the local gates pass. For a
   non-destructive manual pass, prefer `make prepare-azahar-daily-use`.

## Architecture Boundaries

- Backend/study modules output strings, counters, scheduling state, save
  outcomes, and deck summaries. They do not know Citro2D, texture names,
  colors, fonts, screen dimensions, or coordinates.
- Contract/screen-model modules build renderer-facing strings and flags. They
  still do not own draw calls or geometry.
- `app_renderer_c2d.c` is the only active pixel owner. It owns Citro2D/Citro3D
  includes, render targets, texture loading, FE background/parchment draw
  order, colors, word wrapping, visible text windows, scrollbars, tag chips,
  status/footer baselines, and text scale.
- `app_shell.c` owns workflow orchestration: mode, selected deck, active
  backend, help visibility, scroll offsets, settings/confirmations, save
  outcomes, battery samples, day rollover, and dispatch to reducers.
- `main.c` should stay thin: initialize platform services, scan 3DS input,
  build input frames, run the shell, draw the screen model, wait, and shut
  down.
- If a new feature needs renderer and backend work, keep the renderer accepting
  strings/flags and keep backend logic unaware of view geometry.

## Citro2D And Rendering Lessons

- Citro2D is the right baseline for this 2D app. Citro3D is not needed for
  parchment, background, text, scrollbars, or cards.
- Do not mix Citro2D with console text or direct framebuffer drawing on the
  same active screens. The severe color bugs came from competing draw paths and
  stale framebuffer assumptions, not from a single bad palette constant.
- When a visual change seems ignored, prove the active renderer before tuning:
  check the app Makefile source list, rebuild, clear stale SDMC diagnostics,
  inspect `sdmc:/3ds/anki3ds/renderer.tsv`, and only then adjust constants.
- Top screen is 400x240. Bottom screen is 320x240. Treat them as separate
  render targets with independent parchment and safe text rectangles.
- The parchment boundary bug was fixed by matching renderer text boxes to the
  generated asset rectangles. Screenshot guessing wasted time.
- The renderer currently uses Citro2D shared/system-font text for readability.
  FE/Hack glyph assets can be revisited later only as renderer-only work and
  only if readability survives on Azahar and real hardware.
- The best parchment direction so far is the OpenGameArt old parchment asset
  under `assets/fe-themes/external/opengameart/old-parchment-paper/`, processed
  through the FE theme asset pipeline.
- Keep runtime diagnostics boring and explicit. `renderer.tsv` is useful
  because it records the exact linked constants Azahar executed.

## UI/UX Preferences

- The app should feel like a Fire Emblem-style study screen: map background,
  parchment/scroll cards, readable dark text, restrained controls, no console
  box.
- Background should fill the 3DS screen with no black edges.
- Parchment should sit over the background like a scroll/card, with text safely
  inside its visual bounds.
- Front/question text belongs on the top screen. Revealed back/answer text
  belongs on the bottom screen.
- Do not show a `Question` title. The front is implicit.
- Empty answers should render as muted `(No answer)`, not as a long warning.
- Tags belong on the back side as separate chips/blocks, one tag per chip,
  using the same readable renderer font.
- Help should be hidden by default. `START` toggles detailed help.
- Expanded help should be grouped as `button: action`, not a flat list.
- Use side scrollbars for scroll state. Do not render `Scroll 1/2` in footer
  text.
- Deck-specific state belongs on the top screen. Overall learning/deck-list
  summary belongs in the bottom screen footer.
- Use explicit labels such as `Due`, `New`, and `Susp`. Avoid unexplained
  shorthand like `N/R` or `Held`.

## Footer Baseline Rule

- The user specifically disliked footer/status labels moving between screens.
- The visual target is the deck selector `Deck 5/6 Ignored 0` placement: the
  distance from the text baseline to the parchment bottom should remain static.
- Do not fix bottom status drift by moving the top status upward. The requested
  fix was to move bottom footer/status text down to match the top placement.
- Current guarded renderer constants are:
  - top deck selector meta: `y=178`
  - top review meta: `y=178`
  - bottom footer/status: `y=174`
- Status/footer text should stay one line. If it would wrap, shorten or
  truncate it instead of shifting the baseline.
- `tools/verify_app_theme.py` now guards these constants when it can parse the
  live renderer text-box declarations.

## Current Controls

The current preference is `START` for help and `B` for undo.

- `A`: open deck, reveal answer, rate Again after reveal, review introduced
  cards again on completion, confirm exit.
- `B`: undo in review; cancel settings and confirmations.
- `X`: open settings before reveal; rate Good after reveal; save settings;
  confirm suspend/restore/reset.
- `Y`: open reset before reveal or on completion; rate Easy after reveal; open
  exit confirmation from deck select.
- `L`: page deck selector up; rate Hard after reveal.
- `R`: page deck selector down; suspend current card or restore suspended.
- D-pad Up/Down: move deck/settings selection and scroll the bottom answer.
- Circle Pad Up/Down: scroll the top front/question.
- D-pad Left/Right: page deck list and adjust setting presets.
- Circle Pad Left/Right: same logical left/right navigation as D-pad.
- `START`: toggle help on deck/review screens; open exit confirmation from
  settings.
- `SELECT`: rescan deck selector, return from review to deck selector, cancel
  settings/confirmations.

Azahar keyboard profile:

- face buttons use matching letters
- D-pad uses arrow keys
- Circle Pad uses `W/S/Q/E`
- `START` is Enter
- `SELECT` is Backspace

Mixed button chords should stay inert for destructive actions, ratings,
settings saves, reset, suspend/restore, and exit. If a command is pressed while
another command/navigation key is held, reducers should reject it.

## Data And Imports

- Runtime SD root: `sdmc:/3ds/anki3ds/`.
- Deck folders: `sdmc:/3ds/anki3ds/decks/<deck-id>/`.
- Imported content: `deck.json` and `cards.tsv`.
- App-owned durable progress: `state.tsv`.
- Per-deck limits/learning mode: `settings.tsv`.
- Root diagnostics: `session.tsv`.
- Review-log diagnostics: `review-log.tsv`. This is evidence only and can be
  skipped after a successful compact state save on low battery.
- Re-importing deck content should preserve local progress.
- Personal decks are expected in ignored local/Azahar SD-card mirrors, not
  necessarily tracked repo files.
- Important user decks mentioned: Leetcode, Recsys, SAT Vocabulary, Vim, and an
  advanced algorithms deck. Recsys and advanced algorithms are especially
  important to verify when import work resumes.

## Learning Policy Direction

- Keep the scheduler swappable.
- Current default is day-based spaced repetition with compact `state.tsv`
  version 2.
- A card-count cooldown mode exists for same-session repeats.
- Likely long-term policy: use card-count cooldown for immediate relearning,
  then day/time spacing for mature reviews.
- Renderer, input, and import code should not know which learning policy is
  active.

## Environment Notes

- Azahar app path used during this session:
  `/Users/eric/Applications/azahar-macos-arm64-2125.1.2/Azahar.app/Contents/MacOS/azahar`
- Azahar SD root:
  `~/Library/Application Support/Azahar/sdmc/3ds/anki3ds`
- Azahar launch can be finicky. The reliable path is through the Makefile
  launch/install targets, and sometimes a clean GUI restart is needed.
- Automated Azahar key driving may fail until macOS Accessibility permission is
  granted to the terminal/Codex host. If automation is blocked, continue with
  manual Azahar or hardware testing.
- Screenshots can be misleading if macOS captures the wrong region or stale
  process. Runtime evidence from `renderer.tsv` and `session.tsv` is more
  trustworthy when debugging whether a rebuild is actually running.

## Useful Commands

```sh
make test-host
make test-tools
make -C app-3ds
git diff --check
make verify-m7-preflight
make prepare-azahar-daily-use
make install-azahar-app
make fix-azahar-controls
make run-emulator
```

## Last Known Verification

Before this sleep-context file was added, these had passed in the current
daily-use push:

- `python3 -m unittest tests/test_verify_m7_artifacts.py`
- `python3 -m unittest tests/test_verify_app_theme.py`
- `make test-tools`
- `make test-host`
- `make -C app-3ds`
- `git diff --check`
- `make verify-m7-preflight`
- `make prepare-azahar-daily-use`

`make prepare-azahar-daily-use` also verified the current Azahar personal deck
mirrors for Leetcode, Recsys, SAT Vocabulary, and Vim.

For future implementation work, keep `git diff --check` in the normal local
gate before handing off or launching Azahar.

2026-06-08 control-safety follow-up:

- `tests/test_app_shell.c` now has shell-level coverage for `START+A` on deck
  select, hidden-answer review, and revealed-answer review.
- The test proves `START+A` neither toggles help nor fires the primary action
  when help is hidden or already visible.
- Verification passed after the guard with `make test-host`,
  `make test-tools`, `make -C app-3ds`, `git diff --check`, and
  `make verify-m7-preflight`.

2026-06-08 low-battery undo follow-up:

- `tests/test_app_shell.c` now drives the real review flow through reveal,
  rating, and `B` undo while the battery sample is low.
- The test proves undo saves compact state, increments `undo_saved_count`, keeps
  the restored introduced card due, and skips the optional `review-log.tsv`
  append with `Saved; log skipped; batt low`.
- This is workflow-level coverage above the existing save-layer low-battery
  undo test.
- Verification passed after this guard with `make test-host`,
  `make test-tools`, `make -C app-3ds`, `git diff --check`, and
  `make verify-m7-preflight`.

2026-06-08 manual-test readiness follow-up:

- `make prepare-azahar-daily-use` passed and installed the current app plus FE
  assets into Azahar's SDMC mirror.
- Live Azahar launched from `make run-emulator` and wrote fresh runtime
  evidence: `session.tsv` shows `deck_count=6`, `ignored_count=0`,
  `last_event=scan`, and no study actions yet; `renderer.tsv` shows
  `renderer=citro2d`, `assets_loaded=1`, top meta `y=178`, and bottom footer
  `y=174`.
- `make drive-azahar-m7-smoke-dry-run` produced the expected current key
  sequence for `limits-demo` plus reset `sample`.
- `make check-azahar-smoke-automation` is still blocked by macOS Accessibility:
  `macOS denied osascript keystrokes`. Grant Accessibility permission to the
  terminal/Codex host, then rerun `make run-emulator-m7-smoke`, or continue
  with manual Azahar/hardware testing.

## Next Task List

1. Manual Azahar/hardware validation of the current UI and controls:
   `START` help, `B` undo, `L` Hard, `Y` reset/exit, `SELECT` deck return,
   Enter/Backspace Azahar mapping, tag chips, scrollbars, fixed footers, and
   `(No answer)`.
2. Run a full M7 daily-use manual pass on imported decks: open, reveal, rate,
   undo, suspend/restore, settings save, reset one deck, exit/relaunch, then
   inspect `session.tsv`, `state.tsv`, optional `review-log.tsv`, and verifier
   output.
3. Continue UI polish only after proving the active renderer with runtime
   evidence. Do not return to console/raw-framebuffer rendering.
4. Resume import/algorithm work after UI/control validation. Verify one
   important imported deck, especially Recsys or advanced algorithms, before
   bulk importing more content.
