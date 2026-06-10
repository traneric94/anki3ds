#include "study_backend.h"
#include "study_deck_index.h"
#include "study_review_log.h"
#include "study_session.h"
#include "study_settings.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_ROOT "/tmp/anki3ds-m7-direct-smoke"
#define TEST_APP_ROOT TEST_ROOT "/3ds/anki3ds"
#define TEST_DECK_ROOT TEST_APP_ROOT "/decks"
#define TEST_LIMITS_DEMO_DIR TEST_DECK_ROOT "/limits-demo"
#define TEST_SAMPLE_DIR TEST_DECK_ROOT "/sample"
#define TEST_DECK_DIR TEST_LIMITS_DEMO_DIR
#define TEST_SESSION_PATH TEST_APP_ROOT "/session.tsv"

static void remove_path(const char *path)
{
	(void)remove(path);
}

static void write_file(const char *path, const char *contents)
{
	FILE *file = fopen(path, "w");

	assert(file != NULL);
	assert(fputs(contents, file) >= 0);
	assert(fclose(file) == 0);
}

static void remove_deck_tree(const char *deck_dir)
{
	const char *deck_files[] = {
		"cards.tsv",
		"settings.tsv",
		"settings.tsv.tmp",
		"settings.tsv.bak",
		"state.tsv",
		"state.tsv.tmp",
		"state.tsv.bak",
		"review-log.tsv",
		"review-log.tsv.tmp",
		"review-log.tsv.bak",
	};
	char path[256];

	for (size_t index = 0; index < sizeof(deck_files) / sizeof(deck_files[0]); index++)
	{
		snprintf(path, sizeof(path), "%s/%s", deck_dir, deck_files[index]);
		remove_path(path);
	}
	(void)rmdir(deck_dir);
}

static void remove_test_tree(void)
{
	remove_deck_tree(TEST_LIMITS_DEMO_DIR);
	remove_deck_tree(TEST_SAMPLE_DIR);
	remove_path(TEST_SESSION_PATH);
	remove_path(TEST_SESSION_PATH ".tmp");
	remove_path(TEST_SESSION_PATH ".bak");
	(void)rmdir(TEST_DECK_ROOT);
	(void)rmdir(TEST_APP_ROOT);
	(void)rmdir(TEST_ROOT "/3ds");
	(void)rmdir(TEST_ROOT);
}

static void make_test_tree(void)
{
	struct study_settings settings;

	remove_test_tree();
	assert(mkdir(TEST_ROOT, 0700) == 0);
	assert(mkdir(TEST_ROOT "/3ds", 0700) == 0);
	assert(mkdir(TEST_APP_ROOT, 0700) == 0);
	assert(mkdir(TEST_DECK_ROOT, 0700) == 0);
	assert(mkdir(TEST_LIMITS_DEMO_DIR, 0700) == 0);
	assert(mkdir(TEST_SAMPLE_DIR, 0700) == 0);
	write_file(
		TEST_LIMITS_DEMO_DIR "/cards.tsv",
		"limit-0001\tlimit-note-0001\tFront 1\tBack 1\tlimits\n"
		"limit-0002\tlimit-note-0002\tFront 2\tBack 2\tlimits\n"
		"limit-0003\tlimit-note-0003\tFront 3\tBack 3\tlimits\n"
		"limit-0004\tlimit-note-0004\tFront 4\tBack 4\tlimits\n"
		"limit-0005\tlimit-note-0005\tFront 5\tBack 5\tlimits\n"
		"limit-0006\tlimit-note-0006\tFront 6\tBack 6\tlimits\n"
	);
	write_file(
		TEST_SAMPLE_DIR "/cards.tsv",
		"sample-0001\tsample-note-0001\tSample front 1\tSample back 1\tsample\n"
		"sample-0002\tsample-note-0002\tSample front 2\tSample back 2\tsample\n"
	);
	study_settings_defaults(&settings);
	settings.new_limit = 2;
	settings.review_limit = 5;
	assert(
		study_settings_save_tsv(
			&settings,
			TEST_LIMITS_DEMO_DIR "/settings.tsv"
		) == STUDY_SETTINGS_OK
	);
	settings.new_limit = 20;
	settings.review_limit = 200;
	assert(
		study_settings_save_tsv(
			&settings,
			TEST_SAMPLE_DIR "/settings.tsv"
		) == STUDY_SETTINGS_OK
	);
}

