#include "deck_summary.h"

#include <stdlib.h>

void deck_summary_init(struct deck_summary *summary)
{
	summary->deck_load_result = DECK_LOAD_NOT_FOUND;
	summary->deck_load_report.line_number = 0;
	summary->deck_load_report.parse_result = DECK_PARSE_OK;
	summary->settings_load_result = APP_SETTINGS_LOAD_NOT_FOUND;
	summary->settings_load_report.line_number = 0;
	summary->settings_load_report.parse_result = APP_SETTINGS_PARSE_OK;
	summary->state_load_result = REVIEW_STATE_LOAD_NOT_FOUND;
	summary->card_count = 0;
	summary->due_count = 0;
	summary->new_due_count = 0;
	summary->learning_due_count = 0;
	summary->review_due_count = 0;
	summary->suspended_count = 0;
}

void deck_summary_from_session(
	struct deck_summary *summary,
	enum deck_load_result deck_load_result,
	enum app_settings_load_result settings_load_result,
	enum review_state_load_result state_load_result,
	const struct scheduler_session *session
)
{
	deck_summary_init(summary);
	summary->deck_load_result = deck_load_result;
	summary->settings_load_result = settings_load_result;
	summary->state_load_result = state_load_result;

	if (deck_load_result != DECK_LOAD_OK || session == NULL)
		return;

	summary->card_count = session->card_count;
	if (!review_state_load_result_allows_save(state_load_result))
		return;

	summary->due_count = session->due_count;
	summary->new_due_count = scheduler_new_due_count(session);
	summary->learning_due_count = scheduler_learning_due_count(session);
	summary->review_due_count = scheduler_review_due_count(session);
	summary->suspended_count = scheduler_suspended_count(session);
}

void deck_summary_load(
	struct deck_summary *summary,
	const struct deck_entry *entry,
	unsigned int today
)
{
	struct app_settings settings;
	struct app_settings_load_report settings_load_report;
	struct deck_load_report deck_load_report;
	struct deck *deck;
	struct scheduler_session *session;
	enum deck_load_result deck_load_result;
	enum app_settings_load_result settings_load_result;
	enum review_state_load_result state_load_result;

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
	deck_load_result = deck_load_cards_with_report(
		deck,
		entry->cards_path,
		&deck_load_report
	);
	summary->deck_load_result = deck_load_result;
	summary->deck_load_report = deck_load_report;
	if (deck_load_result != DECK_LOAD_OK)
	{
		free(session);
		free(deck);
		return;
	}

	summary->card_count = deck->card_count;
	settings_load_result = app_settings_load_with_report(
		&settings,
		entry->settings_path,
		&settings_load_report
	);
	scheduler_init(session, deck->card_count, today);
	scheduler_set_daily_limits(session, settings.new_limit, settings.review_limit);
	state_load_result = review_state_load(deck, session, entry->state_path);
	deck_summary_from_session(
		summary,
		deck_load_result,
		settings_load_result,
		state_load_result,
		session
	);
	summary->deck_load_report = deck_load_report;
	summary->settings_load_report = settings_load_report;

	free(session);
	free(deck);
}
