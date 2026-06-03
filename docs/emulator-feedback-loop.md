# Emulator Feedback Loop

The goal is to avoid copying every intermediate build to the 3DS SD card.
Emulator testing catches basic mistakes quickly, then real hardware validates
checkpoint builds.

## Preferred Emulator

Use Azahar for the first local loop because it supports `.3dsx` homebrew files.

Useful local loop:

```text
edit code -> make -> make run-emulator-samples -> fix obvious problems
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

Install the tracked sample decks into the local mirror with:

```sh
make install-local-sample-decks
```

Azahar's macOS SD card data directory is:

```text
~/Library/Application Support/Azahar/sdmc/
```

Install the sample decks there with:

```sh
make install-azahar-sample-decks
```

Or install the sample decks and launch the current build in one command:

```sh
make run-emulator-samples
```

## M1 Local Test

For the toolchain proof, the emulator test is intentionally small:

1. Build `anki3ds.3dsx` with `make`.
2. Launch it in Azahar with `make run-emulator`.
3. Confirm the top screen shows the app name and version.
4. Press `Start`, confirm the exit screen appears, then press `A`.

Pass condition:

- the app launches, renders text, and exits cleanly.

## Installed Emulator

Current local install path:

```text
~/Applications/azahar-macos-arm64-2125.1.2/Azahar.app
```

The downloaded release was `azahar-macos-arm64-2125.1.2.zip` from the official
`azahar-emu/azahar` GitHub release. Its SHA-256 digest was verified as:

```text
5d3aedc3840cf3b536caea9e9b60811e6c3a07475e2e5a36d816ffcbd58eecb4
```

## Hardware Still Matters

The original 3DS has different constraints than a desktop emulator:

- smaller physical screen
- real button feel and timing
- slower hardware
- real SD-card behavior
- firmware and Homebrew Launcher differences

Every tagged checkpoint should still be tested on the `CTR-001` device.
