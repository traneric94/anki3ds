#include "app_flashcard_contract.h"
#include "app_text.h"
#include "study_backend.h"
#include "study_settings.h"

#include <assert.h>
#include <string.h>

static void test_text_window_copies_visible_wrapped_rows(void)
{
	char window[32];

	app_text_copy_wrapped_window(
		"abcdefghij",
		4,
		1,
		2,
		window,
		sizeof(window)
	);

	assert(strcmp(window, "efgh\nij") == 0);
}

static void test_text_window_wraps_on_spaces_when_possible(void)
{
	char window[48];

	assert(app_text_wrapped_row_count("alpha beta", 7) == 2);
	app_text_copy_wrapped_window(
		"alpha beta",
		7,
		0,
		2,
		window,
		sizeof(window)
	);
	assert(strcmp(window, "alpha\nbeta") == 0);

	app_text_copy_wrapped_window(
		"a bc",
		3,
		0,
		2,
		window,
		sizeof(window)
	);
	assert(strcmp(window, "a\nbc") == 0);
}

static void test_text_window_keeps_next_word_intact(void)
{
	char window[48];

	app_text_copy_wrapped_window(
		"red blue",
		6,
		0,
		2,
		window,
		sizeof(window)
	);

	assert(strcmp(window, "red\nblue") == 0);
}

static void test_text_window_prefers_earlier_space_over_word_cutoff(void)
{
	char window[80];

	app_text_copy_wrapped_window(
		"Front question needs readable breaks",
		18,
		0,
		3,
		window,
		sizeof(window)
	);

	assert(strcmp(window, "Front question\nneeds readable\nbreaks") == 0);
}

static void test_text_view_copies_backend_strings_without_layout(void)
{
	char front[] = "Mutable prompt text";
	char back[] = "Mutable answer text";
	char tags[] = "mutable tags";
	struct study_backend_card cards[] = {
		{front, back, tags}
	};
	struct study_backend backend;
	struct study_backend_view view;
	struct app_flashcard_text_view text_view;

	study_backend_init(&backend, cards, 1);
	study_backend_set_status(&backend, "Custom status");
	study_backend_build_view(&backend, &view);
	app_flashcard_text_view_build(&text_view, &view, NULL, false);

	assert(text_view.has_active_card);
	assert(strcmp(text_view.title_text, "Question") == 0);
	assert(strcmp(text_view.front_text, "Mutable prompt text") == 0);
	assert(strcmp(text_view.back_text, "Mutable answer text") == 0);
	assert(strcmp(text_view.display_text, "Mutable prompt text") == 0);
	assert(strstr(text_view.progress_text, "Card 1/1") != NULL);
	assert(strstr(text_view.progress_text, "Susp 0") != NULL);
	assert(strstr(text_view.progress_text, "Scroll") == NULL);
	assert(strcmp(text_view.status_text, "Custom status") == 0);

	front[0] = 'X';
	back[0] = 'X';
	tags[0] = 'X';
	view.status_text[0] = 'X';

	assert(strcmp(text_view.front_text, "Mutable prompt text") == 0);
	assert(strcmp(text_view.back_text, "Mutable answer text") == 0);
	assert(strcmp(text_view.status_text, "Custom status") == 0);
}

