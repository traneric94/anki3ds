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
  and readable dark-terminal colors without hard-to-read blue text.
- D-pad or Circle Pad Up/Down moves one deck per quick tap; holding a single
  direction repeats only after the expected short delay.
- `A` opens the selected deck, `Y` opens contextual help before reveal, and
  `B`, `Y`, or `SELECT` closes help.
- On the help screen, `X` cycles the panel theme between Amber, Forest, Ruby,
  and Chalk without changing the active deck or daily-limit settings.
- On `sample`, `A` reveals the first card. After reveal, `Y` Again, `X` Hard,
  `B` Good, and `A` Easy each save and advance or report that the same card is
  still due. The card front remains on the top screen, the back appears on the
  bottom screen, and ambiguous face-button chords must not rate a card.
- `L` undoes the last rating or suspend action and leaves the restored card due
  again.
- Long text on `sample` card `card-0011` scrolls with D-pad Up/Down on the
  active text pane; quick taps should not jump multiple rows.
- `R`, then `X`, suspends a review card. `SELECT` actions, restore suspended,
  then `X` restores it.
- On `limits-demo`, rating the visible new cards reaches the daily limit
  summary and shows hidden new/review counts instead of looking simply done.
- From actions, Daily limits can change `new_limit` and `review_limit`, save
  with `A`, update due counts immediately, and persist after exit and relaunch.
- Reset progress requires the reset confirmation and `X`; it removes review
  progress while keeping `settings.tsv`.
- `START` opens exit confirmation from each major screen. `B` or `SELECT`
  cancels back to the prior screen; `A` exits.
- After relaunch, reviewed cards, suspended/restored state, daily-limit edits,
  and deck selection behavior match the saved SD-card state.
- If the battery line is low and not charging during a saved rating, undo,
  suspend, restore, reset, or daily-limit save, the status line includes
  `; batt low` after confirming the save. If the battery is unavailable,
  normal, or charging, no low-battery save suffix is required.
- On the SD card, the active test decks have expected app-owned files:
  `state.tsv` after saved study, `settings.tsv` after limit edits, and
  `review-log.tsv` after saved study actions unless the status reported
  `log skipped`. The root `3ds/anki3ds/session.tsv` has a complete session
  snapshot with scan, deck-open, action, reset, and exit counters. Answer
  reveals are allowed to appear only after the next snapshot write because they
  do not write the diagnostic file by themselves.
- After the pass, `make verify-m7-artifacts M7_SDMC=/path/to/sdmc` accepts the
  tested SD root. For Azahar, omit `M7_SDMC` to use the configured
  `AZAHAR_SDMC`. Include `M7_EXPECT_SETTINGS="deck:new:review ..."` for the
  exact daily-limit values changed during the pass; each expected-settings deck
  must be part of `M7_DECKS` or `M7_RESET_DECKS`. If the app reported
  `log skipped`, pass `M7_ALLOW_MISSING_REVIEW_LOG=1` and
  `M7_NO_REQUIRED_EVENTS=1`. If reset was the final state for a deck, also
  pass `M7_RESET_DECKS="deck-id"` so the verifier expects no progress files
  while still checking `settings.tsv`.

Fail the checkpoint for any uncontrolled multi-step movement from a quick
direction tap, any rating button that does not advance or clearly keep the same
due card, unreadable selected/status colors, lost state after relaunch, or a
save failure that advances the visible queue.
