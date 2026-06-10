# Display Backend Split

Checkpoint note, 2026-06-05: this split plan is superseded by the clean-slate
Citro2D app shell described in `docs/session-handoff.md`. The active app
binary now compiles `main.c`, small `app_*` display/policy contracts such as
`app_flashcard_contract.c`, `app_deck_select_contract.c`, and
`app_settings_contract.c`, `app_confirm_contract.c`, `app_screen_model.c`, the
active `app_renderer_c2d.c` Citro2D renderer, and clean `study_*` backend
modules; it embeds FE background/parchment textures from `app-3ds/gfx/*.t3s`
and draws both images and backend strings through Citro2D.
The stale raw-framebuffer
`app_fe_renderer` path was removed from the active app tree.
Treat the proposal below as historical context unless it is explicitly revived.

This note proposes a small boundary between the study app and rendering. The
goal is to let Plain and FE display work move quickly without changing
scheduler, persistence, input safety, or deck behavior.

## Current Coupling

`app-3ds/source/main.c` currently owns both sides of the app:

- State transitions, SD-card side effects, diagnostics, scheduler actions, and
  battery/day polling.
- All top/bottom draw functions, theme selection, framebuffer background
  loading, console colors, text wrapping, and FE-specific layout choices.

The render path is centered on `draw_app(const struct app_state *app)`. It
selects the top console, draws one top-screen function by `app->mode`, selects
the bottom console, draws either review controls or generic controls, then
applies FE framebuffer backgrounds as a post-process.

That post-process is the main FE risk. It copies baked BGR888 background pixels
only where the console framebuffer is black, with special exceptions for Plain
paper cards. This makes FE color and composition depend on whatever the console
renderer happened to draw first. The FE review front path also branches inside
normal review drawing through helpers like `app_uses_fe_review_front_layout`,
`review_front_text_row`, and `review_front_text_width`.

The clean split should therefore forbid renderers from reading `app_state`
directly. `main.c` can keep owning app mutation, but display code should receive
a prebuilt, immutable screen model.

## Proposed Modules

Add the boundary in small C files, not a framework.

- `app_view_model.h` / `app_view_model.c`
  - Builds immutable screen structs from `app_state`.
  - This is the only new display-facing code allowed to inspect `app_state`,
    `deck_summary`, `scheduler_session`, `deck`, load reports, settings, and
    battery fields.
  - It also owns pure layout metrics used by both input scroll clamping and
    renderers.

- `display.h` / `display.c`
  - Public entry point: `display_render(const struct app_view_model *view)`.
  - Owns `consoleSelect`, screen clearing, final background application, and
    eventually `present_current_frame`.
  - Dispatches to Plain or FE renderer by `view->theme`.

- `display_plain.c`
  - Console renderer for the current Plain UI.
  - Initially it can be a mechanical move of existing `draw_*` helpers after
    they consume view structs instead of `app_state`.

- `display_fe.c`
  - FE renderer for opt-in theme screens.
  - It should draw from the same view structs as Plain, but use FE-specific
    background, scroll/card/legend composition, and text positions.
  - It should not call scheduler, deck, app settings, or app controls.

- `display_assets.h` / `display_assets.c`
  - Loads fixed-size framebuffer assets and exposes explicit asset status.
  - Inputs are theme ids and file paths, not `app_state`.

- `display_text.h` / `display_text.c`
  - Console primitives that are not app-specific: cursor move, truncated text,
    wrapped text, repeated chars, button-chip printing.
  - Can reuse the existing `app_text` module for UTF-8 column math.

The 3DS app Makefile and host test targets use explicit source lists, so both
must be updated deliberately when a new module needs app or host coverage.

## Current Checkpoint

The current concrete slice is:

