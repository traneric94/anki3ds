# Project Review And Roadmap

Date: 2026-06-10. Full-codebase review (app C, tests, tools, build) plus a
runtime performance audit. This file records the verdict, the prioritized
roadmap to finish the project, and the performance findings for future
optimization work. Line references are as of commit `a8ef377`.

## Verdict

Do not rewrite. The architecture is sound and verified; the over-engineering
is peripheral and can be deleted in place. The project's real gap is not code
quality — it is that M7's manual acceptance pass has not been run, CI is red
from one GCC-only warning, and checkpoints were never tagged.

### Verified strengths

- Strict layering holds under grep: `study_*` modules have zero UI knowledge;
  `app_renderer_c2d.c` is the only file including `citro2d.h`; `main.c` is a
  real 120-line platform harness; `app_shell.c` (834 LOC) has no static state
  and plain if/else mode dispatch.
- C hygiene: 69 bounded `snprintf` calls, zero `sprintf`/`strcpy`/`strcat`;
  const-correct string params; consistent bool + result-enum error convention;
  malloc confined to two modules with paired frees; include guards everywhere;
  zero TODO/FIXME or commented-out code.
- The scheduler/state core has behavior tests that survive refactoring:
  `test_study_backend.c` (45 tests), `test_app_shell.c` (20 integration
  tests), `test_m7_direct_smoke_flow.c` (full session through real modules
  writing real artifacts).

### Real over-engineering (deletable in place)

| Problem | Evidence |
| --- | --- |
| Source-regex theme verifier | `tools/verify_app_theme.py` (901 LOC + 2,195 LOC meta-tests) regex-parses renderer source to assert pixel constants (`y==178.0`, `y==174.0`) and variable names; breaks on any rename |
| M7 evidence apparatus | ~1,400 LOC shipped app code (`study_session.c` 874, `study_review_log.c` 547) + 1,259-LOC verifier + 1,502 LOC meta-tests + `M7_*` flag ecosystem, largely to prove a manual test happened |
| Makefile copy-paste | 37 hand-copied compile+run blocks for host tests (~299 LOC) |
| Duplicated helpers | Identical 8-line `copy_string` static in 8 `app_*` files; `write_file`/`remove_test_tree` hand-copied across ~40 test files |
| God module | `study_backend.c` 2,307 LOC (scheduling + TSV parse/encode + queue + rollback) — 53% of app C |
| Brittle assertions | 204 exact UI-string `strcmp`s in C tests; a copy edit breaks ~50 tests |
| Micro-modules | `app_input_policy.c` (26 LOC), `app_session_save.c` (51 LOC) |

## Roadmap

### Phase 0: Environment Setup

- Install devkitPro pacman, then `dkp-pacman -S 3ds-dev` (toolchain at
  `/opt/devkitpro`, matching `app-3ds/Makefile`).
- Install an Azahar macOS arm64 release under `~/Applications`. For a version
  other than the `Makefile` default, override `AZAHAR_APP=` on invocations
  rather than editing the Makefile.
- Prove the toolchain: `make`, then `make verify-local`.

### Phase 1: Green Main

- Fix `tests/test_app_deck_select_contract.c` deck-id buffer
  (`char id[16]` → `char id[32]`): GCC `-Werror=format-truncation` fails CI on
  ubuntu; clang does not emit it, so local gates pass by construction.
- Confirm the `host-tests` Actions workflow is green.

### Phase 2: Finish M7 (the actual blocker)

1. `make verify-m7-preflight`.
2. Emulator pass per CHECKPOINTS.md M7 script:
   `make run-emulator-fresh-samples`, complete a real session (review,
   suspend, undo, restore, settings edit, reset one deck, relaunch,
   exit-confirm), then `make verify-m7-artifacts M7_EXPECT_SETTINGS=...`.
   Record evidence in `docs/emulator-test-log.md`.
3. Hardware pass on a real 3DS; verify with `M7_SDMC=/path/to/sd`; record in
   `docs/device-test-log.md`.
4. Tag `v0.7-daily-use-mvp` (hardware is the gate) and push the tag.

