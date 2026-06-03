#ifndef ANKI3DS_APP_REVIEW_H
#define ANKI3DS_APP_REVIEW_H

#include <stdbool.h>

#include "review_state.h"
#include "scheduler.h"

bool app_review_should_show_queue(
	enum review_state_load_result state_load_result,
	const struct scheduler_session *session
);

#endif
