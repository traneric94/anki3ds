# Theme Assets

The local Fire Emblem graphics repo at `/Users/eric/codebase/FE-Repo` has the
right kinds of source material for a more advanced skin. Its top-level README
describes the repo as public free-to-use, with an F2U/F2E distinction:

- F2U assets are treated as use-only unless the creator grants broader terms.
- F2E assets are safe for this app's crop, resize, and darkening import path.
- Unlisted assets are treated conservatively as F2U.

`anki3ds` currently has a conversion, isolated-renderer, and main-app FE UI
checkpoint. The active main app path is now centralized Citro2D: the Forest
background/parchment textures live under `app-3ds/gfx/` as PNG plus `.t3s`
inputs, the app Makefile turns them into generated `*_t3x.h` data, and
`main.c` loads those embedded sheets with `C2D_SpriteSheetLoadFromMem()`.
The previous SD-card BGR888 framebuffer path remains useful for conversion
previews and the isolated viewer, but it is not the active app renderer.

The isolated FE background viewer has been manually verified in Azahar: the
generated top and bottom framebuffer assets render upright on both screens.

## Current Sources

The converter uses only generated derivatives from an explicitly `[F2E]`
folder:

| Theme | Source | Credit |
| --- | --- | --- |
| Amber | `BGs, Interface Elements/Background CGs/WAve's BGs {WAve} [F2E]/Ballroom.png` | WAve |
| Forest | `BGs, Interface Elements/Background CGs/WAve's BGs {WAve} [F2E]/Grassland.png` | WAve |
| Ruby | `BGs, Interface Elements/Background CGs/WAve's BGs {WAve} [F2E]/Red Castle.png` | WAve |
| Chalk | `BGs, Interface Elements/Background CGs/WAve's BGs {WAve} [F2E]/Blue Castle.png` | WAve |

The generated screen-sized app backgrounds also use these UI sources:

| Purpose | Source | Credit |
| --- | --- | --- |
| Flashcard panel palette | `BGs, Interface Elements/Battle Frames & Backgrounds/Sokaballa's Battle Screen/Full Battle Screen.png` | Sokaballa |
| Bottom-screen legend icon strip | `BGs, Interface Elements/Text Characters/{JeyTheCount} Sword Icon [F2E].png`, `Staff Icon`, `Anima Icon` | JeyTheCount |
| Active dialogue panel parchment | `assets/fe-themes/external/opengameart/old-parchment-paper/parchment_alpha.png` | OpenGameArt old parchment paper, CC0 |
| Fallback parchment panel | `assets/fe-themes/external/opengameart/parchment-gui/panels.png` | OpenGameArt parchment GUI, CC0 |
| Primary dialogue font | `/Users/eric/Library/Fonts/HackNerdFontMono-Regular.ttf` | Hack Nerd Font Mono |
| FE8 dialogue font fallback | `assets/fe-themes/font/fe8_vanilla_fontMap.png` | 7743 / FEUniverse |
| FE7-FE8 fallback/menu font | `BGs, Interface Elements/Vanilla Fonts & Logos & Save Slots/FE7-FE8/Checkmate by Bob Quinzel/Checkmate.otf` | Bob Quinzel |
| Map-warning parchment reference fallback | `assets/fe-themes/reference/fe_dialogue_warning_275x183.png` | downloaded screenshot reference |

The FE8 dialogue font map came from
`https://feuniverse.us/t/fe8u-editing-menu-dialogue-font-glyphs-draft/6716`.
That post describes FE8 text/menu glyphs as 16x16, 4-color glyph images and
provides the vanilla font-map PNG now vendored under `assets/fe-themes/font/`.
It remains useful as a fallback/reference, but the current UI text pass uses
the user's terminal font first because the FE8 sheet looked too bold at this
checkpoint.

The screenshot-style FE map dialogue/parchment box has not been found in the
local `FE-Repo` checkout by obvious filename. Useful local directories checked:

- `/Users/eric/codebase/FE-Repo/BGs, Interface Elements/Popups & Arrow Graphics`
  contains phase banners and movement arrows, not dialogue boxes.
- `/Users/eric/codebase/FE-Repo/BGs, Interface Elements/Battle Frames & Backgrounds/Sokaballa's Battle Screen`
  contains the current red/blue battle-frame source and component boxes.
- `/Users/eric/codebase/FE-Repo/BGs, Interface Elements/Battle Frames & Backgrounds/{Cynon} Chrono Trigger Inspired Battle Frames {F2E}`
  contains wood/trim battle-frame variants that are closer in tone but still
  not the vanilla map warning parchment.
