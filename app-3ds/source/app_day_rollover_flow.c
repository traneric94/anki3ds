#include "app_day_rollover_flow.h"

#include "app_state_save.h"
#include "study_backend.h"
#include "study_review_log.h"
#include "study_session.h"

static struct study_backend app_day_rollover_previous_backend;

bool app_day_rollover_flow_apply(
	struct study_backend *backend,
	bool state_enabled,
	const char *state_path,
	const char *review_log_path,
	struct study_session *session,
	const char *deck_id,
	bool *session_dirty,
	unsigned int *current_day,
	unsigned long timestamp,
	unsigned int observed_day,
	bool battery_save_warning
)
{
	return app_day_rollover_flow_apply_with_outcome(
		backend,
		state_enabled,
		state_path,
		review_log_path,
		session,
		deck_id,
		session_dirty,
		current_day,
		timestamp,
		observed_day,
		battery_save_warning,
		NULL
	);
}

bool app_day_rollover_flow_apply_with_outcome(
	struct study_backend *backend,
	bool state_enabled,
	const char *state_path,
	const char *review_log_path,
	struct study_session *session,
	const char *deck_id,
	bool *session_dirty,
	unsigned int *current_day,
	unsigned long timestamp,
	unsigned int observed_day,
	bool battery_save_warning,
	enum app_state_save_outcome *save_outcome
)
{
	bool redraw = false;
	bool previous_session_dirty;
	unsigned int previous_day = 0;
	struct study_session previous_session;
	enum app_state_save_outcome local_save_outcome;
	enum app_state_save_outcome *active_save_outcome;

	active_save_outcome = save_outcome != NULL ?
		save_outcome :
		&local_save_outcome;
	*active_save_outcome = APP_STATE_SAVE_OUTCOME_NONE;
	if (current_day == NULL || *current_day == observed_day)
		return false;

	previous_day = *current_day;
	if (backend != NULL)
		app_day_rollover_previous_backend = *backend;
	if (session != NULL)
		previous_session = *session;
	previous_session_dirty = session_dirty != NULL && *session_dirty;

	*current_day = observed_day;
	if (study_session_rollover_day(session, timestamp, observed_day))
	{
		if (session_dirty != NULL)
			*session_dirty = true;
	}
	if (study_backend_rollover_day(backend, observed_day))
	{
		redraw = true;
		if (
			app_state_save_if_dirty_with_outcome(
				backend,
				true,
				state_enabled,
				state_path,
				review_log_path,
				false,
				STUDY_REVIEW_LOG_EVENT_RATING,
				STUDY_REVIEW_LOG_RATING_NONE,
				NULL,
				deck_id,
				NULL,
				battery_save_warning,
				timestamp,
				observed_day,
				active_save_outcome
			)
		)
		{
			redraw = true;
		}
		if (*active_save_outcome == APP_STATE_SAVE_OUTCOME_WRITE_FAILED)
		{
			*current_day = previous_day;
			if (backend != NULL)
			{
				*backend = app_day_rollover_previous_backend;
				study_backend_set_status(backend, "Save failed");
			}
			if (session != NULL)
				*session = previous_session;
			if (session_dirty != NULL)
				*session_dirty = previous_session_dirty;
			return true;
		}
	}

	return redraw;
}
