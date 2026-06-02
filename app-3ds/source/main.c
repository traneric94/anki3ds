#include <3ds.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "deck.h"
#include "deck_index.h"
#include "review_state.h"
#include "scheduler.h"

#define APP_VERSION "0.5.0-dev"
#define SECONDS_PER_DAY 86400
#define TEXT_LEFT 1
#define TEXT_WIDTH 48

enum app_mode
{
	APP_MODE_DECK_SELECT,
	APP_MODE_LOAD_ERROR,
	APP_MODE_REVIEW,
	APP_MODE_SUMMARY,
	APP_MODE_ACTIONS,
};

struct app_state
{
	enum app_mode mode;
	enum app_mode action_return_mode;
	bool revealed;
	enum deck_load_result load_result;
	enum review_state_load_result state_load_result;
	enum review_state_save_result state_save_result;
	const char *state_message;
	size_t selected_deck_index;
	char active_cards_path[DECK_INDEX_MAX_PATH_LENGTH];
	char active_state_path[DECK_INDEX_MAX_PATH_LENGTH];
	struct deck_index deck_index;
	struct deck deck;
	struct scheduler_session session;
};

static PrintConsole top_screen;
static PrintConsole bottom_screen;

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

static void draw_wrapped_text(const char *text, int row, int max_rows)
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

		if (column >= TEXT_LEFT + TEXT_WIDTH)
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
		console_move(row + max_rows - 1, TEXT_LEFT + TEXT_WIDTH - 3);
		printf("...");
	}
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

