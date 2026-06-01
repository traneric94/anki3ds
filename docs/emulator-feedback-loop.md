# Emulator Feedback Loop

The goal is to avoid copying every intermediate build to the 3DS SD card.
Emulator testing catches basic mistakes quickly, then real hardware validates
checkpoint builds.

## Preferred Emulator

Use Azahar for the first local loop because it supports `.3dsx` homebrew files.

Useful local loop:

```text
edit code -> build .3dsx -> run in Azahar -> fix obvious problems
```

Then, at checkpoint boundaries:

```text
copy .3dsx to SD card -> run on 3DS -> record hardware result
```

## Local SD Layout

Use a gitignored local folder to mirror the 3DS SD card:

```text
local/sdmc/
  3ds/
    anki3ds/
      anki3ds.3dsx
      decks/
```

`local/` is ignored by git so personal decks, emulator files, and temporary
build artifacts do not get committed.

## M1 Local Test

For the toolchain proof, the emulator test is intentionally small:

1. Build `anki3ds.3dsx`.
2. Launch it in Azahar.
3. Confirm the top screen shows the app name and version.
4. Press `Start` to exit.

Pass condition:

- the app launches, renders text, and exits cleanly.

## Hardware Still Matters

The original 3DS has different constraints than a desktop emulator:

- smaller physical screen
- real button feel and timing
- slower hardware
- real SD-card behavior
- firmware and Homebrew Launcher differences

Every tagged checkpoint should still be tested on the `CTR-001` device.
