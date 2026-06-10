#include "app_confirm_flow.h"

#include "app_confirm_action.h"
#include "app_reset_action.h"
#include "app_status_text.h"
#include "study_backend.h"
#include "study_session.h"

#include <stdio.h>

static void app_confirm_flow_copy_string(
	char *destination,
	size_t destination_size,
	const char *source
)
{
	if (destination == NULL || destination_size == 0)
		return;

	snprintf(destination, destination_size, "%s", source != NULL ? source : "");
}

static bool app_confirm_flow_append_warning_context_to_status(
	struct study_backend *backend,
	const char *context
)
{
	char status_with_context[STUDY_BACKEND_STATUS_SIZE];
	bool context_appended;

	if (backend == NULL)
		return false;

	context_appended = app_status_text_copy_with_warning_context(
		status_with_context,
		sizeof(status_with_context),
		backend->status_text,
		context
	);
	if (context_appended)
		study_backend_set_status(backend, status_with_context);
	return context_appended;
}

static void app_confirm_flow_set_status_with_warning_context(
	struct study_backend *backend,
	const char *status
)
{
	char status_with_context[STUDY_BACKEND_STATUS_SIZE];

	if (backend == NULL)
		return;

	(void)app_status_text_copy_with_warning_context(
		status_with_context,
		sizeof(status_with_context),
		status,
		backend->status_text
	);
	study_backend_set_status(backend, status_with_context);
}

static const char *app_confirm_flow_event_deck_id(const char *deck_id)
{
	return deck_id != NULL && deck_id[0] != '\0' ? deck_id : "-";
}

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
)
{
	enum app_confirm_action_kind confirm_kind;
	enum app_confirm_action_result confirm_action;
	char warning_context[STUDY_BACKEND_STATUS_SIZE];

	if (state_dirty != NULL)
		*state_dirty = false;
	if (log_dirty != NULL)
		*log_dirty = false;
	if (log_event != NULL)
		*log_event = STUDY_REVIEW_LOG_EVENT_RATING;
	if (exit_requested != NULL)
		*exit_requested = false;
	if (review_scroll_reset != NULL)
		*review_scroll_reset = false;
	if (
		mode == NULL ||
		exit_return_mode == NULL ||
		backend == NULL ||
		!app_mode_is_confirm(*mode)
	)
	{
		return false;
	}

	confirm_kind = app_mode_confirm_action_kind(*mode);
	confirm_action = app_confirm_action_apply(confirm_kind, buttons);
	if (confirm_action == APP_CONFIRM_ACTION_NONE)
		return false;

	if (confirm_action == APP_CONFIRM_ACTION_OPEN_EXIT)
	{
		*exit_return_mode = *mode;
		*mode = APP_MODE_CONFIRM_EXIT;
		return true;
	}
	if (confirm_action == APP_CONFIRM_ACTION_CANCEL)
	{
		app_confirm_flow_set_status_with_warning_context(
			backend,
			app_confirm_action_cancel_status(confirm_kind)
		);
		*mode = *mode == APP_MODE_CONFIRM_EXIT ?
			*exit_return_mode :
			APP_MODE_REVIEW;
		return true;
	}

	app_confirm_flow_copy_string(
		warning_context,
		sizeof(warning_context),
		backend->status_text
	);
	if (*mode == APP_MODE_CONFIRM_EXIT)
	{
		study_session_record_event(
			session,
			STUDY_SESSION_EVENT_EXIT_CONFIRMED,
			app_confirm_flow_event_deck_id(active_deck_id),
			timestamp,
			day
		);
		if (session_dirty != NULL)
			*session_dirty = true;
		if (exit_requested != NULL)
			*exit_requested = true;
	}
	else if (*mode == APP_MODE_CONFIRM_RESET)
	{
		if (
			app_reset_action_confirm(
				backend,
				state_enabled,
				active_state_path,
				active_review_log_path,
				warning_context,
				battery_save_warning
			)
		)
		{
			study_session_record_event(
				session,
				STUDY_SESSION_EVENT_RESET_PROGRESS,
				active_deck_id,
				timestamp,
				day
			);
			if (session_dirty != NULL)
				*session_dirty = true;
		}
	}
	else if (*mode == APP_MODE_CONFIRM_RESTORE)
	{
		if (study_backend_restore_suspended(backend) > 0)
		{
			app_confirm_flow_append_warning_context_to_status(
				backend,
				warning_context
			);
			if (state_dirty != NULL)
				*state_dirty = true;
			if (log_dirty != NULL)
				*log_dirty = true;
			if (log_event != NULL)
				*log_event = STUDY_REVIEW_LOG_EVENT_RESTORE;
		}
	}
	else if (study_backend_suspend_current(backend))
	{
		app_confirm_flow_append_warning_context_to_status(
			backend,
			warning_context
		);
		if (state_dirty != NULL)
			*state_dirty = true;
		if (log_dirty != NULL)
			*log_dirty = true;
		if (log_event != NULL)
			*log_event = STUDY_REVIEW_LOG_EVENT_SUSPEND;
	}

	if (review_scroll_reset != NULL)
		*review_scroll_reset = true;
	*mode = APP_MODE_REVIEW;
	return true;
}
