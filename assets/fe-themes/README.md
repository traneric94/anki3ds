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
scale directly from the original `256x160` source images, not from the tracked
`128x80` raw files. Both build directories are intentionally not tracked.

These files are not rendered by the 3DS app yet.
