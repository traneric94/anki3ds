# Control Map

The app shows Nintendo 3DS button names on screen. The local Azahar profile is
configured so the main 3DS buttons use matching keyboard labels where possible:
face buttons use `A`/`B`/`X`/`Y`, D-pad directions use the keyboard arrows,
Circle Pad uses a separate `W`/`S`/`Q`/`E` cluster, and shoulder buttons use
`L`/`R`.

| 3DS button | Azahar key | Main use |
| --- | --- | --- |
| `A` | `A` | Open deck, reveal answer, review again on completion, choose Again, confirm exit |
| `B` | `B` | Undo last rating in review, cancel settings or confirmations |
| `X` | `X` | Open study settings before reveal, choose Good after reveal, save settings, confirm restore/suspend/reset |
| `Y` | `Y` | Choose Easy after reveal, open reset before reveal, open exit from deck selector |
| `L` | `L` | Page deck list up on selector, choose Hard after reveal |
| `R` | `R` | Page deck list down on selector, open suspend or restore confirmation in review |
| D-pad Up/Down | Up/Down Arrow | Move deck selection, scroll answer text after reveal |
| Circle Pad Up/Down | `W`/`S` | Move deck selection, scroll question/front text in review |
| D-pad Left/Right | Left/Right Arrow | Page deck list with wrap, adjust settings preset |
| Circle Pad Left/Right | `Q`/`E` | Same left/right navigation as D-pad |
| `SELECT` | Backspace | Rescan decks, return to deck selector, cancel settings or confirmations |
| `START` | Enter | Toggle help on deck/review screens, open exit from settings |

The Azahar config file is `~/Library/Application Support/Azahar/config/qt-config.ini`.
If Azahar rewrites the profile, reapply the same bindings in
`Emulation > Configure > Controls`.
To verify the local profile before an M7 emulator pass, run:

```sh
make verify-azahar-controls
```

Run `make fix-azahar-controls` to rewrite the active local profile with the
expected bindings.
Set `AZAHAR_CONFIG=/path/to/qt-config.ini` if the config file lives somewhere
else.

A repeatable Azahar smoke driver is available for the tracked sample deck flow:

```sh
make run-emulator-m7-smoke
```

The target assumes fresh sample staging and discovers the current Azahar SD deck
folder order before sending keys, so personal imported decks may be present.
It navigates to `limits-demo`, saves study settings as `5` new and `10` review,
saves a rating, undoes it with `B`, suspends and restores all six active cards,
returns to deck select to confirm exit with `Y` then `A`, relaunches once,
reopens `limits-demo`, returns to the deck selector, navigates to `sample`,
rates one card, resets `sample`, returns to deck select for final exit, then
runs the artifact verifier with `M7_DECKS=limits-demo`,
`M7_RESET_DECKS=sample`, and
`M7_EXPECT_SETTINGS="limits-demo:5:10 sample:20:200"`. To rerun only that
post-smoke verifier, use `make verify-azahar-m7-smoke-artifacts`. To inspect
the planned key sequence without controlling Azahar, run
`make drive-azahar-m7-smoke-dry-run`.
macOS must grant Accessibility permission to the terminal or Codex host process
that runs the target; otherwise `osascript` cannot send the key events.

Each physical key press is handled with both the 3DS key-down and key-held
snapshots. Mixed button chords are rejected by the screen-specific reducers, so
save, reset, suspend/restore confirmation, exit confirmation, settings edits,
rating, and review actions do not fire when another button is pressed with
them. This also applies when a command is pressed while a navigation key is
already held, such as pressing `A` while holding Down.

The D-pad and Circle Pad are preserved as separate navigation sources before
the shell handles a frame. In review, D-pad Up/Down scrolls the answer on the
bottom screen while Circle Pad Up/Down scrolls the front/question on the top
screen. If both physical navigation sources are active in the same frame, the
input frame treats that as ambiguous and emits no navigation action, even when
both sources point in the same direction.

Pure navigation holds repeat after a short delay: deck select repeats
Up/Down/Left/Right and L/R page movement, review repeats Up/Down scrolling,
and study settings repeat Left/Right value changes. Confirmation screens do
not repeat held input.

The app starts on the deck selector. Up/Down moves one deck with wrap;
Left/Right pages by the visible list size, and `L`/`R` provide the same fast
deck-page movement from the shoulder buttons. `A` opens the selected deck;
`START` toggles the compact/detailed legend; `SELECT` rescans SD; `Y`
opens exit confirmation. The selector reports the current deck window and any
ignored entries from SD scanning. When stats are available, each deck row labels
`Due`, `New`, and `Susp` counts explicitly; malformed deck state is marked as
`state?`. The bottom footer summarizes learning across the whole deck list as
`All decks: Due ... | New ... | Susp ...`, with an `Issues` count when a deck's
state or stats cannot be trusted.

