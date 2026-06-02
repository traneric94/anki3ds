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

static unsigned int clamp_unsigned(
	unsigned int value,
	unsigned int minimum,
	unsigned int maximum
)
{
	if (value < minimum)
		return minimum;
	if (value > maximum)
		return maximum;

	return value;
}

static unsigned int clamp_ease(unsigned int ease_permille)
{
	return clamp_unsigned(
		ease_permille,
		SCHEDULER_MIN_EASE_PERMILLE,
		SCHEDULER_MAX_EASE_PERMILLE
	);
}

static unsigned int adjusted_ease(unsigned int ease_permille, int delta)
{
	int adjusted = (int)ease_permille + delta;

	if (adjusted < (int)SCHEDULER_MIN_EASE_PERMILLE)
		return SCHEDULER_MIN_EASE_PERMILLE;
	if (adjusted > (int)SCHEDULER_MAX_EASE_PERMILLE)
		return SCHEDULER_MAX_EASE_PERMILLE;

	return (unsigned int)adjusted;
}

static unsigned int add_days(unsigned int today, unsigned int interval_days)
{
	if (today > SCHEDULER_MAX_DAY - interval_days)
		return SCHEDULER_MAX_DAY;

	return today + interval_days;
}

static unsigned int multiply_interval(
	unsigned int interval_days,
	unsigned int multiplier_permille
)
{
	unsigned long scaled;

	if (interval_days == 0)
		interval_days = 1;

	scaled = ((unsigned long)interval_days * multiplier_permille + 500) / 1000;
	if (scaled < 1)
		return 1;
	if (scaled > SCHEDULER_MAX_INTERVAL_DAYS)
		return SCHEDULER_MAX_INTERVAL_DAYS;

	return (unsigned int)scaled;
}

static unsigned int scheduler_remaining_limit(unsigned int count, unsigned int limit)
{
	if (limit == 0)
		return DECK_MAX_CARDS;
	if (count >= limit)
		return 0;

	return limit - count;
}

static bool scheduler_card_calendar_due(const struct scheduler_session *session, size_t index)
{
	if (index >= session->card_count)
		return false;
	if (session->cards[index].suspended)
		return false;

	return session->cards[index].due_day <= session->today;
}

static bool scheduler_card_started_new_today(
	const struct scheduler_session *session,
	const struct scheduler_card *card
)
{
	return card->first_review_day != 0 && card->first_review_day == session->today;
}

static bool scheduler_card_started_review_today(
	const struct scheduler_session *session,
	const struct scheduler_card *card
)
{
	return (
		card->last_review_day != 0 &&
		card->last_review_day == session->today &&
		!scheduler_card_started_new_today(session, card)
	);
}

static bool scheduler_card_is_unstarted_new(
	const struct scheduler_session *session,
	size_t index
)
{
	return (
		scheduler_card_calendar_due(session, index) &&
		session->cards[index].review_count == 0
	);
}

static bool scheduler_card_is_unstarted_review(
	const struct scheduler_session *session,
	size_t index
)
{
	const struct scheduler_card *card;

	if (!scheduler_card_calendar_due(session, index))
		return false;

	card = &session->cards[index];
	return (
		card->review_count > 0 &&
		!scheduler_card_started_new_today(session, card) &&
		!scheduler_card_started_review_today(session, card)
	);
}

static unsigned int scheduler_count_unstarted_due_before(
	const struct scheduler_session *session,
	size_t index,
	bool count_new
)
{
	unsigned int count = 0;

	for (size_t due_index = 0; due_index < index; due_index++)
	{
		bool due = count_new ?
			scheduler_card_is_unstarted_new(session, due_index) :
			scheduler_card_is_unstarted_review(session, due_index);

		if (due)
			count++;
	}

	return count;
}

static void scheduler_recount_daily(struct scheduler_session *session)
{
	session->new_count_today = 0;
	session->review_count_today = 0;

	for (size_t index = 0; index < session->card_count; index++)
	{
		const struct scheduler_card *card = &session->cards[index];

		if (scheduler_card_started_new_today(session, card))
			session->new_count_today++;
		else if (scheduler_card_started_review_today(session, card))
			session->review_count_today++;
	}
}

static void scheduler_recount_due(struct scheduler_session *session)
{
	session->due_count = 0;

	for (size_t index = 0; index < session->card_count; index++)
	{
		if (scheduler_card_is_due(session, index))
			session->due_count++;
	}
}

static void scheduler_recount(struct scheduler_session *session)
{
	scheduler_recount_daily(session);
	scheduler_recount_due(session);
}

static void scheduler_advance(struct scheduler_session *session)
{
	if (scheduler_is_complete(session))
		return;

	for (size_t offset = 1; offset <= session->card_count; offset++)
	{
		size_t index = (session->current_index + offset) % session->card_count;

		if (scheduler_card_is_due(session, index))
		{
			session->current_index = index;
			return;
		}
	}
}

