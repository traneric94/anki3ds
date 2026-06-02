#include <3ds.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app_settings.h"
#include "deck.h"
#include "deck_index.h"
#include "deck_summary.h"
#include "media_cache.h"
#include "media_image.h"
#include "review_state.h"
#include "scheduler.h"

#define APP_VERSION "0.5.0-dev"
#define SECONDS_PER_DAY 86400
#define IDLE_INPUT_WAIT_NS 100000000LL
#define TEXT_LEFT 1
#define TEXT_WIDTH 48
#define MEDIA_TEXT_WIDTH 25
#define TOP_SCREEN_WIDTH 400
#define TOP_SCREEN_HEIGHT 240
#define MEDIA_IMAGE_X 224
#define MEDIA_FRONT_Y 72
#define MEDIA_BACK_Y 160
#define DECK_NAME_HEADER_WIDTH 42
#define DECK_NAME_SELECTOR_WIDTH 32
#define DECK_SELECTOR_FIRST_ROW 5
#define DECK_SELECTOR_VISIBLE_ROWS 16

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
	enum action_item selected_action;
	bool revealed;
	bool exit_requested;
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
	char active_settings_path[DECK_INDEX_MAX_PATH_LENGTH];
	char active_media_path[DECK_INDEX_MAX_PATH_LENGTH];
	struct app_settings settings;
	struct app_settings edited_settings;
	struct deck_index deck_index;
	struct deck_summary deck_summaries[DECK_INDEX_MAX_DECKS];
	struct deck deck;
	struct scheduler_session session;
};

static PrintConsole top_screen;
static PrintConsole bottom_screen;
static struct media_cache media_cache;

static void select_top_screen(void)
{
	consoleSelect(&top_screen);
}

static void select_bottom_screen(void)
{
	consoleSelect(&bottom_screen);
}

static void console_move(int row, int column)
{
	printf("\x1b[%d;%dH", row, column);
}

static unsigned int current_day(void)
{
	time_t now = time(NULL);

	if (now == (time_t)-1 || now < 0)
		return 0;

	return (unsigned int)(now / SECONDS_PER_DAY);
}

static void draw_wrapped_text_columns(
	const char *text,
	int row,
	int max_rows,
	int max_columns
)
{
	int current_row = row;
	int column = TEXT_LEFT;
	bool truncated = false;

	console_move(current_row, TEXT_LEFT);

	for (size_t index = 0; text[index] != '\0'; index++)
	{
		char value = text[index];

		if (current_row >= row + max_rows)
		{
			truncated = true;
			break;
		}

		if (value == '\r')
			continue;

		if (value == '\n')
		{
			current_row++;
			column = TEXT_LEFT;
			if (current_row < row + max_rows)
				console_move(current_row, TEXT_LEFT);
			continue;
		}

		if (value == '\t')
			value = ' ';

		if (column >= TEXT_LEFT + max_columns)
		{
			current_row++;
			column = TEXT_LEFT;
			if (current_row >= row + max_rows)
			{
				truncated = true;
				break;
			}
			console_move(current_row, TEXT_LEFT);
		}

		putchar(value);
		column++;
	}

	if (truncated && max_rows > 0)
	{
		console_move(row + max_rows - 1, TEXT_LEFT + max_columns - 3);
		printf("...");
	}
}

