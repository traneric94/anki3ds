#include "app_shell.h"

#include <stdio.h>
#include <string.h>

#include "app_confirm_flow.h"
#include "app_day_rollover_flow.h"
#include "app_deck_flow.h"
#include "app_power.h"
#include "app_review_flow.h"
#include "app_session_save.h"
#include "app_settings_flow.h"
#include "app_state_save.h"
#include "app_time.h"
#include "study_controls.h"
#include "study_review_log.h"

static unsigned long app_shell_timestamp(time_t time_value)
{
	return time_value >= 0 ? (unsigned long)time_value : 0;
}

static unsigned int app_shell_day_from_timestamp(unsigned long timestamp)
{
	return app_time_local_day_from_time((time_t)timestamp);
}

static unsigned int app_shell_day(time_t time_value)
{
	return app_shell_day_from_timestamp(app_shell_timestamp(time_value));
}

static const char *app_shell_deck_root_path(const struct app_shell *shell)
{
	if (
		shell != NULL &&
		shell->paths.deck_root_path != NULL &&
		shell->paths.deck_root_path[0] != '\0'
	)
	{
		return shell->paths.deck_root_path;
	}

	return STUDY_DECK_INDEX_ROOT_PATH;
}

static const char *app_shell_session_path(const struct app_shell *shell)
{
	if (
		shell != NULL &&
		shell->paths.session_path != NULL &&
		shell->paths.session_path[0] != '\0'
	)
	{
		return shell->paths.session_path;
	}

	return STUDY_SESSION_PATH;
}

static struct app_deck_flow_paths app_shell_deck_paths(struct app_shell *shell)
{
	struct app_deck_flow_paths paths = {
		shell != NULL ? shell->active_state_path : NULL,
		shell != NULL ? sizeof(shell->active_state_path) : 0,
		shell != NULL ? shell->active_settings_path : NULL,
		shell != NULL ? sizeof(shell->active_settings_path) : 0,
		shell != NULL ? shell->active_review_log_path : NULL,
		shell != NULL ? sizeof(shell->active_review_log_path) : 0,
		shell != NULL ? shell->active_deck_id : NULL,
		shell != NULL ? sizeof(shell->active_deck_id) : 0,
	};

	return paths;
}

static bool app_shell_refresh_active_deck_stats(
	struct app_shell *shell,
	unsigned int day
)
{
	if (shell == NULL || shell->active_deck_id[0] == '\0')
		return false;

	return study_deck_index_refresh_entry_for_day(
		&shell->deck_index,
		shell->active_deck_id,
		day
	);
}

static bool app_shell_save_state_if_dirty(
	struct app_shell *shell,
	bool state_dirty,
	bool log_dirty,
	enum study_review_log_event log_event,
	enum study_review_log_rating log_rating,
	struct study_session *session,
	bool *session_dirty,
	bool battery_save_warning,
	unsigned long timestamp,
	unsigned int day,
	enum app_state_save_outcome *save_outcome
)
{
	bool changed;

	if (shell == NULL)
		return false;

	changed = app_state_save_if_dirty_with_outcome(
		&shell->backend,
		state_dirty,
		shell->state_enabled,
		shell->active_state_path,
		shell->active_review_log_path,
		log_dirty,
		log_event,
		log_rating,
		session,
		shell->active_deck_id,
		session_dirty,
		battery_save_warning,
		timestamp,
		day,
		save_outcome
	);
	if (
		state_dirty &&
		shell->state_enabled &&
		app_shell_refresh_active_deck_stats(shell, day)
	)
	{
		changed = true;
	}

	return changed;
}

static void app_shell_flush_session(
	struct app_shell *shell
)
{
	enum app_session_save_outcome save_outcome = APP_SESSION_SAVE_OUTCOME_NONE;

	if (shell == NULL || !shell->session_dirty)
		return;

	if (
		app_session_save_if_dirty_with_outcome(
			&shell->backend,
			&shell->session,
			shell->session_dirty,
			app_shell_session_path(shell),
			&save_outcome
		)
	)
	{
		shell->redraw = true;
	}
	if (save_outcome != APP_SESSION_SAVE_OUTCOME_WRITE_FAILED)
		shell->session_dirty = false;
}

