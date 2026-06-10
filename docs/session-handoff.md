# Session Handoff

Last updated: 2026-06-08

Use this file as the first read for a fresh session. Keep it current when the
thread changes direction, after a meaningful verifier pass, before stopping for
the day, or after a confusing emulator/debugging detour.

Before resuming after the 2026-06-08 sleep break, read
`docs/sleep-context-2026-06-08.md` first. It is the concise current-state
handoff for renderer lessons, UX preferences, controls, data/import notes, and
the next task list; this file keeps the full historical record.

## 2026-06-08 Final Sleep Snapshot

Read this section first after the overnight break. It supersedes older same-day
notes below where they conflict, especially around controls, renderer ownership,
and the current UI polish queue.

Current state:

- `anki3ds` is a 3DS homebrew flashcard app with imported text/Anki decks,
  daily limits, compact per-deck state, undo, suspend/restore, reset, settings,
  session diagnostics, and a Fire Emblem-inspired Citro2D UI.
- The old Anki-rendering/console/raw-framebuffer path should stay deleted. The
  active app is the clean-shell `app_*` and `study_*` module set.
- The worktree is intentionally dirty. Many active clean-shell files are
  untracked and many legacy files are deleted. Do not restore legacy files to
  make `git status` look clean.
- The most important pending checkpoint is manual Azahar or hardware
  validation of the current UI/control flow, not another renderer rewrite.

Architecture to preserve:

- Backend/study modules output strings, counters, scheduling state, save
  outcomes, and deck summaries. They must not know Citro2D, colors, fonts,
  texture names, coordinates, or screen dimensions.
- Contract/screen-model modules build renderer-facing strings and flags. They
  still must not expose draw APIs, geometry constants, or Citro types.
- `app_renderer_c2d.c` is the only active pixel owner. It owns render targets,
  FE background/parchment textures, text layout, word wrapping windows,
  scrollbars, tag chips, colors, font scale, and all Citro2D/Citro3D calls.
- `app_shell` owns workflow orchestration: mode, active deck, settings, help
  visibility, scroll offsets, save/session handling, battery samples, local-day
  rollover, and dispatch to per-mode reducers.
- `main.c` should stay thin: platform init, key scan, 3DS-key-to-logical-input
  translation, shell frame handling, renderer draw, wait, and shutdown.

Rendering lessons that matter most:

- Citro2D is the right baseline. Do not switch to Citro3D for parchment,
  background, text, scrollbars, or card panels.
- Do not mix Citro2D with console text or direct framebuffer drawing in the
  active app. The severe color bugs came from competing draw paths and stale
  framebuffer assumptions.
- When visuals look wrong, prove the active renderer before tuning constants:
  check the app Makefile source list, rebuild, clear stale SDMC diagnostics if
  needed, inspect `renderer.tsv`, and then adjust renderer constants.
- Top screen is 400x240; bottom screen is 320x240. Treat them as independent
  render targets with independent parchment and safe text rectangles.
- The parchment/text boundary bug was solved by matching renderer text
  rectangles to the generated asset rectangles, not by screenshot guessing.
- Text readability wins over exact FE font authenticity. The current readable
  path uses Citro2D shared/system-font text. Bitmap FE/Hack glyphs are future
  renderer-only work if they remain readable.
- The parchment asset direction that looked best was the OpenGameArt old
  parchment paper asset under `assets/fe-themes/external/opengameart/`.
  Preserve the generated FE asset pipeline instead of drawing parchment by
  hand in runtime code.

Current UI preferences:

- Keep the FE background visible with no black edges. Parchment should feel
  like a scroll/card over the map, not a console box.
- Front/question text belongs on the top screen. Revealed back/answer text
  belongs on the bottom screen.
- Do not render a `Question` header; the front text is implicit.
- Empty answer text should show muted `(No answer)`, not a verbose warning.
- Tags belong on the back side as separate chips/blocks, one tag per chip,
  using the same readable renderer font.
- Help is hidden by default and toggled with `START`. Expanded help should be
  grouped as `button: action`, not a flat list.
- Use side scrollbars for scrollable front/back text. Do not show `Scroll 1/2`
  in footer/status text.
- Deck-specific state belongs on the top screen. Overall learning/deck-list
  summary belongs in the bottom screen footer.
- Use explicit labels like `Due`, `New`, and `Susp`. Avoid unexplained
  shorthand like `N/R` or `Held`.

Footer/status baseline issue to remember:

- The user called out that footer/status labels were moving between deck
  select, front/back review, and bottom legend screens.
- The ideal visual reference is the deck selector `Deck 5/6 Ignored 0`
  placement: the distance from the text baseline to the parchment bottom should
  remain static.
- Do not fix this by moving the top-screen footer up to match the bottom. The
  requested fix was to move bottom footer/status text down to match the top
  placement when the bottom has extra space.
- If a status/footer would exceed one line, shorten or truncate the text so the
  baseline does not jump. If multiline status is unavoidable later, grow the
  text upward from the fixed bottom baseline.

Current control intent:

- `A`: open deck, reveal, rate Again after reveal, review introduced cards
  again on completion, confirm exit.
- `B`: undo in review; cancel in settings and confirmations.
- `X`: open settings before reveal; rate Good after reveal; save settings;
  confirm suspend/restore/reset.
- `Y`: open reset before reveal or on completion; rate Easy after reveal; open
  exit confirmation from deck select.
- `L`: page deck selector up; rate Hard after reveal.
- `R`: page deck selector down; suspend current card or restore suspended.
- D-pad Up/Down: deck/settings movement and bottom-answer scrolling.
- Circle Pad Up/Down: top-front/question scrolling.
- `START`: toggle help on deck/review screens; open exit confirmation from
  settings.
- `SELECT`: rescan deck selector, return from review to deck selector, cancel
  settings/confirmations.
- Azahar profile: face buttons use matching letters, D-pad uses arrow keys,
  Circle Pad uses `W/S/Q/E`, `START` is Enter, and `SELECT` is Backspace.

Important data model notes:

- Decks live under `sdmc:/3ds/anki3ds/decks/<deck-id>/`.
- Imported deck content is `deck.json` and `cards.tsv`.
- App-owned durable progress is `state.tsv`.
- Per-deck daily-limit/learning settings are `settings.tsv`.
- `review-log.tsv` is diagnostic/evidence only; it can be skipped after a
  successful compact state save on low battery.
- Re-importing deck content should not require deleting local progress.
- Personal Anki imports are expected in ignored local/Azahar SD-card mirrors,
  not necessarily tracked repo files. Important user decks mentioned: Leetcode,
  Recsys, SAT Vocabulary, Vim, and an advanced algorithms deck.

Current learning-algorithm direction:

- Keep the scheduler swappable.
- Current default is day-based spaced repetition with compact `state.tsv`
  version 2.
- A card-count cooldown mode exists for same-session repeats.
- Likely long-term policy: card-count cooldown for immediate relearning, then
  day/time spacing for mature reviews.
- Renderer, input, and import code should not know which policy is active.

Known next tasks:

1. Manual Azahar/hardware pass of the current UI: help toggle, `B` undo,
   `L` Hard, `Y` reset/exit, `SELECT` deck return, Enter/Backspace mapping,
   tag chips, scrollbars, fixed footer baselines, and `(No answer)`.
2. Finish the M7 daily-use pass on imported decks: open, reveal, rate, undo,
   suspend/restore, settings save, reset one deck, exit/relaunch, inspect
   `session.tsv`, `state.tsv`, optional `review-log.tsv`, and verifier output.
3. Continue UI polish only after proving the active renderer with runtime
   evidence. Do not return to console/raw-framebuffer rendering to solve
   layout or color issues.
4. After UI/control validation, continue import/algorithm work. Verify one
   important imported deck, especially Recsys or advanced algorithms, before
   bulk importing more content.

Useful commands:

- `make test-host`
- `make -C app-3ds`
- `git diff --check`
- `make verify-m7-preflight`
- `make prepare-azahar-daily-use`
- `make install-azahar-app`
- `make fix-azahar-controls`

2026-06-08 verifier follow-up:

- `tools/verify_m7_artifacts.py --expect-settings` now accepts
  `deck_id:new_limit:review_limit[:learning_mode]`.
- Three-field expectations remain compatible; the optional fourth field proves
  saved learning mode when supplied.
- Missing `learning_mode` in `settings.tsv` verifies as default `0`. Expected
  `1` requires a saved `learning_mode\t1` row.
- Verification passed with `python3 -m unittest tests/test_verify_m7_artifacts.py`,
  `make test-host`, `make test-tools`, `make -C app-3ds`, `git diff --check`,
  `make verify-m7-preflight`, and `make prepare-azahar-daily-use`.
- `make prepare-azahar-daily-use` also verified the current Azahar personal
  deck mirrors for Leetcode, Recsys, SAT Vocabulary, and Vim.

2026-06-08 renderer-baseline verifier follow-up:

- `tools/verify_app_theme.py` now parses live renderer text-box declarations
  when present and rejects regressions where `app_menu_top_meta_box` and
  `app_review_top_meta_box` stop sharing the fixed `y=178` baseline.
- The same verifier rejects moving `app_bottom_footer_box` away from fixed
  `y=174`, guarding the Azahar-reported footer/status jumping bug.
- This is a regression guard only; the current renderer constants were already
  at the desired values.
- Verification passed with `python3 -m unittest tests/test_verify_app_theme.py`,
  `make test-tools`, `make test-host`, `make -C app-3ds`, `git diff --check`,
  and `make verify-m7-preflight`.

2026-06-08 control-safety follow-up:

- `tests/test_app_shell.c` now has shell-level coverage for `START+A` on deck
  select, hidden-answer review, and revealed-answer review.
- This closes the shell-special-case gap where `START` help toggling happens
  before lower-level reducers see input: `START+A` neither toggles help nor
  opens a deck, reveals, or rates.
- Verification passed with `make test-host`, `make test-tools`,
  `make -C app-3ds`, `git diff --check`, and `make verify-m7-preflight`.

2026-06-08 low-battery undo follow-up:

- Added app-shell workflow coverage for the real low-battery undo path:
  reveal, rate, then press `B` while PTMU reports low battery.
- The test proves compact state is saved, `undo_saved_count` increments, the
  restored introduced card is due again, and optional `review-log.tsv` evidence
  is skipped with `Saved; log skipped; batt low`.
- Verification passed after the change with `make test-host`,
  `make test-tools`, `make -C app-3ds`, `git diff --check`, and
  `make verify-m7-preflight`.

2026-06-08 manual-test readiness follow-up:

- `make prepare-azahar-daily-use` passed and verified the staged app, FE
  assets, Azahar control profile, and personal deck mirrors for Leetcode,
  Recsys, SAT Vocabulary, and Vim.
- `make run-emulator` launched Azahar with the current `.3dsx`. Fresh
  `session.tsv` showed `deck_count=6`, `ignored_count=0`, `last_event=scan`,
  and no accepted study actions yet. Fresh `renderer.tsv` showed Citro2D,
  loaded assets, and the guarded footer/meta baselines.
- `make drive-azahar-m7-smoke-dry-run` generated a valid key plan for the
  current Azahar SD deck order.
- `make check-azahar-smoke-automation` still failed at the macOS permission
  gate: `macOS denied osascript keystrokes`. The next acceptance step is manual
  Azahar/hardware testing, or granting Accessibility permission and rerunning
  `make run-emulator-m7-smoke`.

## 2026-06-08 Sleep Context Dump

Read this section first if resuming after the user slept or after a few days
away. It is intentionally practical and should let a fresh session avoid the
Citro/rendering traps that cost time earlier.

Current mental model:

- This is now a clean Nintendo 3DS homebrew flashcard app, not the previous
  Anki backend with FE art painted behind a console UI.
- The target feel is Fire Emblem-inspired: full-screen map-like background,
  parchment/scroll cards, readable dark text, hidden help, and 3DS-native
  button flow.
- Daily-use functionality is real again: imported decks, deck selector stats,
  reveal/rate, undo, suspend/restore, reset, per-deck settings, state saves,
  root session diagnostics, low-battery save behavior, and local-day rollover.
- The worktree is intentionally dirty. Old app files were deleted and the
  clean `app_*` / `study_*` files are mostly untracked. Do not "clean up" by
  restoring deleted legacy files.

Authoritative implementation paths:

- Platform loop: `app-3ds/source/main.c`.
- Workflow shell: `app-3ds/source/app_shell.c`.
- Renderer: `app-3ds/source/app_renderer_c2d.c`.
- Renderer handoff: `app-3ds/source/app_screen_model.c` and
  `app-3ds/source/app_*_contract.c`.
- Backend/study state: `app-3ds/source/study_backend.c`,
  `study_deck_index.c`, `study_settings.c`, `study_review_log.c`, and
  `study_session.c`.
- Input boundary: `study_controls.c`, `study_3ds_key_map.c`,
  `app_input_frame.c`, and `app_input_policy.c`.
- Theme pipeline: `tools/import_fe_theme_assets.py`, `app-3ds/gfx/`, and
  `docs/theme-assets.md`.
- Controls reference: `docs/control-map.md`.
- 3DS/Citro fundamentals reference: `docs/3ds-coding-fundamentals.md`.

Non-negotiable boundaries:

- Backend emits strings, state, counters, save outcomes, and scheduling state.
  It does not know renderer geometry, colors, Citro types, textures, or fonts.
- Contract/screen-model code prepares renderer-facing strings and flags. It
  still does not expose coordinates or draw calls.
- `app_renderer_c2d.c` is the only active pixel owner. It owns render targets,
  textures, colors, text buffers, wrapping/windowing, scrollbars, tag chips,
  status/footer baselines, and all Citro2D/Citro3D calls.
- `main.c` should stay thin: initialize platform services, scan input,
  translate 3DS keys once, run `app_shell`, draw the screen model, wait, exit.
- Mixed button chords must stay inert. Ratings, confirmations, settings saves,
  resets, suspend/restore, and exit should not fire when another button is
  pressed or held.

Citro2D/rendering lessons to preserve:

- Citro2D is the correct baseline. Do not switch to Citro3D for parchment,
  background, text, or scrollbars.
- Do not mix Citro2D with console text or raw framebuffer drawing in the active
  app. The severe color bugs were caused by competing draw paths and stale
  framebuffer assumptions, not by one bad palette constant.
- When visuals look wrong, prove the active renderer first: inspect the
  Makefile source list, rebuild, clear stale SDMC diagnostics if needed, check
  `renderer.tsv`, and only then tune coordinates/colors.
- The top screen is 400x240 and the bottom screen is 320x240. Treat them as
  independent render targets with independent safe text rectangles.
- The parchment boundary issue was solved by matching renderer text rectangles
  to generated asset rectangles, not by guessing from screenshots.
- Text readability currently beats exact FE font authenticity. The live path
  uses Citro2D shared/system-font text. FE/Hack glyph assets can become a later
  renderer-owned bitmap-font path only if readability stays good.
- Text wrapping belongs in `app_text`: prefer spaces, and only split inside a
  word when the word is wider than the row. Renderer owns visible windows and
  scroll indicators.
- One-line footer/status baselines should not move between deck select,
  review, completion, and settings. If a status would exceed one line, shorten
  or truncate it rather than shifting the baseline.

UX/UI preferences from the user:

- Keep the FE background visible with no black edges.
- Parchment should feel like a scroll/card over the map, not a console panel.
- Front/question goes on the top screen. Revealed back/answer goes on the
  bottom screen.
- No `Question` header. The front text is implicitly the question.
- Empty back text should render as muted `(No answer)`.
- Tags belong on the back side as separate chips/blocks, one tag per chip.
- Help should be hidden by default and toggled with `START`.
- Expanded help should be grouped as `button: action`, not a flat list.
- Deck-specific state belongs on the top screen. Overall learning/deck-list
  summary belongs on the bottom footer.
- D-pad Up/Down scrolls bottom answer text. Circle Pad Up/Down scrolls top
  front/question text.
- Use side scrollbars for scroll state. Do not show `Scroll 1/2` as footer
  text.
- Use explicit labels like `Due`, `New`, and `Susp`. Avoid unexplained
  shorthand such as `N/R` or `Held`.

Current control intent:

- `A`: open deck, reveal, Again after reveal, review introduced cards again on
  completion, confirm exit.
- `B`: undo in review, cancel settings/confirmations.
- `X`: settings before reveal, Good after reveal, save settings, confirm
  suspend/restore/reset.
- `Y`: reset before reveal/completion, Easy after reveal, exit from selector.
- `L`: page deck selector up, Hard after reveal.
- `R`: page deck selector down, suspend current card or restore suspended.
- D-pad: deck movement, settings value movement, bottom-answer scrolling.
- Circle Pad: top-front/question scrolling.
- `START`: toggle help on deck/review screens; open settings exit
  confirmation.
- `SELECT`: rescan deck selector, return from review to deck selector, cancel
  settings/confirmations.
- Azahar mapping: face buttons are matching letters, D-pad is arrow keys,
  Circle Pad is `W/S/Q/E`, `START` is Enter, and `SELECT` is Backspace.

Data and import model:

- Decks live under `sdmc:/3ds/anki3ds/decks/<deck-id>/`.
- Imported source files are `deck.json` and `cards.tsv`.
- App-owned local progress is `state.tsv`; per-deck settings are
  `settings.tsv`; `review-log.tsv` is diagnostic/evidence only.
- Re-importing deck content should not require erasing local progress.
- Personal Anki imports are expected in ignored local/Azahar SD-card mirrors,
  not necessarily tracked repo files.
- Important imported decks mentioned by the user include Leetcode, Recsys, SAT
  Vocabulary, Vim, plus tracked sample decks. The Recsys and advanced
  algorithms decks are important for later import verification.

Learning algorithm direction:

- Keep scheduling swappable.
- Current default is day-based spaced repetition with simple intervals stored
  in compact `state.tsv` version 2.
- A card-count cooldown policy exists for same-session learning repeats.
- The likely long-term policy is hybrid: card-count cooldown for immediate
  relearning steps, day/time spacing for mature reviews.
- Renderer, import, and input should not know which policy is active.

Azahar/environment notes:

- Fast local check: `make test-host`.
- 3DS build check: `make -C app-3ds`.
- Pre-emulator packaging/staging check: `make verify-m7-preflight`.
- Current Azahar SD root:
  `~/Library/Application Support/Azahar/sdmc/3ds/anki3ds`.
- `make install-azahar-app` updates only the app binary. Use it when personal
  imported decks should stay in place.
- Automated Azahar key driving may be blocked by macOS Accessibility. If
  `osascript` cannot send keys, that is an environment gate, not necessarily
  an app regression.
- Azahar screenshots can be misleading if macOS window capture fails. If a
  capture cannot prove what is visible, have the user inspect the window or
  restart Azahar before debugging renderer constants.

Current verification state at this sleep dump:

- `make test-host` passed after the low-battery day-rollover shell coverage.
- `make -C app-3ds` passed; the current `anki3ds.3dsx` was already up to date.
- `git diff --check` passed.
- Trailing-whitespace scan passed for `docs/session-handoff.md` and
  `tests/test_app_shell.c`.
