#include "scheduler.h"

#include <string.h>

static size_t clamp_card_count(size_t card_count)
{
	if (card_count > DECK_MAX_CARDS)
		return DECK_MAX_CARDS;

	return card_count;
}

static bool scheduler_rating_is_valid(enum scheduler_rating rating)
{
	return rating >= 0 && rating < SCHEDULER_RATING_COUNT;
}

static void scheduler_advance(struct scheduler_session *session)
{
	if (scheduler_is_complete(session))
		return;

	for (size_t offset = 1; offset <= session->card_count; offset++)
	{
		size_t index = (session->current_index + offset) % session->card_count;

		if (!session->cards[index].done)
		{
			session->current_index = index;
			return;
		}
	}
}

void scheduler_init(struct scheduler_session *session, size_t card_count)
{
	memset(session, 0, sizeof(*session));
	session->card_count = clamp_card_count(card_count);
	session->current_index = 0;
}

bool scheduler_has_current(const struct scheduler_session *session)
{
	return session->card_count > 0 && !scheduler_is_complete(session);
}

bool scheduler_is_complete(const struct scheduler_session *session)
{
	return session->card_count == 0 || session->done_count >= session->card_count;
}

size_t scheduler_current_index(const struct scheduler_session *session)
{
	return session->current_index;
}

bool scheduler_restore_card(
	struct scheduler_session *session,
	size_t index,
	bool done,
	unsigned int review_count,
	enum scheduler_rating last_rating
)
{
	struct scheduler_card *card;

	if (index >= session->card_count)
		return false;
	if (!scheduler_rating_is_valid(last_rating))
		return false;

	card = &session->cards[index];

	if (done && !card->done)
		session->done_count++;
	else if (!done && card->done && session->done_count > 0)
		session->done_count--;

	card->done = done;
	card->review_count = review_count;
	card->last_rating = last_rating;
	return true;
}

void scheduler_reposition(struct scheduler_session *session)
{
	if (scheduler_is_complete(session))
		return;

	for (size_t index = 0; index < session->card_count; index++)
	{
		if (!session->cards[index].done)
		{
			session->current_index = index;
			return;
		}
	}
}

void scheduler_rate_current(struct scheduler_session *session, enum scheduler_rating rating)
{
	struct scheduler_card *card;

	if (!scheduler_has_current(session))
		return;
	if (!scheduler_rating_is_valid(rating))
		return;

	card = &session->cards[session->current_index];
	card->last_rating = rating;
	card->review_count++;
	session->rating_counts[rating]++;

	if (rating != SCHEDULER_RATING_AGAIN && !card->done)
	{
		card->done = true;
		session->done_count++;
	}

	scheduler_advance(session);
}

const char *scheduler_rating_name(enum scheduler_rating rating)
{
	switch (rating)
	{
	case SCHEDULER_RATING_AGAIN:
		return "Again";
	case SCHEDULER_RATING_HARD:
		return "Hard";
	case SCHEDULER_RATING_GOOD:
		return "Good";
	case SCHEDULER_RATING_EASY:
		return "Easy";
	case SCHEDULER_RATING_COUNT:
		break;
	}

	return "Unknown";
}
