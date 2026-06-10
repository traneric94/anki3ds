#include "app_shell.h"

#include <3ds.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app_time.h"
#include "app_power.h"
#include "study_controls.h"

#define TEST_ROOT "/tmp/anki3ds-app-shell-test"
#define TEST_SESSION_PATH TEST_ROOT "/session.tsv"
#define TEST_APP_TIME 1700000000

static const struct study_backend_card test_cards[] = {
	{ "Alpha front", "Alpha back", "tag" },
};

struct fake_ptmu_state
{
	Result init_result;
	Result shell_result;
	Result level_result;
	Result charge_result;
	u8 shell_state;
	u8 level;
	u8 charge_state;
	unsigned int exit_count;
};

static struct fake_ptmu_state fake_ptmu;

Result ptmuInit(void)
{
	return fake_ptmu.init_result;
}

void ptmuExit(void)
{
	fake_ptmu.exit_count++;
}

Result PTMU_GetShellState(u8 *state)
{
	if (state != NULL)
		*state = fake_ptmu.shell_state;
	return fake_ptmu.shell_result;
}

Result PTMU_GetBatteryLevel(u8 *level)
{
	if (level != NULL)
		*level = fake_ptmu.level;
	return fake_ptmu.level_result;
}

Result PTMU_GetBatteryChargeState(u8 *state)
{
	if (state != NULL)
		*state = fake_ptmu.charge_state;
	return fake_ptmu.charge_result;
}

static void reset_fake_ptmu(void)
{
	memset(&fake_ptmu, 0, sizeof(fake_ptmu));
	fake_ptmu.shell_state = 1;
	fake_ptmu.level = 3;
}

static void write_file(const char *path, const char *contents)
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

static void remove_test_tree(void)
{
	(void)remove(TEST_ROOT "/alpha/review-log.tsv");
	(void)remove(TEST_ROOT "/alpha/review-log.tsv.tmp");
	(void)remove(TEST_ROOT "/alpha/review-log.tsv.bak");
	(void)remove(TEST_ROOT "/alpha/state.tsv");
	(void)remove(TEST_ROOT "/alpha/state.tsv.tmp");
	(void)remove(TEST_ROOT "/alpha/state.tsv.bak");
	(void)remove(TEST_ROOT "/alpha/settings.tsv");
	(void)remove(TEST_ROOT "/alpha/cards.tsv");
	(void)remove(TEST_ROOT "/alpha/deck.json");
	(void)rmdir(TEST_ROOT "/alpha");
	(void)remove(TEST_SESSION_PATH);
	(void)remove(TEST_SESSION_PATH ".tmp");
	(void)remove(TEST_SESSION_PATH ".bak");
	(void)rmdir(TEST_ROOT);
}

static void make_test_tree(void)
{
	remove_test_tree();
	assert(mkdir(TEST_ROOT, 0700) == 0);
	assert(mkdir(TEST_ROOT "/alpha", 0700) == 0);
	write_file(
		TEST_ROOT "/alpha/cards.tsv",
		"alpha-1\tnote-1\tAlpha front\tAlpha back\ttag\n"
	);
	write_file(TEST_ROOT "/alpha/deck.json", "{ \"name\": \"Alpha Deck\" }\n");
}

static struct app_shell_paths test_paths(void)
{
	struct app_shell_paths paths = {
		TEST_ROOT,
		TEST_SESSION_PATH,
	};

	return paths;
}

static bool handle_frame(
	struct app_shell *shell,
	unsigned int buttons,
	time_t now,
	size_t max_scroll_offset
)
{
	return app_shell_handle_frame(
		shell,
		buttons,
		0,
		0,
		now,
		max_scroll_offset,
		max_scroll_offset
	);
}

static bool handle_frame_with_sources(
	struct app_shell *shell,
	unsigned int buttons,
	unsigned int dpad_buttons,
	unsigned int cpad_buttons,
	time_t now,
	size_t max_answer_scroll_offset,
	size_t max_front_scroll_offset
)
{
	return app_shell_handle_frame(
		shell,
		buttons,
		dpad_buttons,
		cpad_buttons,
		now,
		max_answer_scroll_offset,
		max_front_scroll_offset
	);
}

static bool review_scroll_is_answer(const struct app_shell *shell)
{
	return app_shell_review_answer_scroll_text(shell)[0] != '\0';
}

static void test_startup_owns_scan_session_and_screen_model(void)
{
	struct app_shell shell;
	struct app_screen_model model;
	struct app_shell_paths paths = test_paths();

	reset_fake_ptmu();
	make_test_tree();
	app_shell_init(&shell, &paths, 100, 101, 100);

	assert(app_shell_redraw_needed(&shell));
	assert(app_shell_screen_kind(&shell) == APP_SCREEN_MODEL_DECK_SELECT);
	app_shell_build_screen_model(&shell, &model);
	assert(model.kind == APP_SCREEN_MODEL_DECK_SELECT);
	assert(strcmp(model.deck_select.status_text, "Found 1 deck") == 0);
	assert(strstr(model.deck_select.list_text, "Alpha Deck") != NULL);
	assert(shell.session.scan_completed);
	assert(shell.session.deck_count == 1);
	assert(shell.session.updated_at == 101);
	assert(shell.session.last_event == STUDY_SESSION_EVENT_SCAN);

	remove_test_tree();
}