- `make verify-m7-preflight` passed and restaged the current app, FE assets,
  tracked sample decks, and Azahar control profile into local/package/Azahar
  SD roots.
- Manual Azahar/hardware validation is still pending for the latest control
  map and footer/status layout.

2026-06-08 continuation note:

- Added a live-SD verification mode to `tools/verify_text_deck.py`:
  `--allow-progress-files`.
- Default behavior remains strict for tracked fixtures and package payloads:
  app-owned `state.tsv`, `review-log.tsv`, and temp/backup artifacts are still
  rejected unless this flag is explicitly provided.
- The new mode is for ignored local/Azahar SD-card deck folders where personal
  decks may already have live app progress. It still validates `deck.json`,
  `cards.tsv`, and `settings.tsv`, and still rejects unknown entries such as
  media folders.
- Verified local personal imports without the flag:
  `recsys` 159 cards, `leetcode` 169 cards, `vim` 279 cards, and
  `sat-vocabulary` 626 cards.
- Verified the live Azahar SD-card copies with `--allow-progress-files` so
  existing user progress was not deleted. Those folders are present in
  `~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks/`.

2026-06-08 personal-deck verifier target follow-up:

- Added `PERSONAL_DECKS ?= leetcode recsys sat-vocabulary vim` plus
  `make verify-local-personal-decks` and `make verify-azahar-personal-decks`.
- These targets use `tools/verify_text_deck.py --allow-progress-files`, so
  they validate live ignored SD-card deck folders without rejecting app-owned
  progress/log/temp/backup files.
- They are intentionally not part of `verify-ci`, `verify-local`, or
  `verify-m7-preflight`; tracked fixtures and package payloads remain strict.
- Both targets passed on the current local and Azahar personal deck mirrors.

2026-06-08 non-destructive Azahar daily-use prep follow-up:

- Added `make prepare-azahar-daily-use`.
- It installs the current app binary into Azahar's SDMC mirror, installs FE
  theme assets, verifies the Azahar control profile, and verifies the current
  configured personal deck set with live progress files allowed.
- It does not reset `session.tsv`, sample progress, personal deck progress, or
  review logs. Use it before manual daily-use passes on imported decks.
- Added `make run-emulator-daily-use`, which runs the same prep path and then
  launches Azahar with the current `.3dsx`.
- `make prepare-azahar-daily-use` passed against the current Azahar mirror.

2026-06-08 input-source safety follow-up:

- Hardened `app_input_frame` so simultaneous D-pad and Circle Pad navigation is
  ambiguous and emits no navigation action, even if both physical sources point
  in the same direction after logical key-map collapse.
- This preserves the current UX contract: D-pad Up/Down scrolls the bottom
  answer, Circle Pad Up/Down scrolls the top front/question, and mixed physical
  navigation sources do not pick whichever scroll panel happens to move.
- Added host regressions in `tests/test_app_input_frame.c` and clarified the
  collapsed-key-map behavior in `tests/test_study_3ds_key_map.c`.
- `make test-host`, `make -C app-3ds`, `git diff --check`, and
  `make prepare-azahar-daily-use` passed after this change.

2026-06-08 low-battery reveal-save coverage follow-up:

- Added shell-level host coverage for revealing a new card while the fake PTMU
  battery sample is low.
- The test proves reveal-only progress writes compact `state.tsv`, skips
  optional `review-log.tsv`, flushes root `session.tsv` answer-shown evidence,
  and reloads with the card introduced but not answer-visible.
- This closes the low-battery evidence gap between answer reveal and rating:
  reveal is still a state mutation because it consumes new-card daily limit
  progress even before a rating is chosen.
- `make test-host`, `make -C app-3ds`, `git diff --check`, and
  `make prepare-azahar-daily-use` passed after this test addition.

2026-06-08 review-again persistence follow-up:

- Added shell-level host coverage for pressing `A` on a completed deck to
  review introduced cards again.
- The test completes the deck, presses `A` on the completion state, reloads
  `state.tsv`, and proves this is not a reset: lifetime `reviewed_count`
  remains, the card stays introduced, daily counters are reopened for the same
  session, and `settings.tsv` / active settings remain unchanged.
- This covers the M7 checklist item that `A` on completion makes introduced
  cards available again without resetting lifetime progress or deck settings.
- `make test-host`, `make -C app-3ds`, `git diff --check`, and
  `make prepare-azahar-daily-use` passed after this test addition.

Next best wake-up checklist:

1. Run `git status --short`.
2. Read this section, `docs/control-map.md`, and
   `docs/3ds-coding-fundamentals.md`.
3. If no code changed, run `make -C app-3ds`, `git diff --check`, and
   `make verify-m7-preflight` before launching Azahar.
4. Manually verify in Azahar or hardware: `START` help, `B` undo, `L` Hard,
   `Y` reset/exit, `SELECT` deck return, Enter/Backspace mapping, fixed footer
   baselines, tag chips, scrollbars, and `(No answer)`.
5. Do a daily-use pass on imported decks: open, reveal, rate, undo,
   suspend/restore, settings save, reset one deck, exit/relaunch, inspect
   `session.tsv`, `state.tsv`, and optional `review-log.tsv`.
6. After the UI/control pass, continue algorithm/import work. Verify one
   important imported deck before bulk continuing.

## 2026-06-08 Overnight Resume Packet

Read this section first after a long break. It is the current project memory
dump, and it overrides older notes in `PROJECT_PLAN.md` or historical sections
below that describe console rendering, raw framebuffer themes, or the old
scheduler-era app tree.

Current state at shutdown:

- The active direction is a daily-use 3DS text flashcard app with a Fire
  Emblem-inspired Citro2D presentation, not a full Anki clone and not a console
  UI with FE assets painted behind it.
- The repo worktree is intentionally dirty because the old Anki/backend files
  were deleted and the clean-shell modules are still mostly untracked. Treat
  the current `app-3ds/source/app_*.c`, `app-3ds/source/study_*.c`,
  matching headers, tests, and generated `app-3ds/gfx/` assets as the active
  source of truth.
- Latest non-GUI verification passed: `make test`, `make test-host`,
  `make -C app-3ds`, `git diff --check`, and `make verify-m7-preflight`.
- The automated Azahar smoke driver is blocked until macOS Accessibility allows
  the terminal/Codex host to send keystrokes. This is an environment gate, not
  evidence of an app regression.
- Manual visual verification is still the next important checkpoint: open the
  rebuilt app in Azahar or on hardware and confirm controls, footer baselines,
  tag chips, scrolling, and daily-use persistence with imported decks.

Resume checklist:

- Start with `git status --short`, then read this section plus
  `docs/control-map.md` and `docs/3ds-coding-fundamentals.md`.
- If touching rendering, inspect `app-3ds/source/app_renderer_c2d.c` and the
  generated layout fingerprint at `sdmc:/3ds/anki3ds/renderer.tsv`.
- Use `make test-host` for fast C host coverage, `make -C app-3ds` for the
  3DS build, and `make verify-m7-preflight` before an emulator/hardware pass.
- Use `make install-azahar-app` when only the app binary needs updating.
  Personal/imported decks in Azahar's SD root should not be wiped during normal
  app rebuilds.

Architecture to preserve:

- Backend/study code outputs strings, state, counters, deck summaries, and save
  outcomes. It does not draw, include Citro headers, own coordinates, choose
  colors, or know screen geometry.
- Contract/screen-model code builds renderer-facing strings. It still must not
  expose Citro2D types, textures, fonts, or pixel coordinates.
- `app_renderer_c2d.c` is the only active draw path. It owns both render
  targets, texture sheets, colors, text layout, word wrapping/windowing,
  scrollbars, tag chips, and fixed baselines.
- `app_shell` owns workflow and orchestration: mode, active deck paths,
  settings, help visibility, scroll offsets, battery sampling, day rollover,
  save/session handling, and dispatch to per-mode flow reducers.
- `main.c` should remain thin: platform init, input scan, key translation,
  shell frame handling, renderer draw, and shutdown.
- `study_controls` and `study_3ds_key_map` are the control boundary. Workflow
  code should use logical controls, not raw `KEY_*` checks.

Rendering lessons that saved time:

- Citro2D is the right baseline. Citro3D is not needed for parchment,
  background, text, scrollbars, or simple card UI.
- Do not mix Citro2D drawing, console text, raw framebuffer writes, or
  post-process color replacement. The severe color/clashing bugs came from
  competing draw paths and stale console/framebuffer assumptions.
- When visuals look wrong, prove what is rendering before tuning constants.
  Check the Makefile source list, add/use a runtime fingerprint, clear stale
  SDMC diagnostics, rebuild, then inspect the exact active layout data.
- The parchment boundary bug was fixed by matching renderer text rectangles to
  the generated asset rectangle. Do not guess from screenshots when asset
  sizes and renderer constants can be logged.
- Top screen is 400x240; bottom screen is 320x240. Treat them as separate
  surfaces with independent safe rectangles.
- Use Citro2D shared/system-font text for now. FE bitmap font work can happen
  later, but readability is more important than exact FE authenticity.
- Word wrapping should prefer spaces and only split inside a word when a word
  is longer than the row. That behavior belongs in `app_text`; the renderer
  owns the visible window and scroll indicators.
- Fixed one-line status/footer baselines are important UX. The current
  renderer draws top meta at y=178 and the bottom footer at y=174 so status
  text does not jump between screens. If text is too long, truncate or format
  it; do not shift the baseline.

UX/UI preferences to preserve:

- Keep the FE background visible with no black edges. Parchment should feel
  like a scroll laid over a map, not a framed console panel.
- Front/question belongs on the top screen. Revealed answer belongs on the
  bottom screen.
- `Question` as a title/header is unnecessary; the top card text is implicitly
  the prompt.
- Help/instructions are hidden by default and toggled. The expanded legend
  should be grouped by controls with `button: action` labels, not a flat list.
- Deck-specific state belongs on the top screen. Overall learning/deck-list
  summary belongs on the bottom screen footer.
- Answers are more likely to be long. D-pad Up/Down scrolls the bottom answer;
  Circle Pad Up/Down scrolls the top front/question.
- Use side scrollbars to indicate scroll state. Do not put `Scroll 1/2` in the
  status text.
- Tags on the back side should show as separate chips/blocks, one tag per
  chip, using the same readable renderer text.
- Empty Anki back fields should display a muted `(No answer)` marker, not a
  verbose sentence.
- `Susp` is the visible abbreviation for suspended cards. Avoid unexplained
  `N/R` counters.
- The `review-log.tsv` is diagnostic/evidence only. `state.tsv` is the durable
  progress source. It is acceptable to skip review-log appends on low battery
  after the compact state save succeeds.

Current control intent:

- `A`: open deck, reveal, Again after reveal, review introduced cards again
  on completion, confirm exit.
- `B`: undo in review; cancel in settings/confirmations.
- `X`: settings before reveal; Good after reveal; save settings; confirm
  suspend/restore/reset.
- `Y`: reset before reveal/completion; Easy after reveal; exit confirmation
  from deck selector.
- `L`: page deck selector up; Hard after reveal.
- `R`: page deck selector down; suspend current card or restore suspended.
- D-pad: deck movement, settings value movement, bottom-answer scrolling.
- Circle Pad: separate top-front/question scrolling in review.
- `START`: toggle help on deck/review screens; exit confirmation from settings.
- `SELECT`: rescan deck selector, return to deck selector from review, cancel
  settings/confirmations.
- Azahar keyboard profile: face buttons use matching letters, D-pad uses arrow
  keys, Circle Pad uses `W/S/Q/E`, `START` is Enter, and `SELECT` is Backspace.

Current data/import model:

- Decks live under `sdmc:/3ds/anki3ds/decks/<deck-id>/`.
- Each deck has imported `deck.json`, `cards.tsv`, optional `settings.tsv`,
  app-owned `state.tsv`, and optional diagnostic `review-log.tsv`.
- Imported card content and local review progress are deliberately separate so
  re-importing a deck does not have to erase progress.
- Personal decks imported from Anki are expected in ignored local/Azahar SD
  mirrors, not necessarily as tracked repo files. The user has imported decks
  such as Leetcode, Recsys, SAT Vocabulary, Vim, plus the tracked sample decks.
- `tools/import_anki_collection.py` is the direct read-only Anki database
  importer. Verify one important imported deck before bulk continuing.

Current engineering queue:

- Manual Azahar/hardware check of the latest control map: `START` help,
  `B` undo, `L` Hard, `Y` reset/exit, `SELECT` deck return, Enter/Backspace
  Azahar mapping.
- Daily-use M7 pass on imported decks: open, reveal, rate, undo, suspend,
  restore, settings save, reset one deck, exit/relaunch, and verify artifacts.
- Keep algorithm policy swappable. Current default is day-based spaced
  repetition with a card-count cooldown policy available for same-session
  repeats. The likely long-term direction is hybrid: card-count cooldown for
  immediate learning steps, day/time spacing for mature reviews.
- Continue UI polish only after proving the active renderer: footer/status
  wording, tag-chip readability, scrollbars, and help grouping. Do not revive
  console/raw-framebuffer rendering to solve polish issues.

## 2026-06-08 Sleep Handoff

Start here when resuming after a few days. This section supersedes older
control-map notes later in this file that mention `B` as help/Hard,
`L` as undo, or `START` as main-screen exit.

Current project shape:

- `anki3ds` is a Nintendo 3DS homebrew flashcard app with imported Anki/text
  decks, day-level spaced repetition, per-deck progress, daily limits, undo,
  suspend/restore, settings, and session diagnostics.
- The active UI is a centralized Citro2D Fire Emblem-inspired shell:
  generated forest background, parchment panels, renderer-owned text, and
  backend-owned strings/state only.
- The old console/direct-framebuffer/Anki-rendering path should stay dead.
  Do not reintroduce console overlay drawing or BGR framebuffer post-processing
  for the active app. That caused the earlier color/clashing failures.
- Backend/study modules emit strings and state. Renderer modules own pixels,
  coordinates, wrapping/windowing, colors, fonts, scrollbars, tags, and all
  Citro2D/Citro3D calls.

Important files and boundaries:

- `app-3ds/source/main.c` is the platform loop: initialize services, translate
  3DS keys, run `app_shell`, draw only through the renderer, and wait/exit.
- `app_shell` owns app workflow state: mode, active deck/settings paths,
  help visibility, scroll offsets, save/session orchestration, battery polling,
  day rollover, and dispatch to per-mode flow reducers.
- `study_backend` owns cards, reveal/rating state, scheduling, undo,
  suspend/restore, compact `state.tsv`, and backend status strings.
- `app_*_contract` files build renderer-facing strings. They should expose no
  geometry, colors, textures, fonts, Citro types, or draw calls.
- `app_screen_model` aggregates current screen contracts for the renderer.
- `app_renderer_c2d.c` is the only active draw path. Its public header is
  opaque; Citro2D/Citro3D headers stay private.
- `study_controls` owns platform-neutral controls. `study_3ds_key_map` is the
  thin libctru key-bit adapter. Workflow code should not branch on `KEY_*`.

Current 3DS control mapping:

- Deck selector: `A` opens, `START` toggles help, `SELECT` rescans,
  `Y` opens exit confirmation, D-pad up/down moves, D-pad left/right pages,
  and `L/R` page decks.
- Review: `A` reveals, then rates Again. `L` rates Hard after reveal.
  `X` opens settings before reveal, then rates Good. `Y` opens reset before
  reveal/completion, then rates Easy after reveal. `B` undoes the previous
  rating. `R` suspends/restores. `SELECT` returns to deck select.
  `START` toggles help.
- Exit from review is intentionally indirect for now: `SELECT` to deck select,
  then `Y` to exit confirmation.
- Settings still use `START` for exit confirmation, `X` save, and
  `B/SELECT` cancel. Confirm screens still use `X` for suspend/restore/reset,
  `A` for exit confirm, and `B/SELECT` cancel.
- Azahar keyboard profile: face buttons use matching letters, D-pad uses arrow
  keys, Circle Pad uses `W/S/Q/E`, `START` is Enter, and `SELECT` is
  Backspace. If Azahar rewrites controls, run `make fix-azahar-controls`.

UX/UI preferences captured from the session:

- Keep the FE background visible with no black edges. Parchment should feel
  like a scroll/card laid over the background, not a console window.
- Text readability wins over exact FE font authenticity. The current live path
  uses Citro2D shared/system-font text at one shared scale; Hack/FE glyph assets
  can stay as future bitmap-font work, not a blocker.
- No visible "Question" header is needed; the front is implicit.
- Review layout preference: front/prompt on the top screen, revealed back on
  the bottom screen. Answers are more likely to be long, so D-pad scrolls the
  bottom answer; Circle Pad scrolls the top front/question.
- Help/instructions should be hidden by default and toggled. Use grouped
  `button: action` labels, not a long flat list.
- Deck-specific information belongs mostly on top; overall learning/deck-list
  summary belongs on the bottom footer.
- Fixed status/footer baselines matter. One-line status/footer text should sit
  at the same distance from the parchment bottom across deck select, review,
  completion, and settings. If it overflows, truncate rather than shifting.
- Tags should draw as distinguishable chips/blocks, one tag per chip, using
  renderer-owned text/chip layout.

Citro2D/rendering lessons:

- Citro2D is the right baseline for this 2D app. Citro3D is only needed for
  lower-level custom effects; parchment/background/text do not justify it.
- C is fine here. C++ may be nicer for abstractions, but the current devkitPro
  C + small-module style is already testable and works with the existing app.
- Keep one active renderer per screen. The major color bugs came from mixed
  draw paths and stale framebuffer/console assumptions, not asset color alone.
- Generated assets and renderer constants must match. The parchment text
  boundary bug was solved by proving the active renderer and aligning renderer
  rectangles with the generated asset rectangle instead of guessing.
- Use runtime evidence before tuning visuals: `renderer.tsv`, asset dimensions,
  layout constants, and screenshots. Clear stale SDMC diagnostics when needed.
- Text wrapping should prefer spaces; only break inside a word when the word is
  wider than the row. `app_text` owns this logic; renderer owns visible windows.
- Top screen is 400x240 and bottom is 320x240. Treat them as independent
  render targets with independent safe text rectangles.
- Citro2D frame shape remains: frame begin, target clear, scene begin, draw
  textures/rects/text for top, repeat for bottom, frame end.
- Embedded FE assets are generated as PNG previews, converted with `tex3ds`
  into `.t3x`, linked from `app-3ds/gfx/*.t3s`, then loaded with
  `C2D_SpriteSheetLoadFromMem()` and drawn by `C2D_DrawImageAt()`.

Azahar/environment lessons:

- Primary verification commands after app changes:
  `make test`, `make -C app-3ds`, `git diff --check`,
  `make verify-m7-preflight`.
