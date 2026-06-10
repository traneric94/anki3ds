#include "app_deck_flow.h"

#include "study_controls.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_ROOT "/tmp/anki3ds-app-deck-flow-test"

struct test_paths
{
	char state_path[STUDY_DECK_INDEX_PATH_SIZE];
	char settings_path[STUDY_DECK_INDEX_PATH_SIZE];
	char review_log_path[STUDY_DECK_INDEX_PATH_SIZE];
	char deck_id[STUDY_DECK_INDEX_ID_SIZE];
	struct app_deck_flow_paths flow_paths;
};

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
		(void)remove(path);
		(void)rmdir(path);
	}
	snprintf(path, sizeof(path), "%s/%s", TEST_ROOT, deck_id);
	(void)rmdir(path);
}

static void remove_test_tree(void)
{
	remove_deck_files("alpha");
	remove_deck_files("bad");
	remove_deck_files("beta");
	(void)rmdir(TEST_ROOT);
}

static void make_deck_dir(const char *deck_id)
{
	char path[256];

	snprintf(path, sizeof(path), "%s/%s", TEST_ROOT, deck_id);
	assert(mkdir(path, 0700) == 0);
}

static void write_alpha_progress_state(void)
{
	struct study_backend_deck deck;
	struct study_backend backend;

	assert(
		study_backend_load_cards_tsv(
			&deck,
			TEST_ROOT "/alpha/cards.tsv"
		) == STUDY_BACKEND_LOAD_OK
	);
	study_backend_init(&backend, deck.cards, deck.card_count);
	assert(!study_backend_rollover_day(&backend, 1));
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	assert(
		study_backend_save_state_tsv(
			&backend,
			TEST_ROOT "/alpha/state.tsv"
		) == STUDY_BACKEND_STATE_OK
	);
}

static void make_test_tree(void)
{
	struct study_settings settings;

	remove_test_tree();
	assert(mkdir(TEST_ROOT, 0700) == 0);
	make_deck_dir("alpha");
	make_deck_dir("bad");
	make_deck_dir("beta");
	write_file(
		TEST_ROOT "/alpha/cards.tsv",
		"alpha-1\tnote-1\tAlpha front 1\tAlpha back 1\ttag\n"
		"alpha-2\tnote-2\tAlpha front 2\tAlpha back 2\ttag\n"
	);
	write_file(TEST_ROOT "/alpha/deck.json", "{ \"name\": \"Alpha Daily\" }\n");
	write_file(TEST_ROOT "/bad/cards.tsv", "bad-row\n");
	write_file(
		TEST_ROOT "/beta/cards.tsv",
		"beta-1\tnote-1\tBeta front 1\tBeta back 1\ttag\n"
	);
	study_settings_defaults(&settings);
	settings.new_limit = 5;
	settings.review_limit = 10;
	settings.learning_mode = STUDY_SETTINGS_LEARNING_CARD_COOLDOWN;
	assert(
		study_settings_save_tsv(
			&settings,
			TEST_ROOT "/alpha/settings.tsv"
		) == STUDY_SETTINGS_OK
	);
	write_alpha_progress_state();
}

static void init_test_paths(struct test_paths *paths)
{
	memset(paths, 0, sizeof(*paths));
	snprintf(paths->state_path, sizeof(paths->state_path), "stale-state");
	snprintf(paths->settings_path, sizeof(paths->settings_path), "stale-settings");
	snprintf(
		paths->review_log_path,
		sizeof(paths->review_log_path),
		"stale-log"
	);
	snprintf(paths->deck_id, sizeof(paths->deck_id), "stale-id");
	paths->flow_paths.state_path = paths->state_path;
	paths->flow_paths.state_path_size = sizeof(paths->state_path);
	paths->flow_paths.settings_path = paths->settings_path;
	paths->flow_paths.settings_path_size = sizeof(paths->settings_path);
	paths->flow_paths.review_log_path = paths->review_log_path;
	paths->flow_paths.review_log_path_size = sizeof(paths->review_log_path);
	paths->flow_paths.deck_id = paths->deck_id;
	paths->flow_paths.deck_id_size = sizeof(paths->deck_id);
}

