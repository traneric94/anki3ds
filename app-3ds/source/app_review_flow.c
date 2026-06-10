#include "app_review_flow.h"

#include "app_review_action.h"
#include "app_settings_action.h"
#include "app_status_text.h"
#include "study_backend.h"
#include "study_controls.h"
#include "study_session.h"
#include "study_settings.h"

#include <stdio.h>

static void app_review_flow_copy_string(
	char *destination,
	size_t destination_size,
	const char *source
)
{
	if (destination == NULL || destination_size == 0)
		return;

	snprintf(destination, destination_size, "%s", source != NULL ? source : "");
}

static bool app_review_flow_append_warning_context_to_status(
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

static void app_review_flow_set_status_with_warning_context(
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

static void app_review_flow_safe_settings(
	struct study_settings *settings,
	const struct study_settings *source
)
{
	if (settings == NULL)
		return;

	if (source != NULL)
		*settings = *source;
	else
		study_settings_defaults(settings);
}

static bool app_review_flow_open_settings(
	struct study_backend *backend,
	const struct study_settings *active_settings,
	struct study_settings *draft_settings,
	const char *active_settings_path,
	size_t *selected_setting_index,
	enum app_mode *mode
)
{
	if (backend == NULL || mode == NULL)
		return false;

	if (
		active_settings_path == NULL ||
		active_settings_path[0] == '\0' ||
		active_settings == NULL ||
		draft_settings == NULL ||
		selected_setting_index == NULL
	)
	{
		app_review_flow_set_status_with_warning_context(
			backend,
			"Settings unavailable"
		);
		return true;
	}

	*draft_settings = *active_settings;
	*selected_setting_index = 0;
	app_review_flow_set_status_with_warning_context(
		backend,
		app_settings_action_edit_status(active_settings, draft_settings)
	);
	*mode = APP_MODE_SETTINGS;
	return true;
}

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
)
{
	struct study_backend_view view;
	struct study_settings settings;
	enum study_control_rating control_rating;
	enum study_control_action action;
	char warning_context[STUDY_BACKEND_STATUS_SIZE];

	if (state_dirty != NULL)
		*state_dirty = false;
	if (log_dirty != NULL)
		*log_dirty = false;
	if (log_event != NULL)
		*log_event = STUDY_REVIEW_LOG_EVENT_RATING;
	if (log_rating != NULL)
		*log_rating = STUDY_REVIEW_LOG_RATING_NONE;
	if (exit_requested != NULL)
		*exit_requested = false;
	if (
		backend == NULL ||
		scroll_offset == NULL ||
		mode == NULL ||
		exit_return_mode == NULL ||
		exit_requested == NULL
	)
	{
		return false;
	}

	study_backend_build_view(backend, &view);
	app_review_flow_safe_settings(&settings, active_settings);

	if (buttons == STUDY_CONTROL_BUTTON_START)
	{
		if (help_visible != NULL)
			*help_visible = !*help_visible;
		return true;
	}
	if (
		buttons == STUDY_CONTROL_BUTTON_X &&
		!view.answer_visible &&
		view.card_count > 0
	)
	{
		return app_review_flow_open_settings(
			backend,
			active_settings,
			draft_settings,
			active_settings_path,
			selected_setting_index,
			mode
		);
	}
	if (
		buttons == STUDY_CONTROL_BUTTON_A &&
		!view.has_active_card &&
		view.card_count > 0
	)
	{
		app_review_flow_copy_string(
			warning_context,
			sizeof(warning_context),
			backend->status_text
		);
		if (study_backend_review_again(backend) && state_dirty != NULL)
			*state_dirty = true;
		app_review_flow_append_warning_context_to_status(
			backend,
			warning_context
		);
		return true;
	}

	action = study_controls_interpret(
		buttons,
		view.answer_visible,
		view.undo_available,
		&control_rating
	);
	if (buttons == STUDY_CONTROL_BUTTON_B)
	{
		switch (app_review_action_undo_target(&view))
		{
		case APP_REVIEW_UNDO_UNAVAILABLE:
			app_review_flow_set_status_with_warning_context(
				backend,
				"Nothing to undo"
			);
			return true;
		case APP_REVIEW_UNDO_APPLY:
		case APP_REVIEW_UNDO_NONE:
			break;
		}
	}
	if (
		buttons == STUDY_CONTROL_BUTTON_Y &&
		!view.answer_visible &&
		view.card_count > 0
	)
	{
		*mode = APP_MODE_CONFIRM_RESET;
		return true;
	}
	if (action == STUDY_CONTROL_ACTION_DECKS)
	{
		*mode = APP_MODE_DECK_SELECT;
		*scroll_offset = 0;
		return true;
	}
	if (action == STUDY_CONTROL_ACTION_TOGGLE_HELP)
	{
		if (help_visible != NULL)
			*help_visible = !*help_visible;
		return true;
	}
	if (action == STUDY_CONTROL_ACTION_EXIT)
	{
		*exit_return_mode = APP_MODE_REVIEW;
		*mode = APP_MODE_CONFIRM_EXIT;
		return true;
	}
	if (action == STUDY_CONTROL_ACTION_SUSPEND_RESTORE)
	{
		switch (app_review_action_suspend_restore_target(&view))
		{
		case APP_REVIEW_SUSPEND_RESTORE_SUSPEND:
			*mode = APP_MODE_CONFIRM_SUSPEND;
			return true;
		case APP_REVIEW_SUSPEND_RESTORE_RESTORE:
			*mode = APP_MODE_CONFIRM_RESTORE;
			return true;
		case APP_REVIEW_SUSPEND_RESTORE_UNAVAILABLE:
			app_review_flow_set_status_with_warning_context(
				backend,
				"Nothing suspended"
			);
			return true;
		case APP_REVIEW_SUSPEND_RESTORE_NONE:
			return false;
		}
	}
	if (
		action == STUDY_CONTROL_ACTION_RATE &&
		study_settings_review_limit_blocks_rating(
			settings.review_limit,
			view.reviewed_today_count
		)
	)
	{
		app_review_flow_set_status_with_warning_context(
			backend,
			"Review limit reached"
		);
		return true;
	}
	if (
		action == STUDY_CONTROL_ACTION_REVEAL &&
		study_settings_new_limit_blocks_reveal(
			settings.new_limit,
			view.introduced_today_count,
			view.current_card_introduced
		)
	)
	{
		app_review_flow_set_status_with_warning_context(
			backend,
			"New limit reached"
		);
		return true;
	}

	app_review_flow_copy_string(
		warning_context,
		sizeof(warning_context),
		backend->status_text
	);
	if (
		!app_review_action_apply(
			backend,
			action,
			control_rating,
			max_scroll_offset,
			scroll_offset,
			state_dirty,
			log_dirty,
			log_event,
			log_rating,
			exit_requested
		)
	)
	{
		return false;
	}

	app_review_flow_append_warning_context_to_status(backend, warning_context);
	if (action == STUDY_CONTROL_ACTION_REVEAL)
	{
		if (help_visible != NULL)
			*help_visible = false;
		study_session_record_event(
			session,
			STUDY_SESSION_EVENT_ANSWER_SHOWN,
			active_deck_id,
			timestamp,
			day
		);
		if (session_dirty != NULL)
			*session_dirty = true;
	}
	return true;
}
