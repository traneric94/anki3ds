#include "app_mode.h"

#include <assert.h>

static void test_confirm_predicate(void)
{
	assert(!app_mode_is_confirm(APP_MODE_DECK_SELECT));
	assert(!app_mode_is_confirm(APP_MODE_REVIEW));
	assert(!app_mode_is_confirm(APP_MODE_SETTINGS));
	assert(app_mode_is_confirm(APP_MODE_CONFIRM_SUSPEND));
	assert(app_mode_is_confirm(APP_MODE_CONFIRM_RESTORE));
	assert(app_mode_is_confirm(APP_MODE_CONFIRM_RESET));
	assert(app_mode_is_confirm(APP_MODE_CONFIRM_EXIT));
	assert(!app_mode_is_confirm((enum app_mode)99));
}

static void test_screen_model_mapping(void)
{
	assert(
		app_mode_screen_model_kind(APP_MODE_DECK_SELECT) ==
		APP_SCREEN_MODEL_DECK_SELECT
	);
	assert(
		app_mode_screen_model_kind(APP_MODE_REVIEW) ==
		APP_SCREEN_MODEL_REVIEW
	);
	assert(
		app_mode_screen_model_kind(APP_MODE_SETTINGS) ==
		APP_SCREEN_MODEL_SETTINGS
	);
	assert(
		app_mode_screen_model_kind(APP_MODE_CONFIRM_SUSPEND) ==
		APP_SCREEN_MODEL_CONFIRM
	);
	assert(
		app_mode_screen_model_kind(APP_MODE_CONFIRM_RESTORE) ==
		APP_SCREEN_MODEL_CONFIRM
	);
	assert(
		app_mode_screen_model_kind(APP_MODE_CONFIRM_RESET) ==
		APP_SCREEN_MODEL_CONFIRM
	);
	assert(
		app_mode_screen_model_kind(APP_MODE_CONFIRM_EXIT) ==
		APP_SCREEN_MODEL_CONFIRM
	);
	assert(
		app_mode_screen_model_kind((enum app_mode)99) ==
		APP_SCREEN_MODEL_REVIEW
	);
}

static void test_confirm_contract_mapping(void)
{
	assert(
		app_mode_confirm_contract_kind(APP_MODE_CONFIRM_SUSPEND) ==
		APP_CONFIRM_CONTRACT_SUSPEND
	);
	assert(
		app_mode_confirm_contract_kind(APP_MODE_CONFIRM_RESTORE) ==
		APP_CONFIRM_CONTRACT_RESTORE
	);
	assert(
		app_mode_confirm_contract_kind(APP_MODE_CONFIRM_RESET) ==
		APP_CONFIRM_CONTRACT_RESET
	);
	assert(
		app_mode_confirm_contract_kind(APP_MODE_CONFIRM_EXIT) ==
		APP_CONFIRM_CONTRACT_EXIT
	);
	assert(
		app_mode_confirm_contract_kind(APP_MODE_REVIEW) ==
		APP_CONFIRM_CONTRACT_SUSPEND
	);
	assert(
		app_mode_confirm_contract_kind((enum app_mode)99) ==
		APP_CONFIRM_CONTRACT_SUSPEND
	);
}

static void test_confirm_action_mapping(void)
{
	assert(
		app_mode_confirm_action_kind(APP_MODE_CONFIRM_SUSPEND) ==
		APP_CONFIRM_ACTION_SUSPEND
	);
	assert(
		app_mode_confirm_action_kind(APP_MODE_CONFIRM_RESTORE) ==
		APP_CONFIRM_ACTION_RESTORE
	);
	assert(
		app_mode_confirm_action_kind(APP_MODE_CONFIRM_RESET) ==
		APP_CONFIRM_ACTION_RESET
	);
	assert(
		app_mode_confirm_action_kind(APP_MODE_CONFIRM_EXIT) ==
		APP_CONFIRM_ACTION_EXIT
	);
	assert(
		app_mode_confirm_action_kind(APP_MODE_DECK_SELECT) ==
		APP_CONFIRM_ACTION_SUSPEND
	);
	assert(
		app_mode_confirm_action_kind((enum app_mode)99) ==
		APP_CONFIRM_ACTION_SUSPEND
	);
}

int main(void)
{
	test_confirm_predicate();
	test_screen_model_mapping();
	test_confirm_contract_mapping();
	test_confirm_action_mapping();
	return 0;
}