static long line_count(const char *path)
{
	FILE *file = fopen(path, "r");
	long count = 0;
	int value;

	assert(file != NULL);
	while ((value = fgetc(file)) != EOF)
	{
		if (value == '\n')
			count++;
	}
	assert(!ferror(file));
	assert(fclose(file) == 0);
	return count;
}

static void append_log_entry(
	const char *path,
	unsigned long timestamp,
	enum study_review_log_event event,
	enum study_review_log_rating rating,
	const struct study_backend *backend
)
{
	struct study_review_log_entry entry;

	memset(&entry, 0, sizeof(entry));
	entry.timestamp = timestamp;
	entry.event = event;
	entry.rating = rating;
	entry.card_index = backend != NULL ? backend->current_index : 0;
	entry.reviewed_count = backend != NULL ? backend->reviewed_count : 0;
	entry.suspended_count = backend != NULL ? backend->suspended_count : 0;
	assert(study_review_log_append(path, &entry));
}

static void save_state_and_session(
	const struct study_backend *backend,
	const char *state_path,
	struct study_session *session
)
{
	assert(study_backend_save_state_tsv(backend, state_path) == STUDY_BACKEND_STATE_OK);
	assert(study_session_save_tsv(session, TEST_SESSION_PATH) == STUDY_SESSION_OK);
}

static const struct study_deck_entry *find_entry(
	const struct study_deck_index *index,
	const char *deck_id
)
{
	size_t selected_index = 0;

	assert(study_deck_index_find(index, deck_id, &selected_index));
	return study_deck_index_get(index, selected_index);
}

static void save_settings_for_smoke(
	const struct study_deck_entry *entry,
	struct study_settings *settings,
	struct study_session *session
)
{
	settings->new_limit = 5;
	settings->review_limit = 10;
	assert(study_settings_save_tsv(settings, entry->settings_path) == STUDY_SETTINGS_OK);
	study_session_record_event(
		session,
		STUDY_SESSION_EVENT_SETTINGS_SAVED,
		"limits-demo",
		1003,
		1
	);
	assert(study_session_save_tsv(session, TEST_SESSION_PATH) == STUDY_SESSION_OK);
}

static void reveal_rate_and_undo(
	struct study_backend *backend,
	const struct study_deck_entry *entry,
	struct study_session *session
)
{
	assert(study_backend_show_answer(backend));
	study_session_record_event(
		session,
		STUDY_SESSION_EVENT_ANSWER_SHOWN,
		"limits-demo",
		1004,
		1
	);
	save_state_and_session(backend, entry->state_path, session);

	assert(study_backend_rate_current(backend, STUDY_BACKEND_RATING_GOOD));
	append_log_entry(
		entry->review_log_path,
		1005,
		STUDY_REVIEW_LOG_EVENT_RATING,
		STUDY_REVIEW_LOG_RATING_GOOD,
		backend
	);
	study_session_record_event(
		session,
		STUDY_SESSION_EVENT_RATING_SAVED,
		"limits-demo",
		1005,
		1
	);
	save_state_and_session(backend, entry->state_path, session);

	assert(study_backend_undo_last_rating(backend));
	append_log_entry(
		entry->review_log_path,
		1006,
		STUDY_REVIEW_LOG_EVENT_UNDO,
		STUDY_REVIEW_LOG_RATING_NONE,
		backend
	);
	study_session_record_event(
		session,
		STUDY_SESSION_EVENT_UNDO_SAVED,
		"limits-demo",
		1006,
		1
	);
	save_state_and_session(backend, entry->state_path, session);

	assert(study_backend_show_answer(backend));
	study_session_record_event(
		session,
		STUDY_SESSION_EVENT_ANSWER_SHOWN,
		"limits-demo",
		1007,
		1
	);
	save_state_and_session(backend, entry->state_path, session);
}

static void suspend_all_cards(
	struct study_backend *backend,
	const struct study_deck_entry *entry,
	struct study_session *session
)
{
	struct study_backend_view view;

	for (size_t index = 0; index < 6; index++)
	{
		assert(study_backend_suspend_current(backend));
		append_log_entry(
			entry->review_log_path,
			1008 + (unsigned long)index,
			STUDY_REVIEW_LOG_EVENT_SUSPEND,
			STUDY_REVIEW_LOG_RATING_NONE,
			backend
		);
		study_session_record_event(
			session,
			STUDY_SESSION_EVENT_SUSPEND_SAVED,
			"limits-demo",
			1008 + (unsigned long)index,
			1
		);
		save_state_and_session(backend, entry->state_path, session);
	}
	study_backend_build_view(backend, &view);
	assert(!view.has_active_card);
	assert(backend->suspended_count == 6);
}

