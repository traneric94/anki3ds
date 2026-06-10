#ifndef ANKI3DS_APP_RESET_ACTION_H
#define ANKI3DS_APP_RESET_ACTION_H

#include <stdbool.h>

struct study_backend;

bool app_reset_action_confirm(
	struct study_backend *backend,
	bool state_enabled,
	const char *state_path,
	const char *review_log_path,
	const char *warning_context,
	bool battery_save_warning
);

#endif
