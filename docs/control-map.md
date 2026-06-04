# Control Map

The app shows Nintendo 3DS button names on screen. The local Azahar profile is
configured so the main 3DS buttons use matching keyboard labels where possible:
face buttons use `A`/`B`/`X`/`Y`, D-pad directions use the keyboard arrows, and
shoulder buttons use `L`/`R`.

| 3DS button | Azahar key | Main use |
| --- | --- | --- |
| `A` | `A` | Open deck, reveal answer, choose Easy, confirm selected action, save limits, confirm exit |
| `B` | `B` | Choose Good after reveal, go back, cancel |
| `X` | `X` | Choose Hard after reveal, confirm restore/suspend/reset, cycle theme on help page |
| `Y` | `Y` | Choose Again after reveal, open help before reveal and where available |
| `L` | `L` | Undo last rating or suspend action |
| `R` | `R` | Open suspend confirmation |
| D-pad or Circle Pad Up | Up Arrow | Move selection up, scroll review text up |
| D-pad or Circle Pad Down | Down Arrow | Move selection down, scroll review text down |
| D-pad or Circle Pad Left | Left Arrow | Page deck list up with wrap, decrease daily-limit preset |
| D-pad or Circle Pad Right | Right Arrow | Page deck list down with wrap, increase daily-limit preset |
| `SELECT` | `N` | Rescan decks, open actions, cancel actions, return from settings |
| `START` | `M` | Open exit confirmation |

The Azahar config file is `~/Library/Application Support/Azahar/config/qt-config.ini`.
If Azahar rewrites the profile, reapply the same bindings in
`Emulation > Configure > Controls`.
To verify the local profile before an M7 emulator pass, run:

```sh
make verify-azahar-controls
```

Set `AZAHAR_CONFIG=/path/to/qt-config.ini` if the config file lives somewhere
else.

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
colors use an amber/chalk/green/red palette. Review text appears in an original
light paper flashcard panel with black text, while the rest of the app stays in
a dark terminal shell. Before reveal, the front of the card is on the top
screen and compact prompts/status are on the bottom screen. After reveal, the
front stays on the top screen and the back moves to the bottom screen with the
four rating buttons as bracketed chips below it. Help-page `X` cycles the card
panel trim through four session-local themes: Amber, Forest, Ruby, and Chalk.
Warm amber is used for headings, labels, key prompts, Hard ratings, learning
counts, cautions, and normal reverse-video selected or focused items; green for
Good ratings, review counts, and saved/safe state; red reverse-video for
selected reset actions; red for Again ratings, suspended counts, errors, and
reset actions; bright white for Easy ratings, new counts, and neutral values,
with dim white separators and version text. Blue, cyan, and violet are
intentionally avoided because they are hard to read on the dark 3DS console
background.
If deck scan ignores non-hidden entries because they are not valid deck ids or
do not contain `cards.tsv`, the selector shows an ignored count.
If daily limits hide otherwise due cards, the deck row shows a `limit` warning
and the bottom selected-deck details show the hidden new/review counts. Moving
onto that deck also reports `limit reached` in the status line.
If a deck has both ignored settings and unmatched saved state, the selector row
uses the compact `settings! state!` warning, deck-move status reports the
warning, and the bottom screen shows details.

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
that the card did not advance. Successful rating, suspend, restore, and undo
save feedback keep active deck warning context visible unless a stronger
save-failure or daily-limit-complete message takes priority. Navigation, action
selection, daily-limit value changes, review-text scrolling, and answer reveal
also update the bottom status line so manual input has immediate feedback and
active warning context stays visible. If daily limits hide more calendar-due
cards after the visible queue empties, the summary says `Daily limit reached`
and shows new and review cards past the limit.
When the battery sample is low and not charging, successful save-oriented
actions add `; batt low` to the status line while the bottom battery line keeps
showing the full low-battery prompt.

## Help Screen