static void test_failed_startup_session_save_retries_on_next_frame(void)
{
	struct app_shell shell;
	struct app_shell_paths paths = test_paths();
	struct study_session loaded;

	reset_fake_ptmu();
	make_test_tree();
	assert(mkdir(TEST_SESSION_PATH ".tmp", 0700) == 0);
	app_shell_init(&shell, &paths, 100, 101, 100);

	assert(shell.session_dirty);
	assert(!file_exists(TEST_SESSION_PATH));
	assert(strcmp(shell.backend.status_text, "Session save failed") == 0);

	assert(rmdir(TEST_SESSION_PATH ".tmp") == 0);
	assert(
		!handle_frame(
			&shell,
			0,
			102,
			0
		)
	);

	assert(!shell.session_dirty);
	assert(file_exists(TEST_SESSION_PATH));
	assert(study_session_load_tsv(&loaded, TEST_SESSION_PATH) == STUDY_SESSION_OK);
	assert(loaded.scan_completed);
	assert(loaded.deck_count == 1);
	assert(loaded.ignored_count == 1);
	assert(loaded.updated_at == 101);
	assert(loaded.last_event == STUDY_SESSION_EVENT_SCAN);

	remove_test_tree();
}

static void test_open_deck_moves_to_review_and_supplies_scroll_text(void)
{
	struct app_shell shell;
	struct app_screen_model model;
	struct app_shell_paths paths = test_paths();

	reset_fake_ptmu();
	make_test_tree();
	app_shell_init(
		&shell,
		&paths,
		TEST_APP_TIME,
		TEST_APP_TIME,
		TEST_APP_TIME
	);
	app_shell_mark_drawn(&shell);

	assert(shell.deck_index.entries[0].due_count == 0);
	assert(shell.deck_index.entries[0].new_count == 1);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 1,
			0
		)
	);
	assert(app_shell_redraw_needed(&shell));
	assert(app_shell_screen_kind(&shell) == APP_SCREEN_MODEL_REVIEW);
	assert(strcmp(app_shell_review_scroll_text(&shell), "Alpha front") == 0);
	assert(!review_scroll_is_answer(&shell));
	app_shell_build_screen_model(&shell, &model);
	assert(model.kind == APP_SCREEN_MODEL_REVIEW);
	assert(model.flashcard.has_active_card);
	assert(strcmp(model.flashcard.display_text, "Alpha front") == 0);
	assert(shell.session.deck_open_count == 1);
	assert(shell.session.last_event == STUDY_SESSION_EVENT_DECK_REVIEW);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 2,
			0
		)
	);
	assert(strcmp(app_shell_review_scroll_text(&shell), "Alpha back") == 0);
	assert(review_scroll_is_answer(&shell));
	assert(shell.deck_index.entries[0].due_count == 1);
	assert(shell.deck_index.entries[0].new_count == 0);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X,
			TEST_APP_TIME + 3,
			0
		)
	);
	assert(shell.deck_index.entries[0].due_count == 0);
	assert(shell.deck_index.entries[0].new_count == 0);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_Y,
			TEST_APP_TIME + 4,
			0
		)
	);
	assert(shell.mode == APP_MODE_CONFIRM_RESET);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X,
			TEST_APP_TIME + 5,
			0
		)
	);
	assert(shell.mode == APP_MODE_REVIEW);
	assert(shell.deck_index.entries[0].due_count == 0);
	assert(shell.deck_index.entries[0].new_count == 1);

	remove_test_tree();
}

static void test_review_dpad_scrolls_answer_and_cpad_scrolls_front(void)
{
	struct app_shell shell;
	struct app_shell_paths paths = test_paths();

	reset_fake_ptmu();
	make_test_tree();
	app_shell_init(
		&shell,
		&paths,
		TEST_APP_TIME,
		TEST_APP_TIME,
		TEST_APP_TIME
	);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 1,
			0
		)
	);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 2,
			0
		)
	);
	assert(review_scroll_is_answer(&shell));

	assert(
		!handle_frame_with_sources(
			&shell,
			STUDY_CONTROL_BUTTON_DOWN,
			STUDY_CONTROL_BUTTON_DOWN,
			0,
			TEST_APP_TIME + 3,
			2,
			3
		)
	);
	assert(shell.answer_scroll_offset == 1);
	assert(shell.front_scroll_offset == 0);

	assert(
		!handle_frame_with_sources(
			&shell,
			STUDY_CONTROL_BUTTON_DOWN,
			0,
			STUDY_CONTROL_BUTTON_DOWN,
			TEST_APP_TIME + 4,
			2,
			3
		)
	);
	assert(shell.answer_scroll_offset == 1);
	assert(shell.front_scroll_offset == 1);

	remove_test_tree();
}