- `make verify-m7-preflight` runs host/tool suites, regenerates/stages FE
  assets, stages sample decks into local/dist/Azahar SD roots, clears stale
  sample state/session/theme/renderer files, and verifies Azahar controls.
- Local Azahar SD root:
  `~/Library/Application Support/Azahar/sdmc/3ds/anki3ds`.
- Personal/imported decks are intentionally under ignored local/Azahar SD-card
  mirrors. Do not treat every deck in the live Azahar SD root as tracked data.
- Azahar launch can be finicky. `make run-emulator` may start a process without
  a capturable visible window on macOS. CoreGraphics can report an onscreen
  Metal/Qt window while `screencapture -l` fails with
  `could not create image from window`. If that happens, do not debug renderer
  layout from a desktop screenshot. Have the user inspect the visible app, or
  restart Azahar cleanly from the GUI.
- If duplicate Azahar processes appear, use `pgrep -fl Azahar` and kill only
  stale duplicates. At this handoff, the detached tmux Azahar session was
  stopped and no Azahar process remained.

Latest verification before sleep:

- `make test` passed.
- `make -C app-3ds` passed and built `anki3ds.3dsx`.
- `make verify-m7-preflight` passed.
- `git diff --check` passed.
- Manual visual/control verification of the newest `START` help and `B` undo
  mapping is still pending because the post-build Azahar launch created an
  unhelpful/capture-resistant window state.

Next best checkpoint:

- Manually open the rebuilt app in Azahar or hardware and verify:
  `START` toggles help on deck/review screens, `B` undoes, `L` rates Hard after
  reveal, deck `Y` opens exit confirmation, and the help legend text matches.
- After that, run a daily-use pass on imported decks: open deck, reveal, rate,
  undo, suspend/restore, settings change, reset one deck, exit/relaunch, and
  verify `session.tsv`, per-deck `state.tsv`, and review logs.
- Then continue with algorithm work: keep the learning policy swappable and
  evaluate day-based spaced repetition against card-count/cooldown-style
  repeats. Current scheduler is deliberately simple day-level spacing.

## 2026-06-08 Smoke Driver Keymap Follow-Up

- Updated `tools/drive_azahar_m7_smoke.py` after the control remap. The driver
  now uses `B` for undo, returns from review to deck select with `SELECT`, and
  opens exit confirmation from deck select with `Y`. It no longer sends
  `START` for main review exits.
- Updated `tests/test_drive_azahar_m7_smoke.py` to assert those exact buttons,
  including the final exit sequence `SELECT`, `Y`, `A`.
- Updated active guidance docs so daily-use/hardware smoke instructions match
  the new mapping: `docs/control-map.md`, `docs/device-test-log.md`, and
  `docs/emulator-feedback-loop.md`.
- Focused verification passed:
  `python3 tools/drive_azahar_m7_smoke.py --dry-run` and
  `python3 -m unittest tests/test_drive_azahar_m7_smoke.py`.

## 2026-06-08 Continuation Verification Follow-Up

- Confirmed the active code path already keeps D-pad and Circle Pad scrolling
  separate: `main.c` passes source-specific masks to `app_input_frame`, and
  `app_shell` applies D-pad Up/Down to the answer scroll offset while Circle
  Pad Up/Down applies to the front/question scroll offset.
- Updated stale `CHECKPOINTS.md` M1/M2 instructions that still said `START`
  opened exit from the main app. They now say `START` toggles help and deck
  selector exit is `Y`, then `A` to confirm.
- Tried `make run-emulator-m7-smoke`, but macOS denied System Events
  keystrokes before Azahar launched. This is an Accessibility permission gate,
  not a renderer/app failure. Rerun the smoke target after granting the
  terminal/Codex host Accessibility permission.
- Non-GUI verification passed afterward:
  `make test-host`,
  `python3 -m unittest tests/test_drive_azahar_m7_smoke.py`,
  `make -C app-3ds`,
  `make drive-azahar-m7-smoke-dry-run`,
  `git diff --check`, and
  `make verify-m7-preflight`.

## 2026-06-08 Warning Context Follow-Up

- Fixed the review-side Study Settings unavailable path so it preserves active
  warning context. Example: opening settings without a usable settings path now
  reports `Settings unavailable; Settings ignored` instead of dropping the
  existing ignored-settings warning.
- Updated `tests/test_app_review_flow.c` to prove the unavailable-settings
  warning-context behavior.
- Updated stale M5 checkpoint text in `CHECKPOINTS.md`: reset is now documented
  as `Y` before reveal, then `X` to confirm, not `SELECT`.
- Verification passed after this patch:
  `make test-host`,
  `make -C app-3ds`,
  `git diff --check`, and
  `make verify-m7-preflight`. The preflight rebuilt/restaged the current app,
  FE assets, fresh tracked sample decks, and the Azahar control profile.

## 2026-06-08 Low-Battery Shell Evidence Follow-Up

- Added shell-level host coverage for the low-battery review-save path in
  `tests/test_app_shell.c`. The test sets the fake PTMU battery level low,
  opens a deck, reveals a card, rates it, and verifies compact `state.tsv`
  progress is saved while optional `review-log.tsv` is skipped with
  `Saved; log skipped; batt low`.
- This complements the lower-level `app_state_save` and `app_battery_monitor`
  tests by proving `app_shell` actually passes the sampled battery warning into
  the review save path during a normal study action.
- Verification passed after this test addition:
  `make test-host`, `make -C app-3ds`, `git diff --check`, and
  `make verify-m7-preflight`.

## 2026-06-08 Low-Battery Settings Save Follow-Up

- Added shell-level host coverage for the low-battery Study Settings save path
  in `tests/test_app_shell.c`.
- The test opens a deck on a low fake PTMU battery sample, opens settings,
  changes `new_limit`, saves, verifies `settings.tsv` was written, verifies the
  active settings updated in review mode, and checks that the status contains
  `Settings saved` plus the low-battery `batt low` suffix.
- This proves the shell passes low-battery warning context into settings saves,
  not only review ratings. It also records `settings_saved_count` in the root
  session evidence.

## 2026-06-08 Shell Chord-Safety Follow-Up

- Added `test_mixed_button_chords_do_not_trigger_shell_commands` in
  `tests/test_app_shell.c`.
- The test drives M7-dangerous mixed-button inputs through the full shell:
  deck open, deck exit, unrevealed reset, settings save, reveal, rating, and
  reset confirmation. Each mixed input is ignored without opening, saving,
  revealing, rating, resetting, or confirming.
- This complements the lower-level `study_controls`, `app_input_frame`, and
  per-screen action reducer tests by proving the composed app shell preserves
  chord safety across normal mode transitions.

## 2026-06-08 Low-Battery Suspend/Restore Shell Follow-Up

- Added shell-level host coverage for low-battery suspend and restore in
  `tests/test_app_shell.c`.
- The test opens a deck on a low fake PTMU battery sample, suspends the current
  card through `R`, then `X`, restores through `R`, then `X`, and proves each
  action writes compact `state.tsv`, skips optional `review-log.tsv`, refreshes
  selector suspended counts, and records `suspend_saved_count` /
  `restore_saved_count` in root session evidence.
- This extends the low-battery shell proof beyond ratings and settings saves,
  covering the remaining M7 daily-use save actions that mutate card state
  through confirmation screens.

## 2026-06-08 Low-Battery Reset Shell Follow-Up

- Added shell-level host coverage for low-battery reset preserving settings in
  `tests/test_app_shell.c`.
- The test opens a deck on a low fake PTMU battery sample, saves a changed
  `new_limit`, reveals and rates a card so compact `state.tsv` exists, then
  opens reset with `Y` and confirms with `X`.
- It proves reset removes progress state, keeps `settings.tsv`, keeps the
  active settings value, refreshes selector counts back to a fresh new card,
  records `reset_progress_count`, and keeps a low-battery suffix in the reset
  status. This closes the host-test evidence gap for the M7 "reset progress
  while preserving settings" workflow.

## 2026-06-08 Low-Battery Day Rollover Shell Follow-Up

- Added shell-level host coverage for low-battery local-day rollover in
  `tests/test_app_shell.c`.
- The test opens a deck, reveals and rates a card, advances to a later local
  day, and proves the shell persists compact `state.tsv` with daily counters
  reset while preserving the low-battery `batt low` suffix.
- It also proves the deck selector summary refreshes after rollover: the
  previously Good-rated card becomes due on the later day, `new_count` stays
  zero, and optional `review-log.tsv` creation is skipped during the
  low-battery rollover save.
- `make test-host` passed after this coverage was added.

## Current Clean-Slate Checkpoint

As of the latest turn on 2026-06-06, `app-3ds` has intentionally been stripped
down to a centralized Citro2D FE UI shell. This is no longer the old Anki
backend running behind FE visuals. This section is the current source of truth;
older sections below are historical context unless they are explicitly called
out here.

Active app-side files in the 3DS binary:

- `app-3ds/source/main.c`
- `app-3ds/source/app_battery_monitor.c`
- `app-3ds/source/app_battery_status.c`
- `app-3ds/source/app_confirm_action.c`
- `app-3ds/source/app_confirm_contract.c`
- `app-3ds/source/app_confirm_flow.c`
- `app-3ds/source/app_day_rollover_flow.c`
- `app-3ds/source/app_deck_flow.c`
- `app-3ds/source/app_deck_navigation.c`
- `app-3ds/source/app_deck_select_action.c`
- `app-3ds/source/app_deck_select_contract.c`
- `app-3ds/source/app_flashcard_contract.c`
- `app-3ds/source/app_input_frame.c`
- `app-3ds/source/app_input_policy.c`
- `app-3ds/source/app_mode.c`
- `app-3ds/source/app_power.c`
- `app-3ds/source/app_renderer_c2d.c`
- `app-3ds/source/app_reset_action.c`
- `app-3ds/source/app_review_action.c`
- `app-3ds/source/app_review_flow.c`
- `app-3ds/source/app_screen_model.c`
- `app-3ds/source/app_session_save.c`
- `app-3ds/source/app_settings_action.c`
- `app-3ds/source/app_settings_contract.c`
- `app-3ds/source/app_settings_flow.c`
- `app-3ds/source/app_settings_save.c`
- `app-3ds/source/app_shell.c`
- `app-3ds/source/app_state_save.c`
- `app-3ds/source/app_status_text.c`
- `app-3ds/source/app_text.c`
- `app-3ds/source/study_3ds_key_map.c`
- `app-3ds/source/study_backend.c`
- `app-3ds/source/study_controls.c`
- `app-3ds/source/study_deck_index.c`
- `app-3ds/source/study_settings.c`
- `app-3ds/source/study_review_log.c`
- `app-3ds/source/study_session.c`
- `app-3ds/include/app_battery_monitor.h`
- `app-3ds/include/app_battery_status.h`
- `app-3ds/include/app_confirm_action.h`
- `app-3ds/include/app_confirm_contract.h`
- `app-3ds/include/app_confirm_flow.h`
- `app-3ds/include/app_day_rollover_flow.h`
- `app-3ds/include/app_deck_flow.h`
- `app-3ds/include/app_deck_navigation.h`
- `app-3ds/include/app_deck_select_action.h`
- `app-3ds/include/app_deck_select_contract.h`
- `app-3ds/include/app_flashcard_contract.h`
- `app-3ds/include/app_input_frame.h`
- `app-3ds/include/app_input_policy.h`
- `app-3ds/include/app_mode.h`
- `app-3ds/include/app_power.h`
- `app-3ds/include/app_renderer_c2d.h`
- `app-3ds/include/app_reset_action.h`
- `app-3ds/include/app_review_action.h`
- `app-3ds/include/app_review_flow.h`
- `app-3ds/include/app_screen_model.h`
- `app-3ds/include/app_session_save.h`
- `app-3ds/include/app_settings_action.h`
- `app-3ds/include/app_settings_contract.h`
- `app-3ds/include/app_settings_flow.h`
- `app-3ds/include/app_settings_save.h`
- `app-3ds/include/app_shell.h`
- `app-3ds/include/app_state_save.h`
- `app-3ds/include/app_status_text.h`
- `app-3ds/include/app_text.h`
- `app-3ds/include/study_3ds_key_map.h`
- `app-3ds/include/study_backend.h`
- `app-3ds/include/study_controls.h`
- `app-3ds/include/study_deck_index.h`
- `app-3ds/include/study_settings.h`
- `app-3ds/include/study_review_log.h`
- `app-3ds/include/study_session.h`

Deleted app-side backend files include the old deck parser/index, scheduler,
learning/card-state adapter, review state/log, settings, diagnostics, status,
view-model, display-model, app controls, storage, and their headers. Those
responsibilities were replaced for this checkpoint by small `study_backend`,
`study_controls`, `study_deck_index`, `study_settings`, and
`study_review_log` modules, plus `study_session` for root session diagnostics.
`app_power` has been restored as a small pure policy helper for battery
polling, low-battery latching, and idle input wait backoff; it still owns no
PTMU calls or rendering.

`app-3ds/Makefile` now compiles only `main.c`, `app_battery_monitor.c`,
`app_battery_status.c`, `app_confirm_action.c`, `app_confirm_contract.c`,
`app_confirm_flow.c`, `app_day_rollover_flow.c`, `app_deck_flow.c`,
`app_deck_navigation.c`, `app_deck_select_action.c`, `app_deck_select_contract.c`,
`app_flashcard_contract.c`, `app_input_frame.c`, `app_input_policy.c`,
`app_mode.c`, `app_power.c`, `app_renderer_c2d.c`, `app_reset_action.c`,
`app_review_action.c`, `app_review_flow.c`, `app_screen_model.c`,
`app_session_save.c`, `app_settings_action.c`,
`app_settings_contract.c`, `app_settings_flow.c`, `app_settings_save.c`,
`app_shell.c`, `app_state_save.c`, `app_status_text.c`, `app_text.c`,
`study_3ds_key_map.c`, `study_backend.c`, `study_controls.c`,
`study_deck_index.c`, `study_settings.c`,
`study_review_log.c`, and `study_session.c` for the app binary. It also links
the generated texture objects from `app-3ds/gfx/*.t3s`.
The stale
direct-framebuffer `app_fe_renderer` / `app_review_front_contract` path and
orphaned `test_app_view_model.c` were removed after the clean Citro2D path was
confirmed.

Current app behavior:

- `app_renderer_c2d` owns the active Citro2D render path. There is no libctru
  console overlay and no direct BGR888 framebuffer background copy in the
  active app path.
- FE background/parchment textures are embedded from `app-3ds/gfx/*.t3s` /
  `*.png`. The Makefile runs `tex3ds`, generates `*_t3x.h` headers, links the
  `.t3x` data, and exposes symbols such as `fe_forest_top_legend_t3x`.
- At startup, `main.c` initializes gfx and calls `app_renderer_c2d_init()`.
  The renderer loads the embedded textures with `C2D_SpriteSheetLoadFromMem()`,
  fetches the first image from each sheet, and draws top/bottom
  parchment/background images with `C2D_DrawImageAt()`.
- Text is drawn from backend strings with `C2D_DrawText()` through the local
  renderer helper. The app uses Citro2D system-font text, `C2D_TextBuf`
  storage, and fixed review-body windowing for the readable parchment text.
  `app_text` now wraps on spaces when possible and only hard-wraps inside a
  word when the word is longer than the available row.
- Review text now crosses the backend/display boundary through
  `app_flashcard_text_view_build()`. That contract owns copied title, front,
  back, display, progress, status, help, and footer strings plus simple state
  flags only. It does not expose geometry or pre-wrapped visible text;
  `app_shell` owns the transient scroll offset and tells the renderer whether
  the current scroll surface is the prompt or revealed answer.
- Review layout now uses the top parchment for the front/prompt and the bottom
  parchment for the revealed back/answer. The bottom body shows the help legend
  only when help is explicitly visible before reveal; reveal closes that help
  surface so answer text has a predictable section.
- `docs/3ds-coding-fundamentals.md` captures the local 3DS coding baseline:
  one active renderer owns pixels, app contracts pass strings/state only,
  physical 3DS keys are translated once, generated assets and renderer
  constants must stay tied together by tests/evidence, and runtime fingerprints
  should prove the active draw path before visual tuning.
- `app_screen_model` owns the aggregate renderer-facing handoff for the current
  app mode. It builds the review, deck selector, daily-limit settings, and
  confirmation string contracts from app/backend state, carries separate
  front/question and answer review scroll offsets, and intentionally exposes no
  geometry, colors,
  textures, fonts, or draw calls.
- `study_deck_index_scan_for_day()` now adds lightweight per-deck selector
  stats without loading full card text: card count, current-day due introduced
  count, new count, suspended count, and malformed state/count markers. The
  deck selector renders these as explicit `Due`, `New`, and `Susp` row labels,
  with `state?` when stats or compact state are unsafe.
- `study_deck_index_refresh_entry_for_day()` refreshes the cached stats for one
  existing deck entry. `app_shell` calls it after active deck state saves, reset
  confirmation, and day rollover so returning to the deck selector does not
  require a full SD rescan to show current due/new/held counts.
- `app_renderer_c2d` receives only `app_screen_model`. It owns render targets,
  text buffers, embedded FE assets, colors, coordinates, review body
  wrapping/windowing, and all Citro2D/Citro3D calls. Its public header exposes
  an opaque stack-allocatable handle plus functions; Citro2D headers and types
  stay private to `app_renderer_c2d.c`.
- `app_shell` owns app workflow state and orchestration: backend/deck/settings
  state, root session diagnostics, active deck paths, app mode, help/scroll
  state, startup scan, battery polling policy, local-day rollover, session and
  state-save flushes, and per-mode workflow reducer dispatch. It builds
  `app_screen_model` for the renderer and supplies raw review text to the
  renderer for max-scroll calculation. It does not call `hid*`, wait APIs,
  Citro2D/Citro3D draw APIs, or physical key mapping.
- The active theme verifier now checks the renderer/backend contract across the
  whole clean app slice: public non-renderer headers must not expose
  Citro2D/Citro3D types or display geometry constants, non-renderer app/study
  modules must not call draw APIs, `study_*` backend modules must not include app display
  contracts/geometry, and the renderer must not include, call, or forward
  declare backend modules. The deck-selector visible-row count has been moved
  out of `app_deck_select_contract.h` and is private to the contract/action
  implementation files.
- `study_backend` owns deck TSV loading, card front/back strings, answer
  visibility, card index, total and current-day rating/introduced counters,
  single-rating undo state, day rollover for daily limits, and the compact
  clean-shell `state.tsv` load/save/delete format, including its `.tmp` and
  `.bak` recovery artifacts. It does not include or call any renderer or 3DS
  key APIs.
- `study_controls` owns the platform-neutral control contract. It accepts
  logical buttons and returns one action. Mixed button chords return
  `STUDY_CONTROL_ACTION_NONE`, so ambiguous rating/command chords are inert.
  It also exposes the clean-shell held-input helpers used by `main.c`: held
  context turns a command pressed while another button is held into a rejected
  chord, and a pure frame-counted repeat helper emits only a single held
  direction from the caller-provided repeat mask.
