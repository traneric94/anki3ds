#ifndef ANKI3DS_STUDY_REVIEW_LOG_H
#define ANKI3DS_STUDY_REVIEW_LOG_H

#include <stdbool.h>
#include <stddef.h>

#define STUDY_REVIEW_LOG_MAX_BYTES 262144u

enum study_review_log_event
{
	STUDY_REVIEW_LOG_EVENT_RATING,
	STUDY_REVIEW_LOG_EVENT_UNDO,
	STUDY_REVIEW_LOG_EVENT_SUSPEND,
	STUDY_REVIEW_LOG_EVENT_RESTORE,
};

enum study_review_log_rating
{
	STUDY_REVIEW_LOG_RATING_NONE,
	STUDY_REVIEW_LOG_RATING_AGAIN,
	STUDY_REVIEW_LOG_RATING_HARD,
	STUDY_REVIEW_LOG_RATING_GOOD,
	STUDY_REVIEW_LOG_RATING_EASY,
};

struct study_review_log_entry
{
	unsigned long timestamp;
	enum study_review_log_event event;
	enum study_review_log_rating rating;
	size_t card_index;
	unsigned int reviewed_count;
	unsigned int suspended_count;
};

bool study_review_log_append(
	const char *path,
	const struct study_review_log_entry *entry
);
bool study_review_log_delete(const char *path);
const char *study_review_log_event_name(enum study_review_log_event event);
const char *study_review_log_rating_name(enum study_review_log_rating rating);

#endif
