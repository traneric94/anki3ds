#ifndef ANKI3DS_APP_SETTINGS_CONTRACT_H
#define ANKI3DS_APP_SETTINGS_CONTRACT_H

#include <stddef.h>

#include "study_backend.h"
#include "study_settings.h"

#define APP_SETTINGS_TITLE_TEXT_SIZE 32
#define APP_SETTINGS_BODY_TEXT_SIZE 256
#define APP_SETTINGS_FOOTER_TEXT_SIZE 80
#define APP_SETTINGS_STATUS_TEXT_SIZE STUDY_BACKEND_STATUS_SIZE
#define APP_SETTINGS_CONTROLS_TEXT_SIZE 160
#define APP_SETTINGS_HELP_TEXT_SIZE 96

struct app_settings_contract
{
	char title_text[APP_SETTINGS_TITLE_TEXT_SIZE];
	char body_text[APP_SETTINGS_BODY_TEXT_SIZE];
	char footer_text[APP_SETTINGS_FOOTER_TEXT_SIZE];
	char status_text[APP_SETTINGS_STATUS_TEXT_SIZE];
	char controls_text[APP_SETTINGS_CONTROLS_TEXT_SIZE];
	char help_text[APP_SETTINGS_HELP_TEXT_SIZE];
};

void app_settings_contract_build(
	struct app_settings_contract *contract,
	const struct study_settings *settings,
	size_t selected_index,
	const char *status_text
);

#endif
