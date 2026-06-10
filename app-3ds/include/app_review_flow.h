#ifndef ANKI3DS_APP_REVIEW_FLOW_H
#define ANKI3DS_APP_REVIEW_FLOW_H

#include <stdbool.h>
#include <stddef.h>

#include "app_mode.h"
#include "study_review_log.h"

struct study_backend;
struct study_session;
struct study_settings;

bool app_review_flow_handle_input(
	unsigned int buttons,
	struct study_backend *backend,
	const struct study_settings *active_settings,
	struct study_settings *draft_settings,
	const char *active_settings_path,
	size_t *selected_setting_index,
	size_t max_scroll_offset,
	size_t *scroll_offset,
	bool *help_visible,
	enum app_mode *mode,
	enum app_mode *exit_return_mode,
	struct study_session *session,
	const char *active_deck_id,
	bool *session_dirty,
	bool *state_dirty,
	bool *log_dirty,
	enum study_review_log_event *log_event,
	enum study_review_log_rating *log_rating,
	bool *exit_requested,
	unsigned long timestamp,
	unsigned int day
);

#endif
