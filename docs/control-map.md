# Control Map

The app shows Nintendo 3DS button names on screen. In Azahar, those buttons are
pressed with keyboard keys from the emulator's default control profile, so some
letters do not match the 3DS button label.

| 3DS button | Azahar key | Main use |
| --- | --- | --- |
| `A` | `A` | Open deck, reveal answer, choose Easy, confirm selected action, save limits, confirm exit |
| `B` | `S` | Choose Good after reveal, go back, cancel |
| `X` | `Z` | Choose Hard after reveal, confirm restore/suspend/reset |
| `Y` | `X` | Choose Again after reveal, open controls where available |
| `L` | `Q` | Undo last rating or suspend action |
| `R` | `W` | Open suspend confirmation |
| D-pad or Circle Pad Up | `T` | Move selection up, scroll review text up |
| D-pad or Circle Pad Down | `G` | Move selection down, scroll review text down |
| D-pad or Circle Pad Left | `F` | Page deck list up with wrap, decrease daily-limit preset |
| D-pad or Circle Pad Right | `H` | Page deck list down with wrap, increase daily-limit preset |
| `SELECT` | `N` | Rescan decks, open actions, cancel actions, return from settings |
| `START` | `M` | Open exit confirmation |

Tapping a D-pad or Circle Pad direction moves, pages, or scrolls once. Holding
a single direction for roughly 300 ms starts repeat movement, paging,
review-text scrolling, or daily-limit value changes, then repeats roughly every
80 ms. Daily-limit field selection with Up/Down is single-step so a held button
does not bounce between fields. The app keeps the input loop responsive while a
repeatable direction is held so deck selection, action selection, value
changes, and review text scrolling do not fall into the idle backoff cadence.
Pressing multiple directions together does not move or change values.
Directions pressed or held together with command buttons are ignored.
Face-button actions such as reveal, rating, save, reset, and exit remain
single-press actions. After reveal, pressing more than one rating button at the
same time does not save a rating. Save, restore-confirm, suspend-confirm,
reset, action-confirm, exit-open, and exit-confirm buttons are also ignored
when another button is active or held.

The deck selector shows the current position as `selected/total` on both
screens. Up/Down moves one deck with wrap; Left/Right pages by the visible
list size and wraps between the first and last decks. Basic terminal-style
colors are used for status: cyan headings/status labels, magenta selected or
focused items, white neutral status and separators, yellow cautions, green
saved/safe state, and red errors/reset actions. Pure blue is intentionally
avoided because it is hard to read on the dark 3DS console background; Easy
uses bright cyan as the readable cool-color substitute.
If deck scan ignores non-hidden entries because they are not valid deck ids or
do not contain `cards.tsv`, the selector shows an ignored count.
If a deck has both ignored settings and unmatched saved state, the selector row
uses the compact `settings! state!` warning and the bottom screen shows details.

## Review Flow

1. Press `A` to reveal the answer.
2. Rate the card with `Y` Again, `X` Hard, `B` Good, or `A` Easy.
3. Use D-pad Up/Down to scroll long front text before reveal or long back text
   after reveal. The active text pane shows a compact `^ 1/3 v` style cue when
   more rows are available.
4. Press `L` to undo the last rating or suspend action.
5. Press `R`, then `X`, to suspend the current card.
6. Press `SELECT` to open deck actions.

After a rating, the bottom status line shows whether the rating saved and which
card is next. If saving fails, the app keeps the old scheduler state and shows
that the card did not advance. Navigation, action selection, daily-limit value
changes, and answer reveal also update the bottom status line so manual input
has immediate feedback. If daily limits hide more calendar-due cards after the
visible queue empties, the summary says `Daily limit reached` and shows new and
review cards past the limit.

## Controls Screen

Press `Y` from deck select, load error, unrevealed review cards, summary,
actions, or daily limits to show the in-app controls screen. Press `B`, `Y`, or
`SELECT` to return. On a revealed review card, `Y` is reserved for Again.
If `START` opens exit confirmation from the controls screen, canceling exit
returns to the controls screen.
If `START` opens exit confirmation from daily limits, or from controls opened
by daily limits, while edits are unsaved, the confirmation screen warns that
those limit edits will be lost.
Controls opened from daily limits also show `Unsaved limit edits` while the
edit buffer differs from the active saved limits. Opening or returning from
controls, and canceling exit back to daily limits or its controls screen, keeps
the unsaved warning visible on the status line.

The controls screen is contextual. Its top screen lists controls for the screen
that opened it, including empty deck-list and review-state-error variants, so it
does not show review-only controls from deck select, load error, summary, or
daily limits.

## Actions Flow

From review or summary, `SELECT` opens actions. The default selected action is
restore suspended cards. If a deck opens with malformed saved state, reset is
selected by default so the repair flow is immediately reachable.
The action, daily-limit, and restore/suspend/reset confirmation screens show
the active deck name on the bottom controls screen as well as the top screen.

- Use D-pad Up/Down to choose restore suspended, daily limits, or reset.
- Press `A` to choose the selected action.
- Press `B` or `SELECT` to cancel.
- Restoring suspended cards requires `X` on the restore confirmation screen if
  any suspended cards exist.
- Choosing restore when no cards are suspended keeps the actions screen open
  and reports `Nothing suspended`.
- Reset progress requires `X` on the reset confirmation screen.
- A successful reset reloads the deck and shows `Progress reset`; if only the
  diagnostic log cleanup fails, it shows `Progress reset; log kept`.
- If a deck opens with malformed saved state, use this reset flow before study;
  normal study controls such as undo remain disabled until reset succeeds.

Suspending from review also uses a confirmation screen:

- Press `R` on the current card to open suspend confirmation.
- Press `X` to suspend the card.
- Press `B` or `SELECT` to cancel.

## Daily Limits

In the daily-limits screen:

- D-pad Up/Down chooses `new_limit` or `review_limit`.
- D-pad Left/Right cycles preset values and repeats while held.
- `A` saves.
- `B` or `SELECT` returns to actions without saving.

`0` means all available cards.
The screen shows `no changes` when the edited values match the active saved
limits and warns about `unsaved changes` after a value change. The bottom
status line uses the warning color for unsaved edits, including after moving
between fields while edits are still dirty. Canceling with unsaved changes
discards the edit buffer and returns to actions.
Saving limits updates `settings.tsv` and leaves review progress status separate.