static void restore_suspended_cards(
	struct study_backend *backend,
	const struct study_deck_entry *entry,
	struct study_session *session
)
{
	struct study_backend_view view;

	assert(study_backend_restore_suspended(backend) == 6);
	append_log_entry(
		entry->review_log_path,
		1014,
		STUDY_REVIEW_LOG_EVENT_RESTORE,
		STUDY_REVIEW_LOG_RATING_NONE,
		backend
	);
	study_session_record_event(
		session,
		STUDY_SESSION_EVENT_RESTORE_SAVED,
		"limits-demo",
		1014,
		1
	);
	save_state_and_session(backend, entry->state_path, session);
	study_backend_build_view(backend, &view);
	assert(view.has_active_card);
	assert(backend->suspended_count == 0);
}

static void study_and_reset_sample_deck(
	const struct study_deck_entry *entry,
	struct study_session *session
)
{
	struct study_backend_deck deck;
	struct study_backend backend;
	struct study_settings settings;

	assert(
		study_backend_load_cards_tsv(&deck, entry->cards_path) ==
		STUDY_BACKEND_LOAD_OK
	);
	study_backend_init(&backend, deck.cards, deck.card_count);
	assert(
		study_backend_load_state_tsv(&backend, entry->state_path) ==
		STUDY_BACKEND_STATE_NOT_FOUND
	);
	assert(!study_backend_rollover_day(&backend, 1));
	assert(study_settings_load_tsv(&settings, entry->settings_path) == STUDY_SETTINGS_OK);
	assert(settings.new_limit == 20);
	assert(settings.review_limit == 200);

	study_session_record_deck_open(session, "sample", true, true, 2003, 1);
	assert(study_backend_show_answer(&backend));
	study_session_record_event(
		session,
		STUDY_SESSION_EVENT_ANSWER_SHOWN,
		"sample",
		2004,
		1
	);
	save_state_and_session(&backend, entry->state_path, session);

	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	append_log_entry(
		entry->review_log_path,
		2005,
		STUDY_REVIEW_LOG_EVENT_RATING,
		STUDY_REVIEW_LOG_RATING_GOOD,
		&backend
	);
	study_session_record_event(
		session,
		STUDY_SESSION_EVENT_RATING_SAVED,
		"sample",
		2005,
		1
	);
	save_state_and_session(&backend, entry->state_path, session);

	assert(study_backend_reset_progress(&backend));
	assert(study_backend_delete_state_tsv(entry->state_path));
	assert(study_review_log_delete(entry->review_log_path));
	study_session_record_event(
		session,
		STUDY_SESSION_EVENT_RESET_PROGRESS,
		"sample",
		2006,
		1
	);
	assert(study_session_save_tsv(session, TEST_SESSION_PATH) == STUDY_SESSION_OK);
	assert(access(entry->state_path, F_OK) != 0);
	assert(access(entry->review_log_path, F_OK) != 0);
	assert(access(entry->settings_path, F_OK) == 0);
	assert(study_settings_load_tsv(&settings, entry->settings_path) == STUDY_SETTINGS_OK);
	assert(settings.new_limit == 20);
	assert(settings.review_limit == 200);
}

