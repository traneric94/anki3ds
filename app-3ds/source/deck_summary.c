#include "deck_summary.h"

#include "scheduler.h"

static size_t count_new_due_cards(const struct scheduler_session *session)
{
	size_t count = 0;

	for (size_t index = 0; index < session->card_count; index++)
	{
		if (
			scheduler_card_is_due(session, index) &&
			session->cards[index].review_count == 0
		)
		{
			count++;
		}
	}

	return count;
}

void deck_summary_init(struct deck_summary *summary)
{
	summary->deck_load_result = DECK_LOAD_NOT_FOUND;
	summary->settings_load_result = APP_SETTINGS_LOAD_NOT_FOUND;
	summary->state_load_result = REVIEW_STATE_LOAD_NOT_FOUND;
	summary->card_count = 0;
	summary->due_count = 0;
	summary->new_due_count = 0;
	summary->suspended_count = 0;
}

void deck_summary_load(
	struct deck_summary *summary,
	const struct deck_entry *entry,
	unsigned int today
)
{
	struct app_settings settings;
	struct deck deck;
	struct scheduler_session session;

	deck_summary_init(summary);

	if (entry == NULL)
		return;

	deck_init(&deck, entry->display_name);
	summary->deck_load_result = deck_load_cards(&deck, entry->cards_path);
	if (summary->deck_load_result != DECK_LOAD_OK)
		return;

	summary->card_count = deck.card_count;
	summary->settings_load_result = app_settings_load(&settings, entry->settings_path);
	scheduler_init(&session, deck.card_count, today);
	scheduler_set_daily_limits(&session, settings.new_limit, settings.review_limit);
	summary->state_load_result = review_state_load(&deck, &session, entry->state_path);
	summary->due_count = session.due_count;
	summary->new_due_count = count_new_due_cards(&session);
	summary->suspended_count = scheduler_suspended_count(&session);
}
