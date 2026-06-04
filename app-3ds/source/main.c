#include <3ds.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app_settings.h"
#include "app_controls.h"
#include "app_layout.h"
#include "app_power.h"
#include "app_review.h"
#include "app_status.h"
#include "app_text.h"
#include "app_time.h"
#include "deck.h"
#include "deck_index.h"
#include "deck_summary.h"
#include "review_log.h"
#include "review_state.h"
#include "scheduler.h"

#define APP_VERSION "0.5.0-dev"
#define STATUS_MESSAGE_SIZE 64
#define DAY_CHECK_INTERVAL_SECONDS 60
#define APP_COLOR_RESET CONSOLE_RESET
/*
 * High-contrast terminal palette for the dark 3DS console. Blue-family ANSI
 * colors wash out on 3DS LCDs, especially at low brightness.
 */
#define APP_COLOR_PRIMARY CONSOLE_GREEN
#define APP_COLOR_TEXT CONSOLE_WHITE
#define APP_COLOR_MUTED CONSOLE_ESC(37m)
#define APP_COLOR_FOCUS CONSOLE_ESC(33;1;7m)
#define APP_COLOR_CAUTION CONSOLE_YELLOW
#define APP_COLOR_SUCCESS CONSOLE_GREEN
#define APP_COLOR_DANGER CONSOLE_RED
#define APP_COLOR_WARNING APP_COLOR_CAUTION
#define APP_COLOR_ACCENT APP_COLOR_PRIMARY
#define APP_COLOR_NEUTRAL APP_COLOR_TEXT
#define APP_COLOR_SELECTED APP_COLOR_FOCUS
#define APP_COLOR_EASY APP_COLOR_TEXT
#define APP_COLOR_NEW APP_COLOR_TEXT
#define APP_COLOR_LEARNING APP_COLOR_WARNING
#define APP_COLOR_REVIEW APP_COLOR_SUCCESS
#define APP_COLOR_SUSPENDED APP_COLOR_DANGER
#define APP_COLOR_RULE APP_COLOR_MUTED
static const unsigned int daily_limit_presets[] = {
	5,
	10,
	20,
	50,
	100,
	200,
	500,
	1000,
	0,
};

enum app_mode
{
	APP_MODE_DECK_SELECT,
	APP_MODE_LOAD_ERROR,
	APP_MODE_REVIEW,
	APP_MODE_SUMMARY,
	APP_MODE_ACTIONS,
	APP_MODE_SETTINGS,
	APP_MODE_CONTROLS,
	APP_MODE_CONFIRM_RESTORE,
	APP_MODE_CONFIRM_SUSPEND,
	APP_MODE_CONFIRM_RESET,
	APP_MODE_CONFIRM_EXIT,
};

enum action_item
{
	ACTION_ITEM_UNSUSPEND_ALL,
	ACTION_ITEM_DAILY_LIMITS,
	ACTION_ITEM_RESET_PROGRESS,
	ACTION_ITEM_COUNT,
};

enum setting_item
{
	SETTING_ITEM_NEW_LIMIT,
	SETTING_ITEM_REVIEW_LIMIT,
	SETTING_ITEM_COUNT,
};

struct app_state
{
	enum app_mode mode;
	enum app_mode action_return_mode;
	enum app_mode exit_return_mode;
	enum app_mode controls_return_mode;
	enum action_item selected_action;
	bool revealed;
	bool exit_requested;
	bool battery_service_available;
	bool battery_status_available;
	bool battery_low;
	bool battery_charging;
	u8 battery_level;
	unsigned int current_day;
	enum deck_load_result load_result;
	struct deck_load_report load_report;
	enum app_settings_load_result settings_load_result;
	struct app_settings_load_report settings_load_report;
	enum app_settings_save_result settings_save_result;
	enum review_state_load_result state_load_result;
	struct review_state_load_report state_load_report;
	enum review_state_save_result state_save_result;
	const char *state_message;
	const char *settings_message;
	enum setting_item selected_setting;
	size_t selected_deck_index;
	size_t review_scroll_offset;
	char active_cards_path[DECK_INDEX_MAX_PATH_LENGTH];
	char active_state_path[DECK_INDEX_MAX_PATH_LENGTH];
	char active_review_log_path[DECK_INDEX_MAX_PATH_LENGTH];
	char active_settings_path[DECK_INDEX_MAX_PATH_LENGTH];
	struct app_settings settings;
	struct app_settings edited_settings;
	struct deck_index deck_index;
	struct deck_summary deck_summaries[DECK_INDEX_MAX_DECKS];
	struct deck deck;
	struct scheduler_session session;
	struct scheduler_session save_rollback_session;
	char status_message[STATUS_MESSAGE_SIZE];
};

static PrintConsole top_screen;
static PrintConsole bottom_screen;

static void draw_scanning_progress_screen(
	const struct app_state *app,
	size_t loaded_count,
	size_t total_count,
	const char *deck_name
);
static void app_set_status(struct app_state *app, const char *message);
static void present_current_frame(void);

static void select_top_screen(void)
{
	consoleSelect(&top_screen);
}

static void select_bottom_screen(void)
{
	consoleSelect(&bottom_screen);
}

static void app_console_clear(void)
{
	consoleClear();
	printf(APP_COLOR_RESET);
}

static enum app_control_mode app_control_mode_for_app_mode(enum app_mode mode)
{
	switch (mode)
	{
	case APP_MODE_DECK_SELECT:
		return APP_CONTROL_MODE_DECK_SELECT;
	case APP_MODE_LOAD_ERROR:
		return APP_CONTROL_MODE_LOAD_ERROR;
	case APP_MODE_REVIEW:
		return APP_CONTROL_MODE_REVIEW;
	case APP_MODE_SUMMARY:
		return APP_CONTROL_MODE_SUMMARY;
	case APP_MODE_ACTIONS:
		return APP_CONTROL_MODE_ACTIONS;
	case APP_MODE_SETTINGS:
		return APP_CONTROL_MODE_SETTINGS;
	case APP_MODE_CONTROLS:
		return APP_CONTROL_MODE_CONTROLS;
	case APP_MODE_CONFIRM_RESTORE:
		return APP_CONTROL_MODE_CONFIRM_RESTORE;
	case APP_MODE_CONFIRM_SUSPEND:
		return APP_CONTROL_MODE_CONFIRM_SUSPEND;
	case APP_MODE_CONFIRM_RESET:
		return APP_CONTROL_MODE_CONFIRM_RESET;
	case APP_MODE_CONFIRM_EXIT:
		return APP_CONTROL_MODE_CONFIRM_EXIT;
	}

	return APP_CONTROL_MODE_DECK_SELECT;
}

static bool app_mode_is_review_surface(enum app_mode mode)
{
	return (
		mode == APP_MODE_REVIEW ||
		mode == APP_MODE_SUMMARY ||
		mode == APP_MODE_CONFIRM_RESTORE ||
		mode == APP_MODE_CONFIRM_SUSPEND
	);
}

static void console_move(int row, int column)
{
	printf("\x1b[%d;%dH", row, column);
}

static void draw_app_title(const char *section)
{
	printf("\x1b[1;1H" APP_COLOR_ACCENT "anki3ds");
	if (section != NULL && section[0] != '\0')
		printf(" %s", section);
	printf(" %s" APP_COLOR_RESET, APP_VERSION);
}

static void draw_wrapped_text_columns(
	const char *text,
	int row,
	int max_rows,
	int max_columns,
	size_t scroll_offset
)
{
	size_t text_row = 0;
	int column = APP_LAYOUT_TEXT_LEFT;

	if (text == NULL || max_rows <= 0 || max_columns <= 0)
		return;

	for (size_t index = 0; text[index] != '\0'; )
	{
		char value = text[index];
		size_t char_length = app_text_utf8_char_length(&text[index]);

		if (char_length == 0)
			break;
		if (value == '\r')
		{
			index += char_length;
			continue;
		}
		if (value == '\n')
		{
			text_row++;
			column = APP_LAYOUT_TEXT_LEFT;
			index += char_length;
			continue;
		}
		if (value == '\t')
		{
			value = ' ';
			char_length = 1;
		}

		if (column >= APP_LAYOUT_TEXT_LEFT + max_columns)
		{
			text_row++;
			column = APP_LAYOUT_TEXT_LEFT;
		}

		if (
			text_row >= scroll_offset &&
			text_row < scroll_offset + (size_t)max_rows
		)
		{
			console_move(
				row + (int)(text_row - scroll_offset),
				column
			);
			if (value == ' ' && text[index] == '\t')
				putchar(value);
			else
				fwrite(&text[index], 1, char_length, stdout);
		}

		index += char_length;
		column++;
	}
}

static void draw_review_scroll_hint(
	int row,
	size_t scroll_offset,
	size_t max_scroll_offset
)
{
	char up_marker;
	char down_marker;

	if (max_scroll_offset == 0)
		return;

	up_marker = scroll_offset > 0 ? '^' : ' ';
	down_marker = scroll_offset < max_scroll_offset ? 'v' : ' ';
	printf(
		"\x1b[%d;%dH" APP_COLOR_WARNING "%c %lu/%lu %c" APP_COLOR_RESET,
		row,
		APP_LAYOUT_REVIEW_SCROLL_HINT_COLUMN,
		up_marker,
		(unsigned long)(scroll_offset + 1),
		(unsigned long)(max_scroll_offset + 1),
		down_marker
	);
}

static void print_truncated(const char *text, size_t max_columns)
{
	size_t columns = app_text_column_count(text);
	size_t visible_bytes;

	if (columns <= max_columns)
	{
		printf("%s", text);
		return;
	}

	if (max_columns <= 3)
	{
		visible_bytes = app_text_byte_count_for_columns(text, max_columns);
		printf("%.*s", (int)visible_bytes, text);
		return;
	}

	visible_bytes = app_text_byte_count_for_columns(text, max_columns - 3);
	printf("%.*s", (int)visible_bytes, text);
	printf("...");
}

static void draw_deck_name_line(
	const struct app_state *app,
	int row,
	size_t name_width
)
{
	printf("\x1b[%d;1HDeck: ", row);
	print_truncated(app->deck.name, name_width);
}

static unsigned int review_count_total(const struct scheduler_session *session)
{
	unsigned int total = 0;

	for (size_t index = 0; index < session->card_count; index++)
		total += session->cards[index].review_count;

	return total;
}

static void format_daily_limit(char *destination, size_t destination_size, unsigned int limit)
{
	if (limit == 0)
		snprintf(destination, destination_size, "%s", "all");
	else
		snprintf(destination, destination_size, "%u", limit);
}

static const char *action_item_name(enum action_item action)
{
	switch (action)
	{
	case ACTION_ITEM_UNSUSPEND_ALL:
		return "Restore suspended";
	case ACTION_ITEM_DAILY_LIMITS:
		return "Daily limits";
	case ACTION_ITEM_RESET_PROGRESS:
		return "Reset progress";
	case ACTION_ITEM_COUNT:
		break;
	}

	return "Action";
}

static const char *setting_item_name(enum setting_item setting)
{
	switch (setting)
	{
	case SETTING_ITEM_NEW_LIMIT:
		return "New limit";
	case SETTING_ITEM_REVIEW_LIMIT:
		return "Review limit";
	case SETTING_ITEM_COUNT:
		break;
	}

	return "Limit";
}

static size_t daily_limit_preset_count(void)
{
	return sizeof(daily_limit_presets) / sizeof(daily_limit_presets[0]);
}

static unsigned int adjusted_daily_limit(unsigned int current, bool increase)
{
	size_t preset_count = daily_limit_preset_count();

	for (size_t index = 0; index < preset_count; index++)
	{
		if (daily_limit_presets[index] == current)
		{
			if (increase)
				return daily_limit_presets[(index + 1) % preset_count];
			if (index == 0)
				return daily_limit_presets[preset_count - 1];

			return daily_limit_presets[index - 1];
		}
	}

	if (increase)
	{
		for (size_t index = 0; index < preset_count; index++)
		{
			if (daily_limit_presets[index] != 0 && daily_limit_presets[index] > current)
				return daily_limit_presets[index];
		}

		return 0;
	}

	for (size_t offset = 0; offset < preset_count; offset++)
	{
		size_t index = preset_count - offset - 1;

		if (daily_limit_presets[index] != 0 && daily_limit_presets[index] < current)
			return daily_limit_presets[index];
	}

	return 0;
}

static unsigned int *selected_daily_limit(struct app_state *app)
{
	if (app->selected_setting == SETTING_ITEM_NEW_LIMIT)
		return &app->edited_settings.new_limit;

	return &app->edited_settings.review_limit;
}

static bool app_command_pressed(
	unsigned int trigger_buttons,
	unsigned int active_buttons,
	unsigned int command_button
)
{
	return app_controls_command_triggered(
		trigger_buttons,
		active_buttons,
		command_button
	);
}

static bool deck_summary_state_allows_study(const struct deck_summary *summary)
{
	return review_state_load_result_allows_save(summary->state_load_result);
}

static bool app_state_allows_study(const struct app_state *app)
{
	return review_state_load_result_allows_save(app->state_load_result);
}

static size_t app_new_limit_blocked_count(const struct app_state *app)
{
	return scheduler_new_limit_blocked_count(&app->session);
}

static size_t app_review_limit_blocked_count(const struct app_state *app)
{
	return scheduler_review_limit_blocked_count(&app->session);
}

static bool app_daily_limit_blocks_cards(const struct app_state *app)
{
	return (
		app_new_limit_blocked_count(app) > 0 ||
		app_review_limit_blocked_count(app) > 0
	);
}

static bool deck_summary_daily_limit_blocks_cards(const struct deck_summary *summary)
{
	return (
		summary->new_limit_blocked_count > 0 ||
		summary->review_limit_blocked_count > 0
	);
}

static bool settings_load_result_needs_warning(enum app_settings_load_result result)
{
	return result == APP_SETTINGS_LOAD_BAD_FORMAT;
}

static bool state_load_result_needs_warning(enum review_state_load_result result)
{
	return result == REVIEW_STATE_LOAD_UNMATCHED;
}

