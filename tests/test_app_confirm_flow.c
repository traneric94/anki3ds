#include "app_confirm_flow.h"

#include "app_status_text.h"
#include "study_backend.h"
#include "study_controls.h"
#include "study_session.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST_STATE_PATH "/tmp/anki3ds-app-confirm-flow-state.tsv"
#define TEST_STATE_TMP_PATH "/tmp/anki3ds-app-confirm-flow-state.tsv.tmp"
#define TEST_STATE_BAK_PATH "/tmp/anki3ds-app-confirm-flow-state.tsv.bak"
#define TEST_LOG_PATH "/tmp/anki3ds-app-confirm-flow-review-log.tsv"
#define TEST_LOG_TMP_PATH "/tmp/anki3ds-app-confirm-flow-review-log.tsv.tmp"
#define TEST_LOG_BAK_PATH "/tmp/anki3ds-app-confirm-flow-review-log.tsv.bak"

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

static void write_test_file(const char *path, const char *contents)
{
	FILE *file = fopen(path, "w");

	assert(file != NULL);
	assert(fputs(contents, file) >= 0);
	assert(fclose(file) == 0);
}

static bool file_exists(const char *path)
{
	return access(path, F_OK) == 0;
}

static void make_dirty_progress(struct study_backend *backend)
{
	assert(study_backend_show_answer(backend));
	assert(study_backend_rate_current(backend, STUDY_BACKEND_RATING_GOOD));
	assert(study_backend_show_answer(backend));
	assert(study_backend_suspend_current(backend));
}

static void test_start_opens_exit_confirmation_from_nested_confirm(void)
{
	struct study_backend backend;
	enum app_mode mode = APP_MODE_CONFIRM_RESET;
	enum app_mode exit_return_mode = APP_MODE_REVIEW;
	bool session_dirty = true;
	bool state_dirty = true;
	bool log_dirty = true;
	bool exit_requested = true;
	bool review_scroll_reset = true;
	enum study_review_log_event log_event = STUDY_REVIEW_LOG_EVENT_RESTORE;

	study_backend_init(&backend, cards, 2);
	study_backend_set_status(&backend, "Settings ignored");

	assert(app_confirm_flow_handle_input(
		&mode,
		&exit_return_mode,
		STUDY_CONTROL_BUTTON_START,
		&backend,
		false,
		NULL,
		NULL,
		NULL,
		"alpha",
		&session_dirty,
		&state_dirty,
		&log_dirty,
		&log_event,
		&exit_requested,
		&review_scroll_reset,
		false,
		100,
		1
	));
	assert(mode == APP_MODE_CONFIRM_EXIT);
	assert(exit_return_mode == APP_MODE_CONFIRM_RESET);
	assert(!state_dirty);
	assert(!log_dirty);
	assert(session_dirty);
	assert(!exit_requested);
	assert(!review_scroll_reset);
	assert(log_event == STUDY_REVIEW_LOG_EVENT_RATING);
	assert(strcmp(backend.status_text, "Settings ignored") == 0);
}

static void test_cancel_exit_returns_to_saved_mode_with_warning_context(void)
{
	struct study_backend backend;
	enum app_mode mode = APP_MODE_CONFIRM_EXIT;
	enum app_mode exit_return_mode = APP_MODE_SETTINGS;
	bool session_dirty = false;
	bool state_dirty = false;
	bool log_dirty = false;
	bool exit_requested = false;
	bool review_scroll_reset = true;
	enum study_review_log_event log_event = STUDY_REVIEW_LOG_EVENT_RATING;

	study_backend_init(&backend, cards, 2);
	study_backend_set_status(&backend, "Settings ignored");

	assert(app_confirm_flow_handle_input(
		&mode,
		&exit_return_mode,
		STUDY_CONTROL_BUTTON_B,
		&backend,
		false,
		NULL,
		NULL,
		NULL,
		"alpha",
		&session_dirty,
		&state_dirty,
		&log_dirty,
		&log_event,
		&exit_requested,
		&review_scroll_reset,
		false,
		100,
		1
	));
	assert(mode == APP_MODE_SETTINGS);
	assert(strcmp(backend.status_text, "Exit canceled; Settings ignored") == 0);
	assert(!session_dirty);
	assert(!state_dirty);
	assert(!log_dirty);
	assert(!exit_requested);
	assert(!review_scroll_reset);
	assert(log_event == STUDY_REVIEW_LOG_EVENT_RATING);
}