### Phase 3: Targeted Simplification (one commit each, ranked)

1. Makefile test-host template: per-test `_SRCS` variables + one
   `$(foreach)`-generated rule replace the 37 blocks (~299 → ~60 LOC). Prove
   with identical `make -n test-host` output before/after.
2. Shared `tests/test_util.h` for `write_file`/`remove_test_tree`/
   `reset_fake_ptmu`; sweep test files incrementally.
3. `app_text_copy()` in `app_text.c`; delete the 8 duplicate statics in
   `app_*` files. Keep the one copy in `study_deck_index.c` — `study_*` must
   not include `app_*` headers; one duplicated helper is the price of the
   layering.
4. Trim `tools/verify_app_theme.py` to ~100 LOC: keep the architectural guard
   (only renderer modules may touch `C2D_*`/`console*`); delete pixel-baseline
   and variable-name checks; shrink its meta-test file accordingly.
   Net ≈ −3,000 LOC of rename-brittle tooling.
5. M7-apparatus freeze (doc-only): `verify_m7_artifacts.py` and its meta-tests
   become bugfix-only after M7; no new `M7_*` flags. Do not gate review-log
   writes behind a build flag — `review-log.tsv` is product data (input for
   write-back and stats below) and a flag would fork the tested binary.
6. Micro-module absorption (opportunistic): `app_input_policy.c` →
   `app_input_frame.c`; `app_session_save.c` → `app_state_save.c`.

Explicitly skipped restructuring: do not collapse the four `*_contract.c`
modules (they are the renderer-facing seam that keeps the renderer
string-only; collapsing is churn in heavily-asserted tests for zero behavior
gain); do not split `study_backend.c` yet (defer until the scheduler feature
below gives the split a customer); do not complete the action/contract/flow
triad for symmetry.

### Phase 4: M8 Polish And Packaging

- Typography/UI final pass in `app_renderer_c2d.c` (safe once pixel asserts
  are gone).
- User-facing quick-start README section (current docs are developer-facing).
- Release artifact = zip of the `package-sd` payload, `verify-package-sd`
  green.
- Stay `.3dsx`-only; skip `.cia`: the usual pipeline needs bannertool
  (archived) and makerom (unmanaged release binaries), and anyone who can
  install a `.cia` already runs the Homebrew Launcher, which the existing
  `.3dsx` + `.smdh` serves. Revisit only if launcher friction hurts daily use.
- Tag `v0.8-polish`.

### Phase 5: Missing Features, Ranked By Daily-Use Value

1. One-home-per-deck convention (doc-only, immediate): stable card ids plus
   converter progress-migration already make re-import preserve 3DS progress;
   drift only hurts when the same deck is reviewed both on-device and in
   desktop Anki. Document the convention.
2. Cloze support in the converter: `tools/import_anki_collection.py` hard
   rejects `card_ord != 0`; render cloze deletions per-ordinal at convert time
   (front shows `[...]`/`[hint]`, back shows resolved text). Python + tests
   only; no device changes; unlocks a large fraction of real decks.
3. Ease-based interval growth: `ease_permille` is persisted in state.tsv v2
   but write-only; use it in the Good/Easy multipliers. This change is the
   trigger to extract `study_scheduler.c`/`study_state_tsv.c` from
   `study_backend.c`.
4. Anki write-back exporter: desktop tool replaying `review-log.tsv`
   (timestamps + ratings already captured) through AnkiConnect `answerCards`,
   so Anki's own scheduler does the math. Do not write Anki's SQLite directly;
   its v3 scheduler internals churn and hand-written revlog/queue rows are
   corruption-prone.
5. Stats/streak screen: data already in `review-log.tsv`; needs a log reader,
   one contract/flow pair, and a renderer section.
6. UTF-8/font-coverage warnings in the converter for language decks; fold
   into the cloze work.

### Do Not Do

No rewrite of any layer; no C++/framework migration; no FSRS parity (state
already stores enough to upgrade later); no media, templates, `.apkg` output,
or two-way sync engine; no contract collapse; no build-flag forks of
diagnostics; no growing `verify_app_theme.py` or the `M7_*` flag ecosystem
back.

