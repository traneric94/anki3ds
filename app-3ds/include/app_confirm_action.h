#ifndef ANKI3DS_APP_CONFIRM_ACTION_H
#define ANKI3DS_APP_CONFIRM_ACTION_H

enum app_confirm_action_kind
{
	APP_CONFIRM_ACTION_SUSPEND,
	APP_CONFIRM_ACTION_RESTORE,
	APP_CONFIRM_ACTION_RESET,
	APP_CONFIRM_ACTION_EXIT,
};

enum app_confirm_action_result
{
	APP_CONFIRM_ACTION_NONE,
	APP_CONFIRM_ACTION_OPEN_EXIT,
	APP_CONFIRM_ACTION_CANCEL,
	APP_CONFIRM_ACTION_CONFIRM,
};

enum app_confirm_action_result app_confirm_action_apply(
	enum app_confirm_action_kind kind,
	unsigned int buttons
);
const char *app_confirm_action_cancel_status(
	enum app_confirm_action_kind kind
);

#endif
