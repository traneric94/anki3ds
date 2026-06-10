#include "app_settings_flow.h"

#include "app_status_text.h"
#include "study_backend.h"
#include "study_controls.h"
#include "study_session.h"
#include "study_settings.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST_SETTINGS_PATH "/tmp/anki3ds-app-settings-flow.tsv"
#define TEST_SETTINGS_TMP_PATH "/tmp/anki3ds-app-settings-flow.tsv.tmp"
#define TEST_SETTINGS_BAK_PATH "/tmp/anki3ds-app-settings-flow.tsv.bak"

struct settings_flow_state
{
	struct study_backend backend;
	struct study_settings active;
	struct study_settings draft;
	struct study_session session;
	size_t selected_index;
	bool session_dirty;
	enum app_mode mode;
	enum app_mode exit_return_mode;
};

static void remove_file_if_present(const char *path)
{
	if (path != NULL)
		(void)remove(path);
}

static void remove_test_files(void)
{
	remove_file_if_present(TEST_SETTINGS_PATH);
	remove_file_if_present(TEST_SETTINGS_TMP_PATH);
	remove_file_if_present(TEST_SETTINGS_BAK_PATH);
}

static void init_flow_state(struct settings_flow_state *flow)
{
	study_backend_init(&flow->backend, NULL, 0);
	study_settings_defaults(&flow->active);
	flow->draft = flow->active;
	study_session_init(&flow->session, 10, 1);
	flow->selected_index = 0;
	flow->session_dirty = false;
	flow->mode = APP_MODE_SETTINGS;
	flow->exit_return_mode = APP_MODE_REVIEW;
}

static bool apply_flow(
	struct settings_flow_state *flow,
	unsigned int buttons,
	const char *settings_path,
	bool battery_save_warning
)
{
	return app_settings_flow_handle_input(
		buttons,
		&flow->backend,
		&flow->active,
		&flow->draft,
		settings_path,
		&flow->selected_index,
		&flow->session,
		"limits-demo",
		&flow->session_dirty,
		&flow->mode,
		&flow->exit_return_mode,
		battery_save_warning,
		1234,
		7
	);
}

static void test_updated_field_changes_draft_and_status(void)
{
	struct settings_flow_state flow;

	init_flow_state(&flow);
	study_backend_set_status(&flow.backend, "Settings ignored");

	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_RIGHT, TEST_SETTINGS_PATH, false));
	assert(flow.draft.new_limit == 50);
	assert(flow.draft.review_limit == 200);
	assert(flow.selected_index == 0);
	assert(flow.mode == APP_MODE_SETTINGS);
	assert(strcmp(flow.backend.status_text, "unsaved changes; Settings ignored") == 0);
	assert(!flow.session_dirty);
}

static void test_cancel_returns_review_with_discard_status(void)
{
	struct settings_flow_state flow;

	init_flow_state(&flow);
	flow.draft.review_limit = 10;
	study_backend_set_status(&flow.backend, "Settings ignored");

	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_B, TEST_SETTINGS_PATH, false));
	assert(flow.mode == APP_MODE_REVIEW);
	assert(strcmp(flow.backend.status_text, "Settings canceled; discarded; Settings ignored") == 0);
	assert(!flow.session_dirty);
}

static void test_cancel_without_changes_uses_plain_cancel_status(void)
{
	struct settings_flow_state flow;

	init_flow_state(&flow);
	study_backend_set_status(&flow.backend, "Settings ignored");

	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_SELECT, TEST_SETTINGS_PATH, false));
	assert(flow.mode == APP_MODE_REVIEW);
	assert(strcmp(flow.backend.status_text, "Settings canceled; Settings ignored") == 0);
	assert(!flow.session_dirty);
}

