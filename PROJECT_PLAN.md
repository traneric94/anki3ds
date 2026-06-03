# Project Plan

## Goal

Build a practical Nintendo 3DS flashcard reviewer for Anki-derived decks.

The 3DS app should stay small and predictable. Complex Anki parsing should
happen on the desktop before files are copied to the SD card.

## Architecture

### 3DS App

Current stack:

- language: C
- SDK: devkitPro
- libraries: libctru console rendering
- package target: `.3dsx`
- later package target: optional `.cia`

Responsibilities:

- discover decks on the SD card
- load a simple deck format
- show front and back text
- collect ratings: Again, Hard, Good, Easy
- update scheduling state
- save after every review
- tolerate app exits and power loss as much as practical

### Desktop Converter

Current stack:

- language: Python
- input: Anki-style plain-text export
- later input: `.apkg` or AnkiConnect, if worthwhile

Responsibilities:

- convert Anki exports into a stable 3DS deck folder
- normalize text and line breaks
- strip or simplify HTML
- generate stable card IDs
- preserve progress when importing updated card content
- reject image, audio, media, and arbitrary template output

## Milestones

### M0: Repo And Planning

Deliverables:

- project plan
- checkpoint list
- test procedure
- draft deck format
- hardware test log template

Exit criteria:

- public GitHub repo exists
- initial docs are committed and pushed

### M1: Toolchain Proof

Deliverables:

- minimal `.3dsx`
- app boots in the local emulator
- app boots on hardware at the checkpoint boundary
- app draws text on screen
- app exits cleanly

Exit criteria:

- app launches in the local emulator
- user confirms the app launches on the 3DS
- result is recorded in `docs/device-test-log.md`

### M2: Input And Rendering Proof

Deliverables:

- button handling
- basic text wrapping
- simple screen layout

Exit criteria:

- `A` changes visible state
- D-pad or face buttons are detected
- text remains readable on original 3DS screens

### M3: SD Deck Read

Deliverables:

- app reads a sample deck from the SD card
- app shows front and back of a card

Exit criteria:

- sample deck copied to SD card appears in the app
- one card can be reviewed without crashing

### M4: Review Loop

Deliverables:

- deck picker
- due-card queue
- reveal answer action
- rating actions
- next-card flow

Exit criteria:

- user can review at least ten sample cards in one session

### M5: Save State

Deliverables:

- per-card scheduling state
- save after each rating
- load saved state on startup
- simple backup or temp-file write strategy

Exit criteria:

- review progress survives closing and reopening the app

### M6: Converter MVP

Deliverables:

- command-line converter
- Anki plain-text export input
- output deck folder for SD card
- sample conversion fixtures and tests

Exit criteria:

- user exports a real Anki deck
- converter produces a deck the 3DS app can review

### M7: Daily-Use MVP

Deliverables:

- multiple decks
- due counts
- new/review card limits
- undo last review
- suspend card
- settings file

Exit criteria:

- user can complete one real study session without editing files manually

### M8: Polish And Packaging

Deliverables:

- better typography
- nicer deck stats
- optional `.cia` package

Exit criteria:

- text-card review feels readable and predictable on hardware

## Scheduling Strategy

Start with a simple SM-2-inspired scheduler:

- new cards enter learning
- Again repeats soon
- Hard grows interval slowly
- Good uses the normal interval path
- Easy graduates faster

Store enough data to replace or improve the scheduler later:

- card ID
- state
- due timestamp or day number
- interval
- ease
- lapses
- last reviewed time
- review count

Do not attempt FSRS parity in the first version.

## Key Risks

- Full Anki templates can be arbitrary HTML/CSS and sometimes JavaScript.
- Unicode text and fonts matter for language-learning decks.
- Original 3DS hardware has limited screen space and performance.
- SD cards can fail, so save files need conservative writes.
- Bidirectional sync with Anki is much harder than local review.

## Design Principles

- Keep the 3DS app boring and reliable.
- Move parsing and conversion complexity to the desktop.
- Make every hardware test small and repeatable.
- Preserve review state separately from imported card content.
- Keep the supported deck format text-only.
- Follow the C conventions in `docs/c-style-and-architecture.md`.