static void app_scan_decks(struct app_state *app)
{
	deck_index_scan(&app->deck_index, DECK_INDEX_ROOT_PATH);
	app->selected_deck_index = 0;
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
	deck_init(&app->deck, entry->id);
	app->revealed = false;
	app->state_load_result = REVIEW_STATE_LOAD_NOT_FOUND;
	app->state_save_result = REVIEW_STATE_SAVE_OK;
	app->state_message = "State: not loaded";
	app->load_result = deck_load_cards(&app->deck, app->active_cards_path);

	if (app->load_result == DECK_LOAD_OK)
	{
		scheduler_init(&app->session, app->deck.card_count, current_day());
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
	app->mode = APP_MODE_ACTIONS;
}

static void app_init(struct app_state *app)
{
	memset(app, 0, sizeof(*app));
	app_scan_decks(app);
	app->mode = APP_MODE_DECK_SELECT;
}

static void draw_header(const struct app_state *app)
{
	printf("\x1b[1;1Hanki3ds Review");
	printf(
		"\x1b[2;1HDeck: %s",
		app->deck.name
	);
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
		for (size_t index = 0; index < app->deck_index.count; index++)
		{
			const char *marker = index == app->selected_deck_index ? ">" : " ";

			printf(
				"\x1b[%lu;1H%s %s",
				(unsigned long)(5 + index),
				marker,
				app->deck_index.entries[index].id
			);
		}

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

	consoleClear();
	draw_header(app);

	if (card == NULL)
	{
		printf("\x1b[6;1HNo cards are due today.");
		return;
	}

	draw_card_status(app, &app->session.cards[scheduler_current_index(&app->session)]);
	printf("\x1b[7;1HFront");
	printf("\x1b[8;1H------------------------------------------------");

	if (app->revealed)
	{
		draw_wrapped_text(card->front, 9, 5);
		printf("\x1b[15;1HBack");
		printf("\x1b[16;1H------------------------------------------------");
		draw_wrapped_text(card->back, 17, 7);
	}
	else
	{
		draw_wrapped_text(card->front, 9, 15);
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
	printf("\x1b[8;1HState:    %s", app->state_message);
	printf(
		"\x1b[10;1HY Again: %u",
		session->rating_counts[SCHEDULER_RATING_AGAIN]
	);
	printf(
		"\x1b[11;1HX Hard:  %u",
		session->rating_counts[SCHEDULER_RATING_HARD]
	);
	printf(
		"\x1b[12;1HB Good:  %u",
		session->rating_counts[SCHEDULER_RATING_GOOD]
	);
	printf(
		"\x1b[13;1HA Easy:  %u",
		session->rating_counts[SCHEDULER_RATING_EASY]
	);
}

static void draw_actions_screen(const struct app_state *app)
{
	consoleClear();
	printf("\x1b[1;1Hanki3ds Review");
	printf("\x1b[3;1HActions");
	printf("\x1b[6;1H> Reset deck progress");
	printf("\x1b[8;1HDeck: %s", app->deck.name);
	printf("\x1b[10;1HThis removes state.tsv for");
	printf("\x1b[11;1Hthe active deck, then reloads.");
	printf("\x1b[14;1HState: %s", app->state_message);
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
			printf("\x1b[3;1HA: open selected deck");
			printf("\x1b[5;1HD-pad Up/Down: choose");
			printf("\x1b[7;1HSELECT: rescan decks");
			printf("\x1b[9;1HSTART: exit");
		}
		else
		{
			printf("\x1b[3;1HSELECT: rescan decks");
			printf("\x1b[5;1HSTART: exit");
		}
		printf("\x1b[27;1HFound: %lu", (unsigned long)app->deck_index.count);
		break;
	case APP_MODE_LOAD_ERROR:
		printf("\x1b[1;1HLoad error");
		printf("\x1b[3;1HB or SELECT: deck list");
		printf("\x1b[5;1HSTART: exit");
		break;
	case APP_MODE_REVIEW:
		printf("\x1b[1;1HReview");
		printf(
			"\x1b[3;1HDue %lu   New %lu",
			(unsigned long)app->session.due_count,
			(unsigned long)scheduler_new_due_count(&app->session)
		);

		if (app->revealed)
		{
			printf("\x1b[6;1HY: Again      X: Hard");
			printf("\x1b[8;1HB: Good       A: Easy");
			printf("\x1b[12;1HSELECT: actions");
			printf("\x1b[14;1HSTART: exit");
		}
		else
		{
			printf("\x1b[6;1HA: show answer");
			printf("\x1b[8;1HB: deck list");
			printf("\x1b[10;1HSELECT: actions");
			printf("\x1b[12;1HSTART: exit");
		}
		break;
	case APP_MODE_SUMMARY:
		printf("\x1b[1;1HNo cards due now");
		printf("\x1b[3;1HB: deck list");
		printf("\x1b[5;1HSELECT: actions");
		printf("\x1b[7;1HSTART: exit");
		printf("\x1b[27;1HReviewed this session: %u", app->session.reviewed_count);
		break;
	case APP_MODE_ACTIONS:
		printf("\x1b[1;1HActions");
		printf("\x1b[3;1HA: confirm reset");
		printf("\x1b[5;1HB or SELECT: cancel");
		printf("\x1b[7;1HSTART: exit");
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

static bool reset_progress(struct app_state *app)
{
	if (app->active_state_path[0] == '\0')
	{
		app->state_message = "reset failed";
		return false;
	}

	errno = 0;
	if (remove(app->active_state_path) != 0 && errno != ENOENT)
	{
		app->state_message = "reset failed";
		return false;
	}

	app_load_selected_deck(app);
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
	if (keys_down & KEY_A)
	{
		if (!reset_progress(app))
			app->mode = APP_MODE_ACTIONS;
		return true;
	}

	if (keys_down & (KEY_B | KEY_SELECT))
	{
		app->mode = app->action_return_mode;
		return true;
	}

	return false;
}

static bool app_handle_input(struct app_state *app, u32 keys_down)
{
	if (app->mode == APP_MODE_DECK_SELECT)
		return app_handle_deck_select_input(app, keys_down);
	if (app->mode == APP_MODE_ACTIONS)
		return app_handle_actions_input(app, keys_down);

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

	if (app->mode != APP_MODE_REVIEW)
		return false;

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
			gfxFlushBuffers();

		gspWaitForVBlank();

		if (frame_dirty)
		{
			gfxSwapBuffers();
			frame_dirty = false;
		}

		hidScanInput();

		u32 keys_down = hidKeysDown();
		if (keys_down & KEY_START)
			break;

		if (app_handle_input(&app, keys_down))
		{
			draw_app(&app);
			frame_dirty = true;
		}
	}

	gfxExit();
	return 0;
}
