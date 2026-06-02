#include <3ds.h>
#include <stdbool.h>
#include <stdio.h>

#include "deck.h"
#include "review_state.h"
#include "scheduler.h"

#define APP_VERSION "0.3.0-dev"
#define SAMPLE_DECK_PATH "sdmc:/3ds/anki3ds/decks/sample/cards.tsv"
#define SAMPLE_STATE_PATH "sdmc:/3ds/anki3ds/decks/sample/state.tsv"
#define TEXT_LEFT 1
#define TEXT_WIDTH 48

enum app_mode
{
	APP_MODE_LOAD_ERROR,
	APP_MODE_REVIEW,
	APP_MODE_SUMMARY,
};

struct app_state
{
	enum app_mode mode;
	bool revealed;
	enum deck_load_result load_result;
	enum review_state_load_result state_load_result;
	enum review_state_save_result state_save_result;
	const char *state_message;
	struct deck deck;
	struct scheduler_session session;
};

static void console_move(int row, int column)
{
	printf("\x1b[%d;%dH", row, column);
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

static const struct card *current_card(const struct app_state *app)
{
	if (!scheduler_has_current(&app->session))
		return NULL;

	return &app->deck.cards[scheduler_current_index(&app->session)];
}

static void app_init(struct app_state *app)
{
	deck_init(&app->deck, "sample");
	app->revealed = false;
	app->state_load_result = REVIEW_STATE_LOAD_NOT_FOUND;
	app->state_save_result = REVIEW_STATE_SAVE_OK;
	app->state_message = "State: not loaded";
	app->load_result = deck_load_cards(&app->deck, SAMPLE_DECK_PATH);

	if (app->load_result == DECK_LOAD_OK)
	{
		scheduler_init(&app->session, app->deck.card_count);
		app->state_load_result = review_state_load(&app->deck, &app->session, SAMPLE_STATE_PATH);
		app->state_message = review_state_load_result_name(app->state_load_result);

		if (scheduler_is_complete(&app->session))
			app->mode = APP_MODE_SUMMARY;
		else
			app->mode = APP_MODE_REVIEW;
	}
	else
	{
		app->mode = APP_MODE_LOAD_ERROR;
		scheduler_init(&app->session, 0);
	}
}

static void draw_header(const struct app_state *app)
{
	printf("\x1b[1;1Hanki3ds Review");
	printf(
		"\x1b[2;1HDeck: %s  Done: %lu/%lu",
		app->deck.name,
		(unsigned long)app->session.done_count,
		(unsigned long)app->session.card_count
	);
	printf("\x1b[3;1HState: %s", app->state_message);
}

static void draw_load_error_screen(const struct app_state *app)
{
	consoleClear();
	printf("\x1b[1;1Hanki3ds");
	printf("\x1b[3;1HCould not load deck.");
	printf("\x1b[5;1H%s", SAMPLE_DECK_PATH);
	printf("\x1b[7;1HResult: %s", deck_load_result_name(app->load_result));
	printf("\x1b[10;1HCopy cards.tsv to the path above.");
	printf("\x1b[28;1HSTART/M: exit");
}

static void draw_review_screen(const struct app_state *app)
{
	const struct card *card = current_card(app);

	consoleClear();
	draw_header(app);

	if (card == NULL)
	{
		printf("\x1b[5;1HNo due cards.");
		printf("\x1b[28;1HSTART/M: exit");
		return;
	}

	printf("\x1b[5;1HFront:");

	if (app->revealed)
	{
		draw_wrapped_text(card->front, 6, 7);
		printf("\x1b[14;1HBack:");
		draw_wrapped_text(card->back, 15, 9);
		printf("\x1b[27;1HRate: Y Again  X Hard  B Good  A Easy");
		printf("\x1b[28;1HSTART/M: exit");
	}
	else
	{
		draw_wrapped_text(card->front, 6, 17);
		printf("\x1b[27;1HA: show answer");
		printf("\x1b[28;1HSTART/M: exit");
	}
}

static void draw_summary_screen(const struct app_state *app)
{
	const struct scheduler_session *session = &app->session;

	consoleClear();
	printf("\x1b[1;1Hanki3ds Review");
	printf("\x1b[3;1HSession complete");
	printf("\x1b[5;1HCards:   %lu", (unsigned long)session->card_count);
	printf("\x1b[6;1HReviews: %u", review_count_total(session));
	printf("\x1b[7;1HState:   %s", app->state_message);
	printf(
		"\x1b[9;1HY Again: %u",
		session->rating_counts[SCHEDULER_RATING_AGAIN]
	);
	printf(
		"\x1b[10;1HX Hard:  %u",
		session->rating_counts[SCHEDULER_RATING_HARD]
	);
	printf(
		"\x1b[11;1HB Good:  %u",
		session->rating_counts[SCHEDULER_RATING_GOOD]
	);
	printf(
		"\x1b[12;1HA Easy:  %u",
		session->rating_counts[SCHEDULER_RATING_EASY]
	);
	printf("\x1b[28;1HSTART/M: exit");
}

static void draw_app(const struct app_state *app)
{
	switch (app->mode)
	{
	case APP_MODE_LOAD_ERROR:
		draw_load_error_screen(app);
		break;
	case APP_MODE_REVIEW:
		draw_review_screen(app);
		break;
	case APP_MODE_SUMMARY:
		draw_summary_screen(app);
		break;
	}
}

static bool rate_current_card(struct app_state *app, enum scheduler_rating rating)
{
	if (!app->revealed)
		return false;

	scheduler_rate_current(&app->session, rating);
	app->state_save_result = review_state_save(&app->deck, &app->session, SAMPLE_STATE_PATH);
	app->state_message = review_state_save_result_name(app->state_save_result);
	app->revealed = false;

	if (scheduler_is_complete(&app->session))
		app->mode = APP_MODE_SUMMARY;

	return true;
}

static bool app_handle_input(struct app_state *app, u32 keys_down)
{
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

	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	app_init(&app);
	draw_app(&app);

	while (aptMainLoop())
	{
		gspWaitForVBlank();
		gfxSwapBuffers();
		hidScanInput();

		u32 keys_down = hidKeysDown();
		if (keys_down & KEY_START)
			break;

		if (app_handle_input(&app, keys_down))
			draw_app(&app);
	}

	gfxExit();
	return 0;
}
