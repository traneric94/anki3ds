#include "app_flashcard_contract.h"

#include "study_settings.h"

#include <stdio.h>
#include <string.h>

static void app_flashcard_copy_string(
	char *destination,
	size_t destination_size,
	const char *source
)
{
	if (destination == NULL || destination_size == 0)
		return;

	snprintf(destination, destination_size, "%s", source != NULL ? source : "");
}

static void app_flashcard_format_limit_value(
	char *destination,
	size_t destination_size,
	unsigned int value
)
{
	if (destination == NULL || destination_size == 0)
		return;

	if (value == 0)
	{
		snprintf(destination, destination_size, "All");
		return;
	}

	snprintf(destination, destination_size, "%u", value);
}

static void app_flashcard_build_meta_text(
	char *destination,
	size_t destination_size,
	const struct study_backend_view *view,
	bool include_scroll,
	size_t scroll_offset
)
{
	(void)include_scroll;
	(void)scroll_offset;

	if (destination == NULL || destination_size == 0)
		return;

	if (view != NULL && view->has_active_card)
	{
		snprintf(
			destination,
			destination_size,
			"Card %lu/%lu  Seen %u  Reviewed %u  Susp %u",
			(unsigned long)(view->card_index + 1),
			(unsigned long)view->card_count,
			view->introduced_count,
			view->reviewed_count,
			view->suspended_count
		);
		return;
	}

	snprintf(destination, destination_size, "No active card");
}

static void app_flashcard_build_controls_text(
	char *destination,
	size_t destination_size,
	const struct study_backend_view *view,
	bool help_visible
)
{
	if (destination == NULL || destination_size == 0)
		return;

	destination[0] = '\0';
	if (!help_visible)
		return;

	if (view == NULL || !view->has_active_card)
	{
		if (view != NULL && view->suspended_count > 0)
		{
			snprintf(
				destination,
				destination_size,
				"A: review again   START: hide help\n"
				"X: settings   Y: restart deck\n"
				"R: restore suspended\n"
				"SELECT: decks"
			);
			return;
		}

		if (view != NULL && view->card_count > 0)
		{
			snprintf(
				destination,
				destination_size,
				"A: review again   START: hide help\n"
				"X: settings   Y: restart deck\n"
				"SELECT: decks"
			);
			return;
		}

		snprintf(
			destination,
			destination_size,
			"START: hide help\n"
			"SELECT: decks\n"
			"Add folders with cards.tsv on SD"
		);
		return;
	}

	if (view->answer_visible)
	{
		snprintf(
			destination,
			destination_size,
			"A: Again   L: Hard\n"
			"X: Good    Y: Easy\n"
			"D-pad ↑/↓: answer\n"
			"Circle ↑/↓: question\n"
			"B: undo   R: suspend\n"
			"SELECT: decks   START: hide help"
		);
		return;
	}

	snprintf(
		destination,
		destination_size,
		"A: reveal   START: hide help\n"
		"X: settings   Y: restart deck\n"
		"Circle ↑/↓: question\n"
		"B: undo   R: suspend\n"
		"SELECT: decks"
	);
}

static void app_flashcard_build_footer_text(
	char *destination,
	size_t destination_size,
	const struct study_backend_view *view,
	const struct study_settings *settings
)
{
	char new_limit[16];
	char review_limit[16];

	if (destination == NULL || destination_size == 0)
		return;

	if (
		view != NULL &&
		view->answer_visible &&
		view->tags_text != NULL &&
		view->tags_text[0] != '\0'
	)
	{
		snprintf(destination, destination_size, "%s", view->tags_text);
		return;
	}

	app_flashcard_format_limit_value(
		new_limit,
		sizeof(new_limit),
		settings != NULL ?
			settings->new_limit :
			STUDY_SETTINGS_DEFAULT_NEW_LIMIT
	);
	app_flashcard_format_limit_value(
		review_limit,
		sizeof(review_limit),
		settings != NULL ?
			settings->review_limit :
			STUDY_SETTINGS_DEFAULT_REVIEW_LIMIT
	);
	snprintf(
		destination,
		destination_size,
		"Today New %u/%s  Review %u/%s",
		view != NULL ? view->introduced_today_count : 0,
		new_limit,
		view != NULL ? view->reviewed_today_count : 0,
		review_limit
	);
}

static const char *app_flashcard_active_front_text(
	const struct study_backend_view *view
)
{
	if (view == NULL || !view->has_active_card)
		return "";
	if (view->front_text != NULL)
		return view->front_text;
	return !view->answer_visible && view->primary_text != NULL ?
		view->primary_text :
		"";
}

static const char *app_flashcard_active_back_text(
	const struct study_backend_view *view
)
{
	if (view == NULL || !view->has_active_card)
		return "";
	if (view->back_text != NULL && view->back_text[0] != '\0')
		return view->back_text;
	if (
		view->answer_visible &&
		view->primary_text != NULL &&
		view->primary_text[0] != '\0'
	)
	{
		return view->primary_text;
	}
	return view->answer_visible ?
		APP_FLASHCARD_NO_ANSWER_TEXT :
		"";
}

static const char *app_flashcard_display_text(
	const struct study_backend_view *view
)
{
	const char *front_text;
	const char *back_text;

	if (view == NULL)
		return "";
	if (!view->has_active_card)
		return view->primary_text != NULL ? view->primary_text : "";

	front_text = app_flashcard_active_front_text(view);
	back_text = app_flashcard_active_back_text(view);
	return view->answer_visible ? back_text : front_text;
}

void app_flashcard_text_view_build(
	struct app_flashcard_text_view *text_view,
	const struct study_backend_view *view,
	const struct study_settings *settings,
	bool help_visible
)
{
	if (text_view == NULL)
		return;

	memset(text_view, 0, sizeof(*text_view));
	app_flashcard_copy_string(
		text_view->title_text,
		sizeof(text_view->title_text),
		view != NULL && view->answer_visible ? "Answer" : "Question"
	);
	if (view != NULL)
	{
		text_view->has_active_card = view->has_active_card;
		text_view->answer_visible = view->answer_visible;
		text_view->undo_available = view->undo_available;
		text_view->help_visible = help_visible;
		app_flashcard_copy_string(
			text_view->front_text,
			sizeof(text_view->front_text),
			app_flashcard_active_front_text(view)
		);
		app_flashcard_copy_string(
			text_view->back_text,
			sizeof(text_view->back_text),
			app_flashcard_active_back_text(view)
		);
		app_flashcard_copy_string(
			text_view->tags_text,
			sizeof(text_view->tags_text),
			view->tags_text
		);
		app_flashcard_copy_string(
			text_view->display_text,
			sizeof(text_view->display_text),
			app_flashcard_display_text(view)
		);
		app_flashcard_copy_string(
			text_view->status_text,
			sizeof(text_view->status_text),
			view->status_text
		);
	}

	app_flashcard_build_meta_text(
		text_view->progress_text,
		sizeof(text_view->progress_text),
		view,
		false,
		0
	);
	app_flashcard_build_controls_text(
		text_view->help_text,
		sizeof(text_view->help_text),
		view,
		help_visible
	);
	app_flashcard_build_footer_text(
		text_view->footer_text,
		sizeof(text_view->footer_text),
		view,
		settings
	);
}