- `app_input_policy` owns the app-shell repeat-input mask by screen kind. Deck
  select repeats Up/Down/Left/Right plus L/R page movement, review repeats
  Up/Down, study settings repeat Left/Right, and confirmation screens repeat
  nothing. This keeps
  destructive confirmation actions one-shot even while held-input repeat is
  enabled elsewhere.
- `app_input_frame` owns app-shell frame input assembly after 3DS keys have
  been translated to logical `study_controls` buttons. It combines button-down
  edges with delayed held-repeat output, preserves held context so mixed
  button chords stay inert, and reports when the loop should use vblank waits
  for a single held repeatable direction. It does not call `hid*` APIs, draw,
  or touch backend/session state.
- `app_mode` owns app-shell mode classification and maps modes to screen-model
  kind, confirmation contract kind, and confirmation action kind. This keeps
  suspend/restore/reset/exit confirmation surfaces aligned without leaving the
  mapping as untested static helpers in `main.c`.
- `app_review_action` owns the app-shell review action reducer. It applies
  reveal, rating, undo, scroll, and exit side effects from control actions,
  resets/clamps transient scroll state, and returns state/log dirty flags and
  review-log metadata without drawing anything. It also clears the exit-request
  output before interpreting non-exit actions so callers cannot leak stale exit
  intent through normal review actions.
- `app_review_flow` owns review-screen workflow orchestration above the pure
  reducer. It handles hidden-answer help toggling, study-settings entry,
  undo/reset/deck/exit/suspend/restore mode transitions, unavailable-action
  status text with warning context, new/review daily-limit gates, answer-shown
  session diagnostics with caller-provided timestamp/day, and state/log dirty
  handoff from `app_review_action`. It clears the exit-request output before
  handling direct help/settings/deck/status transitions as well. `app_shell`
  passes in renderer-derived max-scroll capacity and owns the resulting
  persistence calls.
- `app_settings_action` owns the app-shell study-settings editor reducer. It
  rejects mixed button chords, moves the selected settings field, cycles
  preset values or learning mode, exposes save/cancel/exit requests, and
  reports `no changes` / `unsaved changes` edit status without touching files
  or renderer geometry.
- `app_settings_flow` owns study-settings screen workflow orchestration
  above the pure reducer. It applies save/cancel/exit/update actions, preserves
  warning context for status updates, opens exit confirmation for unsaved
  settings exits, delegates persistence to `app_settings_save`, and forwards
  caller-provided timestamp/day plus low-battery save suffix state. If
  `settings.tsv` cannot be written, it keeps the app in settings mode with the
  draft limits intact so the user can retry or cancel explicitly. `app_shell`
  still owns when root `session.tsv` is flushed.
- `app_settings_save` owns study-settings save side effects. It writes
  `settings.tsv`, copies the draft into active settings on success, preserves
  warning context on `Settings saved`, appends the low-battery save suffix when
  needed, and records `settings_saved` in root session diagnostics with the
  caller-provided timestamp/day.
- `app_confirm_action` owns the app-shell confirmation-screen input reducer. It
  rejects mixed button chords, maps `START` to nested exit confirmation only
  from non-exit confirmations, maps `B`/`SELECT` to cancel, maps `X` to
  confirm suspend/restore/reset, maps `A` to confirm exit, and owns cancel
  status labels without touching files, backend state, session counters, or
  renderer geometry.
- `app_confirm_flow` owns confirmation-screen workflow side effects. It applies
  cancel/open-exit transitions, confirmed exit session diagnostics, confirmed
  reset session diagnostics, confirmed suspend/restore backend mutations,
  state/log dirty flags, review-scroll reset requests, warning-context
  preservation, and low-battery reset suffix handoff with caller-provided
  timestamp/day. It clears its output side-effect flags before interpreting an
  action so cancel/open-exit transitions cannot leak stale log or exit requests.
  It does not draw and does not expose renderer geometry.
- `app_reset_action` owns confirmed reset side effects. It resets in-memory
  progress only after `state.tsv` plus `.tmp`/`.bak` cleanup succeeds. If
  state cleanup fails, the existing in-memory review progress is preserved and
  the reset returns failure. If only `review-log.tsv` cleanup fails, progress
  remains reset, the status reports `Progress reset; log kept`, warning context
  and low-battery suffixes are preserved, and confirmation flow records reset
  session diagnostics. Session counters remain owned by `app_shell` and the
  confirmation flow.
- `app_state_save` owns saved-action persistence after review/settings reducers
  mark state dirty. It saves compact `state.tsv`, appends compact
  `review-log.tsv` rows, maps log events to root session counters, records the
  caller-provided local timestamp/day, reports `Saved; log skipped` without
  rolling back accepted state when the diagnostic log append fails or is skipped
  because the battery is low, appends the low-battery save suffix when needed,
  and can report whether the primary state write failed. `app_shell` owns when
  to call it and now uses that outcome to roll back failed review and
  suspend/restore mutations to the pre-action backend/session snapshot while
  keeping `Save failed` visible. A successful state save now reports handled
  work even when no low-battery suffix was appended, so the shell can redraw and
  refresh stats from ordinary successful saves.
- `app_session_save` owns root `session.tsv` save policy for the app shell. It
  no-ops when the caller says session state is clean, writes through
  `study_session_save_tsv` when dirty, preserves existing status on success,
  reports `Session save failed` through the backend on failure, and exposes an
  outcome enum so `app_shell` can keep `session_dirty` set after a failed write.
  Root session dirty state now lives on `struct app_shell`, not on the stack, so
  startup scan diagnostics and later workflow diagnostics retry on subsequent
  frames instead of being dropped after a transient `session.tsv` write failure.
  Confirmed exit is gated on this flush: if the terminal `exit_confirmed` event
  cannot be saved, the shell stays on exit confirmation with `Session save
  failed` visible instead of quitting without post-run artifact evidence. The
  failed terminal event is rolled back in memory before returning to the
  confirmation screen, so canceling afterward cannot later persist a false
  `exit_confirmed` event.
- `app_day_rollover_flow` owns app-shell local-day rollover orchestration. It
  updates the root session day, rolls backend daily-limit counters and
  completed-today state, delegates rollover persistence to `app_state_save`
  without review-log/session saved-action events, preserves caller-provided
  timestamp/day evidence, and returns a redraw signal. If the primary rollover
  `state.tsv` write fails, it restores the previous backend/session/current-day
  snapshot and leaves `Save failed` visible instead of advancing memory past
  durable state. It does not draw and does not expose renderer geometry.
- `app_battery_monitor` owns app-shell battery monitor state and PTMU sampling.
  It initializes PTMU lazily, ignores closed-shell samples, preserves the last
  good battery state after read failures, applies low-battery status feedback
  through `app_battery_status`, exposes save-warning policy through
  `app_power`, and shuts PTMU down once. It does not draw and does not expose
  renderer geometry.
- `app_battery_status` owns the app-shell battery-status transition policy. It
  maps sampled battery state to backend status text, latches the one-shot low
  warning, keeps repeated still-low samples from clearing that warning, and
  clears it only after charging/normal/unavailable evidence.
- `app_deck_select_action` owns the app-shell deck-selector input reducer. It
  rejects mixed button chords, maps `START`/`SELECT`/`A` to exit/rescan/open
  deck requests, and applies Up/Down/Left/Right plus L/R page movement through
  `app_deck_navigation` without scanning SD, opening decks, or touching session
  diagnostics.
- `app_deck_flow` owns deck-selector workflow side effects. It rescans the SD
  deck root on startup and on manual rescan while preserving the selected deck
  id when possible, loads the
  selected deck into the backend, copies active `state.tsv` / `settings.tsv` /
  `review-log.tsv` paths, applies settings and load-time day rollover,
  persists that rollover immediately, rolls the loaded backend back to the
  saved state when the primary rollover save fails, transitions deck-select
  actions into app modes, and records scan/deck-open session diagnostics with
  caller-provided timestamp/day. It does not draw and does not expose renderer
  geometry.
- `study_3ds_key_map` owns the libctru physical-key bridge from 3DS `KEY_*`
  bits to `study_controls` logical button bits. Host coverage uses
  `tests/stubs/3ds.h` to guard D-pad/Circle Pad aliases and physical chords.
- `study_deck_index` owns clean-shell deck discovery. It scans
  `sdmc:/3ds/anki3ds/decks`, keeps folders with portable ids and readable
  `cards.tsv`, loads an optional `deck.json` name, builds `cards.tsv` and
  `state.tsv` / `settings.tsv` / `review-log.tsv` paths, sorts entries by
  folder id, and reports ignored entries.
- `study_settings` owns per-deck `settings.tsv` parsing/saving and the
  daily-limit preset ladder. Missing settings load defaults; malformed
  settings load defaults and show `Settings ignored`.
- `study_review_log` owns compact clean-shell `review-log.tsv` append/delete.
  It appends only after state saves succeed, repairs partial final rows,
  recovers valid temp/backup artifacts before appending, and stops when the
  next row would exceed 262144 bytes.
- `study_session` owns root `sdmc:/3ds/anki3ds/session.tsv` diagnostics. It
  loads prior complete diagnostics when present, preserves saved-action
  counters across relaunches, increments `launch_count` on boot, writes
  verifier-shaped header/footer rows through `.tmp` / `.bak`, and records boot,
  scan, deck-open, answer, saved-action, settings, reset, and confirmed-exit
  events. The loader rejects impossible session evidence, including deck-open
  counters that do not match review/summary/load-error counters, scan events
  without completed scan state, and confirmed-exit flags that do not match the
  final event. In-app day rollover updates `current_day` and `updated_at`
  without replacing the last recorded user-visible event, so root diagnostics
  stay aligned with compact deck `progress_day` evidence.
- Startup now opens a deck selector instead of hard-wiring `sample`. The top
  screen shows the selectable deck names on the FE parchment; `A` opens the
  selected deck, `Up`/`Down` moves the selection with wraparound,
  `Left`/`Right` or `L`/`R` pages by the visible row count, `SELECT` rescans SD,
  and `START` exits. Visible deck-window math is isolated in
  `app_deck_navigation`; display-facing selector strings are isolated in
  `app_deck_select_contract`. Both are covered by host tests.
- Opening a deck loads that deck's `cards.tsv` into fixed backend storage,
  restores that deck's `state.tsv` if present and valid, and loads that deck's
  `settings.tsv` daily limits. The state and settings loaders can recover
  valid `.tmp` or `.bak` artifacts after interrupted saves. Missing state or
  settings are normal; malformed or mismatched state leaves the backend fresh
  and shows a bad-state status. Malformed settings keep default limits and show
  `Settings ignored`. Load failures show a review/load-error surface that can
  return to the selector with `SELECT`.
- Successful rating, undo, confirmed suspend, and confirmed restore actions
  save the compact `state.tsv` through `state.tsv.tmp` and `state.tsv.bak`.
  After a successful state save, the app appends a compact diagnostic
  `review-log.tsv` row for the accepted action unless the battery is low. If
  the log append fails or the optional low-battery skip applies, the accepted
  study action remains saved and the status reports `Saved; log skipped`. The
  same accepted saved actions increment the root `session.tsv`
  counters and cards whose answers have been revealed for `new_limit`
  enforcement.
  Confirmed reset clears in-memory progress and removes `state.tsv`,
  `state.tsv.tmp`, `state.tsv.bak`, `review-log.tsv`, `review-log.tsv.tmp`, and
  `review-log.tsv.bak` for the selected deck. Scroll state remains transient
  and is not persisted.
- `X` opens the study-settings editor while the answer is hidden. `Up`/`Down`
  select `new_limit`, `review_limit`, or learning mode; `Left`/`Right` move
  through presets or toggle learning mode; `X` saves via `settings.tsv.tmp`
  and `settings.tsv.bak`; `B` or `SELECT` cancels. The editor status reports
  `no changes` while the draft matches the saved settings, reports
  `unsaved changes` after edits, and shows `Exit loses unsaved settings`
  before an exit confirmation would discard edits.
  The settings loader can recover a valid temp or backup artifact after an
  interrupted save. When the answer is visible, `X` remains the Good rating.
  The active clean shell enforces `review_limit` with the current-day reviewed
  counter and `new_limit` with the current-day introduced counter for cards
  that have not yet been introduced in the compact saved state. The current day
  is stored as a device-local calendar day count since 1970-01-01 through the
  small host-tested `app_time` helper, avoiding UTC evening rollover for daily
  limits. This is still separate from the later full due/new scheduler.
  Display-facing settings strings are isolated in `app_settings_contract`, and
  settings edit input and save side effects are isolated in
  `app_settings_action` and `app_settings_save`; both are covered by host
  tests.
- `app_battery_monitor` samples PTMU at startup, then `app_shell` uses
  `app_power` policy to retry failures after one minute and otherwise poll at
  most once every ten minutes. If the system clock moves backward far enough
  that the scheduled poll is stranded more than one normal interval in the
  future, `app_power_battery_poll_is_due()` treats the poll as due so the app
  resamples and reschedules. Battery level/charge are read only when the
  shell reports open. A valid low and not-charging sample sets the backend
  status string to
  `Battery low; charge soon` once per low-battery episode. A later still-low
  sample leaves that warning intact; charging, recovered, or unavailable
  samples can clear it to `Battery ok` or `Battery status unavailable`.
- When the screen is unchanged, the main loop waits for HID input with the
  adaptive `app_power` idle backoff instead of scanning controls every frame.
- The TSV loader keeps the old deck escape behavior: single `\n`, `\t`, and
  `\\` sequences decode; doubled slash text such as `\\n` stays visible as
  backslash-n.
- `main.c` is now the platform/render entry: it initializes gfx, initializes
  `app_renderer_c2d`, reads HID state, delegates 3DS-key translation to
  `study_3ds_key_map`, uses `app_input_frame` for logical frame input, waits
  with `app_power` idle backoff or vblank held-repeat pacing, asks
  `app_shell` to build an `app_screen_model`, and passes that model to
  `app_renderer_c2d_draw()`. `app_shell` owns app-mode workflow reducers and
  persistence. The backend must stay view-geometry-free.
- `R` opens a suspend confirmation when a card is active. `X` confirms,
  `B` or `SELECT` cancels. Suspended cards are skipped and persisted by card
  index. When no active cards remain and some cards are suspended, `R` opens a
  restore-all confirmation with the same `X` confirm and `B`/`SELECT` cancel
  shape.
- On the deck selector only, `L` and `R` page through the deck list as shoulder
  aliases for Left/Right. In review, `L` remains undo and `R` remains
  suspend/restore.
- `A` on a completed deck starts another review pass over introduced,
  non-suspended cards by clearing only same-day completion/review counters.
  Lifetime review counts, introduced-card state, suspensions, and settings stay
  intact.
- `Y` opens a reset-progress confirmation only while the answer is not visible
  or no active card remains. `X` confirms the reset; `B` or `SELECT` cancels.
  When the answer is visible, `Y` remains the Easy rating and does not open
  reset.
- `A` reveals the answer, then rates Again once the answer is visible.
- `A`, `B`, `X`, and `Y` rate Again, Hard, Good, and Easy when the answer is
  visible. Exactly one rating button must be pressed.
- `L` undoes the most recent rating when undo is available.
- `Up`/`Down` scroll the current question/answer text.
- `SELECT` returns from review, completion, or load-error surfaces to the deck
  selector; on the selector it rescans SD.
- `START` opens an exit confirmation. `A` confirms and writes
  `last_event=exit_confirmed`; `B` or `SELECT` cancels.

Latest visual checkpoint: the active app has moved past the console-text
readability experiment to a single Citro2D composition path. Background,
parchment, and live app text are all drawn through Citro2D from embedded
texture data and backend strings.

Latest parchment update: `tools/import_fe_theme_assets.py` now uses the CC0
OpenGameArt old parchment PNG at
`assets/fe-themes/external/opengameart/old-parchment-paper/parchment_alpha.png`
as the active dialogue panel source. It falls back to the parchment GUI asset,
then to the downloaded map-warning reference, then to generated chrome. The
active Forest Citro2D PNGs in `app-3ds/gfx/` are synchronized from the
generated `forest_*_layer_legend_*.png` previews by `make
sync-app-fe-ui-assets`, and the top-level `app-3ds` target depends on that
sync. `app-3ds/Makefile` also makes the generated `.t3x` files depend on their
PNG inputs, so direct `make -C app-3ds` rebuilds textures when the embedded
PNGs change. `tests/test_fe_theme_assets.py` guards the old-parchment source,
fallback order, center readability, and embedded PNG parity against the
generated Forest legend previews.

