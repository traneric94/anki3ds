#include "deck_summary.h"

#include "scheduler.h"

#include <stdlib.h>

static void count_due_card_types(
	struct deck_summary *summary,
	const struct scheduler_session *session
)
{
	for (size_t index = 0; index < session->card_count; index++)
	{
		const struct scheduler_card *card = &session->cards[index];

		if (!scheduler_card_is_due(session, index))
			continue;

		if (card->review_count == 0)
			summary->new_due_count++;
		else if (card->interval_days == 0)
			summary->learning_due_count++;
		else
			summary->review_due_count++;
	}
}

void deck_summary_init(struct deck_summary *summary)
{
	summary->deck_load_result = DECK_LOAD_NOT_FOUND;
	summary->settings_load_result = APP_SETTINGS_LOAD_NOT_FOUND;
	summary->state_load_result = REVIEW_STATE_LOAD_NOT_FOUND;
	summary->card_count = 0;
	summary->due_count = 0;
	summary->new_due_count = 0;
	summary->learning_due_count = 0;
	summary->review_due_count = 0;
	summary->suspended_count = 0;
}

void deck_summary_load(
	struct deck_summary *summary,
	const struct deck_entry *entry,
	unsigned int today
)
{
	struct app_settings settings;
	struct deck *deck;
	struct scheduler_session *session;

	deck_summary_init(summary);

	if (entry == NULL)
		return;

	deck = malloc(sizeof(*deck));
	session = malloc(sizeof(*session));
	if (deck == NULL || session == NULL)
	{
		free(session);
		free(deck);
		summary->deck_load_result = DECK_LOAD_OUT_OF_MEMORY;
		return;
	}

	deck_init(deck, entry->display_name);
	summary->deck_load_result = deck_load_cards(deck, entry->cards_path);
	if (summary->deck_load_result != DECK_LOAD_OK)
	{
		free(session);
		free(deck);
		return;
	}

	summary->card_count = deck->card_count;
	summary->settings_load_result = app_settings_load(&settings, entry->settings_path);
	scheduler_init(session, deck->card_count, today);
	scheduler_set_daily_limits(session, settings.new_limit, settings.review_limit);
	summary->state_load_result = review_state_load(deck, session, entry->state_path);
	summary->due_count = session->due_count;
	count_due_card_types(summary, session);
	summary->suspended_count = scheduler_suspended_count(session);

	free(session);
	free(deck);
}