static const char *deck_selection_status_suffix(const struct deck_summary *summary)
{
	if (summary == NULL)
		return "";
	if (summary->deck_load_result != DECK_LOAD_OK)
		return "; load error";
	if (
		settings_load_result_needs_warning(summary->settings_load_result) &&
		state_load_result_needs_warning(summary->state_load_result)
	)
	{
		return "; settings/state";
	}
	if (settings_load_result_needs_warning(summary->settings_load_result))
		return "; settings ignored";
	if (state_load_result_needs_warning(summary->state_load_result))
		return "; state unmatched";
	if (deck_summary_daily_limit_blocks_cards(summary))
		return "; limit reached";

	return "";
}

static void app_set_deck_selection_status(struct app_state *app)
{
	const char *suffix = "";

	if (app->deck_index.count == 0)
	{
		app_set_status(app, "No deck selected");
		return;
	}

	if (app->selected_deck_index < app->deck_index.count)
	{
		suffix = deck_selection_status_suffix(
			&app->deck_summaries[app->selected_deck_index]
		);
	}

	snprintf(
		app->status_message,
		sizeof(app->status_message),
		"Deck %lu/%lu%s",
		(unsigned long)(app->selected_deck_index + 1),
		(unsigned long)app->deck_index.count,
		suffix
	);
}

static const char *active_deck_status_suffix(const struct app_state *app)
{
	bool settings_warning = settings_load_result_needs_warning(
		app->settings_load_result
	);
	bool state_warning = state_load_result_needs_warning(app->state_load_result);

	if (!app_state_allows_study(app))
		return settings_warning ? "; reset state/settings" : "; reset state";
	if (settings_warning && state_warning)
		return "; settings/state";
	if (settings_warning)
		return "; settings ignored";
	if (state_warning)
		return "; state unmatched";
	if (app_daily_limit_blocks_cards(app))
		return "; limit reached";

	return "";
}

static void app_set_actions_status(struct app_state *app)
{
	snprintf(
		app->status_message,
		sizeof(app->status_message),
		"Actions%s",
		active_deck_status_suffix(app)
	);
}

static void app_set_action_selection_status(struct app_state *app)
{
	snprintf(
		app->status_message,
		sizeof(app->status_message),
		"Action: %s%s",
		action_item_name(app->selected_action),
		active_deck_status_suffix(app)
	);
}

static void app_set_settings_open_status(struct app_state *app)
{
	snprintf(
		app->status_message,
		sizeof(app->status_message),
		"Editing limits%s",
		active_deck_status_suffix(app)
	);
}

static const char *app_settings_edit_status_suffix(
	const struct app_state *app,
	bool unsaved_changes
)
{
	return unsaved_changes ? " unsaved" : active_deck_status_suffix(app);
}

static void app_set_setting_field_status(
	struct app_state *app,
	bool unsaved_changes
)
{
	snprintf(
		app->status_message,
		sizeof(app->status_message),
		"Editing %s%s",
		setting_item_name(app->selected_setting),
		app_settings_edit_status_suffix(app, unsaved_changes)
	);
}

static void app_set_setting_value_status(
	struct app_state *app,
	const char *limit_text,
	bool unsaved_changes
)
{
	snprintf(
		app->status_message,
		sizeof(app->status_message),
		"%s: %s%s",
		setting_item_name(app->selected_setting),
		limit_text,
		app_settings_edit_status_suffix(app, unsaved_changes)
	);
}

static void app_set_limits_canceled_status(
	struct app_state *app,
	bool discarded_changes
)
{
	snprintf(
		app->status_message,
		sizeof(app->status_message),
		"%s%s",
		discarded_changes ? "Limits canceled; discarded" : "Limits canceled",
		active_deck_status_suffix(app)
	);
}

static void app_set_nothing_status(struct app_state *app, const char *message)
{
	snprintf(
		app->status_message,
		sizeof(app->status_message),
		"%s%s",
		message,
		active_deck_status_suffix(app)
	);
}

static void app_set_undo_saved_status(struct app_state *app, bool log_saved)
{
	snprintf(
		app->status_message,
		sizeof(app->status_message),
		"%s%s",
		log_saved ? "Undo saved" : "Undo saved; log skipped",
		active_deck_status_suffix(app)
	);
}

static void app_set_canceled_status(struct app_state *app, const char *label)
{
	snprintf(
		app->status_message,
		sizeof(app->status_message),
		"%s canceled%s",
		label,
		active_deck_status_suffix(app)
	);
}

static const char *app_confirmation_status_suffix(
	const struct app_state *app,
	enum app_mode return_mode
)
{
	if (return_mode == APP_MODE_CONTROLS)
	{
		if (app->controls_return_mode == APP_MODE_CONTROLS)
			return "";
		return app_confirmation_status_suffix(app, app->controls_return_mode);
	}
	if (return_mode == APP_MODE_DECK_SELECT)
	{
		if (app->selected_deck_index < app->deck_index.count)
		{
			return deck_selection_status_suffix(
				&app->deck_summaries[app->selected_deck_index]
			);
		}

		return "";
	}
	if (return_mode == APP_MODE_LOAD_ERROR)
		return "; load error";
	if (
		return_mode == APP_MODE_REVIEW ||
		return_mode == APP_MODE_SUMMARY ||
		return_mode == APP_MODE_ACTIONS ||
		return_mode == APP_MODE_SETTINGS ||
		return_mode == APP_MODE_CONFIRM_RESTORE ||
		return_mode == APP_MODE_CONFIRM_SUSPEND ||
		return_mode == APP_MODE_CONFIRM_RESET
	)
	{
		return active_deck_status_suffix(app);
	}

	return "";
}

static void app_set_required_status(
	struct app_state *app,
	const char *label,
	const char *button,
	enum app_mode return_mode
)
{
	snprintf(
		app->status_message,
		sizeof(app->status_message),
		"%s requires %s%s",
		label,
		button,
		app_confirmation_status_suffix(app, return_mode)
	);
}

static bool app_settings_have_unsaved_changes(const struct app_state *app)
{
	return (
		app->edited_settings.new_limit != app->settings.new_limit ||
		app->edited_settings.review_limit != app->settings.review_limit
	);
}

static bool app_exit_would_discard_unsaved_limits(const struct app_state *app)
{
	if (!app_settings_have_unsaved_changes(app))
		return false;
	if (app->mode == APP_MODE_SETTINGS)
		return true;
	if (
		app->mode == APP_MODE_CONTROLS ||
		(
			app->mode == APP_MODE_CONFIRM_EXIT &&
			app->exit_return_mode == APP_MODE_CONTROLS
		)
	)
	{
		return app->controls_return_mode == APP_MODE_SETTINGS;
	}

	return app->mode == APP_MODE_CONFIRM_EXIT &&
		app->exit_return_mode == APP_MODE_SETTINGS;
}

static bool app_mode_shows_unsaved_limit_status(
	const struct app_state *app,
	enum app_mode mode
)
{
	if (!app_settings_have_unsaved_changes(app))
		return false;
	if (mode == APP_MODE_SETTINGS)
		return true;
	if (mode == APP_MODE_CONTROLS)
		return app->controls_return_mode == APP_MODE_SETTINGS;

	return false;
}

static void app_set_controls_status(struct app_state *app)
{
	if (app_mode_shows_unsaved_limit_status(app, APP_MODE_CONTROLS))
	{
		app_set_status(app, "Unsaved limit edits");
		return;
	}

	if (app->controls_return_mode == APP_MODE_LOAD_ERROR)
	{
		app_set_status(app, "Controls; load error");
		return;
	}

	if (app->controls_return_mode == APP_MODE_DECK_SELECT)
	{
		const char *suffix = "";

		if (app->selected_deck_index < app->deck_index.count)
		{
			suffix = deck_selection_status_suffix(
				&app->deck_summaries[app->selected_deck_index]
			);
		}

		snprintf(
			app->status_message,
			sizeof(app->status_message),
			"Controls%s",
			suffix
		);
		return;
	}

	if (
		app->controls_return_mode == APP_MODE_REVIEW ||
		app->controls_return_mode == APP_MODE_SUMMARY ||
		app->controls_return_mode == APP_MODE_ACTIONS ||
		app->controls_return_mode == APP_MODE_SETTINGS
	)
	{
		snprintf(
			app->status_message,
			sizeof(app->status_message),
			"Controls%s",
			active_deck_status_suffix(app)
		);
		return;
	}

	app_set_status(app, "Controls");
}

static void app_set_controls_closed_status(
	struct app_state *app,
	enum app_mode return_mode
)
{
	if (app_mode_shows_unsaved_limit_status(app, return_mode))
	{
		app_set_status(app, "Unsaved limit edits");
		return;
	}
	if (return_mode == APP_MODE_DECK_SELECT)
	{
		app_set_deck_selection_status(app);
		return;
	}
	if (return_mode == APP_MODE_LOAD_ERROR)
	{
		app_set_status(app, "Load error");
		return;
	}
	if (
		return_mode == APP_MODE_REVIEW ||
		return_mode == APP_MODE_SUMMARY ||
		return_mode == APP_MODE_ACTIONS ||
		return_mode == APP_MODE_SETTINGS
	)
	{
		snprintf(
			app->status_message,
			sizeof(app->status_message),
			"Controls closed%s",
			active_deck_status_suffix(app)
		);
		return;
	}

	app_set_status(app, "Controls closed");
}

static void app_set_exit_canceled_status(
	struct app_state *app,
	enum app_mode return_mode
)
{
	if (return_mode == APP_MODE_CONTROLS)
	{
		app_set_controls_status(app);
		return;
	}
	if (app_mode_shows_unsaved_limit_status(app, return_mode))
	{
		app_set_status(app, "Unsaved limit edits");
		return;
	}
	if (return_mode == APP_MODE_DECK_SELECT)
	{
		app_set_deck_selection_status(app);
		return;
	}
	if (return_mode == APP_MODE_LOAD_ERROR)
	{
		app_set_status(app, "Load error");
		return;
	}
	if (
		return_mode == APP_MODE_REVIEW ||
		return_mode == APP_MODE_SUMMARY ||
		return_mode == APP_MODE_ACTIONS ||
		return_mode == APP_MODE_SETTINGS ||
		return_mode == APP_MODE_CONFIRM_RESTORE ||
		return_mode == APP_MODE_CONFIRM_SUSPEND ||
		return_mode == APP_MODE_CONFIRM_RESET
	)
	{
		snprintf(
			app->status_message,
			sizeof(app->status_message),
			"Exit canceled%s",
			active_deck_status_suffix(app)
		);
		return;
	}

	app_set_status(app, "Exit canceled");
}

static void draw_deck_due_counts_inline(
	size_t new_due_count,
	size_t learning_due_count,
	size_t review_due_count,
	size_t suspended_count
)
{
	printf(
		APP_COLOR_RESET " " APP_COLOR_NEW "N" APP_COLOR_RESET ":%lu"
		" " APP_COLOR_LEARNING "L" APP_COLOR_RESET ":%lu"
		" " APP_COLOR_REVIEW "R" APP_COLOR_RESET ":%lu"
		" " APP_COLOR_SUSPENDED "S" APP_COLOR_RESET ":%lu",
		(unsigned long)new_due_count,
		(unsigned long)learning_due_count,
		(unsigned long)review_due_count,
		(unsigned long)suspended_count
	);
}

static void draw_deck_selector_study_suffix(const struct deck_summary *summary)
{
	bool settings_warning = settings_load_result_needs_warning(
		summary->settings_load_result
	);
	bool state_warning = state_load_result_needs_warning(summary->state_load_result);

	if (settings_warning || state_warning)
	{
		printf(APP_COLOR_RESET APP_COLOR_WARNING);
		if (settings_warning && state_warning)
			printf(" settings! state!");
		else if (settings_warning)
			printf(" settings ignored");
		else
			printf(" state unmatched");
		printf(APP_COLOR_RESET);
		return;
	}

	draw_deck_due_counts_inline(
		summary->new_due_count,
		summary->learning_due_count,
		summary->review_due_count,
		summary->suspended_count
	);
	if (deck_summary_daily_limit_blocks_cards(summary))
		printf(APP_COLOR_WARNING " limit" APP_COLOR_RESET);
}

static void draw_deck_load_error_detail(
	enum deck_load_result load_result,
	const struct deck_load_report *report,
	int row
)
{
	if (report == NULL)
		return;

	if (report->line_number > 0 && load_result == DECK_LOAD_TOO_LARGE)
	{
		printf("\x1b[%d;1HLine %u: too many cards", row, report->line_number);
	}
	else if (report->line_number > 0)
	{
		printf(
			"\x1b[%d;1HLine %u: %s",
			row,
			report->line_number,
			deck_parse_result_name(report->parse_result)
		);
	}
	else if (report->parse_result == DECK_PARSE_EMPTY)
	{
		printf("\x1b[%d;1Hcards.tsv has no cards.", row);
	}
}

static void draw_load_error_detail(const struct app_state *app, int row)
{
	draw_deck_load_error_detail(app->load_result, &app->load_report, row);
}

static bool draw_settings_load_error_detail(
	enum app_settings_load_result load_result,
	const struct app_settings_load_report *report,
	int row
)
{
	if (load_result != APP_SETTINGS_LOAD_BAD_FORMAT || report == NULL)
		return false;

	if (report->line_number > 0)
	{
		printf(
			"\x1b[%d;1HLine %u: %s",
			row,
			report->line_number,
			app_settings_parse_result_name(report->parse_result)
		);
		return true;
	}
	if (report->parse_result != APP_SETTINGS_PARSE_OK)
	{
		printf(
			"\x1b[%d;1HSettings: %s",
			row,
			app_settings_parse_result_name(report->parse_result)
		);
		return true;
	}

	return false;
}

static bool draw_review_state_load_error_detail(
	enum review_state_load_result load_result,
	const struct review_state_load_report *report,
	int row
)
{
	if (load_result != REVIEW_STATE_LOAD_BAD_FORMAT || report == NULL)
		return false;

	if (report->line_number > 0)
	{
		printf(
			"\x1b[%d;1HLine %u: %s",
			row,
			report->line_number,
			review_state_parse_result_name(report->parse_result)
		);
		return true;
	}
	if (report->parse_result != REVIEW_STATE_PARSE_OK)
	{
		printf(
			"\x1b[%d;1HState: %s",
			row,
			review_state_parse_result_name(report->parse_result)
		);
		return true;
	}

	return false;
}

