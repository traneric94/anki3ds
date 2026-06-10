#include "app_session_save.h"

#include "study_backend.h"
#include "study_session.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST_SESSION_PATH "/tmp/anki3ds-app-session-save.tsv"
#define TEST_SESSION_TMP_PATH "/tmp/anki3ds-app-session-save.tsv.tmp"
#define TEST_SESSION_BAK_PATH "/tmp/anki3ds-app-session-save.tsv.bak"

static void remove_file_if_present(const char *path)
{
	if (path != NULL)
		(void)remove(path);
}

static void remove_test_files(void)
{
	remove_file_if_present(TEST_SESSION_PATH);
	remove_file_if_present(TEST_SESSION_TMP_PATH);
	remove_file_if_present(TEST_SESSION_BAK_PATH);
}

static bool file_exists(const char *path)
{
	return access(path, F_OK) == 0;
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

static void test_noop_when_not_dirty_preserves_status(void)
{
	struct study_backend backend;
	struct study_session session;

	remove_test_files();
	study_backend_init(&backend, NULL, 0);
	study_backend_set_status(&backend, "Ready");
	study_session_init(&session, 100, 1);

	assert(
		!app_session_save_if_dirty(
			&backend,
			&session,
			false,
			TEST_SESSION_PATH
		)
	);
	assert(strcmp(backend.status_text, "Ready") == 0);
	assert(!file_exists(TEST_SESSION_PATH));
	remove_test_files();
}

static void test_dirty_session_writes_file_without_status_change(void)
{
	struct study_backend backend;
	struct study_session session;
	struct study_session loaded;

	remove_test_files();
	study_backend_init(&backend, NULL, 0);
	study_backend_set_status(&backend, "Ready");
	study_session_init(&session, 100, 1);
	study_session_record_scan(&session, 2, 0, 120, 1);

	assert(
		!app_session_save_if_dirty(
			&backend,
			&session,
			true,
			TEST_SESSION_PATH
		)
	);
	assert(strcmp(backend.status_text, "Ready") == 0);
	assert(file_exists(TEST_SESSION_PATH));
	assert(study_session_load_tsv(&loaded, TEST_SESSION_PATH) == STUDY_SESSION_OK);
	assert(loaded.scan_completed);
	assert(loaded.deck_count == 2);
	assert(loaded.updated_at == 120);
	assert(loaded.last_event == STUDY_SESSION_EVENT_SCAN);
	remove_test_files();
}

static void test_dirty_session_preserves_backup_on_overwrite(void)
{
	struct study_backend backend;
	struct study_session session;
	char backup_text[512];

	remove_test_files();
	study_backend_init(&backend, NULL, 0);
	study_session_init(&session, 100, 1);

	assert(
		!app_session_save_if_dirty(
			&backend,
			&session,
			true,
			TEST_SESSION_PATH
		)
	);
	study_session_record_event(
		&session,
		STUDY_SESSION_EVENT_EXIT_CONFIRMED,
		"sample",
		200,
		1
	);
	assert(
		!app_session_save_if_dirty(
			&backend,
			&session,
			true,
			TEST_SESSION_PATH
		)
	);
	assert(file_exists(TEST_SESSION_BAK_PATH));
	read_file(TEST_SESSION_BAK_PATH, backup_text, sizeof(backup_text));
	assert(strstr(backup_text, "last_event\tboot\n") != NULL);
	remove_test_files();
}

static void test_save_failure_sets_backend_status(void)
{
	struct study_backend backend;
	struct study_session session;

	remove_test_files();
	study_backend_init(&backend, NULL, 0);
	study_backend_set_status(&backend, "Ready");
	study_session_init(&session, 100, 1);

	assert(
		app_session_save_if_dirty(
			&backend,
			&session,
			true,
			NULL
		)
	);
	assert(strcmp(backend.status_text, "Session save failed") == 0);
	assert(!file_exists(TEST_SESSION_PATH));
	remove_test_files();
}

static void test_outcome_reports_noop_saved_and_failed_writes(void)
{
	struct study_backend backend;
	struct study_session session;
	enum app_session_save_outcome outcome;

	remove_test_files();
	study_backend_init(&backend, NULL, 0);
	study_session_init(&session, 100, 1);

	outcome = APP_SESSION_SAVE_OUTCOME_WRITE_FAILED;
	assert(
		!app_session_save_if_dirty_with_outcome(
			&backend,
			&session,
			false,
			TEST_SESSION_PATH,
			&outcome
		)
	);
	assert(outcome == APP_SESSION_SAVE_OUTCOME_NONE);
	assert(!file_exists(TEST_SESSION_PATH));

	assert(
		!app_session_save_if_dirty_with_outcome(
			&backend,
			&session,
			true,
			TEST_SESSION_PATH,
			&outcome
		)
	);
	assert(outcome == APP_SESSION_SAVE_OUTCOME_SAVED);
	assert(file_exists(TEST_SESSION_PATH));

	assert(
		app_session_save_if_dirty_with_outcome(
			&backend,
			&session,
			true,
			NULL,
			&outcome
		)
	);
	assert(outcome == APP_SESSION_SAVE_OUTCOME_WRITE_FAILED);
	assert(strcmp(backend.status_text, "Session save failed") == 0);
	remove_test_files();
}

int main(void)
{
	test_noop_when_not_dirty_preserves_status();
	test_dirty_session_writes_file_without_status_change();
	test_dirty_session_preserves_backup_on_overwrite();
	test_save_failure_sets_backend_status();
	test_outcome_reports_noop_saved_and_failed_writes();
	return 0;
}