static void app_shell_poll_battery_if_due(
	struct app_shell *shell,
	time_t now
)
{
	enum app_power_battery_sample_result sample_result;

	if (shell == NULL)
		return;
	if (!app_power_battery_poll_is_due(&shell->next_battery_poll_time, now))
		return;

	sample_result = app_battery_monitor_sample(&shell->battery_monitor);
	app_power_schedule_next_battery_poll_after_sample(
		&shell->next_battery_poll_time,
		now,
		sample_result
	);
	if (
		app_battery_monitor_apply_warning(
			&shell->battery_monitor,
			&shell->backend,
			sample_result
		)
	)
	{
		shell->redraw = true;
	}
}

static bool app_shell_apply_scroll_buttons(
	unsigned int buttons,
	size_t max_scroll_offset,
	size_t *scroll_offset
)
{
	if (scroll_offset == NULL)
		return false;

	if (buttons == STUDY_CONTROL_BUTTON_DOWN)
	{
		if (*scroll_offset >= max_scroll_offset)
			return false;
		(*scroll_offset)++;
		return true;
	}
	if (buttons == STUDY_CONTROL_BUTTON_UP)
	{
		if (*scroll_offset == 0)
			return false;
		(*scroll_offset)--;
		return true;
	}

	return false;
}

static bool app_shell_view_active_card_changed(
	const struct study_backend_view *previous_view,
	const struct study_backend_view *current_view
)
{
	if (previous_view == NULL || current_view == NULL)
		return false;

	return previous_view->has_active_card != current_view->has_active_card ||
		previous_view->card_index != current_view->card_index;
}

static void app_shell_apply_day_rollover(
	struct app_shell *shell,
	time_t now,
	bool *session_dirty
)
{
	unsigned long timestamp;
	unsigned int observed_day;

	if (shell == NULL || now < 0)
		return;

	timestamp = app_shell_timestamp(now);
	observed_day = app_shell_day_from_timestamp(timestamp);
	if (
		app_day_rollover_flow_apply(
			&shell->backend,
			shell->state_enabled,
			shell->active_state_path,
			shell->active_review_log_path,
			&shell->session,
			shell->active_deck_id,
			session_dirty,
			&shell->current_day,
			timestamp,
			observed_day,
			app_battery_monitor_save_warning_needed(&shell->battery_monitor)
		)
	)
	{
		(void)app_shell_refresh_active_deck_stats(shell, shell->current_day);
		shell->redraw = true;
	}
}

static bool app_shell_handle_deck_select(
	struct app_shell *shell,
	unsigned int buttons,
	unsigned long timestamp,
	unsigned int day
)
{
	struct app_deck_flow_paths paths;
	bool state_dirty = false;

	if (shell == NULL)
		return false;

	if (buttons == STUDY_CONTROL_BUTTON_START)
	{
		shell->help_visible = !shell->help_visible;
		shell->redraw = true;
		return false;
	}

	paths = app_shell_deck_paths(shell);
	if (
		app_deck_flow_handle_select_input(
			buttons,
			app_shell_deck_root_path(shell),
			&shell->deck_index,
			&shell->selected_deck_index,
			&shell->backend,
			&shell->active_settings,
			shell->current_day,
			&paths,
			&shell->session,
			&shell->session_dirty,
			&shell->state_enabled,
			&state_dirty,
			&shell->mode,
			&shell->exit_return_mode,
			app_battery_monitor_save_warning_needed(&shell->battery_monitor),
			timestamp,
			day
		)
	)
	{
		shell->front_scroll_offset = 0;
		shell->answer_scroll_offset = 0;
		shell->redraw = true;
	}
	if (
		app_shell_save_state_if_dirty(
			shell,
			state_dirty,
			false,
			STUDY_REVIEW_LOG_EVENT_RATING,
			STUDY_REVIEW_LOG_RATING_NONE,
			NULL,
			NULL,
			app_battery_monitor_save_warning_needed(&shell->battery_monitor),
			timestamp,
			day,
			NULL
		)
	)
	{
		shell->redraw = true;
	}
	return false;
}

static bool app_shell_handle_settings(
	struct app_shell *shell,
	unsigned int buttons,
	unsigned long timestamp,
	unsigned int day
)
{
	if (shell == NULL)
		return false;

	if (
		!app_settings_flow_handle_input(
			buttons,
			&shell->backend,
			&shell->active_settings,
			&shell->draft_settings,
			shell->active_settings_path,
			&shell->selected_setting_index,
			&shell->session,
			shell->active_deck_id,
			&shell->session_dirty,
			&shell->mode,
			&shell->exit_return_mode,
			app_battery_monitor_save_warning_needed(&shell->battery_monitor),
			timestamp,
			day
		)
	)
	{
		return false;
	}

	shell->redraw = true;
	return false;
}

