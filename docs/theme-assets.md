# Theme Assets

The local Fire Emblem graphics repo at `/Users/eric/codebase/FE-Repo` has the
right kinds of source material for a more advanced skin. Its top-level README
describes the repo as public free-to-use, with an F2U/F2E distinction:

- F2U assets are treated as use-only unless the creator grants broader terms.
- F2E assets are safe for this app's crop, resize, and darkening import path.
- Unlisted assets are treated conservatively as F2U.

`anki3ds` currently has a conversion-only FE asset checkpoint. The 3DS app does
not render these backgrounds yet.

## Current Sources

The converter uses only generated derivatives from an explicitly `[F2E]`
folder:

| Theme | Source | Credit |
| --- | --- | --- |
| Amber | `BGs, Interface Elements/Background CGs/WAve's BGs {WAve} [F2E]/Ballroom.png` | WAve |
| Forest | `BGs, Interface Elements/Background CGs/WAve's BGs {WAve} [F2E]/Grassland.png` | WAve |
| Ruby | `BGs, Interface Elements/Background CGs/WAve's BGs {WAve} [F2E]/Red Castle.png` | WAve |
| Chalk | `BGs, Interface Elements/Background CGs/WAve's BGs {WAve} [F2E]/Blue Castle.png` | WAve |

## Conversion

Regenerate the raw files and desktop previews with:

```sh
make verify-fe-theme-assets
```

Set `FE_REPO_PATH` or pass `--fe-repo` if the FE repo clone is somewhere other
than `~/codebase/FE-Repo`.

Outputs:

- `assets/fe-themes/raw/fe_bg_*_128x80_bgr888.bin`: tracked raw BGR888 files.
- `build/fe-theme-previews/*_raw_128x80.bmp`: exact small converted images.
- `build/fe-theme-previews/*_top_400x240.bmp`: desktop preview for top-screen
  scale.
- `build/fe-theme-previews/*_bottom_320x240.bmp`: desktop preview for
  bottom-screen scale.
- `build/fe-theme-framebuffers/*_top_400x240_bgr888_fb.bin`: top-screen
  BGR888 bytes laid out for direct copy into libctru's default sideways
  framebuffer.
- `build/fe-theme-framebuffers/*_bottom_320x240_bgr888_fb.bin`: bottom-screen
  BGR888 bytes laid out for direct copy into libctru's default sideways
  framebuffer.
- `build/fe-theme-previews/*_fb_240x*.bmp`: sideways framebuffer bytes shown as
  a desktop bitmap, useful only for layout debugging.
- `build/fe-theme-previews/*_fb_roundtrip_*.bmp`: framebuffer bytes converted
  back to natural screen order. These should look like the normal top/bottom
  previews.
- `build/fe-theme-previews/themes_top_sheet.bmp` and
  `themes_bottom_sheet.bmp`: contact sheets for quick visual inspection.
- Matching `.png` previews are generated next to the BMPs for tools that do not
  open BMP files.

Do not put the raw files under `app-3ds/data/` until the renderer is ready:
the app Makefile links every file in that directory into the `.3dsx`.

The default 3DS framebuffer is sideways BGR888. If a normal `400x240` desktop
image is copied directly into the framebuffer, it can display as scrambled or
rotated output. Use the `*_bgr888_fb.bin` artifacts for an isolated direct-copy
renderer, and inspect the matching `*_fb_roundtrip_*.png` previews to confirm
the conversion displays upright before integrating with app screens.

## Future Renderer

Current implementation still uses original console-rendered panels:

- Card paper stays a readable black-on-white panel.
- Help-page `X` cycles the panel trim through Amber, Forest, Ruby, and Chalk.
- The theme is session-local and does not change `settings.tsv`.

Moving beyond converted backdrops requires a renderer pass:

1. Prove the conversion output with desktop BMP/PNG previews,
   framebuffer round-trip previews, and
   `python3 -m unittest tests/test_fe_theme_assets.py`.
2. Choose either direct framebuffer drawing or citro2d/citro3d sprites.
3. Render the backgrounds in the isolated `tools/fe-bg-viewer-3ds` homebrew
   before touching app screens:

   ```sh
   make install-azahar-fe-bg-viewer
   make run-emulator-fe-bg-viewer
   ```

   Controls are `A`/Right/`R` for next theme, `B`/Left/`L` for previous theme,
   `X` to reload from SD, and `Start` to exit.
4. Keep console text over the sprites, or replace console text with a bitmap
   font renderer.
5. Only after the renderer exists, consider a custom font. Font import is the
   largest part because wrapping, clipping, scrolling, and UTF-8 fallback all
   move out of the built-in console.

Practical scope order:

1. Original theme colors and console borders.
2. Conversion and desktop preview pipeline.
3. Isolated 3DS background renderer test.
4. App integration.
5. Sprite-backed borders.
6. Per-theme persisted settings.
7. Bitmap font renderer.
