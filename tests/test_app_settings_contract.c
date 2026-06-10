#include "app_settings_contract.h"

#include <assert.h>
#include <string.h>

static void test_settings_contract_formats_selected_new_limit(void)
{
	struct study_settings settings;
	struct app_settings_contract contract;

	study_settings_defaults(&settings);
	settings.new_limit = 5;
	settings.review_limit = 20;
	app_settings_contract_build(&contract, &settings, 0, "Edit settings");

	assert(strcmp(contract.title_text, "Study settings") == 0);
	assert(strstr(contract.body_text, "> New limit: 5") != NULL);
	assert(strstr(contract.body_text, "  Review limit: 20") != NULL);
	assert(strstr(contract.body_text, "  Learning: Due first") != NULL);
	assert(strcmp(contract.status_text, "Edit settings") == 0);
	assert(strstr(contract.controls_text, "X: save") != NULL);
	assert(strstr(contract.controls_text, "B: cancel") != NULL);
	assert(strstr(contract.controls_text, "↑/↓: field") != NULL);
	assert(strstr(contract.controls_text, "←/→: change") != NULL);
	assert(strstr(contract.controls_text, "SELECT: cancel") != NULL);
	assert(strstr(contract.controls_text, "START: exit") != NULL);
	assert(strstr(contract.footer_text, "unlimited") != NULL);
	assert(strstr(contract.help_text, "review caps ratings") != NULL);
}

static void test_settings_contract_formats_selected_review_limit(void)
{
	struct study_settings settings;
	struct app_settings_contract contract;

	study_settings_defaults(&settings);
	settings.new_limit = 10;
	settings.review_limit = 0;
	app_settings_contract_build(&contract, &settings, 1, "Settings changed");

	assert(strstr(contract.body_text, "  New limit: 10") != NULL);
	assert(strstr(contract.body_text, "> Review limit: All") != NULL);
	assert(strcmp(contract.status_text, "Settings changed") == 0);
}

static void test_settings_contract_formats_selected_learning_mode(void)
{
	struct study_settings settings;
	struct app_settings_contract contract;

	study_settings_defaults(&settings);
	settings.learning_mode = STUDY_SETTINGS_LEARNING_CARD_COOLDOWN;
	app_settings_contract_build(&contract, &settings, 2, "Mode changed");

	assert(strstr(contract.body_text, "  New limit: 20") != NULL);
	assert(strstr(contract.body_text, "  Review limit: 200") != NULL);
	assert(strstr(contract.body_text, "> Learning: Cooldown") != NULL);
	assert(strstr(contract.footer_text, "Cooldown") != NULL);
}

static void test_settings_contract_uses_defaults_for_missing_settings(void)
{
	struct app_settings_contract contract;

	app_settings_contract_build(&contract, NULL, 0, NULL);

	assert(strstr(contract.body_text, "> New limit: 20") != NULL);
	assert(strstr(contract.body_text, "  Review limit: 200") != NULL);
	assert(strstr(contract.body_text, "  Learning: Due first") != NULL);
	assert(strcmp(contract.status_text, "") == 0);
}

static void test_settings_contract_clamps_invalid_selected_index(void)
{
	struct study_settings settings;
	struct app_settings_contract contract;

	study_settings_defaults(&settings);
	settings.new_limit = 0;
	settings.review_limit = 100;
	app_settings_contract_build(&contract, &settings, 99, "Status");

	assert(strstr(contract.body_text, "> New limit: All") != NULL);
	assert(strstr(contract.body_text, "  Review limit: 100") != NULL);
}

int main(void)
{
	test_settings_contract_formats_selected_new_limit();
	test_settings_contract_formats_selected_review_limit();
	test_settings_contract_formats_selected_learning_mode();
	test_settings_contract_uses_defaults_for_missing_settings();
	test_settings_contract_clamps_invalid_selected_index();
	return 0;
}