static void test_direct_control_smoke_flow_writes_verifier_shape(void)
{
	struct study_deck_index index;
	struct study_backend_deck deck;
	struct study_backend backend;
	struct study_backend reloaded_backend;
	struct study_backend_view view;
	struct study_settings settings;
	struct study_settings loaded_settings;
	struct study_session session;
	struct study_session loaded_session;
	const struct study_deck_entry *entry;
	const struct study_deck_entry *sample_entry;

	make_test_tree();
	study_deck_index_scan(&index, TEST_DECK_ROOT);
	assert(index.count == 2);
	assert(index.ignored_count == 0);
	entry = find_entry(&index, "limits-demo");
	sample_entry = find_entry(&index, "sample");
	assert(
		study_backend_load_cards_tsv(&deck, entry->cards_path) ==
		STUDY_BACKEND_LOAD_OK
	);
	study_backend_init(&backend, deck.cards, deck.card_count);
	assert(
		study_backend_load_state_tsv(&backend, entry->state_path) ==
		STUDY_BACKEND_STATE_NOT_FOUND
	);
	assert(!study_backend_rollover_day(&backend, 1));
	assert(study_settings_load_tsv(&settings, entry->settings_path) == STUDY_SETTINGS_OK);

	study_session_init(&session, 1000, 1);
	study_session_start_launch(&session, 1000, 1);
	study_session_record_scan(&session, index.count, index.ignored_count, 1001, 1);
	study_session_record_deck_open(&session, "limits-demo", true, true, 1002, 1);
	assert(study_session_save_tsv(&session, TEST_SESSION_PATH) == STUDY_SESSION_OK);

	save_settings_for_smoke(entry, &settings, &session);
	reveal_rate_and_undo(&backend, entry, &session);
	suspend_all_cards(&backend, entry, &session);
	restore_suspended_cards(&backend, entry, &session);

	study_session_record_event(
		&session,
		STUDY_SESSION_EVENT_EXIT_CONFIRMED,
		"limits-demo",
		1015,
		1
	);
	assert(study_session_save_tsv(&session, TEST_SESSION_PATH) == STUDY_SESSION_OK);

	assert(study_session_load_tsv(&loaded_session, TEST_SESSION_PATH) == STUDY_SESSION_OK);
	study_session_start_launch(&loaded_session, 2000, 1);
	study_session_record_scan(&loaded_session, index.count, index.ignored_count, 2001, 1);

	assert(
		study_backend_load_cards_tsv(&deck, entry->cards_path) ==
		STUDY_BACKEND_LOAD_OK
	);
	study_backend_init(&reloaded_backend, deck.cards, deck.card_count);
	assert(
		study_backend_load_state_tsv(&reloaded_backend, entry->state_path) ==
		STUDY_BACKEND_STATE_OK
	);
	study_backend_build_view(&reloaded_backend, &view);
	assert(view.has_active_card);
	assert(study_settings_load_tsv(&loaded_settings, entry->settings_path) == STUDY_SETTINGS_OK);
	assert(loaded_settings.new_limit == 5);
	assert(loaded_settings.review_limit == 10);

	study_session_record_deck_open(&loaded_session, "limits-demo", true, true, 2002, 1);
	study_and_reset_sample_deck(sample_entry, &loaded_session);
	study_session_record_event(
		&loaded_session,
		STUDY_SESSION_EVENT_EXIT_CONFIRMED,
		"sample",
		2007,
		1
	);
	assert(study_session_save_tsv(&loaded_session, TEST_SESSION_PATH) == STUDY_SESSION_OK);

	assert(study_session_load_tsv(&loaded_session, TEST_SESSION_PATH) == STUDY_SESSION_OK);
	assert(loaded_session.launch_count == 2);
	assert(loaded_session.scan_completed);
	assert(loaded_session.deck_count == 2);
	assert(loaded_session.deck_open_count == 3);
	assert(loaded_session.review_screen_count == 3);
	assert(loaded_session.summary_screen_count == 0);
	assert(loaded_session.answer_shown_count == 3);
	assert(loaded_session.rating_saved_count == 2);
	assert(loaded_session.undo_saved_count == 1);
	assert(loaded_session.suspend_saved_count == 6);
	assert(loaded_session.restore_saved_count == 1);
	assert(loaded_session.settings_saved_count == 1);
	assert(loaded_session.reset_progress_count == 1);
	assert(loaded_session.exit_confirmed);
	assert(loaded_session.last_event == STUDY_SESSION_EVENT_EXIT_CONFIRMED);
	assert(strcmp(loaded_session.last_deck_id, "sample") == 0);
	assert(line_count(entry->review_log_path) == 9);
	assert(access(entry->state_path, F_OK) == 0);
	assert(access(entry->settings_path, F_OK) == 0);
	assert(access(sample_entry->state_path, F_OK) != 0);
	assert(access(sample_entry->review_log_path, F_OK) != 0);
	assert(access(sample_entry->settings_path, F_OK) == 0);

	remove_test_tree();
}

int main(void)
{
	test_direct_control_smoke_flow_writes_verifier_shape();
	return 0;
}
