AZAHAR_APP ?= $(HOME)/Applications/azahar-macos-arm64-2125.1.2/Azahar.app
AZAHAR_SDMC ?= $(HOME)/Library/Application Support/Azahar/sdmc
LOCAL_SDMC ?= local/sdmc
PACKAGE_SDMC ?= dist/sdmc
HOST_CC ?= cc
HOST_CFLAGS ?= -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iapp-3ds/include

APP_SD_DIR := 3ds/anki3ds
SAMPLE_DECKS := limits-demo sample
OPTIONAL_SAMPLE_DECKS := media-demo
SAMPLE_DECK_SD_ROOT := $(APP_SD_DIR)/decks

.PHONY: all app-3ds clean test test-host test-converter verify-ci verify-local verify-sample-decks check-package-sd-root package-sd install-local-sd install-local-sample-deck install-local-sample-decks reset-local-sample-progress prepare-local-samples-fresh install-azahar-sample-deck install-azahar-sample-decks reset-azahar-sample-progress prepare-azahar-samples-fresh check-emulator run-emulator run-emulator-samples run-emulator-fresh-samples

all: app-3ds

app-3ds:
	$(MAKE) -C app-3ds

clean:
	$(MAKE) -C app-3ds clean

test: test-host test-converter

test-host:
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_deck_scheduler.c \
		app-3ds/source/app_controls.c \
		app-3ds/source/app_layout.c \
		app-3ds/source/app_power.c \
		app-3ds/source/app_review.c \
		app-3ds/source/app_settings.c \
		app-3ds/source/app_text.c \
		app-3ds/source/app_time.c \
		app-3ds/source/deck.c \
		app-3ds/source/deck_index.c \
		app-3ds/source/deck_summary.c \
		app-3ds/source/media_cache.c \
		app-3ds/source/media_image.c \
		app-3ds/source/review_log.c \
		app-3ds/source/review_state.c \
		app-3ds/source/scheduler.c \
		app-3ds/source/storage.c \
		-o /tmp/anki3ds-test-deck-scheduler
	/tmp/anki3ds-test-deck-scheduler

test-converter:
	python3 -m unittest tests/test_converter.py

verify-ci: test verify-sample-decks

verify-local: test verify-sample-decks install-local-sd

verify-sample-decks:
	@set -e; \
	for deck in $(SAMPLE_DECKS); do \
		deck_dir="sample-decks/$$deck"; \
		test -d "$$deck_dir" || { echo "$$deck_dir missing"; exit 1; }; \
		test -f "$$deck_dir/deck.json" || { echo "$$deck_dir/deck.json missing"; exit 1; }; \
		test -f "$$deck_dir/cards.tsv" || { echo "$$deck_dir/cards.tsv missing"; exit 1; }; \
		test -f "$$deck_dir/settings.tsv" || { echo "$$deck_dir/settings.tsv missing"; exit 1; }; \
		python3 -c 'import json, pathlib, sys; deck = sys.argv[1]; root = pathlib.Path("sample-decks") / deck; data = json.loads((root / "deck.json").read_text(encoding="utf-8")); rows = (root / "cards.tsv").read_text(encoding="utf-8").splitlines(); errors = []; errors += [] if data.get("format_version") == 1 else [f"{root}/deck.json: format_version must be 1"]; errors += [] if data.get("deck_id") == deck else [f"{root}/deck.json: deck_id must match folder name"]; errors += [] if isinstance(data.get("name"), str) and data.get("name") else [f"{root}/deck.json: name is required"]; errors += [] if data.get("card_count") == len(rows) else [f"{root}/deck.json: card_count must match cards.tsv row count"]; sys.exit("\n".join(errors)) if errors else None' "$$deck"; \
		awk -F '\t' 'NF != 5 && NF != 7 { printf "%s:%d: expected 5 or 7 tab-separated fields, got %d\n", FILENAME, NR, NF; bad = 1 } $$1 == "" { printf "%s:%d: card_id is required\n", FILENAME, NR; bad = 1 } seen[$$1]++ { printf "%s:%d: duplicate card_id %s\n", FILENAME, NR, $$1; bad = 1 } END { exit bad }' "$$deck_dir/cards.tsv"; \
		awk -F '\t' '$$1 == "new_limit" && $$2 ~ /^[0-9]+$$/ { new += 1; next } $$1 == "review_limit" && $$2 ~ /^[0-9]+$$/ { review += 1; next } { printf "%s:%d: expected new_limit or review_limit with a non-negative integer value\n", FILENAME, NR; bad = 1 } END { if (new != 1 || review != 1 || NR != 2) { printf "%s: expected exactly one new_limit row and one review_limit row\n", FILENAME; bad = 1 } exit bad }' "$$deck_dir/settings.tsv"; \
		awk -F '\t' 'NF == 7 { if ($$6 != "") print $$6; if ($$7 != "") print $$7 }' "$$deck_dir/cards.tsv" | while IFS= read -r media; do \
			media_path="$$deck_dir/media/$$media"; \
			test -f "$$media_path" || { echo "$$deck_dir/cards.tsv references missing media $$media"; exit 1; }; \
			python3 -c 'import pathlib, sys; from converter.anki3ds_convert import validate_a3i_content; path = pathlib.Path(sys.argv[1]); sys.exit(f"{path}: referenced sample media must be .a3i") if path.suffix.lower() != ".a3i" else None; validate_a3i_content(path, path.read_bytes())' "$$media_path"; \
		done; \
		for progress_file in state.tsv state.tsv.tmp state.tsv.bak review-log.tsv review-log.tsv.tmp review-log.tsv.bak settings.tsv.tmp settings.tsv.bak; do \
			test ! -e "$$deck_dir/$$progress_file" || { echo "$$deck_dir/$$progress_file must not be committed with tracked samples"; exit 1; }; \
		done; \
	done

