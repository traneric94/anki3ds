#include "app_day_rollover_flow.h"

#include "app_status_text.h"
#include "study_backend.h"
#include "study_session.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST_STATE_PATH "/tmp/anki3ds-app-day-rollover-state.tsv"
#define TEST_STATE_TMP_PATH "/tmp/anki3ds-app-day-rollover-state.tsv.tmp"
#define TEST_STATE_BAK_PATH "/tmp/anki3ds-app-day-rollover-state.tsv.bak"
#define TEST_LOG_PATH "/tmp/anki3ds-app-day-rollover-review-log.tsv"
#define TEST_LOG_TMP_PATH "/tmp/anki3ds-app-day-rollover-review-log.tsv.tmp"
#define TEST_LOG_BAK_PATH "/tmp/anki3ds-app-day-rollover-review-log.tsv.bak"

static const struct study_backend_card cards[] = {
	{ "Front 1", "Back 1", NULL },
	{ "Front 2", "Back 2", NULL },
};

static void remove_file_if_present(const char *path)
{
	if (path != NULL)
		(void)remove(path);
}

static void remove_test_files(void)
{
	remove_file_if_present(TEST_STATE_PATH);
	remove_file_if_present(TEST_STATE_TMP_PATH);
	remove_file_if_present(TEST_STATE_BAK_PATH);
	remove_file_if_present(TEST_LOG_PATH);
	remove_file_if_present(TEST_LOG_TMP_PATH);
	remove_file_if_present(TEST_LOG_BAK_PATH);
}

static bool file_exists(const char *path)
{
	return access(path, F_OK) == 0;
}

static void make_progress_on_day(
	struct study_backend *backend,
	unsigned int day
)
{
	assert(!study_backend_rollover_day(backend, day));
	assert(study_backend_show_answer(backend));
	assert(study_backend_rate_current(backend, STUDY_BACKEND_RATING_GOOD));
	assert(backend->progress_day == day);
	assert(backend->reviewed_today_count == 1);
	assert(backend->completed_today_count == 1);
}

static bool apply_rollover(
	struct study_backend *backend,
	bool state_enabled,
	const char *state_path,
	struct study_session *session,
	bool *session_dirty,
	unsigned int *current_day,
	unsigned int observed_day,
	bool battery_save_warning
)
{
	return app_day_rollover_flow_apply(
		backend,
		state_enabled,
		state_path,
		TEST_LOG_PATH,
		session,
		"sample",
		session_dirty,
		current_day,
		2222,
		observed_day,
		battery_save_warning
	);
}

static bool apply_rollover_with_outcome(
	struct study_backend *backend,
	bool state_enabled,
	const char *state_path,
	struct study_session *session,
	bool *session_dirty,
	unsigned int *current_day,
	unsigned int observed_day,
	bool battery_save_warning,
	enum app_state_save_outcome *save_outcome
)
{
	return app_day_rollover_flow_apply_with_outcome(
		backend,
		state_enabled,
		state_path,
		TEST_LOG_PATH,
		session,
		"sample",
		session_dirty,
		current_day,
		2222,
		observed_day,
		battery_save_warning,
		save_outcome
	);
}

static void test_same_day_noops_without_clearing_existing_dirty_flags(void)
{
	struct study_backend backend;
	struct study_session session;
	unsigned int current_day = 1;
	bool session_dirty = true;
	enum app_state_save_outcome save_outcome = APP_STATE_SAVE_OUTCOME_SAVED;

	remove_test_files();
	study_backend_init(&backend, cards, 2);
	study_backend_set_status(&backend, "Ready");
	study_session_init(&session, 100, 1);
	assert(!study_backend_rollover_day(&backend, 1));

	assert(
		!apply_rollover_with_outcome(
			&backend,
			true,
			TEST_STATE_PATH,
			&session,
			&session_dirty,
			&current_day,
			1,
			true,
			&save_outcome
		)
	);
	assert(save_outcome == APP_STATE_SAVE_OUTCOME_NONE);
	assert(current_day == 1);
	assert(session.current_day == 1);
	assert(session_dirty);
	assert(strcmp(backend.status_text, "Ready") == 0);
	assert(!file_exists(TEST_STATE_PATH));
	remove_test_files();
}

