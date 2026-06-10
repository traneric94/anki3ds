#include "study_session.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST_SESSION_PATH "/tmp/anki3ds-study-session.tsv"
#define TEST_SESSION_TEMP_PATH "/tmp/anki3ds-study-session.tsv.tmp"
#define TEST_SESSION_BACKUP_PATH "/tmp/anki3ds-study-session.tsv.bak"

static void remove_session_files(void)
{
	(void)remove(TEST_SESSION_PATH);
	(void)remove(TEST_SESSION_TEMP_PATH);
	(void)remove(TEST_SESSION_BACKUP_PATH);
}

static void write_file(const char *path, const char *contents)
{
	FILE *file = fopen(path, "w");

	assert(file != NULL);
	assert(fputs(contents, file) >= 0);
	assert(fclose(file) == 0);
}

static void test_start_launch_preserves_prior_counters(void)
{
	struct study_session session;

	study_session_init(&session, 100, 1);
	study_session_start_launch(&session, 100, 1);
	study_session_record_event(
		&session,
		STUDY_SESSION_EVENT_RATING_SAVED,
		"sample",
		110,
		1
	);
	study_session_record_event(
		&session,
		STUDY_SESSION_EVENT_EXIT_CONFIRMED,
		"sample",
		120,
		1
	);
	study_session_start_launch(&session, 200, 2);

	assert(session.started_at == 100);
	assert(session.updated_at == 200);
	assert(session.launch_count == 2);
	assert(session.started_day == 1);
	assert(session.current_day == 2);
	assert(!session.exit_confirmed);
	assert(session.rating_saved_count == 1);
	assert(strcmp(session.last_deck_id, "sample") == 0);
	assert(session.last_event == STUDY_SESSION_EVENT_BOOT);
}

static void test_scan_deck_and_events_update_counters(void)
{
	struct study_session session;

	study_session_init(&session, 100, 1);
	study_session_start_launch(&session, 100, 1);
	study_session_record_scan(&session, 2, 1, 101, 1);
	study_session_record_deck_open(&session, "sample", true, true, 102, 1);
	study_session_record_event(
		&session,
		STUDY_SESSION_EVENT_ANSWER_SHOWN,
		"sample",
		103,
		1
	);
	study_session_record_event(
		&session,
		STUDY_SESSION_EVENT_UNDO_SAVED,
		"sample",
		104,
		1
	);
	study_session_record_deck_open(&session, "bad", false, false, 105, 1);

	assert(session.scan_completed);
	assert(session.deck_count == 2);
	assert(session.ignored_count == 1);
	assert(session.deck_open_count == 2);
	assert(session.review_screen_count == 1);
	assert(session.load_error_count == 1);
	assert(session.answer_shown_count == 1);
	assert(session.undo_saved_count == 1);
	assert(strcmp(session.last_deck_id, "bad") == 0);
	assert(session.last_event == STUDY_SESSION_EVENT_DECK_LOAD_ERROR);
}

static void test_day_rollover_updates_day_without_rewriting_event(void)
{
	struct study_session session;

	study_session_init(&session, 100, 1);
	study_session_start_launch(&session, 100, 1);
	study_session_record_scan(&session, 2, 0, 101, 1);

	assert(study_session_rollover_day(&session, 200, 2));
	assert(session.updated_at == 200);
	assert(session.started_day == 1);
	assert(session.current_day == 2);
	assert(session.scan_completed);
	assert(session.deck_count == 2);
	assert(session.last_event == STUDY_SESSION_EVENT_SCAN);
	assert(!study_session_rollover_day(&session, 201, 2));
	assert(session.updated_at == 200);
	assert(!study_session_rollover_day(NULL, 201, 3));
}

static void test_save_and_load_round_trip(void)
{
	struct study_session session;
	struct study_session loaded;

	remove_session_files();
	study_session_init(&session, 100, 1);
	study_session_start_launch(&session, 100, 1);
	study_session_record_scan(&session, 2, 0, 101, 1);
	study_session_record_deck_open(&session, "sample", true, true, 102, 1);
	study_session_record_event(
		&session,
		STUDY_SESSION_EVENT_SETTINGS_SAVED,
		"sample",
		103,
		1
	);
	assert(study_session_save_tsv(&session, TEST_SESSION_PATH) == STUDY_SESSION_OK);
	assert(study_session_load_tsv(&loaded, TEST_SESSION_PATH) == STUDY_SESSION_OK);

	assert(loaded.started_at == 100);
	assert(loaded.updated_at == 103);
	assert(loaded.launch_count == 1);
	assert(loaded.scan_completed);
	assert(loaded.deck_count == 2);
	assert(loaded.deck_open_count == 1);
	assert(loaded.review_screen_count == 1);
	assert(loaded.settings_saved_count == 1);
	assert(strcmp(loaded.last_deck_id, "sample") == 0);
	assert(loaded.last_event == STUDY_SESSION_EVENT_SETTINGS_SAVED);
	assert(remove(TEST_SESSION_PATH) == 0);
	remove_session_files();
}

