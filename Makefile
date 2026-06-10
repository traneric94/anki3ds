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
PERSONAL_DECKS ?= leetcode recsys sat-vocabulary vim
SAMPLE_DECK_SD_ROOT := $(APP_SD_DIR)/decks
SAMPLE_PROGRESS_FILE_CASE := state.tsv|state.tsv.tmp|state.tsv.bak|review-log.tsv|review-log.tsv.tmp|review-log.tsv.bak
APP_SESSION_FILES := session.tsv session.tsv.tmp session.tsv.bak renderer.tsv
APP_UI_THEME_FILES := theme.tsv theme.tsv.tmp theme.tsv.bak
VERIFY_TEXT_DECK := python3 tools/verify_text_deck.py
VERIFY_AZAHAR_CONTROLS := python3 tools/verify_azahar_controls.py
VERIFY_M7_ARTIFACTS := python3 tools/verify_m7_artifacts.py
DRIVE_AZAHAR_M7_SMOKE := python3 tools/drive_azahar_m7_smoke.py
IMPORT_FE_THEME_ASSETS := python3 tools/import_fe_theme_assets.py
FE_BG_VIEWER_DIR := tools/fe-bg-viewer-3ds
FE_BG_VIEWER_3DSX := $(abspath $(FE_BG_VIEWER_DIR)/fe-bg-viewer.3dsx)
FE_THEME_FRAMEBUFFER_DIR := build/fe-theme-framebuffers
FE_THEME_PREVIEW_DIR := build/fe-theme-previews
FE_THEME_SD_DIR := $(APP_SD_DIR)/fe-themes
FE_UI_THEME ?= forest
APP_FE_EMBEDDED_THEME := forest
APP_FE_TOP_LEGEND_PREVIEW := $(FE_THEME_PREVIEW_DIR)/$(APP_FE_EMBEDDED_THEME)_top_layer_legend_400x240.png
APP_FE_BOTTOM_LEGEND_PREVIEW := $(FE_THEME_PREVIEW_DIR)/$(APP_FE_EMBEDDED_THEME)_bottom_layer_legend_320x240.png
APP_FE_TOP_LEGEND_PNG := app-3ds/gfx/fe_forest_top_legend.png
APP_FE_BOTTOM_LEGEND_PNG := app-3ds/gfx/fe_forest_bottom_legend.png
APP_FE_TOP_LEGEND_T3S := app-3ds/gfx/fe_forest_top_legend.t3s
APP_FE_BOTTOM_LEGEND_T3S := app-3ds/gfx/fe_forest_bottom_legend.t3s
M7_SDMC ?= $(AZAHAR_SDMC)
M7_DECKS ?= limits-demo sample
M7_REQUIRED_EVENTS ?= rating undo suspend restore
M7_EXPECT_SETTINGS ?=
M7_ALLOW_MISSING_REVIEW_LOG ?= 0
M7_NO_REQUIRED_EVENTS ?= 0
M7_RESET_DECKS ?=
M7_SMOKE_DECKS ?= limits-demo
M7_SMOKE_RESET_DECKS ?= sample
M7_SMOKE_EXPECT_SETTINGS ?= limits-demo:5:10 sample:20:200
M7_STUDY_DECKS = $(filter-out $(M7_RESET_DECKS),$(M7_DECKS))
M7_DECK_ARGS = $(foreach deck,$(M7_STUDY_DECKS),--deck $(deck))
M7_NO_STUDY_DECKS_ARG = $(if $(M7_STUDY_DECKS),,--no-study-decks)
M7_NO_REQUIRED_EVENTS_ARG = $(if $(filter 1 yes true,$(M7_NO_REQUIRED_EVENTS)),--no-required-events)
M7_REQUIRED_EVENT_ARGS = $(if $(M7_NO_REQUIRED_EVENTS_ARG),,\
	$(foreach event,$(M7_REQUIRED_EVENTS),--require-event $(event)))
M7_EXPECT_SETTING_ARGS = $(foreach setting,$(M7_EXPECT_SETTINGS),--expect-settings $(setting))
M7_RESET_DECK_ARGS = $(foreach deck,$(M7_RESET_DECKS),--expect-reset-deck $(deck))
M7_ALLOW_MISSING_REVIEW_LOG_ARG = $(if $(filter 1 yes true,$(M7_ALLOW_MISSING_REVIEW_LOG)),--allow-missing-review-log)
M7_ARTIFACT_ARGS = $(strip \
	$(M7_DECK_ARGS) \
	$(M7_NO_STUDY_DECKS_ARG) \
	$(M7_RESET_DECK_ARGS) \
	$(M7_REQUIRED_EVENT_ARGS) \
	$(M7_EXPECT_SETTING_ARGS) \
	$(M7_ALLOW_MISSING_REVIEW_LOG_ARG) \
	$(M7_NO_REQUIRED_EVENTS_ARG))

