#include "review_log.h"

#include <stddef.h>
#include <stdio.h>

static const char *review_log_rating_field(enum scheduler_rating rating)
{
	switch (rating)
	{
	case SCHEDULER_RATING_AGAIN:
		return "again";
	case SCHEDULER_RATING_HARD:
		return "hard";
	case SCHEDULER_RATING_GOOD:
		return "good";
	case SCHEDULER_RATING_EASY:
		return "easy";
	case SCHEDULER_RATING_COUNT:
		break;
	}

	return "-";
}

static bool review_log_rating_is_valid(enum scheduler_rating rating)
{
	return (
		rating == SCHEDULER_RATING_AGAIN ||
		rating == SCHEDULER_RATING_HARD ||
		rating == SCHEDULER_RATING_GOOD ||
		rating == SCHEDULER_RATING_EASY
	);
}

static bool review_log_card_id_is_valid(const char *card_id)
{
	if (card_id == NULL || card_id[0] == '\0')
		return false;

	for (size_t index = 0; card_id[index] != '\0'; index++)
	{
		if (
			card_id[index] == '\t' ||
			card_id[index] == '\n' ||
			card_id[index] == '\r'
		)
		{
			return false;
		}
	}

	return true;
}

const char *review_log_event_name(enum review_log_event event)
{
	switch (event)
	{
	case REVIEW_LOG_EVENT_RATING:
		return "rating";
	case REVIEW_LOG_EVENT_SUSPEND:
		return "suspend";
	case REVIEW_LOG_EVENT_UNDO:
		return "undo";
	case REVIEW_LOG_EVENT_RESTORE:
		return "restore";
	}

	return NULL;
}

bool review_log_append(const char *path, const struct review_log_entry *entry)
{
	FILE *file;
	const char *event_name;
	const char *rating;

	if (path == NULL || entry == NULL)
		return false;
	if (!review_log_card_id_is_valid(entry->card_id))
		return false;
	event_name = review_log_event_name(entry->event);
	if (event_name == NULL)
		return false;
	if (entry->event == REVIEW_LOG_EVENT_RATING)
	{
		if (!review_log_rating_is_valid(entry->rating))
			return false;
		rating = review_log_rating_field(entry->rating);
	}
	else
	{
		rating = "-";
	}

	file = fopen(path, "a");
	if (file == NULL)
		return false;

	if (fprintf(
		file,
		"%ld\t%u\t%s\t%s\t%s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n",
		(long)entry->timestamp,
		entry->day,
		event_name,
		entry->card_id,
		rating,
		entry->before.review_count,
		entry->before.due_day,
		entry->before.interval_days,
		entry->before.ease_permille,
		entry->before.lapses,
		entry->before.suspended ? 1u : 0u,
		entry->after.review_count,
		entry->after.due_day,
		entry->after.interval_days,
		entry->after.ease_permille,
		entry->after.lapses,
		entry->after.suspended ? 1u : 0u
	) < 0)
	{
		fclose(file);
		return false;
	}

	if (fclose(file) != 0)
		return false;

	return true;
}

bool review_log_delete(const char *path)
{
	FILE *file;

	if (path == NULL)
		return false;

	file = fopen(path, "rb");
	if (file == NULL)
		return true;

	fclose(file);
	return remove(path) == 0;
}
