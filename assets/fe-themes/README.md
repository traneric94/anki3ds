# FE Theme Conversions

Generated derivatives from selected FE-Repo backgrounds that are explicitly in
an `[F2E]` folder.

Run from the repo root:

```sh
make verify-fe-theme-assets
```

Tracked raw output lives in `raw/` as low-resolution `128x80` BGR888 data.
Desktop BMP/PNG previews are generated under `build/fe-theme-previews/`, and
3DS framebuffer-ready BGR888 files are generated under
`build/fe-theme-framebuffers/`. The screen-sized previews and framebuffer files
scale directly from the visible `240x160` art crop inside the original `256x160`
source images, not from the tracked `128x80` raw files. Both build directories
are intentionally not tracked.

The main 3DS app can read the framebuffer files from
`sdmc:/3ds/anki3ds/fe-themes/` and use them behind the console UI after
cycling from the default `Plain` theme to an FE theme. The screen-sized
generated files are composed as darkened background, a generated FE map-style
parchment dialogue panel, an inert card layer, then baked FE dialogue text on
top. The card frame layer does not include text; the legend is a separate layer
before the card and font layers. The main app also installs
`sdmc:/3ds/anki3ds/fe-themes/layers/*_layer_legend_*_bgr888_fb.bin` and uses
that layer for unrevealed FE review-front screens so live question text is not
drawn over the baked sample font layer.
The generator also writes
`sdmc:/3ds/anki3ds/fe-themes/fe_font_review_8x14_alpha.bin`, a printable ASCII
alpha atlas derived from the local Ghostty terminal font when available. On this
machine that is Hack Nerd Font Mono; the FE8 font map, Checkmate, and the
built-in pixel font remain fallbacks.
If the files are not installed, the app falls back to its original black
console background.