- `app-3ds/include/app_flashcard_contract.h`
- `app-3ds/source/app_flashcard_contract.c`
- `app-3ds/include/app_renderer_c2d.h`
- `app-3ds/source/app_renderer_c2d.c`
- `app-3ds/include/app_screen_model.h`
- `app-3ds/source/app_screen_model.c`
- `app-3ds/include/app_deck_select_contract.h`
- `app-3ds/source/app_deck_select_contract.c`
- `app-3ds/include/app_deck_select_action.h`
- `app-3ds/source/app_deck_select_action.c`
- `app-3ds/include/app_settings_contract.h`
- `app-3ds/source/app_settings_contract.c`
- `app-3ds/include/app_settings_action.h`
- `app-3ds/source/app_settings_action.c`
- `app-3ds/include/app_review_action.h`
- `app-3ds/source/app_review_action.c`
- `app-3ds/include/app_battery_status.h`
- `app-3ds/source/app_battery_status.c`
- `app-3ds/include/app_confirm_contract.h`
- `app-3ds/source/app_confirm_contract.c`
- `app-3ds/include/app_confirm_action.h`
- `app-3ds/source/app_confirm_action.c`
- `app-3ds/include/study_backend.h`
- `app-3ds/source/study_backend.c`
- `app-3ds/include/study_controls.h`
- `app-3ds/source/study_controls.c`
- `app-3ds/include/study_3ds_key_map.h`
- `app-3ds/source/study_3ds_key_map.c`
- `tests/test_app_flashcard_contract.c`
- `tests/test_app_screen_model.c`
- `tests/test_app_deck_select_contract.c`
- `tests/test_app_deck_select_action.c`
- `tests/test_app_settings_contract.c`
- `tests/test_app_settings_action.c`
- `tests/test_app_review_action.c`
- `tests/test_app_battery_status.c`
- `tests/test_app_confirm_contract.c`
- `tests/test_app_confirm_action.c`
- `tests/test_study_backend.c`
- `tests/test_study_controls.c`
- `tests/test_study_3ds_key_map.c`
- `tests/test_fe_theme_assets.py`

`study_backend` owns cards, review state, rating transitions, undo,
suspend/restore, reset, compact state persistence, and backend view strings.
`study_controls` owns platform-neutral action classification, while
`study_3ds_key_map` is the thin libctru adapter from physical 3DS key bits to
those logical buttons.

`app_flashcard_contract` is the active display string contract for review
cards. `app_flashcard_text_view_build()` accepts `study_backend_view` plus
settings, then outputs owned title, front, back, display, progress, status,
help, and footer strings. It exposes no screen dimensions, coordinates, colors,
or pre-wrapped visible text. `app_review_action` owns pure review action side
effects for the app shell, including scroll clamping, scroll reset on
reveal/rating/undo, log metadata, dirty flags, and exit signaling. `main.c`
owns the review text windowing and scroll hint presentation through the local
Citro2D renderer helpers.
`app_screen_model` is the active aggregate display handoff. It accepts app
mode-adjacent state from `main.c`, builds the review, deck selector, settings,
and confirmation string contracts, and carries only those contracts plus the
current review scroll offset to the Citro2D renderer. It owns no coordinates,
colors, textures, fonts, or draw calls.
`app_renderer_c2d` is the active Citro2D renderer boundary. It accepts only
`app_screen_model`, owns C3D/C2D lifecycle, render targets, text buffers,
embedded FE texture loading, coordinates, colors, wrapping/windowing, and all
draw calls. Its public header exposes only an opaque stack-allocatable handle
and functions, so `main.c` no longer imports Citro2D headers or renderer
geometry constants. It does not inspect deck/session/backend storage directly.
`app_deck_select_contract` is the active display string contract for the deck
selector. It owns title, list, meta, status, controls, and footer strings while
navigation and deck opening stay in `main.c`.
`app_deck_select_action` is the active app-shell input reducer for the deck
selector. It owns chord rejection, command classification, and pure selection
movement while SD rescans, deck opening, and session updates stay in `main.c`.
`app_settings_contract` is the active display string contract for the daily
limits editor. It owns title, body, footer, status, controls, and help strings
while setting mutation and saves stay in `main.c`.
`app_settings_action` is the active app-shell input reducer for the daily
limits editor. It owns chord rejection, selected-field movement, preset value
cycling, save/cancel/exit requests, and edit-status classification while file
saves and mode transitions stay in `main.c`.
`app_confirm_contract` is the active display string contract for
suspend/restore/reset/exit confirmation screens. It owns status, prompt, and
footer strings while confirmation actions and mode transitions stay in
`main.c`.
`app_confirm_action` is the active app-shell input reducer for confirmation
screens. It owns chord rejection, cancel/confirm/open-exit classification, and
cancel status labels while destructive actions, session updates, and mode
transitions stay in `main.c`.
`app_battery_status` is the active app-shell battery status transition policy.
It owns the one-shot low-battery warning latch behavior at the status-string
level while PTMU sampling stays in `main.c` and polling cadence stays in
`app_power`.
`main.c` still owns app-mode orchestration, input, persistence,
battery/day polling, and screen-model construction. It no longer calls
Citro2D/Citro3D directly. The next practical split would be a larger app-core
layer rather than another single-screen string extraction.

