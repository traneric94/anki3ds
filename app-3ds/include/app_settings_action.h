#ifndef ANKI3DS_APP_SETTINGS_ACTION_H
#define ANKI3DS_APP_SETTINGS_ACTION_H

#include <stdbool.h>
#include <stddef.h>

#include "study_settings.h"

enum app_settings_action_result
{
	APP_SETTINGS_ACTION_NONE,
	APP_SETTINGS_ACTION_EXIT,
	APP_SETTINGS_ACTION_CANCEL,
	APP_SETTINGS_ACTION_SAVE,
	APP_SETTINGS_ACTION_UPDATED,
};

enum app_settings_action_result app_settings_action_apply(
	unsigned int buttons,
	struct study_settings *draft_settings,
	size_t *selected_index
);
bool app_settings_action_has_unsaved_changes(
	const struct study_settings *active_settings,
	const struct study_settings *draft_settings
);
const char *app_settings_action_edit_status(
	const struct study_settings *active_settings,
	const struct study_settings *draft_settings
);

#endif
