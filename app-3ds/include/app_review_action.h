#ifndef ANKI3DS_APP_REVIEW_ACTION_H
#define ANKI3DS_APP_REVIEW_ACTION_H

#include <stdbool.h>
#include <stddef.h>

#include "study_backend.h"
#include "study_controls.h"
#include "study_review_log.h"

enum app_review_suspend_restore_target
{
	APP_REVIEW_SUSPEND_RESTORE_NONE,
	APP_REVIEW_SUSPEND_RESTORE_SUSPEND,
	APP_REVIEW_SUSPEND_RESTORE_RESTORE,
	APP_REVIEW_SUSPEND_RESTORE_UNAVAILABLE,
};

enum app_review_undo_target
{
	APP_REVIEW_UNDO_NONE,
	APP_REVIEW_UNDO_APPLY,
	APP_REVIEW_UNDO_UNAVAILABLE,
};

enum app_review_undo_target app_review_action_undo_target(
	const struct study_backend_view *view
);
enum app_review_suspend_restore_target app_review_action_suspend_restore_target(
	const struct study_backend_view *view
);
bool app_review_action_apply(
	struct study_backend *backend,
	enum study_control_action action,
	enum study_control_rating control_rating,
	size_t max_scroll_offset,
	size_t *scroll_offset,
	bool *state_dirty,
	bool *log_dirty,
	enum study_review_log_event *log_event,
	enum study_review_log_rating *log_rating,
	bool *exit_requested
);

#endif
