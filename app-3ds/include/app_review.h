#ifndef ANKI3DS_APP_REVIEW_H
#define ANKI3DS_APP_REVIEW_H

#include <stdbool.h>
#include <stddef.h>

#include "review_state.h"
#include "scheduler.h"

bool app_review_should_show_queue(
	enum review_state_load_result state_load_result,
	const struct scheduler_session *session
);
void app_review_format_rating_status(
	char *destination,
	size_t destination_size,
	const char *rating_name,
	bool log_saved,
	bool queue_complete,
	bool same_card_due,
	size_t current_index,
	size_t card_count
);
void app_review_format_day_change_status(
	char *destination,
	size_t destination_size,
	enum review_state_load_result state_load_result,
	const struct scheduler_session *session
);

#endif