.PHONY: all app-3ds sync-app-fe-ui-assets fe-bg-viewer-3ds clean clean-fe-bg-viewer-3ds test test-host test-converter test-tools verify-ci verify-local verify-m7-preflight verify-m7-artifacts verify-azahar-m7-smoke-artifacts verify-sample-decks verify-local-personal-decks verify-azahar-personal-decks verify-azahar-controls fix-azahar-controls verify-fe-theme-assets check-package-sd-root package-sd verify-package-sd install-local-sd install-local-fe-theme-assets verify-local-sd install-local-sample-deck install-local-sample-decks reset-local-sample-progress prepare-local-samples-fresh install-azahar-app install-azahar-sample-deck install-azahar-sample-decks install-azahar-fe-theme-assets install-azahar-fe-bg-viewer install-azahar-fe-ui-theme clear-azahar-fe-ui-theme reset-azahar-sample-progress prepare-azahar-samples-fresh prepare-azahar-daily-use verify-azahar-fresh-samples check-emulator check-azahar-smoke-automation drive-azahar-m7-smoke-dry-run drive-azahar-m7-smoke run-emulator run-emulator-samples run-emulator-fresh-samples run-emulator-daily-use run-emulator-m7-smoke run-emulator-fe-ui run-emulator-fe-bg-viewer

all: app-3ds

app-3ds: sync-app-fe-ui-assets
	$(MAKE) -C app-3ds

sync-app-fe-ui-assets: verify-fe-theme-assets
	cmp -s "$(APP_FE_TOP_LEGEND_PREVIEW)" "$(APP_FE_TOP_LEGEND_PNG)" || { cp "$(APP_FE_TOP_LEGEND_PREVIEW)" "$(APP_FE_TOP_LEGEND_PNG)"; touch "$(APP_FE_TOP_LEGEND_T3S)"; }
	cmp -s "$(APP_FE_BOTTOM_LEGEND_PREVIEW)" "$(APP_FE_BOTTOM_LEGEND_PNG)" || { cp "$(APP_FE_BOTTOM_LEGEND_PREVIEW)" "$(APP_FE_BOTTOM_LEGEND_PNG)"; touch "$(APP_FE_BOTTOM_LEGEND_T3S)"; }

fe-bg-viewer-3ds: verify-fe-theme-assets
	$(MAKE) -C $(FE_BG_VIEWER_DIR)

clean:
	$(MAKE) -C app-3ds clean
	$(MAKE) -C $(FE_BG_VIEWER_DIR) clean

clean-fe-bg-viewer-3ds:
	$(MAKE) -C $(FE_BG_VIEWER_DIR) clean

test: test-host test-converter test-tools

