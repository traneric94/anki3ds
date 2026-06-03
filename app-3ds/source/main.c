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
#include "app_text.h"
#include "app_time.h"
#include "deck.h"
#include "deck_index.h"
#include "deck_summary.h"
#include "media_cache.h"
#include "media_image.h"
#include "review_log.h"
#include "review_state.h"
#include "scheduler.h"

#define APP_VERSION "0.5.0-dev"
#define IDLE_INPUT_WAIT_INITIAL_NS 100000000LL
#define IDLE_INPUT_WAIT_MID_NS 250000000LL
#define IDLE_INPUT_WAIT_MAX_NS 500000000LL
#define IDLE_INPUT_FAST_WAIT_COUNT 10
#define IDLE_INPUT_MID_WAIT_COUNT 30
#define IDLE_INPUT_MAX_WAIT_COUNT 60
#define STATUS_MESSAGE_SIZE 64
#define DAY_CHECK_INTERVAL_SECONDS 60
#define APP_COLOR_RESET "\x1b[0m"
#define APP_COLOR_BLUE "\x1b[34m"
#define APP_COLOR_GREEN "\x1b[32m"
#define APP_COLOR_RED "\x1b[31m"
#define APP_COLOR_YELLOW "\x1b[33m"
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
	enum app_settings_load_result settings_load_result;
	enum app_settings_save_result settings_save_result;
	enum review_state_load_result state_load_result;
	enum review_state_save_result state_save_result;
	const char *state_message;
	const char *settings_message;
	enum setting_item selected_setting;
	size_t selected_deck_index;
	char active_cards_path[DECK_INDEX_MAX_PATH_LENGTH];
	char active_state_path[DECK_INDEX_MAX_PATH_LENGTH];
	char active_review_log_path[DECK_INDEX_MAX_PATH_LENGTH];
	char active_settings_path[DECK_INDEX_MAX_PATH_LENGTH];
	char active_media_path[DECK_INDEX_MAX_PATH_LENGTH];
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
static struct media_cache media_cache;

static void draw_scanning_progress_screen(
	const struct app_state *app,
	size_t loaded_count,
	size_t total_count,
	const char *deck_name
);
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

