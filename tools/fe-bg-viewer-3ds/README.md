# FE Background Viewer

Standalone 3DS homebrew for validating converted FE background assets before
integrating them into `app-3ds`.

Build only:

```sh
make fe-bg-viewer-3ds
```

Install into Azahar's SDMC and copy the generated framebuffer assets:

```sh
make install-azahar-fe-bg-viewer
```

Run in Azahar:

```sh
make run-emulator-fe-bg-viewer
```

Controls:

- `A`, `Right`, or `R`: next theme.
- `B`, `Left`, or `L`: previous theme.
- `X`: reload the current theme from SD.
- `Start`: exit.

The viewer reads `sdmc:/3ds/anki3ds/fe-themes/*_bgr888_fb.bin` and copies the
bytes directly into libctru's default BGR888 framebuffers. If a file is missing
or the size is wrong, the viewer displays a checker pattern instead of that
screen's background.
