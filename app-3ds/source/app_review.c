#include "app_review.h"

#include <stdio.h>

bool app_review_should_show_queue(
	enum review_state_load_result state_load_result,
	const struct scheduler_session *session
)
{
	if (!review_state_load_result_allows_save(state_load_result))
		return false;
	if (session == NULL)
		return false;

	return !scheduler_is_complete(session);
}

void app_review_format_rating_status(
	char *destination,
	size_t destination_size,
	const char *rating_name,
	bool log_saved,
	bool queue_complete,
	bool same_card_due,
	size_t current_index,
	size_t card_count
)
{
	const char *name = rating_name != NULL ? rating_name : "Rating";

	if (destination == NULL || destination_size == 0)
		return;

	if (queue_complete)
	{
		snprintf(
			destination,
			destination_size,
			log_saved ? "%s saved; no cards due" : "%s saved; no due; log skipped",
			name
		);
		return;
	}

	if (same_card_due)
	{
		snprintf(
			destination,
			destination_size,
			log_saved ?
				"%s saved; same card due" :
				"%s saved; same due; log skipped",
			name
		);
		return;
	}

	snprintf(
		destination,
		destination_size,
		log_saved ?
			"%s saved; card %lu/%lu" :
			"%s saved; card %lu/%lu; log skipped",
		name,
		(unsigned long)(current_index + 1),
		(unsigned long)card_count
	);
}

void app_review_format_day_change_status(
	char *destination,
	size_t destination_size,
	enum review_state_load_result state_load_result,
	const struct scheduler_session *session
)
{
	if (destination == NULL || destination_size == 0)
		return;

	if (
		!review_state_load_result_allows_save(state_load_result) ||
		session == NULL
	)
	{
		snprintf(destination, destination_size, "Reset bad state first");
		return;
	}

	if (!scheduler_is_complete(session))
	{
		snprintf(destination, destination_size, "New day; cards due");
		return;
	}

	if (
		scheduler_new_limit_blocked_count(session) > 0 ||
		scheduler_review_limit_blocked_count(session) > 0
	)
	{
		snprintf(destination, destination_size, "New day; daily limit reached");
		return;
	}

	snprintf(destination, destination_size, "New day; no cards due");
}