static enum app_mode app_review_mode_for_session(const struct app_state *app)
{
	return app_review_should_show_queue(app->state_load_result, &app->session) ?
		APP_MODE_REVIEW :
		APP_MODE_SUMMARY;
}

static const struct card *current_card(const struct app_state *app)
{
	if (!scheduler_has_current(&app->session))
		return NULL;

	return &app->deck.cards[scheduler_current_index(&app->session)];
}

static bool review_scroll_metrics(
	const struct app_state *app,
	const char **text,
	size_t *max_columns,
	size_t *visible_rows
)
{
	const struct card *card;

	if (
		app == NULL ||
		text == NULL ||
		max_columns == NULL ||
		visible_rows == NULL
	)
	{
		return false;
	}

	card = current_card(app);
	if (card == NULL)
		return false;

	if (app->revealed)
	{
		*text = card->back;
		*max_columns = APP_LAYOUT_TEXT_WIDTH;
		*visible_rows = APP_LAYOUT_REVIEW_BACK_TEXT_ROWS;
		return true;
	}

	*text = card->front;
	*max_columns = APP_LAYOUT_TEXT_WIDTH;
	*visible_rows = APP_LAYOUT_REVIEW_FRONT_TEXT_ROWS;
	return true;
}

static size_t review_max_scroll_offset(const struct app_state *app)
{
	const char *text;
	size_t max_columns;
	size_t visible_rows;

	if (!review_scroll_metrics(app, &text, &max_columns, &visible_rows))
		return 0;

	return app_text_max_scroll_offset(text, max_columns, visible_rows);
}

static void reset_review_scroll(struct app_state *app)
{
	app->review_scroll_offset = 0;
}

static void copy_string(char *destination, size_t destination_size, const char *source)
{
	if (destination_size == 0)
		return;

	if (source == NULL)
	{
		destination[0] = '\0';
		return;
	}

	snprintf(destination, destination_size, "%s", source);
}

static void app_set_status(struct app_state *app, const char *message)
{
	copy_string(app->status_message, sizeof(app->status_message), message);
}

static void app_set_scan_complete_status(struct app_state *app)
{
	if (app->deck_index.count == 0)
	{
		if (app->deck_index.ignored_count > 0)
		{
			snprintf(
				app->status_message,
				sizeof(app->status_message),
				"No decks found; %lu ignored",
				(unsigned long)app->deck_index.ignored_count
			);
		}
		else
		{
			app_set_status(app, "No decks found");
		}
		return;
	}

	if (app->deck_index.overflowed)
	{
		if (app->deck_index.ignored_count > 0)
		{
			snprintf(
				app->status_message,
				sizeof(app->status_message),
				"Scan: %lu/%lu shown; %lu ignored",
				(unsigned long)app->deck_index.count,
				(unsigned long)app->deck_index.total_count,
				(unsigned long)app->deck_index.ignored_count
			);
		}
		else
		{
			snprintf(
				app->status_message,
				sizeof(app->status_message),
				"Scan done; %lu/%lu decks shown",
				(unsigned long)app->deck_index.count,
				(unsigned long)app->deck_index.total_count
			);
		}
		return;
	}

	if (app->deck_index.ignored_count > 0)
	{
		snprintf(
			app->status_message,
			sizeof(app->status_message),
			"Scan: %lu decks; %lu ignored",
			(unsigned long)app->deck_index.count,
			(unsigned long)app->deck_index.ignored_count
		);
		return;
	}

	snprintf(
		app->status_message,
		sizeof(app->status_message),
		"Scan done; %lu decks",
		(unsigned long)app->deck_index.count
	);
}

static void app_set_day_change_status(struct app_state *app)
{
	if (
		app->mode == APP_MODE_CONFIRM_EXIT &&
		app_exit_would_discard_unsaved_limits(app)
	)
	{
		app_set_status(app, "Exit loses unsaved limits");
		return;
	}

	if (app_mode_shows_unsaved_limit_status(app, app->mode))
	{
		app_set_status(app, "Unsaved limit edits");
		return;
	}

	app_review_format_day_change_status(
		app->status_message,
		sizeof(app->status_message),
		app->state_load_result,
		&app->session
	);
}

static bool scroll_review_text(struct app_state *app, bool scroll_down)
{
	size_t max_offset = review_max_scroll_offset(app);
	bool edge_message = false;

	if (max_offset == 0)
	{
		app_set_status(app, "Text fits");
		return true;
	}

	if (scroll_down)
	{
		if (app->review_scroll_offset < max_offset)
			app->review_scroll_offset++;
		else
		{
			app_set_status(app, "Text bottom");
			edge_message = true;
		}
	}
	else if (app->review_scroll_offset > 0)
	{
		app->review_scroll_offset--;
	}
	else
	{
		app_set_status(app, "Text top");
		edge_message = true;
	}

	if (!edge_message)
	{
		snprintf(
			app->status_message,
			sizeof(app->status_message),
			"Text %lu/%lu",
			(unsigned long)(app->review_scroll_offset + 1),
			(unsigned long)(max_offset + 1)
		);
	}

	return true;
}

static const char *status_color_escape(enum app_status_color color)
{
	switch (color)
	{
	case APP_STATUS_COLOR_DANGER:
		return APP_COLOR_DANGER;
	case APP_STATUS_COLOR_WARNING:
		return APP_COLOR_WARNING;
	case APP_STATUS_COLOR_SUCCESS:
		return APP_COLOR_SUCCESS;
	case APP_STATUS_COLOR_NEUTRAL:
		break;
	}

	return APP_COLOR_NEUTRAL;
}

static void wait_for_idle_input(unsigned int idle_wait_count)
{
	hidWaitForAnyEvent(true, 0, app_power_idle_input_wait_ns(idle_wait_count));
}

static bool app_mode_uses_held_navigation_wait(
	enum app_mode mode,
	unsigned int buttons_held
)
{
	if (
		!app_controls_mode_uses_navigation_repeat(
			app_control_mode_for_app_mode(mode)
		)
	)
	{
		return false;
	}

	return app_controls_repeatable_navigation_held_for_mode(
		app_control_mode_for_app_mode(mode),
		buttons_held
	);
}

static void schedule_next_day_check(time_t *next_check_time, time_t now)
{
	if (next_check_time == NULL || now == (time_t)-1)
		return;

	*next_check_time = now + DAY_CHECK_INTERVAL_SECONDS;
}

static bool day_check_is_due(time_t *next_check_time, time_t now)
{
	if (next_check_time == NULL || now == (time_t)-1)
		return false;
	if (*next_check_time == 0)
		return true;

	return now >= *next_check_time;
}

static void app_scan_decks(struct app_state *app)
{
	unsigned int today = app_time_current_day();
	char selected_deck_id[DECK_MAX_NAME_LENGTH];
	const struct deck_entry *selected_deck;

	app->current_day = today;
	selected_deck_id[0] = '\0';
	selected_deck = deck_index_get(&app->deck_index, app->selected_deck_index);
	if (selected_deck != NULL)
		copy_string(selected_deck_id, sizeof(selected_deck_id), selected_deck->id);

	deck_index_scan(&app->deck_index, DECK_INDEX_ROOT_PATH);

	for (size_t index = 0; index < DECK_INDEX_MAX_DECKS; index++)
	{
		if (index < app->deck_index.count)
		{
			draw_scanning_progress_screen(
				app,
				index,
				app->deck_index.count,
				app->deck_index.entries[index].display_name
			);
			present_current_frame();
			deck_summary_load(
				&app->deck_summaries[index],
				&app->deck_index.entries[index],
				today
			);
		}
		else
		{
			deck_summary_init(&app->deck_summaries[index]);
		}
	}

	app->selected_deck_index = 0;
	if (selected_deck_id[0] != '\0')
		deck_index_find(&app->deck_index, selected_deck_id, &app->selected_deck_index);
}

static enum app_power_battery_sample_result app_sample_battery(struct app_state *app)
{
	u8 shell_state;
	u8 level;
	u8 charge_state;
	bool status_available;
	bool charging;
	bool low;
	bool changed;
	bool old_status_available;
	bool old_low;
	bool old_charging;
	u8 old_level;

	if (!app->battery_service_available)
		return APP_POWER_BATTERY_SAMPLE_UNAVAILABLE;

	if (R_FAILED(PTMU_GetShellState(&shell_state)))
		return APP_POWER_BATTERY_SAMPLE_READ_FAILED;
	if (shell_state == 0)
		return APP_POWER_BATTERY_SAMPLE_SKIPPED_CLOSED;

	old_status_available = app->battery_status_available;
	old_low = app->battery_low;
	old_charging = app->battery_charging;
	old_level = app->battery_level;
	status_available =
		R_SUCCEEDED(PTMU_GetBatteryLevel(&level)) &&
		R_SUCCEEDED(PTMU_GetBatteryChargeState(&charge_state));
	if (!status_available)
		return APP_POWER_BATTERY_SAMPLE_READ_FAILED;

	charging = status_available && charge_state != 0;
	low = status_available && !charging && level <= APP_POWER_BATTERY_LOW_LEVEL;
	changed =
		old_status_available != status_available ||
		(
			status_available &&
			(
				old_level != level ||
				old_charging != charging ||
				old_low != low
			)
		);

	app->battery_status_available = true;
	app->battery_low = low;
	app->battery_charging = charging;
	app->battery_level = level;

	return changed ?
		APP_POWER_BATTERY_SAMPLE_CHANGED :
		APP_POWER_BATTERY_SAMPLE_UNCHANGED;
}

static void app_refresh_selected_deck_summary(struct app_state *app)
{
	if (app->selected_deck_index >= app->deck_index.count)
		return;

	deck_summary_from_session(
		&app->deck_summaries[app->selected_deck_index],
		app->load_result,
		app->settings_load_result,
		app->state_load_result,
		&app->session
	);
	app->deck_summaries[app->selected_deck_index].deck_load_report =
		app->load_report;
	app->deck_summaries[app->selected_deck_index].settings_load_report =
		app->settings_load_report;
	app->deck_summaries[app->selected_deck_index].state_load_report =
		app->state_load_report;
}

static void app_return_to_deck_select(struct app_state *app)
{
	app_refresh_selected_deck_summary(app);
	app_set_deck_selection_status(app);
	app->mode = APP_MODE_DECK_SELECT;
}

static void app_load_selected_deck(struct app_state *app)
{
	const struct deck_entry *entry;
	unsigned int today = app_time_current_day();

	app->current_day = today;
	app->load_report.line_number = 0;
	app->load_report.parse_result = DECK_PARSE_OK;

	if (app->deck_index.count == 0)
	{
		app_set_status(app, "No deck selected");
		app->mode = APP_MODE_DECK_SELECT;
		return;
	}

	if (app->selected_deck_index >= app->deck_index.count)
		app->selected_deck_index = 0;

	entry = deck_index_get(&app->deck_index, app->selected_deck_index);
	if (entry == NULL)
	{
		app->load_result = DECK_LOAD_NOT_FOUND;
		app->state_message = "Missing deck";
		app_set_status(app, "Missing deck");
		app->mode = APP_MODE_LOAD_ERROR;
		return;
	}

	copy_string(app->active_cards_path, sizeof(app->active_cards_path), entry->cards_path);
	copy_string(app->active_state_path, sizeof(app->active_state_path), entry->state_path);
	copy_string(
		app->active_review_log_path,
		sizeof(app->active_review_log_path),
		entry->review_log_path
	);
	copy_string(
		app->active_settings_path,
		sizeof(app->active_settings_path),
		entry->settings_path
	);
	deck_init(&app->deck, entry->display_name);
	app->revealed = false;
	reset_review_scroll(app);
	app_settings_default(&app->settings);
	app->settings_load_result = APP_SETTINGS_LOAD_NOT_FOUND;
	app_settings_load_report_clear(&app->settings_load_report);
	app->settings_save_result = APP_SETTINGS_SAVE_OK;
	app->state_load_result = REVIEW_STATE_LOAD_NOT_FOUND;
	review_state_load_report_clear(&app->state_load_report);
	app->state_save_result = REVIEW_STATE_SAVE_OK;
	app->state_message = "State: not loaded";
	app->settings_message = "settings not saved";
	app->load_result = deck_load_cards_with_report(
		&app->deck,
		app->active_cards_path,
		&app->load_report
	);

	if (app->load_result == DECK_LOAD_OK)
	{
		app->settings_load_result = app_settings_load_with_report(
			&app->settings,
			app->active_settings_path,
			&app->settings_load_report
		);
		scheduler_init(&app->session, app->deck.card_count, today);
		scheduler_set_daily_limits(
			&app->session,
			app->settings.new_limit,
			app->settings.review_limit
		);
		app->state_load_result = review_state_load_with_report(
			&app->deck,
			&app->session,
			app->active_state_path,
			&app->state_load_report
		);
		app->state_message = review_state_load_result_name(app->state_load_result);

		app->mode = app_review_mode_for_session(app);
		if (
			settings_load_result_needs_warning(app->settings_load_result) &&
			app->state_load_result == REVIEW_STATE_LOAD_UNMATCHED
		)
		{
			app_set_status(app, "Settings ignored; state fresh");
		}
		else if (app->state_load_result == REVIEW_STATE_LOAD_UNMATCHED)
		{
			app_set_status(app, "State unmatched; started fresh");
		}
		else if (settings_load_result_needs_warning(app->settings_load_result))
		{
			app_set_status(app, "Settings ignored; using defaults");
		}
		else if (app->mode == APP_MODE_REVIEW)
		{
			app_set_status(app, "Loaded deck");
		}
		else if (!app_state_allows_study(app))
		{
			app_set_status(app, "Reset bad state first");
		}
		else if (app_daily_limit_blocks_cards(app))
		{
			app_set_status(app, "Loaded; daily limit reached");
		}
		else
		{
			app_set_status(app, "Loaded; no cards due");
		}
	}
	else
	{
		app_set_status(app, "Deck load failed");
		app->mode = APP_MODE_LOAD_ERROR;
		scheduler_init(&app->session, 0, today);
	}

	app_refresh_selected_deck_summary(app);
}

