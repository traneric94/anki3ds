#ifndef ANKI3DS_APP_DECK_FLOW_H
#define ANKI3DS_APP_DECK_FLOW_H

#include <stdbool.h>
#include <stddef.h>

#include "app_mode.h"
#include "study_backend.h"
#include "study_deck_index.h"
#include "study_session.h"
#include "study_settings.h"

struct app_deck_flow_paths
{
	char *state_path;
	size_t state_path_size;
	char *settings_path;
	size_t settings_path_size;
	char *review_log_path;
	size_t review_log_path_size;
	char *deck_id;
	size_t deck_id_size;
};

void app_deck_flow_rescan(
	struct study_deck_index *index,
	size_t *selected_deck_index,
	struct study_backend *backend,
	const char *root_path,
	unsigned int current_day
);

void app_deck_flow_startup_scan(
	const char *root_path,
	struct study_deck_index *deck_index,
	size_t *selected_deck_index,
	struct study_backend *backend,
	struct study_session *session,
	bool *session_dirty,
	unsigned long timestamp,
	unsigned int day
);

bool app_deck_flow_handle_select_input(
	unsigned int buttons,
	const char *root_path,
	struct study_deck_index *deck_index,
	size_t *selected_deck_index,
	struct study_backend *backend,
	struct study_settings *settings,
	unsigned int current_day,
	const struct app_deck_flow_paths *paths,
	struct study_session *session,
	bool *session_dirty,
	bool *state_enabled,
	bool *state_dirty,
	enum app_mode *mode,
	enum app_mode *exit_return_mode,
	bool battery_save_warning,
	unsigned long timestamp,
	unsigned int day
);

#endif