static void test_confirm_exit_records_terminal_session_event(void)
{
	struct study_backend backend;
	struct study_session session;
	enum app_mode mode = APP_MODE_CONFIRM_EXIT;
	enum app_mode exit_return_mode = APP_MODE_DECK_SELECT;
	bool session_dirty = false;
	bool state_dirty = false;
	bool log_dirty = false;
	bool exit_requested = false;
	bool review_scroll_reset = false;
	enum study_review_log_event log_event = STUDY_REVIEW_LOG_EVENT_RATING;

	study_backend_init(&backend, cards, 2);
	study_session_init(&session, 10, 1);

	assert(app_confirm_flow_handle_input(
		&mode,
		&exit_return_mode,
		STUDY_CONTROL_BUTTON_A,
		&backend,
		false,
		NULL,
		NULL,
		&session,
		"",
		&session_dirty,
		&state_dirty,
		&log_dirty,
		&log_event,
		&exit_requested,
		&review_scroll_reset,
		false,
		200,
		2
	));
	assert(mode == APP_MODE_REVIEW);
	assert(session_dirty);
	assert(exit_requested);
	assert(review_scroll_reset);
	assert(session.exit_confirmed);
	assert(session.updated_at == 200);
	assert(session.current_day == 2);
	assert(strcmp(session.last_deck_id, "-") == 0);
	assert(session.last_event == STUDY_SESSION_EVENT_EXIT_CONFIRMED);
	assert(!state_dirty);
	assert(!log_dirty);
}

static void test_confirm_suspend_marks_state_and_log_dirty(void)
{
	struct study_backend backend;
	struct study_backend_view view;
	enum app_mode mode = APP_MODE_CONFIRM_SUSPEND;
	enum app_mode exit_return_mode = APP_MODE_REVIEW;
	bool session_dirty = false;
	bool state_dirty = false;
	bool log_dirty = false;
	bool exit_requested = false;
	bool review_scroll_reset = false;
	enum study_review_log_event log_event = STUDY_REVIEW_LOG_EVENT_RATING;

	study_backend_init(&backend, cards, 2);
	study_backend_set_status(&backend, "Settings ignored");

	assert(app_confirm_flow_handle_input(
		&mode,
		&exit_return_mode,
		STUDY_CONTROL_BUTTON_X,
		&backend,
		false,
		NULL,
		NULL,
		NULL,
		"alpha",
		&session_dirty,
		&state_dirty,
		&log_dirty,
		&log_event,
		&exit_requested,
		&review_scroll_reset,
		false,
		300,
		3
	));
	study_backend_build_view(&backend, &view);
	assert(mode == APP_MODE_REVIEW);
	assert(state_dirty);
	assert(log_dirty);
	assert(log_event == STUDY_REVIEW_LOG_EVENT_SUSPEND);
	assert(!session_dirty);
	assert(!exit_requested);
	assert(review_scroll_reset);
	assert(view.suspended_count == 1);
	assert(strstr(view.status_text, "Suspended") != NULL);
	assert(strstr(view.status_text, "Settings ignored") != NULL);
}

static void test_confirm_restore_marks_state_and_log_dirty(void)
{
	struct study_backend backend;
	struct study_backend_view view;
	enum app_mode mode = APP_MODE_CONFIRM_RESTORE;
	enum app_mode exit_return_mode = APP_MODE_REVIEW;
	bool session_dirty = false;
	bool state_dirty = false;
	bool log_dirty = false;
	bool exit_requested = false;
	bool review_scroll_reset = false;
	enum study_review_log_event log_event = STUDY_REVIEW_LOG_EVENT_RATING;

	study_backend_init(&backend, cards, 2);
	assert(study_backend_suspend_current(&backend));
	study_backend_set_status(&backend, "Settings ignored");

	assert(app_confirm_flow_handle_input(
		&mode,
		&exit_return_mode,
		STUDY_CONTROL_BUTTON_X,
		&backend,
		false,
		NULL,
		NULL,
		NULL,
		"alpha",
		&session_dirty,
		&state_dirty,
		&log_dirty,
		&log_event,
		&exit_requested,
		&review_scroll_reset,
		false,
		400,
		4
	));
	study_backend_build_view(&backend, &view);
	assert(mode == APP_MODE_REVIEW);
	assert(state_dirty);
	assert(log_dirty);
	assert(log_event == STUDY_REVIEW_LOG_EVENT_RESTORE);
	assert(!session_dirty);
	assert(!exit_requested);
	assert(review_scroll_reset);
	assert(view.suspended_count == 0);
	assert(strstr(view.status_text, "Restored 1 suspended") != NULL);
	assert(strstr(view.status_text, "Settings ignored") != NULL);
}

