# Tracked Sample Decks

These decks are tiny, original fixtures for local, emulator, and hardware
acceptance passes. The default daily-use sample set is text-only. All fixtures
are safe to commit and should not contain personal Anki data or copyrighted
material.

## Deck Coverage

- `sample`: text-card SD path, parser smoke, and long-text scroll coverage.
- `limits-demo`: multi-deck selection and daily-limit workflow coverage.

## Verification

Run:

```sh
make verify-sample-decks
```

This checks required files, `deck.json` metadata, `settings.tsv`, five-field
text-card rows, duplicate card IDs, accidentally committed progress files, and
unexpected media or extra files for the default text-only sample set.
GitHub Actions runs the same check through `make verify-ci`.

The same text-deck rules are applied to the copy-ready SD payload by:

```sh
make verify-package-sd
```

That target also rejects stray files or media folders in the packaged text
decks.

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
make verify-azahar-fresh-samples
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
`cards.tsv`, and `settings.tsv`, remove stale `settings.tsv.tmp` and
`settings.tsv.bak`, and preserve `state.tsv` and `review-log.tsv` unless a
fresh target is used.
They also remove old installed `media-demo` folders so the default pass remains
text-only.
The singular install targets remain compatibility aliases, but the plural
targets describe the current multi-deck workflow more accurately.

Tracked text sample directories should contain only `deck.json`, `cards.tsv`,
and `settings.tsv`.