static void test_text_view_exposes_front_back_and_display_strings(void)
{
	struct study_backend_card cards[] = {
		{"Front text", "Back text", NULL}
	};
	struct study_backend backend;
	struct study_backend_view view;
	struct app_flashcard_text_view text_view;

	study_backend_init(&backend, cards, 1);
	study_backend_build_view(&backend, &view);
	app_flashcard_text_view_build(&text_view, &view, NULL, false);

	assert(text_view.has_active_card);
	assert(!text_view.answer_visible);
	assert(!text_view.undo_available);
	assert(strcmp(text_view.title_text, "Question") == 0);
	assert(strcmp(text_view.front_text, "Front text") == 0);
	assert(strcmp(text_view.back_text, "Back text") == 0);
	assert(strcmp(text_view.display_text, "Front text") == 0);
	assert(strcmp(text_view.status_text, "Question") == 0);
	assert(strstr(text_view.progress_text, "Card 1/1") != NULL);
	assert(strstr(text_view.progress_text, "Scroll") == NULL);
	assert(strcmp(text_view.help_text, "") == 0);

	assert(study_backend_show_answer(&backend));
	study_backend_build_view(&backend, &view);
	app_flashcard_text_view_build(&text_view, &view, NULL, false);

	assert(text_view.has_active_card);
	assert(text_view.answer_visible);
	assert(strcmp(text_view.title_text, "Answer") == 0);
	assert(strcmp(text_view.front_text, "Front text") == 0);
	assert(strcmp(text_view.back_text, "Back text") == 0);
	assert(strcmp(text_view.display_text, "Back text") == 0);
	assert(strcmp(text_view.status_text, "Answer") == 0);
	assert(strcmp(text_view.help_text, "") == 0);
}

static void test_revealed_text_view_keeps_top_front_and_bottom_back_strings(void)
{
	struct study_backend_card cards[] = {
		{"Top prompt stays visible", "Bottom answer is revealed", NULL}
	};
	struct study_backend backend;
	struct study_backend_view view;
	struct app_flashcard_text_view text_view;

	study_backend_init(&backend, cards, 1);
	assert(study_backend_show_answer(&backend));
	study_backend_build_view(&backend, &view);
	app_flashcard_text_view_build(&text_view, &view, NULL, false);

	assert(text_view.answer_visible);
	assert(strcmp(text_view.front_text, "Top prompt stays visible") == 0);
	assert(strcmp(text_view.back_text, "Bottom answer is revealed") == 0);
	assert(strcmp(text_view.display_text, "Bottom answer is revealed") == 0);
	assert(strcmp(text_view.help_text, "") == 0);
}

static void test_revealed_empty_back_uses_no_answer_placeholder(void)
{
	struct study_backend_card cards[] = {
		{"Prompt with no source back", "", NULL}
	};
	struct study_backend backend;
	struct study_backend_view view;
	struct app_flashcard_text_view text_view;

	study_backend_init(&backend, cards, 1);
	study_backend_build_view(&backend, &view);
	app_flashcard_text_view_build(&text_view, &view, NULL, false);

	assert(strcmp(text_view.back_text, "") == 0);
	assert(strcmp(text_view.display_text, "Prompt with no source back") == 0);

	assert(study_backend_show_answer(&backend));
	study_backend_build_view(&backend, &view);
	app_flashcard_text_view_build(&text_view, &view, NULL, false);

	assert(text_view.answer_visible);
	assert(strcmp(text_view.front_text, "Prompt with no source back") == 0);
	assert(strcmp(text_view.back_text, APP_FLASHCARD_NO_ANSWER_TEXT) == 0);
	assert(strcmp(text_view.display_text, APP_FLASHCARD_NO_ANSWER_TEXT) == 0);
}

static void test_text_view_for_empty_session_is_string_only(void)
{
	struct study_backend backend;
	struct study_backend_view view;
	struct app_flashcard_text_view text_view;

	study_backend_init(&backend, NULL, 0);
	study_backend_build_view(&backend, &view);
	app_flashcard_text_view_build(&text_view, &view, NULL, false);

	assert(!text_view.has_active_card);
	assert(!text_view.answer_visible);
	assert(strcmp(text_view.title_text, "Question") == 0);
	assert(strcmp(text_view.front_text, "") == 0);
	assert(strcmp(text_view.back_text, "") == 0);
	assert(strcmp(text_view.display_text, "No cards loaded.") == 0);
	assert(strcmp(text_view.progress_text, "No active card") == 0);
	assert(strcmp(text_view.status_text, "No cards loaded") == 0);
	assert(strcmp(text_view.help_text, "") == 0);
	assert(strstr(text_view.footer_text, "Today New 0/") != NULL);
}