- `/Users/eric/codebase/FE-Repo/BGs, Interface Elements/Battle Frames & Backgrounds/{Cynon} FF9 Inspired Battle Screen (F2E)`
  contains low-contrast battle-screen frames inspired by FF9 dialogue frames.
- `/Users/eric/codebase/FE-Repo/BGs, Interface Elements/Vanilla Fonts & Logos & Save Slots/FE7-FE8`
  contains the current Checkmate OTF font source.

## Conversion

Regenerate the raw files and desktop previews with:

```sh
make verify-fe-theme-assets
```

Set `FE_REPO_PATH` or pass `--fe-repo` if the FE repo clone is somewhere other
than `~/codebase/FE-Repo`.

Outputs:

- `assets/fe-themes/raw/fe_bg_*_128x80_bgr888.bin`: tracked low-resolution
  BGR888 files for lightweight experiments.
- `build/fe-theme-previews/*_raw_128x80.bmp`: exact small converted images.
- `build/fe-theme-previews/*_top_400x240.bmp`: desktop preview for top-screen
  scale, generated directly from the visible `240x160` FE art crop rather than
  from the `128x80` raw file, then composited through the background, legend,
  card, and font layers.
- `build/fe-theme-previews/*_bottom_320x240.bmp`: desktop preview for
  bottom-screen scale, generated directly from the visible `240x160` FE art crop
  rather than from the `128x80` raw file, then composited through the
  background, legend, card, and font layers.
- `build/fe-theme-previews/*_layer_background_*.bmp`: dimmed FE background
  before any UI decoration.
- `build/fe-theme-previews/*_layer_legend_*.bmp`: background plus the active
  FE-style parchment dialogue panel, with no baked Anki text. The active panel
  source is the CC0 old parchment PNG at
  `assets/fe-themes/external/opengameart/old-parchment-paper/parchment_alpha.png`.
  Its transparent corners preserve the FE background, and its opaque light
  center is tested for dark dialogue-text readability. If that file is absent,
  the importer falls back to the CC0 Parchment GUI 9-slice source, then the
  downloaded map-warning reference crop, then the generated panel.
- `build/fe-theme-previews/*_layer_card_*.bmp`: currently identical to the
  legend layer. The old red/orange full-card chrome is intentionally inert for
  this checkpoint so the isolated renderer stays focused on background,
  parchment, and FE text.
- `build/fe-theme-previews/*_layer_font_*.bmp`: final layer, adding the baked
  terminal-font sample text on top of the card layer. The current local primary
  is Hack Nerd Font Mono rendered through `hb-view`; if that font is missing,
  the generator falls back to the FE8 font map, then Checkmate through
  `hb-view`, and then to its pixel font.
- `build/fe-theme-framebuffers/*_top_400x240_bgr888_fb.bin`: top-screen
  BGR888 bytes laid out for direct copy into libctru's default sideways
  framebuffer. These are generated directly from the full source image.
- `build/fe-theme-framebuffers/*_bottom_320x240_bgr888_fb.bin`: bottom-screen
  BGR888 bytes laid out for direct copy into libctru's default sideways
  framebuffer. These are generated directly from the full source image.
- `build/fe-theme-framebuffers/layers/*_layer_*_bgr888_fb.bin`: background,
  legend, inert card, and font layer framebuffers. The isolated 3DS viewer
  currently cycles background, parchment, then text. The previous SD-card app
  integration packaged the legend layers so live question text could be drawn
  over a dialogue panel without the baked sample font layer. In the active
  Citro2D app path, these files are legacy/preview artifacts rather than the
  main app renderer input.
- `build/fe-theme-framebuffers/fe_font_review_8x14_alpha.bin`: printable ASCII
  8x14 alpha atlas for live FE review-front text. It is generated from the
  local Ghostty terminal font when available, then falls back to the FE8
  dialogue font map,
  then Checkmate through `hb-view`, and finally to the local pixel font. The
  active app now uses `C2D_DrawText()` instead of blending this atlas.
- `build/fe-theme-previews/*_fb_240x*.bmp`: sideways framebuffer bytes shown as
  a desktop bitmap, useful only for layout debugging.
- `build/fe-theme-previews/*_fb_roundtrip_*.bmp`: framebuffer bytes converted
  back to natural screen order. These should look like the normal top/bottom
  previews.
- `build/fe-theme-previews/themes_top_sheet.bmp` and
  `themes_bottom_sheet.bmp`: contact sheets for quick visual inspection.
- `build/fe-theme-previews/themes_top_layers_sheet.bmp` and
  `themes_bottom_layers_sheet.bmp`: layer contact sheets. Each row is a theme;
  columns are background, legend, card, then font.
- Matching `.png` previews are generated next to the BMPs for tools that do not
  open BMP files.

