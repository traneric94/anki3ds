AZAHAR_APP ?= $(HOME)/Applications/azahar-macos-arm64-2125.1.2/Azahar.app
AZAHAR_SDMC ?= $(HOME)/Library/Application Support/Azahar/sdmc
LOCAL_SDMC ?= local/sdmc
HOST_CC ?= cc
HOST_CFLAGS ?= -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iapp-3ds/include

APP_SD_DIR := 3ds/anki3ds
SAMPLE_DECKS := limits-demo media-demo sample
SAMPLE_DECK_SD_ROOT := $(APP_SD_DIR)/decks

.PHONY: all app-3ds clean test test-host test-converter verify-local install-local-sd install-local-sample-deck install-local-sample-decks reset-local-sample-progress prepare-local-samples-fresh install-azahar-sample-deck install-azahar-sample-decks reset-azahar-sample-progress prepare-azahar-samples-fresh check-emulator run-emulator run-emulator-samples

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

verify-local: test install-local-sd

install-local-sd: app-3ds install-local-sample-decks
	mkdir -p "$(LOCAL_SDMC)/$(APP_SD_DIR)"
	cp app-3ds/anki3ds.3dsx "$(LOCAL_SDMC)/$(APP_SD_DIR)/anki3ds.3dsx"
	cp app-3ds/anki3ds.smdh "$(LOCAL_SDMC)/$(APP_SD_DIR)/anki3ds.smdh"

install-local-sample-deck: install-local-sample-decks

install-local-sample-decks:
	set -e; \
	for deck in $(SAMPLE_DECKS); do \
		mkdir -p "$(LOCAL_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
		cp -R "sample-decks/$$deck/." "$(LOCAL_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck/"; \
	done

reset-local-sample-progress:
	set -e; \
	for deck in $(SAMPLE_DECKS); do \
		deck_dir="$(LOCAL_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
		rm -f "$$deck_dir/state.tsv" "$$deck_dir/state.tsv.tmp" "$$deck_dir/state.tsv.bak" "$$deck_dir/review-log.tsv"; \
	done

prepare-local-samples-fresh: install-local-sd reset-local-sample-progress

install-azahar-sample-deck: install-azahar-sample-decks

install-azahar-sample-decks:
	set -e; \
	for deck in $(SAMPLE_DECKS); do \
		mkdir -p "$(AZAHAR_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
		cp -R "sample-decks/$$deck/." "$(AZAHAR_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck/"; \
	done

reset-azahar-sample-progress:
	set -e; \
	for deck in $(SAMPLE_DECKS); do \
		deck_dir="$(AZAHAR_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
		rm -f "$$deck_dir/state.tsv" "$$deck_dir/state.tsv.tmp" "$$deck_dir/state.tsv.bak" "$$deck_dir/review-log.tsv"; \
	done

prepare-azahar-samples-fresh: install-azahar-sample-decks reset-azahar-sample-progress

check-emulator:
	@test -d "$(AZAHAR_APP)" || \
		(echo "Azahar not found at $(AZAHAR_APP). Set AZAHAR_APP=/path/to/Azahar.app"; exit 1)

run-emulator: app-3ds check-emulator
	open -a "$(AZAHAR_APP)" app-3ds/anki3ds.3dsx

run-emulator-samples: install-azahar-sample-decks run-emulator