The active app uses the centralized Citro2D FE shell. Review text is drawn from
backend strings on top of embedded parchment/background textures through one
shared Citro2D text scale. Before reveal, the current prompt is shown as the
primary review text on the top screen. After reveal, the answer is shown on
the bottom screen. D-pad Up/Down scrolls the answer panel; circle-pad Up/Down
scrolls the front/question panel. Side scrollbars appear when more rows are
available. If a revealed card has tags, the answer side shows one chip per tag.
Otherwise the footer spells out daily counters as `Today New x/y` and
`Review x/y`.

## Review Flow

1. Press `A` to reveal the answer.
2. Rate the card with `A` Again, `L` Hard, `X` Good, or `Y` Easy.
3. Use Circle Pad Up/Down to scroll the top front/question panel. Use D-pad
   Up/Down to scroll the bottom answer panel after reveal.
4. Press `B` to undo the last rating.
5. Press `R`, then `X`, to suspend the current card.
6. Press `R`, then `X`, on the completion screen to restore all suspended
   cards when any are suspended.
7. Press `A` on the completion screen to review introduced cards again without
   resetting progress.
8. Press `X` before reveal to edit study settings.
9. Press `Y` before reveal, or on the completion screen, to reset deck
   progress after confirmation.
10. Press `SELECT` to return to the deck selector.
11. Press `START` before reveal, after reveal, on completion, or on the deck
    selector to toggle the compact/detailed legend.
12. To exit from review, press `SELECT` to return to deck select, then `Y` to
    open exit confirmation.

## Legend Toggle

The bottom legend is compact by default. `START` expands or collapses it on the
deck selector and review screens.
Expanded help uses grouped `button: action` labels for `A/B`, `X/Y`, arrows,
`L/R`, `SELECT`, and `START`. On deck select, `L/R` page through the deck list;
in review, `B` is undo, `R` is suspend/restore, and `L` is Hard only after the
answer is visible. Settings and confirmation screens keep `B` as cancel.

After a rating, suspend, restore, or undo, the app saves compact deck state and
then appends review-log evidence when possible. If the diagnostic log append
fails after a state save, or if low battery makes that optional write
undesirable, the accepted action remains saved and the status reports
`Saved; log skipped`. Daily limits can block new-card reveals or review ratings
with status feedback instead of mutating the card.
When the battery sample first becomes low and not charging, the status line
announces `Battery low; charge soon` once for that low-battery episode. While
the sample remains low, repeated battery polls keep that warning intact, and
successful save-oriented actions preserve active warning context and add
warning-colored `; batt low` status context.
The root `session.tsv` diagnostic snapshot is written at boot, deck scan, deck
open, reveal, durable save actions, reset, and confirmed exit. Revealing an
answer also saves compact state so introduced-card and `new_limit` progress
survives exit and relaunch.
If a fresh pass exits and relaunches, the app loads the existing complete
snapshot, increments `launch_count`, and keeps earlier saved-action counters so
the final verifier can prove the whole pass instead of only the last launch.

## Confirmations

- Press `R` on the current card to open suspend confirmation.
- Press `R` on the completion screen, when suspended cards exist, to open
  restore confirmation.
- Press `R` on the completion screen with no suspended cards to report
  `Nothing suspended` while keeping any active warning context visible.
- Press `B` when no undo is available to report `Nothing to undo` while keeping
  any active warning context visible.
- Press `Y` before reveal or on the completion screen to open reset
  confirmation.
- Press `Y` from the deck selector to open exit confirmation.
- Press `START` from settings to open exit confirmation.
- Press `X` to confirm suspend, restore, or reset.
- Press `A` to confirm exit.
- Press `B` or `SELECT` to cancel.
- Press `START` from a suspend/restore/reset confirmation to open exit
  confirmation instead. On the exit confirmation itself, `START` is inert.

## Study Settings

In the study-settings screen:

- D-pad Up/Down chooses `new_limit`, `review_limit`, or learning mode.
- D-pad Left/Right cycles preset limit values or toggles learning mode.
- `X` saves.
- `B` or `SELECT` returns to review without saving.
- `START` opens exit confirmation. If edits are unsaved, the status warns
  `Exit loses unsaved settings` before the confirmation.

Preset values cycle through `5`, `10`, `20`, `50`, `100`, `200`, `500`,
`1000`, then `all`; `all` is saved as `0` and means all available cards.
If an existing settings file contains an off-ladder value, the next Left/Right
press snaps to the nearest preset in that direction.
Learning mode toggles between `Due first` and `Cooldown`; cooldown spaces
same-session repeats by other cards before they reappear.
The current clean shell shows `no changes` when the edited values match the
active saved limits and reports `unsaved changes` after a value change.
Canceling with unsaved changes discards the edit buffer and returns to review
with `Settings canceled; discarded`.
Saving updates `settings.tsv` and reports `Settings saved`, with the
low-battery save suffix when applicable.