static void test_text_view_preserves_long_raw_display_text(void)
{
	struct study_backend_card cards[] = {
		{
			"abcdefghijklmnopqrstuvwxyz0123456789row1\n"
			"row2\n"
			"row3\n"
			"row4\n"
			"row5\n"
			"row6",
			"Back",
			NULL
		}
	};
	struct study_backend backend;
	struct study_backend_view view;
	struct study_settings settings;
	struct app_flashcard_text_view text_view;

	study_settings_defaults(&settings);
	settings.new_limit = 5;
	settings.review_limit = 10;
	study_backend_init(&backend, cards, 1);
	study_backend_build_view(&backend, &view);
	app_flashcard_text_view_build(&text_view, &view, &settings, false);

	assert(text_view.has_active_card);
	assert(strcmp(text_view.title_text, "Question") == 0);
	assert(strcmp(text_view.display_text, cards[0].front) == 0);
	assert(strstr(text_view.display_text, "6789row1\nrow2") != NULL);
	assert(strstr(text_view.progress_text, "Card 1/1") != NULL);
	assert(strstr(text_view.progress_text, "Scroll") == NULL);
	assert(strcmp(text_view.status_text, "Question") == 0);
	assert(strcmp(text_view.help_text, "") == 0);
	assert(strstr(text_view.footer_text, "Today New 0/5") != NULL);
	assert(strstr(text_view.footer_text, "Review 0/10") != NULL);
}

static void test_answer_text_view_switches_actions(void)
{
	struct study_backend_card cards[] = {
		{"Front", "Readable answer text", NULL}
	};
	struct study_backend backend;
	struct study_backend_view view;
	struct app_flashcard_text_view text_view;

	study_backend_init(&backend, cards, 1);
	assert(study_backend_show_answer(&backend));
	study_backend_build_view(&backend, &view);
	app_flashcard_text_view_build(&text_view, &view, NULL, false);

	assert(text_view.has_active_card);
	assert(strcmp(text_view.title_text, "Answer") == 0);
	assert(strcmp(text_view.display_text, cards[0].back) == 0);
	assert(strcmp(text_view.status_text, "Answer") == 0);
	assert(strcmp(text_view.help_text, "") == 0);
	assert(strstr(text_view.footer_text, "Today New 1/") != NULL);

	app_flashcard_text_view_build(&text_view, &view, NULL, true);
	assert(strstr(text_view.help_text, "A: Again") != NULL);
	assert(strstr(text_view.help_text, "L: Hard") != NULL);
	assert(strstr(text_view.help_text, "X: Good") != NULL);
	assert(strstr(text_view.help_text, "Y: Easy") != NULL);
	assert(strstr(text_view.help_text, "B: undo") != NULL);
	assert(strstr(text_view.help_text, "START: hide help") != NULL);
	assert(strstr(text_view.help_text, "D-pad ↑/↓: answer") != NULL);
	assert(strstr(text_view.help_text, "Circle ↑/↓: question") != NULL);
}

static void test_revealed_text_view_shows_tags_in_footer(void)
{
	struct study_backend_card cards[] = {
		{"Front", "Answer", "recs algorithms"}
	};
	struct study_backend backend;
	struct study_backend_view view;
	struct app_flashcard_text_view text_view;

	study_backend_init(&backend, cards, 1);
	study_backend_build_view(&backend, &view);
	app_flashcard_text_view_build(&text_view, &view, NULL, false);
	assert(strcmp(text_view.tags_text, "recs algorithms") == 0);
	assert(strstr(text_view.footer_text, "Today New") != NULL);

	assert(study_backend_show_answer(&backend));
	study_backend_build_view(&backend, &view);
	app_flashcard_text_view_build(&text_view, &view, NULL, false);

	assert(text_view.answer_visible);
	assert(strcmp(text_view.tags_text, "recs algorithms") == 0);
	assert(strcmp(text_view.footer_text, "recs algorithms") == 0);
}