test-host:
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_battery_status.c \
		app-3ds/source/app_battery_status.c \
		app-3ds/source/app_power.c \
		-o /tmp/anki3ds-test-app-battery-status
	/tmp/anki3ds-test-app-battery-status
	$(HOST_CC) $(HOST_CFLAGS) -Itests/stubs \
		tests/test_app_battery_monitor.c \
		app-3ds/source/app_battery_monitor.c \
		app-3ds/source/app_battery_status.c \
		app-3ds/source/app_power.c \
		app-3ds/source/study_backend.c \
		-o /tmp/anki3ds-test-app-battery-monitor
	/tmp/anki3ds-test-app-battery-monitor
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_confirm_action.c \
		app-3ds/source/app_confirm_action.c \
		-o /tmp/anki3ds-test-app-confirm-action
	/tmp/anki3ds-test-app-confirm-action
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_confirm_contract.c \
		app-3ds/source/app_confirm_contract.c \
		-o /tmp/anki3ds-test-app-confirm-contract
	/tmp/anki3ds-test-app-confirm-contract
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_confirm_flow.c \
		app-3ds/source/app_confirm_flow.c \
		app-3ds/source/app_confirm_action.c \
		app-3ds/source/app_confirm_contract.c \
		app-3ds/source/app_mode.c \
		app-3ds/source/app_reset_action.c \
		app-3ds/source/app_status_text.c \
		app-3ds/source/study_backend.c \
		app-3ds/source/study_review_log.c \
		app-3ds/source/study_session.c \
		-o /tmp/anki3ds-test-app-confirm-flow
	/tmp/anki3ds-test-app-confirm-flow
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_flashcard_contract.c \
		app-3ds/source/app_flashcard_contract.c \
		app-3ds/source/app_text.c \
		app-3ds/source/study_backend.c \
		app-3ds/source/study_settings.c \
		-o /tmp/anki3ds-test-app-flashcard-contract
	/tmp/anki3ds-test-app-flashcard-contract
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_screen_model.c \
		app-3ds/source/app_screen_model.c \
		app-3ds/source/app_confirm_contract.c \
		app-3ds/source/app_deck_navigation.c \
		app-3ds/source/app_deck_select_contract.c \
		app-3ds/source/app_flashcard_contract.c \
		app-3ds/source/app_settings_contract.c \
		app-3ds/source/study_backend.c \
		app-3ds/source/study_deck_index.c \
		app-3ds/source/study_settings.c \
		-o /tmp/anki3ds-test-app-screen-model
	/tmp/anki3ds-test-app-screen-model
	$(HOST_CC) $(HOST_CFLAGS) -Itests/stubs \
		tests/test_app_shell.c \
		app-3ds/source/app_shell.c \
		app-3ds/source/app_battery_monitor.c \
		app-3ds/source/app_battery_status.c \
		app-3ds/source/app_confirm_action.c \
		app-3ds/source/app_confirm_contract.c \
		app-3ds/source/app_confirm_flow.c \
		app-3ds/source/app_day_rollover_flow.c \
		app-3ds/source/app_deck_flow.c \
		app-3ds/source/app_deck_navigation.c \
		app-3ds/source/app_deck_select_action.c \
		app-3ds/source/app_deck_select_contract.c \
		app-3ds/source/app_flashcard_contract.c \
		app-3ds/source/app_mode.c \
		app-3ds/source/app_power.c \
		app-3ds/source/app_reset_action.c \
		app-3ds/source/app_review_action.c \
		app-3ds/source/app_review_flow.c \
		app-3ds/source/app_screen_model.c \
		app-3ds/source/app_session_save.c \
		app-3ds/source/app_settings_action.c \
		app-3ds/source/app_settings_contract.c \
		app-3ds/source/app_settings_flow.c \
		app-3ds/source/app_settings_save.c \
		app-3ds/source/app_state_save.c \
		app-3ds/source/app_status_text.c \
		app-3ds/source/app_text.c \
		app-3ds/source/app_time.c \
		app-3ds/source/study_backend.c \
		app-3ds/source/study_controls.c \
		app-3ds/source/study_deck_index.c \
		app-3ds/source/study_review_log.c \
		app-3ds/source/study_session.c \
		app-3ds/source/study_settings.c \
		-o /tmp/anki3ds-test-app-shell
	/tmp/anki3ds-test-app-shell
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_input_policy.c \
		app-3ds/source/app_input_policy.c \
		-o /tmp/anki3ds-test-app-input-policy
	/tmp/anki3ds-test-app-input-policy
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_input_frame.c \
		app-3ds/source/app_input_frame.c \
		app-3ds/source/app_input_policy.c \
		app-3ds/source/study_controls.c \
		-o /tmp/anki3ds-test-app-input-frame
	/tmp/anki3ds-test-app-input-frame
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_mode.c \
		app-3ds/source/app_mode.c \
		-o /tmp/anki3ds-test-app-mode
	/tmp/anki3ds-test-app-mode
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_reset_action.c \
		app-3ds/source/app_reset_action.c \
		app-3ds/source/app_status_text.c \
		app-3ds/source/study_backend.c \
		app-3ds/source/study_review_log.c \
		-o /tmp/anki3ds-test-app-reset-action
	/tmp/anki3ds-test-app-reset-action
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_state_save.c \
		app-3ds/source/app_state_save.c \
		app-3ds/source/app_status_text.c \
		app-3ds/source/study_backend.c \
		app-3ds/source/study_review_log.c \
		app-3ds/source/study_session.c \
		-o /tmp/anki3ds-test-app-state-save
	/tmp/anki3ds-test-app-state-save
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_day_rollover_flow.c \
		app-3ds/source/app_day_rollover_flow.c \
		app-3ds/source/app_state_save.c \
		app-3ds/source/app_status_text.c \
		app-3ds/source/study_backend.c \
		app-3ds/source/study_review_log.c \
		app-3ds/source/study_session.c \
		-o /tmp/anki3ds-test-app-day-rollover-flow
	/tmp/anki3ds-test-app-day-rollover-flow
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_session_save.c \
		app-3ds/source/app_session_save.c \
		app-3ds/source/study_backend.c \
		app-3ds/source/study_session.c \
		-o /tmp/anki3ds-test-app-session-save
	/tmp/anki3ds-test-app-session-save
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_power.c \
		app-3ds/source/app_power.c \
		-o /tmp/anki3ds-test-app-power
	/tmp/anki3ds-test-app-power
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_review_action.c \
		app-3ds/source/app_review_action.c \
		app-3ds/source/study_backend.c \
		-o /tmp/anki3ds-test-app-review-action
	/tmp/anki3ds-test-app-review-action
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_review_flow.c \
		app-3ds/source/app_review_flow.c \
		app-3ds/source/app_review_action.c \
		app-3ds/source/app_settings_action.c \
		app-3ds/source/app_status_text.c \
		app-3ds/source/study_backend.c \
		app-3ds/source/study_controls.c \
		app-3ds/source/study_session.c \
		app-3ds/source/study_settings.c \
		-o /tmp/anki3ds-test-app-review-flow
	/tmp/anki3ds-test-app-review-flow
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_status_text.c \
		app-3ds/source/app_status_text.c \
		-o /tmp/anki3ds-test-app-status-text
	/tmp/anki3ds-test-app-status-text
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_time.c \
		app-3ds/source/app_time.c \
		-o /tmp/anki3ds-test-app-time
	/tmp/anki3ds-test-app-time
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_deck_navigation.c \
		app-3ds/source/app_deck_navigation.c \
		-o /tmp/anki3ds-test-app-deck-navigation
	/tmp/anki3ds-test-app-deck-navigation
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_deck_select_action.c \
		app-3ds/source/app_deck_select_action.c \
		app-3ds/source/app_deck_navigation.c \
		-o /tmp/anki3ds-test-app-deck-select-action
	/tmp/anki3ds-test-app-deck-select-action
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_deck_flow.c \
		app-3ds/source/app_deck_flow.c \
		app-3ds/source/app_deck_navigation.c \
		app-3ds/source/app_deck_select_action.c \
		app-3ds/source/app_deck_select_contract.c \
		app-3ds/source/app_state_save.c \
		app-3ds/source/app_status_text.c \
		app-3ds/source/study_backend.c \
		app-3ds/source/study_deck_index.c \
		app-3ds/source/study_review_log.c \
		app-3ds/source/study_session.c \
		app-3ds/source/study_settings.c \
		-o /tmp/anki3ds-test-app-deck-flow
	/tmp/anki3ds-test-app-deck-flow
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_deck_select_contract.c \
		app-3ds/source/app_deck_select_contract.c \
		app-3ds/source/app_deck_navigation.c \
		app-3ds/source/study_deck_index.c \
		-o /tmp/anki3ds-test-app-deck-select-contract
	/tmp/anki3ds-test-app-deck-select-contract
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_settings_contract.c \
		app-3ds/source/app_settings_contract.c \
		app-3ds/source/study_settings.c \
		-o /tmp/anki3ds-test-app-settings-contract
	/tmp/anki3ds-test-app-settings-contract
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_settings_action.c \
		app-3ds/source/app_settings_action.c \
		app-3ds/source/study_settings.c \
		-o /tmp/anki3ds-test-app-settings-action
	/tmp/anki3ds-test-app-settings-action
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_settings_flow.c \
		app-3ds/source/app_settings_flow.c \
		app-3ds/source/app_settings_action.c \
		app-3ds/source/app_settings_save.c \
		app-3ds/source/app_status_text.c \
		app-3ds/source/study_backend.c \
		app-3ds/source/study_session.c \
		app-3ds/source/study_settings.c \
		-o /tmp/anki3ds-test-app-settings-flow
	/tmp/anki3ds-test-app-settings-flow
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_app_settings_save.c \
		app-3ds/source/app_settings_save.c \
		app-3ds/source/app_status_text.c \
		app-3ds/source/study_backend.c \
		app-3ds/source/study_session.c \
		app-3ds/source/study_settings.c \
		-o /tmp/anki3ds-test-app-settings-save
	/tmp/anki3ds-test-app-settings-save
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_study_backend.c \
		app-3ds/source/study_backend.c \
		-o /tmp/anki3ds-test-study-backend
	/tmp/anki3ds-test-study-backend
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_study_controls.c \
		app-3ds/source/study_controls.c \
		-o /tmp/anki3ds-test-study-controls
	/tmp/anki3ds-test-study-controls
	$(HOST_CC) $(HOST_CFLAGS) -Itests/stubs \
		tests/test_study_3ds_key_map.c \
		app-3ds/source/study_3ds_key_map.c \
		app-3ds/source/study_controls.c \
		-o /tmp/anki3ds-test-study-3ds-key-map
	/tmp/anki3ds-test-study-3ds-key-map
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_study_deck_index.c \
		app-3ds/source/study_deck_index.c \
		-o /tmp/anki3ds-test-study-deck-index
	/tmp/anki3ds-test-study-deck-index
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_study_settings.c \
		app-3ds/source/study_settings.c \
		-o /tmp/anki3ds-test-study-settings
	/tmp/anki3ds-test-study-settings
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_study_review_log.c \
		app-3ds/source/study_review_log.c \
		-o /tmp/anki3ds-test-study-review-log
	/tmp/anki3ds-test-study-review-log
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_study_session.c \
		app-3ds/source/study_session.c \
		-o /tmp/anki3ds-test-study-session
	/tmp/anki3ds-test-study-session
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_clean_shell_daily_use.c \
		app-3ds/source/study_backend.c \
		app-3ds/source/study_deck_index.c \
		app-3ds/source/study_review_log.c \
		app-3ds/source/study_session.c \
		app-3ds/source/study_settings.c \
		-o /tmp/anki3ds-test-clean-shell-daily-use
	/tmp/anki3ds-test-clean-shell-daily-use
	$(HOST_CC) $(HOST_CFLAGS) \
		tests/test_m7_direct_smoke_flow.c \
		app-3ds/source/study_backend.c \
		app-3ds/source/study_deck_index.c \
		app-3ds/source/study_review_log.c \
		app-3ds/source/study_session.c \
		app-3ds/source/study_settings.c \
		-o /tmp/anki3ds-test-m7-direct-smoke-flow
	/tmp/anki3ds-test-m7-direct-smoke-flow