void scheduler_init(struct scheduler_session *session, size_t card_count, unsigned int today)
{
	memset(session, 0, sizeof(*session));
	session->card_count = clamp_card_count(card_count);
	session->current_index = 0;
	session->today = today;

	for (size_t index = 0; index < session->card_count; index++)
	{
		session->cards[index].last_rating = SCHEDULER_RATING_GOOD;
		session->cards[index].due_day = today;
		session->cards[index].ease_permille = SCHEDULER_DEFAULT_EASE_PERMILLE;
	}

	scheduler_recount(session);
}

bool scheduler_has_current(const struct scheduler_session *session)
{
	return session->card_count > 0 && session->due_count > 0;
}

bool scheduler_is_complete(const struct scheduler_session *session)
{
	return session->due_count == 0;
}

size_t scheduler_current_index(const struct scheduler_session *session)
{
	return session->current_index;
}

size_t scheduler_suspended_count(const struct scheduler_session *session)
{
	size_t count = 0;

	for (size_t index = 0; index < session->card_count; index++)
	{
		if (session->cards[index].suspended)
			count++;
	}

	return count;
}

bool scheduler_card_is_due(const struct scheduler_session *session, size_t index)
{
	unsigned int remaining;

	if (!scheduler_card_calendar_due(session, index))
		return false;

	if (session->cards[index].review_count == 0)
	{
		remaining = scheduler_remaining_limit(
			session->new_count_today,
			session->new_limit
		);
		return scheduler_count_unstarted_due_before(session, index, true) < remaining;
	}

	if (scheduler_card_started_new_today(session, &session->cards[index]))
		return true;
	if (scheduler_card_started_review_today(session, &session->cards[index]))
		return true;

	remaining = scheduler_remaining_limit(
		session->review_count_today,
		session->review_limit
	);
	return scheduler_count_unstarted_due_before(session, index, false) < remaining;
}

bool scheduler_restore_card(
	struct scheduler_session *session,
	size_t index,
	unsigned int review_count,
	enum scheduler_rating last_rating,
	unsigned int due_day,
	unsigned int interval_days,
	unsigned int ease_permille,
	unsigned int lapses,
	bool suspended,
	unsigned int first_review_day,
	unsigned int last_review_day
)
{
	struct scheduler_card *card;

	if (index >= session->card_count)
		return false;
	if (!scheduler_rating_is_valid(last_rating))
		return false;
	if (due_day > SCHEDULER_MAX_DAY)
		return false;
	if (first_review_day > SCHEDULER_MAX_DAY)
		return false;
	if (last_review_day > SCHEDULER_MAX_DAY)
		return false;
	if (interval_days > SCHEDULER_MAX_INTERVAL_DAYS)
		return false;
	if (
		ease_permille < SCHEDULER_MIN_EASE_PERMILLE ||
		ease_permille > SCHEDULER_MAX_EASE_PERMILLE
	)
	{
		return false;
	}

	card = &session->cards[index];
	card->review_count = review_count;
	card->last_rating = last_rating;
	card->first_review_day = first_review_day;
	card->last_review_day = last_review_day;
	card->due_day = due_day;
	card->interval_days = interval_days;
	card->ease_permille = ease_permille;
	card->lapses = lapses;
	card->suspended = suspended;
	session->undo.available = false;
	session->undo.kind = SCHEDULER_UNDO_NONE;
	scheduler_recount(session);
	return true;
}

void scheduler_set_daily_limits(
	struct scheduler_session *session,
	unsigned int new_limit,
	unsigned int review_limit
)
{
	session->new_limit = new_limit;
	session->review_limit = review_limit;
	scheduler_recount(session);
	scheduler_reposition(session);
}

void scheduler_reposition(struct scheduler_session *session)
{
	if (scheduler_is_complete(session))
		return;

	for (size_t index = 0; index < session->card_count; index++)
	{
		if (scheduler_card_is_due(session, index))
		{
			session->current_index = index;
			return;
		}
	}
}

static void schedule_new_card(
	struct scheduler_session *session,
	struct scheduler_card *card,
	enum scheduler_rating rating
)
{
	switch (rating)
	{
	case SCHEDULER_RATING_AGAIN:
		card->interval_days = 0;
		card->due_day = session->today;
		card->ease_permille = adjusted_ease(card->ease_permille, -200);
		break;
	case SCHEDULER_RATING_HARD:
		card->interval_days = 1;
		card->due_day = add_days(session->today, card->interval_days);
		card->ease_permille = adjusted_ease(card->ease_permille, -150);
		break;
	case SCHEDULER_RATING_GOOD:
		card->interval_days = 1;
		card->due_day = add_days(session->today, card->interval_days);
		break;
	case SCHEDULER_RATING_EASY:
		card->interval_days = 4;
		card->due_day = add_days(session->today, card->interval_days);
		card->ease_permille = adjusted_ease(card->ease_permille, 150);
		break;
	case SCHEDULER_RATING_COUNT:
		break;
	}
}

