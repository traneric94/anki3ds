#ifndef ANKI3DS_APP_DECK_SELECT_CONTRACT_H
#define ANKI3DS_APP_DECK_SELECT_CONTRACT_H

#include <stdbool.h>
#include <stddef.h>

#include "study_backend.h"
#include "study_deck_index.h"

#define APP_DECK_SELECT_TITLE_TEXT_SIZE 32
#define APP_DECK_SELECT_LIST_TEXT_SIZE 512
#define APP_DECK_SELECT_META_TEXT_SIZE 96
#define APP_DECK_SELECT_STATUS_TEXT_SIZE STUDY_BACKEND_STATUS_SIZE
#define APP_DECK_SELECT_CONTROLS_TEXT_SIZE 160
#define APP_DECK_SELECT_FOOTER_TEXT_SIZE 160

struct app_deck_select_contract
{
	char title_text[APP_DECK_SELECT_TITLE_TEXT_SIZE];
	char list_text[APP_DECK_SELECT_LIST_TEXT_SIZE];
	char meta_text[APP_DECK_SELECT_META_TEXT_SIZE];
	char status_text[APP_DECK_SELECT_STATUS_TEXT_SIZE];
	char controls_text[APP_DECK_SELECT_CONTROLS_TEXT_SIZE];
	char footer_text[APP_DECK_SELECT_FOOTER_TEXT_SIZE];
};

void app_deck_select_contract_build(
	struct app_deck_select_contract *contract,
	const struct study_deck_index *index,
	size_t selected_deck_index,
	const char *status_text,
	bool help_visible
);

void app_deck_select_contract_build_scan_status(
	const struct study_deck_index *index,
	char *destination,
	size_t destination_size
);

#endif
