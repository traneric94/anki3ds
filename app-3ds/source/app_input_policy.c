#include "app_input_policy.h"

#include "study_controls.h"

unsigned int app_input_policy_repeat_mask(enum app_screen_model_kind kind)
{
	switch (kind)
	{
	case APP_SCREEN_MODEL_DECK_SELECT:
		return
			STUDY_CONTROL_BUTTON_UP |
			STUDY_CONTROL_BUTTON_DOWN |
			STUDY_CONTROL_BUTTON_LEFT |
			STUDY_CONTROL_BUTTON_RIGHT |
			STUDY_CONTROL_BUTTON_L |
			STUDY_CONTROL_BUTTON_R;
	case APP_SCREEN_MODEL_REVIEW:
		return STUDY_CONTROL_BUTTON_UP | STUDY_CONTROL_BUTTON_DOWN;
	case APP_SCREEN_MODEL_SETTINGS:
		return STUDY_CONTROL_BUTTON_LEFT | STUDY_CONTROL_BUTTON_RIGHT;
	case APP_SCREEN_MODEL_CONFIRM:
		return 0;
	}

	return 0;
}
