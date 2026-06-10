#include "app_settings_contract.h"

#include <stdio.h>
#include <string.h>

static void app_settings_contract_copy_string(
	char *destination,
	size_t destination_size,
	const char *source
)
{
	if (destination == NULL || destination_size == 0)
		return;

	snprintf(destination, destination_size, "%s", source != NULL ? source : "");
}

static void app_settings_contract_format_limit_value(
	char *destination,
	size_t destination_size,
	unsigned int value
)
{
	if (destination == NULL || destination_size == 0)
		return;

	if (value == 0)
	{
		snprintf(destination, destination_size, "All");
		return;
	}

	snprintf(destination, destination_size, "%u", value);
}

static void app_settings_contract_build_body_text(
	char *destination,
	size_t destination_size,
	const struct study_settings *settings,
	size_t selected_index
)
{
	char new_limit[16];
	char review_limit[16];
	enum study_settings_learning_mode learning_mode =
		settings != NULL ?
			settings->learning_mode :
			STUDY_SETTINGS_DEFAULT_LEARNING_MODE;
	size_t safe_selected_index = selected_index <= 2 ? selected_index : 0;

	if (destination == NULL || destination_size == 0)
		return;

	app_settings_contract_format_limit_value(
		new_limit,
		sizeof(new_limit),
		settings != NULL ?
			settings->new_limit :
			STUDY_SETTINGS_DEFAULT_NEW_LIMIT
	);
	app_settings_contract_format_limit_value(
		review_limit,
		sizeof(review_limit),
		settings != NULL ?
			settings->review_limit :
			STUDY_SETTINGS_DEFAULT_REVIEW_LIMIT
	);
	snprintf(
		destination,
		destination_size,
		"%c New limit: %s\n"
		"%c Review limit: %s\n"
		"%c Learning: %s",
		safe_selected_index == 0 ? '>' : ' ',
		new_limit,
		safe_selected_index == 1 ? '>' : ' ',
		review_limit,
		safe_selected_index == 2 ? '>' : ' ',
		study_settings_learning_mode_label(learning_mode)
	);
}

void app_settings_contract_build(
	struct app_settings_contract *contract,
	const struct study_settings *settings,
	size_t selected_index,
	const char *status_text
)
{
	if (contract == NULL)
		return;

	memset(contract, 0, sizeof(*contract));
	app_settings_contract_copy_string(
		contract->title_text,
		sizeof(contract->title_text),
		"Study settings"
	);
	app_settings_contract_build_body_text(
		contract->body_text,
		sizeof(contract->body_text),
		settings,
		selected_index
	);
	app_settings_contract_copy_string(
		contract->footer_text,
		sizeof(contract->footer_text),
		"All = unlimited. Cooldown spaces repeats."
	);
	app_settings_contract_copy_string(
		contract->status_text,
		sizeof(contract->status_text),
		status_text
	);
	app_settings_contract_copy_string(
		contract->controls_text,
		sizeof(contract->controls_text),
		"X: save   B: cancel\n"
		"↑/↓: field   ←/→: change\n"
		"SELECT: cancel   START: exit"
	);
	app_settings_contract_copy_string(
		contract->help_text,
		sizeof(contract->help_text),
		"New blocks reveal; review caps ratings; learning controls queue."
	);
}