static void test_completed_text_view_uses_review_controls(void)
{
	struct study_backend_card cards[] = {
		{"Front", "Back", NULL}
	};
	struct study_backend backend;
	struct study_backend_view view;
	struct app_flashcard_text_view text_view;

	study_backend_init(&backend, cards, 1);
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	study_backend_build_view(&backend, &view);
	app_flashcard_text_view_build(&text_view, &view, NULL, false);

	assert(!text_view.has_active_card);
	assert(strcmp(text_view.title_text, "Question") == 0);
	assert(strcmp(text_view.display_text, "Session complete.") == 0);
	assert(strcmp(text_view.status_text, "Complete") == 0);
	assert(strcmp(text_view.help_text, "") == 0);
	assert(strstr(text_view.footer_text, "Review 1/") != NULL);
}

static void test_no_card_text_view_outputs_only_strings_and_flags(void)
{
	struct study_backend backend;
	struct study_backend_view view;
	struct app_flashcard_text_view text_view;

	study_backend_init(&backend, NULL, 0);
	study_backend_build_view(&backend, &view);
	app_flashcard_text_view_build(&text_view, &view, NULL, false);

	assert(!text_view.has_active_card);
	assert(strcmp(text_view.title_text, "Question") == 0);
	assert(strcmp(text_view.display_text, "No cards loaded.") == 0);
	assert(strcmp(text_view.progress_text, "No active card") == 0);
	assert(strcmp(text_view.status_text, "No cards loaded") == 0);
	assert(strcmp(text_view.help_text, "") == 0);
	assert(strstr(text_view.footer_text, "Today New 0/") != NULL);
	assert(strstr(text_view.footer_text, "Review 0/") != NULL);
}

static void test_help_visible_expands_review_legend(void)
{
	struct study_backend_card cards[] = {
		{"Front", "Back", NULL}
	};
	struct study_backend backend;
	struct study_backend_view view;
	struct app_flashcard_text_view text_view;

	study_backend_init(&backend, cards, 1);
	study_backend_build_view(&backend, &view);
	app_flashcard_text_view_build(&text_view, &view, NULL, true);

	assert(strstr(text_view.help_text, "A: reveal") != NULL);
	assert(strstr(text_view.help_text, "START: hide help") != NULL);
	assert(strstr(text_view.help_text, "X: settings") != NULL);
	assert(strstr(text_view.help_text, "Y: restart deck") != NULL);
	assert(strstr(text_view.help_text, "Circle ↑/↓: question") != NULL);
	assert(strstr(text_view.help_text, "B: undo") != NULL);
	assert(strstr(text_view.help_text, "R: suspend") != NULL);
}

static void test_completed_help_surfaces_review_again_restart_and_settings(void)
{
	struct study_backend_card cards[] = {
		{"Front", "Back", NULL}
	};
	struct study_backend backend;
	struct study_backend_view view;
	struct app_flashcard_text_view text_view;

	study_backend_init(&backend, cards, 1);
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	study_backend_build_view(&backend, &view);
	app_flashcard_text_view_build(&text_view, &view, NULL, true);

	assert(!text_view.has_active_card);
	assert(strstr(text_view.help_text, "A: review again") != NULL);
	assert(strstr(text_view.help_text, "Y: restart deck") != NULL);
	assert(strstr(text_view.help_text, "X: settings") != NULL);
	assert(strstr(text_view.help_text, "SELECT: decks") != NULL);
}

int main(void)
{
	test_text_window_copies_visible_wrapped_rows();
	test_text_window_wraps_on_spaces_when_possible();
	test_text_window_keeps_next_word_intact();
	test_text_window_prefers_earlier_space_over_word_cutoff();
	test_text_view_copies_backend_strings_without_layout();
	test_text_view_exposes_front_back_and_display_strings();
	test_revealed_text_view_keeps_top_front_and_bottom_back_strings();
	test_revealed_empty_back_uses_no_answer_placeholder();
	test_text_view_for_empty_session_is_string_only();
	test_text_view_preserves_long_raw_display_text();
	test_answer_text_view_switches_actions();
	test_revealed_text_view_shows_tags_in_footer();
	test_completed_text_view_uses_review_controls();
	test_no_card_text_view_outputs_only_strings_and_flags();
	test_help_visible_expands_review_legend();
	test_completed_help_surfaces_review_again_restart_and_settings();
	return 0;
}