Verification status for this Citro2D checkpoint: `make -C app-3ds clean`,
`make -C app-3ds`, `make test-host`, `make test-tools`, `python3
tools/verify_app_theme.py`, and `git diff --check` passed on 2026-06-05 after
the centralized Citro2D verifier update. After adding the clean-shell
`app_power` slice, `make -C app-3ds`, `make test-host`, `make test-tools`,
`python3 tools/verify_app_theme.py`, and `git diff --check` passed again.
After adding the clean-shell deck selector and `study_deck_index`, the same app
build, host-test, tool-test, theme-verifier, and diff-check pass completed
again.
After adding clean-shell suspend/restore with compact suspended-index state,
the same app build, host-test, tool-test, theme-verifier, and diff-check pass
completed again.
After adding clean-shell reset-progress confirmation and compact state artifact
deletion, the same app build, host-test, tool-test, theme-verifier, and
diff-check pass completed again.
After finishing clean-shell daily-limit settings, adding state/settings tmp/bak
recovery, adding compact `review-log.tsv` event appends/deletes, adding root
`session.tsv` diagnostics with exit confirmation, and adding the downloaded
dialogue-reference guard, `make -C app-3ds`, `make test-host`, `make
test-tools`, and `python3 tools/verify_app_theme.py` passed on 2026-06-06.
`tools/verify_m7_artifacts.py` now accepts both the current compact clean-shell
`state.tsv`/six-field `review-log.tsv` artifacts and the fuller framed
scheduler state/21-field log artifacts. The compact path is checkpoint evidence
for the current clean Citro2D shell, not proof that the later full scheduler is
done.
After adding compact introduced-card state and clean-shell `new_limit`
enforcement for answer reveal, `make test-host`, `make test-tools`, `make -C
app-3ds`, and `git diff --check` passed on 2026-06-06. The app saves
`introduced_count` / `introduced_index` rows after a successful answer reveal
and keeps review-log rows scoped to rating, undo, suspend, and restore events.
The daily-limit predicates are pure `study_settings` helpers:
`study_settings_new_limit_blocks_reveal()` and
`study_settings_review_limit_blocks_rating()`, with host coverage in
`tests/test_study_settings.c`; `main.c` delegates to those helpers.
After the day-aware daily-limit pass, compact `state.tsv` also stores
`progress_day`, `introduced_today_count`, and `reviewed_today_count`. The app
uses those per-day counters for `new_limit` / `review_limit`, resets only those
counters on day rollover, preserves lifetime introduced/reviewed progress, and
saves the rollover when a deck is active. The backend now also persists
`completed_today_count` / `completed_today_index` rows so same-day reloads skip
already completed cards. A true day rollover clears that completed-today bitmap
and rebuilds the active queue with due introduced cards before new cards, so
reviews do not consume `new_limit`; first-load day establishment for legacy
dayless state does not rewind. First-load migration now also treats stamping an
existing-progress deck with `progress_day` as dirty, so legacy compact states
without day fields do not keep reusing lifetime counts as today's counts across
launches. The M7 artifact verifier compares compact `progress_day` to root
`session.tsv current_day` when compact state evidence is present. Focused
`make test-host`, `python3 -m unittest
tests/test_verify_m7_artifacts.py`, and `make -C app-3ds` passed after the
initial slice. The broader non-GUI gate also passed on 2026-06-06: `make
test`, `make -C app-3ds`, `python3 tools/verify_app_theme.py`, `git diff
--check`, and `make verify-m7-preflight`. That preflight staged the current
app, FE assets, and fresh tracked `limits-demo`/`sample` decks into
local/package SD roots and Azahar SDMC, then verified the Azahar control
profile. It did not launch Azahar or prove the manual M7 interaction checklist.
After restoring the clean-shell `app_time` helper, `main.c` now drives root
session days and daily-limit rollover from device-local calendar days instead
of `timestamp / 86400`; `tests/test_app_time.c`, `make test-host`,
`make -C app-3ds`, and `git diff --check` passed after this slice. The helper
covers leap dates, max-day clamping, invalid timestamps, and the
UTC-evening/local-day distinction. The broader `make test`, `python3
tools/verify_app_theme.py`, `git diff --check`, and `make verify-m7-preflight`
gate also passed after this local-day slice, restaging the current app and
fresh tracked samples into local/package SD roots and Azahar SDMC without
launching Azahar.
After tightening rollover diagnostics, `study_session_rollover_day()` updates
root `session.tsv` day evidence while preserving the last event name, and
`main.c` saves that rollover-only session change before mode-specific early
continues can drop it. Focused `tests/test_study_session.c`, direct
`make -C app-3ds`, and `git diff --check` passed after this slice.
After adding repeat daily review to the clean shell, day rollover rewinds into
the due-card queue while keeping lifetime counters and suspended indexes.
Follow-up queue work made that stronger: `study_backend` now saves
completed-today card indexes, skips them on same-day reload, and offers due
introduced cards before any new card even when introduced cards are sparse in
the deck. First-load day establishment for legacy dayless compact state still
only stamps `progress_day` and does not rewind. Focused
`tests/test_study_backend.c`, `tests/test_clean_shell_daily_use.c`, and
`tests/test_verify_m7_artifacts.py` passed after this queue-state slice. The
broader `make test`, `make -C app-3ds`, `python3 tools/verify_app_theme.py`,
`git diff --check`, and `make verify-m7-preflight` gates also passed, restaging
fresh tracked samples into local/package SD roots and Azahar SDMC without
launching Azahar.
After extracting review action side effects from `main.c`, `app_review_action`
now owns the pure reducer for scroll movement and clamped scroll no-ops,
scroll reset on reveal/rating/undo, state/log dirty flags, review-log rating
metadata, and exit signaling. The theme verifier now accepts the clean
app-shell transition path through `app_review_action_apply()` without allowing
`study_backend.c` itself to satisfy renderer/app-shell checks. Focused
`tests/test_app_review_action.c`,
`python3 -m unittest tests/test_verify_app_theme.py`, `make test`, `make -C
app-3ds`, `python3 tools/verify_app_theme.py`, `git diff --check`, and `make
verify-m7-preflight` passed after this extraction. The preflight restaged the
current app, FE assets, and fresh tracked `limits-demo`/`sample` decks into
local/package SD roots and Azahar SDMC without launching Azahar.
After automating embedded FE UI texture sync, `make app-3ds`, direct
`make -C app-3ds`, and `python3 -m unittest tests/test_fe_theme_assets.py`
passed on 2026-06-06. The first asset-test run was intentionally disregarded
because it ran in parallel with preview regeneration and reported skips; the
post-sync rerun completed with 21 tests and no skips.
The full `make verify-m7-preflight` gate passed on 2026-06-06: host/tool
tests, sample validation, local SD staging, package SD staging, fresh Azahar
sample staging, and Azahar key-profile validation all completed. A subsequent
`make run-emulator-m7-smoke` attempt is blocked by macOS Accessibility denying
`System Events` keystrokes. The smoke target now runs
`check-azahar-smoke-automation` first, so it fails before restaging samples or
relaunching Azahar when that permission is missing. Once the driver succeeds,
the target runs `verify-azahar-m7-smoke-artifacts` automatically. No post-run
artifact acceptance evidence exists from the blocked attempt because no study
actions were sent.
After extracting the active libctru key bridge into `study_3ds_key_map`,
`make test-host`, direct `cc` coverage for `tests/test_study_3ds_key_map.c`,
and `make -C app-3ds` passed on 2026-06-06. The active app no longer keeps the
physical key map as an untested static helper in `main.c`.
After moving reset state-artifact cleanup into `study_backend_delete_state_tsv`,
focused `cc` coverage for `tests/test_study_backend.c`, `make test-host`, and
`make -C app-3ds` passed on 2026-06-06. Reset now delegates `state.tsv`,
`state.tsv.tmp`, and `state.tsv.bak` cleanup to the backend, matching the
existing `study_review_log_delete()` boundary for review-log artifacts.
After wiring the clean-shell Citro2D app path to append the `; batt low` suffix
on successful rating/undo/suspend/restore state saves, reset success, and
daily-limit saves, `make test`, `make -C app-3ds`, and
`python3 tools/verify_app_theme.py` passed on 2026-06-06.
After extracting that suffix formatting into `app_status_text`, focused `cc`
coverage for `tests/test_app_status_text.c`, `make test`, `make -C app-3ds`,
and `python3 tools/verify_app_theme.py` passed on 2026-06-06. The helper
guards idempotence and keeps the low-battery suffix visible when truncating a
long save status.
After extracting deck selector movement/windowing into `app_deck_navigation`,
focused `cc` coverage for `tests/test_app_deck_navigation.c`, `make test`,
`make -C app-3ds`, and `python3 tools/verify_app_theme.py` passed on
2026-06-06. The helper covers empty/single-deck safety, Up/Down wraparound,
Left/Right page movement, and clamped visible-window start.
After extracting deck selector display strings into `app_deck_select_contract`,
focused `cc` coverage for `tests/test_app_deck_select_contract.c`,
`make test-host`, and `make -C app-3ds` passed on 2026-06-06. The contract
owns title/list/meta/status/control/footer strings for the selector, including
empty-list messaging, visible-window list text, scan status pluralization, and
out-of-range selection clamping for meta text.
After extracting study-settings display strings into `app_settings_contract`,
focused `cc` coverage for `tests/test_app_settings_contract.c`,
`make test-host`, and `make -C app-3ds` passed on 2026-06-06. The contract
owns title/body/footer/status/control/help strings for the settings screen,
including `All` formatting, default settings text, and invalid selected-index
clamping.
After extracting study-settings input into `app_settings_action`, focused
`cc` coverage for `tests/test_app_settings_action.c`, `make test-host`, `make
test`, direct `make -C app-3ds`, `python3 tools/verify_app_theme.py`, and `git
diff --check` passed on 2026-06-06. `make verify-m7-preflight` also passed
after the extraction, restaging the current app, FE assets, and fresh tracked
`limits-demo`/`sample` decks into local/package SD roots and Azahar SDMC
without launching Azahar. The reducer covers chord rejection, selected-field
movement, preset value cycling, save/cancel/exit requests without selection or
draft side effects, ignored-input no-op behavior, and `no changes` /
`unsaved changes` edit status.
After extracting suspend/restore/reset/exit confirmation display strings into
`app_confirm_contract`, focused `cc` coverage for
`tests/test_app_confirm_contract.c`, `make test-host`, and `make -C app-3ds`
passed on 2026-06-06. The contract owns status/prompt/footer strings for
confirmation surfaces, including restore-card pluralization and null-status
handling.
After extracting suspend/restore/reset/exit confirmation input into
`app_confirm_action`, focused `cc` coverage for
`tests/test_app_confirm_action.c`, `make test-host`, `make test`, direct
`make -C app-3ds`, `python3 tools/verify_app_theme.py`, and `git diff --check`
passed on 2026-06-06. `make verify-m7-preflight` also passed after the
extraction, restaging the current app, FE assets, and fresh tracked
`limits-demo`/`sample` decks into local/package SD roots and Azahar SDMC
without launching Azahar. The reducer covers chord rejection, cancel buttons,
`X` confirmation for suspend/restore/reset, `A` confirmation for exit, nested
exit opening from non-exit confirmations, inert `START` on the exit
confirmation itself, invalid confirmation kinds failing closed for
confirm/open-exit actions, and cancel status labels.
After extracting battery status transitions into `app_battery_status`, focused
`cc` coverage for `tests/test_app_battery_status.c`, `make test-host`, `make
test`, direct `make -C app-3ds`, `python3 tools/verify_app_theme.py`, and `git
diff --check` passed on 2026-06-06. `make verify-m7-preflight` also passed
after the extraction, restaging the current app, FE assets, and fresh tracked
`limits-demo`/`sample` decks into local/package SD roots and Azahar SDMC
without launching Azahar. The policy covers first low-sample announcement,
repeated still-low samples preserving the low warning, clearing the warning
after charging/recovery/unavailable evidence, closed-shell samples staying
quiet, and non-battery status text staying intact on normal samples.
After extracting deck-selector input into `app_deck_select_action`, focused
`cc` coverage for `tests/test_app_deck_select_action.c`, `make test-host`,
`make test`, direct `make -C app-3ds`, `python3 tools/verify_app_theme.py`, and
`git diff --check` passed on 2026-06-06. `make verify-m7-preflight` also passed
after the extraction, restaging the current app, FE assets, and fresh tracked
`limits-demo`/`sample` decks into local/package SD roots and Azahar SDMC
without launching Azahar. The reducer covers mixed-button chord rejection,
exit/rescan/open-deck requests, Up/Down movement, Left/Right paging, and no-op
navigation/open on an empty deck list while keeping SD scanning, deck opening,
and session diagnostics in `main.c`.
After dispatching parallel renderer/backend audit agents on 2026-06-06, the
backend audit confirmed the active `study_*` modules expose strings/state only
and do not call Citro2D/Citro3D. The first renderer audit found readability was
still concentrated in the then-inline draw helpers: baked FE
background/parchment first, live Citro2D default-font strings second, with
review-body double wrapping and small single-pass dark text. The follow-up
renderer-only patch keeps backend/card logic unchanged, draws review
title/body/meta through a one-pixel parchment-highlight shadow, moves the
review body to `80,73`, raises body scale to `0.60`, and relies on `app_text`
35-column windowing instead of Citro2D word wrap for the body.
`make -C app-3ds`, `make test-host`, `python3 tools/verify_app_theme.py`, and
`make verify-m7-preflight` passed after the patch; the preflight restaged the
current app, FE assets, and fresh tracked `limits-demo`/`sample` decks into
local/package SD roots and Azahar SDMC without launching Azahar.
After adding the aggregate `app_screen_model` handoff on 2026-06-06, `main.c`
now calls `app_screen_model_build()` for the current app mode and passes the
result to `app_renderer_c2d_draw()`. Focused `cc` coverage for
`tests/test_app_screen_model.c`, `make test-host`, direct `make -C app-3ds`,
`python3 tools/verify_app_theme.py`, and `python3 -m unittest
tests/test_verify_app_theme.py` passed after the screen-model slice. The theme
verifier follows `app_screen_model.{h,c}` as a clean-shell boundary and has a
regression fixture for rendering `screen_model.flashcard` while
`study_backend_build_view()` stays outside the renderer. After extracting the
Citro2D draw path into `app_renderer_c2d.{h,c}`, the renderer public header was
made opaque, `main.c` stopped importing Citro2D types/geometry, and
`tools/verify_app_theme.py` gained regressions for direct Citro2D in `main.c`,
Citro2D leakage from the renderer header, renderer/backend reach-through, and
missing renderer display/status text draws. `make test`, direct `make -C
app-3ds`, `python3 tools/verify_app_theme.py`, `git diff --check`, and `make
verify-m7-preflight` passed after those changes. The preflight restaged the
current app, FE assets, and fresh tracked `limits-demo`/`sample` decks into
local/package SD roots and Azahar SDMC without launching Azahar.
After the review/deck/settings/confirm string-contract split, `make
verify-m7-preflight` passed on 2026-06-06. That rerun covered host/tool tests,
sample-deck validation, FE theme generation, app build/package checks, local SD
staging, package SD verification, fresh Azahar sample staging, and Azahar
control-profile verification for the current app. It did not launch Azahar or
prove the manual M7 interaction checklist.
After adding `tests/test_clean_shell_daily_use.c`, direct focused `cc`
coverage, `make test-host`, `make test`, `make -C app-3ds`,
`python3 tools/verify_app_theme.py`, and `git diff --check` passed on
2026-06-06. The integration test drives the current clean-shell modules across
two decks with scan/load, reveal/rate/undo, settings save/reload,
suspend/restore, compact state saves, review-log appends, reset cleanup that
preserves settings, and root session diagnostics ending in confirmed exit.
After the parallel renderer/backend split pass, `app_flashcard_text_view_build()`
became the active review text contract. It outputs copied raw strings only;
`main.c` now owns review body windowing and scroll metadata formatting through
`app_text`. `make test`, `make -C app-3ds`, `python3
tools/verify_app_theme.py`, `git diff --check`, and `make verify-m7-preflight`
passed on 2026-06-06 after that change. The preflight staged the current app,
FE assets, and fresh tracked `limits-demo`/`sample` decks into local/package
SD roots and Azahar SDMC. Manual M7 interaction acceptance is still pending.
An attempted `make run-emulator-m7-smoke` on 2026-06-06 stopped at the
pre-launch macOS Accessibility check because `osascript` was not allowed to
send keystrokes. During that audit, the app was corrected to match the smoke
driver and checkpoint docs: exit confirmation now uses `A`, while `X` remains
the deliberate confirm key for suspend/restore/reset. The visible exit prompt
now says `A: confirm`; `tests/test_app_confirm_action.c` and
`tests/test_app_confirm_contract.c` cover this split. `make test-host`, direct
`make -C app-3ds`, `git diff --check`, and `make verify-m7-preflight` passed
after the correction. The preflight restaged the corrected app into Azahar
SDMC. Manual M7 interaction acceptance is still pending.
After tightening warning-context status behavior on 2026-06-06,
`app_status_text` now has tested helpers for warning-context merging and for
composing warning context with the low-battery save suffix in one string-policy
call. `main.c` uses those helpers so successful reveal/rating/undo,
suspend/restore/reset, settings open/update/cancel/save, confirmation cancel,
and daily-limit-blocked feedback can retain active warning context such as
`Settings ignored`, `New limit reached`, `Review limit reached`, or
`; batt low` instead of replacing it with plain transient status. Focused
`make test-host`, direct `make -C app-3ds`, broader `make test`,
`git diff --check`, and `make verify-m7-preflight` passed after the latest
composition helper change. The preflight restaged the current app, FE assets,
and fresh tracked `limits-demo`/`sample` decks into local/package SD roots and
Azahar SDMC without launching Azahar.

## Historical Goal

Advance `anki3ds` toward the M7 daily-use MVP:

- safer key mapping and chord-gated destructive actions
- low-battery-conscious polling/status behavior
- core study workflows: multi-deck selection, review, undo, suspend/restore,
  daily-limit editing, reset, relaunch persistence, and post-run evidence
- current visual subtask: Fire Emblem style backgrounds/frame/text in Azahar
  without console color clashes

Do not mark the goal complete until the full M7 acceptance in
`CHECKPOINTS.md` is proven by manual emulator or hardware use and the artifact
verifier accepts the tested SD root.

## Pause Checkpoint

Paused on 2026-06-05 after the display/function split reached a stable
compile/test checkpoint. No final M7 acceptance claim has been made.

Current working checkpoint:

- Core study logic is in the clean-shell `study_*` modules. Backend code
  outputs state and strings only.
- Review display text flows through `app_flashcard_text_view_build()`; `main.c`
  builds `app_screen_model`, then `app_renderer_c2d` draws embedded FE
  parchment/background textures and live strings through Citro2D and owns
  wrapping/scroll-window presentation.
- The stale direct-framebuffer FE renderer and orphaned view-model test were
  removed after subagent review.
- `main.c` still owns app state, input, persistence, battery/day polling, and
  screen-model construction. It no longer calls Citro2D/Citro3D directly.
- Latest non-GUI verification after the cleanup passed for `make test`,
  `make -C app-3ds`, `python3 tools/verify_app_theme.py`, `git diff --check`,
  and `make verify-m7-preflight` after the opaque `app_renderer_c2d` boundary
  hardening. Full M7 acceptance still requires Azahar/manual verification.
- No fresh Azahar visual capture or manual M7 study-flow pass was run after
  this cleanup.

Resume by reading this file first, then prefer either manual M7 acceptance from
fresh samples or the next display split slice. Do not revert unknown dirty
worktree changes.

## Historical Visual State Before Citro2D Shell

This section describes the pre-clean-slate display path. The current active
path is the embedded Forest Citro2D shell in the checkpoint section above; do
not use this historical Plain/FE theme-cycle path as current implementation
guidance.

The older main app had five session themes:

- `Plain`
- `Amber`
- `Forest`
- `Ruby`
- `Chalk`

That older path used `app_initial_theme()` in `app-3ds/source/main.c`, started
on `Plain`, and made FE themes available through the help screen `X` theme
cycle. The current clean-slate app starts directly in the embedded FE shell
through `app_renderer_c2d`.

The app expects final FE framebuffer files on SD at:

```text
sdmc:/3ds/anki3ds/fe-themes/fe_bg_<theme>_top_400x240_bgr888_fb.bin
sdmc:/3ds/anki3ds/fe-themes/fe_bg_<theme>_bottom_320x240_bgr888_fb.bin
```

`tools/verify_app_theme.py` plus `tests/test_verify_app_theme.py` now enforce
the active app theme/rendering invariants, including the clean FE shell,
renderer/backend boundary, and the `main.c -> app_screen_model ->
app_renderer_c2d` handoff. Older Plain startup/fallback assertions remain
historical coverage for the removed theme-cycle path.

The FE asset pipeline in `tools/import_fe_theme_assets.py` builds the final app
framebuffers as layered images:

1. dimmed FE background
2. FE map-style parchment dialogue-panel legend
3. inert card checkpoint layer
4. baked FE-style sample text

