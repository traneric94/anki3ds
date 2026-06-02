#ifndef ANKI3DS_SCHEDULER_H
#define ANKI3DS_SCHEDULER_H

#include <stdbool.h>
#include <stddef.h>

#include "deck.h"

enum scheduler_rating
{
	SCHEDULER_RATING_AGAIN,
	SCHEDULER_RATING_HARD,
	SCHEDULER_RATING_GOOD,
	SCHEDULER_RATING_EASY,
	SCHEDULER_RATING_COUNT,
};

struct scheduler_card
{
	enum scheduler_rating last_rating;
	unsigned int review_count;
	bool done;
};

struct scheduler_session
{
	size_t card_count;
	size_t current_index;
	size_t done_count;
	unsigned int rating_counts[SCHEDULER_RATING_COUNT];
	struct scheduler_card cards[DECK_MAX_CARDS];
};

void scheduler_init(struct scheduler_session *session, size_t card_count);
bool scheduler_has_current(const struct scheduler_session *session);
bool scheduler_is_complete(const struct scheduler_session *session);
size_t scheduler_current_index(const struct scheduler_session *session);
void scheduler_rate_current(struct scheduler_session *session, enum scheduler_rating rating);
const char *scheduler_rating_name(enum scheduler_rating rating);

#endif