test-converter:
	python3 -m unittest tests/test_converter.py

test-tools:
	python3 -m unittest tests/test_import_anki_collection.py
	python3 -m unittest tests/test_verify_text_deck.py
	python3 -m unittest tests/test_verify_azahar_controls.py
	python3 -m unittest tests/test_verify_app_theme.py
	python3 -m unittest tests/test_verify_m7_artifacts.py
	python3 -m unittest tests/test_drive_azahar_m7_smoke.py
	python3 -m unittest tests/test_fe_theme_assets.py

verify-ci: test verify-sample-decks

verify-local: test verify-sample-decks verify-local-sd verify-package-sd

verify-m7-preflight: verify-local verify-azahar-fresh-samples verify-azahar-controls

verify-m7-artifacts:
	$(VERIFY_M7_ARTIFACTS) --sdmc "$(M7_SDMC)" $(M7_ARTIFACT_ARGS)

verify-azahar-m7-smoke-artifacts:
	$(MAKE) verify-m7-artifacts M7_DECKS="$(M7_SMOKE_DECKS)" M7_RESET_DECKS="$(M7_SMOKE_RESET_DECKS)" M7_EXPECT_SETTINGS="$(M7_SMOKE_EXPECT_SETTINGS)"

verify-sample-decks:
	@set -e; \
	for deck in $(SAMPLE_DECKS); do \
		$(VERIFY_TEXT_DECK) "sample-decks/$$deck"; \
	done

