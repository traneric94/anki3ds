#include "app_settings_action.h"

#include "study_controls.h"

#define APP_SETTINGS_ACTION_FIELD_COUNT 3u

static bool app_settings_action_has_single_button(unsigned int buttons)
{
	return buttons != 0 && (buttons & (buttons - 1)) == 0;
}

static void app_settings_action_normalize_settings(
	struct study_settings *destination,
	const struct study_settings *source
)
{
	if (destination == NULL)
		return;

	if (source != NULL)
	{
		*destination = *source;
		return;
	}

	study_settings_defaults(destination);
}

static size_t app_settings_action_normalize_selected_index(size_t selected_index)
{
	return selected_index < APP_SETTINGS_ACTION_FIELD_COUNT ?
		selected_index :
		0;
}

enum app_settings_action_result app_settings_action_apply(
	unsigned int buttons,
	struct study_settings *draft_settings,
	size_t *selected_index
)
{
	unsigned int *selected_limit;

	if (draft_settings == NULL || selected_index == NULL)
		return APP_SETTINGS_ACTION_NONE;
	if (!app_settings_action_has_single_button(buttons))
		return APP_SETTINGS_ACTION_NONE;

	switch (buttons)
	{
	case STUDY_CONTROL_BUTTON_START:
		return APP_SETTINGS_ACTION_EXIT;
	case STUDY_CONTROL_BUTTON_B:
	case STUDY_CONTROL_BUTTON_SELECT:
		return APP_SETTINGS_ACTION_CANCEL;
	case STUDY_CONTROL_BUTTON_X:
		return APP_SETTINGS_ACTION_SAVE;
	case STUDY_CONTROL_BUTTON_UP:
		*selected_index = app_settings_action_normalize_selected_index(
			*selected_index
		);
		if (*selected_index == 0)
			*selected_index = APP_SETTINGS_ACTION_FIELD_COUNT - 1;
		else
			(*selected_index)--;
		return APP_SETTINGS_ACTION_UPDATED;
	case STUDY_CONTROL_BUTTON_DOWN:
		*selected_index = app_settings_action_normalize_selected_index(
			*selected_index
		);
		*selected_index = (*selected_index + 1) % APP_SETTINGS_ACTION_FIELD_COUNT;
		return APP_SETTINGS_ACTION_UPDATED;
	case STUDY_CONTROL_BUTTON_LEFT:
	case STUDY_CONTROL_BUTTON_RIGHT:
		*selected_index = app_settings_action_normalize_selected_index(
			*selected_index
		);
		if (*selected_index == 2)
		{
			draft_settings->learning_mode = study_settings_next_learning_mode(
				draft_settings->learning_mode
			);
		}
		else
		{
			selected_limit = *selected_index == 0 ?
				&draft_settings->new_limit :
				&draft_settings->review_limit;
			*selected_limit = study_settings_next_limit_preset(
				*selected_limit,
				buttons == STUDY_CONTROL_BUTTON_RIGHT
			);
		}
		return APP_SETTINGS_ACTION_UPDATED;
	}

	return APP_SETTINGS_ACTION_NONE;
}

bool app_settings_action_has_unsaved_changes(
	const struct study_settings *active_settings,
	const struct study_settings *draft_settings
)
{
	struct study_settings active;
	struct study_settings draft;

	app_settings_action_normalize_settings(&active, active_settings);
	app_settings_action_normalize_settings(&draft, draft_settings);
	return (
		active.new_limit != draft.new_limit ||
		active.review_limit != draft.review_limit ||
		active.learning_mode != draft.learning_mode
	);
}

const char *app_settings_action_edit_status(
	const struct study_settings *active_settings,
	const struct study_settings *draft_settings
)
{
	return app_settings_action_has_unsaved_changes(
		active_settings,
		draft_settings
	) ?
		"unsaved changes" :
		"no changes";
}
