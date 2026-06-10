#include "app_state_save.h"

#include "app_status_text.h"
#include "study_backend.h"
#include "study_session.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST_STATE_PATH "/tmp/anki3ds-app-state-save-state.tsv"
#define TEST_STATE_TMP_PATH "/tmp/anki3ds-app-state-save-state.tsv.tmp"
#define TEST_STATE_BAK_PATH "/tmp/anki3ds-app-state-save-state.tsv.bak"
#define TEST_LOG_PATH "/tmp/anki3ds-app-state-save-review-log.tsv"
#define TEST_LOG_TMP_PATH "/tmp/anki3ds-app-state-save-review-log.tsv.tmp"
#define TEST_LOG_BAK_PATH "/tmp/anki3ds-app-state-save-review-log.tsv.bak"

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

static void read_file(
	const char *path,
	char *destination,
	size_t destination_size
)
{
	FILE *file = fopen(path, "rb");
	size_t bytes_read;

	assert(file != NULL);
	bytes_read = fread(destination, 1, destination_size - 1, file);
	assert(!ferror(file));
	destination[bytes_read] = '\0';
	assert(fclose(file) == 0);
}

static bool file_exists(const char *path)
{
	return access(path, F_OK) == 0;
}

static void make_good_rating(struct study_backend *backend)
{
	assert(study_backend_show_answer(backend));
	assert(study_backend_rate_current(backend, STUDY_BACKEND_RATING_GOOD));
}

static void test_noop_when_not_dirty_or_state_disabled(void)
{
	struct study_backend backend;
	struct study_session session;
	bool session_dirty = true;

	remove_test_files();
	study_backend_init(&backend, cards, 2);
	study_backend_set_status(&backend, "Ready");
	study_session_init(&session, 100, 1);

	assert(
		!app_state_save_if_dirty(
			&backend,
			false,
			true,
			TEST_STATE_PATH,
			TEST_LOG_PATH,
			true,
			STUDY_REVIEW_LOG_EVENT_RATING,
			STUDY_REVIEW_LOG_RATING_GOOD,
			&session,
			"sample",
			&session_dirty,
			true,
			1234,
			7
		)
	);
	assert(strcmp(backend.status_text, "Ready") == 0);
	assert(session_dirty);
	assert(!file_exists(TEST_STATE_PATH));

	session_dirty = false;
	assert(
		!app_state_save_if_dirty(
			&backend,
			true,
			false,
			TEST_STATE_PATH,
			TEST_LOG_PATH,
			true,
			STUDY_REVIEW_LOG_EVENT_RATING,
			STUDY_REVIEW_LOG_RATING_GOOD,
			&session,
			"sample",
			&session_dirty,
			true,
			1234,
			7
		)
	);
	assert(!session_dirty);
	assert(!file_exists(TEST_STATE_PATH));
	remove_test_files();
}

static void test_save_rating_writes_state_log_and_session(void)
{
	struct study_backend backend;
	struct study_session session;
	char log_text[256];
	bool session_dirty = false;
	enum app_state_save_outcome outcome = APP_STATE_SAVE_OUTCOME_NONE;

	remove_test_files();
	study_backend_init(&backend, cards, 2);
	make_good_rating(&backend);
	study_session_init(&session, 100, 1);

	assert(
		app_state_save_if_dirty_with_outcome(
			&backend,
			true,
			true,
			TEST_STATE_PATH,
			TEST_LOG_PATH,
			true,
			STUDY_REVIEW_LOG_EVENT_RATING,
			STUDY_REVIEW_LOG_RATING_GOOD,
			&session,
			"sample",
			&session_dirty,
			false,
			1234,
			7,
			&outcome
		)
	);
	read_file(TEST_LOG_PATH, log_text, sizeof(log_text));

	assert(outcome == APP_STATE_SAVE_OUTCOME_SAVED);
	assert(file_exists(TEST_STATE_PATH));
	assert(strcmp(log_text, "1234\trating\tgood\t1\t1\t0\n") == 0);
	assert(session_dirty);
	assert(session.rating_saved_count == 1);
	assert(session.updated_at == 1234);
	assert(session.current_day == 7);
	assert(strcmp(session.last_deck_id, "sample") == 0);
	assert(session.last_event == STUDY_SESSION_EVENT_RATING_SAVED);
	assert(!app_status_text_has_low_battery_suffix(backend.status_text));
	remove_test_files();
}

