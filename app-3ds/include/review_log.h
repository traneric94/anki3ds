#ifndef ANKI3DS_REVIEW_LOG_H
#define ANKI3DS_REVIEW_LOG_H

#include <stdbool.h>
#include <time.h>

#include "scheduler.h"

#define REVIEW_LOG_MAX_BYTES 262144L

enum review_log_event
{
	REVIEW_LOG_EVENT_RATING,
	REVIEW_LOG_EVENT_SUSPEND,
	REVIEW_LOG_EVENT_UNDO,
	REVIEW_LOG_EVENT_RESTORE,
};

struct review_log_entry
{
	time_t timestamp;
	unsigned int day;
	enum review_log_event event;
	const char *card_id;
	enum scheduler_rating rating;
	struct scheduler_card before;
	struct scheduler_card after;
};

bool review_log_append(const char *path, const struct review_log_entry *entry);
bool review_log_delete(const char *path);
const char *review_log_event_name(enum review_log_event event);

#endif
