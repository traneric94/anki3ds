AZAHAR_APP ?= $(HOME)/Applications/azahar-macos-arm64-2125.1.2/Azahar.app
AZAHAR_SDMC ?= $(HOME)/Library/Application Support/Azahar/sdmc
LOCAL_SDMC ?= local/sdmc

APP_SD_DIR := 3ds/anki3ds
SAMPLE_DECK := sample
SAMPLE_DECK_SRC := sample-decks/$(SAMPLE_DECK)
SAMPLE_DECK_SD_DIR := $(APP_SD_DIR)/decks/$(SAMPLE_DECK)

.PHONY: all app-3ds clean test test-host test-converter install-local-sd install-local-sample-deck install-azahar-sample-deck check-emulator run-emulator

all: app-3ds

app-3ds:
	$(MAKE) -C app-3ds

clean:
	$(MAKE) -C app-3ds clean

test: test-host test-converter

test-host:
	cc -std=c99 -Wall -Wextra -Werror -Iapp-3ds/include \
		tests/test_deck_scheduler.c \
		app-3ds/source/app_settings.c \
		app-3ds/source/deck.c \
		app-3ds/source/deck_index.c \
		app-3ds/source/review_state.c \
		app-3ds/source/scheduler.c \
		-o /private/tmp/anki3ds-test-deck-scheduler
	/private/tmp/anki3ds-test-deck-scheduler

test-converter:
	python3 -m unittest tests/test_converter.py

install-local-sd: app-3ds install-local-sample-deck
	mkdir -p "$(LOCAL_SDMC)/$(APP_SD_DIR)"
	cp app-3ds/anki3ds.3dsx "$(LOCAL_SDMC)/$(APP_SD_DIR)/anki3ds.3dsx"
	cp app-3ds/anki3ds.smdh "$(LOCAL_SDMC)/$(APP_SD_DIR)/anki3ds.smdh"

install-local-sample-deck:
	mkdir -p "$(LOCAL_SDMC)/$(SAMPLE_DECK_SD_DIR)"
	cp -R "$(SAMPLE_DECK_SRC)/." "$(LOCAL_SDMC)/$(SAMPLE_DECK_SD_DIR)/"

install-azahar-sample-deck:
	mkdir -p "$(AZAHAR_SDMC)/$(SAMPLE_DECK_SD_DIR)"
	cp -R "$(SAMPLE_DECK_SRC)/." "$(AZAHAR_SDMC)/$(SAMPLE_DECK_SD_DIR)/"

check-emulator:
	@test -d "$(AZAHAR_APP)" || \
		(echo "Azahar not found at $(AZAHAR_APP). Set AZAHAR_APP=/path/to/Azahar.app"; exit 1)

run-emulator: app-3ds check-emulator
	open -a "$(AZAHAR_APP)" app-3ds/anki3ds.3dsx