static void test_low_battery_save_skips_optional_review_log(void)
{
	struct study_backend backend;
	struct study_session session;
	bool session_dirty = false;
	enum app_state_save_outcome outcome = APP_STATE_SAVE_OUTCOME_NONE;

	remove_test_files();
	study_backend_init(&backend, cards, 2);
	make_good_rating(&backend);
	study_session_init(&session, 100, 1);

	assert(
		app_state_save_if_dirty_with_outcome(
			&backend,
			true,
			true,
			TEST_STATE_PATH,
			TEST_LOG_PATH,
			true,
			STUDY_REVIEW_LOG_EVENT_RATING,
			STUDY_REVIEW_LOG_RATING_GOOD,
			&session,
			"sample",
			&session_dirty,
			true,
			1234,
			7,
			&outcome
		)
	);

	assert(outcome == APP_STATE_SAVE_OUTCOME_LOG_SKIPPED);
	assert(file_exists(TEST_STATE_PATH));
	assert(!file_exists(TEST_LOG_PATH));
	assert(session_dirty);
	assert(session.rating_saved_count == 1);
	assert(strcmp(backend.status_text, "Saved; log skipped; batt low") == 0);
	remove_test_files();
}

static void test_save_without_log_dirty_only_appends_battery_suffix(void)
{
	struct study_backend backend;
	struct study_session session;
	bool session_dirty = false;

	remove_test_files();
	study_backend_init(&backend, cards, 2);
	make_good_rating(&backend);
	study_session_init(&session, 100, 1);

	assert(
		app_state_save_if_dirty(
			&backend,
			true,
			true,
			TEST_STATE_PATH,
			TEST_LOG_PATH,
			false,
			STUDY_REVIEW_LOG_EVENT_RATING,
			STUDY_REVIEW_LOG_RATING_NONE,
			&session,
			"sample",
			&session_dirty,
			true,
			1234,
			7
		)
	);
	assert(file_exists(TEST_STATE_PATH));
	assert(!file_exists(TEST_LOG_PATH));
	assert(!session_dirty);
	assert(session.rating_saved_count == 0);
	assert(app_status_text_has_low_battery_suffix(backend.status_text));
	remove_test_files();
}

static void test_log_failure_keeps_state_and_marks_session(void)
{
	struct study_backend backend;
	struct study_session session;
	char long_log_path[640];
	bool session_dirty = false;
	enum app_state_save_outcome outcome = APP_STATE_SAVE_OUTCOME_NONE;

	remove_test_files();
	memset(long_log_path, 'x', sizeof(long_log_path) - 1);
	long_log_path[sizeof(long_log_path) - 1] = '\0';
	study_backend_init(&backend, cards, 2);
	make_good_rating(&backend);
	study_session_init(&session, 100, 1);

	assert(
		app_state_save_if_dirty_with_outcome(
			&backend,
			true,
			true,
			TEST_STATE_PATH,
			long_log_path,
			true,
			STUDY_REVIEW_LOG_EVENT_UNDO,
			STUDY_REVIEW_LOG_RATING_NONE,
			&session,
			"sample",
			&session_dirty,
			true,
			1234,
			7,
			&outcome
		)
	);
	assert(outcome == APP_STATE_SAVE_OUTCOME_LOG_SKIPPED);
	assert(file_exists(TEST_STATE_PATH));
	assert(session_dirty);
	assert(session.undo_saved_count == 1);
	assert(strcmp(backend.status_text, "Saved; log skipped; batt low") == 0);
	remove_test_files();
}

static void test_state_write_failure_reports_save_failed(void)
{
	struct study_backend backend;
	struct study_session session;
	bool session_dirty = false;
	enum app_state_save_outcome outcome = APP_STATE_SAVE_OUTCOME_NONE;

	study_backend_init(&backend, cards, 2);
	make_good_rating(&backend);
	study_session_init(&session, 100, 1);

	assert(
		app_state_save_if_dirty_with_outcome(
			&backend,
			true,
			true,
			NULL,
			TEST_LOG_PATH,
			true,
			STUDY_REVIEW_LOG_EVENT_RATING,
			STUDY_REVIEW_LOG_RATING_GOOD,
			&session,
			"sample",
			&session_dirty,
			true,
			1234,
			7,
			&outcome
		)
	);
	assert(outcome == APP_STATE_SAVE_OUTCOME_WRITE_FAILED);
	assert(strcmp(backend.status_text, "Save failed") == 0);
	assert(!session_dirty);
	assert(session.rating_saved_count == 0);
}

int main(void)
{
	test_noop_when_not_dirty_or_state_disabled();
	test_save_rating_writes_state_log_and_session();
	test_low_battery_save_skips_optional_review_log();
	test_save_without_log_dirty_only_appends_battery_suffix();
	test_log_failure_keeps_state_and_marks_session();
	test_state_write_failure_reports_save_failed();
	return 0;
}