static bool app_shell_handle_confirm(
	struct app_shell *shell,
	unsigned int buttons,
	unsigned long timestamp,
	unsigned int day
)
{
	bool review_scroll_reset = false;
	bool state_dirty = false;
	bool log_dirty = false;
	bool exit_requested = false;
	bool previous_session_dirty;
	enum app_mode previous_mode;
	struct study_session previous_session;
	size_t previous_front_scroll_offset;
	size_t previous_answer_scroll_offset;
	enum app_state_save_outcome save_outcome = APP_STATE_SAVE_OUTCOME_NONE;
	enum study_review_log_event log_event = STUDY_REVIEW_LOG_EVENT_RATING;

	if (shell == NULL)
		return false;

	previous_mode = shell->mode;
	shell->rollback_backend = shell->backend;
	previous_session = shell->session;
	previous_front_scroll_offset = shell->front_scroll_offset;
	previous_answer_scroll_offset = shell->answer_scroll_offset;
	previous_session_dirty = shell->session_dirty;
	if (
		!app_confirm_flow_handle_input(
			&shell->mode,
			&shell->exit_return_mode,
			buttons,
			&shell->backend,
			shell->state_enabled,
			shell->active_state_path,
			shell->active_review_log_path,
			&shell->session,
			shell->active_deck_id,
			&shell->session_dirty,
			&state_dirty,
			&log_dirty,
			&log_event,
			&exit_requested,
			&review_scroll_reset,
			app_battery_monitor_save_warning_needed(&shell->battery_monitor),
			timestamp,
			day
		)
	)
	{
		return false;
	}
	if (review_scroll_reset)
	{
		shell->front_scroll_offset = 0;
		shell->answer_scroll_offset = 0;
	}
	if (
		previous_mode == APP_MODE_CONFIRM_RESET &&
		app_shell_refresh_active_deck_stats(shell, day)
	)
	{
		shell->redraw = true;
	}

	shell->redraw = true;
	if (
		app_shell_save_state_if_dirty(
			shell,
			state_dirty,
			log_dirty,
			log_event,
			STUDY_REVIEW_LOG_RATING_NONE,
			&shell->session,
			&shell->session_dirty,
			app_battery_monitor_save_warning_needed(&shell->battery_monitor),
			timestamp,
			day,
			&save_outcome
		)
	)
	{
		shell->redraw = true;
	}
	if (save_outcome == APP_STATE_SAVE_OUTCOME_WRITE_FAILED)
	{
		shell->backend = shell->rollback_backend;
		shell->session = previous_session;
		shell->front_scroll_offset = previous_front_scroll_offset;
		shell->answer_scroll_offset = previous_answer_scroll_offset;
		shell->session_dirty = previous_session_dirty;
		study_backend_set_status(&shell->backend, "Save failed");
		(void)app_shell_refresh_active_deck_stats(shell, day);
		shell->redraw = true;
	}
	return exit_requested;
}

