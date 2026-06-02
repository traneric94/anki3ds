#ifndef ANKI3DS_DECK_SUMMARY_H
#define ANKI3DS_DECK_SUMMARY_H

#include <stddef.h>

#include "app_settings.h"
#include "deck.h"
#include "deck_index.h"
#include "review_state.h"
#include "scheduler.h"

struct deck_summary
{
	enum deck_load_result deck_load_result;
	enum app_settings_load_result settings_load_result;
	enum review_state_load_result state_load_result;
	size_t card_count;
	size_t due_count;
	size_t new_due_count;
	size_t learning_due_count;
	size_t review_due_count;
	size_t suspended_count;
};

void deck_summary_init(struct deck_summary *summary);
void deck_summary_from_session(
	struct deck_summary *summary,
	enum deck_load_result deck_load_result,
	enum app_settings_load_result settings_load_result,
	enum review_state_load_result state_load_result,
	const struct scheduler_session *session
);
void deck_summary_load(
	struct deck_summary *summary,
	const struct deck_entry *entry,
	unsigned int today
);

#endif
