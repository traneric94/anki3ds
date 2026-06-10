#include "app_confirm_contract.h"

#include <stdio.h>
#include <string.h>

static void app_confirm_contract_copy_string(
	char *destination,
	size_t destination_size,
	const char *source
)
{
	if (destination == NULL || destination_size == 0)
		return;

	snprintf(destination, destination_size, "%s", source != NULL ? source : "");
}

void app_confirm_contract_build(
	struct app_confirm_contract *contract,
	enum app_confirm_contract_kind kind,
	const char *status_text,
	unsigned int suspended_count
)
{
	if (contract == NULL)
		return;

	memset(contract, 0, sizeof(*contract));
	app_confirm_contract_copy_string(
		contract->status_text,
		sizeof(contract->status_text),
		status_text
	);
	switch (kind)
	{
	case APP_CONFIRM_CONTRACT_RESET:
		snprintf(
			contract->prompt_text,
			sizeof(contract->prompt_text),
			"Reset deck progress?\nX: confirm   B: cancel"
		);
		break;
	case APP_CONFIRM_CONTRACT_EXIT:
		snprintf(
			contract->prompt_text,
			sizeof(contract->prompt_text),
			"Exit anki3ds?\nA: confirm   B: cancel"
		);
		break;
	case APP_CONFIRM_CONTRACT_RESTORE:
		snprintf(
			contract->prompt_text,
			sizeof(contract->prompt_text),
			"Restore %u suspended card%s?\nX: confirm   B: cancel",
			suspended_count,
			suspended_count == 1 ? "" : "s"
		);
		break;
	case APP_CONFIRM_CONTRACT_SUSPEND:
		snprintf(
			contract->prompt_text,
			sizeof(contract->prompt_text),
			"Suspend current card?\nX: confirm   B: cancel"
		);
		break;
	}
	app_confirm_contract_copy_string(
		contract->footer_text,
		sizeof(contract->footer_text),
		"SELECT: cancel"
	);
}