Press `Y` from deck select, load error, unrevealed review cards, summary,
actions, or daily limits to show the in-app help screen. Press `B`, `Y`, or
`SELECT` to return. Press `X` on the help screen to cycle the card panel theme.
On a revealed review card, `Y` is reserved for Again.
Help opened from deck-specific screens keep warning status visible,
including selected-deck, load-error, reset-needed, ignored-settings,
unmatched-state, and daily-limit warnings. Returning from help preserves
those warnings; returning to the deck selector restores the selected-deck
status.
If `START` opens exit confirmation from the help screen, canceling exit
returns to the help screen and restores that help-screen status.
If `START` opens exit confirmation from daily limits, or from help opened
by daily limits, while edits are unsaved, the confirmation screen warns that
those limit edits will be lost.
Otherwise, opening exit confirmation keeps selected-deck, load-error, help,
or active-deck warning context visible in the status line.
Help opened from daily limits also show `Unsaved limit edits` while the
edit buffer differs from the active saved limits. Opening or returning from
help, and canceling exit back to daily limits or its help screen, keeps
the unsaved warning visible on the status line.

The help screen is contextual. Its top screen lists controls for the screen
that opened it, including empty deck-list and review-state-error variants, so it
does not show review-only controls from deck select, load error, summary, or
daily limits.
In-app key prompts color the active key names in amber and avoid bare `L/R`
wording for deck paging so D-pad left/right is not confused with the shoulder
buttons.

## Actions Flow

From review or summary, `SELECT` opens actions. The default selected action is
restore suspended cards. If a deck opens with malformed saved state, reset is
selected by default so the repair flow is immediately reachable.
Opening actions preserves active deck warnings in the status line, including
reset-needed state, ignored settings, unmatched state, and daily-limit
exhaustion. Moving through actions preserves that warning context too, and the
reset action keeps a warning status even on otherwise healthy decks. Opening or
canceling restore/suspend/reset confirmations preserves that warning context
too. Canceling exit from actions or those confirmations also preserves the
active deck warning context.
Opening daily limits preserves the same active deck warning context. The
action, daily-limit, and restore/suspend/reset confirmation screens show the
active deck name on the bottom help/status screen as well as the top screen.

- Use D-pad Up/Down to choose restore suspended, daily limits, or reset.
- Press `A` to choose the selected action.
- Press `B` or `SELECT` to cancel.
- Restoring suspended cards requires `X` on the restore confirmation screen if
  any suspended cards exist.
- Choosing restore when no cards are suspended keeps the actions screen open
  and reports `Nothing suspended` with active deck warning context when present.
- Reset progress requires `X` on the reset confirmation screen.
- A successful reset reloads the deck and shows `Progress reset` with active
  deck warning context when present; if only review-log cleanup fails,
  it shows `Progress reset; log kept`.
- If a deck opens with malformed saved state, use this reset flow before study;
  normal study controls such as undo remain disabled until reset succeeds.

Suspending from review also uses a confirmation screen:

- Press `R` on the current card to open suspend confirmation.
- Press `X` to suspend the card.
- Press `B` or `SELECT` to cancel.
No-op undo, suspend, and restore attempts keep active deck warning context in
the status line.

## Daily Limits

In the daily-limits screen:

- D-pad Up/Down chooses `new_limit` or `review_limit`.
- D-pad Left/Right cycles preset values and repeats while held.
- `A` saves.
- `B` or `SELECT` returns to actions without saving.

`0` means all available cards.
The screen shows `no changes` when the edited values match the active saved
limits and warns about `unsaved changes` after a value change. The bottom
status line uses the warning color for active deck warnings on entry, after
clean field movement or value movement, and for unsaved edits. Unsaved edits
take priority over the active deck warning. Canceling with unsaved changes
discards the edit buffer and returns to actions while restoring active deck
warning context.
Saving limits updates `settings.tsv` and leaves review progress status separate.
Save feedback keeps active deck warning context visible when present, while
omitting redundant reset-state or daily-limit wording.