static void app_open_actions(struct app_state *app)
{
	app->action_return_mode = app->mode;
	app->selected_action = app_state_allows_study(app) ?
		ACTION_ITEM_UNSUSPEND_ALL :
		ACTION_ITEM_RESET_PROGRESS;
	app_set_actions_status(app);
	app->mode = APP_MODE_ACTIONS;
}

static void app_open_settings(struct app_state *app)
{
	app->edited_settings = app->settings;
	app->selected_setting = SETTING_ITEM_NEW_LIMIT;
	app->settings_message = "no changes";
	app_set_settings_open_status(app);
	app->mode = APP_MODE_SETTINGS;
}

static void app_open_restore_confirmation(struct app_state *app)
{
	app_set_required_status(app, "Restore", "X", app->mode);
	app->mode = APP_MODE_CONFIRM_RESTORE;
}

static void app_open_reset_confirmation(struct app_state *app)
{
	app_set_required_status(app, "Reset", "X", app->mode);
	app->mode = APP_MODE_CONFIRM_RESET;
}

static void app_open_suspend_confirmation(struct app_state *app)
{
	app_set_required_status(app, "Suspend", "X", app->mode);
	app->mode = APP_MODE_CONFIRM_SUSPEND;
}

static void app_open_exit_confirmation(struct app_state *app)
{
	app->exit_return_mode = app->mode;
	if (app_exit_would_discard_unsaved_limits(app))
		app_set_status(app, "Exit loses unsaved limits");
	else
		app_set_required_status(app, "Exit", "A", app->exit_return_mode);
	app->mode = APP_MODE_CONFIRM_EXIT;
}

static void app_open_controls(struct app_state *app)
{
	app->controls_return_mode = app->mode;
	app->mode = APP_MODE_CONTROLS;
	app_set_controls_status(app);
}

static void app_close_controls(struct app_state *app)
{
	enum app_mode return_mode = app->controls_return_mode;

	app->mode = return_mode;
	app_set_controls_closed_status(app, return_mode);
}

static enum app_power_battery_sample_result app_init(struct app_state *app)
{
	enum app_power_battery_sample_result battery_sample_result;

	memset(app, 0, sizeof(*app));
	app->current_day = app_time_current_day();
	app->battery_service_available = R_SUCCEEDED(ptmuInit());
	battery_sample_result = app_sample_battery(app);
	app_set_status(app, "Ready");
	app->mode = APP_MODE_DECK_SELECT;
	return battery_sample_result;
}

static void draw_header(const struct app_state *app)
{
	draw_app_title("Review");
	draw_deck_name_line(app, 2, APP_LAYOUT_DECK_NAME_HEADER_WIDTH);
	printf(
		"\x1b[3;1HDue %lu  New %lu  Done %u",
		(unsigned long)app->session.due_count,
		(unsigned long)scheduler_new_due_count(&app->session),
		app->session.reviewed_count
	);
	printf(
		"\x1b[4;1HState: %s  Day %u",
		app->state_message,
		app->session.today
	);
}

static void draw_card_status(const struct app_state *app, const struct scheduler_card *state)
{
	printf(
		"\x1b[5;1HCard %lu/%lu  Int %ud  Ease %u.%02u",
		(unsigned long)(scheduler_current_index(&app->session) + 1),
		(unsigned long)app->session.card_count,
		state->interval_days,
		state->ease_permille / 1000,
		(state->ease_permille % 1000) / 10
	);
}

static void draw_deck_select_screen(const struct app_state *app)
{
	size_t first_visible_deck = 0;
	size_t visible_deck_count;

	app_console_clear();
	draw_app_title(NULL);
	printf("\x1b[3;1H" APP_COLOR_ACCENT "Select deck" APP_COLOR_RESET);
	if (app->deck_index.count > 0)
	{
		printf(
			" %lu/%lu",
			(unsigned long)(app->selected_deck_index + 1),
			(unsigned long)app->deck_index.count
		);
	}

	if (app->deck_index.count == 0)
	{
		printf("\x1b[5;1H" APP_COLOR_WARNING "No decks found." APP_COLOR_RESET);
		printf("\x1b[7;1HCreate a folder like:");
		printf("\x1b[8;1H%s/my-deck/cards.tsv", DECK_INDEX_ROOT_PATH);
		if (app->deck_index.ignored_count > 0)
		{
			printf(
				"\x1b[10;1HIgnored %lu entries.",
				(unsigned long)app->deck_index.ignored_count
			);
			printf("\x1b[11;1HUse letters, digits, _ -");
		}
	}
	else
	{
		if (app->selected_deck_index >= APP_LAYOUT_DECK_SELECTOR_VISIBLE_ROWS)
		{
			first_visible_deck =
				app->selected_deck_index - APP_LAYOUT_DECK_SELECTOR_VISIBLE_ROWS + 1;
		}
		visible_deck_count = app->deck_index.count - first_visible_deck;
		if (visible_deck_count > APP_LAYOUT_DECK_SELECTOR_VISIBLE_ROWS)
			visible_deck_count = APP_LAYOUT_DECK_SELECTOR_VISIBLE_ROWS;

		for (size_t visible_index = 0; visible_index < visible_deck_count; visible_index++)
		{
			size_t index = first_visible_deck + visible_index;
			const char *marker = index == app->selected_deck_index ? ">" : " ";
			bool selected = index == app->selected_deck_index;
			const struct deck_summary *summary = &app->deck_summaries[index];

			if (selected)
				printf(APP_COLOR_SELECTED);
			printf(
				"\x1b[%lu;1H%s ",
				(unsigned long)(APP_LAYOUT_DECK_SELECTOR_FIRST_ROW + visible_index),
				marker
			);
			print_truncated(
				app->deck_index.entries[index].display_name,
				APP_LAYOUT_DECK_NAME_SELECTOR_WIDTH
			);
			if (
				summary->deck_load_result == DECK_LOAD_OK &&
				deck_summary_state_allows_study(summary)
			)
			{
				draw_deck_selector_study_suffix(summary);
			}
			else if (summary->deck_load_result == DECK_LOAD_OK)
			{
				printf(APP_COLOR_RESET APP_COLOR_DANGER " state error" APP_COLOR_RESET);
			}
			else
			{
				printf(APP_COLOR_RESET APP_COLOR_DANGER " load error" APP_COLOR_RESET);
			}
			printf(APP_COLOR_RESET);
		}

		if (first_visible_deck > 0)
			printf("\x1b[4;1H... %lu above", (unsigned long)first_visible_deck);
		if (first_visible_deck + visible_deck_count < app->deck_index.count)
			printf(
				"\x1b[21;1H... %lu below",
				(unsigned long)(
					app->deck_index.count - first_visible_deck - visible_deck_count
				)
			);
		if (app->deck_index.overflowed)
		{
			printf(
				"\x1b[23;1HShowing %lu/%lu decks. Ignored %lu.",
				(unsigned long)app->deck_index.count,
				(unsigned long)app->deck_index.total_count,
				(unsigned long)app->deck_index.ignored_count
			);
		}
		else if (app->deck_index.ignored_count > 0)
		{
			printf(
				"\x1b[23;1HIgnored %lu entries.",
				(unsigned long)app->deck_index.ignored_count
			);
		}
	}
}

static void draw_load_error_screen(const struct app_state *app)
{
	app_console_clear();
	draw_app_title(NULL);
	printf("\x1b[3;1H" APP_COLOR_DANGER "Could not load deck." APP_COLOR_RESET);
	printf("\x1b[5;1H");
	print_truncated(
		app->active_cards_path[0] ? app->active_cards_path : DECK_INDEX_ROOT_PATH,
		APP_LAYOUT_TEXT_WIDTH
	);
	printf("\x1b[7;1HResult: %s", deck_load_result_name(app->load_result));
	draw_load_error_detail(app, 9);
	printf("\x1b[12;1HFix cards.tsv at the path above.");
}

static void draw_review_screen(const struct app_state *app)
{
	const struct card *card = current_card(app);
	size_t max_scroll_offset;

	app_console_clear();
	draw_header(app);

	if (card == NULL)
	{
		printf("\x1b[6;1HNo cards are due today.");
		return;
	}

	max_scroll_offset = review_max_scroll_offset(app);
	draw_card_status(app, &app->session.cards[scheduler_current_index(&app->session)]);
	printf("\x1b[7;1H" APP_COLOR_ACCENT "Front" APP_COLOR_RESET);
	printf("\x1b[8;1H" APP_COLOR_RULE "------------------------------------------------" APP_COLOR_RESET);

	if (app->revealed)
	{
		draw_wrapped_text_columns(
			card->front,
			APP_LAYOUT_REVIEW_FRONT_TEXT_ROW,
			APP_LAYOUT_REVIEW_REVEALED_FRONT_TEXT_ROWS,
			APP_LAYOUT_TEXT_WIDTH,
			0
		);
		printf("\x1b[15;1H" APP_COLOR_ACCENT "Back" APP_COLOR_RESET);
		draw_review_scroll_hint(15, app->review_scroll_offset, max_scroll_offset);
		printf("\x1b[16;1H" APP_COLOR_RULE "------------------------------------------------" APP_COLOR_RESET);
		draw_wrapped_text_columns(
			card->back,
			APP_LAYOUT_REVIEW_BACK_TEXT_ROW,
			APP_LAYOUT_REVIEW_BACK_TEXT_ROWS,
			APP_LAYOUT_TEXT_WIDTH,
			app->review_scroll_offset
		);
	}
	else
	{
		draw_review_scroll_hint(7, app->review_scroll_offset, max_scroll_offset);
		draw_wrapped_text_columns(
			card->front,
			APP_LAYOUT_REVIEW_FRONT_TEXT_ROW,
			APP_LAYOUT_REVIEW_FRONT_TEXT_ROWS,
			APP_LAYOUT_TEXT_WIDTH,
			app->review_scroll_offset
		);
	}
}

static void draw_summary_screen(const struct app_state *app)
{
	const struct scheduler_session *session = &app->session;
	size_t new_blocked_count = app_new_limit_blocked_count(app);
	size_t review_blocked_count = app_review_limit_blocked_count(app);
	bool daily_limit_reached = new_blocked_count > 0 || review_blocked_count > 0;

	app_console_clear();
	draw_app_title("Review");
	draw_deck_name_line(app, 2, APP_LAYOUT_DECK_NAME_HEADER_WIDTH);
	if (!app_state_allows_study(app))
	{
		printf("\x1b[3;1H" APP_COLOR_DANGER "Review state error" APP_COLOR_RESET);
		printf("\x1b[5;1HCards:         %lu", (unsigned long)session->card_count);
		printf("\x1b[7;1HState:         %s", app->state_message);
		draw_review_state_load_error_detail(
			app->state_load_result,
			&app->state_load_report,
			8
		);
		printf("\x1b[10;1HUse SELECT actions, then");
		printf("\x1b[11;1Hreset deck progress.");
		if (settings_load_result_needs_warning(app->settings_load_result))
		{
			printf("\x1b[13;1HSettings:      ignored");
			draw_settings_load_error_detail(
				app->settings_load_result,
				&app->settings_load_report,
				14
			);
		}
		return;
	}

	printf(
		"\x1b[3;1H%s%s" APP_COLOR_RESET,
		daily_limit_reached ? APP_COLOR_WARNING : APP_COLOR_SUCCESS,
		daily_limit_reached ? "Daily limit reached" : "No cards due now"
	);
	printf("\x1b[5;1HCards:         %lu", (unsigned long)session->card_count);
	printf("\x1b[6;1HRated session: %u", session->reviewed_count);
	printf(
		"\x1b[7;1HCards today:   %lu",
		(unsigned long)scheduler_reviewed_today_count(session)
	);
	printf("\x1b[8;1HTotal reviews: %u", review_count_total(session));
	printf(
		"\x1b[9;1HSuspended:     %lu",
		(unsigned long)scheduler_suspended_count(session)
	);
	if (daily_limit_reached)
	{
		printf(
			"\x1b[10;1HPast limit:    N %lu  R %lu",
			(unsigned long)new_blocked_count,
			(unsigned long)review_blocked_count
		);
	}
	printf("\x1b[11;1HState:         %s", app->state_message);
	printf(
		"\x1b[13;1H" APP_COLOR_DANGER "Y Again" APP_COLOR_RESET ": %u",
		session->rating_counts[SCHEDULER_RATING_AGAIN]
	);
	printf(
		"\x1b[14;1H" APP_COLOR_WARNING "X Hard" APP_COLOR_RESET ":  %u",
		session->rating_counts[SCHEDULER_RATING_HARD]
	);
	printf(
		"\x1b[15;1H" APP_COLOR_SUCCESS "B Good" APP_COLOR_RESET ":  %u",
		session->rating_counts[SCHEDULER_RATING_GOOD]
	);
	printf(
		"\x1b[16;1H" APP_COLOR_EASY "A Easy" APP_COLOR_RESET ":  %u",
		session->rating_counts[SCHEDULER_RATING_EASY]
	);
}

static void draw_actions_screen(const struct app_state *app)
{
	const char *unsuspend_marker =
		app->selected_action == ACTION_ITEM_UNSUSPEND_ALL ? ">" : " ";
	const char *settings_marker =
		app->selected_action == ACTION_ITEM_DAILY_LIMITS ? ">" : " ";
	const char *reset_marker =
		app->selected_action == ACTION_ITEM_RESET_PROGRESS ? ">" : " ";

	app_console_clear();
	draw_app_title("Review");
	printf("\x1b[3;1H" APP_COLOR_ACCENT "Actions" APP_COLOR_RESET);
	printf(
		"\x1b[6;1H%s%s Restore suspended cards" APP_COLOR_RESET,
		app->selected_action == ACTION_ITEM_UNSUSPEND_ALL ? APP_COLOR_SELECTED : "",
		unsuspend_marker
	);
	printf(
		"\x1b[8;1H%s%s Daily limits" APP_COLOR_RESET,
		app->selected_action == ACTION_ITEM_DAILY_LIMITS ? APP_COLOR_SELECTED : "",
		settings_marker
	);
	printf(
		"\x1b[10;1H%s%s Reset deck progress" APP_COLOR_RESET,
		app->selected_action == ACTION_ITEM_RESET_PROGRESS ? APP_COLOR_DANGER : "",
		reset_marker
	);
	draw_deck_name_line(app, 13, APP_LAYOUT_DECK_NAME_HEADER_WIDTH);

	if (app->selected_action == ACTION_ITEM_UNSUSPEND_ALL)
	{
		printf(
			"\x1b[16;1HSuspended cards: %lu",
			(unsigned long)scheduler_suspended_count(&app->session)
		);
		printf("\x1b[17;1HClears all suspended flags");
		printf("\x1b[18;1Hfor the active deck.");
		printf("\x1b[19;1HRequires X if any exist.");
	}
	else if (app->selected_action == ACTION_ITEM_DAILY_LIMITS)
	{
		printf("\x1b[16;1HChange new and review");
		printf("\x1b[17;1Hlimits for this deck.");
	}
	else
	{
		printf("\x1b[16;1HRemoves saved progress for");
		printf("\x1b[17;1Hthe active deck, then reloads.");
	}

	printf("\x1b[21;1HState: %s", app->state_message);
}

