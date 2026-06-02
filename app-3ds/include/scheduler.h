#ifndef ANKI3DS_SCHEDULER_H
#define ANKI3DS_SCHEDULER_H

#include <stdbool.h>
#include <stddef.h>

#include "deck.h"

#define SCHEDULER_DEFAULT_EASE_PERMILLE 2500
#define SCHEDULER_MIN_EASE_PERMILLE 1300
#define SCHEDULER_MAX_EASE_PERMILLE 3500
#define SCHEDULER_MAX_INTERVAL_DAYS 36500
#define SCHEDULER_MAX_DAY 1000000

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
	unsigned int due_day;
	unsigned int interval_days;
	unsigned int ease_permille;
	unsigned int lapses;
};

struct scheduler_session
{
	size_t card_count;
	size_t current_index;
	unsigned int today;
	size_t due_count;
	unsigned int reviewed_count;
	unsigned int rating_counts[SCHEDULER_RATING_COUNT];
	struct scheduler_card cards[DECK_MAX_CARDS];
};

void scheduler_init(struct scheduler_session *session, size_t card_count, unsigned int today);
bool scheduler_has_current(const struct scheduler_session *session);
bool scheduler_is_complete(const struct scheduler_session *session);
size_t scheduler_current_index(const struct scheduler_session *session);
bool scheduler_card_is_due(const struct scheduler_session *session, size_t index);
bool scheduler_restore_card(
	struct scheduler_session *session,
	size_t index,
	unsigned int review_count,
	enum scheduler_rating last_rating,
	unsigned int due_day,
	unsigned int interval_days,
	unsigned int ease_permille,
	unsigned int lapses
);
void scheduler_reposition(struct scheduler_session *session);
void scheduler_rate_current(struct scheduler_session *session, enum scheduler_rating rating);
const char *scheduler_rating_name(enum scheduler_rating rating);

#endif
