#ifndef ANKI3DS_APP_LEARNING_POLICY_H
#define ANKI3DS_APP_LEARNING_POLICY_H

#include "study_backend.h"
#include "study_settings.h"

static inline void app_learning_policy_apply_to_backend(
	struct study_backend *backend,
	const struct study_settings *settings
)
{
	enum study_settings_learning_mode mode =
		settings != NULL ?
			settings->learning_mode :
			STUDY_SETTINGS_DEFAULT_LEARNING_MODE;

	if (backend == NULL)
		return;

	switch (mode)
	{
	case STUDY_SETTINGS_LEARNING_CARD_COOLDOWN:
		study_backend_set_scheduler_policy(
			backend,
			STUDY_BACKEND_SCHEDULER_CARD_COUNT_COOLDOWN,
			STUDY_BACKEND_DEFAULT_CARD_COOLDOWN
		);
		return;
	case STUDY_SETTINGS_LEARNING_DUE_FIRST:
	default:
		study_backend_set_scheduler_policy(
			backend,
			STUDY_BACKEND_SCHEDULER_DUE_FIRST,
			STUDY_BACKEND_DEFAULT_CARD_COOLDOWN
		);
		return;
	}
}

#endif