static void draw_settings_screen(const struct app_state *app)
{
	const char *new_marker =
		app->selected_setting == SETTING_ITEM_NEW_LIMIT ? ">" : " ";
	const char *review_marker =
		app->selected_setting == SETTING_ITEM_REVIEW_LIMIT ? ">" : " ";
	char new_limit[16];
	char review_limit[16];
	int save_row = 18;
	bool unsaved_changes = app_settings_have_unsaved_changes(app);

	format_daily_limit(
		new_limit,
		sizeof(new_limit),
		app->edited_settings.new_limit
	);
	format_daily_limit(
		review_limit,
		sizeof(review_limit),
		app->edited_settings.review_limit
	);

	app_console_clear();
	draw_app_title("Review");
	printf("\x1b[3;1H" APP_COLOR_ACCENT "Daily limits" APP_COLOR_RESET);
	draw_deck_name_line(app, 5, APP_LAYOUT_DECK_NAME_HEADER_WIDTH);
	printf(
		"\x1b[8;1H%s%s New cards:    %s" APP_COLOR_RESET,
		app->selected_setting == SETTING_ITEM_NEW_LIMIT ? APP_COLOR_SELECTED : "",
		new_marker,
		new_limit
	);
	printf(
		"\x1b[10;1H%s%s Review cards: %s" APP_COLOR_RESET,
		app->selected_setting == SETTING_ITEM_REVIEW_LIMIT ? APP_COLOR_SELECTED : "",
		review_marker,
		review_limit
	);
	printf("\x1b[13;1H0 means all available cards.");
	printf(
		"\x1b[16;1HCurrent source: %s",
		app_settings_load_result_name(app->settings_load_result)
	);
	if (
		draw_settings_load_error_detail(
			app->settings_load_result,
			&app->settings_load_report,
			17
		)
	)
	{
		save_row = 19;
	}
	printf(
		"\x1b[%d;1H%sSave: %s" APP_COLOR_RESET,
		save_row,
		unsaved_changes ? APP_COLOR_WARNING : "",
		app->settings_message
	);
	if (unsaved_changes)
		printf("\x1b[%d;1HPress A to save changes.", save_row + 1);
}

static void draw_reset_confirmation_screen(const struct app_state *app)
{
	app_console_clear();
	draw_app_title("Review");
	printf("\x1b[3;1H" APP_COLOR_DANGER "Reset deck progress?" APP_COLOR_RESET);
	draw_deck_name_line(app, 5, APP_LAYOUT_DECK_NAME_HEADER_WIDTH);
	printf("\x1b[8;1HThis removes saved review");
	printf("\x1b[9;1Hstate for this deck.");
	printf("\x1b[12;1HCards stay in cards.tsv.");
	printf("\x1b[15;1HUse " APP_COLOR_DANGER "X" APP_COLOR_RESET " to reset.");
	printf("\x1b[17;1HUse B or SELECT to cancel.");
}

static void draw_restore_confirmation_screen(const struct app_state *app)
{
	app_console_clear();
	draw_app_title("Review");
	printf("\x1b[3;1H" APP_COLOR_WARNING "Restore suspended cards?" APP_COLOR_RESET);
	draw_deck_name_line(app, 5, APP_LAYOUT_DECK_NAME_HEADER_WIDTH);
	printf(
		"\x1b[8;1HSuspended: %lu",
		(unsigned long)scheduler_suspended_count(&app->session)
	);
	printf("\x1b[11;1HRestored cards can become");
	printf("\x1b[12;1Hdue again if scheduled.");
	printf("\x1b[15;1HUse " APP_COLOR_SUCCESS "X" APP_COLOR_RESET " to restore.");
	printf("\x1b[17;1HUse B or SELECT to cancel.");
}

static void draw_suspend_confirmation_screen(const struct app_state *app)
{
	const struct card *card = current_card(app);

	app_console_clear();
	draw_app_title("Review");
	printf("\x1b[3;1H" APP_COLOR_WARNING "Suspend current card?" APP_COLOR_RESET);
	draw_deck_name_line(app, 5, APP_LAYOUT_DECK_NAME_HEADER_WIDTH);
	if (card != NULL)
	{
		printf("\x1b[8;1HCard: ");
		print_truncated(card->front, APP_LAYOUT_TEXT_WIDTH - 6);
	}
	printf("\x1b[11;1HThis hides the card from");
	printf("\x1b[12;1Hreview until restored.");
	printf("\x1b[15;1HUse " APP_COLOR_WARNING "X" APP_COLOR_RESET " to suspend.");
	printf("\x1b[17;1HUse B or SELECT to cancel.");
}

static void draw_exit_confirmation_screen(const struct app_state *app)
{
	app_console_clear();
	draw_app_title("Review");
	printf("\x1b[3;1H" APP_COLOR_WARNING "Exit app?" APP_COLOR_RESET);
	if (app_exit_would_discard_unsaved_limits(app))
	{
		printf("\x1b[6;1H" APP_COLOR_WARNING "Unsaved daily-limit edits" APP_COLOR_RESET);
		printf("\x1b[7;1Hwill be lost.");
	}
	else
	{
		printf("\x1b[6;1HProgress is saved after");
		printf("\x1b[7;1Heach review action.");
	}
	printf("\x1b[10;1HUse A to exit.");
	printf("\x1b[12;1HUse B or SELECT to cancel.");
}

static void draw_controls_screen_footer(void)
{
	printf("\x1b[21;1HSTART: confirm exit");
	printf("\x1b[24;1HHere: B, Y, or SELECT returns.");
}

static void draw_controls_screen(const struct app_state *app)
{
	app_console_clear();
	draw_app_title(NULL);

	switch (app->controls_return_mode)
	{
	case APP_MODE_DECK_SELECT:
		printf("\x1b[3;1H" APP_COLOR_ACCENT "Deck list controls" APP_COLOR_RESET);
		if (app->deck_index.count > 0)
		{
			printf("\x1b[5;1HA: open selected deck");
			printf("\x1b[7;1HD-pad U/D move, L/R page");
			printf("\x1b[9;1HHold direction to repeat");
			printf("\x1b[11;1HSELECT: rescan decks");
			printf("\x1b[13;1HY: controls");
		}
		else
		{
			printf("\x1b[5;1HSELECT: rescan decks");
			printf("\x1b[7;1HY: controls");
		}
		break;
	case APP_MODE_LOAD_ERROR:
		printf("\x1b[3;1H" APP_COLOR_DANGER "Load error controls" APP_COLOR_RESET);
		printf("\x1b[5;1HB or SELECT: deck list");
		printf("\x1b[7;1HY: controls");
		break;
	case APP_MODE_SUMMARY:
		if (!app_state_allows_study(app))
		{
			printf(
				"\x1b[3;1H" APP_COLOR_DANGER
				"Review state controls" APP_COLOR_RESET
			);
			printf("\x1b[5;1HSELECT: actions");
			printf("\x1b[7;1HB: deck list");
			printf("\x1b[9;1HY: controls");
			printf("\x1b[13;1HReset progress to study.");
		}
		else
		{
			printf(
				"\x1b[3;1H" APP_COLOR_SUCCESS
				"No-due controls" APP_COLOR_RESET
			);
			printf("\x1b[5;1HB: deck list");
			printf("\x1b[7;1HL: undo last action");
			printf("\x1b[9;1HSELECT: actions");
			printf("\x1b[11;1HY: controls");
		}
		break;
	case APP_MODE_ACTIONS:
		printf("\x1b[3;1H" APP_COLOR_ACCENT "Actions controls" APP_COLOR_RESET);
		printf("\x1b[5;1HA: choose selected");
		printf("\x1b[7;1HD-pad/Circle U/D: move/hold");
		printf("\x1b[9;1HB or SELECT: cancel");
		printf("\x1b[11;1HY: controls");
		break;
	case APP_MODE_SETTINGS:
		printf("\x1b[3;1H" APP_COLOR_ACCENT "Daily-limit controls" APP_COLOR_RESET);
		printf("\x1b[5;1HD-pad/Circle U/D: field");
		printf("\x1b[7;1HD-pad/Circle L/R: value/hold");
		printf("\x1b[9;1HA: save limits");
		printf("\x1b[11;1HB or SELECT: actions");
		printf("\x1b[13;1HY: controls");
		if (app_settings_have_unsaved_changes(app))
		{
			printf(
				"\x1b[15;1H" APP_COLOR_WARNING
				"Unsaved limit edits" APP_COLOR_RESET
			);
		}
		break;
	case APP_MODE_REVIEW:
	default:
		if (app->revealed)
		{
			printf(
				"\x1b[3;1H" APP_COLOR_ACCENT
				"Review rating controls" APP_COLOR_RESET
			);
			printf(
				"\x1b[5;1H" APP_COLOR_DANGER "Y: Again" APP_COLOR_RESET
				"      " APP_COLOR_WARNING "X: Hard" APP_COLOR_RESET
			);
			printf(
				"\x1b[7;1H" APP_COLOR_SUCCESS "B: Good" APP_COLOR_RESET
				"       " APP_COLOR_EASY "A: Easy" APP_COLOR_RESET
			);
			printf("\x1b[9;1HL: undo last action");
			printf("\x1b[11;1HR: confirm suspend");
			printf("\x1b[13;1HSELECT: actions");
			printf(
				"\x1b[15;1H" APP_COLOR_WARNING
				"Use one rating button only." APP_COLOR_RESET
			);
			printf("\x1b[17;1HD-pad U/D: scroll back");
		}
		else
		{
			printf(
				"\x1b[3;1H" APP_COLOR_ACCENT
				"Review front controls" APP_COLOR_RESET
			);
			printf("\x1b[5;1HA: show answer");
			printf("\x1b[7;1HB: deck list");
			printf("\x1b[9;1HL: undo last action");
			printf("\x1b[11;1HR: confirm suspend");
			printf("\x1b[13;1HSELECT: actions");
			printf("\x1b[15;1HY: controls");
			printf("\x1b[17;1HD-pad U/D: scroll front");
		}
		break;
	}

	draw_controls_screen_footer();
}

static void draw_battery_status(const struct app_state *app)
{
	enum app_power_battery_display_state display_state =
		app_power_battery_display_state(
			app->battery_status_available,
			app->battery_charging,
			app->battery_level
		);

	if (display_state == APP_POWER_BATTERY_DISPLAY_UNAVAILABLE)
	{
		printf(
			"\x1b[29;1H" APP_COLOR_WARNING
			"Battery: unavailable" APP_COLOR_RESET
		);
	}
	else if (display_state == APP_POWER_BATTERY_DISPLAY_CHARGING)
	{
		printf(
			"\x1b[29;1H" APP_COLOR_SUCCESS "Battery: %u/5 charging" APP_COLOR_RESET,
			(unsigned int)app->battery_level
		);
	}
	else if (display_state == APP_POWER_BATTERY_DISPLAY_LOW)
	{
		printf(
			"\x1b[29;1H" APP_COLOR_DANGER "Battery: %u/5 low. Charge soon." APP_COLOR_RESET,
			(unsigned int)app->battery_level
		);
	}
	else
	{
		printf(
			"\x1b[29;1H" APP_COLOR_NEUTRAL "Battery: %u/5" APP_COLOR_RESET,
			(unsigned int)app->battery_level
		);
	}
}

static void draw_due_legend(int row, bool include_suspended)
{
	printf(
		"\x1b[%d;1H" APP_COLOR_NEW "N" APP_COLOR_RESET
		" new  " APP_COLOR_LEARNING "L" APP_COLOR_RESET
		" learn  " APP_COLOR_REVIEW "R" APP_COLOR_RESET " review",
		row
	);
	if (include_suspended)
		printf(
			"\x1b[%d;1H" APP_COLOR_SUSPENDED "S" APP_COLOR_RESET " suspended",
			row + 1
		);
}

static void draw_status_message(const struct app_state *app)
{
	if (app->status_message[0] == '\0')
		return;

	printf("\x1b[24;1H" APP_COLOR_ACCENT "Status:" APP_COLOR_RESET " ");
	printf(
		"%s",
		status_color_escape(app_status_message_color(app->status_message))
	);
	print_truncated(app->status_message, APP_LAYOUT_STATUS_MESSAGE_WIDTH);
	printf(APP_COLOR_RESET);
}

static void draw_scanning_screen(const struct app_state *app)
{
	draw_scanning_progress_screen(app, 0, 0, NULL);
}

static void draw_scanning_progress_screen(
	const struct app_state *app,
	size_t loaded_count,
	size_t total_count,
	const char *deck_name
)
{
	select_top_screen();
	app_console_clear();
	draw_app_title(NULL);
	printf("\x1b[3;1H" APP_COLOR_ACCENT "Scanning decks..." APP_COLOR_RESET);
	printf("\x1b[5;1H%s", DECK_INDEX_ROOT_PATH);
	if (total_count > 0)
	{
		printf(
			"\x1b[7;1HLoading summary %lu/%lu",
			(unsigned long)(loaded_count + 1),
			(unsigned long)total_count
		);
		if (deck_name != NULL && deck_name[0] != '\0')
		{
			printf("\x1b[9;1HDeck: ");
			print_truncated(deck_name, APP_LAYOUT_DECK_NAME_HEADER_WIDTH);
		}
	}
	else
	{
		printf("\x1b[7;1HReading deck folders");
	}

	select_bottom_screen();
	app_console_clear();
	printf("\x1b[1;1H" APP_COLOR_ACCENT "Deck scan" APP_COLOR_RESET);
	if (total_count > 0)
	{
		printf(
			"\x1b[3;1HLoaded: %lu/%lu summaries",
			(unsigned long)loaded_count,
			(unsigned long)total_count
		);
	}
	else
	{
		printf("\x1b[3;1HReading SD card");
	}
	draw_battery_status(app);
	select_top_screen();
}