check-package-sd-root:
	@set -e; \
	package_abs=$$(python3 -c 'import pathlib, sys; print(pathlib.Path(sys.argv[1]).expanduser().resolve())' "$(PACKAGE_SDMC)"); \
	dist_abs=$$(python3 -c 'import pathlib; print(pathlib.Path("dist").resolve())'); \
	case "$$package_abs" in \
		"$$dist_abs"|"$$dist_abs"/*) ;; \
		*) echo "PACKAGE_SDMC must stay under dist/ because package-sd cleans its app directory"; exit 1 ;; \
	esac

package-sd: check-package-sd-root verify-sample-decks app-3ds
	rm -rf "$(PACKAGE_SDMC)/$(APP_SD_DIR)"
	mkdir -p "$(PACKAGE_SDMC)/$(APP_SD_DIR)"
	cp app-3ds/anki3ds.3dsx "$(PACKAGE_SDMC)/$(APP_SD_DIR)/anki3ds.3dsx"
	cp app-3ds/anki3ds.smdh "$(PACKAGE_SDMC)/$(APP_SD_DIR)/anki3ds.smdh"
	set -e; \
	for deck in $(SAMPLE_DECKS); do \
		deck_dir="$(PACKAGE_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
		mkdir -p "$$deck_dir"; \
		cp -R "sample-decks/$$deck/." "$$deck_dir/"; \
	done

install-local-sd: app-3ds install-local-sample-decks
	mkdir -p "$(LOCAL_SDMC)/$(APP_SD_DIR)"
	cp app-3ds/anki3ds.3dsx "$(LOCAL_SDMC)/$(APP_SD_DIR)/anki3ds.3dsx"
	cp app-3ds/anki3ds.smdh "$(LOCAL_SDMC)/$(APP_SD_DIR)/anki3ds.smdh"

install-local-sample-deck: install-local-sample-decks

install-local-sample-decks: verify-sample-decks
	set -e; \
	for deck in $(OPTIONAL_SAMPLE_DECKS); do \
		rm -rf "$(LOCAL_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
	done; \
	for deck in $(SAMPLE_DECKS); do \
		deck_dir="$(LOCAL_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
		mkdir -p "$$deck_dir"; \
		rm -f "$$deck_dir/deck.json" "$$deck_dir/cards.tsv" "$$deck_dir/settings.tsv"; \
		rm -rf "$$deck_dir/media"; \
		cp -R "sample-decks/$$deck/." "$$deck_dir/"; \
	done

reset-local-sample-progress:
	set -e; \
	for deck in $(SAMPLE_DECKS); do \
		deck_dir="$(LOCAL_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
		rm -f "$$deck_dir/state.tsv" "$$deck_dir/state.tsv.tmp" "$$deck_dir/state.tsv.bak" "$$deck_dir/review-log.tsv" "$$deck_dir/review-log.tsv.tmp" "$$deck_dir/review-log.tsv.bak"; \
	done

prepare-local-samples-fresh: install-local-sd reset-local-sample-progress

install-azahar-sample-deck: install-azahar-sample-decks

install-azahar-sample-decks: verify-sample-decks
	set -e; \
	for deck in $(OPTIONAL_SAMPLE_DECKS); do \
		rm -rf "$(AZAHAR_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
	done; \
	for deck in $(SAMPLE_DECKS); do \
		deck_dir="$(AZAHAR_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
		mkdir -p "$$deck_dir"; \
		rm -f "$$deck_dir/deck.json" "$$deck_dir/cards.tsv" "$$deck_dir/settings.tsv"; \
		rm -rf "$$deck_dir/media"; \
		cp -R "sample-decks/$$deck/." "$$deck_dir/"; \
	done

reset-azahar-sample-progress:
	set -e; \
	for deck in $(SAMPLE_DECKS); do \
		deck_dir="$(AZAHAR_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
		rm -f "$$deck_dir/state.tsv" "$$deck_dir/state.tsv.tmp" "$$deck_dir/state.tsv.bak" "$$deck_dir/review-log.tsv" "$$deck_dir/review-log.tsv.tmp" "$$deck_dir/review-log.tsv.bak"; \
	done

prepare-azahar-samples-fresh: install-azahar-sample-decks reset-azahar-sample-progress

check-emulator:
	@test -d "$(AZAHAR_APP)" || \
		(echo "Azahar not found at $(AZAHAR_APP). Set AZAHAR_APP=/path/to/Azahar.app"; exit 1)

run-emulator: app-3ds check-emulator
	open -a "$(AZAHAR_APP)" app-3ds/anki3ds.3dsx

run-emulator-samples: install-azahar-sample-decks run-emulator

run-emulator-fresh-samples: prepare-azahar-samples-fresh run-emulator