static void test_session_rollover_without_backend_progress_marks_session_only(void)
{
	struct study_backend backend;
	struct study_session session;
	unsigned int current_day = 1;
	bool session_dirty = false;

	remove_test_files();
	study_backend_init(&backend, cards, 2);
	study_session_init(&session, 100, 1);

	assert(
		!apply_rollover(
			&backend,
			true,
			TEST_STATE_PATH,
			&session,
			&session_dirty,
			&current_day,
			2,
			true
		)
	);
	assert(current_day == 2);
	assert(session_dirty);
	assert(session.current_day == 2);
	assert(session.updated_at == 2222);
	assert(backend.progress_day == 2);
	assert(!file_exists(TEST_STATE_PATH));
	remove_test_files();
}

static void test_backend_progress_rollover_saves_state_and_requests_redraw(void)
{
	struct study_backend backend;
	struct study_backend restored;
	struct study_session session;
	unsigned int current_day = 1;
	bool session_dirty = false;

	remove_test_files();
	study_backend_init(&backend, cards, 2);
	make_progress_on_day(&backend, 1);
	study_session_init(&session, 100, 1);

	assert(
		apply_rollover(
			&backend,
			true,
			TEST_STATE_PATH,
			&session,
			&session_dirty,
			&current_day,
			2,
			true
		)
	);
	assert(current_day == 2);
	assert(session_dirty);
	assert(session.current_day == 2);
	assert(file_exists(TEST_STATE_PATH));
	assert(backend.progress_day == 2);
	assert(backend.reviewed_today_count == 0);
	assert(backend.completed_today_count == 0);
	assert(strcmp(backend.status_text, "New day; limits reset; batt low") == 0);
	assert(app_status_text_has_low_battery_suffix(backend.status_text));

	study_backend_init(&restored, cards, 2);
	assert(
		study_backend_load_state_tsv(
			&restored,
			TEST_STATE_PATH
		) == STUDY_BACKEND_STATE_OK
	);
	assert(restored.progress_day == 2);
	assert(restored.reviewed_today_count == 0);
	assert(restored.completed_today_count == 0);
	remove_test_files();
}

static void test_backend_rollover_without_state_enabled_keeps_status(void)
{
	struct study_backend backend;
	struct study_session session;
	unsigned int current_day = 1;
	bool session_dirty = false;

	remove_test_files();
	study_backend_init(&backend, cards, 2);
	make_progress_on_day(&backend, 1);
	study_session_init(&session, 100, 1);

	assert(
		apply_rollover(
			&backend,
			false,
			TEST_STATE_PATH,
			&session,
			&session_dirty,
			&current_day,
			2,
			true
		)
	);
	assert(current_day == 2);
	assert(session_dirty);
	assert(strcmp(backend.status_text, "New day; limits reset") == 0);
	assert(!file_exists(TEST_STATE_PATH));
	remove_test_files();
}

static void test_backend_rollover_state_save_failure_reports_status(void)
{
	struct study_backend backend;
	struct study_session session;
	unsigned int current_day = 1;
	bool session_dirty = false;
	enum app_state_save_outcome save_outcome = APP_STATE_SAVE_OUTCOME_NONE;

	remove_test_files();
	study_backend_init(&backend, cards, 2);
	make_progress_on_day(&backend, 1);
	study_session_init(&session, 100, 1);

	assert(
		apply_rollover_with_outcome(
			&backend,
			true,
			NULL,
			&session,
			&session_dirty,
			&current_day,
			2,
			true,
			&save_outcome
		)
	);
	assert(save_outcome == APP_STATE_SAVE_OUTCOME_WRITE_FAILED);
	assert(current_day == 1);
	assert(!session_dirty);
	assert(session.current_day == 1);
	assert(backend.progress_day == 1);
	assert(backend.reviewed_today_count == 1);
	assert(backend.completed_today_count == 1);
	assert(strcmp(backend.status_text, "Save failed") == 0);
	assert(!app_status_text_has_low_battery_suffix(backend.status_text));
	remove_test_files();
}

int main(void)
{
	test_same_day_noops_without_clearing_existing_dirty_flags();
	test_session_rollover_without_backend_progress_marks_session_only();
	test_backend_progress_rollover_saves_state_and_requests_redraw();
	test_backend_rollover_without_state_enabled_keeps_status();
	test_backend_rollover_state_save_failure_reports_status();
	return 0;
}
