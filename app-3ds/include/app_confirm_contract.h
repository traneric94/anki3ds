#ifndef ANKI3DS_APP_CONFIRM_CONTRACT_H
#define ANKI3DS_APP_CONFIRM_CONTRACT_H

#include "study_backend.h"

#define APP_CONFIRM_STATUS_TEXT_SIZE STUDY_BACKEND_STATUS_SIZE
#define APP_CONFIRM_PROMPT_TEXT_SIZE 160
#define APP_CONFIRM_FOOTER_TEXT_SIZE 80

enum app_confirm_contract_kind
{
	APP_CONFIRM_CONTRACT_SUSPEND,
	APP_CONFIRM_CONTRACT_RESTORE,
	APP_CONFIRM_CONTRACT_RESET,
	APP_CONFIRM_CONTRACT_EXIT,
};

struct app_confirm_contract
{
	char status_text[APP_CONFIRM_STATUS_TEXT_SIZE];
	char prompt_text[APP_CONFIRM_PROMPT_TEXT_SIZE];
	char footer_text[APP_CONFIRM_FOOTER_TEXT_SIZE];
};

void app_confirm_contract_build(
	struct app_confirm_contract *contract,
	enum app_confirm_contract_kind kind,
	const char *status_text,
	unsigned int suspended_count
);

#endif