static void print_truncated(const char *text, size_t max_columns)
{
	size_t length = strlen(text);

	if (length <= max_columns)
	{
		printf("%s", text);
		return;
	}

	if (max_columns <= 3)
	{
		for (size_t index = 0; index < max_columns; index++)
			putchar(text[index]);
		return;
	}

	for (size_t index = 0; index < max_columns - 3; index++)
		putchar(text[index]);
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

static void wait_for_idle_input(void)
{
	hidWaitForAnyEvent(true, 0, IDLE_INPUT_WAIT_NS);
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

		if (screen_y < 0 || screen_y >= TOP_SCREEN_HEIGHT)
			continue;

		for (unsigned int source_x = 0; source_x < image->width; source_x++)
		{
			int screen_x = x + (int)source_x;
			uint16_t pixel;
			size_t offset;

			if (screen_x < 0 || screen_x >= TOP_SCREEN_WIDTH)
				continue;

			pixel = image->pixels[source_y * image->width + source_x];
			offset = (
				(size_t)(TOP_SCREEN_HEIGHT - screen_y - 1) +
				(size_t)screen_x * TOP_SCREEN_HEIGHT
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
		printf("\x1b[%d;1HMedia path too long", status_row);
		return;
	}

	slot = media_cache_load(&media_cache, path);
	if (slot->result == MEDIA_IMAGE_LOAD_OK)
	{
		draw_top_image(&slot->image, x, y);
		return;
	}

	printf("\x1b[%d;1HMedia ", status_row);
	print_truncated(media_name, 24);
	printf(": %s", media_image_load_result_name(slot->result));
}

static void app_scan_decks(struct app_state *app)
{
	unsigned int today = current_day();
	char selected_deck_id[DECK_MAX_NAME_LENGTH];
	const struct deck_entry *selected_deck;

	selected_deck_id[0] = '\0';
	selected_deck = deck_index_get(&app->deck_index, app->selected_deck_index);
	if (selected_deck != NULL)
		copy_string(selected_deck_id, sizeof(selected_deck_id), selected_deck->id);

	deck_index_scan(&app->deck_index, DECK_INDEX_ROOT_PATH);

	for (size_t index = 0; index < DECK_INDEX_MAX_DECKS; index++)
	{
		if (index < app->deck_index.count)
		{
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

static void app_load_selected_deck(struct app_state *app)
{
	const struct deck_entry *entry;

	if (app->deck_index.count == 0)
	{
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
		app->mode = APP_MODE_LOAD_ERROR;
		return;
	}

	copy_string(app->active_cards_path, sizeof(app->active_cards_path), entry->cards_path);
	copy_string(app->active_state_path, sizeof(app->active_state_path), entry->state_path);
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
		scheduler_init(&app->session, app->deck.card_count, current_day());
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

		if (scheduler_is_complete(&app->session))
			app->mode = APP_MODE_SUMMARY;
		else
			app->mode = APP_MODE_REVIEW;
	}
	else
	{
		app->mode = APP_MODE_LOAD_ERROR;
		scheduler_init(&app->session, 0, current_day());
	}
}

static void app_open_actions(struct app_state *app)
{
	app->action_return_mode = app->mode;
	app->selected_action = ACTION_ITEM_UNSUSPEND_ALL;
	app->mode = APP_MODE_ACTIONS;
}

static void app_open_settings(struct app_state *app)
{
	app->edited_settings = app->settings;
	app->selected_setting = SETTING_ITEM_NEW_LIMIT;
	app->settings_message = "settings not saved";
	app->mode = APP_MODE_SETTINGS;
}

static void app_open_reset_confirmation(struct app_state *app)
{
	app->mode = APP_MODE_CONFIRM_RESET;
}

static void app_open_exit_confirmation(struct app_state *app)
{
	app->exit_return_mode = app->mode;
	app->mode = APP_MODE_CONFIRM_EXIT;
}

static void app_init(struct app_state *app)
{
	memset(app, 0, sizeof(*app));
	media_cache_init(&media_cache);
	app_scan_decks(app);
	app->mode = APP_MODE_DECK_SELECT;
}

static void draw_header(const struct app_state *app)
{
	printf("\x1b[1;1Hanki3ds Review");
	printf("\x1b[2;1HDeck: ");
	print_truncated(app->deck.name, DECK_NAME_HEADER_WIDTH);
	printf(
		"\x1b[3;1HDue: %lu  New: %lu  Reviewed: %u",
		(unsigned long)app->session.due_count,
		(unsigned long)scheduler_new_due_count(&app->session),
		app->session.reviewed_count
	);
	printf(
		"\x1b[4;1HState: %s  Day: %u",
		app->state_message,
		app->session.today
	);
}

static void draw_card_status(const struct app_state *app, const struct scheduler_card *state)
{
	printf(
		"\x1b[5;1H%lu/%lu  Due:%lu  Int:%ud  Ease:%u.%02u",
		(unsigned long)(scheduler_current_index(&app->session) + 1),
		(unsigned long)app->session.card_count,
		(unsigned long)app->session.due_count,
		state->interval_days,
		state->ease_permille / 1000,
		(state->ease_permille % 1000) / 10
	);
}

static void draw_deck_select_screen(const struct app_state *app)
{
	size_t first_visible_deck = 0;
	size_t visible_deck_count;

	consoleClear();
	printf("\x1b[1;1Hanki3ds");
	printf("\x1b[3;1HSelect deck");

	if (app->deck_index.count == 0)
	{
		printf("\x1b[5;1HNo decks found.");
		printf("\x1b[7;1HCreate a folder like:");
		printf("\x1b[8;1H%s/my-deck/cards.tsv", DECK_INDEX_ROOT_PATH);
	}
	else
	{
		if (app->selected_deck_index >= DECK_SELECTOR_VISIBLE_ROWS)
			first_visible_deck = app->selected_deck_index - DECK_SELECTOR_VISIBLE_ROWS + 1;
		visible_deck_count = app->deck_index.count - first_visible_deck;
		if (visible_deck_count > DECK_SELECTOR_VISIBLE_ROWS)
			visible_deck_count = DECK_SELECTOR_VISIBLE_ROWS;

		for (size_t visible_index = 0; visible_index < visible_deck_count; visible_index++)
		{
			size_t index = first_visible_deck + visible_index;
			const char *marker = index == app->selected_deck_index ? ">" : " ";
			const struct deck_summary *summary = &app->deck_summaries[index];

			printf(
				"\x1b[%lu;1H%s ",
				(unsigned long)(DECK_SELECTOR_FIRST_ROW + visible_index),
				marker
			);
			print_truncated(
				app->deck_index.entries[index].display_name,
				DECK_NAME_SELECTOR_WIDTH
			);
			if (summary->deck_load_result == DECK_LOAD_OK)
			{
				printf(
					" D:%lu N:%lu S:%lu",
					(unsigned long)summary->due_count,
					(unsigned long)summary->new_due_count,
					(unsigned long)summary->suspended_count
				);
			}
			else
			{
				printf(" load error");
			}
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
			printf("\x1b[23;1HShowing first %u decks.", (unsigned int)DECK_INDEX_MAX_DECKS);
	}
}

static void draw_load_error_screen(const struct app_state *app)
{
	consoleClear();
	printf("\x1b[1;1Hanki3ds");
	printf("\x1b[3;1HCould not load deck.");
	printf(
		"\x1b[5;1H%s",
		app->active_cards_path[0] ? app->active_cards_path : DECK_INDEX_ROOT_PATH
	);
	printf("\x1b[7;1HResult: %s", deck_load_result_name(app->load_result));
	printf("\x1b[10;1HCopy cards.tsv to the path above.");
}

static void draw_review_screen(const struct app_state *app)
{
	const struct card *card = current_card(app);
	bool front_has_media;
	bool back_has_media;

	consoleClear();
	draw_header(app);

	if (card == NULL)
	{
		printf("\x1b[6;1HNo cards are due today.");
		return;
	}

	front_has_media = card->front_media[0] != '\0';
	back_has_media = card->back_media[0] != '\0';

	draw_card_status(app, &app->session.cards[scheduler_current_index(&app->session)]);
	printf("\x1b[7;1HFront");
	printf("\x1b[8;1H------------------------------------------------");

	if (app->revealed)
	{
		draw_wrapped_text_columns(
			card->front,
			9,
			5,
			front_has_media ? MEDIA_TEXT_WIDTH : TEXT_WIDTH
		);
		printf("\x1b[15;1HBack");
		printf("\x1b[16;1H------------------------------------------------");
		draw_wrapped_text_columns(
			card->back,
			17,
			6,
			back_has_media ? MEDIA_TEXT_WIDTH : TEXT_WIDTH
		);
		draw_card_media(app, card->front_media, MEDIA_IMAGE_X, MEDIA_FRONT_Y, 14);
		draw_card_media(app, card->back_media, MEDIA_IMAGE_X, MEDIA_BACK_Y, 23);
	}
	else
	{
		draw_wrapped_text_columns(
			card->front,
			9,
			15,
			front_has_media ? MEDIA_TEXT_WIDTH : TEXT_WIDTH
		);
		draw_card_media(app, card->front_media, MEDIA_IMAGE_X, MEDIA_FRONT_Y, 21);
	}
}

static void draw_summary_screen(const struct app_state *app)
{
	const struct scheduler_session *session = &app->session;

	consoleClear();
	printf("\x1b[1;1Hanki3ds Review");
	printf("\x1b[3;1HNo cards due now");
	printf("\x1b[5;1HCards:    %lu", (unsigned long)session->card_count);
	printf("\x1b[6;1HReviewed: %u", session->reviewed_count);
	printf("\x1b[7;1HTotal rev:%u", review_count_total(session));
	printf(
		"\x1b[8;1HSuspended:%lu",
		(unsigned long)scheduler_suspended_count(session)
	);
	printf("\x1b[9;1HState:    %s", app->state_message);
	printf(
		"\x1b[11;1HY Again: %u",
		session->rating_counts[SCHEDULER_RATING_AGAIN]
	);
	printf(
		"\x1b[12;1HX Hard:  %u",
		session->rating_counts[SCHEDULER_RATING_HARD]
	);
	printf(
		"\x1b[13;1HB Good:  %u",
		session->rating_counts[SCHEDULER_RATING_GOOD]
	);
	printf(
		"\x1b[14;1HA Easy:  %u",
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

	consoleClear();
	printf("\x1b[1;1Hanki3ds Review");
	printf("\x1b[3;1HActions");
	printf("\x1b[6;1H%s Restore suspended cards", unsuspend_marker);
	printf("\x1b[8;1H%s Daily limits", settings_marker);
	printf("\x1b[10;1H%s Reset deck progress", reset_marker);
	printf("\x1b[13;1HDeck: ");
	print_truncated(app->deck.name, DECK_NAME_HEADER_WIDTH);

	if (app->selected_action == ACTION_ITEM_UNSUSPEND_ALL)
	{
		printf(
			"\x1b[16;1HSuspended cards: %lu",
			(unsigned long)scheduler_suspended_count(&app->session)
		);
		printf("\x1b[17;1HClears all suspended flags");
		printf("\x1b[18;1Hfor the active deck.");
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

	consoleClear();
	printf("\x1b[1;1Hanki3ds Review");
	printf("\x1b[3;1HDaily limits");
	printf("\x1b[5;1HDeck: ");
	print_truncated(app->deck.name, DECK_NAME_HEADER_WIDTH);
	printf("\x1b[8;1H%s New cards:    %s", new_marker, new_limit);
	printf("\x1b[10;1H%s Review cards: %s", review_marker, review_limit);
	printf("\x1b[13;1H0 means all available cards.");
	printf(
		"\x1b[16;1HCurrent source: %s",
		app_settings_load_result_name(app->settings_load_result)
	);
	printf("\x1b[18;1HSave: %s", app->settings_message);
}

static void draw_reset_confirmation_screen(const struct app_state *app)
{
	consoleClear();
	printf("\x1b[1;1Hanki3ds Review");
	printf("\x1b[3;1HReset deck progress?");
	printf("\x1b[5;1HDeck: ");
	print_truncated(app->deck.name, DECK_NAME_HEADER_WIDTH);
	printf("\x1b[8;1HThis removes saved review");
	printf("\x1b[9;1Hstate for this deck.");
	printf("\x1b[12;1HCards stay in cards.tsv.");
	printf("\x1b[15;1HUse X to reset.");
	printf("\x1b[17;1HUse B or SELECT to cancel.");
}

static void draw_exit_confirmation_screen(const struct app_state *app)
{
	consoleClear();
	printf("\x1b[1;1Hanki3ds Review");
	printf("\x1b[3;1HExit app?");
	printf("\x1b[6;1HProgress is saved after");
	printf("\x1b[7;1Heach review action.");
	printf("\x1b[10;1HUse START to exit.");
	printf("\x1b[12;1HUse B or SELECT to cancel.");
}

static void draw_bottom_controls_screen(const struct app_state *app)
{
	consoleClear();

	switch (app->mode)
	{
	case APP_MODE_DECK_SELECT:
		printf("\x1b[1;1HDecks");
		if (app->deck_index.count > 0)
		{
			const struct deck_summary *summary =
				&app->deck_summaries[app->selected_deck_index];

			printf("\x1b[3;1HA: open selected deck");
			printf("\x1b[5;1HD-pad Up/Down: choose");
			printf("\x1b[7;1HSELECT: rescan decks");
			printf("\x1b[9;1HSTART: confirm exit");
			if (summary->deck_load_result == DECK_LOAD_OK)
			{
				printf(
					"\x1b[12;1HSelected: Due %lu  New %lu",
					(unsigned long)summary->due_count,
					(unsigned long)summary->new_due_count
				);
				printf(
					"\x1b[14;1HCards: %lu  Suspended: %lu",
					(unsigned long)summary->card_count,
					(unsigned long)summary->suspended_count
				);
			}
			else
			{
				printf("\x1b[12;1HSelected deck load error");
			}
		}
		else
		{
			printf("\x1b[3;1HSELECT: rescan decks");
			printf("\x1b[5;1HSTART: confirm exit");
		}
		printf("\x1b[27;1HFound: %lu", (unsigned long)app->deck_index.count);
		break;
	case APP_MODE_LOAD_ERROR:
		printf("\x1b[1;1HLoad error");
		printf("\x1b[3;1HB or SELECT: deck list");
		printf("\x1b[5;1HSTART: confirm exit");
		break;
	case APP_MODE_REVIEW:
	{
		char new_limit[16];
		char review_limit[16];

		format_daily_limit(new_limit, sizeof(new_limit), app->session.new_limit);
		format_daily_limit(review_limit, sizeof(review_limit), app->session.review_limit);
		printf("\x1b[1;1HReview");
		printf(
			"\x1b[3;1HDue %lu   New %lu",
			(unsigned long)app->session.due_count,
			(unsigned long)scheduler_new_due_count(&app->session)
		);
		printf(
			"\x1b[4;1HNew %u/%s  Review %u/%s",
			app->session.new_count_today,
			new_limit,
			app->session.review_count_today,
			review_limit
		);

		if (app->revealed)
		{
			printf("\x1b[6;1HY: Again      X: Hard");
			printf("\x1b[8;1HB: Good       A: Easy");
			printf("\x1b[10;1HL: undo last action");
			printf("\x1b[12;1HR: suspend card");
			printf("\x1b[14;1HSELECT: actions");
			printf("\x1b[16;1HSTART: confirm exit");
		}
		else
		{
			printf("\x1b[6;1HA: show answer");
			printf("\x1b[8;1HB: deck list");
			printf("\x1b[10;1HL: undo last action");
			printf("\x1b[12;1HR: suspend card");
			printf("\x1b[14;1HSELECT: actions");
			printf("\x1b[16;1HSTART: confirm exit");
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

		format_daily_limit(new_limit, sizeof(new_limit), app->session.new_limit);
		format_daily_limit(review_limit, sizeof(review_limit), app->session.review_limit);
		printf("\x1b[1;1HNo cards due now");
		printf("\x1b[3;1HB: deck list");
		printf("\x1b[5;1HL: undo last action");
		printf("\x1b[7;1HSELECT: actions");
		printf("\x1b[9;1HSTART: confirm exit");
		printf(
			"\x1b[11;1HNew %u/%s  Review %u/%s",
			app->session.new_count_today,
			new_limit,
			app->session.review_count_today,
			review_limit
		);
		printf(
			"\x1b[13;1HSettings: %s",
			app_settings_load_result_name(app->settings_load_result)
		);
		printf("\x1b[27;1HReviewed this session: %u", app->session.reviewed_count);
		break;
	}
	case APP_MODE_ACTIONS:
		printf("\x1b[1;1HActions");
		printf("\x1b[3;1HA: confirm selected");
		printf("\x1b[5;1HD-pad Up/Down: choose");
		printf("\x1b[7;1HB or SELECT: cancel");
		printf("\x1b[9;1HSTART: confirm exit");
		break;
	case APP_MODE_SETTINGS:
		printf("\x1b[1;1HDaily limits");
		printf("\x1b[3;1HD-pad Up/Down: field");
		printf("\x1b[5;1HD-pad Left/Right: value");
		printf("\x1b[7;1HA: save limits");
		printf("\x1b[9;1HB or SELECT: cancel");
		printf("\x1b[11;1HSTART: confirm exit");
		break;
	case APP_MODE_CONFIRM_RESET:
		printf("\x1b[1;1HConfirm reset");
		printf("\x1b[3;1HX: reset progress");
		printf("\x1b[5;1HB or SELECT: cancel");
		printf("\x1b[7;1HSTART: confirm exit");
		break;
	case APP_MODE_CONFIRM_EXIT:
		printf("\x1b[1;1HConfirm exit");
		printf("\x1b[3;1HSTART: exit app");
		printf("\x1b[5;1HB or SELECT: cancel");
		break;
	}
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

static bool rate_current_card(struct app_state *app, enum scheduler_rating rating)
{
	if (!app->revealed)
		return false;

	scheduler_rate_current(&app->session, rating);
	app->state_save_result = review_state_save(
		&app->deck,
		&app->session,
		app->active_state_path
	);
	app->state_message = review_state_save_result_name(app->state_save_result);
	app->revealed = false;

	if (scheduler_is_complete(&app->session))
		app->mode = APP_MODE_SUMMARY;

	return true;
}

static bool undo_last_action(struct app_state *app)
{
	if (!scheduler_undo_last(&app->session))
	{
		app->state_message = "nothing to undo";
		return true;
	}

	app->state_save_result = review_state_save(
		&app->deck,
		&app->session,
		app->active_state_path
	);
	app->state_message =
		app->state_save_result == REVIEW_STATE_SAVE_OK ?
		"undone" :
		review_state_save_result_name(app->state_save_result);
	app->revealed = false;
	app->mode = APP_MODE_REVIEW;
	return true;
}

static bool suspend_current_card(struct app_state *app)
{
	if (!scheduler_suspend_current(&app->session))
	{
		app->state_message = "nothing to suspend";
		return true;
	}

	app->state_save_result = review_state_save(
		&app->deck,
		&app->session,
		app->active_state_path
	);
	app->state_message =
		app->state_save_result == REVIEW_STATE_SAVE_OK ?
		"suspended" :
		review_state_save_result_name(app->state_save_result);
	app->revealed = false;

	if (scheduler_is_complete(&app->session))
		app->mode = APP_MODE_SUMMARY;

	return true;
}

static bool reset_progress(struct app_state *app)
{
	if (app->active_state_path[0] == '\0')
	{
		app->state_message = "reset failed";
		return false;
	}

	if (!review_state_delete(app->active_state_path))
	{
		app->state_message = "reset failed";
		return false;
	}

	app_load_selected_deck(app);
	return true;
}

static bool unsuspend_all_cards(struct app_state *app)
{
	unsigned int unsuspended_count = scheduler_unsuspend_all(&app->session);

	if (unsuspended_count == 0)
	{
		app->state_message = "nothing suspended";
		app->mode = app->action_return_mode;
		return true;
	}

	app->state_save_result = review_state_save(
		&app->deck,
		&app->session,
		app->active_state_path
	);
	app->state_message =
		app->state_save_result == REVIEW_STATE_SAVE_OK ?
		"unsuspended" :
		review_state_save_result_name(app->state_save_result);
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
	app->state_message = app->settings_message;

	if (app->settings_save_result != APP_SETTINGS_SAVE_OK)
		return true;

	app->settings = app->edited_settings;
	app->settings_load_result = APP_SETTINGS_LOAD_OK;
	scheduler_set_daily_limits(
		&app->session,
		app->settings.new_limit,
		app->settings.review_limit
	);
	app->revealed = false;

	if (scheduler_is_complete(&app->session))
		app->mode = APP_MODE_SUMMARY;
	else
		app->mode = APP_MODE_REVIEW;

	return true;
}

static bool app_handle_deck_select_input(struct app_state *app, u32 keys_down)
{
	if (keys_down & KEY_SELECT)
	{
		app_scan_decks(app);
		return true;
	}

	if (app->deck_index.count == 0)
		return false;

	if (keys_down & KEY_DUP)
	{
		if (app->selected_deck_index == 0)
			app->selected_deck_index = app->deck_index.count - 1;
		else
			app->selected_deck_index--;
		return true;
	}

	if (keys_down & KEY_DDOWN)
	{
		app->selected_deck_index =
			(app->selected_deck_index + 1) % app->deck_index.count;
		return true;
	}

	if (keys_down & KEY_A)
	{
		app_load_selected_deck(app);
		return true;
	}

	return false;
}

static bool app_handle_actions_input(struct app_state *app, u32 keys_down)
{
	if (keys_down & KEY_DUP)
	{
		if (app->selected_action == ACTION_ITEM_UNSUSPEND_ALL)
			app->selected_action = ACTION_ITEM_COUNT - 1;
		else
			app->selected_action--;
		return true;
	}

	if (keys_down & KEY_DDOWN)
	{
		app->selected_action =
			(enum action_item)((app->selected_action + 1) % ACTION_ITEM_COUNT);
		return true;
	}

	if (keys_down & KEY_A)
	{
		if (app->selected_action == ACTION_ITEM_UNSUSPEND_ALL)
			return unsuspend_all_cards(app);
		if (app->selected_action == ACTION_ITEM_DAILY_LIMITS)
		{
			app_open_settings(app);
			return true;
		}

		app_open_reset_confirmation(app);
		return true;
	}

	if (keys_down & (KEY_B | KEY_SELECT))
	{
		app->mode = app->action_return_mode;
		return true;
	}

	return false;
}

static bool app_handle_reset_confirmation_input(struct app_state *app, u32 keys_down)
{
	if (keys_down & KEY_X)
	{
		if (!reset_progress(app))
			app->mode = APP_MODE_CONFIRM_RESET;
		return true;
	}

	if (keys_down & (KEY_B | KEY_SELECT))
	{
		app->mode = APP_MODE_ACTIONS;
		return true;
	}

	return false;
}

static bool app_handle_exit_confirmation_input(struct app_state *app, u32 keys_down)
{
	if (keys_down & KEY_START)
	{
		app->exit_requested = true;
		return true;
	}

	if (keys_down & (KEY_B | KEY_SELECT))
	{
		app->mode = app->exit_return_mode;
		return true;
	}

	return false;
}

static bool app_handle_settings_input(struct app_state *app, u32 keys_down)
{
	if (keys_down & (KEY_DUP | KEY_DDOWN))
	{
		if (app->selected_setting == SETTING_ITEM_NEW_LIMIT)
			app->selected_setting = SETTING_ITEM_REVIEW_LIMIT;
		else
			app->selected_setting = SETTING_ITEM_NEW_LIMIT;
		return true;
	}

	if (keys_down & (KEY_DLEFT | KEY_DRIGHT))
	{
		unsigned int *limit = selected_daily_limit(app);

		*limit = adjusted_daily_limit(*limit, (keys_down & KEY_DRIGHT) != 0);
		return true;
	}

	if (keys_down & KEY_A)
		return save_daily_limits(app);

	if (keys_down & (KEY_B | KEY_SELECT))
	{
		app->mode = app->action_return_mode;
		return true;
	}

	return false;
}

static bool app_handle_input(struct app_state *app, u32 keys_down)
{
	if (app->mode == APP_MODE_CONFIRM_EXIT)
		return app_handle_exit_confirmation_input(app, keys_down);

	if (keys_down & KEY_START)
	{
		app_open_exit_confirmation(app);
		return true;
	}

	if (app->mode == APP_MODE_DECK_SELECT)
		return app_handle_deck_select_input(app, keys_down);
	if (app->mode == APP_MODE_ACTIONS)
		return app_handle_actions_input(app, keys_down);
	if (app->mode == APP_MODE_SETTINGS)
		return app_handle_settings_input(app, keys_down);
	if (app->mode == APP_MODE_CONFIRM_RESET)
		return app_handle_reset_confirmation_input(app, keys_down);

	if (app->mode == APP_MODE_LOAD_ERROR && (keys_down & (KEY_B | KEY_SELECT)))
	{
		app_scan_decks(app);
		app->mode = APP_MODE_DECK_SELECT;
		return true;
	}

	if (
		(app->mode == APP_MODE_SUMMARY && (keys_down & KEY_B)) ||
		(app->mode == APP_MODE_REVIEW && !app->revealed && (keys_down & KEY_B))
	)
	{
		app_scan_decks(app);
		app->mode = APP_MODE_DECK_SELECT;
		return true;
	}

	if (
		(app->mode == APP_MODE_REVIEW || app->mode == APP_MODE_SUMMARY) &&
		(keys_down & KEY_SELECT)
	)
	{
		app_open_actions(app);
		return true;
	}

	if (
		(app->mode == APP_MODE_REVIEW || app->mode == APP_MODE_SUMMARY) &&
		(keys_down & KEY_L)
	)
	{
		return undo_last_action(app);
	}

	if (app->mode != APP_MODE_REVIEW)
		return false;

	if (keys_down & KEY_R)
		return suspend_current_card(app);

	if (!app->revealed)
	{
		if (keys_down & KEY_A)
		{
			app->revealed = true;
			return true;
		}

		return false;
	}

	if (keys_down & KEY_Y)
		return rate_current_card(app, SCHEDULER_RATING_AGAIN);
	if (keys_down & KEY_X)
		return rate_current_card(app, SCHEDULER_RATING_HARD);
	if (keys_down & KEY_B)
		return rate_current_card(app, SCHEDULER_RATING_GOOD);
	if (keys_down & KEY_A)
		return rate_current_card(app, SCHEDULER_RATING_EASY);

	return false;
}

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	static struct app_state app;
	bool frame_dirty = true;

	gfxInitDefault();
	consoleInit(GFX_TOP, &top_screen);
	consoleInit(GFX_BOTTOM, &bottom_screen);

	app_init(&app);
	draw_app(&app);

	while (aptMainLoop())
	{
		if (frame_dirty)
		{
			gfxFlushBuffers();
			gspWaitForVBlank();
			gfxSwapBuffers();
			frame_dirty = false;
		}
		else
		{
			wait_for_idle_input();
		}

		hidScanInput();

		u32 keys_down = hidKeysDown();

		if (app_handle_input(&app, keys_down))
		{
			if (app.exit_requested)
				break;

			draw_app(&app);
			frame_dirty = true;
		}
	}

	gfxExit();
	return 0;
}