static size_t find_deck_index(
	const struct study_deck_index *index,
	const char *deck_id
)
{
	size_t entry_index = 999;

	assert(study_deck_index_find(index, deck_id, &entry_index));
	return entry_index;
}

static void test_rescan_preserves_selected_deck_and_status(void)
{
	struct study_deck_index index;
	struct study_backend backend;
	size_t selected_index;

	make_test_tree();
	study_deck_index_scan(&index, TEST_ROOT);
	selected_index = find_deck_index(&index, "beta");
	study_backend_init(&backend, NULL, 0);

	app_deck_flow_rescan(&index, &selected_index, &backend, TEST_ROOT, 1);

	assert(strcmp(index.entries[selected_index].id, "beta") == 0);
	assert(strcmp(backend.status_text, "Found 3 decks") == 0);
	remove_test_tree();
}

static void test_startup_scan_initializes_backend_and_records_session(void)
{
	static const struct study_backend_card stale_cards[] = {
		{ "stale front", "stale back", NULL },
	};
	struct study_deck_index index;
	struct study_backend backend;
	struct study_session session;
	size_t selected_index = 0;
	bool session_dirty = false;

	make_test_tree();
	study_deck_index_init(&index);
	study_backend_init(
		&backend,
		stale_cards,
		sizeof(stale_cards) / sizeof(stale_cards[0])
	);
	study_backend_set_status(&backend, "stale");
	study_session_init(&session, 10, 1);

	app_deck_flow_startup_scan(
		TEST_ROOT,
		&index,
		&selected_index,
		&backend,
		&session,
		&session_dirty,
		123,
		4
	);

	assert(index.count == 3);
	assert(index.ignored_count == 0);
	assert(selected_index == 0);
	assert(backend.cards == NULL);
	assert(backend.card_count == 0);
	assert(strcmp(backend.status_text, "Found 3 decks") == 0);
	assert(session_dirty);
	assert(session.scan_completed);
	assert(session.deck_count == 3);
	assert(session.ignored_count == 0);
	assert(session.updated_at == 123);
	assert(session.current_day == 4);
	assert(session.last_event == STUDY_SESSION_EVENT_SCAN);
	assert(session.launch_count == 0);
	assert(session.deck_open_count == 0);
	assert(session.review_screen_count == 0);
	assert(session.load_error_count == 0);
	remove_test_tree();
}

static void test_startup_scan_records_missing_root_as_empty_scan(void)
{
	static const struct study_backend_card stale_cards[] = {
		{ "stale front", "stale back", NULL },
	};
	struct study_deck_index index;
	struct study_backend backend;
	struct study_session session;
	size_t selected_index = 7;
	bool session_dirty = false;

	remove_test_tree();
	study_deck_index_init(&index);
	study_backend_init(
		&backend,
		stale_cards,
		sizeof(stale_cards) / sizeof(stale_cards[0])
	);
	study_session_init(&session, 10, 1);

	app_deck_flow_startup_scan(
		TEST_ROOT,
		&index,
		&selected_index,
		&backend,
		&session,
		&session_dirty,
		456,
		5
	);

	assert(index.count == 0);
	assert(index.ignored_count == 0);
	assert(selected_index == 0);
	assert(backend.cards == NULL);
	assert(backend.card_count == 0);
	assert(strcmp(backend.status_text, "No decks found") == 0);
	assert(session_dirty);
	assert(session.scan_completed);
	assert(session.deck_count == 0);
	assert(session.ignored_count == 0);
	assert(session.updated_at == 456);
	assert(session.current_day == 5);
	assert(session.last_event == STUDY_SESSION_EVENT_SCAN);
}