static void present_current_frame(void)
{
	gfxFlushBuffers();
	gspWaitForVBlank();
	gfxSwapBuffers();
}

static void show_scan_then_scan_decks(struct app_state *app)
{
	app_set_status(app, "Scanning decks");
	draw_scanning_screen(app);
	present_current_frame();
	app_scan_decks(app);
	app_set_scan_complete_status(app);
}

static void app_update_review_return_modes_for_day_change(
	struct app_state *app,
	enum app_mode target_mode
)
{
	if (app_mode_is_review_surface(app->action_return_mode))
		app->action_return_mode = target_mode;
	if (app_mode_is_review_surface(app->controls_return_mode))
		app->controls_return_mode = target_mode;
	if (app_mode_is_review_surface(app->exit_return_mode))
		app->exit_return_mode = target_mode;
}

static bool app_refresh_day_if_changed(struct app_state *app, unsigned int today)
{
	enum app_mode target_mode;

	if (today == 0 || today == app->current_day)
		return false;

	app->current_day = today;

	if (app->mode == APP_MODE_DECK_SELECT)
	{
		show_scan_then_scan_decks(app);
		return true;
	}

	if (app->load_result != DECK_LOAD_OK)
		return false;

	scheduler_set_today(&app->session, today);
	app->revealed = false;
	reset_review_scroll(app);
	app_refresh_selected_deck_summary(app);
	target_mode = app_review_mode_for_session(app);
	app_update_review_return_modes_for_day_change(app, target_mode);

	if (app_mode_is_review_surface(app->mode))
		app->mode = target_mode;

	app_set_day_change_status(app);
	return true;
}

static void draw_bottom_controls_screen(const struct app_state *app)
{
	app_console_clear();

	switch (app->mode)
	{
	case APP_MODE_DECK_SELECT:
		if (app->deck_index.count > 0)
		{
			printf(
				"\x1b[1;1H" APP_COLOR_ACCENT "Decks" APP_COLOR_RESET " %lu/%lu",
				(unsigned long)(app->selected_deck_index + 1),
				(unsigned long)app->deck_index.count
			);
		}
		else
		{
			printf("\x1b[1;1H" APP_COLOR_ACCENT "Decks" APP_COLOR_RESET);
		}
		if (app->deck_index.count > 0)
		{
			const struct deck_summary *summary =
				&app->deck_summaries[app->selected_deck_index];

			printf("\x1b[3;1HA: open selected deck");
			printf("\x1b[5;1HD-pad U/D move, L/R page");
			printf("\x1b[7;1HHold direction to repeat");
			printf("\x1b[9;1HSELECT: rescan decks");
			printf("\x1b[11;1HSTART: confirm exit");
			printf("\x1b[13;1HY: controls");
			if (
				summary->deck_load_result == DECK_LOAD_OK &&
				deck_summary_state_allows_study(summary)
			)
			{
				int details_row = 14;

				if (settings_load_result_needs_warning(summary->settings_load_result))
				{
					printf(
						"\x1b[14;1H" APP_COLOR_WARNING
						"Settings ignored; defaults" APP_COLOR_RESET
					);
					details_row = 16;
					if (
						draw_settings_load_error_detail(
							summary->settings_load_result,
							&summary->settings_load_report,
							15
						)
					)
					{
						details_row = 17;
					}
				}
				if (state_load_result_needs_warning(summary->state_load_result))
				{
					printf(
						"\x1b[%d;1H" APP_COLOR_WARNING
						"State unmatched; fresh" APP_COLOR_RESET,
						details_row
					);
					details_row += 2;
				}
				printf(
					"\x1b[%d;1HDue: N %lu  L %lu  R %lu",
					details_row,
					(unsigned long)summary->new_due_count,
					(unsigned long)summary->learning_due_count,
					(unsigned long)summary->review_due_count
				);
				printf(
					"\x1b[%d;1HTotal due: %lu",
					details_row + 2,
					(unsigned long)summary->due_count
				);
				if (deck_summary_daily_limit_blocks_cards(summary))
				{
					printf(
						"\x1b[%d;1H" APP_COLOR_WARNING
						"Past limit: N %lu  R %lu" APP_COLOR_RESET,
						details_row + 3,
						(unsigned long)summary->new_limit_blocked_count,
						(unsigned long)summary->review_limit_blocked_count
					);
				}
				printf(
					"\x1b[%d;1HCards: %lu  Suspended: %lu",
					details_row + 4,
					(unsigned long)summary->card_count,
					(unsigned long)summary->suspended_count
				);
				if (details_row <= 16)
					draw_due_legend(details_row + 6, true);
				else if (details_row == 17)
					draw_due_legend(details_row + 6, false);
			}
			else if (summary->deck_load_result == DECK_LOAD_OK)
			{
				printf(
					"\x1b[14;1HState: " APP_COLOR_DANGER "%s" APP_COLOR_RESET,
					review_state_load_result_name(summary->state_load_result)
				);
				draw_review_state_load_error_detail(
					summary->state_load_result,
					&summary->state_load_report,
					15
				);
				printf(
					"\x1b[16;1H" APP_COLOR_WARNING
					"Open deck, then reset progress." APP_COLOR_RESET
				);
				printf(
					"\x1b[18;1HCards: %lu",
					(unsigned long)summary->card_count
				);
				if (settings_load_result_needs_warning(summary->settings_load_result))
				{
					printf("\x1b[20;1HSettings: ignored");
					draw_settings_load_error_detail(
						summary->settings_load_result,
						&summary->settings_load_report,
						21
					);
				}
			}
			else
			{
				printf(
					"\x1b[14;1H" APP_COLOR_DANGER
					"Selected deck load error" APP_COLOR_RESET
				);
				draw_deck_load_error_detail(
					summary->deck_load_result,
					&summary->deck_load_report,
					16
				);
			}
		}
		else
		{
			printf("\x1b[3;1HSELECT: rescan decks");
			printf("\x1b[5;1HSTART: confirm exit");
			printf("\x1b[7;1HY: controls");
		}
		if (app->deck_index.overflowed)
		{
			printf(
				"\x1b[27;1HFound: %lu/%lu  Ignored: %lu",
				(unsigned long)app->deck_index.count,
				(unsigned long)app->deck_index.total_count,
				(unsigned long)app->deck_index.ignored_count
			);
		}
		else if (app->deck_index.ignored_count > 0)
		{
			printf(
				"\x1b[27;1HFound: %lu  Ignored: %lu",
				(unsigned long)app->deck_index.count,
				(unsigned long)app->deck_index.ignored_count
			);
		}
		else
		{
			printf("\x1b[27;1HFound: %lu", (unsigned long)app->deck_index.count);
		}
		break;
	case APP_MODE_LOAD_ERROR:
		printf("\x1b[1;1H" APP_COLOR_DANGER "Load error" APP_COLOR_RESET);
		printf("\x1b[3;1HB or SELECT: deck list");
		printf("\x1b[5;1HSTART: confirm exit");
		printf("\x1b[7;1HY: controls");
		draw_load_error_detail(app, 10);
		break;
	case APP_MODE_REVIEW:
	{
		char new_limit[16];
		char review_limit[16];
		size_t max_scroll_offset = review_max_scroll_offset(app);

		format_daily_limit(new_limit, sizeof(new_limit), app->session.new_limit);
		format_daily_limit(review_limit, sizeof(review_limit), app->session.review_limit);
		printf("\x1b[1;1H" APP_COLOR_ACCENT "Review" APP_COLOR_RESET);
		printf(
			"\x1b[3;1HDue: N %lu  L %lu  R %lu",
			(unsigned long)scheduler_new_due_count(&app->session),
			(unsigned long)scheduler_learning_due_count(&app->session),
			(unsigned long)scheduler_review_due_count(&app->session)
		);
		printf(
			"\x1b[4;1HStarted: N %u/%s  R %u/%s",
			app->session.new_count_today,
			new_limit,
			app->session.review_count_today,
			review_limit
		);
		draw_due_legend(18, false);

		if (app->revealed)
		{
			printf(
				"\x1b[6;1H" APP_COLOR_DANGER "Y: Again" APP_COLOR_RESET
				"      " APP_COLOR_WARNING "X: Hard" APP_COLOR_RESET
			);
			printf(
				"\x1b[8;1H" APP_COLOR_SUCCESS "B: Good" APP_COLOR_RESET
				"       " APP_COLOR_EASY "A: Easy" APP_COLOR_RESET
			);
			printf("\x1b[10;1HL: undo last action");
			printf("\x1b[12;1HR: confirm suspend");
			printf("\x1b[14;1HSELECT: actions");
			printf("\x1b[16;1HSTART: confirm exit");
			printf(
				"\x1b[20;1H" APP_COLOR_WARNING
				"Use one rating button only." APP_COLOR_RESET
			);
		}
		else
		{
			printf("\x1b[6;1HA: show answer");
			printf("\x1b[8;1HB: deck list");
			printf("\x1b[10;1HL: undo last action");
			printf("\x1b[12;1HR: confirm suspend");
			printf("\x1b[14;1HSELECT: actions");
			printf("\x1b[16;1HSTART: confirm exit");
			printf("\x1b[20;1HY: controls");
		}
		if (max_scroll_offset > 0)
		{
			printf(
				"\x1b[22;1HD-pad U/D: text %lu/%lu",
				(unsigned long)(app->review_scroll_offset + 1),
				(unsigned long)(max_scroll_offset + 1)
			);
		}
		printf(
			"\x1b[27;1HSettings: %s",
			app_settings_load_result_name(app->settings_load_result)
		);
		draw_settings_load_error_detail(
			app->settings_load_result,
			&app->settings_load_report,
			28
		);
		break;
	}
	case APP_MODE_SUMMARY:
	{
		char new_limit[16];
		char review_limit[16];
		size_t new_blocked_count;
		size_t review_blocked_count;
		bool daily_limit_reached;

		if (!app_state_allows_study(app))
		{
			printf("\x1b[1;1H" APP_COLOR_DANGER "Review state error" APP_COLOR_RESET);
			printf("\x1b[3;1HSELECT: actions");
			printf("\x1b[5;1HB: deck list");
			printf("\x1b[7;1HSTART: confirm exit");
			printf("\x1b[9;1HY: controls");
			draw_review_state_load_error_detail(
				app->state_load_result,
				&app->state_load_report,
				11
			);
			printf(
				"\x1b[13;1H" APP_COLOR_WARNING
				"Reset progress to study." APP_COLOR_RESET
			);
			if (settings_load_result_needs_warning(app->settings_load_result))
			{
				printf("\x1b[15;1HSettings: ignored");
				draw_settings_load_error_detail(
					app->settings_load_result,
					&app->settings_load_report,
					16
				);
			}
			break;
		}

		format_daily_limit(new_limit, sizeof(new_limit), app->session.new_limit);
		format_daily_limit(review_limit, sizeof(review_limit), app->session.review_limit);
		new_blocked_count = app_new_limit_blocked_count(app);
		review_blocked_count = app_review_limit_blocked_count(app);
		daily_limit_reached = new_blocked_count > 0 || review_blocked_count > 0;
		printf(
			"\x1b[1;1H%s%s" APP_COLOR_RESET,
			daily_limit_reached ? APP_COLOR_WARNING : APP_COLOR_SUCCESS,
			daily_limit_reached ? "Daily limit reached" : "No cards due now"
		);
		printf("\x1b[3;1HB: deck list");
		printf("\x1b[5;1HL: undo last action");
		printf("\x1b[7;1HSELECT: actions");
		printf("\x1b[9;1HSTART: confirm exit");
		printf("\x1b[17;1HY: controls");
		printf(
			"\x1b[11;1HStarted: N %u/%s  R %u/%s",
			app->session.new_count_today,
			new_limit,
			app->session.review_count_today,
			review_limit
		);
		if (daily_limit_reached)
		{
			printf(
				"\x1b[12;1HPast limit: N %lu  R %lu",
				(unsigned long)new_blocked_count,
				(unsigned long)review_blocked_count
			);
		}
		draw_due_legend(15, false);
		printf(
			"\x1b[13;1HSettings: %s",
			app_settings_load_result_name(app->settings_load_result)
		);
		draw_settings_load_error_detail(
			app->settings_load_result,
			&app->settings_load_report,
			14
		);
		printf("\x1b[27;1HReviewed this session: %u", app->session.reviewed_count);
		break;
	}
	case APP_MODE_ACTIONS:
		printf("\x1b[1;1H" APP_COLOR_ACCENT "Actions" APP_COLOR_RESET);
		printf("\x1b[3;1HA: choose selected");
		printf("\x1b[5;1HD-pad/Circle U/D: move/hold");
		printf("\x1b[7;1HB or SELECT: cancel");
		printf("\x1b[9;1HSTART: confirm exit");
		printf("\x1b[11;1HY: controls");
		draw_deck_name_line(app, 13, APP_LAYOUT_DECK_NAME_BOTTOM_WIDTH);
		break;
	case APP_MODE_SETTINGS:
		printf("\x1b[1;1H" APP_COLOR_ACCENT "Daily limits" APP_COLOR_RESET);
		printf("\x1b[3;1HD-pad/Circle U/D: field");
		printf("\x1b[5;1HD-pad/Circle L/R: value/hold");
		printf("\x1b[7;1HA: save limits");
		printf("\x1b[9;1HB or SELECT: actions");
		printf("\x1b[11;1HSTART: confirm exit");
		printf("\x1b[13;1HY: controls");
		draw_deck_name_line(app, 15, APP_LAYOUT_DECK_NAME_BOTTOM_WIDTH);
		break;
	case APP_MODE_CONTROLS:
		printf("\x1b[1;1H" APP_COLOR_ACCENT "Controls" APP_COLOR_RESET);
		printf("\x1b[3;1HB, Y, or SELECT: back");
		printf("\x1b[5;1HSTART: confirm exit");
		break;
	case APP_MODE_CONFIRM_RESTORE:
		printf("\x1b[1;1H" APP_COLOR_WARNING "Confirm restore" APP_COLOR_RESET);
		printf("\x1b[3;1H" APP_COLOR_SUCCESS "X: restore cards" APP_COLOR_RESET);
		printf("\x1b[5;1HB or SELECT: cancel");
		printf("\x1b[7;1HSTART: confirm exit");
		draw_deck_name_line(app, 9, APP_LAYOUT_DECK_NAME_BOTTOM_WIDTH);
		break;
	case APP_MODE_CONFIRM_SUSPEND:
		printf("\x1b[1;1H" APP_COLOR_WARNING "Confirm suspend" APP_COLOR_RESET);
		printf("\x1b[3;1H" APP_COLOR_WARNING "X: suspend card" APP_COLOR_RESET);
		printf("\x1b[5;1HB or SELECT: cancel");
		printf("\x1b[7;1HSTART: confirm exit");
		draw_deck_name_line(app, 9, APP_LAYOUT_DECK_NAME_BOTTOM_WIDTH);
		break;
	case APP_MODE_CONFIRM_RESET:
		printf("\x1b[1;1H" APP_COLOR_DANGER "Confirm reset" APP_COLOR_RESET);
		printf("\x1b[3;1H" APP_COLOR_DANGER "X: reset progress" APP_COLOR_RESET);
		printf("\x1b[5;1HB or SELECT: cancel");
		printf("\x1b[7;1HSTART: confirm exit");
		draw_deck_name_line(app, 9, APP_LAYOUT_DECK_NAME_BOTTOM_WIDTH);
		break;
	case APP_MODE_CONFIRM_EXIT:
		printf("\x1b[1;1H" APP_COLOR_WARNING "Confirm exit" APP_COLOR_RESET);
		printf("\x1b[3;1HA: exit app");
		printf("\x1b[5;1HB or SELECT: cancel");
		break;
	}

	draw_status_message(app);
	draw_battery_status(app);
}

