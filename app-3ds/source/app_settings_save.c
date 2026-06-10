#include "app_settings_save.h"

#include "app_learning_policy.h"
#include "app_status_text.h"
#include "study_backend.h"
#include "study_session.h"
#include "study_settings.h"

static void app_settings_save_set_status_with_context(
	struct study_backend *backend,
	const char *status,
	bool battery_save_warning
)
{
	char status_text[STUDY_BACKEND_STATUS_SIZE];

	if (backend == NULL)
		return;

	(void)app_status_text_copy_with_warning_context_and_low_battery_suffix(
		status_text,
		sizeof(status_text),
		status,
		backend->status_text,
		battery_save_warning
	);
	study_backend_set_status(backend, status_text);
}

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
)
{
	enum study_settings_result result;

	if (backend == NULL || active_settings == NULL || draft_settings == NULL)
		return false;

	result = study_settings_save_tsv(draft_settings, settings_path);
	if (result != STUDY_SETTINGS_OK)
	{
		study_backend_set_status(backend, study_settings_result_name(result));
		return false;
	}

	*active_settings = *draft_settings;
	app_learning_policy_apply_to_backend(backend, active_settings);
	app_settings_save_set_status_with_context(
		backend,
		"Settings saved",
		battery_save_warning
	);
	if (session != NULL)
	{
		study_session_record_event(
			session,
			STUDY_SESSION_EVENT_SETTINGS_SAVED,
			deck_id,
			timestamp,
			day
		);
		if (session_dirty != NULL)
			*session_dirty = true;
	}
	return true;
}
