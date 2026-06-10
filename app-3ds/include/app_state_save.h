#ifndef ANKI3DS_APP_STATE_SAVE_H
#define ANKI3DS_APP_STATE_SAVE_H

#include <stdbool.h>

#include "study_review_log.h"

struct study_backend;
struct study_session;

enum app_state_save_outcome
{
	APP_STATE_SAVE_OUTCOME_NONE,
	APP_STATE_SAVE_OUTCOME_SAVED,
	APP_STATE_SAVE_OUTCOME_WRITE_FAILED,
	APP_STATE_SAVE_OUTCOME_LOG_SKIPPED,
};

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
);
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
);

#endif