2026-06-06 follow-up: `main.c` now renders through `app_screen_model_build()`
instead of constructing each display contract inside `app_draw()`. The
`tools/verify_app_theme.py` clean-shell policy follows this module boundary and
has a regression fixture for the screen-model handoff.

2026-06-06 renderer follow-up: the Citro2D draw path moved out of `main.c` and
into `app_renderer_c2d.{h,c}`. The verifier accepts this module as the only
Citro2D/Citro3D owner and rejects backend/contract code that reintroduces draw
APIs or geometry into the string/screen-model boundary. A follow-up hardening
pass made the renderer header opaque, moved review-body scroll clamping behind
`app_renderer_c2d_review_body_max_scroll_offset()`, and added verifier
regressions for direct Citro2D in `main.c`, renderer public-header Citro2D
leaks, renderer/backend dependencies, and missing display/status text draws.

2026-06-06 review-layout follow-up: the review contract still passes strings
and state flags only, but the renderer now draws the card front on the top
screen and the revealed card back on the bottom screen. The bottom review body
is no longer permanently occupied by controls; help text is rendered only when
the explicit help legend is visible, and reveal closes that legend. The scroll
metric API now receives whether the active scroll surface is the answer so
layout measurement matches the screen that will render the text.

2026-06-06 contract-verifier audit: `tools/verify_app_theme.py` now also scans
all public non-renderer headers for Citro2D/Citro3D type leaks, display
geometry constants, all non-renderer app/study modules for direct draw APIs,
all `study_*` modules for app display contracts or geometry constants, and the
renderer module for backend includes, function calls, or forward declarations.
The deck-selector row-count leak found during that audit was moved out of
`app_deck_select_contract.h`; the contract and action reducer now keep their
window/page sizing private to their `.c` files.

2026-06-06 parallel agent audit: backend/card modules still do not call
Citro2D, Citro3D, or draw APIs. Active rendering is concentrated in
`app_renderer_c2d`, while `main.c` remains the app shell that selects modes,
mutates state, and builds the aggregate screen model.

## View Model Shape

Use fixed buffers and borrowed pointers. Do not allocate.

```c
enum app_screen_view
{
	APP_SCREEN_DECK_SELECT,
	APP_SCREEN_LOAD_ERROR,
	APP_SCREEN_REVIEW,
	APP_SCREEN_SUMMARY,
	APP_SCREEN_ACTIONS,
	APP_SCREEN_SETTINGS,
	APP_SCREEN_CONTROLS,
	APP_SCREEN_CONFIRM_RESTORE,
	APP_SCREEN_CONFIRM_SUSPEND,
	APP_SCREEN_CONFIRM_RESET,
	APP_SCREEN_CONFIRM_EXIT,
};

struct app_status_view
{
	const char *message;
	enum app_status_color color;
};

struct app_battery_view
{
	enum app_power_battery_display_state state;
	u8 level;
};

struct app_review_view
{
	bool has_card;
	bool revealed;
	const char *front;
	const char *back;
	size_t scroll_offset;
	size_t max_scroll_offset;
	size_t card_index;
	size_t card_count;
	size_t new_due_count;
	size_t learning_due_count;
	size_t review_due_count;
	unsigned int new_count_today;
	unsigned int review_count_today;
	char new_limit[16];
	char review_limit[16];
};

struct app_view_model
{
	enum app_screen_view screen;
	enum app_theme theme;
	struct app_status_view status;
	struct app_battery_view battery;
	union
	{
		struct app_deck_select_view deck_select;
		struct app_review_view review;
		struct app_summary_view summary;
		struct app_actions_view actions;
		struct app_settings_view settings;
		struct app_controls_view controls;
		struct app_error_view error;
		struct app_confirm_view confirm;
	} body;
};
```

The exact field list should be screen-driven. Avoid copying whole backend
objects into the view model. A view should expose already formatted or simple
primitive values such as `new_due_count`, `deck_name`, `state_message`, and
`settings_warning`, not a full `struct scheduler_session`.

## Data Flow

Keep mutation and rendering one-way:

```text
3DS input
  -> app_controls classifies action
  -> main/app logic mutates app_state and durable files
  -> app_view_model_build(app_state, app_view_model)
  -> display_render(app_view_model)
  -> display_plain or display_fe writes framebuffers
```

