# 3DS Coding Fundamentals

This note is the practical baseline for `anki3ds`. It is based on the local
devkitPro 3DS examples under `/opt/devkitpro/examples/3ds`, the Citro2D app
path currently in `app-3ds`, and the FE renderer debugging session.

## App Loop

A normal 3DS homebrew app is a small loop:

1. Initialize platform services, graphics, renderer resources, and app state.
2. Enter `while (aptMainLoop())`.
3. Draw only when the app state says a redraw is needed.
4. Scan input with `hidScanInput()`, then read `hidKeysDown()` and
   `hidKeysHeld()`.
5. Translate physical keys into app controls once, then run pure workflow
   reducers.
6. Wait when idle. Use vblank waits for active held-repeat navigation, and a
   longer timed input wait when nothing is happening.
7. Flush app-owned saves after state mutations and shut down renderer/platform
   resources in reverse order.

For this app, `main.c` should stay thin: own the platform loop, call the
renderer, assemble input frames, ask the shell to handle a frame, and exit.

## Renderer Ownership

One renderer should own the pixels for a screen. Mixing Citro2D, console text,
and direct framebuffer writes for the same surface makes bugs look like palette
or asset failures when the real issue is competing draw paths.

For the current app:

- `app_renderer_c2d.c` owns Citro2D/Citro3D includes, render targets, texture
  loading, colors, coordinates, wrapping/windowing, and all draw calls.
- Non-renderer modules hand over strings and state flags only.
- Backend modules must not include renderer headers, display geometry, Citro2D
  types, or draw functions.
- If a visual asset and a renderer constant describe the same thing, they need
  one tested source of truth. The parchment bug was fixed by matching renderer
  panel constants to the generated asset rectangle instead of tuning from
  screenshots.

The usual Citro2D frame shape is:

1. `C3D_FrameBegin(...)`
2. `C2D_TargetClear(...)`
3. `C2D_SceneBegin(target)`
4. draw textures, rectangles, and text for that target
5. repeat clear/scene/draw for the other screen
6. `C3D_FrameEnd(...)`

## Screens And Assets

The top screen is 400x240. The bottom screen is 320x240. Treat them as separate
surfaces with separate safe text rectangles.

Texture assets should be prepared before runtime. In this repo the FE
background/parchment assets are generated as PNG previews, converted through
`tex3ds` into `.t3x`, embedded in the app, then loaded with
`C2D_SpriteSheetLoadFromMem()`.

When the emulator screenshot cannot be trusted, add runtime evidence:

- write a tiny diagnostic file such as `renderer.tsv`
- include asset sizes and key layout constants
- clear stale diagnostics during fresh emulator launches
- only then inspect visually

## Text

The renderer owns text layout. The backend owns text content.

Good contract:

- backend: front string, back string, status string, progress string
- shell/contracts: app mode, answer-visible flag, help-visible flag, scroll
  offset
- renderer: top/bottom placement, word wrapping, visible row windows, colors,
  shadows, and font choice

Wrapping should prefer spaces and only hard-wrap inside a word when the word is
longer than the available row. Card text should be kept raw across the
backend/display boundary so the renderer can reflow it for the active screen.

## Input

Physical 3DS keys should be translated once into app-level logical buttons.
Workflow code should not branch on `KEY_A`, `KEY_B`, or circle-pad constants.
When two physical controls share one logical direction, keep source-specific
fields until the shell can route them. `anki3ds` uses that split so D-pad
scrolls the answer while Circle Pad scrolls the front/question.

Held input needs two separate concepts:

- edge actions from `hidKeysDown()`
- repeat actions from `hidKeysHeld()` only for screens that opt in

Mixed button chords should be inert for destructive or rating actions. This is
why `study_controls` and `app_input_frame` sit between libctru input and the
workflow reducers. Ambiguous same-frame D-pad plus Circle Pad navigation should
also be inert, because those physical sources can collapse to the same logical
direction before workflow code sees them.

## Saves And Battery

Save after user-visible study mutations: reveal, rating, undo, suspend,
restore, settings save, reset, and day rollover. Use temp files and backups for
deck state. Treat missing state as normal and malformed state as recoverable.

Battery sampling should be low-frequency and non-rendering:

- app shell decides when a sample is due
- battery monitor talks to PTMU
- power policy decides low-battery save warnings and idle waits
- renderer only receives status text

Low-battery status should not erase more important save/error evidence unless
the warning context is deliberately preserved.

## Debugging Rule

Before changing coordinates, colors, or draw order, prove which code is
rendering. The fastest path is usually:

1. identify the active source file from the Makefile
2. add a runtime fingerprint or visible one-pixel/color marker
3. rebuild and clear stale emulator state
4. verify the fingerprint from SDMC
5. then change layout with constants tied to assets or tests