static void test_deck_help_does_not_leak_into_review(void)
{
	struct app_shell shell;
	struct app_screen_model model;
	struct app_shell_paths paths = test_paths();

	reset_fake_ptmu();
	make_test_tree();
	app_shell_init(
		&shell,
		&paths,
		TEST_APP_TIME,
		TEST_APP_TIME,
		TEST_APP_TIME
	);
	app_shell_mark_drawn(&shell);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_START,
			TEST_APP_TIME + 1,
			0
		)
	);
	app_shell_build_screen_model(&shell, &model);
	assert(app_shell_screen_kind(&shell) == APP_SCREEN_MODEL_DECK_SELECT);
	assert(strstr(model.deck_select.controls_text, "SELECT: rescan") != NULL);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 2,
			0
		)
	);
	app_shell_build_screen_model(&shell, &model);
	assert(app_shell_screen_kind(&shell) == APP_SCREEN_MODEL_REVIEW);
	assert(strcmp(model.flashcard.help_text, "") == 0);

	remove_test_tree();
}

static void test_review_help_does_not_leak_into_deck_select(void)
{
	struct app_shell shell;
	struct app_screen_model model;
	struct app_shell_paths paths = test_paths();

	reset_fake_ptmu();
	make_test_tree();
	app_shell_init(
		&shell,
		&paths,
		TEST_APP_TIME,
		TEST_APP_TIME,
		TEST_APP_TIME
	);
	app_shell_mark_drawn(&shell);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 1,
			0
		)
	);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_START,
			TEST_APP_TIME + 2,
			0
		)
	);
	app_shell_build_screen_model(&shell, &model);
	assert(app_shell_screen_kind(&shell) == APP_SCREEN_MODEL_REVIEW);
	assert(strstr(model.flashcard.help_text, "A: reveal") != NULL);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_SELECT,
			TEST_APP_TIME + 3,
			0
		)
	);
	app_shell_build_screen_model(&shell, &model);
	assert(app_shell_screen_kind(&shell) == APP_SCREEN_MODEL_DECK_SELECT);
	assert(strcmp(model.deck_select.controls_text, "START: help") == 0);

	remove_test_tree();
}

static void test_start_a_chord_does_not_toggle_help_or_fire_primary_action(void)
{
	struct app_shell shell;
	struct app_screen_model model;
	struct app_shell_paths paths = test_paths();
	unsigned int start_a = STUDY_CONTROL_BUTTON_START | STUDY_CONTROL_BUTTON_A;

	reset_fake_ptmu();
	make_test_tree();
	app_shell_init(
		&shell,
		&paths,
		TEST_APP_TIME,
		TEST_APP_TIME,
		TEST_APP_TIME
	);
	app_shell_mark_drawn(&shell);

	assert(!shell.help_visible);
	assert(
		!handle_frame(
			&shell,
			start_a,
			TEST_APP_TIME + 1,
			0
		)
	);
	assert(shell.mode == APP_MODE_DECK_SELECT);
	assert(!shell.help_visible);
	assert(shell.session.deck_open_count == 0);
	app_shell_build_screen_model(&shell, &model);
	assert(model.kind == APP_SCREEN_MODEL_DECK_SELECT);
	assert(strcmp(model.deck_select.controls_text, "START: help") == 0);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_START,
			TEST_APP_TIME + 2,
			0
		)
	);
	assert(shell.help_visible);
	assert(
		!handle_frame(
			&shell,
			start_a,
			TEST_APP_TIME + 3,
			0
		)
	);
	assert(shell.mode == APP_MODE_DECK_SELECT);
	assert(shell.help_visible);
	assert(shell.session.deck_open_count == 0);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 4,
			0
		)
	);
	assert(shell.mode == APP_MODE_REVIEW);
	assert(!shell.help_visible);
	assert(!review_scroll_is_answer(&shell));
	assert(shell.session.answer_shown_count == 0);

	assert(
		!handle_frame(
			&shell,
			start_a,
			TEST_APP_TIME + 5,
			0
		)
	);
	assert(shell.mode == APP_MODE_REVIEW);
	assert(!shell.help_visible);
	assert(!review_scroll_is_answer(&shell));
	assert(shell.session.answer_shown_count == 0);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_START,
			TEST_APP_TIME + 6,
			0
		)
	);
	assert(shell.help_visible);
	assert(
		!handle_frame(
			&shell,
			start_a,
			TEST_APP_TIME + 7,
			0
		)
	);
	assert(shell.mode == APP_MODE_REVIEW);
	assert(shell.help_visible);
	assert(!review_scroll_is_answer(&shell));
	assert(shell.session.answer_shown_count == 0);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 8,
			0
		)
	);
	assert(review_scroll_is_answer(&shell));
	assert(shell.session.answer_shown_count == 1);

	assert(
		!handle_frame(
			&shell,
			start_a,
			TEST_APP_TIME + 9,
			0
		)
	);
	assert(shell.mode == APP_MODE_REVIEW);
	assert(!shell.help_visible);
	assert(review_scroll_is_answer(&shell));
	assert(shell.backend.reviewed_count == 0);
	assert(shell.session.rating_saved_count == 0);

	remove_test_tree();
}