static bool app_shell_handle_review(
	struct app_shell *shell,
	unsigned int buttons,
	unsigned int dpad_buttons,
	unsigned int cpad_buttons,
	size_t max_answer_scroll_offset,
	size_t max_front_scroll_offset,
	unsigned long timestamp,
	unsigned int day
)
{
	bool state_dirty = false;
	bool log_dirty = false;
	bool exit_requested = false;
	bool previous_session_dirty;
	struct study_session previous_session;
	struct study_backend_view previous_view;
	struct study_backend_view current_view;
	size_t previous_front_scroll_offset;
	size_t previous_answer_scroll_offset;
	enum app_state_save_outcome save_outcome = APP_STATE_SAVE_OUTCOME_NONE;
	enum study_review_log_event log_event = STUDY_REVIEW_LOG_EVENT_RATING;
	enum study_review_log_rating log_rating = STUDY_REVIEW_LOG_RATING_NONE;

	if (shell == NULL)
		return false;

	if (
		app_shell_apply_scroll_buttons(
			dpad_buttons,
			max_answer_scroll_offset,
			&shell->answer_scroll_offset
		) ||
		app_shell_apply_scroll_buttons(
			cpad_buttons,
			max_front_scroll_offset,
			&shell->front_scroll_offset
		)
	)
	{
		shell->redraw = true;
		return false;
	}
	if (dpad_buttons != 0 || cpad_buttons != 0)
		return false;

	study_backend_build_view(&shell->backend, &previous_view);
	shell->rollback_backend = shell->backend;
	previous_session = shell->session;
	previous_front_scroll_offset = shell->front_scroll_offset;
	previous_answer_scroll_offset = shell->answer_scroll_offset;
	previous_session_dirty = shell->session_dirty;
	if (
		app_review_flow_handle_input(
			buttons,
			&shell->backend,
			&shell->active_settings,
			&shell->draft_settings,
			shell->active_settings_path,
			&shell->selected_setting_index,
			max_answer_scroll_offset,
			&shell->answer_scroll_offset,
			&shell->help_visible,
			&shell->mode,
			&shell->exit_return_mode,
			&shell->session,
			shell->active_deck_id,
			&shell->session_dirty,
			&state_dirty,
			&log_dirty,
			&log_event,
			&log_rating,
			&exit_requested,
			timestamp,
			day
		)
	)
	{
		study_backend_build_view(&shell->backend, &current_view);
		if (app_shell_view_active_card_changed(&previous_view, &current_view))
		{
			shell->front_scroll_offset = 0;
			shell->answer_scroll_offset = 0;
		}
		shell->redraw = true;
	}
	if (
		app_shell_save_state_if_dirty(
			shell,
			state_dirty,
			log_dirty,
			log_event,
			log_rating,
			&shell->session,
			&shell->session_dirty,
			app_battery_monitor_save_warning_needed(&shell->battery_monitor),
			timestamp,
			day,
			&save_outcome
		)
	)
	{
		shell->redraw = true;
	}
	if (save_outcome == APP_STATE_SAVE_OUTCOME_WRITE_FAILED)
	{
		shell->backend = shell->rollback_backend;
		shell->session = previous_session;
		shell->front_scroll_offset = previous_front_scroll_offset;
		shell->answer_scroll_offset = previous_answer_scroll_offset;
		shell->session_dirty = previous_session_dirty;
		study_backend_set_status(&shell->backend, "Save failed");
		(void)app_shell_refresh_active_deck_stats(shell, day);
		shell->redraw = true;
	}
	return exit_requested;
}

void app_shell_init(
	struct app_shell *shell,
	const struct app_shell_paths *paths,
	time_t startup_time,
	time_t startup_scan_time,
	time_t battery_poll_time
)
{
	unsigned long startup_timestamp;
	unsigned long startup_scan_timestamp;
	enum app_power_battery_sample_result battery_sample_result;

	if (shell == NULL)
		return;

	memset(shell, 0, sizeof(*shell));
	if (paths != NULL)
		shell->paths = *paths;
	app_battery_monitor_init(&shell->battery_monitor);
	study_settings_defaults(&shell->active_settings);
	study_settings_defaults(&shell->draft_settings);
	shell->mode = APP_MODE_DECK_SELECT;
	shell->exit_return_mode = APP_MODE_DECK_SELECT;
	shell->redraw = true;

	startup_timestamp = app_shell_timestamp(startup_time);
	shell->current_day = app_shell_day_from_timestamp(startup_timestamp);
	(void)study_session_load_or_init(
		&shell->session,
		app_shell_session_path(shell),
		startup_timestamp,
		shell->current_day
	);
	study_session_start_launch(
		&shell->session,
		startup_timestamp,
		shell->current_day
	);

	startup_scan_timestamp = app_shell_timestamp(startup_scan_time);
	app_deck_flow_startup_scan(
		app_shell_deck_root_path(shell),
		&shell->deck_index,
		&shell->selected_deck_index,
		&shell->backend,
		&shell->session,
		&shell->session_dirty,
		startup_scan_timestamp,
		app_shell_day_from_timestamp(startup_scan_timestamp)
	);
	app_shell_flush_session(shell);

	battery_sample_result = app_battery_monitor_sample(&shell->battery_monitor);
	app_power_schedule_next_battery_poll_after_sample(
		&shell->next_battery_poll_time,
		battery_poll_time,
		battery_sample_result
	);
	if (
		app_battery_monitor_apply_warning(
			&shell->battery_monitor,
			&shell->backend,
			battery_sample_result
		)
	)
	{
		shell->redraw = true;
	}
}

void app_shell_shutdown(struct app_shell *shell)
{
	if (shell == NULL)
		return;

	app_battery_monitor_shutdown(&shell->battery_monitor);
}