static void test_rescan_records_session_scan(void)
{
	struct study_deck_index index;
	struct study_backend backend;
	struct study_settings settings;
	struct study_session session;
	struct test_paths paths;
	size_t selected_index = 0;
	bool session_dirty = false;
	bool state_enabled = false;
	bool state_dirty = true;
	enum app_mode mode = APP_MODE_DECK_SELECT;
	enum app_mode exit_return_mode = APP_MODE_REVIEW;

	make_test_tree();
	study_deck_index_init(&index);
	study_backend_init(&backend, NULL, 0);
	study_settings_defaults(&settings);
	study_session_init(&session, 10, 1);
	init_test_paths(&paths);

	assert(app_deck_flow_handle_select_input(
		STUDY_CONTROL_BUTTON_SELECT,
		TEST_ROOT,
		&index,
		&selected_index,
		&backend,
		&settings,
		2,
		&paths.flow_paths,
		&session,
		&session_dirty,
		&state_enabled,
		&state_dirty,
		&mode,
		&exit_return_mode,
		false,
		100,
		2
	));
	assert(session_dirty);
	assert(!state_dirty);
	assert(session.scan_completed);
	assert(session.deck_count == 3);
	assert(session.ignored_count == 0);
	assert(session.updated_at == 100);
	assert(session.current_day == 2);
	assert(session.last_event == STUDY_SESSION_EVENT_SCAN);
	assert(mode == APP_MODE_DECK_SELECT);
	assert(strcmp(backend.status_text, "Found 3 decks") == 0);
	remove_test_tree();
}

static void test_open_deck_loads_state_settings_paths_and_session(void)
{
	struct study_deck_index index;
	struct study_backend backend;
	struct study_settings settings;
	struct study_session session;
	struct test_paths paths;
	size_t selected_index;
	bool session_dirty = false;
	bool state_enabled = false;
	bool state_dirty = false;
	enum app_mode mode = APP_MODE_DECK_SELECT;
	enum app_mode exit_return_mode = APP_MODE_DECK_SELECT;

	make_test_tree();
	study_deck_index_scan(&index, TEST_ROOT);
	selected_index = find_deck_index(&index, "alpha");
	study_backend_init(&backend, NULL, 0);
	study_settings_defaults(&settings);
	study_session_init(&session, 10, 1);
	init_test_paths(&paths);

	assert(app_deck_flow_handle_select_input(
		STUDY_CONTROL_BUTTON_A,
		TEST_ROOT,
		&index,
		&selected_index,
		&backend,
		&settings,
		2,
		&paths.flow_paths,
		&session,
		&session_dirty,
		&state_enabled,
		&state_dirty,
		&mode,
		&exit_return_mode,
		false,
		200,
		2
	));
	assert(mode == APP_MODE_REVIEW);
	assert(session_dirty);
	assert(state_enabled);
	assert(!state_dirty);
	assert(backend.card_count == 2);
	assert(backend.progress_day == 2);
	assert(backend.reviewed_count == 1);
	assert(backend.reviewed_today_count == 0);
	assert(settings.new_limit == 5);
	assert(settings.review_limit == 10);
	assert(settings.learning_mode == STUDY_SETTINGS_LEARNING_CARD_COOLDOWN);
	assert(
		backend.scheduler_policy ==
		STUDY_BACKEND_SCHEDULER_CARD_COUNT_COOLDOWN
	);
	assert(strcmp(paths.deck_id, "alpha") == 0);
	assert(strcmp(paths.state_path, TEST_ROOT "/alpha/state.tsv") == 0);
	assert(strcmp(paths.settings_path, TEST_ROOT "/alpha/settings.tsv") == 0);
	assert(strcmp(paths.review_log_path, TEST_ROOT "/alpha/review-log.tsv") == 0);
	assert(session.deck_open_count == 1);
	assert(session.review_screen_count == 1);
	assert(session.load_error_count == 0);
	assert(session.updated_at == 200);
	assert(session.current_day == 2);
	assert(strcmp(session.last_deck_id, "alpha") == 0);
	assert(session.last_event == STUDY_SESSION_EVENT_DECK_REVIEW);
	remove_test_tree();
}

