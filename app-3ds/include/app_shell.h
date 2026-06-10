#ifndef ANKI3DS_APP_SHELL_H
#define ANKI3DS_APP_SHELL_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "app_battery_monitor.h"
#include "app_mode.h"
#include "app_screen_model.h"
#include "study_backend.h"
#include "study_deck_index.h"
#include "study_session.h"
#include "study_settings.h"

struct app_shell_paths
{
	const char *deck_root_path;
	const char *session_path;
};

struct app_shell
{
	struct study_backend backend;
	struct study_backend rollback_backend;
	struct app_battery_monitor battery_monitor;
	struct study_deck_index deck_index;
	struct study_settings active_settings;
	struct study_settings draft_settings;
	struct study_session session;
	char active_state_path[STUDY_DECK_INDEX_PATH_SIZE];
	char active_settings_path[STUDY_DECK_INDEX_PATH_SIZE];
	char active_review_log_path[STUDY_DECK_INDEX_PATH_SIZE];
	char active_deck_id[STUDY_DECK_INDEX_ID_SIZE];
	struct app_shell_paths paths;
	size_t front_scroll_offset;
	size_t answer_scroll_offset;
	size_t selected_deck_index;
	size_t selected_setting_index;
	time_t next_battery_poll_time;
	unsigned int current_day;
	enum app_mode mode;
	enum app_mode exit_return_mode;
	bool state_enabled;
	bool session_dirty;
	bool redraw;
	bool help_visible;
};

void app_shell_init(
	struct app_shell *shell,
	const struct app_shell_paths *paths,
	time_t startup_time,
	time_t startup_scan_time,
	time_t battery_poll_time
);
void app_shell_shutdown(struct app_shell *shell);
bool app_shell_redraw_needed(const struct app_shell *shell);
void app_shell_mark_drawn(struct app_shell *shell);
enum app_screen_model_kind app_shell_screen_kind(const struct app_shell *shell);
void app_shell_build_screen_model(
	const struct app_shell *shell,
	struct app_screen_model *model
);
const char *app_shell_review_scroll_text(const struct app_shell *shell);
const char *app_shell_review_front_scroll_text(const struct app_shell *shell);
const char *app_shell_review_answer_scroll_text(const struct app_shell *shell);
bool app_shell_handle_frame(
	struct app_shell *shell,
	unsigned int buttons,
	unsigned int dpad_buttons,
	unsigned int cpad_buttons,
	time_t now,
	size_t max_answer_scroll_offset,
	size_t max_front_scroll_offset
);

#endif