static void draw_app(const struct app_state *app)
{
	select_top_screen();

	switch (app->mode)
	{
	case APP_MODE_DECK_SELECT:
		draw_deck_select_screen(app);
		break;
	case APP_MODE_LOAD_ERROR:
		draw_load_error_screen(app);
		break;
	case APP_MODE_REVIEW:
		draw_review_screen(app);
		break;
	case APP_MODE_SUMMARY:
		draw_summary_screen(app);
		break;
	case APP_MODE_ACTIONS:
		draw_actions_screen(app);
		break;
	case APP_MODE_SETTINGS:
		draw_settings_screen(app);
		break;
	case APP_MODE_CONTROLS:
		draw_controls_screen(app);
		break;
	case APP_MODE_CONFIRM_RESTORE:
		draw_restore_confirmation_screen(app);
		break;
	case APP_MODE_CONFIRM_SUSPEND:
		draw_suspend_confirmation_screen(app);
		break;
	case APP_MODE_CONFIRM_RESET:
		draw_reset_confirmation_screen(app);
		break;
	case APP_MODE_CONFIRM_EXIT:
		draw_exit_confirmation_screen(app);
		break;
	}

	select_bottom_screen();
	draw_bottom_controls_screen(app);
	select_top_screen();
}

static void save_session_rollback(struct app_state *app)
{
	app->save_rollback_session = app->session;
}

static void restore_session_rollback(struct app_state *app)
{
	app->session = app->save_rollback_session;
	app->state_message = review_state_save_result_name(app->state_save_result);
}

static bool save_review_state(struct app_state *app)
{
	if (!review_state_load_result_allows_save(app->state_load_result))
	{
		app->state_save_result = REVIEW_STATE_SAVE_FAILED;
		app->state_message = review_state_load_result_name(app->state_load_result);
		return false;
	}

	app->state_save_result = review_state_save(
		&app->deck,
		&app->session,
		app->active_state_path
	);
	app->state_message = review_state_save_result_name(app->state_save_result);
	if (app->state_save_result != REVIEW_STATE_SAVE_OK)
		return false;

	app_refresh_selected_deck_summary(app);
	return true;
}

static void app_set_review_save_failed_status(
	struct app_state *app,
	const char *fallback_message
)
{
	if (!review_state_load_result_allows_save(app->state_load_result))
	{
		app->state_message = review_state_load_result_name(app->state_load_result);
		app_set_status(app, "Reset bad state first");
		return;
	}

	app_set_status(app, fallback_message);
}

static bool append_review_log_entry(
	struct app_state *app,
	enum review_log_event event,
	size_t card_index,
	enum scheduler_rating rating,
	const struct scheduler_card *before
)
{
	struct review_log_entry entry;
	time_t timestamp;

	if (before == NULL)
		return false;
	if (app->active_review_log_path[0] == '\0')
		return false;
	if (card_index >= app->deck.card_count || card_index >= app->session.card_count)
		return false;

	timestamp = time(NULL);
	if (timestamp == (time_t)-1)
		timestamp = 0;

	entry.timestamp = timestamp;
	entry.day = app->session.today;
	entry.event = event;
	entry.card_id = app->deck.cards[card_index].card_id;
	entry.rating = rating;
	entry.before = *before;
	entry.after = app->session.cards[card_index];
	return review_log_append(app->active_review_log_path, &entry);
}

static bool rate_current_card(struct app_state *app, enum scheduler_rating rating)
{
	const char *rating_name = scheduler_rating_name(rating);
	size_t card_index;
	struct scheduler_card before;
	bool log_saved;
	bool queue_complete;
	bool same_card_due;

	if (!app->revealed)
		return false;
	if (!scheduler_has_current(&app->session))
		return false;

	save_session_rollback(app);
	card_index = scheduler_current_index(&app->session);
	before = app->session.cards[card_index];
	scheduler_rate_current(&app->session, rating);
	if (!save_review_state(app))
	{
		restore_session_rollback(app);
		app_set_review_save_failed_status(app, "Save failed; card not advanced");
		app->mode = APP_MODE_REVIEW;
		return true;
	}

	log_saved = append_review_log_entry(
		app,
		REVIEW_LOG_EVENT_RATING,
		card_index,
		rating,
		&before
	);
	app->revealed = false;
	reset_review_scroll(app);
	queue_complete = scheduler_is_complete(&app->session);
	same_card_due =
		!queue_complete && scheduler_current_index(&app->session) == card_index;

	app_review_format_rating_status(
		app->status_message,
		sizeof(app->status_message),
		rating_name,
		log_saved,
		queue_complete,
		same_card_due,
		scheduler_current_index(&app->session),
		app->session.card_count
	);
	if (queue_complete && app_daily_limit_blocks_cards(app))
	{
		app_set_status(
			app,
			log_saved ?
				"Rating saved; daily limit reached" :
				"Rating saved; limit reached; log skipped"
		);
	}
	app->mode = queue_complete ? APP_MODE_SUMMARY : APP_MODE_REVIEW;

	return true;
}

static bool move_deck_selection_by_page(struct app_state *app, bool move_right)
{
	size_t old_index = app->selected_deck_index;
	size_t page_size = APP_LAYOUT_DECK_SELECTOR_VISIBLE_ROWS;

	if (app->deck_index.count <= 1)
		return false;

	if (move_right)
	{
		if (app->selected_deck_index + page_size >= app->deck_index.count)
			app->selected_deck_index = 0;
		else
			app->selected_deck_index += page_size;
	}
	else if (app->selected_deck_index < page_size)
	{
		app->selected_deck_index = app->deck_index.count - 1;
	}
	else
	{
		app->selected_deck_index -= page_size;
	}

	if (app->selected_deck_index == old_index)
		return false;

	app_set_deck_selection_status(app);
	return true;
}

static bool undo_last_action(struct app_state *app)
{
	size_t card_index = 0;
	struct scheduler_card before;
	bool can_log_undo = false;
	bool log_saved = true;

	save_session_rollback(app);
	if (
		app->session.undo.available &&
		app->session.undo.card_index < app->session.card_count &&
		app->session.undo.card_index < app->deck.card_count
	)
	{
		card_index = app->session.undo.card_index;
		before = app->session.cards[card_index];
		can_log_undo = true;
	}

	if (!scheduler_undo_last(&app->session))
	{
		app->state_message = "nothing to undo";
		app_set_nothing_status(app, "Nothing to undo");
		return true;
	}

	if (!save_review_state(app))
	{
		restore_session_rollback(app);
		app_set_review_save_failed_status(app, "Save failed; undo not kept");
		return true;
	}

	if (can_log_undo)
	{
		log_saved = append_review_log_entry(
			app,
			REVIEW_LOG_EVENT_UNDO,
			card_index,
			SCHEDULER_RATING_COUNT,
			&before
		);
	}
	app->state_message = "undone";
	app_set_undo_saved_status(app, log_saved);
	app->revealed = false;
	reset_review_scroll(app);
	app->mode = APP_MODE_REVIEW;
	return true;
}

static bool suspend_current_card(struct app_state *app)
{
	size_t card_index = 0;
	struct scheduler_card before;
	bool can_log_suspend = false;
	bool log_saved = true;

	save_session_rollback(app);
	if (
		scheduler_has_current(&app->session) &&
		scheduler_current_index(&app->session) < app->deck.card_count
	)
	{
		card_index = scheduler_current_index(&app->session);
		before = app->session.cards[card_index];
		can_log_suspend = true;
	}

	if (!scheduler_suspend_current(&app->session))
	{
		app->state_message = "nothing to suspend";
		app_set_nothing_status(app, "Nothing to suspend");
		app->mode = app_review_mode_for_session(app);
		return true;
	}

	if (!save_review_state(app))
	{
		restore_session_rollback(app);
		app_set_review_save_failed_status(app, "Save failed; card not suspended");
		app->mode = APP_MODE_REVIEW;
		return true;
	}

	if (can_log_suspend)
	{
		log_saved = append_review_log_entry(
			app,
			REVIEW_LOG_EVENT_SUSPEND,
			card_index,
			SCHEDULER_RATING_COUNT,
			&before
		);
	}
	app->state_message = "suspended";
	app_set_status(app, log_saved ? "Suspend saved" : "Suspend saved; log skipped");
	app->revealed = false;
	reset_review_scroll(app);

	if (scheduler_is_complete(&app->session))
	{
		app_set_status(
			app,
			app_daily_limit_blocks_cards(app) ?
				(log_saved ?
					"Suspend saved; daily limit reached" :
					"Suspend saved; limit reached; log skipped") :
				(log_saved ? "Suspend saved; no cards due" : "Suspend saved; log skipped")
		);
		app->mode = APP_MODE_SUMMARY;
	}
	else
	{
		app->mode = APP_MODE_REVIEW;
	}

	return true;
}

static bool reset_progress(struct app_state *app)
{
	bool log_deleted;

	if (app->active_state_path[0] == '\0' || app->active_review_log_path[0] == '\0')
	{
		app->state_message = "reset failed";
		app_set_status(app, "Reset failed");
		return false;
	}

	if (!review_state_delete(app->active_state_path))
	{
		app->state_message = "reset failed";
		app_set_status(app, "Reset failed");
		return false;
	}

	log_deleted = review_log_delete(app->active_review_log_path);

	app_load_selected_deck(app);
	if (app->load_result == DECK_LOAD_OK)
		app_set_status(
			app,
			log_deleted ? "Progress reset" : "Progress reset; log kept"
		);
	return true;
}

static bool unsuspend_all_cards(struct app_state *app)
{
	unsigned int unsuspended_count;
	bool was_suspended[DECK_MAX_CARDS];
	bool logs_saved = true;

	save_session_rollback(app);
	memset(was_suspended, 0, sizeof(was_suspended));
	for (size_t index = 0; index < app->session.card_count; index++)
		was_suspended[index] = app->session.cards[index].suspended;

	unsuspended_count = scheduler_unsuspend_all(&app->session);
	if (unsuspended_count == 0)
	{
		app->state_message = "nothing suspended";
		app_set_nothing_status(app, "Nothing suspended");
		app->mode = APP_MODE_ACTIONS;
		return true;
	}

	if (!save_review_state(app))
	{
		restore_session_rollback(app);
		app_set_review_save_failed_status(app, "Save failed; restore undone");
		app->mode = APP_MODE_ACTIONS;
		return true;
	}

	for (size_t index = 0; index < app->session.card_count; index++)
	{
		struct scheduler_card before;

		if (!was_suspended[index])
			continue;

		before = app->session.cards[index];
		before.suspended = true;
		if (!append_review_log_entry(
			app,
			REVIEW_LOG_EVENT_RESTORE,
			index,
			SCHEDULER_RATING_COUNT,
			&before
		))
		{
			logs_saved = false;
		}
	}

	app->state_message = "unsuspended";
	snprintf(
		app->status_message,
		sizeof(app->status_message),
		logs_saved ? "Restored %u suspended" : "Restored %u; log skipped",
		unsuspended_count
	);
	app->revealed = false;
	reset_review_scroll(app);

	if (scheduler_is_complete(&app->session))
	{
		if (app_daily_limit_blocks_cards(app))
		{
			snprintf(
				app->status_message,
				sizeof(app->status_message),
				logs_saved ?
					"Restored %u; daily limit reached" :
					"Restored %u; limit reached; log skipped",
				unsuspended_count
			);
		}
		app->mode = APP_MODE_SUMMARY;
	}
	else
	{
		app->mode = APP_MODE_REVIEW;
	}

	return true;
}

static bool save_daily_limits(struct app_state *app)
{
	app->settings_save_result = app_settings_save(
		&app->edited_settings,
		app->active_settings_path
	);
	app->settings_message = app_settings_save_result_name(app->settings_save_result);

	if (app->settings_save_result != APP_SETTINGS_SAVE_OK)
	{
		app_set_status(app, "Settings save failed");
		return true;
	}

	app->settings = app->edited_settings;
	app->settings_load_result = APP_SETTINGS_LOAD_OK;
	app_settings_load_report_clear(&app->settings_load_report);
	scheduler_set_daily_limits(
		&app->session,
		app->settings.new_limit,
		app->settings.review_limit
	);
	app_refresh_selected_deck_summary(app);
	app->revealed = false;
	reset_review_scroll(app);
	app->mode = app_review_mode_for_session(app);

	if (!app_state_allows_study(app))
	{
		app_set_status(app, "Limits saved; reset state");
	}
	else if (app->mode == APP_MODE_SUMMARY)
	{
		app_set_status(
			app,
			app_daily_limit_blocks_cards(app) ?
				"Limits saved; daily limit reached" :
				"Limits saved; no cards due"
		);
	}
	else
	{
		app_set_status(app, "Limits saved");
	}

	return true;
}

