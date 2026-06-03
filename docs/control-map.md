# Control Map

The app shows Nintendo 3DS button names on screen. In Azahar, those buttons are
pressed with keyboard keys from the emulator's default control profile, so some
letters do not match the 3DS button label.

| 3DS button | Azahar key | Main use |
| --- | --- | --- |
| `A` | `A` | Open deck, reveal answer, choose Easy, confirm selected action, save limits, confirm exit |
| `B` | `S` | Choose Good after reveal, go back, cancel |
| `X` | `Z` | Choose Hard after reveal, confirm reset progress |
| `Y` | `X` | Choose Again after reveal, open controls where available |
| `L` | `Q` | Undo last rating or suspend action |
| `R` | `W` | Suspend current card |
| D-pad Up | `T` | Move selection up |
| D-pad Down | `G` | Move selection down |
| D-pad Left | `F` | Decrease daily-limit preset |
| D-pad Right | `H` | Increase daily-limit preset |
| `SELECT` | `N` | Rescan decks, open actions, cancel action/settings screens |
| `START` | `M` | Open exit confirmation |

Holding a single D-pad direction repeats movement or daily-limit value changes
after a short delay. The app keeps the input loop responsive while a button is
held so deck and action selection do not fall into the idle backoff cadence.
Pressing multiple D-pad directions together does not move or change values.
D-pad directions pressed together with command buttons are ignored. Face-button
actions such as reveal, rating, save, reset, and exit remain single-press
actions. After reveal, pressing more than one rating button at the same time
does not save a rating. Save, reset, action-confirm, exit-open, and
exit-confirm buttons are also ignored when pressed together with another
button.

The deck selector shows the current position as `selected/total` on both
screens. Basic terminal-style colors are used for status: blue headings, green
selected/saved/safe items, red errors/reset actions, and yellow cautions.

## Review Flow

1. Press `A` to reveal the answer.
2. Rate the card with `Y` Again, `X` Hard, `B` Good, or `A` Easy.
3. Press `L` to undo the last rating or suspend action.
4. Press `R` to suspend the current card.
5. Press `SELECT` to open deck actions.

After a rating, the bottom status line shows whether the rating saved and which
card is next. If saving fails, the app keeps the old scheduler state and shows
that the card did not advance.

## Controls Screen

Press `Y` from deck select, load error, unrevealed review cards, summary,
actions, or daily limits to show the in-app controls screen. Press `B`, `Y`, or
`SELECT` to return. On a revealed review card, `Y` is reserved for Again.
If `START` opens exit confirmation from the controls screen, canceling exit
returns to the controls screen.

## Actions Flow

From review or summary, `SELECT` opens actions. The default selected action is
restore suspended cards. If a deck opens with malformed saved state, reset is
selected by default so the repair flow is immediately reachable.

- Use D-pad Up/Down to choose restore suspended, daily limits, or reset.
- Press `A` to confirm the selected action.
- Press `B` or `SELECT` to cancel.
- Reset progress requires `X` on the reset confirmation screen.
- A successful reset reloads the deck and shows `Progress reset`.
- If a deck opens with malformed saved state, use this reset flow before study;
  normal study controls such as undo remain disabled until reset succeeds.

## Daily Limits

In the daily-limits screen:

- D-pad Up/Down chooses `new_limit` or `review_limit`.
- D-pad Left/Right cycles preset values.
- `A` saves.
- `B` or `SELECT` cancels.

`0` means all available cards.
Saving limits updates `settings.tsv` and leaves review progress status separate.