static void test_failed_reveal_save_rolls_back_visible_progress(void)
{
	struct app_shell shell;
	struct app_shell_paths paths = test_paths();

	reset_fake_ptmu();
	make_test_tree();
	app_shell_init(
		&shell,
		&paths,
		TEST_APP_TIME,
		TEST_APP_TIME,
		TEST_APP_TIME
	);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 1,
			0
		)
	);
	assert(!review_scroll_is_answer(&shell));

	shell.active_state_path[0] = '\0';
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 2,
			0
		)
	);

	assert(shell.mode == APP_MODE_REVIEW);
	assert(!review_scroll_is_answer(&shell));
	assert(strcmp(app_shell_review_scroll_text(&shell), "Alpha front") == 0);
	assert(strcmp(shell.backend.status_text, "Save failed") == 0);
	assert(shell.session.answer_shown_count == 0);
	assert(shell.deck_index.entries[0].due_count == 0);
	assert(shell.deck_index.entries[0].new_count == 1);
	assert(!file_exists(TEST_ROOT "/alpha/state.tsv"));

	remove_test_tree();
}

static void test_failed_rating_save_rolls_back_to_revealed_answer(void)
{
	struct app_shell shell;
	struct app_shell_paths paths = test_paths();

	reset_fake_ptmu();
	make_test_tree();
	app_shell_init(
		&shell,
		&paths,
		TEST_APP_TIME,
		TEST_APP_TIME,
		TEST_APP_TIME
	);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 1,
			0
		)
	);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 2,
			0
		)
	);
	assert(review_scroll_is_answer(&shell));
	assert(strcmp(app_shell_review_scroll_text(&shell), "Alpha back") == 0);
	assert(shell.deck_index.entries[0].due_count == 1);
	assert(shell.deck_index.entries[0].new_count == 0);

	shell.active_state_path[0] = '\0';
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X,
			TEST_APP_TIME + 3,
			0
		)
	);

	assert(shell.mode == APP_MODE_REVIEW);
	assert(review_scroll_is_answer(&shell));
	assert(strcmp(app_shell_review_scroll_text(&shell), "Alpha back") == 0);
	assert(strcmp(shell.backend.status_text, "Save failed") == 0);
	assert(shell.backend.reviewed_count == 0);
	assert(shell.session.rating_saved_count == 0);
	assert(shell.deck_index.entries[0].due_count == 1);
	assert(shell.deck_index.entries[0].new_count == 0);

	remove_test_tree();
}

static void test_mixed_button_chords_do_not_trigger_shell_commands(void)
{
	struct app_shell shell;
	struct app_shell_paths paths = test_paths();

	reset_fake_ptmu();
	make_test_tree();
	app_shell_init(
		&shell,
		&paths,
		TEST_APP_TIME,
		TEST_APP_TIME,
		TEST_APP_TIME
	);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A | STUDY_CONTROL_BUTTON_DOWN,
			TEST_APP_TIME + 1,
			0
		)
	);
	assert(shell.mode == APP_MODE_DECK_SELECT);
	assert(shell.session.deck_open_count == 0);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_Y | STUDY_CONTROL_BUTTON_DOWN,
			TEST_APP_TIME + 2,
			0
		)
	);
	assert(shell.mode == APP_MODE_DECK_SELECT);
	assert(!shell.session.exit_confirmed);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 3,
			0
		)
	);
	assert(shell.mode == APP_MODE_REVIEW);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_Y | STUDY_CONTROL_BUTTON_DOWN,
			TEST_APP_TIME + 4,
			0
		)
	);
	assert(shell.mode == APP_MODE_REVIEW);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X,
			TEST_APP_TIME + 5,
			0
		)
	);
	assert(shell.mode == APP_MODE_SETTINGS);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_RIGHT,
			TEST_APP_TIME + 6,
			0
		)
	);
	assert(shell.draft_settings.new_limit == 50);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X | STUDY_CONTROL_BUTTON_RIGHT,
			TEST_APP_TIME + 7,
			0
		)
	);
	assert(shell.mode == APP_MODE_SETTINGS);
	assert(shell.active_settings.new_limit == STUDY_SETTINGS_DEFAULT_NEW_LIMIT);
	assert(!file_exists(TEST_ROOT "/alpha/settings.tsv"));
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_B,
			TEST_APP_TIME + 8,
			0
		)
	);
	assert(shell.mode == APP_MODE_REVIEW);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_Y,
			TEST_APP_TIME + 9,
			0
		)
	);
	assert(shell.mode == APP_MODE_CONFIRM_RESET);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X | STUDY_CONTROL_BUTTON_DOWN,
			TEST_APP_TIME + 10,
			0
		)
	);
	assert(shell.mode == APP_MODE_CONFIRM_RESET);
	assert(shell.deck_index.entries[0].new_count == 1);
	assert(shell.session.reset_progress_count == 0);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_B,
			TEST_APP_TIME + 11,
			0
		)
	);
	assert(shell.mode == APP_MODE_REVIEW);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A | STUDY_CONTROL_BUTTON_DOWN,
			TEST_APP_TIME + 12,
			0
		)
	);
	assert(!review_scroll_is_answer(&shell));
	assert(shell.session.answer_shown_count == 0);
	assert(!file_exists(TEST_ROOT "/alpha/state.tsv"));

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 13,
			0
		)
	);
	assert(review_scroll_is_answer(&shell));
	assert(shell.session.answer_shown_count == 1);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X | STUDY_CONTROL_BUTTON_DOWN,
			TEST_APP_TIME + 14,
			0
		)
	);
	assert(shell.mode == APP_MODE_REVIEW);
	assert(shell.backend.reviewed_count == 0);
	assert(shell.session.rating_saved_count == 0);
	assert(shell.deck_index.entries[0].due_count == 1);

	remove_test_tree();
}

