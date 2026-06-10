#ifndef ANKI3DS_APP_DAY_ROLLOVER_FLOW_H
#define ANKI3DS_APP_DAY_ROLLOVER_FLOW_H

#include <stdbool.h>

#include "app_state_save.h"

struct study_backend;
struct study_session;

bool app_day_rollover_flow_apply(
	struct study_backend *backend,
	bool state_enabled,
	const char *state_path,
	const char *review_log_path,
	struct study_session *session,
	const char *deck_id,
	bool *session_dirty,
	unsigned int *current_day,
	unsigned long timestamp,
	unsigned int observed_day,
	bool battery_save_warning
);
bool app_day_rollover_flow_apply_with_outcome(
	struct study_backend *backend,
	bool state_enabled,
	const char *state_path,
	const char *review_log_path,
	struct study_session *session,
	const char *deck_id,
	bool *session_dirty,
	unsigned int *current_day,
	unsigned long timestamp,
	unsigned int observed_day,
	bool battery_save_warning,
	enum app_state_save_outcome *save_outcome
);

#endif