### Parallelization Notes

- Wave A (parallel): Phase 0 installs alongside Phase 1 — the CI fix needs no
  local toolchain.
- Phase 2 cannot be delegated or automated: the milestone is a human
  acceptance gate by design, and macOS Accessibility blocks Azahar key
  automation regardless. It is the critical path; other waves run around it.
- Wave B (parallel worktrees): simplification items 1, 2, 4, 5 touch disjoint
  files. Items 3 and 6 follow after item 1 lands (they edit the Makefile link
  lists it owns).
- Wave C (parallel worktrees): the four Phase 5 feature tracks have near-zero
  file overlap.
- Merge discipline: serialize full test runs at merge time — host test
  binaries compile to fixed `/tmp/anki3ds-test-*` paths, so concurrent
  `test-host` runs across worktrees race on those binaries.

## Performance Review

Audited 2026-06-10 against original-3DS constraints: 268 MHz ARM11, slow FAT
SD I/O, battery-sensitive.

### Already efficient (keep as is)

- Idle is genuinely idle: dirty-only redraw; when clean, the loop waits on
  `gspWaitForVBlank()` or `hidWaitForAnyEvent()` with backoff (`main.c:59-71`)
  — no busy spin, ~0% CPU on an idle review screen.
- Rendering: one 8192-glyph C2D text buffer created at init and cleared per
  frame, never reallocated; sprite sheets loaded once; draws happen only when
  the shell marks the model dirty.
- Parsing: word wrap and TSV split are single-pass O(n); no
  strlen-inside-loop patterns.
- Memory: shell and renderer storage are static (BSS), not stack;
  `study_backend` is ~19.6 KB; the undo rollback is a struct assignment on
  the save path only. No stack pressure (the loader issue was fixed in
  `0470b59`).
- Durability: state saves use tmp-file + rename with a `.bak` generation —
  atomic on FAT, power-loss tolerant.
- State save serializes only loaded cards, not the full 1024-slot capacity.

### Ranked inefficiencies worth fixing (future optimization)

1. Synchronous SD I/O on the rating path (`app_state_save.c:99-194`): every
   rating performs blocking writes (state rewrite + review-log append + three
   rename-family operations) on the single thread — ~10-15 ms visible stall
   per rating on slow cards. Fix sketch: mark dirty, flush on the next idle
   frame (the idle loop already exists), keeping exit/HOME flush mandatory.
2. Full `state.tsv` rewrite per rating (`study_backend.c:1354-1470`): ~2-3 KB
   rewritten per rating; with 50+ ratings per session this is unnecessary
   write amplification on SD. Fix sketch: append-only delta rows during a
   session, compact to the full format on deck close/exit.
3. Deck-selector scan (`study_deck_index.c:437-528`): every selector entry
   re-reads every deck's `cards.tsv` and `state.tsv` for Due/New/Susp counts —
   ~30 file opens and ~300-500 ms for 10×500-card decks. Fix sketch: cache the
   scan; invalidate on day change, deck open/close, or explicit SELECT rescan.
4. Test-build redundancy (`Makefile` test-host blocks): `study_backend.c` is
   recompiled 18 times per `make test-host` with no object reuse — seconds of
   wasted build per run. Fix sketch: compile shared objects once and link
   (pairs naturally with the Phase 3 Makefile template).

### Looks inefficient, does not matter (measured reasoning)

- Undo rollback struct copy: ~19.6 KB assignment, microseconds, and only on
  the save path.
- Text re-wrap during screen-model build: O(text length) ≈ 500 iterations for
  a 512-byte card, ~1-2 ms, and only on dirty frames.
- Both screens drawn per dirty frame: Citro2D batches; ~5 ms, dwarfed by SD
  I/O, and dirty frames are rare.
- Queue reposition linear scans: two O(card_count) passes ≈ 2,000 comparisons
  per rating, ~1 ms; not worth indexing at 1,024-card scale.
