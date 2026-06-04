# Theme Assets

The local Fire Emblem graphics repo at `/Users/eric/codebase/FE-Repo` has the
right kinds of source material for a more advanced skin:

- `BGs, Interface Elements/Battle Frames & Backgrounds`
- `BGs, Interface Elements/Status Screen Backgrounds`
- `BGs, Interface Elements/Vanilla Fonts & Logos & Save Slots`

The repo README distinguishes F2U and F2E assets and says unlisted resources
should be treated conservatively as F2U unless the original creator confirms
broader permission. Several folders are also game rips or preservation material.
Because `anki3ds` is a public repo and not a Fire Emblem ROM hack, this project
does not vendor FE assets until a specific asset has clear redistribution and
reuse permission for this app.

Current implementation uses original console-rendered panels instead:

- Card paper stays a readable black-on-white panel.
- Help-page `X` cycles the panel trim through Amber, Forest, Ruby, and Chalk.
- The theme is session-local and does not change `settings.tsv`.

Importing real bitmap assets would require a renderer pass:

1. Pick explicitly permitted F2E or otherwise compatible assets.
2. Convert PNGs into 3DS textures with a build step.
3. Render background and border sprites with citro2d/citro3d.
4. Keep console text over the sprites, or replace console text with a bitmap
   font renderer.
5. Only after the renderer exists, consider a custom font. Font import is the
   largest part because wrapping, clipping, scrolling, and UTF-8 fallback all
   move out of the built-in console.

Practical scope order:

1. Original theme colors and console borders.
2. Sprite-backed borders/backgrounds.
3. Per-theme persisted settings.
4. Bitmap font renderer.