static void test_confirm_reset_deletes_artifacts_and_records_session(void)
{
	struct study_backend backend;
	struct study_backend_view view;
	struct study_session session;
	enum app_mode mode = APP_MODE_CONFIRM_RESET;
	enum app_mode exit_return_mode = APP_MODE_REVIEW;
	bool session_dirty = false;
	bool state_dirty = true;
	bool log_dirty = true;
	bool exit_requested = false;
	bool review_scroll_reset = false;
	enum study_review_log_event log_event = STUDY_REVIEW_LOG_EVENT_RESTORE;

	remove_test_files();
	study_backend_init(&backend, cards, 2);
	make_dirty_progress(&backend);
	study_backend_set_status(&backend, "Settings ignored");
	study_session_init(&session, 10, 1);
	write_test_file(TEST_STATE_PATH, "version\t1\n");
	write_test_file(TEST_STATE_TMP_PATH, "version\t1\n");
	write_test_file(TEST_STATE_BAK_PATH, "version\t1\n");
	write_test_file(TEST_LOG_PATH, "row\n");
	write_test_file(TEST_LOG_TMP_PATH, "tmp\n");
	write_test_file(TEST_LOG_BAK_PATH, "bak\n");

	assert(app_confirm_flow_handle_input(
		&mode,
		&exit_return_mode,
		STUDY_CONTROL_BUTTON_X,
		&backend,
		true,
		TEST_STATE_PATH,
		TEST_LOG_PATH,
		&session,
		"alpha",
		&session_dirty,
		&state_dirty,
		&log_dirty,
		&log_event,
		&exit_requested,
		&review_scroll_reset,
		true,
		500,
		5
	));
	study_backend_build_view(&backend, &view);
	assert(mode == APP_MODE_REVIEW);
	assert(session_dirty);
	assert(!state_dirty);
	assert(!log_dirty);
	assert(log_event == STUDY_REVIEW_LOG_EVENT_RATING);
	assert(!exit_requested);
	assert(review_scroll_reset);
	assert(session.reset_progress_count == 1);
	assert(session.updated_at == 500);
	assert(session.current_day == 5);
	assert(strcmp(session.last_deck_id, "alpha") == 0);
	assert(session.last_event == STUDY_SESSION_EVENT_RESET_PROGRESS);
	assert(view.reviewed_count == 0);
	assert(view.suspended_count == 0);
	assert(strstr(view.status_text, "Progress reset") != NULL);
	assert(strstr(view.status_text, "Settings ignored") != NULL);
	assert(app_status_text_has_low_battery_suffix(view.status_text));
	assert(!file_exists(TEST_STATE_PATH));
	assert(!file_exists(TEST_STATE_TMP_PATH));
	assert(!file_exists(TEST_STATE_BAK_PATH));
	assert(!file_exists(TEST_LOG_PATH));
	assert(!file_exists(TEST_LOG_TMP_PATH));
	assert(!file_exists(TEST_LOG_BAK_PATH));
	remove_test_files();
}

static void test_confirm_reset_records_session_when_only_log_cleanup_fails(void)
{
	struct study_backend backend;
	struct study_backend_view view;
	struct study_session session;
	enum app_mode mode = APP_MODE_CONFIRM_RESET;
	enum app_mode exit_return_mode = APP_MODE_REVIEW;
	bool session_dirty = false;
	bool state_dirty = true;
	bool log_dirty = true;
	bool exit_requested = false;
	bool review_scroll_reset = false;
	enum study_review_log_event log_event = STUDY_REVIEW_LOG_EVENT_RESTORE;
	char long_log_path[640];

	remove_test_files();
	memset(long_log_path, 'x', sizeof(long_log_path) - 1);
	long_log_path[sizeof(long_log_path) - 1] = '\0';
	study_backend_init(&backend, cards, 2);
	make_dirty_progress(&backend);
	study_backend_set_status(&backend, "Settings ignored");
	study_session_init(&session, 10, 1);
	write_test_file(TEST_STATE_PATH, "version\t1\n");

	assert(app_confirm_flow_handle_input(
		&mode,
		&exit_return_mode,
		STUDY_CONTROL_BUTTON_X,
		&backend,
		true,
		TEST_STATE_PATH,
		long_log_path,
		&session,
		"alpha",
		&session_dirty,
		&state_dirty,
		&log_dirty,
		&log_event,
		&exit_requested,
		&review_scroll_reset,
		true,
		500,
		5
	));
	study_backend_build_view(&backend, &view);
	assert(mode == APP_MODE_REVIEW);
	assert(session_dirty);
	assert(!state_dirty);
	assert(!log_dirty);
	assert(!exit_requested);
	assert(review_scroll_reset);
	assert(log_event == STUDY_REVIEW_LOG_EVENT_RATING);
	assert(session.reset_progress_count == 1);
	assert(session.last_event == STUDY_SESSION_EVENT_RESET_PROGRESS);
	assert(view.reviewed_count == 0);
	assert(view.suspended_count == 0);
	assert(strcmp(view.status_text, "Progress reset; log kept; Settings ignored; batt low") == 0);
	assert(!file_exists(TEST_STATE_PATH));
	remove_test_files();
}

int main(void)
{
	test_start_opens_exit_confirmation_from_nested_confirm();
	test_cancel_exit_returns_to_saved_mode_with_warning_context();
	test_confirm_exit_records_terminal_session_event();
	test_confirm_suspend_marks_state_and_log_dirty();
	test_confirm_restore_marks_state_and_log_dirty();
	test_confirm_reset_deletes_artifacts_and_records_session();
	test_confirm_reset_records_session_when_only_log_cleanup_fails();
	return 0;
}
