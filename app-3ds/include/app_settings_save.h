#ifndef ANKI3DS_APP_SETTINGS_SAVE_H
#define ANKI3DS_APP_SETTINGS_SAVE_H

#include <stdbool.h>

struct study_backend;
struct study_session;
struct study_settings;

bool app_settings_save_apply(
	struct study_backend *backend,
	struct study_settings *active_settings,
	const struct study_settings *draft_settings,
	const char *settings_path,
	struct study_session *session,
	const char *deck_id,
	bool *session_dirty,
	bool battery_save_warning,
	unsigned long timestamp,
	unsigned int day
);

#endif
