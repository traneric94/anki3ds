#include "app_confirm_action.h"

#include "study_controls.h"

#include <stdbool.h>

static bool app_confirm_action_has_single_button(unsigned int buttons)
{
	return buttons != 0 && (buttons & (buttons - 1)) == 0;
}

static bool app_confirm_action_kind_is_known(enum app_confirm_action_kind kind)
{
	switch (kind)
	{
	case APP_CONFIRM_ACTION_SUSPEND:
	case APP_CONFIRM_ACTION_RESTORE:
	case APP_CONFIRM_ACTION_RESET:
	case APP_CONFIRM_ACTION_EXIT:
		return true;
	}

	return false;
}

enum app_confirm_action_result app_confirm_action_apply(
	enum app_confirm_action_kind kind,
	unsigned int buttons
)
{
	if (!app_confirm_action_has_single_button(buttons))
		return APP_CONFIRM_ACTION_NONE;

	if (
		buttons == STUDY_CONTROL_BUTTON_B ||
		buttons == STUDY_CONTROL_BUTTON_SELECT
	)
	{
		return APP_CONFIRM_ACTION_CANCEL;
	}
	if (!app_confirm_action_kind_is_known(kind))
		return APP_CONFIRM_ACTION_NONE;
	if (buttons == STUDY_CONTROL_BUTTON_START)
	{
		return kind == APP_CONFIRM_ACTION_EXIT ?
			APP_CONFIRM_ACTION_NONE :
			APP_CONFIRM_ACTION_OPEN_EXIT;
	}
	if (
		buttons == STUDY_CONTROL_BUTTON_X &&
		kind != APP_CONFIRM_ACTION_EXIT
	)
	{
		return APP_CONFIRM_ACTION_CONFIRM;
	}
	if (
		buttons == STUDY_CONTROL_BUTTON_A &&
		kind == APP_CONFIRM_ACTION_EXIT
	)
	{
		return APP_CONFIRM_ACTION_CONFIRM;
	}

	return APP_CONFIRM_ACTION_NONE;
}

const char *app_confirm_action_cancel_status(
	enum app_confirm_action_kind kind
)
{
	switch (kind)
	{
	case APP_CONFIRM_ACTION_EXIT:
		return "Exit canceled";
	case APP_CONFIRM_ACTION_RESET:
		return "Reset canceled";
	case APP_CONFIRM_ACTION_RESTORE:
		return "Restore canceled";
	case APP_CONFIRM_ACTION_SUSPEND:
		return "Suspend canceled";
	}

	return "Canceled";
}