The generator uses the local FE repo at `/Users/eric/codebase/FE-Repo`.
The fixed sample labels now use the user's Ghostty terminal font first. The
current Ghostty config resolves to Hack Nerd Font Mono at size 14; the generator
uses `/Users/eric/Library/Fonts/HackNerdFontMono-Regular.ttf` through `hb-view`
at size 15 for better 3DS readability. The vendored FE8 dialogue font map at
`assets/fe-themes/font/fe8_vanilla_fontMap.png`, Checkmate through `hb-view`,
and the built-in pixel font are fallbacks.
Layer previews are written under `build/fe-theme-previews/`; final main-app
framebuffers are written under `build/fe-theme-framebuffers/`. The generator
also writes `build/fe-theme-framebuffers/fe_font_review_8x14_alpha.bin`, a
10,640-byte printable ASCII alpha atlas for live FE review-front text.

Latest visual checkpoint: the exact vanilla map-warning parchment asset was not
found in the local FE repo, so the generator now recreates that style
procedurally. The isolated viewer starts on Forest and cycles
`background -> parchment -> text` with Down/Up. The parchment face is now a
solid pale-yellow panel with dark olive borders, stepped corners, and edge
bevels; the previous checker texture was removed. The `card` layer remains a
no-op so the checkpoint stays focused on background, dialogue panel, and FE
font text instead of reintroducing the full-screen orange card chrome.

The main app now uses Citro2D for its active screens. `main.c` draws embedded
FE top/bottom parchment textures with `C2D_DrawImageAt()` and renders live
strings with `C2D_DrawText()`. Backend study code produces state and strings
only. `app_flashcard_text_view_build()` performs the display-facing review-card
string selection and owns copied title/front/back/display/progress/status/help/
footer strings. `main.c` owns all review-card wrapping, scroll-window, and
scroll-hint geometry. There is no active libctru console overlay and no raw
framebuffer renderer in the app binary.

## Latest Daily-Use Code Update

The current display/backend split is the clean-shell `study_*` backend plus the
`app_flashcard_contract`, `app_deck_select_contract`,
`app_settings_contract`, and `app_confirm_contract` display string contracts.
`main.c` still owns controller glue and app-mode orchestration, but active
study modules do not draw, include renderer headers, or know view geometry.
The next split step is a broader app-core/view-model extraction or manual M7
acceptance rather than another small screen-string contract.

The old scheduler/learning/card-state stack is not part of the clean-shell app
checkpoint. The active study implementation is intentionally smaller:
`study_backend` owns TSV card loading, active-card state, answer visibility,
ratings, undo, suspend/restore, reset, compact state persistence, and view
strings; `study_settings`, `study_deck_index`, `study_review_log`, and
`study_session` own their respective storage/state boundaries.

The display mechanics split is currently the `app_flashcard_contract` review
string contract, the `app_deck_select_contract` selector string contract, the
`app_settings_contract` study-settings string contract, the
`app_confirm_contract` confirmation string contract, and Citro2D drawing in
`main.c`. This is not a finished app-core/view-model extraction.
`app_flashcard_text_view_build()` is the preferred review contract entry point
for future renderer work because it outputs raw strings and leaves scroll-window
geometry in the renderer.

The physical 3DS-key regression test now covers the main M7 command surface,
not just ratings/exit/reset. `tests/test_app_controls_3ds_keys.c` verifies
translated `KEY_SELECT`, `KEY_L`, `KEY_R`, `KEY_A`, `KEY_B`, and `KEY_X`
behavior for opening actions, undo, opening suspend confirmation, choosing or
canceling actions, saving or canceling daily limits, and confirming
suspend/restore. Mixed command/navigation/face-button chords are expected to
classify as `APP_CONTROL_ACTION_NONE`.

Low-battery handling now has a one-shot proactive status warning. When PTMU
reports a valid low and not-charging sample, `app_power_battery_low_warning_due`
latches that low-battery episode and the app sets `Battery low; charge soon`.
The latch resets after charging, normal, or unavailable battery status. Existing
saved-action suffix behavior remains: successful rating, undo, suspend,
restore, reset, and daily-limit saves still append `; batt low` when the sample
is low and not charging.

## Important Commands

Build the 3DS app:

```sh
make -C app-3ds
```

Regenerate FE assets and previews:

```sh
make verify-fe-theme-assets
```

Run the focused tool/verifier suite:

```sh
make test-tools
```

Stage fresh sample decks, install FE framebuffers into Azahar SDMC, build, and
launch Azahar:

```sh
make run-emulator-fresh-samples
```

Full M7 preflight before a real manual pass:

```sh
make verify-m7-preflight
```

Post-run artifact verification after manual testing:

```sh
make verify-m7-artifacts M7_EXPECT_SETTINGS="sample:5:20 limits-demo:1:10"
```

Adjust `M7_EXPECT_SETTINGS`, `M7_DECKS`, and `M7_RESET_DECKS` to match the
actual manual pass.

Repeatable Azahar smoke flow:

```sh
make run-emulator-m7-smoke
```

The smoke target now runs `verify-azahar-m7-smoke-artifacts` after the driver
succeeds. Rerun that artifact-only check with:

```sh
make verify-azahar-m7-smoke-artifacts
```

Dry-run the planned key sequence without controlling Azahar:

```sh
make drive-azahar-m7-smoke-dry-run
```

The smoke driver covers `limits-demo` daily-limit save, rating, undo, suspend,
restore, confirmed exit, relaunch, persistence reopen, then a `sample`
rating/reset leg before final confirmed exit. The artifact-only target expects
`M7_DECKS=limits-demo`, `M7_RESET_DECKS=sample`, and
`M7_EXPECT_SETTINGS="limits-demo:5:10 sample:20:200"`. It is a bridge for
repeatable emulator evidence; it does not replace a broader manual M7 pass
across all target workflows. On macOS, the shell/Codex host running the driver
needs Accessibility permission so `osascript` can send keys to Azahar.

## Latest Verification Evidence

Recent commands that passed in this worktree:

- `make test-host`
- `make test`
- `make -C app-3ds`
- `make verify-fe-theme-assets`
- `make test-tools`
- `make run-emulator-fresh-samples`
- `python3 tools/verify_app_theme.py`
- `git diff --check`
- `make verify-m7-preflight`

The latest full preflight was run after replacing the legacy review adapter
with the raw `app_flashcard_text_view_build()` contract and renderer-owned
review text windowing. It passed host C tests, converter tests, verifier-tool
tests, sample deck checks, app build/package checks, FE theme generation, local
SD staging, package SD verification, fresh Azahar sample staging, and Azahar
control-profile verification. It did not
launch Azahar.

Latest rerun note: `make verify-m7-preflight` passed again on 2026-06-05 after
the expanded translated 3DS-key safety tests. The rerun staged fresh
`limits-demo` and `sample` decks into Azahar SDMC, cleared tracked sample
progress and root `session.tsv` diagnostics, regenerated FE theme framebuffers,
and confirmed the Azahar keyboard profile still matches `docs/control-map.md`.

Superseded Plain-startup note: after an earlier FE color regression, the app
temporarily restored `Plain` startup and rejected FE-first startup. That note is
historical only. The current clean-slate app starts in the centralized Forest
Citro2D FE shell, and the active verifier now enforces the
`main.c -> app_screen_model -> app_renderer_c2d` boundary instead of a Plain
startup path.

Latest split note: the active app now passes through `study_backend_view` and
`app_flashcard_contract` before drawing review text. The contract owns copied
raw title/body/status strings, and host coverage verifies the string-only
builder does not pre-wrap text for a renderer. `tools/verify_app_theme.py`
accepts this clean Citro2D path and still rejects console startup, direct
framebuffer drawing in `main.c`, missing backend text rendering, and renderer
headers leaking into the backend.

Latest renderer boundary note: the old raw-framebuffer `app_fe_renderer` /
`app_review_front_contract` path was removed after subagent review identified
it as inactive drift. The active separate renderer is now
`app_renderer_c2d`; it receives only `app_screen_model`, keeps Citro2D types out
of its public header, and avoids reintroducing direct BGR framebuffer writes or
backend dependencies into the main app.

Latest FE font note: `make test-host`, `make test-tools`, `make -C app-3ds`,
`make verify-fe-theme-assets`, `make verify-package-sd`, and
`make verify-local-sd` passed on 2026-06-05 after adding the generated
`fe_font_review_8x14_alpha.bin` atlas. The emitted atlas size was verified as
10,640 bytes.

Latest FE dialogue font source note: the FEUniverse FE8U glyph draft at
`https://feuniverse.us/t/fe8u-editing-menu-dialogue-font-glyphs-draft/6716`
provided `fe8_vanilla_fontMap.png`, now vendored at
`assets/fe-themes/font/fe8_vanilla_fontMap.png`. The generator can crop that
sheet for mixed-case parchment text, but it is currently secondary because the
sheet rendered too bold at 3DS scale. The current primary is Hack Nerd Font
Mono from the local Ghostty setup, with a white key background to preserve
antialiasing on the parchment. `python3 -m unittest tests/test_fe_theme_assets.py`
and `make verify-fe-theme-assets` passed after this change.

Latest launch note: `make run-emulator-fresh-samples` was run after that
preflight. Azahar was running afterward, and the freshly written root
`session.tsv` showed `launch_count=1`, `scan_completed=1`, `deck_count=2`,
`deck_open_count=0`, and `last_event=scan`. No tested sample deck `state.tsv`
or `review-log.tsv` files existed yet, only `settings.tsv`. This is ready for
manual M7 interaction from a clean sample state.

`make test-tools` currently includes:

- text deck verifier tests
- Azahar control profile verifier tests
- app theme startup/fallback verifier tests
- M7 artifact verifier tests
- Azahar M7 smoke-driver planning tests
- FE theme asset tests

`make test-host` also includes `tests/test_clean_shell_daily_use.c`, a
current-module integration test for the clean-shell M7 workflow. It complements
the older scheduler-stack workflow tests in `tests/test_deck_scheduler.c`.

`make test-host` currently includes focused C tests for `app_confirm_contract`,
`app_deck_navigation`, `app_deck_select_contract`, `app_flashcard_contract`,
`app_power`, `app_settings_contract`, `app_status_text`, `study_backend`,
`study_controls`, `study_3ds_key_map`, `study_deck_index`, `study_settings`,
`study_review_log`, and `study_session`.

The last launched Azahar process was manually stopped before the preflight. The
configured Azahar app path is:

```text
/Users/eric/Applications/azahar-macos-arm64-2125.1.2/Azahar.app/Contents/MacOS/azahar
```

## Azahar Capture Notes

macOS capture automation is inconsistent:

- `Cmd+Shift+3` captures the whole display.
- Window-only manual capture is `Cmd+Shift+4`, Space, click the Azahar window.
- `screencapture -l<windowid>` failed for the Azahar main window with
  `could not create image from window`.
- Unsandboxed CoreGraphics can list windows. The last observed Azahar main
  window was visible with bounds roughly `X=240 Y=175 W=960 H=539`, but direct
  window sharing state was `0`.
- Region capture over those bounds can succeed as a command but may capture the
  desktop/wallpaper instead of emulator pixels. Treat user screenshots or live
  visual feedback as the reliable source unless capture behavior improves.

Useful WindowServer query pattern:

```sh
CLANG_MODULE_CACHE_PATH=/private/tmp/swift-clang-cache \
swift -module-cache-path /private/tmp/swift-module-cache -e 'import CoreGraphics; if let windows = CGWindowListCopyWindowInfo([.optionOnScreenOnly, .excludeDesktopElements], kCGNullWindowID) as? [[String: Any]] { print("count=\(windows.count)"); for w in windows { let owner = w[kCGWindowOwnerName as String] as? String ?? ""; let pid = w[kCGWindowOwnerPID as String] ?? "?"; let number = w[kCGWindowNumber as String] ?? "?"; let name = w[kCGWindowName as String] ?? ""; print("\(number)\tpid=\(pid)\t\(owner)\t\(name)") } }'
```

Run it unsandboxed if the sandbox returns no windows.

## Latest FE Renderer Readability Pass

On 2026-06-06, the active frontend-only pass stayed scoped to
`app_renderer_c2d.c`: no backend learning/deck code, `main.c`, or renderer
header contract changes were made. The renderer still accepts only the aggregate
screen model strings/state and owns all Citro2D texture, geometry, color, and
font presentation.

Visual changes:

- Kept the embedded Forest background/parchment legend textures as the base
  layer for both screens.
- Added a thin Citro2D inset trim over the parchment so the dialogue area reads
  more like a framed FE-style scroll while still using the existing baked
  parchment asset.
- Made every live parchment string use the same readable ink path: a light
  parchment highlight at `+1,+1`, then a second dark ink pass to thicken the
  Citro2D system font.
- Darkened the primary/muted/faint ink colors and raised the review/menu/bottom
  text scales for better Azahar and hardware readability.
- Tightened review body wrapping to 31 columns by 4 visible rows so larger text
  stays inside the parchment panel and scrolls through the existing renderer
  scroll helper.

Checks from this pass:

- `make -C app-3ds` passed.
- `python3 tools/verify_app_theme.py` passed in the main workspace.
- `make test`, `git diff --check`, and `make verify-m7-preflight` passed after
  the parallel-agent changes were visible in the main workspace. The preflight
  restaged the current app, FE assets, and fresh tracked sample decks into the
  local, package, and Azahar SDMC roots.

Latest renderer/contract audit note: the display contract verifier now has
broader coverage for public-header Citro2D leaks, public display-geometry
constants, non-renderer draw APIs, backend display-contract/geometry coupling,
and renderer backend forward declarations. The former
`APP_DECK_SELECT_VISIBLE_ROWS` public-header leak is resolved: list windowing
stays private to `app_deck_select_contract.c`, and deck-selector paging stays
private to `app_deck_select_action.c`.
`make test-host`, `python3 -m unittest tests/test_verify_app_theme.py`,
`python3 tools/verify_app_theme.py`, `make -C app-3ds`, `make test`,
`git diff --check`, and `make verify-m7-preflight` passed after this cleanup.
The app-theme verifier suite now has 51 tests, including a regression that
rejects public non-renderer display-geometry constants.

Latest suspend/restore feedback note: `app_review_action` now owns a tested
`app_review_action_suspend_restore_target()` helper for the direct `R` flow.
Active cards open suspend confirmation, completed decks with suspended cards
open restore confirmation, completed decks with no suspended cards report
`Nothing suspended` through the warning-context-preserving status helper, and
empty/no-deck states remain inert. `make test-host`,
`python3 tools/verify_app_theme.py`, `make -C app-3ds`, `git diff --check`,
`make test`, and `make verify-m7-preflight` passed after this change. The
preflight restaged the current app, FE assets, and fresh tracked sample decks
into local, package, and Azahar SDMC roots.

Latest no-op undo feedback note: `app_review_action` now also owns
`app_review_action_undo_target()` for the direct `L` flow. Decks with a saved
rating expose normal undo, decks/cards with no undo history report
`Nothing to undo` through the warning-context-preserving status helper, and
empty/no-deck states stay inert. Focused `make test-host`,
`python3 tools/verify_app_theme.py`, direct `make -C app-3ds`,
`git diff --check`, `python3 -m unittest tests/test_verify_app_theme.py`,
`make test`, and `make verify-m7-preflight` passed after this change. The
preflight restaged the current app, FE assets, and fresh tracked sample decks
into local, package, and Azahar SDMC roots.

Latest held-input safety note: `study_controls` now owns the clean-shell
chord-safe held-button helper and frame-counted navigation repeat state. The
3DS loop reads both `hidKeysDown()` and `hidKeysHeld()`, feeds one chord-safe
logical button mask to deck select, settings, confirmations, and review, and
uses one-VBlank waits while a repeatable navigation direction is held. Deck
select repeats Up/Down/Left/Right, review repeats Up/Down scroll, settings
repeat Left/Right value changes, and confirmation screens do not repeat held
input. `make test-host`, `python3 tools/verify_app_theme.py`,
`python3 -m unittest tests/test_verify_app_theme.py`, `make -C app-3ds`,
`git diff --check`, `make test`, and `make verify-m7-preflight` passed after
the parallel-agent merge. The preflight restaged the current app, FE assets,
and fresh tracked sample decks into local, package, and Azahar SDMC roots.

Latest FE renderer and first-save persistence note: the review top screen no
longer draws the implicit `Question`/`Answer` title header; the card text moves
up into that space. The parchment panels remain expanded, and the visible
inner readability frame now stays inset inside the parchment edge instead of
overhanging it. Active app text is currently Citro2D/shared-font text via
`C2D_TextParse`; in Azahar this uses the open-source shared-font fallback when
the real 3DS shared font is unavailable. The earlier Hack and FE atlas assets
remain experiments/assets, not the active renderer path.

The first-save atomic persistence path was hardened for root `session.tsv`,
deck `state.tsv`, deck `settings.tsv`, and `review-log.tsv`: writers now check
for the primary file before renaming it to `.bak`, and missing backup cleanup
is skipped before `remove()`. This avoids the Azahar/3DS first-save failure
mode where `rename(missing, .bak)` did not reliably behave like host
`ENOENT`, and it removes the noisy missing-`.bak` log line from clean launches.
`make test-host`, `git diff --check`, and `make -C app-3ds` passed after the
persistence patch. `python3 tools/verify_app_theme.py` also passed after the
renderer spacing/header update.

Latest Azahar first-scan evidence: `make run-emulator-fresh-samples` was run
from a blank staged root after the persistence fix. The freshly written root
`session.tsv` in Azahar SDMC showed `launch_count=1`, `scan_completed=1`,
`deck_count=2`, `ignored_count=0`, `deck_open_count=0`, `last_deck_id=-`, and
`last_event=scan`. The newest Azahar log tail for that run had normal emulator
startup warnings only; the earlier missing `session.tsv.bak` cleanup error was
gone. Manual app interaction and post-run `make verify-m7-artifacts` remain
pending.

Latest legend/font note: the renderer no longer draws the synthetic Citro2D
outline/corner border over the baked parchment asset; it only draws a faint
readability fill inside the parchment. All live Citro2D text boxes now use one
shared `APP_RENDERER_C2D_TEXT_SCALE` value so card text, deck text, status, and
legend text are the same size. The bottom legend is compact by default and can
be expanded/collapsed with `B` on deck select and unrevealed/no-active review
screens; `B` remains the Easy rating after an answer is revealed. The compact
legend surfaces core commands, while expanded help explains deck folders,
options, reset progress, and daily-limit behavior.

Available font sources found locally:

- Active live app: Citro2D shared 3DS font through `C2D_TextParse`; Azahar uses
  its open-source shared-font fallback when the real shared font is unavailable.
- Ghostty/default terminal: `Hack Nerd Font Mono`, configured at size 14 in
  `/Users/eric/.config/ghostty/config`, with Hack Nerd Font variants in
  `/Users/eric/Library/Fonts/`.
- FE repo font files: `Checkmate.otf`, `Emporio.ttf`, and
  `Emporio Italic.ttf` under `/Users/eric/codebase/FE-Repo/BGs, Interface Elements/Vanilla Fonts & Logos & Save Slots/`.
