#ifndef ANKI3DS_APP_MODE_H
#define ANKI3DS_APP_MODE_H

#include <stdbool.h>

#include "app_confirm_action.h"
#include "app_confirm_contract.h"
#include "app_screen_model.h"

enum app_mode
{
	APP_MODE_DECK_SELECT,
	APP_MODE_REVIEW,
	APP_MODE_CONFIRM_SUSPEND,
	APP_MODE_CONFIRM_RESTORE,
	APP_MODE_CONFIRM_RESET,
	APP_MODE_CONFIRM_EXIT,
	APP_MODE_SETTINGS,
};

bool app_mode_is_confirm(enum app_mode mode);
enum app_confirm_contract_kind app_mode_confirm_contract_kind(enum app_mode mode);
enum app_screen_model_kind app_mode_screen_model_kind(enum app_mode mode);
enum app_confirm_action_kind app_mode_confirm_action_kind(enum app_mode mode);

#endif
