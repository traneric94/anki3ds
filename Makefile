AZAHAR_APP ?= $(HOME)/Applications/azahar-macos-arm64-2125.1.2/Azahar.app
AZAHAR_SDMC ?= $(HOME)/Library/Application Support/Azahar/sdmc
LOCAL_SDMC ?= local/sdmc
PACKAGE_SDMC ?= dist/sdmc
HOST_CC ?= cc
HOST_CFLAGS ?= -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iapp-3ds/include

APP_SD_DIR := 3ds/anki3ds
APP_3DSX := $(abspath app-3ds/anki3ds.3dsx)
SAMPLE_DECKS := limits-demo sample
REMOVED_SAMPLE_DECKS := media-demo
SAMPLE_DECK_SD_ROOT := $(APP_SD_DIR)/decks
SAMPLE_PROGRESS_FILE_CASE := state.tsv|state.tsv.tmp|state.tsv.bak|review-log.tsv|review-log.tsv.tmp|review-log.tsv.bak
VERIFY_TEXT_DECK := python3 tools/verify_text_deck.py
VERIFY_AZAHAR_CONTROLS := python3 tools/verify_azahar_controls.py
VERIFY_M7_ARTIFACTS := python3 tools/verify_m7_artifacts.py
IMPORT_FE_THEME_ASSETS := python3 tools/import_fe_theme_assets.py
M7_SDMC ?= $(AZAHAR_SDMC)

.PHONY: all app-3ds clean test test-host test-converter test-tools verify-ci verify-local verify-m7-preflight verify-m7-artifacts verify-sample-decks verify-azahar-controls verify-fe-theme-assets check-package-sd-root package-sd verify-package-sd install-local-sd verify-local-sd install-local-sample-deck install-local-sample-decks reset-local-sample-progress prepare-local-samples-fresh install-azahar-sample-deck install-azahar-sample-decks reset-azahar-sample-progress prepare-azahar-samples-fresh verify-azahar-fresh-samples check-emulator run-emulator run-emulator-samples run-emulator-fresh-samples

all: app-3ds

app-3ds:
	$(MAKE) -C app-3ds

clean:
	$(MAKE) -C app-3ds clean

test: test-host test-converter test-tools

test-host:
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_deck_scheduler.c \
		app-3ds/source/app_controls.c \
		app-3ds/source/app_power.c \
		app-3ds/source/app_review.c \
		app-3ds/source/app_settings.c \
		app-3ds/source/app_status.c \
		app-3ds/source/app_text.c \
		app-3ds/source/app_time.c \
		app-3ds/source/deck.c \
		app-3ds/source/deck_index.c \
		app-3ds/source/deck_summary.c \
		app-3ds/source/review_log.c \
		app-3ds/source/review_state.c \
		app-3ds/source/scheduler.c \
		app-3ds/source/storage.c \
		-o /tmp/anki3ds-test-deck-scheduler
	/tmp/anki3ds-test-deck-scheduler
	$(HOST_CC) $(HOST_CFLAGS) -D__3DS__ -Itests/stubs \
		tests/test_app_controls_3ds_keys.c \
		app-3ds/source/app_controls.c \
		-o /tmp/anki3ds-test-app-controls-3ds-keys
	/tmp/anki3ds-test-app-controls-3ds-keys

test-converter:
	python3 -m unittest tests/test_converter.py

test-tools:
	python3 -m unittest tests/test_verify_text_deck.py
	python3 -m unittest tests/test_verify_azahar_controls.py
	python3 -m unittest tests/test_verify_m7_artifacts.py
	python3 -m unittest tests/test_fe_theme_assets.py

verify-ci: test verify-sample-decks

verify-local: test verify-sample-decks verify-local-sd verify-package-sd

verify-m7-preflight: verify-local verify-azahar-fresh-samples verify-azahar-controls

verify-m7-artifacts:
	$(VERIFY_M7_ARTIFACTS) --sdmc "$(M7_SDMC)" \
		--deck limits-demo --deck sample \
		--require-event rating --require-event undo \
		--require-event suspend --require-event restore

verify-sample-decks:
	@set -e; \
	for deck in $(SAMPLE_DECKS); do \
		$(VERIFY_TEXT_DECK) "sample-decks/$$deck"; \
	done

verify-azahar-controls:
	$(VERIFY_AZAHAR_CONTROLS)

verify-fe-theme-assets:
	$(IMPORT_FE_THEME_ASSETS)

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