- Vendored FE dialogue source: `assets/fe-themes/font/fe8_vanilla_fontMap.png`.

Only the Citro2D shared-font path is active in `app_renderer_c2d.c`. Using
Hack, Checkmate, Emporio, or FE8 glyphs live on 3DS requires a frontend-only
bitmap/sprite font renderer or generated atlas path; Citro2D text cannot load
host macOS `.ttf`/`.otf` files directly inside the 3DS app.

Latest direct-control smoke-driver note: `tools/drive_azahar_m7_smoke.py` was
updated away from the removed actions screen and then extended with a reset
deck leg. The dry run now uses direct controls for the current app: open
`limits-demo`, press `X` for study settings, change `new_limit` from `2` to
`5`, change `review_limit` from `5` to `10`, save with `X`, reveal/rate Good
with `X`, undo with `L`, reveal again, suspend all six active cards with repeated
`R` then `X`, restore from the no-active suspended summary with `R` then `X`,
exit with `START` then `A`, relaunch, reopen `limits-demo`, return to the deck
selector with `SELECT`, open `sample`, reveal/rate Good, reset `sample` with
`Y` then `X`, and final-exit with `START` then `A`.

The smoke-driver unit test now guards that the sequence saves limits with `X`,
uses `SELECT` only for returning to deck select after the persistence reopen,
and no longer contains the deleted actions-screen labels. The tracked
`limits-demo` card that still taught `SELECT` as opening deck actions was
corrected to describe returning to the deck list. Focused driver tests, the
direct-control compact artifact-shape verifier test, and
`tests/test_m7_direct_smoke_flow.c` host integration coverage now expect
studied `limits-demo` artifacts plus reset `sample` artifacts with
`sample` settings preserved as `20/200`. `python3 -m unittest
tests/test_drive_azahar_m7_smoke.py`, `python3 -m unittest
tests/test_verify_m7_artifacts.py`, `make test-host`, and `python3
tools/drive_azahar_m7_smoke.py --dry-run` passed after the reset-leg update.
The broader `make test`, direct `make -C app-3ds`, `git diff --check`, and
`make verify-m7-preflight` gates also passed and restaged fresh local/package
and Azahar SD roots.
Follow-up audit on 2026-06-06 confirmed the smoke sequence matches the active
deck selector, study, confirmation, and settings reducers. The remaining stale
old smoke-flow wording was removed from the driver/doc contract, and
`python3 -m unittest tests/test_drive_azahar_m7_smoke.py`, `python3
tools/drive_azahar_m7_smoke.py --dry-run`, and `git diff --check` passed.
Another 2026-06-06 continuation retried `make run-emulator-m7-smoke`; it still
failed at `tools/drive_azahar_m7_smoke.py --check-automation` because macOS
denied `osascript` keystrokes, before staging or launching Azahar. The same
continuation extracted per-screen held-repeat masks from `main.c` into
`app_input_policy`, added `tests/test_app_input_policy.c`, and wired
the app input path to use `app_input_policy_repeat_mask()` with the active
`app_screen_model_kind`. Focused compile, `/tmp/anki3ds-test-app-input-policy`,
`make test-host`, `python3 tools/verify_app_theme.py`, `python3 -m unittest
tests/test_verify_app_theme.py`, direct `make -C app-3ds`, and
`git diff --check` passed after the extraction.
The next continuation extracted app-mode mapping from `main.c` into
`app_mode`, added `tests/test_app_mode.c`, and wired the render/input and
confirm-action paths to `app_mode_screen_model_kind()`,
`app_mode_confirm_contract_kind()`, `app_mode_confirm_action_kind()`, and
`app_mode_is_confirm()`. Focused compile, `/tmp/anki3ds-test-app-mode`,
`make test-host`, `python3 tools/verify_app_theme.py`, `python3 -m unittest
tests/test_verify_app_theme.py`, direct `make -C app-3ds`, and
`git diff --check` passed before the final full gate.
The next continuation extracted confirmed reset side effects from `main.c` into
`app_reset_action`, added `tests/test_app_reset_action.c`, and kept session
counter recording in `main.c`. The test covers successful state/log artifact
cleanup with warning context and low-battery suffix, reset with no enabled
state files, state-delete failure, and log-delete failure after state cleanup.
Focused compile, `/tmp/anki3ds-test-app-reset-action`, `make test-host`,
`python3 tools/verify_app_theme.py`, `python3 -m unittest
tests/test_verify_app_theme.py`, direct `make -C app-3ds`, and
`git diff --check` passed before the final full gate.
The interrupted continuation added `app_state_save` and was then completed by
wiring `main.c` through the new helper. The helper owns state save,
review-log append/fallback, session saved-action counter mapping, deterministic
timestamp/day handoff from the app loop, and low-battery suffix handling.
Focused compile, `/tmp/anki3ds-test-app-state-save`, `make test-host`,
`python3 tools/verify_app_theme.py`, direct `make -C app-3ds`, and
`git diff --check` passed before the final full gate.
The next continuation extracted daily-limit save side effects from `main.c`
into `app_settings_save`, added `tests/test_app_settings_save.c`, and wired the
settings editor save branch through `app_settings_save_apply()`. The helper
owns `settings.tsv` persistence, active-setting replacement on success, status
context and low-battery save suffix handling, and root session
`settings_saved` diagnostics. Focused compile,
`/tmp/anki3ds-test-app-settings-save`, `make test-host`,
`python3 tools/verify_app_theme.py`, direct `make -C app-3ds`, and
`git diff --check` passed before the final full gate. The following full gate
also passed: `make test`, direct `make -C app-3ds`,
`python3 tools/verify_app_theme.py`, and `git diff --check`.
The next continuation extracted deck-selector workflow side effects from
`main.c` into `app_deck_flow`, added `tests/test_app_deck_flow.c`, and wired
startup scan plus deck-select input through the new helper. The helper owns
rescan selection preservation, deck loading, active path copying, settings
load, state day rollover, app-mode transition for deck-select commands, and
root session scan/deck-open diagnostics with deterministic timestamp/day
inputs. Focused compile, `/tmp/anki3ds-test-app-deck-flow`,
`make test-host`, `python3 tools/verify_app_theme.py`, and direct
`make -C app-3ds` passed before the final full gate. The following full gate
also passed: `make test`, direct `make -C app-3ds`,
`python3 tools/verify_app_theme.py`, and `git diff --check`.
The next continuation extracted confirmation workflow side effects from
`main.c` into `app_confirm_flow`, added `tests/test_app_confirm_flow.c`, and
wired confirmation-mode input through the new helper. The helper owns
open-exit/cancel transitions, confirmed exit/reset session events, confirmed
suspend/restore state-log dirty handoff, review-scroll reset requests, warning
context preservation, and low-battery reset suffix handoff. Focused compile,
`/tmp/anki3ds-test-app-confirm-flow`, `make test-host`,
`python3 tools/verify_app_theme.py`, and direct `make -C app-3ds` passed
before the final full gate. The following full gate also passed: `make test`,
direct `make -C app-3ds`, `python3 tools/verify_app_theme.py`, and
`git diff --check`.
The next continuation extracted review-screen workflow orchestration from
`main.c` into `app_review_flow`, added `tests/test_app_review_flow.c`, and
wired review-mode input through the new helper. The helper owns help toggling,
settings entry, unavailable undo/suspend feedback, reset/deck/exit/suspend/
restore mode transitions, daily-limit gates, answer-shown session diagnostics,
warning-context preservation, and state/log dirty handoff from
`app_review_action`. `tools/verify_app_theme.py` now includes
`app_review_flow.[ch]` in the clean FE source aggregate and accepts answer/
rating transitions through `app_review_flow_handle_input()`. Focused compile,
`/tmp/anki3ds-test-app-review-flow`, `make test-host`,
`python3 tools/verify_app_theme.py`, `python3 -m unittest
tests/test_verify_app_theme.py`, and direct `make -C app-3ds` passed before
the final full gate. The following full gate also passed: `make test`, direct
`make -C app-3ds`, `python3 tools/verify_app_theme.py`, and
`git diff --check`.
The next continuation extracted settings-screen workflow orchestration from
`main.c` into `app_settings_flow`, added `tests/test_app_settings_flow.c`, and
wired settings-mode input through the new helper. The helper owns save/cancel/
exit/update mode transitions, warning-context status text, save delegation to
`app_settings_save`, and settings-saved session dirty handoff with
deterministic timestamp/day inputs. Focused compile,
`/tmp/anki3ds-test-app-settings-flow`, `make test-host`, direct
`make -C app-3ds`, and `python3 tools/verify_app_theme.py` passed before the
final full gate. The following full gate also passed: `make test`, direct
`make -C app-3ds`, `python3 tools/verify_app_theme.py`, and
`git diff --check`.
The next continuation extracted local-day rollover orchestration from `main.c`
into `app_day_rollover_flow`, added `tests/test_app_day_rollover_flow.c`, and
wired the app loop through the new helper. The helper owns root session day
updates, backend daily-limit/completed-today rollover, state persistence via
`app_state_save`, low-battery save suffix handoff, and redraw signaling without
renderer geometry. Focused compile,
`/tmp/anki3ds-test-app-day-rollover-flow`, `make test-host`, direct
`make -C app-3ds`, and `python3 tools/verify_app_theme.py` passed before the
final full gate. The following full gate also passed: `make test`, direct
`make -C app-3ds`, `python3 tools/verify_app_theme.py`, and
`git diff --check`.
The next continuation extracted root session save policy from `main.c` into
`app_session_save`, added `tests/test_app_session_save.c`, and wired all app
loop session flushes through the new helper. The helper owns clean-session
no-op behavior, dirty root `session.tsv` writes, status preservation on
success, and `Session save failed` backend feedback on failure. Focused
compile, `/tmp/anki3ds-test-app-session-save`, `make test-host`, direct
`make -C app-3ds`, and `python3 tools/verify_app_theme.py` passed before the
final full gate. The following full gate also passed: `make test`, direct
`make -C app-3ds`, `python3 tools/verify_app_theme.py`, and
`git diff --check`.
The next continuation extracted app-shell battery monitor state from `main.c`
into `app_battery_monitor`, added `tests/test_app_battery_monitor.c`, extended
`tests/stubs/3ds.h` with PTMU declarations, and wired app startup/poll/save
suffix/shutdown paths through the new helper. The helper owns lazy PTMU
initialization, closed-shell samples, change detection, failed-read behavior,
low-battery status application, save-warning policy, and one-shot PTMU
shutdown. Focused compile, `/tmp/anki3ds-test-app-battery-monitor`,
`make test-host`, direct `make -C app-3ds`, and
`python3 tools/verify_app_theme.py` passed before the final full gate. The
following full gate also passed: `make test`, direct `make -C app-3ds`,
`python3 tools/verify_app_theme.py`, and `git diff --check`.
The next continuation moved startup deck scan/session evidence from `main.c`
into `app_deck_flow_startup_scan`, reused shared scan-session recording for
startup and manual `SELECT` rescan, and extended `tests/test_app_deck_flow.c`.
The helper initializes an empty scanning backend, rescans the deck root,
clears stale card pointers before the first deck opens, records session scan
diagnostics with caller-provided timestamp/day, marks the root session dirty,
and reports missing deck roots as completed empty scans. Focused compile,
`/tmp/anki3ds-test-app-deck-flow`, `make test-host`, and direct
`make -C app-3ds` passed before the final full gate. The following full gate
also passed: `make test`, direct `make -C app-3ds`,
`python3 tools/verify_app_theme.py`, and `git diff --check`.
The next continuation extracted app-frame input assembly from `main.c` into
`app_input_frame`, added `tests/test_app_input_frame.c`, and wired the app loop
through the new helper after `study_3ds_key_map` translates physical 3DS keys
to logical buttons. The helper owns combining fresh button edges with held
repeat output, preserving held context for chord rejection, applying
per-screen repeat masks, and returning the held-navigation vblank wait signal.
Focused compile, `/tmp/anki3ds-test-app-input-frame`, `make test-host`, and
direct `make -C app-3ds` passed before the final full gate. The following full
gate also passed: `make test`, direct `make -C app-3ds`,
`python3 tools/verify_app_theme.py`, and `git diff --check`.
The next continuation corrected the larger boundary by moving app workflow
state and orchestration from `main.c` into `app_shell`, adding
`tests/test_app_shell.c`, and reducing `main.c` to libctru lifecycle, idle
wait/vblank pacing, HID reads, physical key translation, app-frame input
assembly, renderer max-scroll calculation, and renderer draw calls. `app_shell`
now owns startup deck scan, root session evidence, active deck paths,
backend/settings/session state, app mode, help/scroll state, battery polling
policy, local-day rollover, state/session persistence flushes, screen-model
construction, raw review scroll text handoff, and per-mode workflow reducer
dispatch. `tools/verify_app_theme.py` was updated to verify the new
`main.c -> app_shell -> app_screen_model -> app_renderer_c2d` boundary while
still accepting older direct-Citro2D clean-shell fixtures in unit tests.
Focused compile, `/tmp/anki3ds-test-app-shell`, `make test-host`, direct
`make -C app-3ds`, `python3 -m unittest tests/test_verify_app_theme.py`, and
`python3 tools/verify_app_theme.py` passed before the final full gate. The
following full gate also passed: `make test`, direct `make -C app-3ds`,
`python3 tools/verify_app_theme.py`, and `git diff --check`.

`make run-emulator-m7-smoke` still cannot drive Azahar in this environment
because the precheck fails before launch/input with macOS denying `osascript`
keystrokes. A fresh manual launch was restaged with
`make run-emulator-fresh-samples`; the new root `session.tsv` showed
`launch_count=1`, `scan_completed=1`, `deck_count=2`, `ignored_count=0`,
`deck_open_count=0`, `exit_confirmed=0`, `last_deck_id=-`, and
`last_event=scan`. A screenshot attempt did not capture a visible emulator
window even after activating Azahar, and System Events window inspection is
also blocked by Assistive Access in this environment. After the interrupted
turn was resumed, `pgrep -fl Azahar` and `pgrep -fl osascript` returned no
processes; the scan evidence remains on disk, but do not assume the emulator
window is still open.

## 2026-06-06 Render Path Investigation

Do not resume the parchment/text issue by guessing at coordinates. First prove
which surface is being viewed and captured.

Proved source path:

- `app-3ds/source/main.c` initializes libctru, initializes
  `app_renderer_c2d`, asks `app_shell` for an `app_screen_model`, and calls
  `app_renderer_c2d_draw()`. It does not draw deck text directly.
- Deck-select strings are produced by `app_deck_select_contract_build()` via
  `app_screen_model_build()`.
- Top deck title/list/meta draw through
  `app_renderer_c2d_draw_deck_select_top_screen()`.
- Bottom deck status/help/footer draw through
  `app_renderer_c2d_draw_deck_select_bottom_screen()`.
- Both paths call `app_renderer_c2d_draw_box_text()`,
  `app_renderer_c2d_draw_text_shadowed()`, `app_renderer_c2d_draw_parsed_text()`,
  then `C2D_DrawText()`.

Proved image path:

- `app_renderer_c2d_init()` loads `fe_forest_top_legend_t3x` and
  `fe_forest_bottom_legend_t3x` with `C2D_SpriteSheetLoadFromMem()`.
- `app_renderer_c2d_draw_theme_surface()` draws those images with
  `C2D_DrawImageAt()`, then applies `app_renderer_c2d_draw_parchment_readability_frame()`.
- The embedded PNG sources are `app-3ds/gfx/fe_forest_top_legend.png` and
  `app-3ds/gfx/fe_forest_bottom_legend.png`, listed in the matching `.t3s`
  files and converted to `.t3x` assets during the 3DS build.
- `sync-app-fe-ui-assets` copies them from
  `build/fe-theme-previews/forest_top_layer_legend_400x240.png` and
  `build/fe-theme-previews/forest_bottom_layer_legend_320x240.png`.
- `rg` found no baked copies of `Deck 1/2`, `Left/Right pages`,
  `Select deck`, or `Found 2 decks` in the generated image assets. Those
  visible strings are live C2D text, not part of the parchment image.

Proved linked-artifact state after the attempted footer move:

- `strings app-3ds/anki3ds.elf` contains the deck-select strings and
  `app_renderer_c2d` symbols.
- `arm-none-eabi-nm -an app-3ds/anki3ds.elf` shows
  `app_menu_top_meta_box` at `0x00135094` and
  `app_bottom_footer_box` at `0x001350c4`.
- `arm-none-eabi-objdump -s` decoded current linked values as:
  `app_menu_top_meta_box = x 42, y 150, scale 0.5, wrap 316` and
  `app_bottom_footer_box = x 34, y 150, scale 0.5, wrap 252`.
- If Azahar still appears to show those lines on the torn bottom edge, the
  contradiction is not explained by current source or the linked ELF.

Proved Azahar/runtime state:

- Azahar was running as PID `35163`.
- `azahar_log.txt` showed repeated homebrew boots with SelfNCCH warnings
  typical of `.3dsx` loading.
- `sdmc:/3ds/anki3ds/session.tsv` was updated by the running app and showed
  `launch_count=2`, `scan_completed=1`, `deck_count=2`, and `last_event=scan`.
- CoreGraphics window enumeration found an Azahar window owned by PID `35163`,
  window id `13504`, bounds `X=1440`, `Y=-234`, `Width=1280`,
  `Height=1415`.
- Plain `screencapture` captured the desktop instead of the emulator because
  the Azahar window was outside/above the captured visible desktop region.
- `screencapture -l 13504` failed with `could not create image from window`.
- Targeted `screencapture -R 1440,0,1280,1180`, display 1, and display 2
  captures still showed menu bars/wallpaper without app-window contents.
  CoreGraphics reported Azahar and other app windows with `sharing=0`, so this
  appears to be a macOS Screen Recording/window-sharing limitation rather than
  evidence about the 3DS frame.
- Azahar config shows the internal screenshot shortcut as `Ctrl+P`
  (`Shortcuts\Main%20Window\Capture%20Screenshot\KeySeq=Ctrl+P`) and an empty
  default screenshot path. A single CoreGraphics `Ctrl+P` key event was posted
  to focused Azahar, but no new PNG/BMP was found under Azahar app support,
  Pictures, Desktop, Downloads, or the non-Library home tree, and the Azahar
  log did not record `Screenshot saved`.
- A runtime renderer fingerprint was then added at
  `sdmc:/3ds/anki3ds/renderer.tsv`, with fresh-sample reset clearing stale
  copies through `APP_SESSION_FILES`. After killing Azahar and launching with
  `make run-emulator-fresh-samples`, Azahar wrote a new `renderer.tsv` at
  2026-06-06 20:42 with:
  `renderer=citro2d`, `assets_loaded=1`, top legend size `156613`, bottom
  legend size `131733`, `menu_top_meta x=42 y=150 scale=0.50 wrap=316`,
  `bottom_footer x=34 y=150 scale=0.50 wrap=252`, and
  `review_top_meta x=42 y=150 scale=0.50 wrap=316`.