bool app_shell_redraw_needed(const struct app_shell *shell)
{
	return shell != NULL && shell->redraw;
}

void app_shell_mark_drawn(struct app_shell *shell)
{
	if (shell == NULL)
		return;

	shell->redraw = false;
}

enum app_screen_model_kind app_shell_screen_kind(const struct app_shell *shell)
{
	return app_mode_screen_model_kind(
		shell != NULL ? shell->mode : APP_MODE_DECK_SELECT
	);
}

void app_shell_build_screen_model(
	const struct app_shell *shell,
	struct app_screen_model *model
)
{
	if (shell == NULL)
		return;

	app_screen_model_build(
		model,
		app_shell_screen_kind(shell),
		app_mode_confirm_contract_kind(shell->mode),
		&shell->deck_index,
		shell->selected_deck_index,
		&shell->backend,
		&shell->active_settings,
		&shell->draft_settings,
		shell->selected_setting_index,
		shell->front_scroll_offset,
		shell->answer_scroll_offset,
		shell->help_visible
	);
}

const char *app_shell_review_front_scroll_text(const struct app_shell *shell)
{
	struct study_backend_view view;

	if (shell == NULL || shell->mode != APP_MODE_REVIEW)
		return "";

	study_backend_build_view(&shell->backend, &view);
	if (!view.has_active_card)
		return view.primary_text != NULL ? view.primary_text : "";
	return view.front_text != NULL ? view.front_text : "";
}

const char *app_shell_review_answer_scroll_text(const struct app_shell *shell)
{
	struct study_backend_view view;

	if (shell == NULL || shell->mode != APP_MODE_REVIEW)
		return "";

	study_backend_build_view(&shell->backend, &view);
	if (!view.has_active_card || !view.answer_visible)
		return "";
	return view.back_text != NULL ? view.back_text : "";
}

const char *app_shell_review_scroll_text(const struct app_shell *shell)
{
	struct study_backend_view view;

	if (shell == NULL || shell->mode != APP_MODE_REVIEW)
		return "";

	study_backend_build_view(&shell->backend, &view);
	if (view.has_active_card && view.answer_visible)
		return view.back_text != NULL ? view.back_text : "";
	if (view.has_active_card)
		return view.front_text != NULL ? view.front_text : "";
	return view.primary_text != NULL ? view.primary_text : "";
}

bool app_shell_handle_frame(
	struct app_shell *shell,
	unsigned int buttons,
	unsigned int dpad_buttons,
	unsigned int cpad_buttons,
	time_t now,
	size_t max_answer_scroll_offset,
	size_t max_front_scroll_offset
)
{
	bool exit_requested = false;
	bool previous_session_dirty = false;
	struct study_session previous_session;
	enum app_mode previous_mode;
	unsigned long timestamp;
	unsigned int day;

	if (shell == NULL)
		return false;

	app_shell_poll_battery_if_due(shell, now);
	app_shell_apply_day_rollover(shell, now, &shell->session_dirty);
	app_shell_flush_session(shell);
	previous_session = shell->session;
	previous_session_dirty = shell->session_dirty;
	previous_mode = shell->mode;

	timestamp = app_shell_timestamp(now);
	day = app_shell_day(now);
	if (shell->mode == APP_MODE_DECK_SELECT)
	{
		exit_requested = app_shell_handle_deck_select(
			shell,
			buttons,
			timestamp,
			day
		);
	}
	else if (shell->mode == APP_MODE_SETTINGS)
	{
		exit_requested = app_shell_handle_settings(
			shell,
			buttons,
			timestamp,
			day
		);
	}
	else if (app_mode_is_confirm(shell->mode))
	{
		exit_requested = app_shell_handle_confirm(
			shell,
			buttons,
			timestamp,
			day
		);
	}
	else
	{
		exit_requested = app_shell_handle_review(
			shell,
			buttons,
			dpad_buttons,
			cpad_buttons,
			max_answer_scroll_offset,
			max_front_scroll_offset,
			timestamp,
			day
		);
	}
	if (shell->mode != previous_mode)
		shell->help_visible = false;
	app_shell_flush_session(shell);
	if (exit_requested && shell->session_dirty)
	{
		shell->session = previous_session;
		shell->session_dirty = previous_session_dirty;
		shell->mode = APP_MODE_CONFIRM_EXIT;
		shell->redraw = true;
		return false;
	}
	return exit_requested;
}
