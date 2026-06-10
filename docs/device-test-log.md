# Device Test Log

Device:

- model: Nintendo 3DS `CTR-001`
- firmware/custom firmware:
- SD card:

## Template

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

For M7 daily-use acceptance, include the deck ids tested, the copy method or
fresh-sample command used before copying, the settings values changed, and
whether relaunch persistence was confirmed on the `CTR-001`.

## M7 Daily-Use Acceptance Checklist

Run this checklist on the `CTR-001` after a clean local preflight:

```sh
make verify-m7-preflight
```

Copy `dist/sdmc/3ds/anki3ds/` to the SD card so the device has the current
`.3dsx`, `.smdh`, and tracked sample decks. Test `sample` and `limits-demo`.
Before launching a fresh acceptance pass, remove any stale root
`3ds/anki3ds/session.tsv`, `.tmp`, and `.bak` diagnostics from the tested SD
root, or use the fresh prep targets that do this automatically.

Record the build commit, copy method, SD card, and whether the battery line was
unavailable, normal, charging, or low.

Pass conditions:

- Deck selector shows both tracked decks, selected deck position, due counts,
  readable FE text, and row-marker selection on the parchment/FE shell.
- D-pad or Circle Pad Up/Down moves one deck per quick tap; holding a single
  direction repeats only after the expected short delay.
- Pressing a command while a navigation direction is already held, such as
  `A` while holding Down, is ignored as a chord instead of firing the command.
- `A` opens the selected deck.
- The app starts in the centralized Citro2D FE shell with the generated Forest
  background and parchment dialogue panels. There should be no console-black
  startup screen or ANSI color clash.
- On `sample`, `A` reveals the first card. After reveal, `A` Again, `L` Hard,
  `X` Good, and `Y` Easy each save and advance or report that the same card is
  still due. The card front remains on the top screen, the back appears on the
  bottom screen, and ambiguous rating-button chords must not rate a card.
- `B` undoes the last rating and leaves the restored card due again. Pressing
  `B` with no undo history reports `Nothing to undo`.
- Long text on `sample` card `card-0011` scrolls with D-pad Up/Down on the
  bottom answer pane and Circle Pad Up/Down on the top front/question pane;
  quick taps should not jump multiple rows.
- `R`, then `X`, suspends a review card. On the completion screen, `R`, then
  `X`, restores suspended cards. Pressing `R` on a completed deck with no
  suspended cards reports `Nothing suspended`.
- On `limits-demo`, rating the visible new cards reaches the daily limit
  summary and shows hidden new/review counts instead of looking simply done.
- Before reveal, `X` opens Study Settings. D-pad Up/Down selects `new_limit`,
  `review_limit`, or learning mode; D-pad Left/Right changes values; `X`
  saves; and the edits update due counts immediately and persist after exit
  and relaunch.
- Before reveal, `Y` opens reset confirmation. `X` confirms reset; it removes
  review progress while keeping `settings.tsv`.
- `START` toggles help on deck/review screens. From review, use `SELECT` to
  return to the deck selector, then `Y` to open exit confirmation. From Study
  Settings, `START` opens exit confirmation. `B` or `SELECT` cancels back to
  the prior screen; `A` confirms exit.
- After relaunch, reviewed cards, suspended/restored state, daily-limit edits,
  and deck selection behavior match the saved SD-card state.
- If the battery line transitions to low and not charging, the status line
  announces `Battery low; charge soon` once for that low-battery episode. If it
  is already low before the first drawn screen, this warning may be the initial
  status.
- If the battery line is low and not charging during a saved rating, undo,
  suspend, restore, reset, or daily-limit save, the status line includes
  `; batt low` after confirming the save. If the battery is unavailable,
  normal, or charging, no low-battery save suffix is required.
- On the SD card, the active test decks have expected app-owned files:
  `state.tsv` after saved study, `settings.tsv` after limit edits, and
  `review-log.tsv` after saved study actions unless the status reported
  `log skipped`. Saved-action session counters should not exceed matching
  review-log event counts unless the pass uses the `log skipped` verifier
  allowance or verifies a deck whose progress was reset at the end. The root
  `3ds/anki3ds/session.tsv` has a complete session
  snapshot with `launch_count` at least `2`, monotonic session time/day fields,
  scan, deck-open, internally consistent deck-open outcome, action, reset,
  settings-save, review-screen, answer-reveal, and exit counters. Answer
  reveal should persist introduced-card state and update session diagnostics.
- After the pass, `make verify-m7-artifacts M7_SDMC=/path/to/sdmc` accepts the
  tested SD root. For Azahar, omit `M7_SDMC` to use the configured
  `AZAHAR_SDMC`. Include
  `M7_EXPECT_SETTINGS="deck:new:review[:learning_mode] ..."` for the exact
  settings values changed during the pass; `learning_mode` is optional and uses
  `0` for Due first or `1` for Cooldown. Each expected-settings deck must be
  part of `M7_DECKS` or `M7_RESET_DECKS`; the verifier fails if these
  expectations are omitted. The root `session.tsv` must end with
  `last_event=exit_confirmed`; use the confirm-exit flow after the final checked
  action. If the app reported `log skipped`, pass
  `M7_ALLOW_MISSING_REVIEW_LOG=1` so incomplete diagnostic review-log evidence
  is accepted while progress/settings/session checks still run. If required
  event types are missing from the remaining log rows, also pass
  `M7_NO_REQUIRED_EVENTS=1`. If reset was the final state for a deck, also pass
  `M7_RESET_DECKS="deck-id"` so the verifier expects no progress files while
  still checking `settings.tsv`.

Fail the checkpoint for any uncontrolled multi-step movement from a quick
direction tap, any rating button that does not advance or clearly keep the same
due card, unreadable selected/status colors, lost state after relaunch, or a
save failure that advances the visible queue.