static void test_low_battery_rating_saves_state_and_skips_review_log(void)
{
	struct app_shell shell;
	struct app_shell_paths paths = test_paths();

	reset_fake_ptmu();
	fake_ptmu.level = APP_POWER_BATTERY_LOW_LEVEL;
	make_test_tree();
	app_shell_init(
		&shell,
		&paths,
		TEST_APP_TIME,
		TEST_APP_TIME,
		TEST_APP_TIME
	);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 1,
			0
		)
	);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 2,
			0
		)
	);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X,
			TEST_APP_TIME + 3,
			0
		)
	);

	assert(strcmp(shell.backend.status_text, "Saved; log skipped; batt low") == 0);
	assert(file_exists(TEST_ROOT "/alpha/state.tsv"));
	assert(!file_exists(TEST_ROOT "/alpha/review-log.tsv"));
	assert(shell.deck_index.entries[0].due_count == 0);
	assert(shell.deck_index.entries[0].new_count == 0);
	assert(shell.session.rating_saved_count == 1);

	remove_test_tree();
}

static void test_low_battery_undo_saves_state_and_skips_review_log(void)
{
	struct app_shell shell;
	struct app_shell_paths paths = test_paths();
	struct study_backend_view view;

	reset_fake_ptmu();
	fake_ptmu.level = APP_POWER_BATTERY_LOW_LEVEL;
	make_test_tree();
	app_shell_init(
		&shell,
		&paths,
		TEST_APP_TIME,
		TEST_APP_TIME,
		TEST_APP_TIME
	);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 1,
			0
		)
	);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 2,
			0
		)
	);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X,
			TEST_APP_TIME + 3,
			0
		)
	);
	assert(shell.session.rating_saved_count == 1);
	assert(shell.session.undo_saved_count == 0);
	assert(file_exists(TEST_ROOT "/alpha/state.tsv"));
	assert(!file_exists(TEST_ROOT "/alpha/review-log.tsv"));

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_B,
			TEST_APP_TIME + 4,
			0
		)
	);
	study_backend_build_view(&shell.backend, &view);
	assert(strcmp(shell.backend.status_text, "Saved; log skipped; batt low") == 0);
	assert(file_exists(TEST_ROOT "/alpha/state.tsv"));
	assert(!file_exists(TEST_ROOT "/alpha/review-log.tsv"));
	assert(shell.session.rating_saved_count == 1);
	assert(shell.session.undo_saved_count == 1);
	assert(shell.backend.reviewed_count == 0);
	assert(shell.deck_index.entries[0].due_count == 1);
	assert(shell.deck_index.entries[0].new_count == 0);
	assert(view.has_active_card);
	assert(!view.answer_visible);
	assert(!view.undo_available);

	remove_test_tree();
}

static void test_low_battery_reveal_saves_introduced_state_without_review_log(void)
{
	struct app_shell shell;
	struct app_shell_paths paths = test_paths();
	struct study_backend restored;
	struct study_backend_view view;
	struct study_session loaded_session;

	reset_fake_ptmu();
	fake_ptmu.level = APP_POWER_BATTERY_LOW_LEVEL;
	make_test_tree();
	app_shell_init(
		&shell,
		&paths,
		TEST_APP_TIME,
		TEST_APP_TIME,
		TEST_APP_TIME
	);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 1,
			0
		)
	);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 2,
			0
		)
	);

	assert(review_scroll_is_answer(&shell));
	assert(shell.session.answer_shown_count == 1);
	assert(!shell.session_dirty);
	assert(file_exists(TEST_ROOT "/alpha/state.tsv"));
	assert(!file_exists(TEST_ROOT "/alpha/review-log.tsv"));
	assert(strstr(shell.backend.status_text, "Answer") != NULL);
	assert(strstr(shell.backend.status_text, "batt low") != NULL);

	study_backend_init(&restored, test_cards, 1);
	assert(
		study_backend_load_state_tsv(
			&restored,
			TEST_ROOT "/alpha/state.tsv"
		) == STUDY_BACKEND_STATE_OK
	);
	study_backend_build_view(&restored, &view);
	assert(view.has_active_card);
	assert(!view.answer_visible);
	assert(view.current_card_introduced);
	assert(view.introduced_count == 1);
	assert(view.introduced_today_count == 1);

	assert(
		study_session_load_tsv(
			&loaded_session,
			TEST_SESSION_PATH
		) == STUDY_SESSION_OK
	);
	assert(loaded_session.answer_shown_count == 1);
	assert(loaded_session.last_event == STUDY_SESSION_EVENT_ANSWER_SHOWN);

	remove_test_tree();
}

