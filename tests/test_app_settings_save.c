#include "app_settings_save.h"

#include "app_status_text.h"
#include "study_backend.h"
#include "study_session.h"
#include "study_settings.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST_SETTINGS_PATH "/tmp/anki3ds-app-settings-save.tsv"
#define TEST_SETTINGS_TMP_PATH "/tmp/anki3ds-app-settings-save.tsv.tmp"
#define TEST_SETTINGS_BAK_PATH "/tmp/anki3ds-app-settings-save.tsv.bak"

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

static void test_save_updates_active_status_session_and_file(void)
{
	struct study_backend backend;
	struct study_settings active;
	struct study_settings draft;
	struct study_settings loaded;
	struct study_session session;
	bool session_dirty = false;

	remove_test_files();
	study_backend_init(&backend, NULL, 0);
	study_backend_set_status(&backend, "Settings ignored");
	study_settings_defaults(&active);
	active.new_limit = 20;
	active.review_limit = 200;
	study_settings_defaults(&draft);
	draft.new_limit = 5;
	draft.review_limit = 10;
	draft.learning_mode = STUDY_SETTINGS_LEARNING_CARD_COOLDOWN;
	study_session_init(&session, 100, 1);

	assert(
		app_settings_save_apply(
			&backend,
			&active,
			&draft,
			TEST_SETTINGS_PATH,
			&session,
			"limits-demo",
			&session_dirty,
			true,
			1234,
			7
		)
	);
	assert(active.new_limit == 5);
	assert(active.review_limit == 10);
	assert(active.learning_mode == STUDY_SETTINGS_LEARNING_CARD_COOLDOWN);
	assert(study_settings_load_tsv(&loaded, TEST_SETTINGS_PATH) == STUDY_SETTINGS_OK);
	assert(loaded.new_limit == 5);
	assert(loaded.review_limit == 10);
	assert(loaded.learning_mode == STUDY_SETTINGS_LEARNING_CARD_COOLDOWN);
	assert(
		backend.scheduler_policy ==
		STUDY_BACKEND_SCHEDULER_CARD_COUNT_COOLDOWN
	);
	assert(strcmp(backend.status_text, "Settings saved; Settings ignored; batt low") == 0);
	assert(session_dirty);
	assert(session.settings_saved_count == 1);
	assert(session.updated_at == 1234);
	assert(session.current_day == 7);
	assert(strcmp(session.last_deck_id, "limits-demo") == 0);
	assert(session.last_event == STUDY_SESSION_EVENT_SETTINGS_SAVED);
	remove_test_files();
}

static void test_save_without_session_still_updates_settings_and_status(void)
{
	struct study_backend backend;
	struct study_settings active;
	struct study_settings draft;
	bool session_dirty = false;

	remove_test_files();
	study_backend_init(&backend, NULL, 0);
	study_backend_set_status(&backend, "Ready");
	study_settings_defaults(&active);
	active.new_limit = 20;
	active.review_limit = 200;
	study_settings_defaults(&draft);
	draft.new_limit = 50;
	draft.review_limit = 100;

	assert(
		app_settings_save_apply(
			&backend,
			&active,
			&draft,
			TEST_SETTINGS_PATH,
			NULL,
			"sample",
			&session_dirty,
			false,
			1234,
			7
		)
	);
	assert(active.new_limit == 50);
	assert(active.review_limit == 100);
	assert(strcmp(backend.status_text, "Settings saved") == 0);
	assert(!session_dirty);
	remove_test_files();
}

static void test_save_failure_keeps_active_and_reports_error(void)
{
	struct study_backend backend;
	struct study_settings active;
	struct study_settings draft;
	struct study_session session;
	bool session_dirty = false;

	study_backend_init(&backend, NULL, 0);
	study_backend_set_status(&backend, "Settings ignored");
	study_settings_defaults(&active);
	active.new_limit = 20;
	active.review_limit = 200;
	study_settings_defaults(&draft);
	draft.new_limit = 5;
	draft.review_limit = 10;
	study_session_init(&session, 100, 1);

	assert(
		!app_settings_save_apply(
			&backend,
			&active,
			&draft,
			NULL,
			&session,
			"limits-demo",
			&session_dirty,
			true,
			1234,
			7
		)
	);
	assert(active.new_limit == 20);
	assert(active.review_limit == 200);
	assert(strcmp(backend.status_text, "Settings save failed") == 0);
	assert(!session_dirty);
	assert(session.settings_saved_count == 0);
	assert(!app_status_text_has_low_battery_suffix(backend.status_text));
}

int main(void)
{
	test_save_updates_active_status_session_and_file();
	test_save_without_session_still_updates_settings_and_status();
	test_save_failure_keeps_active_and_reports_error();
	return 0;
}
