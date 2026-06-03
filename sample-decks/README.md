# Tracked Sample Decks

These decks are tiny, original fixtures for local, emulator, and hardware
acceptance passes. The default daily-use sample set is text-only. All fixtures
are safe to commit and should not contain personal Anki data or copyrighted
media.

## Deck Coverage

- `sample`: text-card SD path and parser smoke coverage.
- `limits-demo`: multi-deck selection and daily-limit workflow coverage.
- `media-demo`: optional non-MVP `.a3i` fixture, not installed by default.

## Verification

Run:

```sh
make verify-sample-decks
```

This checks required files, `deck.json` metadata, `settings.tsv`, duplicate
card IDs, and accidentally committed progress files for the default text-only
sample set.
GitHub Actions runs the same check through `make verify-ci`.

## Install And Fresh Passes

Local SD mirror:

```sh
make install-local-sample-decks
make prepare-local-samples-fresh
```

Azahar SD data directory:

```sh
make install-azahar-sample-decks
make prepare-azahar-samples-fresh
make run-emulator-fresh-samples
```

The fresh targets reinstall the default text-only tracked sample decks, then
clear only tracked sample progress files:

```text
state.tsv
state.tsv.tmp
state.tsv.bak
review-log.tsv
review-log.tsv.tmp
review-log.tsv.bak
```

They do not clear progress for personal decks outside the tracked sample ids.
The install targets replace source-owned files such as `deck.json`,
`cards.tsv`, and `settings.tsv`, while preserving `state.tsv` and
`review-log.tsv` unless a fresh target is used.
They also remove the old optional `media-demo` fixture from the installed
sample root so the default pass remains text-only.
The singular install targets remain compatibility aliases, but the plural
targets describe the current multi-deck workflow more accurately.

Tracked sample directories should contain only source deck files such as
`deck.json`, `cards.tsv`, and `settings.tsv`. Optional media fixtures are kept
outside the daily-use sample set.