static void test_load_recovers_temp_artifact(void)
{
	struct study_session session;

	remove_session_files();
	write_file(TEST_SESSION_PATH, "#anki3ds-session-v1\nbad\trow\n");
	write_file(
		TEST_SESSION_TEMP_PATH,
		"#anki3ds-session-v1\n"
		"started_at\t100\n"
		"updated_at\t101\n"
		"launch_count\t1\n"
		"started_day\t1\n"
		"current_day\t1\n"
		"scan_completed\t1\n"
		"deck_count\t2\n"
		"ignored_count\t0\n"
		"deck_open_count\t0\n"
		"review_screen_count\t0\n"
		"summary_screen_count\t0\n"
		"load_error_count\t0\n"
		"answer_shown_count\t0\n"
		"rating_saved_count\t0\n"
		"undo_saved_count\t0\n"
		"suspend_saved_count\t0\n"
		"restore_saved_count\t0\n"
		"settings_saved_count\t0\n"
		"reset_progress_count\t0\n"
		"exit_confirmed\t0\n"
		"last_deck_id\t-\n"
		"last_event\tscan\n"
		"#anki3ds-session-complete\n"
	);

	assert(study_session_load_tsv(&session, TEST_SESSION_PATH) == STUDY_SESSION_OK);
	assert(session.scan_completed);
	assert(session.deck_count == 2);
	assert(session.last_event == STUDY_SESSION_EVENT_SCAN);
	remove_session_files();
}

static void test_bad_format_load_or_init_starts_fresh(void)
{
	struct study_session session;

	remove_session_files();
	write_file(TEST_SESSION_PATH, "not a session\n");
	assert(
		study_session_load_or_init(
			&session,
			TEST_SESSION_PATH,
			500,
			7
		) == STUDY_SESSION_BAD_FORMAT
	);
	assert(session.started_at == 500);
	assert(session.started_day == 7);
	assert(session.launch_count == 0);
	assert(strcmp(study_session_result_name(STUDY_SESSION_BAD_FORMAT), "Session ignored") == 0);
	remove_session_files();
}

static void test_load_rejects_inconsistent_deck_open_counters(void)
{
	struct study_session session;

	remove_session_files();
	write_file(
		TEST_SESSION_PATH,
		"#anki3ds-session-v1\n"
		"started_at\t100\n"
		"updated_at\t110\n"
		"launch_count\t1\n"
		"started_day\t1\n"
		"current_day\t1\n"
		"scan_completed\t1\n"
		"deck_count\t2\n"
		"ignored_count\t0\n"
		"deck_open_count\t1\n"
		"review_screen_count\t0\n"
		"summary_screen_count\t0\n"
		"load_error_count\t0\n"
		"answer_shown_count\t0\n"
		"rating_saved_count\t0\n"
		"undo_saved_count\t0\n"
		"suspend_saved_count\t0\n"
		"restore_saved_count\t0\n"
		"settings_saved_count\t0\n"
		"reset_progress_count\t0\n"
		"exit_confirmed\t0\n"
		"last_deck_id\tsample\n"
		"last_event\tdeck_review\n"
		"#anki3ds-session-complete\n"
	);

	assert(study_session_load_tsv(&session, TEST_SESSION_PATH) == STUDY_SESSION_BAD_FORMAT);
	assert(session.started_at == 0);
	assert(strcmp(session.last_deck_id, "-") == 0);
	remove_session_files();
}

static void test_load_rejects_scan_event_without_completed_scan(void)
{
	struct study_session session;

	remove_session_files();
	write_file(
		TEST_SESSION_PATH,
		"#anki3ds-session-v1\n"
		"started_at\t100\n"
		"updated_at\t110\n"
		"launch_count\t1\n"
		"started_day\t1\n"
		"current_day\t1\n"
		"scan_completed\t0\n"
		"deck_count\t2\n"
		"ignored_count\t0\n"
		"deck_open_count\t0\n"
		"review_screen_count\t0\n"
		"summary_screen_count\t0\n"
		"load_error_count\t0\n"
		"answer_shown_count\t0\n"
		"rating_saved_count\t0\n"
		"undo_saved_count\t0\n"
		"suspend_saved_count\t0\n"
		"restore_saved_count\t0\n"
		"settings_saved_count\t0\n"
		"reset_progress_count\t0\n"
		"exit_confirmed\t0\n"
		"last_deck_id\t-\n"
		"last_event\tscan\n"
		"#anki3ds-session-complete\n"
	);

	assert(study_session_load_tsv(&session, TEST_SESSION_PATH) == STUDY_SESSION_BAD_FORMAT);
	remove_session_files();
}

static void test_load_rejects_mismatched_exit_confirmation_flag(void)
{
	struct study_session session;

	remove_session_files();
	write_file(
		TEST_SESSION_PATH,
		"#anki3ds-session-v1\n"
		"started_at\t100\n"
		"updated_at\t110\n"
		"launch_count\t1\n"
		"started_day\t1\n"
		"current_day\t1\n"
		"scan_completed\t1\n"
		"deck_count\t2\n"
		"ignored_count\t0\n"
		"deck_open_count\t0\n"
		"review_screen_count\t0\n"
		"summary_screen_count\t0\n"
		"load_error_count\t0\n"
		"answer_shown_count\t0\n"
		"rating_saved_count\t0\n"
		"undo_saved_count\t0\n"
		"suspend_saved_count\t0\n"
		"restore_saved_count\t0\n"
		"settings_saved_count\t0\n"
		"reset_progress_count\t0\n"
		"exit_confirmed\t1\n"
		"last_deck_id\tsample\n"
		"last_event\tscan\n"
		"#anki3ds-session-complete\n"
	);

	assert(study_session_load_tsv(&session, TEST_SESSION_PATH) == STUDY_SESSION_BAD_FORMAT);
	remove_session_files();
}

int main(void)
{
	test_start_launch_preserves_prior_counters();
	test_scan_deck_and_events_update_counters();
	test_day_rollover_updates_day_without_rewriting_event();
	test_save_and_load_round_trip();
	test_load_recovers_temp_artifact();
	test_bad_format_load_or_init_starts_fresh();
	test_load_rejects_inconsistent_deck_open_counters();
	test_load_rejects_scan_event_without_completed_scan();
	test_load_rejects_mismatched_exit_confirmation_flag();
	return 0;
}