Do not put the raw files under `app-3ds/data/`: the app Makefile links every
file in that directory into the `.3dsx`.

The default 3DS framebuffer is sideways BGR888. If a normal `400x240` desktop
image is copied directly into the framebuffer, it can display as scrambled or
rotated output. Use the `*_bgr888_fb.bin` artifacts for an isolated direct-copy
renderer, and inspect the matching `*_fb_roundtrip_*.png` previews to confirm
the conversion displays upright before integrating with app screens.

The screen-sized outputs intentionally avoid the `128x80` intermediate. That
keeps more of the original source detail for the actual rendered background;
the low-resolution tracked raw files are not the quality target for the viewer.

## Current App Renderer

The current app renderer separates content/state from display mechanics in a
smaller clean-slate shell:

- `study_backend` supplies card strings, answer visibility, counters, status,
  ratings, and undo state.
- `study_controls` maps platform-neutral buttons to one action.
- `main.c` owns the 3DS input bridge and the active Citro2D draw path.
- `app_text` remains the small string/scroll wrapping helper.

The active app loads only the embedded Forest legend sheets:

```text
app-3ds/gfx/fe_forest_top_legend.png
app-3ds/gfx/fe_forest_top_legend.t3s
app-3ds/gfx/fe_forest_bottom_legend.png
app-3ds/gfx/fe_forest_bottom_legend.t3s
```

Regenerate and synchronize those embedded PNGs with:

```sh
make sync-app-fe-ui-assets
```

The top-level `app-3ds` target depends on that sync target. It runs the FE
theme importer, compares the generated Forest legend previews against the
embedded PNGs, copies only changed images, and marks the corresponding `.t3s`
dirty so `tex3ds` refreshes the linked textures. Direct `make -C app-3ds`
builds also track the PNG inputs as texture prerequisites.

`tex3ds` emits generated `*_t3x.h` headers and linked texture data. `main.c`
includes those headers, calls `C2D_SpriteSheetLoadFromMem()`, pulls image 0
from each sheet, and draws the top/bottom FE background-parchment images with
`C2D_DrawImageAt()`. If sheet loading fails, the app draws simple Citro2D
fallback rectangles.

Live Anki content is no longer console text or the generated alpha-atlas
framebuffer overlay. `main.c` renders backend strings with `C2D_DrawText()`
through a local helper, using a `C2D_TextBuf` and word wrapping for longer card
and status text. The stale raw-framebuffer `app_fe_renderer` path has been
removed from the active app tree.

The old SD framebuffer outputs still matter for previews, packaging tests, and
the isolated `tools/fe-bg-viewer-3ds` renderer. They should not be described as
the current main-app rendering path unless the app Makefile is changed back to
compile and load them.

The FE font files in `Vanilla Fonts & Logos & Save Slots/` are TTF/OTF sources,
not libctru console bitmap fonts. The asset generator's baked preview text path
uses the local terminal font first, then crops glyph masks from
`assets/fe-themes/font/fe8_vanilla_fontMap.png`, then falls back to Checkmate
through `hb-view` and finally to the local pixel font. The generated 8x14 alpha
atlas remains a legacy/fallback artifact, not the active app text stack.

Moving beyond the current embedded Forest Citro2D checkpoint requires a
renderer pass:

1. Prove the conversion output with desktop BMP/PNG previews,
   framebuffer round-trip previews, and
   `python3 -m unittest tests/test_fe_theme_assets.py`.
2. Add more embedded `.t3s` sheets or reintroduce an SD asset loader behind a
   clear display-assets boundary.
3. Render any new backgrounds in the isolated `tools/fe-bg-viewer-3ds` homebrew
   before touching app screens:

   ```sh
   make install-azahar-fe-bg-viewer
   make run-emulator-fe-bg-viewer
   ```

   Controls are `A`/Right/`R` for next theme, `B`/Left/`L` for previous theme,
   Down for next layer, Up for previous layer, `X` to reload from SD, and
   `Start` to exit.
4. Keep text sourced from backend strings and rendered by the active display
   layer, not by the backend.
5. Extend the generated atlas path only if Citro2D system-font text is not good
   enough on hardware.

Practical scope order:

1. Original theme colors and console borders.
2. Conversion and desktop preview pipeline.
3. Isolated 3DS background renderer test. Done for direct framebuffer copy.
4. App integration. Current checkpoint uses embedded Forest `.t3s` textures,
   `C2D_DrawImageAt()`, and live backend strings rendered with `C2D_DrawText()`.
5. Sprite-backed borders.
6. Per-theme persisted settings.
7. Bitmap font renderer. The 8x14 generated alpha atlas exists as a legacy
   artifact, but the active checkpoint renders text through Citro2D.
