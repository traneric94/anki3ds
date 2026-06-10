#include "app_confirm_contract.h"

#include <assert.h>
#include <string.h>

static void test_suspend_confirmation_strings(void)
{
	struct app_confirm_contract contract;

	app_confirm_contract_build(
		&contract,
		APP_CONFIRM_CONTRACT_SUSPEND,
		"Question",
		0
	);

	assert(strcmp(contract.status_text, "Question") == 0);
	assert(strcmp(
		contract.prompt_text,
		"Suspend current card?\nX: confirm   B: cancel"
	) == 0);
	assert(strcmp(contract.footer_text, "SELECT: cancel") == 0);
}

static void test_restore_confirmation_pluralizes(void)
{
	struct app_confirm_contract contract;

	app_confirm_contract_build(
		&contract,
		APP_CONFIRM_CONTRACT_RESTORE,
		"No active cards",
		2
	);
	assert(strcmp(
		contract.prompt_text,
		"Restore 2 suspended cards?\nX: confirm   B: cancel"
	) == 0);

	app_confirm_contract_build(
		&contract,
		APP_CONFIRM_CONTRACT_RESTORE,
		"No active cards",
		1
	);
	assert(strcmp(
		contract.prompt_text,
		"Restore 1 suspended card?\nX: confirm   B: cancel"
	) == 0);
}

static void test_reset_and_exit_confirmation_strings(void)
{
	struct app_confirm_contract contract;

	app_confirm_contract_build(
		&contract,
		APP_CONFIRM_CONTRACT_RESET,
		"Reset ready",
		0
	);
	assert(strcmp(
		contract.prompt_text,
		"Reset deck progress?\nX: confirm   B: cancel"
	) == 0);

	app_confirm_contract_build(
		&contract,
		APP_CONFIRM_CONTRACT_EXIT,
		"Exit ready",
		0
	);
	assert(strcmp(contract.status_text, "Exit ready") == 0);
	assert(strcmp(
		contract.prompt_text,
		"Exit anki3ds?\nA: confirm   B: cancel"
	) == 0);
}

static void test_null_status_is_empty(void)
{
	struct app_confirm_contract contract;

	app_confirm_contract_build(
		&contract,
		APP_CONFIRM_CONTRACT_EXIT,
		NULL,
		0
	);

	assert(strcmp(contract.status_text, "") == 0);
}

int main(void)
{
	test_suspend_confirmation_strings();
	test_restore_confirmation_pluralizes();
	test_reset_and_exit_confirmation_strings();
	test_null_status_is_empty();
	return 0;
}