verify-package-sd: package-sd
	@set -e; \
	app_dir="$(PACKAGE_SDMC)/$(APP_SD_DIR)"; \
	test -f "$$app_dir/anki3ds.3dsx" || { echo "$$app_dir/anki3ds.3dsx missing"; exit 1; }; \
	test -f "$$app_dir/anki3ds.smdh" || { echo "$$app_dir/anki3ds.smdh missing"; exit 1; }; \
	for deck in $(REMOVED_SAMPLE_DECKS); do \
		test ! -e "$$app_dir/decks/$$deck" || { echo "$$app_dir/decks/$$deck must not be packaged"; exit 1; }; \
	done; \
	for deck in $(SAMPLE_DECKS); do \
		$(VERIFY_TEXT_DECK) "$$app_dir/decks/$$deck"; \
	done

install-local-sd: app-3ds install-local-sample-decks
	mkdir -p "$(LOCAL_SDMC)/$(APP_SD_DIR)"
	cp app-3ds/anki3ds.3dsx "$(LOCAL_SDMC)/$(APP_SD_DIR)/anki3ds.3dsx"
	cp app-3ds/anki3ds.smdh "$(LOCAL_SDMC)/$(APP_SD_DIR)/anki3ds.smdh"

verify-local-sd: prepare-local-samples-fresh
	@set -e; \
	app_dir="$(LOCAL_SDMC)/$(APP_SD_DIR)"; \
	test -f "$$app_dir/anki3ds.3dsx" || { echo "$$app_dir/anki3ds.3dsx missing"; exit 1; }; \
	test -f "$$app_dir/anki3ds.smdh" || { echo "$$app_dir/anki3ds.smdh missing"; exit 1; }; \
	for deck in $(REMOVED_SAMPLE_DECKS); do \
		test ! -e "$$app_dir/decks/$$deck" || { echo "$$app_dir/decks/$$deck must not be installed"; exit 1; }; \
	done; \
	for deck in $(SAMPLE_DECKS); do \
		$(VERIFY_TEXT_DECK) "$$app_dir/decks/$$deck"; \
	done

install-local-sample-deck: install-local-sample-decks

install-local-sample-decks: verify-sample-decks
	set -e; \
	for deck in $(REMOVED_SAMPLE_DECKS); do \
		rm -rf "$(LOCAL_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
	done; \
	for deck in $(SAMPLE_DECKS); do \
		deck_dir="$(LOCAL_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
		mkdir -p "$$deck_dir"; \
		for path in "$$deck_dir"/* "$$deck_dir"/.[!.]* "$$deck_dir"/..?*; do \
			test -e "$$path" || continue; \
			name=$${path##*/}; \
			case "$$name" in \
				$(SAMPLE_PROGRESS_FILE_CASE)) ;; \
				*) rm -rf "$$path" ;; \
			esac; \
		done; \
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
	for deck in $(REMOVED_SAMPLE_DECKS); do \
		rm -rf "$(AZAHAR_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
	done; \
	for deck in $(SAMPLE_DECKS); do \
		deck_dir="$(AZAHAR_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
		mkdir -p "$$deck_dir"; \
		for path in "$$deck_dir"/* "$$deck_dir"/.[!.]* "$$deck_dir"/..?*; do \
			test -e "$$path" || continue; \
			name=$${path##*/}; \
			case "$$name" in \
				$(SAMPLE_PROGRESS_FILE_CASE)) ;; \
				*) rm -rf "$$path" ;; \
			esac; \
		done; \
		cp -R "sample-decks/$$deck/." "$$deck_dir/"; \
	done

reset-azahar-sample-progress:
	set -e; \
	for deck in $(SAMPLE_DECKS); do \
		deck_dir="$(AZAHAR_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
		rm -f "$$deck_dir/state.tsv" "$$deck_dir/state.tsv.tmp" "$$deck_dir/state.tsv.bak" "$$deck_dir/review-log.tsv" "$$deck_dir/review-log.tsv.tmp" "$$deck_dir/review-log.tsv.bak"; \
	done

prepare-azahar-samples-fresh: install-azahar-sample-decks reset-azahar-sample-progress

verify-azahar-fresh-samples: prepare-azahar-samples-fresh
	@set -e; \
	app_dir="$(AZAHAR_SDMC)/$(APP_SD_DIR)"; \
	for deck in $(REMOVED_SAMPLE_DECKS); do \
		test ! -e "$$app_dir/decks/$$deck" || { echo "$$app_dir/decks/$$deck must not be installed"; exit 1; }; \
	done; \
	for deck in $(SAMPLE_DECKS); do \
		$(VERIFY_TEXT_DECK) "$$app_dir/decks/$$deck"; \
	done

check-emulator:
	@test -d "$(AZAHAR_APP)" || \
		(echo "Azahar not found at $(AZAHAR_APP). Set AZAHAR_APP=/path/to/Azahar.app"; exit 1)

run-emulator: app-3ds check-emulator
	open -a "$(AZAHAR_APP)" "$(APP_3DSX)"

run-emulator-samples: install-azahar-sample-decks run-emulator

run-emulator-fresh-samples: verify-azahar-fresh-samples run-emulator
