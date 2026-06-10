#include "app_state_save.h"

#include "app_status_text.h"
#include "study_backend.h"
#include "study_session.h"

static bool app_state_save_set_status_with_battery_suffix(
	struct study_backend *backend,
	const char *status,
	bool battery_save_warning
)
{
	char status_text[STUDY_BACKEND_STATUS_SIZE];
	bool suffix_appended;

	if (backend == NULL)
		return false;

	suffix_appended = app_status_text_copy_with_low_battery_suffix(
		status_text,
		sizeof(status_text),
		status,
		battery_save_warning
	);
	study_backend_set_status(backend, status_text);
	return suffix_appended;
}

static bool app_state_save_append_battery_suffix(
	struct study_backend *backend,
	bool battery_save_warning
)
{
	if (backend == NULL || !battery_save_warning)
		return false;

	return app_state_save_set_status_with_battery_suffix(
		backend,
		backend->status_text,
		true
	);
}

static enum study_session_event app_state_save_session_event(
	enum study_review_log_event event
)
{
	switch (event)
	{
	case STUDY_REVIEW_LOG_EVENT_RATING:
		return STUDY_SESSION_EVENT_RATING_SAVED;
	case STUDY_REVIEW_LOG_EVENT_UNDO:
		return STUDY_SESSION_EVENT_UNDO_SAVED;
	case STUDY_REVIEW_LOG_EVENT_SUSPEND:
		return STUDY_SESSION_EVENT_SUSPEND_SAVED;
	case STUDY_REVIEW_LOG_EVENT_RESTORE:
		return STUDY_SESSION_EVENT_RESTORE_SAVED;
	}

	return STUDY_SESSION_EVENT_RATING_SAVED;
}

bool app_state_save_if_dirty(
	struct study_backend *backend,
	bool state_dirty,
	bool state_enabled,
	const char *state_path,
	const char *review_log_path,
	bool log_dirty,
	enum study_review_log_event log_event,
	enum study_review_log_rating log_rating,
	struct study_session *session,
	const char *deck_id,
	bool *session_dirty,
	bool battery_save_warning,
	unsigned long timestamp,
	unsigned int day
)
{
	return app_state_save_if_dirty_with_outcome(
		backend,
		state_dirty,
		state_enabled,
		state_path,
		review_log_path,
		log_dirty,
		log_event,
		log_rating,
		session,
		deck_id,
		session_dirty,
		battery_save_warning,
		timestamp,
		day,
		NULL
	);
}

bool app_state_save_if_dirty_with_outcome(
	struct study_backend *backend,
	bool state_dirty,
	bool state_enabled,
	const char *state_path,
	const char *review_log_path,
	bool log_dirty,
	enum study_review_log_event log_event,
	enum study_review_log_rating log_rating,
	struct study_session *session,
	const char *deck_id,
	bool *session_dirty,
	bool battery_save_warning,
	unsigned long timestamp,
	unsigned int day,
	enum app_state_save_outcome *outcome
)
{
	struct study_review_log_entry log_entry;

	if (outcome != NULL)
		*outcome = APP_STATE_SAVE_OUTCOME_NONE;
	if (!state_dirty || !state_enabled)
		return false;

	if (study_backend_save_state_tsv(backend, state_path) != STUDY_BACKEND_STATE_OK)
	{
		if (outcome != NULL)
			*outcome = APP_STATE_SAVE_OUTCOME_WRITE_FAILED;
		study_backend_set_status(
			backend,
			study_backend_state_result_name(
				STUDY_BACKEND_STATE_WRITE_FAILED
			)
		);
		return true;
	}
	if (outcome != NULL)
		*outcome = APP_STATE_SAVE_OUTCOME_SAVED;

	if (log_dirty && session != NULL)
	{
		study_session_record_event(
			session,
			app_state_save_session_event(log_event),
			deck_id,
			timestamp,
			day
		);
		if (session_dirty != NULL)
			*session_dirty = true;
	}
	if (!log_dirty || review_log_path == NULL || review_log_path[0] == '\0')
	{
		(void)app_state_save_append_battery_suffix(
			backend,
			battery_save_warning
		);
		return true;
	}
	if (battery_save_warning)
	{
		if (outcome != NULL)
			*outcome = APP_STATE_SAVE_OUTCOME_LOG_SKIPPED;
		app_state_save_set_status_with_battery_suffix(
			backend,
			"Saved; log skipped",
			true
		);
		return true;
	}

	log_entry.timestamp = timestamp;
	log_entry.event = log_event;
	log_entry.rating = log_rating;
	log_entry.card_index = backend != NULL ? backend->current_index : 0;
	log_entry.reviewed_count = backend != NULL ? backend->reviewed_count : 0;
	log_entry.suspended_count = backend != NULL ? backend->suspended_count : 0;
	if (study_review_log_append(review_log_path, &log_entry))
	{
		(void)app_state_save_append_battery_suffix(
			backend,
			battery_save_warning
		);
		return true;
	}

	if (outcome != NULL)
		*outcome = APP_STATE_SAVE_OUTCOME_LOG_SKIPPED;
	app_state_save_set_status_with_battery_suffix(
		backend,
		"Saved; log skipped",
		battery_save_warning
	);
	return true;
}