static void test_open_deck_rollover_save_failure_rolls_back_loaded_state(void)
{
	struct study_deck_index index;
	struct study_backend backend;
	struct study_settings settings;
	struct study_session session;
	struct test_paths paths;
	size_t selected_index;
	bool session_dirty = false;
	bool state_enabled = false;
	bool state_dirty = false;
	enum app_mode mode = APP_MODE_DECK_SELECT;
	enum app_mode exit_return_mode = APP_MODE_DECK_SELECT;

	make_test_tree();
	assert(mkdir(TEST_ROOT "/alpha/state.tsv.tmp", 0700) == 0);
	study_deck_index_scan(&index, TEST_ROOT);
	selected_index = find_deck_index(&index, "alpha");
	study_backend_init(&backend, NULL, 0);
	study_settings_defaults(&settings);
	study_session_init(&session, 10, 1);
	init_test_paths(&paths);

	assert(app_deck_flow_handle_select_input(
		STUDY_CONTROL_BUTTON_A,
		TEST_ROOT,
		&index,
		&selected_index,
		&backend,
		&settings,
		2,
		&paths.flow_paths,
		&session,
		&session_dirty,
		&state_enabled,
		&state_dirty,
		&mode,
		&exit_return_mode,
		false,
		201,
		2
	));
	assert(mode == APP_MODE_REVIEW);
	assert(session_dirty);
	assert(state_enabled);
	assert(!state_dirty);
	assert(backend.progress_day == 1);
	assert(backend.reviewed_today_count == 1);
	assert(backend.completed_today_count == 1);
	assert(strcmp(backend.status_text, "Save failed") == 0);
	assert(session.deck_open_count == 1);
	assert(session.review_screen_count == 1);
	assert(session.load_error_count == 0);
	assert(strcmp(session.last_deck_id, "alpha") == 0);
	assert(session.last_event == STUDY_SESSION_EVENT_DECK_REVIEW);
	remove_test_tree();
}

static void test_open_bad_deck_records_load_error_without_state(void)
{
	struct study_deck_index index;
	struct study_backend backend;
	struct study_settings settings;
	struct study_session session;
	struct test_paths paths;
	size_t selected_index;
	bool session_dirty = false;
	bool state_enabled = true;
	bool state_dirty = true;
	enum app_mode mode = APP_MODE_DECK_SELECT;
	enum app_mode exit_return_mode = APP_MODE_DECK_SELECT;

	make_test_tree();
	study_deck_index_scan(&index, TEST_ROOT);
	selected_index = find_deck_index(&index, "bad");
	study_backend_init(&backend, NULL, 0);
	study_settings_defaults(&settings);
	study_session_init(&session, 10, 1);
	init_test_paths(&paths);

	assert(app_deck_flow_handle_select_input(
		STUDY_CONTROL_BUTTON_A,
		TEST_ROOT,
		&index,
		&selected_index,
		&backend,
		&settings,
		2,
		&paths.flow_paths,
		&session,
		&session_dirty,
		&state_enabled,
		&state_dirty,
		&mode,
		&exit_return_mode,
		false,
		300,
		2
	));
	assert(mode == APP_MODE_REVIEW);
	assert(session_dirty);
	assert(!state_enabled);
	assert(!state_dirty);
	assert(backend.card_count == 0);
	assert(strcmp(backend.status_text, "bad: Bad format") == 0);
	assert(strcmp(paths.deck_id, "bad") == 0);
	assert(strcmp(paths.state_path, "") == 0);
	assert(strcmp(paths.settings_path, "") == 0);
	assert(strcmp(paths.review_log_path, "") == 0);
	assert(session.deck_open_count == 1);
	assert(session.review_screen_count == 0);
	assert(session.load_error_count == 1);
	assert(session.updated_at == 300);
	assert(strcmp(session.last_deck_id, "bad") == 0);
	assert(session.last_event == STUDY_SESSION_EVENT_DECK_LOAD_ERROR);
	remove_test_tree();
}

int main(void)
{
	test_rescan_preserves_selected_deck_and_status();
	test_startup_scan_initializes_backend_and_records_session();
	test_startup_scan_records_missing_root_as_empty_scan();
	test_rescan_records_session_scan();
	test_open_deck_loads_state_settings_paths_and_session();
	test_open_deck_rollover_save_failure_rolls_back_loaded_state();
	test_open_bad_deck_records_load_error_without_state();
	return 0;
}
