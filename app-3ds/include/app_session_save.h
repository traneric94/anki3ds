#ifndef ANKI3DS_APP_SESSION_SAVE_H
#define ANKI3DS_APP_SESSION_SAVE_H

#include <stdbool.h>

struct study_backend;
struct study_session;

enum app_session_save_outcome
{
	APP_SESSION_SAVE_OUTCOME_NONE,
	APP_SESSION_SAVE_OUTCOME_SAVED,
	APP_SESSION_SAVE_OUTCOME_WRITE_FAILED,
};

bool app_session_save_if_dirty(
	struct study_backend *backend,
	const struct study_session *session,
	bool session_dirty,
	const char *session_path
);
bool app_session_save_if_dirty_with_outcome(
	struct study_backend *backend,
	const struct study_session *session,
	bool session_dirty,
	const char *session_path,
	enum app_session_save_outcome *outcome
);

#endif