static void test_review_again_persists_without_resetting_progress_or_settings(void)
{
	struct app_shell shell;
	struct app_shell_paths paths = test_paths();
	struct study_backend restored;
	struct study_backend_view view;
	struct study_settings settings;
	struct study_settings loaded_settings;

	reset_fake_ptmu();
	make_test_tree();
	study_settings_defaults(&settings);
	settings.new_limit = 50;
	settings.review_limit = 100;
	assert(
		study_settings_save_tsv(
			&settings,
			TEST_ROOT "/alpha/settings.tsv"
		) == STUDY_SETTINGS_OK
	);
	app_shell_init(
		&shell,
		&paths,
		TEST_APP_TIME,
		TEST_APP_TIME,
		TEST_APP_TIME
	);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 1,
			0
		)
	);
	assert(shell.active_settings.new_limit == 50);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 2,
			0
		)
	);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X,
			TEST_APP_TIME + 3,
			0
		)
	);
	study_backend_build_view(&shell.backend, &view);
	assert(!view.has_active_card);
	assert(view.reviewed_count == 1);
	assert(view.introduced_count == 1);
	assert(file_exists(TEST_ROOT "/alpha/state.tsv"));

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 4,
			0
		)
	);
	study_backend_build_view(&shell.backend, &view);
	assert(view.has_active_card);
	assert(!view.answer_visible);
	assert(view.current_card_introduced);
	assert(view.reviewed_count == 1);
	assert(view.reviewed_today_count == 0);
	assert(view.introduced_count == 1);
	assert(shell.active_settings.new_limit == 50);
	assert(shell.active_settings.review_limit == 100);

	study_backend_init(&restored, test_cards, 1);
	assert(
		study_backend_load_state_tsv(
			&restored,
			TEST_ROOT "/alpha/state.tsv"
		) == STUDY_BACKEND_STATE_OK
	);
	study_backend_build_view(&restored, &view);
	assert(view.has_active_card);
	assert(!view.answer_visible);
	assert(view.current_card_introduced);
	assert(view.reviewed_count == 1);
	assert(view.reviewed_today_count == 0);
	assert(
		study_settings_load_tsv(
			&loaded_settings,
			TEST_ROOT "/alpha/settings.tsv"
		) == STUDY_SETTINGS_OK
	);
	assert(loaded_settings.new_limit == 50);
	assert(loaded_settings.review_limit == 100);

	remove_test_tree();
}

static void test_low_battery_suspend_restore_saves_state_and_skips_review_log(void)
{
	struct app_shell shell;
	struct app_shell_paths paths = test_paths();
	struct study_backend_view view;

	reset_fake_ptmu();
	fake_ptmu.level = APP_POWER_BATTERY_LOW_LEVEL;
	make_test_tree();
	app_shell_init(
		&shell,
		&paths,
		TEST_APP_TIME,
		TEST_APP_TIME,
		TEST_APP_TIME
	);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 1,
			0
		)
	);
	assert(shell.mode == APP_MODE_REVIEW);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_R,
			TEST_APP_TIME + 2,
			0
		)
	);
	assert(shell.mode == APP_MODE_CONFIRM_SUSPEND);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X,
			TEST_APP_TIME + 3,
			0
		)
	);
	study_backend_build_view(&shell.backend, &view);
	assert(shell.mode == APP_MODE_REVIEW);
	assert(strcmp(shell.backend.status_text, "Saved; log skipped; batt low") == 0);
	assert(file_exists(TEST_ROOT "/alpha/state.tsv"));
	assert(!file_exists(TEST_ROOT "/alpha/review-log.tsv"));
	assert(shell.session.suspend_saved_count == 1);
	assert(shell.deck_index.entries[0].suspended_count == 1);
	assert(view.suspended_count == 1);
	assert(!view.has_active_card);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_R,
			TEST_APP_TIME + 4,
			0
		)
	);
	assert(shell.mode == APP_MODE_CONFIRM_RESTORE);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X,
			TEST_APP_TIME + 5,
			0
		)
	);
	study_backend_build_view(&shell.backend, &view);
	assert(shell.mode == APP_MODE_REVIEW);
	assert(strcmp(shell.backend.status_text, "Saved; log skipped; batt low") == 0);
	assert(file_exists(TEST_ROOT "/alpha/state.tsv"));
	assert(!file_exists(TEST_ROOT "/alpha/review-log.tsv"));
	assert(shell.session.restore_saved_count == 1);
	assert(shell.deck_index.entries[0].suspended_count == 0);
	assert(view.suspended_count == 0);
	assert(view.has_active_card);

	remove_test_tree();
}