static void schedule_review_card(
	struct scheduler_session *session,
	struct scheduler_card *card,
	enum scheduler_rating rating
)
{
	switch (rating)
	{
	case SCHEDULER_RATING_AGAIN:
		card->interval_days = 0;
		card->due_day = session->today;
		card->ease_permille = adjusted_ease(card->ease_permille, -200);
		card->lapses++;
		break;
	case SCHEDULER_RATING_HARD:
		card->interval_days = multiply_interval(card->interval_days, 1200);
		card->due_day = add_days(session->today, card->interval_days);
		card->ease_permille = adjusted_ease(card->ease_permille, -150);
		break;
	case SCHEDULER_RATING_GOOD:
		card->interval_days = multiply_interval(card->interval_days, card->ease_permille);
		card->due_day = add_days(session->today, card->interval_days);
		break;
	case SCHEDULER_RATING_EASY:
		card->interval_days = multiply_interval(card->interval_days, card->ease_permille + 300);
		card->due_day = add_days(session->today, card->interval_days);
		card->ease_permille = adjusted_ease(card->ease_permille, 150);
		break;
	case SCHEDULER_RATING_COUNT:
		break;
	}
}

static bool scheduler_card_is_in_initial_learning(const struct scheduler_card *card)
{
	return card->interval_days == 0 && card->lapses == 0;
}

static void scheduler_save_undo(
	struct scheduler_session *session,
	enum scheduler_undo_kind kind,
	enum scheduler_rating rating
)
{
	size_t index = session->current_index;

	session->undo.available = true;
	session->undo.kind = kind;
	session->undo.card_index = index;
	session->undo.current_index = session->current_index;
	session->undo.due_count = session->due_count;
	session->undo.reviewed_count = session->reviewed_count;
	session->undo.new_count_today = session->new_count_today;
	session->undo.review_count_today = session->review_count_today;
	session->undo.rating = rating;
	session->undo.card = session->cards[index];
}

void scheduler_rate_current(struct scheduler_session *session, enum scheduler_rating rating)
{
	struct scheduler_card *card;
	bool use_initial_schedule;

	if (!scheduler_has_current(session))
		return;
	if (!scheduler_rating_is_valid(rating))
		return;

	scheduler_save_undo(session, SCHEDULER_UNDO_RATING, rating);
	card = &session->cards[session->current_index];

	use_initial_schedule = scheduler_card_is_in_initial_learning(card);
	card->last_rating = rating;
	if (card->review_count == 0 && card->first_review_day == 0)
		card->first_review_day = session->today;
	card->last_review_day = session->today;
	session->rating_counts[rating]++;
	session->reviewed_count++;

	if (use_initial_schedule)
		schedule_new_card(session, card, rating);
	else
		schedule_review_card(session, card, rating);

	card->review_count++;
	card->ease_permille = clamp_ease(card->ease_permille);
	scheduler_recount(session);

	scheduler_advance(session);
}

bool scheduler_suspend_current(struct scheduler_session *session)
{
	struct scheduler_card *card;

	if (!scheduler_has_current(session))
		return false;

	scheduler_save_undo(session, SCHEDULER_UNDO_SUSPEND, SCHEDULER_RATING_COUNT);
	card = &session->cards[session->current_index];
	card->suspended = true;
	scheduler_recount(session);
	scheduler_advance(session);
	return true;
}

unsigned int scheduler_unsuspend_all(struct scheduler_session *session)
{
	unsigned int unsuspended_count = 0;

	for (size_t index = 0; index < session->card_count; index++)
	{
		if (session->cards[index].suspended)
		{
			session->cards[index].suspended = false;
			unsuspended_count++;
		}
	}

	if (unsuspended_count == 0)
		return 0;

	session->undo.available = false;
	session->undo.kind = SCHEDULER_UNDO_NONE;
	scheduler_recount(session);
	scheduler_reposition(session);
	return unsuspended_count;
}

bool scheduler_undo_last(struct scheduler_session *session)
{
	if (!session->undo.available)
		return false;
	if (session->undo.card_index >= session->card_count)
	{
		session->undo.available = false;
		session->undo.kind = SCHEDULER_UNDO_NONE;
		return false;
	}

	session->cards[session->undo.card_index] = session->undo.card;
	session->current_index = session->undo.current_index;
	session->due_count = session->undo.due_count;
	session->reviewed_count = session->undo.reviewed_count;
	session->new_count_today = session->undo.new_count_today;
	session->review_count_today = session->undo.review_count_today;
	if (
		session->undo.kind == SCHEDULER_UNDO_RATING &&
		scheduler_rating_is_valid(session->undo.rating) &&
		session->rating_counts[session->undo.rating] > 0
	)
	{
		session->rating_counts[session->undo.rating]--;
	}

	session->undo.available = false;
	session->undo.kind = SCHEDULER_UNDO_NONE;
	return true;
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