- The matching fresh root `session.tsv` showed `launch_count=1`,
  `scan_completed=1`, `deck_count=2`, `ignored_count=0`, and `last_event=scan`.
  This proves Azahar executed the rebuilt Citro2D renderer and loaded embedded
  assets. The remaining issue is not the wrong source function, baked text, or
  stale binary.
- System Events window inspection failed because Assistive Access is not
  granted to the host running `osascript`.

Next investigation step: establish a reliable capture/inspection loop for the
actual Azahar surface, or add a temporary unmistakable marker in the proven
C2D text path and remove it after verification. Do not introduce another
layout abstraction or coordinate tweak until that proof exists. If the user can
manually see the current launch and the footer/meta still overlap the parchment
edge, fix the actual renderer safe area from the proven C2D text boxes.

## Current Known Rough Edges

- Active FE integration is a centralized Citro2D image/text composition path,
  not the older console-black pixel replacement pass.
- Startup enters the embedded Forest FE shell directly for the clean-slate app.
- All active app text is drawn by the Citro2D renderer from backend/display
  strings. The remaining visual risk is manual readability/color acceptance on
  Azahar and hardware.
- Automated Azahar input is blocked locally until Accessibility/Assistive
  Access is granted to the terminal/Codex host that runs `osascript`.
- Manual Azahar/hardware M7 acceptance remains pending.
- The worktree is dirty with several unrelated or earlier M7/FE edits. Do not
  revert unknown changes without explicit user approval.

## Next Best Work

1. If Accessibility/Assistive Access is available, run
   `make run-emulator-m7-smoke`; it now uses the current direct-control flow
   and should finish by running `make verify-azahar-m7-smoke-artifacts`.
2. If automation is still blocked, run a manual M7 Azahar pass from
   `make run-emulator-fresh-samples`. Open `limits-demo`, save limits as
   `5` new and `10` review, rate one card, undo once, suspend and restore,
   relaunch, reopen the deck, and exit through confirmation.
3. Run `make verify-m7-artifacts` with exact `M7_EXPECT_SETTINGS` and any
   `M7_RESET_DECKS`, `M7_ALLOW_MISSING_REVIEW_LOG`, or
   `M7_NO_REQUIRED_EVENTS` settings matching the actual pass.
4. Continue the display/backend split by moving framebuffer asset
   loading/composition behind a display-assets boundary, or by adding the
   revealed-review view model next.
5. Manually inspect the FE renderer readability pass in Azahar, focusing on the
   expanded panels, removed synthetic border, one-size live text, `B` legend
   toggle, removed review title header, 40x7 front wrapping, and 31x6 answer
   wrapping.
6. If Citro2D default-font readability is still not good enough, the next
   frontend-only step is a real sprite/font-atlas text layer in the renderer,
   not reintroducing backend or console drawing.
7. Record manual evidence in `docs/emulator-test-log.md` or
   `docs/device-test-log.md`.

## 2026-06-06 Review UI Cleanup Checkpoint

- Review rendering remains renderer-owned: the flashcard contract supplies raw
  front/back/status/progress/help strings and flags only.
- The top screen uses the front text window. Once the answer is revealed, the
  bottom screen uses a separate `review_bottom_answer` text box for the back
  text. The answer text is pre-windowed by `app_text` and drawn without
  Citro2D word wrapping to avoid a second wrap pass.
- Review wrap windows are now `front=40x7` and `answer=31x6`. These values are
  emitted to `sdmc:/3ds/anki3ds/renderer.tsv` as
  `review_front_window`, `review_answer_window`, and
  `review_bottom_answer`.
- Deck-select full controls are hidden by default. The default legend is only
  `B: help`; expanded controls still live behind the help toggle.
- `app_shell` clears expanded help on mode changes so deck-select help does not
  leak into review, and review help does not leak back to the deck selector.
- Added host coverage for keeping the next word intact when wrapping
  (`"red blue"` at six columns becomes `"red\nblue"`) and for the hidden
  default deck controls.
- The latest flashcard contract coverage also checks a longer sentence wraps at
  earlier spaces instead of cutting words, and checks that a revealed card keeps
  the front prompt and back answer as separate strings with help hidden by
  default.
- Verification passed after this checkpoint:
  `make test-host`, `python3 tools/verify_app_theme.py`, `make test-tools`,
  and `make -C app-3ds`.

## 2026-06-07 UI Labels, Tags, And Review-Log Checkpoint

- Deck selector expanded help now uses grouped `button: action` labels:
  `A/B`, arrows, `SELECT`, and `START`. Compact help is `B: help`.
- Review and settings legends also use `button: action` labels. Revealed
  answers keep `B: Hard`; unrevealed/no-active review keeps `B: hide help`.
- Deck row stats now label `Due`, `New`, and `Susp` instead of abbreviated
  count suffixes. Review meta uses `Seen`, `Reviewed`, and `Susp`; the
  bottom daily footer spells out `Today New` and `Review`.
- Deck-select bottom footer now consolidates all deck-list stats as
  `All decks: Due ... | New ... | Susp ...`, appending `Issues` when any deck
  has unsafe or unavailable stats.
- `cards.tsv` tags are now parsed into `study_backend_card.tags`, exposed
  through `study_backend_view.tags_text` and `app_flashcard_text_view.tags_text`,
  and shown as separate Citro2D tag chips on the revealed answer side when
  non-empty.
- `study_review_log` now trims partial final rows only after validating complete
  six-field rows. Malformed complete primary logs are discarded when no valid
  recovery artifact exists, or recovered from valid `.tmp` / `.bak` artifacts.
- Verification passed after this checkpoint:
  `make test-host`, `python3 tools/verify_app_theme.py`, `make -C app-3ds`,
  and `git diff --check`.

Next queued tasks:

1. Evaluate the swappable learning/scheduler policy. Preferred direction is a
   hybrid: card-count cooldowns for same-session learning/relearning, then
   time-based spaced repetition for long-term review.
2. Continue manual Azahar/hardware acceptance with the imported personal decks
   visible in the deck selector.

## 2026-06-07 Personal Anki Import Checkpoint

- Added `tools/import_anki_collection.py`, a read-only SQLite importer for
  Basic-style Anki decks. It registers Anki's `unicase` collation, reads deck,
  card, note, and field rows, chooses Front/Back-style fields, normalizes HTML
  through the existing converter helpers, carries note tags into `cards.tsv`,
  rejects inline image HTML plus sound/media markers, rejects non-forward card
  template ordinals, refuses tracked repo output unless explicitly overridden,
  keeps duplicate deck-name ids stable against the full populated collection,
  and writes through the existing `write_split_decks` path so deck limits,
  stable source IDs, settings, and progress preservation stay consistent with
  tab-separated imports.
- Added `tests/test_import_anki_collection.py` and wired it into
  `make test-tools`. The tests use a synthetic SQLite collection and do not
  require or expose personal card content.
- The local Anki collection was copied read-only from
  `~/Library/Application Support/Anki2/User 1/collection.anki2` to
  `/private/tmp/anki3ds-collection.anki2` before import. Populated decks found:
  `Leetcode` (169), `SAT Vocabulary` (626), `Vim` (279), and `RecSys` (159).
  No separate `Advanced Algorithms` deck was present in that profile; algorithm
  tags were visible inside `RecSys`.
- Per user instruction, `RecSys` was imported and verified first:
  `python3 tools/import_anki_collection.py /private/tmp/anki3ds-collection.anki2 local/sdmc/3ds/anki3ds/decks --deck RecSys`
  then
  `python3 tools/verify_text_deck.py local/sdmc/3ds/anki3ds/decks/recsys`.
- After that verification, all populated decks were imported into
  `local/sdmc/3ds/anki3ds/decks` and Azahar's
  `~/Library/Application Support/Azahar/sdmc/3ds/anki3ds/decks`. Verified deck
  ids are `leetcode`, `sat-vocabulary`, `vim`, and `recsys`.
- `Leetcode` has empty Back fields for all 169 cards in the source collection,
  and `SAT Vocabulary` has 2 empty Back fields. The importer uses the explicit
  placeholder `(No answer)`, and the Citro2D renderer draws that marker with
  faint ink so generated decks stay loadable without treating an empty back as
  real answer text.
- Personal deck folders are generated under ignored SD-card mirrors or Azahar's
  application data, not tracked `sample-decks/`.

## 2026-06-07 Swappable Scheduler Policy Checkpoint

- Added `enum study_backend_scheduler_policy` plus
  `study_backend_set_scheduler_policy()` to the clean-shell backend. Rendering,
  imports, input handling, and save-file paths do not depend on the policy.
- The default remains `STUDY_BACKEND_SCHEDULER_DUE_FIRST`, preserving current
  daily-use behavior while daily-limit blocking still lives in app review flow.
- Added `STUDY_BACKEND_SCHEDULER_CARD_COUNT_COOLDOWN` as a tested
  session-local learning policy. `Again` applies a configurable card-count
  cooldown; ratings on other cards decrement it; queue selection tries
  non-cooldown introduced cards, then new cards, then cooldowned introduced
  cards as a no-stuck fallback.
- Cooldown state is transient and not written to `state.tsv`. It is cleared on
  state load, day rollover, reset, suspend, and policy changes. Undo restores a
  cooldown snapshot from before the undone rating.
- Focused verification passed:
  `cc -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iapp-3ds/include tests/test_study_backend.c app-3ds/source/study_backend.c -o /tmp/anki3ds-test-study-backend && /tmp/anki3ds-test-study-backend`.

## 2026-06-07 Imported Deck Capacity And Selector Layout Checkpoint

- User reported `Leetcode: Too many cards` and selector footer `Issues 4`.
  Root cause: the clean-shell backend and deck-index summary cap was still
  `STUDY_BACKEND_MAX_CARDS=128`, while imported personal decks are
  `Leetcode` 169, `SAT Vocabulary` 626, `Vim` 279, and `RecSys` 159.
- Raised the active text-deck cap to 1024 cards per deck folder in
  `study_backend`, `converter/anki3ds_convert.py`, and
  `tools/verify_text_deck.py`. This covers the imported decks and keeps larger
  exports/imports split into numbered sibling folders instead of forcing
  4096-card backend snapshots onto the 3DS stack.
- Moved the deck-selector top meta line (`Deck N/M  Ignored X`) lower inside
  the parchment by changing `app_menu_top_meta_box.y` from `166` to `178`.

## 2026-06-07 Empty Answer And Rollback Buffer Checkpoint

- Empty Back fields imported from Anki now use the display marker `(No answer)`
  instead of a full explanatory sentence. The Citro2D renderer treats that exact
  marker as a muted answer-side state and draws it with faint ink.
- Re-imported personal decks into both `local/sdmc/3ds/anki3ds/decks` and
  Azahar's SD-card mirror. The previous `No back text in source Anki note.`
  sentence is gone from regenerated `cards.tsv` rows.
- After raising the per-deck cap to 1024, moved large backend rollback copies
  out of per-function stack locals:
  `app_shell` now owns a reusable `rollback_backend`, while day-rollover and
  deck-load rollback staging use module-owned backend buffers.

## 2026-06-07 Low-Battery Diagnostic Log Checkpoint

- `app_state_save` now treats low battery as a reason to skip the optional
  `review-log.tsv` append after a successful `state.tsv` save. The accepted
  rating/suspend/restore/undo remains saved, root session saved-action evidence
  is still recorded, and the user-visible status is
  `Saved; log skipped; batt low`.
- The same slice fixed a subtle return-value bug: successful state saves without
  a low-battery suffix now return handled work to the shell instead of returning
  false because no suffix was appended.
- `tools/verify_m7_artifacts.py --allow-missing-review-log` is explicitly the
  verifier allowance for in-app `log skipped` cases, including partial
  diagnostic-log gaps where session saved-action counters exceed review-log
  event rows. Required event checks remain active unless
  `--no-required-events` is also passed.
- Focused and broad verification passed: `/tmp/anki3ds-test-app-state-save`,
  `make test-host`, `git diff --check`, `make -C app-3ds`, and `make test`.

## 2026-06-07 M7 Smoke Driver Deck-Order Checkpoint

- `tools/drive_azahar_m7_smoke.py` now discovers the sorted Azahar SD deck
  folder order and injects selector navigation steps to reach `limits-demo` and
  later `sample`. This fixes the stale assumption that tracked samples are the
  first two rows when personal imports such as `leetcode`, `recsys`,
  `sat-vocabulary`, or `vim` are also installed.
- `Makefile` now passes `AZAHAR_SDMC` into both the smoke dry-run and live
  driver targets.
- A dry-run against the current Azahar SDMC produced the expected personal-deck
  navigation: one Down to reach `limits-demo`, then two Down presses from
  `limits-demo` to reach `sample`.

## 2026-06-07 3DS Control Prompt Checkpoint

- Kept the active control behavior stable, but clarified the on-screen prompts
  around the intended physical 3DS mental model: `A` reveals and then rates
  `Again`, face buttons are grouped worst-to-best after reveal, Circle Pad
  scrolls the question/front text, D-pad scrolls the answer text, `L/R` page
  decks on the selector, and `L/R` remain undo/suspend in review.
- Deck selector expanded help now separates arrow paging from shoulder paging:
  `↑/↓: deck`, `←/→: page`, and `L/R: page decks`.
- Review expanded help now says `Y: restart deck`, `Circle ↑/↓: question`, and
  after reveal `D-pad ↑/↓: answer`, avoiding the previous ambiguous generic
  `scroll` label.
- Verification passed: focused flashcard/deck-selector contract binaries,
  `make test`, `git diff --check`, and `make -C app-3ds`.

## 2026-06-07 Compact Due-Day Scheduler Checkpoint

- Clean-shell compact `state.tsv` is now version `2` when saved by the app.
  Version `1` compact states remain accepted and load with introduced cards due
  immediately.
- `study_backend` persists optional schedule triples for introduced cards:
  `schedule_index`, `schedule_due_day`, and `schedule_interval_days`. The
  loader rejects schedule rows in version `1`, mismatched triples, duplicate or
  out-of-range schedule indexes, schedules for non-introduced cards, and
  intervals above `36500` days.
- The current clean-shell day scheduler is intentionally simple: Again is due
  today with interval `0`; Hard uses the previous interval or starts at `1`;
  Good doubles the previous interval or starts at `2`; Easy triples the
  previous interval or starts at `4`.
- Queue selection and deck-selector summaries now respect future `due_day`
  rows. New cards can fill the queue while a Good/Easy card is scheduled for a
  later day, and due introduced cards still take priority over new cards when
  their due day arrives.
- `review again` on the no-due summary forces same-day completed cards back to
  the current `progress_day` before clearing `completed_today`, preserving the
  user-requested extra review pass even if those cards were normally scheduled
  for the future.
- Focused verification passed for `tests/test_study_backend.c`,
  `tests/test_study_deck_index.c`, `tests/test_clean_shell_daily_use.c`, and
  `tests/test_verify_m7_artifacts.py`. Broad verification passed with
  `make test`, `git diff --check`, `make -C app-3ds`, and
  `make verify-m7-preflight`.

## 2026-06-07 Azahar Control Split And Preflight Checkpoint

- Updated `tools/verify_azahar_controls.py` so the emulator profile keeps
  D-pad on keyboard arrows and maps Circle Pad to `W`/`S`/`Q`/`E`. This lets
  manual Azahar testing exercise answer scrolling and question/front scrolling
  independently instead of both sources collapsing onto arrow keys.
- Added `tools/verify_azahar_controls.py --fix` and `make fix-azahar-controls`
  to repair the active local Azahar profile intentionally. The live
  `~/Library/Application Support/Azahar/config/qt-config.ini` was fixed and
  verified.
- `make verify-m7-preflight` passed after the control split. It ran the full
  host/tool suite, verified tracked samples, refreshed FE assets, built/staged
  local and package SD roots, staged fresh tracked samples into Azahar SDMC, and
  verified the live Azahar keyboard profile.
- Current Azahar SD deck order is `leetcode`, `limits-demo`, `recsys`,
  `sample`, `sat-vocabulary`, `vim`. `make drive-azahar-m7-smoke-dry-run`
  plans one Down press to reach `limits-demo`, then two Down presses from there
  to reach `sample`.
- Remaining M7 evidence is manual or hardware acceptance: inspect the latest
  footer placement, confirm arrows scroll the answer and `W`/`S` scroll the
  question/front, run the daily-use actions, then verify artifacts with the
  exact decks/settings used during that pass.

## 2026-06-07 Fixed Status Baseline And Azahar Launch Checkpoint

- Fixed the moving status/footer baseline reported from Azahar screenshots.
  The top deck selector meta line and top review meta line now share the same
  renderer baseline: `menu_top_meta y=178` and `review_top_meta y=178`.
  Bottom footer/status text now draws as a fixed one-line status at
  `bottom_footer y=174`; the renderer no longer shifts that line upward based
  on wrapped-row count.
- The live app wrote `sdmc:/3ds/anki3ds/renderer.tsv` with:
  `menu_top_meta 49 178`, `review_top_meta 49 178`, and
  `bottom_footer 34 174`, proving Azahar initialized the updated renderer.
- Moved `study_deck_index_load_state_summary` away from returning a large
  `study_deck_index_state_summary` by value. Deck summaries now allocate that
  temporary on the heap, avoiding a 3DS startup stack-pressure pattern seen
  while scanning imported decks before `session.tsv` was written.
- `make run-emulator` and `make run-emulator-fe-bg-viewer` now launch Azahar
  detached with `open -n -a ... --args --windowed <3dsx>`. This matches the
  reliable launch path observed during manual restart; the older document-open
  form could leave Azahar menu-only or fail to refresh visible window state.
- Verification passed: `make test`, `make -C app-3ds`, `git diff --check`,
  `make verify-m7-preflight`, and `make drive-azahar-m7-smoke-dry-run`.
  Azahar was relaunched detached afterward, and `renderer.tsv` plus
  `session.tsv` were rewritten at launch.

## 2026-06-07 Control Mapping Checkpoint

- Current deck selector mapping after this pass: `A` opens the selected deck,
  `START` toggles help, `SELECT` rescans, `Y` opens exit confirmation,
  D-pad up/down changes deck, D-pad left/right pages, and `L/R` page decks.
- Current review mapping after this pass: `A` reveals and then rates Again,
  `B` undoes the previous rating, `L` rates Hard after reveal, `X` opens
  settings before reveal and rates Good after reveal, `Y` opens restart/reset
  before reveal and rates Easy after reveal, `R` suspends/restores, `SELECT`
  returns to deck select, and `START` toggles help.
- Exit from review is now intentionally indirect: `SELECT` returns to deck
  select, then `Y` opens exit confirmation. Settings and confirm screens still
  keep their modal controls (`START` exit from settings, `B/SELECT` cancel).
- Renderer fixed-line footer/status drawing now has an explicit helper
  prototype, avoiding a C build-order hazard introduced during the previous
  one-line status truncation pass.
- Verification passed: `make test`, `make -C app-3ds`, and
  `make verify-m7-preflight`.