static void test_low_battery_reset_clears_progress_and_preserves_settings(void)
{
	struct app_shell shell;
	struct app_shell_paths paths = test_paths();
	struct study_backend_view view;
	struct study_settings loaded;

	reset_fake_ptmu();
	fake_ptmu.level = APP_POWER_BATTERY_LOW_LEVEL;
	make_test_tree();
	app_shell_init(
		&shell,
		&paths,
		TEST_APP_TIME,
		TEST_APP_TIME,
		TEST_APP_TIME
	);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 1,
			0
		)
	);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X,
			TEST_APP_TIME + 2,
			0
		)
	);
	assert(shell.mode == APP_MODE_SETTINGS);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_RIGHT,
			TEST_APP_TIME + 3,
			0
		)
	);
	assert(shell.draft_settings.new_limit == 50);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X,
			TEST_APP_TIME + 4,
			0
		)
	);
	assert(shell.mode == APP_MODE_REVIEW);
	assert(shell.active_settings.new_limit == 50);
	assert(file_exists(TEST_ROOT "/alpha/settings.tsv"));

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 5,
			0
		)
	);
	assert(review_scroll_is_answer(&shell));
	assert(file_exists(TEST_ROOT "/alpha/state.tsv"));
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X,
			TEST_APP_TIME + 6,
			0
		)
	);
	assert(shell.backend.reviewed_count == 1);
	assert(shell.session.rating_saved_count == 1);
	assert(file_exists(TEST_ROOT "/alpha/state.tsv"));
	assert(!file_exists(TEST_ROOT "/alpha/review-log.tsv"));

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_Y,
			TEST_APP_TIME + 7,
			0
		)
	);
	assert(shell.mode == APP_MODE_CONFIRM_RESET);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X,
			TEST_APP_TIME + 8,
			0
		)
	);
	study_backend_build_view(&shell.backend, &view);
	assert(shell.mode == APP_MODE_REVIEW);
	assert(strstr(shell.backend.status_text, "Progress reset") != NULL);
	assert(strstr(shell.backend.status_text, "batt low") != NULL);
	assert(!file_exists(TEST_ROOT "/alpha/state.tsv"));
	assert(!file_exists(TEST_ROOT "/alpha/review-log.tsv"));
	assert(file_exists(TEST_ROOT "/alpha/settings.tsv"));
	assert(
		study_settings_load_tsv(
			&loaded,
			TEST_ROOT "/alpha/settings.tsv"
		) == STUDY_SETTINGS_OK
	);
	assert(loaded.new_limit == 50);
	assert(shell.active_settings.new_limit == 50);
	assert(shell.session.reset_progress_count == 1);
	assert(shell.deck_index.entries[0].new_count == 1);
	assert(shell.deck_index.entries[0].due_count == 0);
	assert(shell.deck_index.entries[0].suspended_count == 0);
	assert(view.reviewed_count == 0);
	assert(view.suspended_count == 0);
	assert(view.has_active_card);

	remove_test_tree();
}

static void test_low_battery_day_rollover_persists_reset_daily_counts(void)
{
	struct app_shell shell;
	struct app_shell_paths paths = test_paths();
	struct study_backend restored;
	unsigned int previous_day;
	time_t next_day_time;

	reset_fake_ptmu();
	fake_ptmu.level = APP_POWER_BATTERY_LOW_LEVEL;
	make_test_tree();
	app_shell_init(
		&shell,
		&paths,
		TEST_APP_TIME,
		TEST_APP_TIME,
		TEST_APP_TIME
	);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 1,
			0
		)
	);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 2,
			0
		)
	);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X,
			TEST_APP_TIME + 3,
			0
		)
	);
	assert(shell.backend.reviewed_today_count == 1);
	assert(shell.backend.introduced_today_count == 1);
	assert(file_exists(TEST_ROOT "/alpha/state.tsv"));
	assert(shell.deck_index.entries[0].due_count == 0);
	assert(shell.deck_index.entries[0].new_count == 0);

	previous_day = shell.current_day;
	next_day_time = TEST_APP_TIME + (time_t)(2 * 24 * 60 * 60);
	assert(app_time_local_day_from_time(next_day_time) > previous_day);
	assert(
		!handle_frame(
			&shell,
			0,
			next_day_time,
			0
		)
	);

	assert(shell.current_day == app_time_local_day_from_time(next_day_time));
	assert(shell.session.current_day == shell.current_day);
	assert(!shell.session_dirty);
	assert(shell.backend.progress_day == shell.current_day);
	assert(shell.backend.reviewed_today_count == 0);
	assert(shell.backend.introduced_today_count == 0);
	assert(shell.backend.completed_today_count == 0);
	assert(strcmp(shell.backend.status_text, "New day; limits reset; batt low") == 0);
	assert(shell.deck_index.entries[0].due_count == 1);
	assert(shell.deck_index.entries[0].new_count == 0);
	assert(file_exists(TEST_ROOT "/alpha/state.tsv"));
	assert(!file_exists(TEST_ROOT "/alpha/review-log.tsv"));

	study_backend_init(&restored, shell.backend.cards, shell.backend.card_count);
	assert(
		study_backend_load_state_tsv(
			&restored,
			TEST_ROOT "/alpha/state.tsv"
		) == STUDY_BACKEND_STATE_OK
	);
	assert(restored.progress_day == shell.current_day);
	assert(restored.reviewed_today_count == 0);
	assert(restored.introduced_today_count == 0);
	assert(restored.completed_today_count == 0);

	remove_test_tree();
}