The renderer never mutates app state and never asks scheduler questions. If the
screen needs a count, formatted setting, warning reason, or active card text,
the builder computes it first.

Scroll is the only cross-boundary detail that affects behavior. Today,
`review_max_scroll_offset()` depends on theme layout, because FE front text uses
different rows and columns from Plain. Move that into a pure layout helper:

```c
void app_view_model_review_text_metrics(
	enum app_theme theme,
	bool revealed,
	size_t *max_columns,
	size_t *visible_rows
);
```

Input code can use this helper to clamp `review_scroll_offset`; renderers can
use the same metrics to place text. That keeps FE display choices explicit
without letting the renderer control study behavior.

## Theme Boundary

Plain and FE should be sibling renderers, not layered hacks.

Plain:

- Draws console background, ASCII panels, prompts, and status exactly like the
  current daily-use UI.
- Startup remains Plain.
- It should keep the current conservative colors.

FE:

- Starts by drawing the complete FE background asset for each screen.
- Draws a scroll/card frame as its own asset or framebuffer layer.
- Draws question/answer text in one planned text box.
- Draws the legend/buttons as a planned strip, not as incidental black console
  holes later filled by the background pass.

This means FE can start as a single-screen implementation. If a screen is not
yet implemented by `display_fe.c`, `display_render` should deliberately fall
back to `display_plain.c` for that screen while preserving the selected theme
status. Do not half-apply FE backgrounds to screens that still use Plain layout.

## Migration Order

1. Add `app_view_model` structs and builder for `DECK_SELECT` only.
   `draw_deck_select_screen` and the deck-select bottom details should consume
   `struct app_deck_select_view`. Done for the current console renderer.

2. Move generic console primitives out of `main.c`.
   Start with `console_move`, `draw_wrapped_text_columns`, `print_truncated`,
   `draw_button_chip`, and prompt helpers. Keep names boring and module-prefixed.

3. Convert bottom deck-selector details to the same deck-select view model.
   This removes renderer access to `deck_summaries` and exposes exactly the
   selected deck's display facts. Done for deck-select bottom controls/details.

4. Convert `REVIEW` front before answer.
   This is the best FE checkpoint: background + scroll/card frame + question
   text + bottom legend. Keep answer/rating and all saving logic unchanged.
   Done for the current unrevealed review-front view model and FE framebuffer
   overlay renderer.

5. Convert `REVIEW` after reveal.
   Add back-text view fields and rating-button view data. The renderer should
   still receive only `revealed`, text, counts, buttons, scroll state, and
   status.

6. Convert summary/load-error/actions/settings/confirm/help screens one at a
   time.
   These are mostly text screens and should be Plain-first moves. FE can fall
   back until each screen has a real design.

7. Move theme background loading into `display_assets`.
   At this point FE can stop relying on black-pixel replacement and draw its
   own framebuffer composition directly.

8. Shrink `main.c`.
   After all draw paths consume view models, `main.c` should retain app state,
   input dispatch, persistence, polling, and calls to
   `app_view_model_build()` / `display_render()`.

## Test And Verification Hooks

Add tests as each boundary lands:

- Host tests for view-model builders with small fake app states.
- A verifier that `display_plain.c` and `display_fe.c` do not reference
  `struct app_state`, `scheduler_`, `deck_summary_`, `review_state_`, or
  `app_settings_load_with_report`.
- A verifier that `main.c` is the only renderer caller that passes `app_state`
  into `app_view_model_build`.
- Existing `verify_app_theme.py` should remain responsible for active
  app theme/rendering invariants: clean FE shell startup, renderer/backend
  boundary, renderer-owned Citro2D calls, and live backend/display text draws.
- Emulator smoke tests should capture the active FE shell and one review flow
  from fresh samples.

## Risks

- Extracting every screen at once would stall study workflow work. Migrate one
  screen at a time and keep mixed old/new render paths temporarily.
- Copying backend structs into the view model would only move the coupling.
  Keep view structs screen-specific and display-shaped.
- Putting scroll math only in FE rendering would break input behavior. Keep
  text metrics pure and shared.
- Continuing black-pixel framebuffer replacement for FE will keep causing color
  and layering regressions. FE should become an explicit renderer that draws
  background, frame, legend, then text.
- Building a generic retained UI tree would be overkill for this app. Use plain
  structs and switch statements.
- View-model string lifetimes must be clear. Borrow stable text from `app_state`
  for the current frame, and use fixed buffers inside the view model for
  formatted values.
