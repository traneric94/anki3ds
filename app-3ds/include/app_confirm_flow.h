#ifndef ANKI3DS_APP_CONFIRM_FLOW_H
#define ANKI3DS_APP_CONFIRM_FLOW_H

#include <stdbool.h>

#include "app_mode.h"
#include "study_review_log.h"

struct study_backend;
struct study_session;

bool app_confirm_flow_handle_input(
	enum app_mode *mode,
	enum app_mode *exit_return_mode,
	unsigned int buttons,
	struct study_backend *backend,
	bool state_enabled,
	const char *active_state_path,
	const char *active_review_log_path,
	struct study_session *session,
	const char *active_deck_id,
	bool *session_dirty,
	bool *state_dirty,
	bool *log_dirty,
	enum study_review_log_event *log_event,
	bool *exit_requested,
	bool *review_scroll_reset,
	bool battery_save_warning,
	unsigned long timestamp,
	unsigned int day
);

#endif
