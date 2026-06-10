#include "app_settings_flow.h"

#include "app_settings_action.h"
#include "app_settings_save.h"
#include "app_status_text.h"
#include "study_backend.h"
#include "study_settings.h"

static void app_settings_flow_set_status_with_warning_context(
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

bool app_settings_flow_handle_input(
	unsigned int buttons,
	struct study_backend *backend,
	struct study_settings *active_settings,
	struct study_settings *draft_settings,
	const char *active_settings_path,
	size_t *selected_setting_index,
	struct study_session *session,
	const char *active_deck_id,
	bool *session_dirty,
	enum app_mode *mode,
	enum app_mode *exit_return_mode,
	bool battery_save_warning,
	unsigned long timestamp,
	unsigned int day
)
{
	enum app_settings_action_result settings_action;

	if (
		backend == NULL ||
		active_settings == NULL ||
		draft_settings == NULL ||
		selected_setting_index == NULL ||
		mode == NULL ||
		exit_return_mode == NULL
	)
	{
		return false;
	}

	settings_action = app_settings_action_apply(
		buttons,
		draft_settings,
		selected_setting_index
	);
	if (settings_action == APP_SETTINGS_ACTION_NONE)
		return false;

	switch (settings_action)
	{
	case APP_SETTINGS_ACTION_EXIT:
		*exit_return_mode = APP_MODE_SETTINGS;
		if (
			app_settings_action_has_unsaved_changes(
				active_settings,
				draft_settings
			)
		)
		{
			app_settings_flow_set_status_with_warning_context(
				backend,
				"Exit loses unsaved settings"
			);
		}
		*mode = APP_MODE_CONFIRM_EXIT;
		return true;
	case APP_SETTINGS_ACTION_CANCEL:
		app_settings_flow_set_status_with_warning_context(
			backend,
			app_settings_action_has_unsaved_changes(
				active_settings,
				draft_settings
			) ?
				"Settings canceled; discarded" :
				"Settings canceled"
		);
		*mode = APP_MODE_REVIEW;
		return true;
	case APP_SETTINGS_ACTION_UPDATED:
		app_settings_flow_set_status_with_warning_context(
			backend,
			app_settings_action_edit_status(active_settings, draft_settings)
		);
		return true;
	case APP_SETTINGS_ACTION_SAVE:
		if (
			!app_settings_save_apply(
				backend,
				active_settings,
				draft_settings,
				active_settings_path,
				session,
				active_deck_id,
				session_dirty,
				battery_save_warning,
				timestamp,
				day
			)
		)
		{
			*mode = APP_MODE_SETTINGS;
			return true;
		}
		*mode = APP_MODE_REVIEW;
		return true;
	case APP_SETTINGS_ACTION_NONE:
		break;
	}

	return false;
}
