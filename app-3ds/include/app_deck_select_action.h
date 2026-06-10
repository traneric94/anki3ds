#ifndef ANKI3DS_APP_DECK_SELECT_ACTION_H
#define ANKI3DS_APP_DECK_SELECT_ACTION_H

#include <stdbool.h>
#include <stddef.h>

enum app_deck_select_action_result
{
	APP_DECK_SELECT_ACTION_NONE,
	APP_DECK_SELECT_ACTION_UPDATED,
	APP_DECK_SELECT_ACTION_OPEN_DECK,
	APP_DECK_SELECT_ACTION_RESCAN,
	APP_DECK_SELECT_ACTION_EXIT,
};

enum app_deck_select_action_result app_deck_select_action_apply(
	unsigned int buttons,
	size_t deck_count,
	size_t *selected_index
);

#endif