static bool app_handle_deck_select_input(
	struct app_state *app,
	unsigned int buttons_down,
	unsigned int buttons_active
)
{
	bool move_down;
	bool move_right;

	if (
		app_command_pressed(
			buttons_down,
			buttons_active,
			APP_CONTROL_BUTTON_SELECT
		)
	)
	{
		show_scan_then_scan_decks(app);
		return true;
	}

	if (app->deck_index.count == 0)
	{
		if (
			app_command_pressed(
				buttons_down,
				buttons_active,
				APP_CONTROL_BUTTON_A
			)
		)
		{
			app_set_status(app, "No deck selected");
			return true;
		}

		return false;
	}

	if (app_controls_up_down_triggered(buttons_down, buttons_active, &move_down))
	{
		if (move_down)
		{
			app->selected_deck_index =
				(app->selected_deck_index + 1) % app->deck_index.count;
		}
		else if (app->selected_deck_index == 0)
		{
			app->selected_deck_index = app->deck_index.count - 1;
		}
		else
		{
			app->selected_deck_index--;
		}

		app_set_deck_selection_status(app);
		return true;
	}

	if (app_controls_left_right_triggered(buttons_down, buttons_active, &move_right))
		return move_deck_selection_by_page(app, move_right);

	if (
		app_command_pressed(
			buttons_down,
			buttons_active,
			APP_CONTROL_BUTTON_A
		)
	)
	{
		app_load_selected_deck(app);
		return true;
	}

	return false;
}

static bool app_handle_actions_input(
	struct app_state *app,
	unsigned int buttons_down,
	unsigned int buttons_active
)
{
	bool move_down;

	if (app_controls_up_down_triggered(buttons_down, buttons_active, &move_down))
	{
		if (move_down)
		{
			app->selected_action =
				(enum action_item)((app->selected_action + 1) % ACTION_ITEM_COUNT);
		}
		else if (app->selected_action == ACTION_ITEM_UNSUSPEND_ALL)
		{
			app->selected_action = ACTION_ITEM_COUNT - 1;
		}
		else
		{
			app->selected_action--;
		}
		app_set_action_selection_status(app);
		return true;
	}

	if (
		app_command_pressed(
			buttons_down,
			buttons_active,
			APP_CONTROL_BUTTON_A
		)
	)
	{
		if (app->selected_action == ACTION_ITEM_UNSUSPEND_ALL)
		{
			if (scheduler_suspended_count(&app->session) == 0)
				return unsuspend_all_cards(app);

			app_open_restore_confirmation(app);
			return true;
		}
		if (app->selected_action == ACTION_ITEM_DAILY_LIMITS)
		{
			app_open_settings(app);
			return true;
		}

		app_open_reset_confirmation(app);
		return true;
	}

	if (
		app_command_pressed(buttons_down, buttons_active, APP_CONTROL_BUTTON_B) ||
		app_command_pressed(
			buttons_down,
			buttons_active,
			APP_CONTROL_BUTTON_SELECT
		)
	)
	{
		app->mode = app->action_return_mode;
		app_set_canceled_status(app, "Actions");
		return true;
	}

	return false;
}

static bool app_handle_restore_confirmation_input(
	struct app_state *app,
	unsigned int buttons_down,
	unsigned int buttons_active
)
{
	if (app_command_pressed(buttons_down, buttons_active, APP_CONTROL_BUTTON_X))
		return unsuspend_all_cards(app);

	if (
		app_command_pressed(buttons_down, buttons_active, APP_CONTROL_BUTTON_B) ||
		app_command_pressed(
			buttons_down,
			buttons_active,
			APP_CONTROL_BUTTON_SELECT
		)
	)
	{
		app->mode = APP_MODE_ACTIONS;
		app_set_canceled_status(app, "Restore");
		return true;
	}

	return false;
}

static bool app_handle_reset_confirmation_input(
	struct app_state *app,
	unsigned int buttons_down,
	unsigned int buttons_active
)
{
	if (app_command_pressed(buttons_down, buttons_active, APP_CONTROL_BUTTON_X))
	{
		if (!reset_progress(app))
			app->mode = APP_MODE_CONFIRM_RESET;
		return true;
	}

	if (
		app_command_pressed(buttons_down, buttons_active, APP_CONTROL_BUTTON_B) ||
		app_command_pressed(
			buttons_down,
			buttons_active,
			APP_CONTROL_BUTTON_SELECT
		)
	)
	{
		app->mode = APP_MODE_ACTIONS;
		app_set_canceled_status(app, "Reset");
		return true;
	}

	return false;
}

static bool app_handle_suspend_confirmation_input(
	struct app_state *app,
	unsigned int buttons_down,
	unsigned int buttons_active
)
{
	if (app_command_pressed(buttons_down, buttons_active, APP_CONTROL_BUTTON_X))
		return suspend_current_card(app);

	if (
		app_command_pressed(buttons_down, buttons_active, APP_CONTROL_BUTTON_B) ||
		app_command_pressed(
			buttons_down,
			buttons_active,
			APP_CONTROL_BUTTON_SELECT
		)
	)
	{
		app->mode = APP_MODE_REVIEW;
		app_set_canceled_status(app, "Suspend");
		return true;
	}

	return false;
}

static bool app_handle_settings_input(
	struct app_state *app,
	unsigned int buttons_down,
	unsigned int buttons_active
)
{
	bool down;
	bool right;

	if (app_controls_up_down_triggered(buttons_down, buttons_active, &down))
	{
		bool unsaved_changes;

		if (app->selected_setting == SETTING_ITEM_NEW_LIMIT)
			app->selected_setting = SETTING_ITEM_REVIEW_LIMIT;
		else
			app->selected_setting = SETTING_ITEM_NEW_LIMIT;
		unsaved_changes = app_settings_have_unsaved_changes(app);
		app_set_setting_field_status(app, unsaved_changes);
		return true;
	}

	if (app_controls_left_right_triggered(buttons_down, buttons_active, &right))
	{
		unsigned int *limit = selected_daily_limit(app);
		char limit_text[16];
		bool unsaved_changes;

		*limit = adjusted_daily_limit(*limit, right);
		format_daily_limit(limit_text, sizeof(limit_text), *limit);
		unsaved_changes = app_settings_have_unsaved_changes(app);
		app->settings_message = unsaved_changes ?
			"unsaved changes" :
			"no changes";
		app_set_setting_value_status(app, limit_text, unsaved_changes);
		return true;
	}

	if (app_command_pressed(buttons_down, buttons_active, APP_CONTROL_BUTTON_A))
		return save_daily_limits(app);

	if (
		app_command_pressed(buttons_down, buttons_active, APP_CONTROL_BUTTON_B) ||
		app_command_pressed(
			buttons_down,
			buttons_active,
			APP_CONTROL_BUTTON_SELECT
		)
	)
	{
		bool discarded_changes = app_settings_have_unsaved_changes(app);

		app->edited_settings = app->settings;
		app->settings_message = "no changes";
		app->mode = APP_MODE_ACTIONS;
		app_set_limits_canceled_status(app, discarded_changes);
		return true;
	}

	return false;
}

static bool app_handle_input(
	struct app_state *app,
	unsigned int buttons_down,
	unsigned int buttons_active
)
{
	enum scheduler_rating rating;
	enum app_control_action action = app_controls_classify_action(
		app_control_mode_for_app_mode(app->mode),
		app->revealed,
		app_state_allows_study(app),
		buttons_down,
		buttons_active,
		&rating
	);

	switch (action)
	{
	case APP_CONTROL_ACTION_CONFIRM_EXIT:
		app->exit_requested = true;
		return true;
	case APP_CONTROL_ACTION_CANCEL_EXIT:
		app->mode = app->exit_return_mode;
		app_set_exit_canceled_status(app, app->mode);
		return true;
	case APP_CONTROL_ACTION_OPEN_EXIT:
		app_open_exit_confirmation(app);
		return true;
	case APP_CONTROL_ACTION_CLOSE_CONTROLS:
		app_close_controls(app);
		return true;
	case APP_CONTROL_ACTION_OPEN_CONTROLS:
		app_open_controls(app);
		return true;
	case APP_CONTROL_ACTION_RETURN_TO_DECK_SELECT:
		app_return_to_deck_select(app);
		return true;
	case APP_CONTROL_ACTION_OPEN_ACTIONS:
		app_open_actions(app);
		return true;
	case APP_CONTROL_ACTION_UNDO:
		return undo_last_action(app);
	case APP_CONTROL_ACTION_OPEN_SUSPEND:
		app_open_suspend_confirmation(app);
		return true;
	case APP_CONTROL_ACTION_SCROLL_UP:
		return scroll_review_text(app, false);
	case APP_CONTROL_ACTION_SCROLL_DOWN:
		return scroll_review_text(app, true);
	case APP_CONTROL_ACTION_SHOW_ANSWER:
		app->revealed = true;
		reset_review_scroll(app);
		app_set_status(app, "Answer shown; choose rating");
		return true;
	case APP_CONTROL_ACTION_RATE:
		return rate_current_card(app, rating);
	case APP_CONTROL_ACTION_NONE:
		break;
	}

	if (app->mode == APP_MODE_DECK_SELECT)
		return app_handle_deck_select_input(app, buttons_down, buttons_active);
	if (app->mode == APP_MODE_ACTIONS)
		return app_handle_actions_input(app, buttons_down, buttons_active);
	if (app->mode == APP_MODE_SETTINGS)
		return app_handle_settings_input(app, buttons_down, buttons_active);
	if (app->mode == APP_MODE_CONFIRM_RESTORE)
		return app_handle_restore_confirmation_input(
			app,
			buttons_down,
			buttons_active
		);
	if (app->mode == APP_MODE_CONFIRM_SUSPEND)
		return app_handle_suspend_confirmation_input(
			app,
			buttons_down,
			buttons_active
		);
	if (app->mode == APP_MODE_CONFIRM_RESET)
		return app_handle_reset_confirmation_input(
			app,
			buttons_down,
			buttons_active
		);

	return false;
}

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	static struct app_state app;
	bool frame_dirty = true;
	unsigned int idle_wait_count = 0;
	bool held_navigation_wait = false;
	time_t next_battery_poll_time = 0;
	time_t next_day_check_time = 0;
	time_t now;
	enum app_power_battery_sample_result startup_battery_sample_result;
	struct app_control_repeat navigation_repeat;

	gfxInitDefault();
	consoleInit(GFX_TOP, &top_screen);
	consoleInit(GFX_BOTTOM, &bottom_screen);

	app_controls_repeat_init(&navigation_repeat);
	startup_battery_sample_result = app_init(&app);
	now = time(NULL);
	app_power_schedule_next_battery_poll_after_sample(
		&next_battery_poll_time,
		now,
		startup_battery_sample_result
	);
	if (app.current_day != 0)
		schedule_next_day_check(&next_day_check_time, now);
	show_scan_then_scan_decks(&app);
	draw_app(&app);

	while (aptMainLoop())
	{
		if (frame_dirty)
		{
			present_current_frame();
			frame_dirty = false;
			idle_wait_count = 0;
		}
		else if (held_navigation_wait)
		{
			gspWaitForVBlank();
		}
		else
		{
			wait_for_idle_input(idle_wait_count);
			idle_wait_count =
				app_power_next_idle_input_wait_count(idle_wait_count);
		}

		hidScanInput();

		u32 keys_down = hidKeysDown();
		u32 keys_held = hidKeysHeld();
		unsigned int buttons_down = app_controls_buttons_from_3ds_keys(keys_down);
		unsigned int buttons_held = app_controls_buttons_from_3ds_keys(keys_held);
		enum app_control_mode control_mode = app_control_mode_for_app_mode(app.mode);
		unsigned int buttons_active;
		unsigned int repeat_buttons = 0;
		if (app_controls_mode_uses_navigation_repeat(control_mode))
		{
			repeat_buttons = app_controls_repeat_buttons_for_mode(
				&navigation_repeat,
				control_mode,
				buttons_down,
				buttons_held
			);
		}
		else
		{
			app_controls_repeat_reset(&navigation_repeat);
		}

		held_navigation_wait = app_mode_uses_held_navigation_wait(
			app.mode,
			buttons_held
		);
		buttons_down |= repeat_buttons;
		buttons_active = buttons_down | buttons_held;
		if (app_controls_input_is_active(buttons_down, buttons_held, repeat_buttons))
		{
			idle_wait_count = 0;
		}

		now = time(NULL);
		bool battery_poll_due = app_power_battery_poll_is_due(
			&next_battery_poll_time,
			now
		);
		enum app_power_battery_sample_result battery_sample_result =
			APP_POWER_BATTERY_SAMPLE_UNCHANGED;
		bool battery_changed = false;
		if (battery_poll_due)
		{
			battery_sample_result = app_sample_battery(&app);
			battery_changed =
				app_power_battery_sample_changes_display(battery_sample_result);
		}
		bool day_check_due = day_check_is_due(&next_day_check_time, now);
		bool day_changed = day_check_due ?
			app_refresh_day_if_changed(&app, app_time_local_day_from_time(now)) :
			false;
		if (battery_poll_due)
		{
			app_power_schedule_next_battery_poll_after_sample(
				&next_battery_poll_time,
				now,
				battery_sample_result
			);
		}
		if (day_check_due)
			schedule_next_day_check(&next_day_check_time, now);

		if (day_changed)
		{
			draw_app(&app);
			frame_dirty = true;
			idle_wait_count = 0;
			continue;
		}

		if (app_handle_input(&app, buttons_down, buttons_active))
		{
			if (app.exit_requested)
				break;

			draw_app(&app);
			frame_dirty = true;
		}
		else if (battery_changed)
		{
			draw_app(&app);
			frame_dirty = true;
		}
	}

	if (app.battery_service_available)
		ptmuExit();
	gfxExit();
	return 0;
}
