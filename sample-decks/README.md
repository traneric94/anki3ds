# Tracked Sample Decks

These decks are tiny, original fixtures for local, emulator, and hardware
acceptance passes. They are safe to commit and should not contain personal Anki
data or copyrighted media.

## Deck Coverage

- `sample`: text-card SD path and parser smoke coverage.
- `limits-demo`: multi-deck selection and daily-limit workflow coverage.
- `media-demo`: bounded `.a3i` image loading and media status coverage.

## Verification

Run:

```sh
make verify-sample-decks
```

This checks required files, `deck.json` metadata, `settings.tsv`, duplicate
card IDs, valid `.a3i` media references, and accidentally committed progress
files.
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

The fresh targets reinstall the tracked sample decks, then clear only tracked
sample progress files:

```text
state.tsv
state.tsv.tmp
state.tsv.bak
review-log.tsv
review-log.tsv.tmp
review-log.tsv.bak
```

They do not clear progress for personal decks outside the tracked sample ids.
The install targets replace source-owned files such as `deck.json`, `cards.tsv`,
`settings.tsv`, and `media/`, while preserving `state.tsv` and
`review-log.tsv` unless a fresh target is used.
The singular install targets remain compatibility aliases, but the plural
targets describe the current multi-deck workflow more accurately.

Tracked sample directories should contain only source deck files such as
`deck.json`, `cards.tsv`, `settings.tsv`, and optional `media/` assets.
