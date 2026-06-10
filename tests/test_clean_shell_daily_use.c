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

#define TEST_ROOT "/tmp/anki3ds-clean-shell-daily-use"
#define TEST_SESSION_PATH TEST_ROOT "/session.tsv"

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

static void remove_deck_files(const char *deck_id)
{
	char path[256];
	const char *files[] = {
		"cards.tsv",
		"deck.json",
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

	for (size_t index = 0; index < sizeof(files) / sizeof(files[0]); index++)
	{
		snprintf(path, sizeof(path), "%s/%s/%s", TEST_ROOT, deck_id, files[index]);
		remove_path(path);
	}
	snprintf(path, sizeof(path), "%s/%s", TEST_ROOT, deck_id);
	(void)rmdir(path);
}

static void remove_test_tree(void)
{
	remove_deck_files("alpha");
	remove_deck_files("beta");
	remove_path(TEST_SESSION_PATH);
	remove_path(TEST_SESSION_PATH ".tmp");
	remove_path(TEST_SESSION_PATH ".bak");
	(void)rmdir(TEST_ROOT);
}

static void make_deck_dir(const char *deck_id)
{
	char path[256];

	snprintf(path, sizeof(path), "%s/%s", TEST_ROOT, deck_id);
	assert(mkdir(path, 0700) == 0);
}

static void make_test_tree(void)
{
	struct study_settings settings;

	remove_test_tree();
	assert(mkdir(TEST_ROOT, 0700) == 0);
	make_deck_dir("alpha");
	make_deck_dir("beta");
	write_file(
		TEST_ROOT "/alpha/cards.tsv",
		"alpha-1\tnote-1\tAlpha front 1\tAlpha back 1\ttag\n"
		"alpha-2\tnote-2\tAlpha front 2\tAlpha back 2\ttag\n"
	);
	write_file(
		TEST_ROOT "/beta/cards.tsv",
		"beta-1\tnote-1\tBeta front 1\tBeta back 1\ttag\n"
		"beta-2\tnote-2\tBeta front 2\tBeta back 2\ttag\n"
	);
	write_file(TEST_ROOT "/alpha/deck.json", "{ \"name\": \"Alpha Daily\" }\n");
	study_settings_defaults(&settings);
	settings.new_limit = 2;
	settings.review_limit = 5;
	assert(
		study_settings_save_tsv(
			&settings,
			TEST_ROOT "/alpha/settings.tsv"
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

static const struct study_deck_entry *find_deck(
	const struct study_deck_index *index,
	const char *deck_id
)
{
	size_t entry_index = 0;

	assert(study_deck_index_find(index, deck_id, &entry_index));
	return study_deck_index_get(index, entry_index);
}

static void test_clean_shell_daily_use_flow(void)
{
	struct study_deck_index index;
	struct study_backend_deck alpha_deck;
	struct study_backend_deck beta_deck;
	struct study_backend alpha;
	struct study_backend beta;
	struct study_backend reloaded;
	struct study_backend_view view;
	struct study_settings settings;
	struct study_settings loaded_settings;
	struct study_session session;
	struct study_session loaded_session;
	const struct study_deck_entry *alpha_entry;
	const struct study_deck_entry *beta_entry;

	make_test_tree();
	study_deck_index_scan(&index, TEST_ROOT);
	assert(index.count == 2);
	assert(index.ignored_count == 0);
	alpha_entry = find_deck(&index, "alpha");
	beta_entry = find_deck(&index, "beta");
	assert(strcmp(alpha_entry->display_name, "Alpha Daily") == 0);

	study_session_init(&session, 1000, 1);
	study_session_start_launch(&session, 1000, 1);
	study_session_record_scan(&session, index.count, index.ignored_count, 1001, 1);

	assert(
		study_backend_load_cards_tsv(&alpha_deck, alpha_entry->cards_path) ==
		STUDY_BACKEND_LOAD_OK
	);
	study_backend_init(&alpha, alpha_deck.cards, alpha_deck.card_count);
	study_backend_set_status(&alpha, alpha_deck.status_text);
	assert(
		study_backend_load_state_tsv(&alpha, alpha_entry->state_path) ==
		STUDY_BACKEND_STATE_NOT_FOUND
	);
	assert(!study_backend_rollover_day(&alpha, 1));
	assert(study_settings_load_tsv(&settings, alpha_entry->settings_path) == STUDY_SETTINGS_OK);
	study_session_record_deck_open(&session, "alpha", true, true, 1002, 1);

	assert(!study_settings_new_limit_blocks_reveal(
		settings.new_limit,
		alpha.introduced_today_count,
		alpha.introduced[alpha.current_index]
	));
	assert(study_backend_show_answer(&alpha));
	study_session_record_event(
		&session,
		STUDY_SESSION_EVENT_ANSWER_SHOWN,
		"alpha",
		1003,
		1
	);
	assert(study_backend_rate_current(&alpha, STUDY_BACKEND_RATING_GOOD));
	append_log_entry(
		alpha_entry->review_log_path,
		1004,
		STUDY_REVIEW_LOG_EVENT_RATING,
		STUDY_REVIEW_LOG_RATING_GOOD,
		&alpha
	);
	study_session_record_event(
		&session,
		STUDY_SESSION_EVENT_RATING_SAVED,
		"alpha",
		1004,
		1
	);
	save_state_and_session(&alpha, alpha_entry->state_path, &session);

	assert(study_backend_undo_last_rating(&alpha));
	append_log_entry(
		alpha_entry->review_log_path,
		1005,
		STUDY_REVIEW_LOG_EVENT_UNDO,
		STUDY_REVIEW_LOG_RATING_NONE,
		&alpha
	);
	study_session_record_event(
		&session,
		STUDY_SESSION_EVENT_UNDO_SAVED,
		"alpha",
		1005,
		1
	);
	save_state_and_session(&alpha, alpha_entry->state_path, &session);

	assert(study_backend_show_answer(&alpha));
	study_session_record_event(
		&session,
		STUDY_SESSION_EVENT_ANSWER_SHOWN,
		"alpha",
		1006,
		1
	);
	assert(study_backend_rate_current(&alpha, STUDY_BACKEND_RATING_EASY));
	append_log_entry(
		alpha_entry->review_log_path,
		1007,
		STUDY_REVIEW_LOG_EVENT_RATING,
		STUDY_REVIEW_LOG_RATING_EASY,
		&alpha
	);
	study_session_record_event(
		&session,
		STUDY_SESSION_EVENT_RATING_SAVED,
		"alpha",
		1007,
		1
	);
	save_state_and_session(&alpha, alpha_entry->state_path, &session);
	assert(line_count(alpha_entry->review_log_path) == 3);

	settings.new_limit = 5;
	settings.review_limit = 10;
	assert(study_settings_save_tsv(&settings, alpha_entry->settings_path) == STUDY_SETTINGS_OK);
	study_session_record_event(
		&session,
		STUDY_SESSION_EVENT_SETTINGS_SAVED,
		"alpha",
		1008,
		1
	);
	assert(study_session_save_tsv(&session, TEST_SESSION_PATH) == STUDY_SESSION_OK);

	assert(
		study_backend_load_cards_tsv(&beta_deck, beta_entry->cards_path) ==
		STUDY_BACKEND_LOAD_OK
	);
	study_backend_init(&beta, beta_deck.cards, beta_deck.card_count);
	study_backend_set_status(&beta, beta_deck.status_text);
	assert(study_settings_load_tsv(&settings, beta_entry->settings_path) == STUDY_SETTINGS_NOT_FOUND);
	assert(!study_backend_rollover_day(&beta, 1));
	study_session_record_deck_open(&session, "beta", true, true, 1009, 1);
	assert(study_backend_suspend_current(&beta));
	append_log_entry(
		beta_entry->review_log_path,
		1010,
		STUDY_REVIEW_LOG_EVENT_SUSPEND,
		STUDY_REVIEW_LOG_RATING_NONE,
		&beta
	);
	study_session_record_event(
		&session,
		STUDY_SESSION_EVENT_SUSPEND_SAVED,
		"beta",
		1010,
		1
	);
	save_state_and_session(&beta, beta_entry->state_path, &session);
	assert(beta.suspended_count == 1);

	assert(study_backend_restore_suspended(&beta) == 1);
	append_log_entry(
		beta_entry->review_log_path,
		1011,
		STUDY_REVIEW_LOG_EVENT_RESTORE,
		STUDY_REVIEW_LOG_RATING_NONE,
		&beta
	);
	study_session_record_event(
		&session,
		STUDY_SESSION_EVENT_RESTORE_SAVED,
		"beta",
		1011,
		1
	);
	save_state_and_session(&beta, beta_entry->state_path, &session);
	assert(line_count(beta_entry->review_log_path) == 2);

	assert(
		study_backend_load_cards_tsv(&alpha_deck, alpha_entry->cards_path) ==
		STUDY_BACKEND_LOAD_OK
	);
	study_backend_init(&reloaded, alpha_deck.cards, alpha_deck.card_count);
	assert(
		study_backend_load_state_tsv(&reloaded, alpha_entry->state_path) ==
		STUDY_BACKEND_STATE_OK
	);
	assert(study_settings_load_tsv(&loaded_settings, alpha_entry->settings_path) == STUDY_SETTINGS_OK);
	study_backend_build_view(&reloaded, &view);
	assert(view.reviewed_count == 1);
	assert(view.reviewed_today_count == 1);
	assert(view.easy_count == 1);
	assert(view.introduced_count == 1);
	assert(view.introduced_today_count == 1);
	assert(loaded_settings.new_limit == 5);
	assert(loaded_settings.review_limit == 10);

	assert(study_backend_rollover_day(&reloaded, 2));
	study_backend_build_view(&reloaded, &view);
	assert(view.has_active_card);
	assert(view.card_index == 1);
	assert(!view.current_card_introduced);
	assert(view.reviewed_count == 1);
	assert(view.reviewed_today_count == 0);
	assert(view.introduced_count == 1);
	assert(view.introduced_today_count == 0);
	assert(!study_settings_new_limit_blocks_reveal(
		loaded_settings.new_limit,
		view.introduced_today_count,
		view.current_card_introduced
	));
	assert(study_backend_show_answer(&reloaded));
	study_backend_build_view(&reloaded, &view);
	assert(view.introduced_count == 2);
	assert(view.introduced_today_count == 1);

	assert(study_backend_reset_progress(&alpha));
	study_backend_build_view(&alpha, &view);
	assert(view.reviewed_today_count == 0);
	assert(view.introduced_today_count == 0);
	assert(study_backend_delete_state_tsv(alpha_entry->state_path));
	assert(study_review_log_delete(alpha_entry->review_log_path));
	study_session_record_event(
		&session,
		STUDY_SESSION_EVENT_RESET_PROGRESS,
		"alpha",
		1012,
		1
	);
	assert(access(alpha_entry->state_path, F_OK) != 0);
	assert(access(alpha_entry->review_log_path, F_OK) != 0);
	assert(access(alpha_entry->settings_path, F_OK) == 0);
	assert(study_settings_load_tsv(&loaded_settings, alpha_entry->settings_path) == STUDY_SETTINGS_OK);
	assert(loaded_settings.new_limit == 5);
	assert(loaded_settings.review_limit == 10);

	study_session_record_event(
		&session,
		STUDY_SESSION_EVENT_EXIT_CONFIRMED,
		"alpha",
		1013,
		1
	);
	assert(study_session_save_tsv(&session, TEST_SESSION_PATH) == STUDY_SESSION_OK);
	assert(study_session_load_tsv(&loaded_session, TEST_SESSION_PATH) == STUDY_SESSION_OK);
	assert(loaded_session.scan_completed);
	assert(loaded_session.deck_count == 2);
	assert(loaded_session.deck_open_count == 2);
	assert(loaded_session.review_screen_count == 2);
	assert(loaded_session.answer_shown_count == 2);
	assert(loaded_session.rating_saved_count == 2);
	assert(loaded_session.undo_saved_count == 1);
	assert(loaded_session.suspend_saved_count == 1);
	assert(loaded_session.restore_saved_count == 1);
	assert(loaded_session.settings_saved_count == 1);
	assert(loaded_session.reset_progress_count == 1);
	assert(loaded_session.exit_confirmed);
	assert(loaded_session.last_event == STUDY_SESSION_EVENT_EXIT_CONFIRMED);
	assert(strcmp(loaded_session.last_deck_id, "alpha") == 0);

	remove_test_tree();
}

int main(void)
{
	test_clean_shell_daily_use_flow();
	return 0;
}