static void test_low_battery_settings_save_persists_settings_with_warning(void)
{
	struct app_shell shell;
	struct app_shell_paths paths = test_paths();
	struct study_settings loaded;

	reset_fake_ptmu();
	fake_ptmu.level = APP_POWER_BATTERY_LOW_LEVEL;
	make_test_tree();
	app_shell_init(
		&shell,
		&paths,
		TEST_APP_TIME,
		TEST_APP_TIME,
		TEST_APP_TIME
	);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 1,
			0
		)
	);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X,
			TEST_APP_TIME + 2,
			0
		)
	);
	assert(shell.mode == APP_MODE_SETTINGS);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_RIGHT,
			TEST_APP_TIME + 3,
			0
		)
	);
	assert(shell.draft_settings.new_limit == 50);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_X,
			TEST_APP_TIME + 4,
			0
		)
	);

	assert(shell.mode == APP_MODE_REVIEW);
	assert(shell.active_settings.new_limit == 50);
	assert(file_exists(TEST_ROOT "/alpha/settings.tsv"));
	assert(
		study_settings_load_tsv(
			&loaded,
			TEST_ROOT "/alpha/settings.tsv"
		) == STUDY_SETTINGS_OK
	);
	assert(loaded.new_limit == 50);
	assert(strstr(shell.backend.status_text, "Settings saved") != NULL);
	assert(strstr(shell.backend.status_text, "batt low") != NULL);
	assert(strstr(shell.backend.status_text, "unsaved changes") == NULL);
	assert(shell.session.settings_saved_count == 1);

	remove_test_tree();
}

static void test_failed_confirmed_exit_session_save_keeps_exit_confirmation(void)
{
	struct app_shell shell;
	struct app_shell_paths paths = test_paths();
	struct study_session loaded;

	reset_fake_ptmu();
	make_test_tree();
	app_shell_init(
		&shell,
		&paths,
		TEST_APP_TIME,
		TEST_APP_TIME,
		TEST_APP_TIME
	);
	assert(!shell.session_dirty);
	assert(file_exists(TEST_SESSION_PATH));

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_Y,
			TEST_APP_TIME + 1,
			0
		)
	);
	assert(shell.mode == APP_MODE_CONFIRM_EXIT);

	assert(mkdir(TEST_SESSION_PATH ".tmp", 0700) == 0);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 2,
			0
		)
	);
	assert(shell.mode == APP_MODE_CONFIRM_EXIT);
	assert(!shell.session_dirty);
	assert(!shell.session.exit_confirmed);
	assert(strcmp(shell.backend.status_text, "Session save failed") == 0);

	assert(rmdir(TEST_SESSION_PATH ".tmp") == 0);
	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_B,
			TEST_APP_TIME + 3,
			0
		)
	);
	assert(shell.mode == APP_MODE_DECK_SELECT);
	assert(!shell.session_dirty);
	assert(study_session_load_tsv(&loaded, TEST_SESSION_PATH) == STUDY_SESSION_OK);
	assert(!loaded.exit_confirmed);
	assert(loaded.last_event == STUDY_SESSION_EVENT_SCAN);

	assert(
		!handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_Y,
			TEST_APP_TIME + 4,
			0
		)
	);
	assert(shell.mode == APP_MODE_CONFIRM_EXIT);
	assert(
		handle_frame(
			&shell,
			STUDY_CONTROL_BUTTON_A,
			TEST_APP_TIME + 5,
			0
		)
	);
	assert(!shell.session_dirty);
	assert(study_session_load_tsv(&loaded, TEST_SESSION_PATH) == STUDY_SESSION_OK);
	assert(loaded.exit_confirmed);
	assert(loaded.last_event == STUDY_SESSION_EVENT_EXIT_CONFIRMED);
	assert(loaded.updated_at == (unsigned long)(TEST_APP_TIME + 5));

	remove_test_tree();
}

static void test_app_shell_shutdown_owns_battery_shutdown(void)
{
	struct app_shell shell;
	struct app_shell_paths paths = test_paths();

	reset_fake_ptmu();
	make_test_tree();
	app_shell_init(&shell, &paths, 100, 101, 100);
	app_shell_shutdown(&shell);

	assert(fake_ptmu.exit_count == 1);
	remove_test_tree();
}

int main(void)
{
	test_startup_owns_scan_session_and_screen_model();
	test_failed_startup_session_save_retries_on_next_frame();
	test_open_deck_moves_to_review_and_supplies_scroll_text();
	test_review_dpad_scrolls_answer_and_cpad_scrolls_front();
	test_deck_help_does_not_leak_into_review();
	test_review_help_does_not_leak_into_deck_select();
	test_start_a_chord_does_not_toggle_help_or_fire_primary_action();
	test_failed_reveal_save_rolls_back_visible_progress();
	test_failed_rating_save_rolls_back_to_revealed_answer();
	test_mixed_button_chords_do_not_trigger_shell_commands();
	test_low_battery_rating_saves_state_and_skips_review_log();
	test_low_battery_undo_saves_state_and_skips_review_log();
	test_low_battery_reveal_saves_introduced_state_without_review_log();
	test_review_again_persists_without_resetting_progress_or_settings();
	test_low_battery_suspend_restore_saves_state_and_skips_review_log();
	test_low_battery_reset_clears_progress_and_preserves_settings();
	test_low_battery_day_rollover_persists_reset_daily_counts();
	test_low_battery_settings_save_persists_settings_with_warning();
	test_failed_confirmed_exit_session_save_keeps_exit_confirmation();
	test_app_shell_shutdown_owns_battery_shutdown();
	return 0;
}
