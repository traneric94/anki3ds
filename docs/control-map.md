# Control Map

The app shows Nintendo 3DS button names on screen. In Azahar, those buttons are
pressed with keyboard keys from the emulator's default control profile, so some
letters do not match the 3DS button label.

| 3DS button | Azahar key | Main use |
| --- | --- | --- |
| `A` | `A` | Open deck, reveal answer, choose Easy, confirm selected action, save limits, confirm exit |
| `B` | `S` | Choose Good after reveal, go back, cancel |
| `X` | `Z` | Choose Hard after reveal, confirm reset progress |
| `Y` | `X` | Choose Again after reveal |
| `L` | `Q` | Undo last rating or suspend action |
| `R` | `W` | Suspend current card |
| D-pad Up | `T` | Move selection up |
| D-pad Down | `G` | Move selection down |
| D-pad Left | `F` | Decrease daily-limit preset |
| D-pad Right | `H` | Increase daily-limit preset |
| `SELECT` | `N` | Rescan decks, open actions, cancel action/settings screens |
| `START` | `M` | Open exit confirmation |

## Review Flow

1. Press `A` to reveal the answer.
2. Rate the card with `Y` Again, `X` Hard, `B` Good, or `A` Easy.
3. Press `L` to undo the last rating or suspend action.
4. Press `R` to suspend the current card.
5. Press `SELECT` to open deck actions.

## Actions Flow

From review or summary, `SELECT` opens actions. The default selected action is
restore suspended cards.

- Use D-pad Up/Down to choose restore suspended, daily limits, or reset.
- Press `A` to confirm the selected action.
- Press `B` or `SELECT` to cancel.
- Reset progress requires `X` on the reset confirmation screen.

## Daily Limits

In the daily-limits screen:

- D-pad Up/Down chooses `new_limit` or `review_limit`.
- D-pad Left/Right cycles preset values.
- `A` saves.
- `B` or `SELECT` cancels.

`0` means all available cards.
