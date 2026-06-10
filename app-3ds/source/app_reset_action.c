#include "app_reset_action.h"

#include "app_status_text.h"
#include "study_backend.h"
#include "study_review_log.h"

static void app_reset_action_set_status_with_context(
	struct study_backend *backend,
	const char *status,
	const char *warning_context,
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
		warning_context,
		battery_save_warning
	);
	study_backend_set_status(backend, status_text);
}

bool app_reset_action_confirm(
	struct study_backend *backend,
	bool state_enabled,
	const char *state_path,
	const char *review_log_path,
	const char *warning_context,
	bool battery_save_warning
)
{
	bool state_deleted;
	bool log_deleted;

	if (backend == NULL)
		return false;

	state_deleted =
		!state_enabled ||
		study_backend_delete_state_tsv(state_path);
	if (!state_deleted)
	{
		study_backend_set_status(backend, "Reset; state delete failed");
		return false;
	}

	(void)study_backend_reset_progress(backend);
	log_deleted =
		!state_enabled ||
		review_log_path == NULL ||
		review_log_path[0] == '\0' ||
		study_review_log_delete(review_log_path);
	if (!log_deleted)
	{
		app_reset_action_set_status_with_context(
			backend,
			"Progress reset; log kept",
			warning_context,
			battery_save_warning
		);
		return true;
	}

	app_reset_action_set_status_with_context(
		backend,
		"Progress reset",
		warning_context,
		battery_save_warning
	);
	return true;
}
