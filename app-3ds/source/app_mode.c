#include "app_mode.h"

bool app_mode_is_confirm(enum app_mode mode)
{
	switch (mode)
	{
	case APP_MODE_CONFIRM_SUSPEND:
	case APP_MODE_CONFIRM_RESTORE:
	case APP_MODE_CONFIRM_RESET:
	case APP_MODE_CONFIRM_EXIT:
		return true;
	case APP_MODE_DECK_SELECT:
	case APP_MODE_REVIEW:
	case APP_MODE_SETTINGS:
		break;
	}

	return false;
}

enum app_confirm_contract_kind app_mode_confirm_contract_kind(enum app_mode mode)
{
	switch (mode)
	{
	case APP_MODE_CONFIRM_RESTORE:
		return APP_CONFIRM_CONTRACT_RESTORE;
	case APP_MODE_CONFIRM_RESET:
		return APP_CONFIRM_CONTRACT_RESET;
	case APP_MODE_CONFIRM_EXIT:
		return APP_CONFIRM_CONTRACT_EXIT;
	case APP_MODE_DECK_SELECT:
	case APP_MODE_REVIEW:
	case APP_MODE_CONFIRM_SUSPEND:
	case APP_MODE_SETTINGS:
		break;
	}

	return APP_CONFIRM_CONTRACT_SUSPEND;
}

enum app_screen_model_kind app_mode_screen_model_kind(enum app_mode mode)
{
	switch (mode)
	{
	case APP_MODE_DECK_SELECT:
		return APP_SCREEN_MODEL_DECK_SELECT;
	case APP_MODE_SETTINGS:
		return APP_SCREEN_MODEL_SETTINGS;
	case APP_MODE_CONFIRM_SUSPEND:
	case APP_MODE_CONFIRM_RESTORE:
	case APP_MODE_CONFIRM_RESET:
	case APP_MODE_CONFIRM_EXIT:
		return APP_SCREEN_MODEL_CONFIRM;
	case APP_MODE_REVIEW:
		break;
	}

	return APP_SCREEN_MODEL_REVIEW;
}

enum app_confirm_action_kind app_mode_confirm_action_kind(enum app_mode mode)
{
	switch (mode)
	{
	case APP_MODE_CONFIRM_RESTORE:
		return APP_CONFIRM_ACTION_RESTORE;
	case APP_MODE_CONFIRM_RESET:
		return APP_CONFIRM_ACTION_RESET;
	case APP_MODE_CONFIRM_EXIT:
		return APP_CONFIRM_ACTION_EXIT;
	case APP_MODE_DECK_SELECT:
	case APP_MODE_REVIEW:
	case APP_MODE_CONFIRM_SUSPEND:
	case APP_MODE_SETTINGS:
		break;
	}

	return APP_CONFIRM_ACTION_SUSPEND;
}