static bool app_mode_uses_navigation_repeat(enum app_mode mode)
{
	return (
		mode == APP_MODE_DECK_SELECT ||
		mode == APP_MODE_ACTIONS ||
		mode == APP_MODE_SETTINGS
	);
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

static u32 keys_for_repeat_buttons(unsigned int buttons)
{
	u32 keys = 0;

	if (buttons & APP_CONTROL_BUTTON_UP)
		keys |= KEY_DUP;
	if (buttons & APP_CONTROL_BUTTON_DOWN)
		keys |= KEY_DDOWN;
	if (buttons & APP_CONTROL_BUTTON_LEFT)
		keys |= KEY_DLEFT;
	if (buttons & APP_CONTROL_BUTTON_RIGHT)
		keys |= KEY_DRIGHT;

	return keys;
}

static void console_move(int row, int column)
{
	printf("\x1b[%d;%dH", row, column);
}

static void draw_wrapped_text_columns(
	const char *text,
	int row,
	int max_rows,
	int max_columns
)
{
	int current_row = row;
	int column = APP_LAYOUT_TEXT_LEFT;
	bool truncated = false;

	console_move(current_row, APP_LAYOUT_TEXT_LEFT);

	for (size_t index = 0; text[index] != '\0'; )
	{
		char value = text[index];
		size_t char_length = app_text_utf8_char_length(&text[index]);

		if (current_row >= row + max_rows)
		{
			truncated = true;
			break;
		}
		if (char_length == 0)
			break;

		if (value == '\r')
		{
			index += char_length;
			continue;
		}

		if (value == '\n')
		{
			current_row++;
			column = APP_LAYOUT_TEXT_LEFT;
			if (current_row < row + max_rows)
				console_move(current_row, APP_LAYOUT_TEXT_LEFT);
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
			current_row++;
			column = APP_LAYOUT_TEXT_LEFT;
			if (current_row >= row + max_rows)
			{
				truncated = true;
				break;
			}
			console_move(current_row, APP_LAYOUT_TEXT_LEFT);
		}

		if (value == ' ' && text[index] == '\t')
			putchar(value);
		else
			fwrite(&text[index], 1, char_length, stdout);

		index += char_length;
		column++;
	}

	if (truncated && max_rows > 0 && max_columns >= 3)
	{
		console_move(row + max_rows - 1, APP_LAYOUT_TEXT_LEFT + max_columns - 3);
		printf("...");
	}
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

static unsigned int review_count_total(const struct scheduler_session *session)
{
	unsigned int total = 0;

	for (size_t index = 0; index < session->card_count; index++)
		total += session->cards[index].review_count;

	return total;
}

static size_t scheduler_new_due_count(const struct scheduler_session *session)
{
	size_t count = 0;

	for (size_t index = 0; index < session->card_count; index++)
	{
		if (scheduler_card_is_due(session, index) && session->cards[index].review_count == 0)
			count++;
	}

	return count;
}

static size_t scheduler_learning_due_count(const struct scheduler_session *session)
{
	size_t count = 0;

	for (size_t index = 0; index < session->card_count; index++)
	{
		if (
			scheduler_card_is_due(session, index) &&
			session->cards[index].review_count > 0 &&
			session->cards[index].interval_days == 0
		)
		{
			count++;
		}
	}

	return count;
}

static size_t scheduler_review_due_count(const struct scheduler_session *session)
{
	size_t count = 0;

	for (size_t index = 0; index < session->card_count; index++)
	{
		if (
			scheduler_card_is_due(session, index) &&
			session->cards[index].review_count > 0 &&
			session->cards[index].interval_days > 0
		)
		{
			count++;
		}
	}

	return count;
}

static void format_daily_limit(char *destination, size_t destination_size, unsigned int limit)
{
	if (limit == 0)
		snprintf(destination, destination_size, "%s", "all");
	else
		snprintf(destination, destination_size, "%u", limit);
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

static bool settings_load_result_needs_warning(enum app_settings_load_result result)
{
	return result == APP_SETTINGS_LOAD_BAD_FORMAT;
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

static void copy_string(char *destination, size_t destination_size, const char *source)
{
	if (destination_size == 0)
		return;

	snprintf(destination, destination_size, "%s", source);
}

static void app_set_status(struct app_state *app, const char *message)
{
	copy_string(app->status_message, sizeof(app->status_message), message);
}

static const char *status_message_color(const char *message)
{
	if (
		strstr(message, "failed") != NULL ||
		strstr(message, "Failed") != NULL ||
		strstr(message, "error") != NULL ||
		strstr(message, "bad") != NULL
	)
	{
		return APP_COLOR_RED;
	}
	if (
		strstr(message, "requires") != NULL ||
		strstr(message, "Nothing") != NULL ||
		strstr(message, "skipped") != NULL ||
		strstr(message, "same card due") != NULL ||
		strstr(message, "same due") != NULL ||
		strstr(message, "kept") != NULL ||
		strstr(message, "reset state") != NULL
	)
	{
		return APP_COLOR_YELLOW;
	}
	if (
		strstr(message, "saved") != NULL ||
		strstr(message, "Loaded") != NULL ||
		strstr(message, "reset") != NULL ||
		strstr(message, "Reset") != NULL
	)
	{
		return APP_COLOR_GREEN;
	}

	return APP_COLOR_BLUE;
}

static long long idle_input_wait_ns(unsigned int idle_wait_count)
{
	if (idle_wait_count < IDLE_INPUT_FAST_WAIT_COUNT)
		return IDLE_INPUT_WAIT_INITIAL_NS;
	if (idle_wait_count < IDLE_INPUT_MID_WAIT_COUNT)
		return IDLE_INPUT_WAIT_MID_NS;

	return IDLE_INPUT_WAIT_MAX_NS;
}

static void wait_for_idle_input(unsigned int idle_wait_count)
{
	hidWaitForAnyEvent(true, 0, idle_input_wait_ns(idle_wait_count));
}

static bool app_mode_uses_held_navigation_wait(
	enum app_mode mode,
	unsigned int buttons_held
)
{
	if (!app_mode_uses_navigation_repeat(mode))
		return false;

	return app_controls_repeatable_navigation_held(buttons_held);
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

static unsigned char rgb565_red(uint16_t pixel)
{
	return (unsigned char)((((pixel >> 11) & 0x1f) * 255u) / 31u);
}

static unsigned char rgb565_green(uint16_t pixel)
{
	return (unsigned char)((((pixel >> 5) & 0x3f) * 255u) / 63u);
}

static unsigned char rgb565_blue(uint16_t pixel)
{
	return (unsigned char)(((pixel & 0x1f) * 255u) / 31u);
}

static void draw_top_image(const struct media_image *image, int x, int y)
{
	u16 frame_width;
	u16 frame_height;
	u8 *framebuffer = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &frame_width, &frame_height);

	(void)frame_width;
	(void)frame_height;

	if (framebuffer == NULL || !image->loaded)
		return;

	for (unsigned int source_y = 0; source_y < image->height; source_y++)
	{
		int screen_y = y + (int)source_y;

		if (screen_y < 0 || screen_y >= APP_LAYOUT_TOP_SCREEN_HEIGHT)
			continue;

		for (unsigned int source_x = 0; source_x < image->width; source_x++)
		{
			int screen_x = x + (int)source_x;
			uint16_t pixel;
			size_t offset;

			if (screen_x < 0 || screen_x >= APP_LAYOUT_TOP_SCREEN_WIDTH)
				continue;

			pixel = image->pixels[source_y * image->width + source_x];
			offset = (
				(size_t)(APP_LAYOUT_TOP_SCREEN_HEIGHT - screen_y - 1) +
				(size_t)screen_x * APP_LAYOUT_TOP_SCREEN_HEIGHT
			) * 3;

			framebuffer[offset] = rgb565_blue(pixel);
			framebuffer[offset + 1] = rgb565_green(pixel);
			framebuffer[offset + 2] = rgb565_red(pixel);
		}
	}
}

static bool build_media_file_path(
	char *destination,
	size_t destination_size,
	const struct app_state *app,
	const char *media_name
)
{
	int written = snprintf(
		destination,
		destination_size,
		"%s/%s",
		app->active_media_path,
		media_name
	);

	return written >= 0 && (size_t)written < destination_size;
}

static void draw_card_media(
	const struct app_state *app,
	const char *media_name,
	int x,
	int y,
	int status_row
)
{
	const struct media_cache_slot *slot;
	char path[DECK_INDEX_MAX_PATH_LENGTH];

	if (media_name[0] == '\0')
		return;

	if (!build_media_file_path(path, sizeof(path), app, media_name))
	{
		printf(
			"\x1b[%d;1H" APP_COLOR_RED "Media path too long" APP_COLOR_RESET,
			status_row
		);
		return;
	}

	slot = media_cache_load(&media_cache, path);
	if (slot->result == MEDIA_IMAGE_LOAD_OK)
	{
		draw_top_image(&slot->image, x, y);
		return;
	}

	printf("\x1b[%d;1H" APP_COLOR_RED "Media " APP_COLOR_RESET, status_row);
	print_truncated(media_name, 24);
	printf(
		": " APP_COLOR_RED "%s" APP_COLOR_RESET,
		media_image_load_result_name(slot->result)
	);
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
}

static void app_return_to_deck_select(struct app_state *app)
{
	app_refresh_selected_deck_summary(app);
	app_set_status(app, "Deck list");
	app->mode = APP_MODE_DECK_SELECT;
}

static void app_load_selected_deck(struct app_state *app)
{
	const struct deck_entry *entry;
	unsigned int today = app_time_current_day();

	app->current_day = today;

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
	copy_string(app->active_media_path, sizeof(app->active_media_path), entry->media_path);
	media_cache_clear(&media_cache);
	deck_init(&app->deck, entry->display_name);
	app->revealed = false;
	app_settings_default(&app->settings);
	app->settings_load_result = APP_SETTINGS_LOAD_NOT_FOUND;
	app->settings_save_result = APP_SETTINGS_SAVE_OK;
	app->state_load_result = REVIEW_STATE_LOAD_NOT_FOUND;
	app->state_save_result = REVIEW_STATE_SAVE_OK;
	app->state_message = "State: not loaded";
	app->settings_message = "settings not saved";
	app->load_result = deck_load_cards(&app->deck, app->active_cards_path);

	if (app->load_result == DECK_LOAD_OK)
	{
		app->settings_load_result = app_settings_load(
			&app->settings,
			app->active_settings_path
		);
		scheduler_init(&app->session, app->deck.card_count, today);
		scheduler_set_daily_limits(
			&app->session,
			app->settings.new_limit,
			app->settings.review_limit
		);
		app->state_load_result = review_state_load(
			&app->deck,
			&app->session,
			app->active_state_path
		);
		app->state_message = review_state_load_result_name(app->state_load_result);

		app->mode = app_review_mode_for_session(app);
		if (app->mode == APP_MODE_REVIEW)
		{
			app_set_status(app, "Loaded deck");
		}
		else if (!app_state_allows_study(app))
		{
			app_set_status(app, "Reset bad state first");
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
	app_set_status(app, "Actions");
	app->mode = APP_MODE_ACTIONS;
}

static void app_open_settings(struct app_state *app)
{
	app->edited_settings = app->settings;
	app->selected_setting = SETTING_ITEM_NEW_LIMIT;
	app->settings_message = "settings not saved";
	app_set_status(app, "Editing limits");
	app->mode = APP_MODE_SETTINGS;
}

static void app_open_restore_confirmation(struct app_state *app)
{
	app_set_status(app, "Restore requires X");
	app->mode = APP_MODE_CONFIRM_RESTORE;
}

static void app_open_reset_confirmation(struct app_state *app)
{
	app_set_status(app, "Reset requires X");
	app->mode = APP_MODE_CONFIRM_RESET;
}

static void app_open_suspend_confirmation(struct app_state *app)
{
	app_set_status(app, "Suspend requires X");
	app->mode = APP_MODE_CONFIRM_SUSPEND;
}

static void app_open_exit_confirmation(struct app_state *app)
{
	app->exit_return_mode = app->mode;
	app_set_status(app, "Exit requires A");
	app->mode = APP_MODE_CONFIRM_EXIT;
}

static void app_open_controls(struct app_state *app)
{
	app->controls_return_mode = app->mode;
	app_set_status(app, "Controls");
	app->mode = APP_MODE_CONTROLS;
}

static void app_init(struct app_state *app)
{
	memset(app, 0, sizeof(*app));
	app->current_day = app_time_current_day();
	app->battery_service_available = R_SUCCEEDED(ptmuInit());
	app_sample_battery(app);
	media_cache_init(&media_cache);
	app_set_status(app, "Ready");
	app->mode = APP_MODE_DECK_SELECT;
}

static void draw_header(const struct app_state *app)
{
	printf("\x1b[1;1H" APP_COLOR_BLUE "anki3ds Review" APP_COLOR_RESET);
	printf("\x1b[2;1HDeck: ");
	print_truncated(app->deck.name, APP_LAYOUT_DECK_NAME_HEADER_WIDTH);
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
	printf("\x1b[1;1H" APP_COLOR_BLUE "anki3ds" APP_COLOR_RESET);
	printf("\x1b[3;1H" APP_COLOR_BLUE "Select deck" APP_COLOR_RESET);
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
		printf("\x1b[5;1H" APP_COLOR_YELLOW "No decks found." APP_COLOR_RESET);
		printf("\x1b[7;1HCreate a folder like:");
		printf("\x1b[8;1H%s/my-deck/cards.tsv", DECK_INDEX_ROOT_PATH);
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
				printf(APP_COLOR_GREEN);
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
				if (settings_load_result_needs_warning(summary->settings_load_result))
				{
					printf(
						APP_COLOR_RESET APP_COLOR_YELLOW
						" settings ignored" APP_COLOR_RESET
					);
				}
				else
				{
					printf(
						" N:%lu L:%lu R:%lu S:%lu",
						(unsigned long)summary->new_due_count,
						(unsigned long)summary->learning_due_count,
						(unsigned long)summary->review_due_count,
						(unsigned long)summary->suspended_count
					);
				}
			}
			else if (summary->deck_load_result == DECK_LOAD_OK)
			{
				printf(APP_COLOR_RESET APP_COLOR_RED " state error" APP_COLOR_RESET);
			}
			else
			{
				printf(APP_COLOR_RESET APP_COLOR_RED " load error" APP_COLOR_RESET);
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
				"\x1b[23;1HShowing %lu/%lu decks.",
				(unsigned long)app->deck_index.count,
				(unsigned long)app->deck_index.total_count
			);
		}
	}
}

static void draw_load_error_screen(const struct app_state *app)
{
	app_console_clear();
	printf("\x1b[1;1H" APP_COLOR_BLUE "anki3ds" APP_COLOR_RESET);
	printf("\x1b[3;1H" APP_COLOR_RED "Could not load deck." APP_COLOR_RESET);
	printf("\x1b[5;1H");
	print_truncated(
		app->active_cards_path[0] ? app->active_cards_path : DECK_INDEX_ROOT_PATH,
		APP_LAYOUT_TEXT_WIDTH
	);
	printf("\x1b[7;1HResult: %s", deck_load_result_name(app->load_result));
	printf("\x1b[10;1HCopy cards.tsv to the path above.");
}

static void draw_review_screen(const struct app_state *app)
{
	const struct card *card = current_card(app);
	bool front_has_media;
	bool back_has_media;

	app_console_clear();
	draw_header(app);

	if (card == NULL)
	{
		printf("\x1b[6;1HNo cards are due today.");
		return;
	}

	front_has_media = card->front_media[0] != '\0';
	back_has_media = card->back_media[0] != '\0';

	draw_card_status(app, &app->session.cards[scheduler_current_index(&app->session)]);
	printf("\x1b[7;1H" APP_COLOR_BLUE "Front" APP_COLOR_RESET);
	printf("\x1b[8;1H" APP_COLOR_BLUE "------------------------------------------------" APP_COLOR_RESET);

	if (app->revealed)
	{
		draw_wrapped_text_columns(
			card->front,
			APP_LAYOUT_REVIEW_FRONT_TEXT_ROW,
			APP_LAYOUT_REVIEW_REVEALED_FRONT_TEXT_ROWS,
			front_has_media ? APP_LAYOUT_MEDIA_TEXT_WIDTH : APP_LAYOUT_TEXT_WIDTH
		);
		printf("\x1b[15;1H" APP_COLOR_BLUE "Back" APP_COLOR_RESET);
		printf("\x1b[16;1H" APP_COLOR_BLUE "------------------------------------------------" APP_COLOR_RESET);
		draw_wrapped_text_columns(
			card->back,
			APP_LAYOUT_REVIEW_BACK_TEXT_ROW,
			APP_LAYOUT_REVIEW_BACK_TEXT_ROWS,
			back_has_media ? APP_LAYOUT_MEDIA_TEXT_WIDTH : APP_LAYOUT_TEXT_WIDTH
		);
		draw_card_media(
			app,
			card->front_media,
			APP_LAYOUT_MEDIA_IMAGE_X,
			APP_LAYOUT_MEDIA_FRONT_Y,
			APP_LAYOUT_REVIEW_REVEALED_FRONT_MEDIA_STATUS_ROW
		);
		draw_card_media(
			app,
			card->back_media,
			APP_LAYOUT_MEDIA_IMAGE_X,
			APP_LAYOUT_MEDIA_BACK_Y,
			APP_LAYOUT_REVIEW_BACK_MEDIA_STATUS_ROW
		);
	}
	else
	{
		draw_wrapped_text_columns(
			card->front,
			APP_LAYOUT_REVIEW_FRONT_TEXT_ROW,
			front_has_media ?
				APP_LAYOUT_REVIEW_FRONT_MEDIA_TEXT_ROWS :
				APP_LAYOUT_REVIEW_FRONT_TEXT_ROWS,
			front_has_media ? APP_LAYOUT_MEDIA_TEXT_WIDTH : APP_LAYOUT_TEXT_WIDTH
		);
		draw_card_media(
			app,
			card->front_media,
			APP_LAYOUT_MEDIA_IMAGE_X,
			APP_LAYOUT_MEDIA_FRONT_Y,
			APP_LAYOUT_REVIEW_FRONT_MEDIA_STATUS_ROW
		);
	}
}

static void draw_summary_screen(const struct app_state *app)
{
	const struct scheduler_session *session = &app->session;

	app_console_clear();
	printf("\x1b[1;1H" APP_COLOR_BLUE "anki3ds Review" APP_COLOR_RESET);
	if (!app_state_allows_study(app))
	{
		printf("\x1b[3;1H" APP_COLOR_RED "Review state error" APP_COLOR_RESET);
		printf("\x1b[5;1HCards:         %lu", (unsigned long)session->card_count);
		printf("\x1b[7;1HState:         %s", app->state_message);
		printf("\x1b[10;1HUse SELECT actions, then");
		printf("\x1b[11;1Hreset deck progress.");
		return;
	}

	printf("\x1b[3;1H" APP_COLOR_GREEN "No cards due now" APP_COLOR_RESET);
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
	printf("\x1b[10;1HState:         %s", app->state_message);
	printf(
		"\x1b[12;1H" APP_COLOR_RED "Y Again" APP_COLOR_RESET ": %u",
		session->rating_counts[SCHEDULER_RATING_AGAIN]
	);
	printf(
		"\x1b[13;1H" APP_COLOR_YELLOW "X Hard" APP_COLOR_RESET ":  %u",
		session->rating_counts[SCHEDULER_RATING_HARD]
	);
	printf(
		"\x1b[14;1H" APP_COLOR_GREEN "B Good" APP_COLOR_RESET ":  %u",
		session->rating_counts[SCHEDULER_RATING_GOOD]
	);
	printf(
		"\x1b[15;1H" APP_COLOR_BLUE "A Easy" APP_COLOR_RESET ":  %u",
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
	printf("\x1b[1;1H" APP_COLOR_BLUE "anki3ds Review" APP_COLOR_RESET);
	printf("\x1b[3;1H" APP_COLOR_BLUE "Actions" APP_COLOR_RESET);
	printf(
		"\x1b[6;1H%s%s Restore suspended cards" APP_COLOR_RESET,
		app->selected_action == ACTION_ITEM_UNSUSPEND_ALL ? APP_COLOR_GREEN : "",
		unsuspend_marker
	);
	printf(
		"\x1b[8;1H%s%s Daily limits" APP_COLOR_RESET,
		app->selected_action == ACTION_ITEM_DAILY_LIMITS ? APP_COLOR_GREEN : "",
		settings_marker
	);
	printf(
		"\x1b[10;1H%s%s Reset deck progress" APP_COLOR_RESET,
		app->selected_action == ACTION_ITEM_RESET_PROGRESS ? APP_COLOR_RED : "",
		reset_marker
	);
	printf("\x1b[13;1HDeck: ");
	print_truncated(app->deck.name, APP_LAYOUT_DECK_NAME_HEADER_WIDTH);

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
	printf("\x1b[1;1H" APP_COLOR_BLUE "anki3ds Review" APP_COLOR_RESET);
	printf("\x1b[3;1H" APP_COLOR_BLUE "Daily limits" APP_COLOR_RESET);
	printf("\x1b[5;1HDeck: ");
	print_truncated(app->deck.name, APP_LAYOUT_DECK_NAME_HEADER_WIDTH);
	printf(
		"\x1b[8;1H%s%s New cards:    %s" APP_COLOR_RESET,
		app->selected_setting == SETTING_ITEM_NEW_LIMIT ? APP_COLOR_GREEN : "",
		new_marker,
		new_limit
	);
	printf(
		"\x1b[10;1H%s%s Review cards: %s" APP_COLOR_RESET,
		app->selected_setting == SETTING_ITEM_REVIEW_LIMIT ? APP_COLOR_GREEN : "",
		review_marker,
		review_limit
	);
	printf("\x1b[13;1H0 means all available cards.");
	printf(
		"\x1b[16;1HCurrent source: %s",
		app_settings_load_result_name(app->settings_load_result)
	);
	printf("\x1b[18;1HSave: %s", app->settings_message);
}

static void draw_reset_confirmation_screen(const struct app_state *app)
{
	app_console_clear();
	printf("\x1b[1;1H" APP_COLOR_BLUE "anki3ds Review" APP_COLOR_RESET);
	printf("\x1b[3;1H" APP_COLOR_RED "Reset deck progress?" APP_COLOR_RESET);
	printf("\x1b[5;1HDeck: ");
	print_truncated(app->deck.name, APP_LAYOUT_DECK_NAME_HEADER_WIDTH);
	printf("\x1b[8;1HThis removes saved review");
	printf("\x1b[9;1Hstate for this deck.");
	printf("\x1b[12;1HCards stay in cards.tsv.");
	printf("\x1b[15;1HUse " APP_COLOR_RED "X" APP_COLOR_RESET " to reset.");
	printf("\x1b[17;1HUse B or SELECT to cancel.");
}

static void draw_restore_confirmation_screen(const struct app_state *app)
{
	app_console_clear();
	printf("\x1b[1;1H" APP_COLOR_BLUE "anki3ds Review" APP_COLOR_RESET);
	printf("\x1b[3;1H" APP_COLOR_YELLOW "Restore suspended cards?" APP_COLOR_RESET);
	printf("\x1b[5;1HDeck: ");
	print_truncated(app->deck.name, APP_LAYOUT_DECK_NAME_HEADER_WIDTH);
	printf(
		"\x1b[8;1HSuspended: %lu",
		(unsigned long)scheduler_suspended_count(&app->session)
	);
	printf("\x1b[11;1HRestored cards can become");
	printf("\x1b[12;1Hdue again if scheduled.");
	printf("\x1b[15;1HUse " APP_COLOR_GREEN "X" APP_COLOR_RESET " to restore.");
	printf("\x1b[17;1HUse B or SELECT to cancel.");
}

static void draw_suspend_confirmation_screen(const struct app_state *app)
{
	const struct card *card = current_card(app);

	app_console_clear();
	printf("\x1b[1;1H" APP_COLOR_BLUE "anki3ds Review" APP_COLOR_RESET);
	printf("\x1b[3;1H" APP_COLOR_YELLOW "Suspend current card?" APP_COLOR_RESET);
	printf("\x1b[5;1HDeck: ");
	print_truncated(app->deck.name, APP_LAYOUT_DECK_NAME_HEADER_WIDTH);
	if (card != NULL)
	{
		printf("\x1b[8;1HCard: ");
		print_truncated(card->front, APP_LAYOUT_TEXT_WIDTH - 6);
	}
	printf("\x1b[11;1HThis hides the card from");
	printf("\x1b[12;1Hreview until restored.");
	printf("\x1b[15;1HUse " APP_COLOR_YELLOW "X" APP_COLOR_RESET " to suspend.");
	printf("\x1b[17;1HUse B or SELECT to cancel.");
}

static void draw_exit_confirmation_screen(const struct app_state *app)
{
	app_console_clear();
	printf("\x1b[1;1H" APP_COLOR_BLUE "anki3ds Review" APP_COLOR_RESET);
	printf("\x1b[3;1H" APP_COLOR_YELLOW "Exit app?" APP_COLOR_RESET);
	printf("\x1b[6;1HProgress is saved after");
	printf("\x1b[7;1Heach review action.");
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
	printf("\x1b[1;1H" APP_COLOR_BLUE "anki3ds" APP_COLOR_RESET);

	switch (app->controls_return_mode)
	{
	case APP_MODE_DECK_SELECT:
		printf("\x1b[3;1H" APP_COLOR_BLUE "Deck list controls" APP_COLOR_RESET);
		if (app->deck_index.count > 0)
		{
			printf("\x1b[5;1HA: open selected deck");
			printf("\x1b[7;1HD-pad/Circle: move/page");
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
		printf("\x1b[3;1H" APP_COLOR_RED "Load error controls" APP_COLOR_RESET);
		printf("\x1b[5;1HB or SELECT: deck list");
		printf("\x1b[7;1HY: controls");
		break;
	case APP_MODE_SUMMARY:
		if (!app_state_allows_study(app))
		{
			printf(
				"\x1b[3;1H" APP_COLOR_RED
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
				"\x1b[3;1H" APP_COLOR_GREEN
				"No-due controls" APP_COLOR_RESET
			);
			printf("\x1b[5;1HB: deck list");
			printf("\x1b[7;1HL: undo last action");
			printf("\x1b[9;1HSELECT: actions");
			printf("\x1b[11;1HY: controls");
		}
		break;
	case APP_MODE_ACTIONS:
		printf("\x1b[3;1H" APP_COLOR_BLUE "Actions controls" APP_COLOR_RESET);
		printf("\x1b[5;1HA: choose selected");
		printf("\x1b[7;1HD-pad/Circle U/D: move/hold");
		printf("\x1b[9;1HB or SELECT: cancel");
		printf("\x1b[11;1HY: controls");
		break;
	case APP_MODE_SETTINGS:
		printf("\x1b[3;1H" APP_COLOR_BLUE "Daily-limit controls" APP_COLOR_RESET);
		printf("\x1b[5;1HD-pad/Circle U/D: field/hold");
		printf("\x1b[7;1HD-pad/Circle L/R: value/hold");
		printf("\x1b[9;1HA: save limits");
		printf("\x1b[11;1HB or SELECT: cancel");
		printf("\x1b[13;1HY: controls");
		break;
	case APP_MODE_REVIEW:
	default:
		if (app->revealed)
		{
			printf(
				"\x1b[3;1H" APP_COLOR_BLUE
				"Review rating controls" APP_COLOR_RESET
			);
			printf(
				"\x1b[5;1H" APP_COLOR_RED "Y: Again" APP_COLOR_RESET
				"      " APP_COLOR_YELLOW "X: Hard" APP_COLOR_RESET
			);
			printf(
				"\x1b[7;1H" APP_COLOR_GREEN "B: Good" APP_COLOR_RESET
				"       " APP_COLOR_BLUE "A: Easy" APP_COLOR_RESET
			);
			printf("\x1b[9;1HL: undo last action");
			printf("\x1b[11;1HR: confirm suspend");
			printf("\x1b[13;1HSELECT: actions");
			printf(
				"\x1b[15;1H" APP_COLOR_YELLOW
				"Use one rating button only." APP_COLOR_RESET
			);
		}
		else
		{
			printf(
				"\x1b[3;1H" APP_COLOR_BLUE
				"Review front controls" APP_COLOR_RESET
			);
			printf("\x1b[5;1HA: show answer");
			printf("\x1b[7;1HB: deck list");
			printf("\x1b[9;1HL: undo last action");
			printf("\x1b[11;1HR: confirm suspend");
			printf("\x1b[13;1HSELECT: actions");
			printf("\x1b[15;1HY: controls");
		}
		break;
	}

	draw_controls_screen_footer();
}

static void draw_battery_status(const struct app_state *app)
{
	if (!app->battery_status_available)
		return;

	if (app->battery_charging)
	{
		printf(
			"\x1b[29;1H" APP_COLOR_GREEN "Battery: %u/5 charging" APP_COLOR_RESET,
			(unsigned int)app->battery_level
		);
	}
	else if (app->battery_low)
	{
		printf(
			"\x1b[29;1H" APP_COLOR_RED "Battery: %u/5 low. Charge soon." APP_COLOR_RESET,
			(unsigned int)app->battery_level
		);
	}
	else
	{
		printf(
			"\x1b[29;1H" APP_COLOR_BLUE "Battery: %u/5" APP_COLOR_RESET,
			(unsigned int)app->battery_level
		);
	}
}

static void draw_due_legend(int row, bool include_suspended)
{
	printf(
		"\x1b[%d;1H" APP_COLOR_BLUE "N" APP_COLOR_RESET
		" new  " APP_COLOR_YELLOW "L" APP_COLOR_RESET
		" learn  " APP_COLOR_GREEN "R" APP_COLOR_RESET " review",
		row
	);
	if (include_suspended)
		printf(
			"\x1b[%d;1H" APP_COLOR_YELLOW "S" APP_COLOR_RESET " suspended",
			row + 1
		);
}

static void draw_status_message(const struct app_state *app)
{
	if (app->status_message[0] == '\0')
		return;

	printf("\x1b[24;1H" APP_COLOR_BLUE "Status:" APP_COLOR_RESET " ");
	printf("%s", status_message_color(app->status_message));
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
	printf("\x1b[1;1H" APP_COLOR_BLUE "anki3ds" APP_COLOR_RESET);
	printf("\x1b[3;1H" APP_COLOR_BLUE "Scanning decks..." APP_COLOR_RESET);
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
	printf("\x1b[1;1H" APP_COLOR_BLUE "Deck scan" APP_COLOR_RESET);
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
	if (app->deck_index.overflowed)
	{
		snprintf(
			app->status_message,
			sizeof(app->status_message),
			"Scan done; %lu/%lu decks shown",
			(unsigned long)app->deck_index.count,
			(unsigned long)app->deck_index.total_count
		);
	}
	else
	{
		snprintf(
			app->status_message,
			sizeof(app->status_message),
			"Scan done; %lu decks",
			(unsigned long)app->deck_index.count
		);
	}
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
	app_refresh_selected_deck_summary(app);
	target_mode = app_review_mode_for_session(app);
	app_update_review_return_modes_for_day_change(app, target_mode);

	if (!app_mode_is_review_surface(app->mode))
		return false;

	app->mode = target_mode;
	if (!app_state_allows_study(app))
		app_set_status(app, "Reset bad state first");
	else if (target_mode == APP_MODE_SUMMARY)
		app_set_status(app, "New day; no cards due");
	else
		app_set_status(app, "New day; cards due");

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
				"\x1b[1;1H" APP_COLOR_BLUE "Decks" APP_COLOR_RESET " %lu/%lu",
				(unsigned long)(app->selected_deck_index + 1),
				(unsigned long)app->deck_index.count
			);
		}
		else
		{
			printf("\x1b[1;1H" APP_COLOR_BLUE "Decks" APP_COLOR_RESET);
		}
		if (app->deck_index.count > 0)
		{
			const struct deck_summary *summary =
				&app->deck_summaries[app->selected_deck_index];

			printf("\x1b[3;1HA: open selected deck");
			printf("\x1b[5;1HD-pad/Circle: move/page");
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
						"\x1b[14;1H" APP_COLOR_YELLOW
						"Settings ignored; defaults" APP_COLOR_RESET
					);
					details_row = 16;
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
				printf(
					"\x1b[%d;1HCards: %lu  Suspended: %lu",
					details_row + 4,
					(unsigned long)summary->card_count,
					(unsigned long)summary->suspended_count
				);
				draw_due_legend(details_row + 6, true);
			}
			else if (summary->deck_load_result == DECK_LOAD_OK)
			{
				printf(
					"\x1b[14;1HState: " APP_COLOR_RED "%s" APP_COLOR_RESET,
					review_state_load_result_name(summary->state_load_result)
				);
				printf(
					"\x1b[16;1H" APP_COLOR_YELLOW
					"Open deck, then reset progress." APP_COLOR_RESET
				);
				printf(
					"\x1b[18;1HCards: %lu",
					(unsigned long)summary->card_count
				);
			}
			else
			{
				printf(
					"\x1b[14;1H" APP_COLOR_RED
					"Selected deck load error" APP_COLOR_RESET
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
				"\x1b[27;1HFound: %lu/%lu",
				(unsigned long)app->deck_index.count,
				(unsigned long)app->deck_index.total_count
			);
		}
		else
		{
			printf("\x1b[27;1HFound: %lu", (unsigned long)app->deck_index.count);
		}
		break;
	case APP_MODE_LOAD_ERROR:
		printf("\x1b[1;1H" APP_COLOR_RED "Load error" APP_COLOR_RESET);
		printf("\x1b[3;1HB or SELECT: deck list");
		printf("\x1b[5;1HSTART: confirm exit");
		printf("\x1b[7;1HY: controls");
		break;
	case APP_MODE_REVIEW:
	{
		char new_limit[16];
		char review_limit[16];

		format_daily_limit(new_limit, sizeof(new_limit), app->session.new_limit);
		format_daily_limit(review_limit, sizeof(review_limit), app->session.review_limit);
		printf("\x1b[1;1H" APP_COLOR_BLUE "Review" APP_COLOR_RESET);
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
				"\x1b[6;1H" APP_COLOR_RED "Y: Again" APP_COLOR_RESET
				"      " APP_COLOR_YELLOW "X: Hard" APP_COLOR_RESET
			);
			printf(
				"\x1b[8;1H" APP_COLOR_GREEN "B: Good" APP_COLOR_RESET
				"       " APP_COLOR_BLUE "A: Easy" APP_COLOR_RESET
			);
			printf("\x1b[10;1HL: undo last action");
			printf("\x1b[12;1HR: confirm suspend");
			printf("\x1b[14;1HSELECT: actions");
			printf("\x1b[16;1HSTART: confirm exit");
			printf(
				"\x1b[20;1H" APP_COLOR_YELLOW
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
		printf(
			"\x1b[27;1HSettings: %s",
			app_settings_load_result_name(app->settings_load_result)
		);
		break;
	}
	case APP_MODE_SUMMARY:
	{
		char new_limit[16];
		char review_limit[16];

		if (!app_state_allows_study(app))
		{
			printf("\x1b[1;1H" APP_COLOR_RED "Review state error" APP_COLOR_RESET);
			printf("\x1b[3;1HSELECT: actions");
			printf("\x1b[5;1HB: deck list");
			printf("\x1b[7;1HSTART: confirm exit");
			printf("\x1b[9;1HY: controls");
			printf(
				"\x1b[13;1H" APP_COLOR_YELLOW
				"Reset progress to study." APP_COLOR_RESET
			);
			break;
		}

		format_daily_limit(new_limit, sizeof(new_limit), app->session.new_limit);
		format_daily_limit(review_limit, sizeof(review_limit), app->session.review_limit);
		printf("\x1b[1;1H" APP_COLOR_GREEN "No cards due now" APP_COLOR_RESET);
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
		draw_due_legend(15, false);
		printf(
			"\x1b[13;1HSettings: %s",
			app_settings_load_result_name(app->settings_load_result)
		);
		printf("\x1b[27;1HReviewed this session: %u", app->session.reviewed_count);
		break;
	}
	case APP_MODE_ACTIONS:
		printf("\x1b[1;1H" APP_COLOR_BLUE "Actions" APP_COLOR_RESET);
		printf("\x1b[3;1HA: choose selected");
		printf("\x1b[5;1HD-pad/Circle U/D: move/hold");
		printf("\x1b[7;1HB or SELECT: cancel");
		printf("\x1b[9;1HSTART: confirm exit");
		printf("\x1b[11;1HY: controls");
		break;
	case APP_MODE_SETTINGS:
		printf("\x1b[1;1H" APP_COLOR_BLUE "Daily limits" APP_COLOR_RESET);
		printf("\x1b[3;1HD-pad/Circle U/D: field/hold");
		printf("\x1b[5;1HD-pad/Circle L/R: value/hold");
		printf("\x1b[7;1HA: save limits");
		printf("\x1b[9;1HB or SELECT: cancel");
		printf("\x1b[11;1HSTART: confirm exit");
		printf("\x1b[13;1HY: controls");
		break;
	case APP_MODE_CONTROLS:
		printf("\x1b[1;1H" APP_COLOR_BLUE "Controls" APP_COLOR_RESET);
		printf("\x1b[3;1HB, Y, or SELECT: back");
		printf("\x1b[5;1HSTART: confirm exit");
		break;
	case APP_MODE_CONFIRM_RESTORE:
		printf("\x1b[1;1H" APP_COLOR_YELLOW "Confirm restore" APP_COLOR_RESET);
		printf("\x1b[3;1H" APP_COLOR_GREEN "X: restore cards" APP_COLOR_RESET);
		printf("\x1b[5;1HB or SELECT: cancel");
		printf("\x1b[7;1HSTART: confirm exit");
		break;
	case APP_MODE_CONFIRM_SUSPEND:
		printf("\x1b[1;1H" APP_COLOR_YELLOW "Confirm suspend" APP_COLOR_RESET);
		printf("\x1b[3;1H" APP_COLOR_YELLOW "X: suspend card" APP_COLOR_RESET);
		printf("\x1b[5;1HB or SELECT: cancel");
		printf("\x1b[7;1HSTART: confirm exit");
		break;
	case APP_MODE_CONFIRM_RESET:
		printf("\x1b[1;1H" APP_COLOR_RED "Confirm reset" APP_COLOR_RESET);
		printf("\x1b[3;1H" APP_COLOR_RED "X: reset progress" APP_COLOR_RESET);
		printf("\x1b[5;1HB or SELECT: cancel");
		printf("\x1b[7;1HSTART: confirm exit");
		break;
	case APP_MODE_CONFIRM_EXIT:
		printf("\x1b[1;1H" APP_COLOR_YELLOW "Confirm exit" APP_COLOR_RESET);
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
	return app->state_save_result == REVIEW_STATE_SAVE_OK;
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
			app->selected_deck_index = app->deck_index.count - 1;
		else
			app->selected_deck_index += page_size;
	}
	else if (app->selected_deck_index < page_size)
	{
		app->selected_deck_index = 0;
	}
	else
	{
		app->selected_deck_index -= page_size;
	}

	return app->selected_deck_index != old_index;
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
		app_set_status(app, "Nothing to undo");
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
	app_set_status(app, log_saved ? "Undo saved" : "Undo saved; log skipped");
	app->revealed = false;
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
		app_set_status(app, "Nothing to suspend");
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

	if (scheduler_is_complete(&app->session))
	{
		app_set_status(
			app,
			log_saved ? "Suspend saved; no cards due" : "Suspend saved; log skipped"
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
		app_set_status(app, "Nothing suspended");
		app->mode = app->action_return_mode;
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

	if (scheduler_is_complete(&app->session))
		app->mode = APP_MODE_SUMMARY;
	else
		app->mode = APP_MODE_REVIEW;

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
	scheduler_set_daily_limits(
		&app->session,
		app->settings.new_limit,
		app->settings.review_limit
	);
	app->revealed = false;
	app->mode = app_review_mode_for_session(app);

	if (!app_state_allows_study(app))
	{
		app_set_status(app, "Limits saved; reset state");
	}
	else if (app->mode == APP_MODE_SUMMARY)
	{
		app_set_status(app, "Limits saved; no cards due");
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
		return false;

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
		app_set_status(app, "Restore canceled");
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
		app_set_status(app, "Suspend canceled");
		return true;
	}

	return false;
}

static bool app_handle_exit_confirmation_input(
	struct app_state *app,
	unsigned int buttons_down,
	unsigned int buttons_active
)
{
	if (app_command_pressed(buttons_down, buttons_active, APP_CONTROL_BUTTON_A))
	{
		app->exit_requested = true;
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
		app->mode = app->exit_return_mode;
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
		if (app->selected_setting == SETTING_ITEM_NEW_LIMIT)
			app->selected_setting = SETTING_ITEM_REVIEW_LIMIT;
		else
			app->selected_setting = SETTING_ITEM_NEW_LIMIT;
		return true;
	}

	if (app_controls_left_right_triggered(buttons_down, buttons_active, &right))
	{
		unsigned int *limit = selected_daily_limit(app);

		*limit = adjusted_daily_limit(*limit, right);
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
		app->mode = app->action_return_mode;
		return true;
	}

	return false;
}

static bool app_handle_controls_input(
	struct app_state *app,
	unsigned int buttons_down,
	unsigned int buttons_active
)
{
	if (
		app_command_pressed(buttons_down, buttons_active, APP_CONTROL_BUTTON_B) ||
		app_command_pressed(buttons_down, buttons_active, APP_CONTROL_BUTTON_Y) ||
		app_command_pressed(
			buttons_down,
			buttons_active,
			APP_CONTROL_BUTTON_SELECT
		)
	)
	{
		app->mode = app->controls_return_mode;
		app_set_status(app, "Returned");
		return true;
	}

	return false;
}

static bool app_handle_input(
	struct app_state *app,
	u32 keys_down,
	u32 keys_active
)
{
	unsigned int buttons_down = app_controls_buttons_from_3ds_keys(keys_down);
	unsigned int buttons_active = app_controls_buttons_from_3ds_keys(keys_active);
	enum scheduler_rating rating;

	if (app->mode == APP_MODE_CONFIRM_EXIT)
		return app_handle_exit_confirmation_input(
			app,
			buttons_down,
			buttons_active
		);

	if (
		app_command_pressed(
			buttons_down,
			buttons_active,
			APP_CONTROL_BUTTON_START
		)
	)
	{
		app_open_exit_confirmation(app);
		return true;
	}

	if (app->mode == APP_MODE_CONTROLS)
		return app_handle_controls_input(app, buttons_down, buttons_active);

	if (
		app_command_pressed(buttons_down, buttons_active, APP_CONTROL_BUTTON_Y) &&
		app_controls_can_open(app_control_mode_for_app_mode(app->mode), app->revealed)
	)
	{
		app_open_controls(app);
		return true;
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

	if (
		app->mode == APP_MODE_LOAD_ERROR &&
		(
			app_command_pressed(
				buttons_down,
				buttons_active,
				APP_CONTROL_BUTTON_B
			) ||
			app_command_pressed(
				buttons_down,
				buttons_active,
				APP_CONTROL_BUTTON_SELECT
			)
		)
	)
	{
		app_return_to_deck_select(app);
		return true;
	}

	if (
		(
			app->mode == APP_MODE_SUMMARY ||
			(app->mode == APP_MODE_REVIEW && !app->revealed)
		) &&
		app_command_pressed(buttons_down, buttons_active, APP_CONTROL_BUTTON_B)
	)
	{
		app_return_to_deck_select(app);
		return true;
	}

	if (
		(app->mode == APP_MODE_REVIEW || app->mode == APP_MODE_SUMMARY) &&
		app_command_pressed(
			buttons_down,
			buttons_active,
			APP_CONTROL_BUTTON_SELECT
		)
	)
	{
		app_open_actions(app);
		return true;
	}

	if (
		(app->mode == APP_MODE_REVIEW || app->mode == APP_MODE_SUMMARY) &&
		app_state_allows_study(app) &&
		app_command_pressed(buttons_down, buttons_active, APP_CONTROL_BUTTON_L)
	)
	{
		return undo_last_action(app);
	}

	if (app->mode != APP_MODE_REVIEW)
		return false;

	if (app_command_pressed(buttons_down, buttons_active, APP_CONTROL_BUTTON_R))
	{
		app_open_suspend_confirmation(app);
		return true;
	}

	if (!app->revealed)
	{
		if (
			app_controls_should_show_answer_triggered(
				buttons_down,
				buttons_active,
				app->revealed
			)
		)
		{
			app->revealed = true;
			return true;
		}

		return false;
	}

	if (
		app_controls_rating_for_trigger(
			buttons_down,
			buttons_active,
			app->revealed,
			&rating
		)
	)
		return rate_current_card(app, rating);

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
	struct app_control_repeat navigation_repeat;

	gfxInitDefault();
	consoleInit(GFX_TOP, &top_screen);
	consoleInit(GFX_BOTTOM, &bottom_screen);

	app_controls_repeat_init(&navigation_repeat);
	app_init(&app);
	now = time(NULL);
	app_power_schedule_next_battery_poll(&next_battery_poll_time, now);
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
			if (idle_wait_count < IDLE_INPUT_MAX_WAIT_COUNT)
				idle_wait_count++;
		}

		hidScanInput();

		u32 keys_down = hidKeysDown();
		u32 keys_held = hidKeysHeld();
		unsigned int buttons_down = app_controls_buttons_from_3ds_keys(keys_down);
		unsigned int buttons_held = app_controls_buttons_from_3ds_keys(keys_held);
		unsigned int repeat_buttons = 0;
		if (app_mode_uses_navigation_repeat(app.mode))
		{
			repeat_buttons = app_controls_repeat_buttons(
				&navigation_repeat,
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
		keys_down |= keys_for_repeat_buttons(repeat_buttons);
		u32 keys_active = keys_down | keys_held;
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

		if (app_handle_input(&app, keys_down, keys_active))
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
