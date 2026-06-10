#include "app_session_save.h"

#include "study_backend.h"
#include "study_session.h"

bool app_session_save_if_dirty(
	struct study_backend *backend,
	const struct study_session *session,
	bool session_dirty,
	const char *session_path
)
{
	return app_session_save_if_dirty_with_outcome(
		backend,
		session,
		session_dirty,
		session_path,
		NULL
	);
}

bool app_session_save_if_dirty_with_outcome(
	struct study_backend *backend,
	const struct study_session *session,
	bool session_dirty,
	const char *session_path,
	enum app_session_save_outcome *outcome
)
{
	if (!session_dirty)
	{
		if (outcome != NULL)
			*outcome = APP_SESSION_SAVE_OUTCOME_NONE;
		return false;
	}

	if (study_session_save_tsv(session, session_path) == STUDY_SESSION_OK)
	{
		if (outcome != NULL)
			*outcome = APP_SESSION_SAVE_OUTCOME_SAVED;
		return false;
	}

	if (outcome != NULL)
		*outcome = APP_SESSION_SAVE_OUTCOME_WRITE_FAILED;
	study_backend_set_status(
		backend,
		study_session_result_name(STUDY_SESSION_WRITE_FAILED)
	);
	return true;
}