static void test_exit_with_unsaved_changes_opens_exit_confirmation(void)
{
	struct settings_flow_state flow;

	init_flow_state(&flow);
	flow.draft.new_limit = 5;
	study_backend_set_status(&flow.backend, "Settings ignored");

	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_START, TEST_SETTINGS_PATH, false));
	assert(flow.mode == APP_MODE_CONFIRM_EXIT);
	assert(flow.exit_return_mode == APP_MODE_SETTINGS);
	assert(strcmp(flow.backend.status_text, "Exit loses unsaved settings; Settings ignored") == 0);
	assert(!flow.session_dirty);
}

static void test_exit_without_changes_leaves_status_and_opens_exit_confirmation(void)
{
	struct settings_flow_state flow;

	init_flow_state(&flow);
	study_backend_set_status(&flow.backend, "Settings ignored");

	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_START, TEST_SETTINGS_PATH, false));
	assert(flow.mode == APP_MODE_CONFIRM_EXIT);
	assert(flow.exit_return_mode == APP_MODE_SETTINGS);
	assert(strcmp(flow.backend.status_text, "Settings ignored") == 0);
	assert(!flow.session_dirty);
}

static void test_save_updates_settings_file_session_and_status(void)
{
	struct settings_flow_state flow;
	struct study_settings loaded;

	remove_test_files();
	init_flow_state(&flow);
	flow.draft.new_limit = 5;
	flow.draft.review_limit = 10;
	flow.draft.learning_mode = STUDY_SETTINGS_LEARNING_CARD_COOLDOWN;
	study_backend_set_status(&flow.backend, "Settings ignored");

	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_X, TEST_SETTINGS_PATH, true));
	assert(flow.mode == APP_MODE_REVIEW);
	assert(flow.active.new_limit == 5);
	assert(flow.active.review_limit == 10);
	assert(flow.active.learning_mode == STUDY_SETTINGS_LEARNING_CARD_COOLDOWN);
	assert(study_settings_load_tsv(&loaded, TEST_SETTINGS_PATH) == STUDY_SETTINGS_OK);
	assert(loaded.new_limit == 5);
	assert(loaded.review_limit == 10);
	assert(loaded.learning_mode == STUDY_SETTINGS_LEARNING_CARD_COOLDOWN);
	assert(
		flow.backend.scheduler_policy ==
		STUDY_BACKEND_SCHEDULER_CARD_COUNT_COOLDOWN
	);
	assert(strcmp(flow.backend.status_text, "Settings saved; Settings ignored; batt low") == 0);
	assert(flow.session_dirty);
	assert(flow.session.settings_saved_count == 1);
	assert(flow.session.updated_at == 1234);
	assert(flow.session.current_day == 7);
	assert(strcmp(flow.session.last_deck_id, "limits-demo") == 0);
	assert(flow.session.last_event == STUDY_SESSION_EVENT_SETTINGS_SAVED);
	remove_test_files();
}

static void test_save_failure_stays_in_settings_with_draft_for_retry(void)
{
	struct settings_flow_state flow;

	init_flow_state(&flow);
	flow.draft.new_limit = 5;
	flow.draft.review_limit = 10;
	study_backend_set_status(&flow.backend, "Settings ignored");

	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_X, NULL, true));
	assert(flow.mode == APP_MODE_SETTINGS);
	assert(flow.active.new_limit == 20);
	assert(flow.active.review_limit == 200);
	assert(flow.draft.new_limit == 5);
	assert(flow.draft.review_limit == 10);
	assert(strcmp(flow.backend.status_text, "Settings save failed") == 0);
	assert(!flow.session_dirty);
	assert(flow.session.settings_saved_count == 0);
	assert(!app_status_text_has_low_battery_suffix(flow.backend.status_text));
}

int main(void)
{
	test_updated_field_changes_draft_and_status();
	test_cancel_returns_review_with_discard_status();
	test_cancel_without_changes_uses_plain_cancel_status();
	test_exit_with_unsaved_changes_opens_exit_confirmation();
	test_exit_without_changes_leaves_status_and_opens_exit_confirmation();
	test_save_updates_settings_file_session_and_status();
	test_save_failure_stays_in_settings_with_draft_for_retry();
	return 0;
}