verify-local-personal-decks:
	@set -e; \
	for deck in $(PERSONAL_DECKS); do \
		$(VERIFY_TEXT_DECK) --allow-progress-files "$(LOCAL_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
	done

verify-azahar-personal-decks:
	@set -e; \
	for deck in $(PERSONAL_DECKS); do \
		$(VERIFY_TEXT_DECK) --allow-progress-files "$(AZAHAR_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
	done

verify-azahar-controls:
	$(VERIFY_AZAHAR_CONTROLS)

fix-azahar-controls:
	$(VERIFY_AZAHAR_CONTROLS) --fix

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

package-sd: check-package-sd-root verify-sample-decks app-3ds verify-fe-theme-assets
	rm -rf "$(PACKAGE_SDMC)/$(APP_SD_DIR)"
	mkdir -p "$(PACKAGE_SDMC)/$(APP_SD_DIR)"
	mkdir -p "$(PACKAGE_SDMC)/$(FE_THEME_SD_DIR)"
	mkdir -p "$(PACKAGE_SDMC)/$(FE_THEME_SD_DIR)/layers"
	cp app-3ds/anki3ds.3dsx "$(PACKAGE_SDMC)/$(APP_SD_DIR)/anki3ds.3dsx"
	cp app-3ds/anki3ds.smdh "$(PACKAGE_SDMC)/$(APP_SD_DIR)/anki3ds.smdh"
	cp "$(FE_THEME_FRAMEBUFFER_DIR)"/*.bin "$(PACKAGE_SDMC)/$(FE_THEME_SD_DIR)/"
	cp "$(FE_THEME_FRAMEBUFFER_DIR)"/layers/*.bin "$(PACKAGE_SDMC)/$(FE_THEME_SD_DIR)/layers/"
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
	test -f "$$app_dir/fe-themes/fe_font_review_8x14_alpha.bin" || { echo "$$app_dir/fe-themes/fe_font_review_8x14_alpha.bin missing"; exit 1; }; \
	for theme in amber forest ruby chalk; do \
		test -f "$$app_dir/fe-themes/fe_bg_$${theme}_top_400x240_bgr888_fb.bin" || { echo "$$app_dir/fe-themes/fe_bg_$${theme}_top_400x240_bgr888_fb.bin missing"; exit 1; }; \
		test -f "$$app_dir/fe-themes/fe_bg_$${theme}_bottom_320x240_bgr888_fb.bin" || { echo "$$app_dir/fe-themes/fe_bg_$${theme}_bottom_320x240_bgr888_fb.bin missing"; exit 1; }; \
		test -f "$$app_dir/fe-themes/layers/fe_bg_$${theme}_top_layer_legend_400x240_bgr888_fb.bin" || { echo "$$app_dir/fe-themes/layers/fe_bg_$${theme}_top_layer_legend_400x240_bgr888_fb.bin missing"; exit 1; }; \
		test -f "$$app_dir/fe-themes/layers/fe_bg_$${theme}_bottom_layer_legend_320x240_bgr888_fb.bin" || { echo "$$app_dir/fe-themes/layers/fe_bg_$${theme}_bottom_layer_legend_320x240_bgr888_fb.bin missing"; exit 1; }; \
	done; \
	for deck in $(REMOVED_SAMPLE_DECKS); do \
		test ! -e "$$app_dir/decks/$$deck" || { echo "$$app_dir/decks/$$deck must not be packaged"; exit 1; }; \
	done; \
	for deck in $(SAMPLE_DECKS); do \
		$(VERIFY_TEXT_DECK) "$$app_dir/decks/$$deck"; \
	done

install-local-sd: app-3ds install-local-sample-decks install-local-fe-theme-assets
	mkdir -p "$(LOCAL_SDMC)/$(APP_SD_DIR)"
	cp app-3ds/anki3ds.3dsx "$(LOCAL_SDMC)/$(APP_SD_DIR)/anki3ds.3dsx"
	cp app-3ds/anki3ds.smdh "$(LOCAL_SDMC)/$(APP_SD_DIR)/anki3ds.smdh"

install-local-fe-theme-assets: verify-fe-theme-assets
	mkdir -p "$(LOCAL_SDMC)/$(FE_THEME_SD_DIR)"
	mkdir -p "$(LOCAL_SDMC)/$(FE_THEME_SD_DIR)/layers"
	cp "$(FE_THEME_FRAMEBUFFER_DIR)"/*.bin "$(LOCAL_SDMC)/$(FE_THEME_SD_DIR)/"
	cp "$(FE_THEME_FRAMEBUFFER_DIR)"/layers/*.bin "$(LOCAL_SDMC)/$(FE_THEME_SD_DIR)/layers/"

verify-local-sd: prepare-local-samples-fresh
	@set -e; \
	app_dir="$(LOCAL_SDMC)/$(APP_SD_DIR)"; \
	test -f "$$app_dir/anki3ds.3dsx" || { echo "$$app_dir/anki3ds.3dsx missing"; exit 1; }; \
	test -f "$$app_dir/anki3ds.smdh" || { echo "$$app_dir/anki3ds.smdh missing"; exit 1; }; \
	test -f "$$app_dir/fe-themes/fe_font_review_8x14_alpha.bin" || { echo "$$app_dir/fe-themes/fe_font_review_8x14_alpha.bin missing"; exit 1; }; \
	for theme in amber forest ruby chalk; do \
		test -f "$$app_dir/fe-themes/fe_bg_$${theme}_top_400x240_bgr888_fb.bin" || { echo "$$app_dir/fe-themes/fe_bg_$${theme}_top_400x240_bgr888_fb.bin missing"; exit 1; }; \
		test -f "$$app_dir/fe-themes/fe_bg_$${theme}_bottom_320x240_bgr888_fb.bin" || { echo "$$app_dir/fe-themes/fe_bg_$${theme}_bottom_320x240_bgr888_fb.bin missing"; exit 1; }; \
		test -f "$$app_dir/fe-themes/layers/fe_bg_$${theme}_top_layer_legend_400x240_bgr888_fb.bin" || { echo "$$app_dir/fe-themes/layers/fe_bg_$${theme}_top_layer_legend_400x240_bgr888_fb.bin missing"; exit 1; }; \
		test -f "$$app_dir/fe-themes/layers/fe_bg_$${theme}_bottom_layer_legend_320x240_bgr888_fb.bin" || { echo "$$app_dir/fe-themes/layers/fe_bg_$${theme}_bottom_layer_legend_320x240_bgr888_fb.bin missing"; exit 1; }; \
	done; \
	for file in $(APP_SESSION_FILES) $(APP_UI_THEME_FILES); do \
		test ! -e "$$app_dir/$$file" || { echo "$$app_dir/$$file must not carry into a fresh pass"; exit 1; }; \
	done; \
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
	app_dir="$(LOCAL_SDMC)/$(APP_SD_DIR)"; \
	for file in $(APP_SESSION_FILES) $(APP_UI_THEME_FILES); do \
		rm -f "$$app_dir/$$file"; \
	done; \
	for deck in $(SAMPLE_DECKS); do \
		deck_dir="$(LOCAL_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
		rm -f "$$deck_dir/state.tsv" "$$deck_dir/state.tsv.tmp" "$$deck_dir/state.tsv.bak" "$$deck_dir/review-log.tsv" "$$deck_dir/review-log.tsv.tmp" "$$deck_dir/review-log.tsv.bak"; \
	done

prepare-local-samples-fresh: install-local-sd reset-local-sample-progress

install-azahar-app: app-3ds
	mkdir -p "$(AZAHAR_SDMC)/$(APP_SD_DIR)"
	cp app-3ds/anki3ds.3dsx "$(AZAHAR_SDMC)/$(APP_SD_DIR)/anki3ds.3dsx"
	cp app-3ds/anki3ds.smdh "$(AZAHAR_SDMC)/$(APP_SD_DIR)/anki3ds.smdh"

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

install-azahar-fe-theme-assets: verify-fe-theme-assets
	mkdir -p "$(AZAHAR_SDMC)/$(FE_THEME_SD_DIR)"
	mkdir -p "$(AZAHAR_SDMC)/$(FE_THEME_SD_DIR)/layers"
	cp "$(FE_THEME_FRAMEBUFFER_DIR)"/*.bin "$(AZAHAR_SDMC)/$(FE_THEME_SD_DIR)/"
	cp "$(FE_THEME_FRAMEBUFFER_DIR)"/layers/*.bin "$(AZAHAR_SDMC)/$(FE_THEME_SD_DIR)/layers/"

install-azahar-fe-bg-viewer: fe-bg-viewer-3ds
	mkdir -p "$(AZAHAR_SDMC)/3ds/fe-bg-viewer"
	mkdir -p "$(AZAHAR_SDMC)/$(FE_THEME_SD_DIR)"
	mkdir -p "$(AZAHAR_SDMC)/$(FE_THEME_SD_DIR)/layers"
	rm -f "$(AZAHAR_SDMC)/$(FE_THEME_SD_DIR)/layers"/fe_bg_*_layer_*_bgr888_fb.bin
	cp "$(FE_BG_VIEWER_3DSX)" "$(AZAHAR_SDMC)/3ds/fe-bg-viewer/fe-bg-viewer.3dsx"
	cp "$(FE_BG_VIEWER_DIR)/fe-bg-viewer.smdh" "$(AZAHAR_SDMC)/3ds/fe-bg-viewer/fe-bg-viewer.smdh"
	cp "$(FE_THEME_FRAMEBUFFER_DIR)"/*.bin "$(AZAHAR_SDMC)/$(FE_THEME_SD_DIR)/"
	cp "$(FE_THEME_FRAMEBUFFER_DIR)"/layers/*.bin "$(AZAHAR_SDMC)/$(FE_THEME_SD_DIR)/layers/"

install-azahar-fe-ui-theme:
	mkdir -p "$(AZAHAR_SDMC)/$(APP_SD_DIR)"
	printf 'theme\t$(FE_UI_THEME)\nmode\tfe-ui\n' > "$(AZAHAR_SDMC)/$(APP_SD_DIR)/theme.tsv"

clear-azahar-fe-ui-theme:
	set -e; \
	app_dir="$(AZAHAR_SDMC)/$(APP_SD_DIR)"; \
	for file in $(APP_UI_THEME_FILES); do \
		rm -f "$$app_dir/$$file"; \
	done

reset-azahar-sample-progress:
	set -e; \
	app_dir="$(AZAHAR_SDMC)/$(APP_SD_DIR)"; \
	for file in $(APP_SESSION_FILES) $(APP_UI_THEME_FILES); do \
		rm -f "$$app_dir/$$file"; \
	done; \
	for deck in $(SAMPLE_DECKS); do \
		deck_dir="$(AZAHAR_SDMC)/$(SAMPLE_DECK_SD_ROOT)/$$deck"; \
		rm -f "$$deck_dir/state.tsv" "$$deck_dir/state.tsv.tmp" "$$deck_dir/state.tsv.bak" "$$deck_dir/review-log.tsv" "$$deck_dir/review-log.tsv.tmp" "$$deck_dir/review-log.tsv.bak"; \
	done

prepare-azahar-samples-fresh: install-azahar-sample-decks reset-azahar-sample-progress

prepare-azahar-daily-use: install-azahar-app install-azahar-fe-theme-assets verify-azahar-controls verify-azahar-personal-decks

verify-azahar-fresh-samples: prepare-azahar-samples-fresh
	@set -e; \
	app_dir="$(AZAHAR_SDMC)/$(APP_SD_DIR)"; \
	for file in $(APP_SESSION_FILES) $(APP_UI_THEME_FILES); do \
		test ! -e "$$app_dir/$$file" || { echo "$$app_dir/$$file must not carry into a fresh pass"; exit 1; }; \
	done; \
	for deck in $(REMOVED_SAMPLE_DECKS); do \
		test ! -e "$$app_dir/decks/$$deck" || { echo "$$app_dir/decks/$$deck must not be installed"; exit 1; }; \
	done; \
	for deck in $(SAMPLE_DECKS); do \
		$(VERIFY_TEXT_DECK) "$$app_dir/decks/$$deck"; \
	done

check-emulator:
	@test -d "$(AZAHAR_APP)" || \
		(echo "Azahar not found at $(AZAHAR_APP). Set AZAHAR_APP=/path/to/Azahar.app"; exit 1)

drive-azahar-m7-smoke-dry-run:
	$(DRIVE_AZAHAR_M7_SMOKE) --sdmc "$(AZAHAR_SDMC)" --dry-run

check-azahar-smoke-automation:
	$(DRIVE_AZAHAR_M7_SMOKE) --check-automation

drive-azahar-m7-smoke: verify-azahar-controls check-emulator check-azahar-smoke-automation app-3ds
	$(DRIVE_AZAHAR_M7_SMOKE) --azahar-app "$(AZAHAR_APP)" --app-3dsx "$(APP_3DSX)" --sdmc "$(AZAHAR_SDMC)"

run-emulator: app-3ds check-emulator
	open -n -a "$(AZAHAR_APP)" --args --windowed "$(APP_3DSX)"

run-emulator-samples: install-azahar-sample-decks install-azahar-fe-theme-assets run-emulator

run-emulator-fresh-samples: verify-azahar-fresh-samples install-azahar-fe-theme-assets run-emulator

run-emulator-daily-use: prepare-azahar-daily-use run-emulator

run-emulator-m7-smoke: check-azahar-smoke-automation
	$(MAKE) run-emulator-fresh-samples
	$(MAKE) drive-azahar-m7-smoke
	$(MAKE) verify-azahar-m7-smoke-artifacts

run-emulator-fe-ui: install-azahar-sample-decks install-azahar-fe-theme-assets install-azahar-fe-ui-theme run-emulator

run-emulator-fe-bg-viewer: install-azahar-fe-bg-viewer check-emulator
	open -n -a "$(AZAHAR_APP)" --args --windowed "$(FE_BG_VIEWER_3DSX)"
