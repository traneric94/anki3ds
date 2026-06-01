# Testing

## Hardware Feedback Loop

The project depends on real 3DS testing. Emulator testing can help, but hardware
is the source of truth.

For each checkpoint:

1. Build the artifact.
2. Copy it to the SD card.
3. Test only the documented acceptance path.
4. Record the result in `docs/device-test-log.md`.
5. Fix the smallest confirmed issue.
6. Repeat until the checkpoint passes.

## Initial Target Device

Known user device:

- model: Nintendo 3DS, `CTR-001`
- custom firmware: Luma3DS present
- GodMode9 available
- SD card recently repaired or replaced

## Manual Test Log Format

Use this format in `docs/device-test-log.md`:

```text
## YYYY-MM-DD - Checkpoint Name

Build:
SD path:
Steps:
Observed:
Expected:
Result: pass/fail
Notes:
```

## Desktop Tests

The converter should have automated tests because it handles user data.

Minimum converter tests:

- parses plain-text Anki export
- preserves stable card IDs
- handles tabs and line breaks
- strips or simplifies simple HTML
- writes expected deck folder layout
- preserves existing review state on re-import

## Save-State Tests

Save files should be tested with:

- normal write and reload
- interrupted write simulation
- missing state file
- corrupted state file
- deck update with existing state

## Release Checklist

Before any tagged checkpoint:

- docs match the current artifact
- sample deck is tiny and original
- no personal Anki data is committed
- no copyrighted media is committed
- build instructions are current
