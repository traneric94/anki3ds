#ifndef ANKI3DS_APP_SETTINGS_FLOW_H
#define ANKI3DS_APP_SETTINGS_FLOW_H

#include <stdbool.h>
#include <stddef.h>

#include "app_mode.h"

struct study_backend;
struct study_session;
struct study_settings;

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
);

#endif
