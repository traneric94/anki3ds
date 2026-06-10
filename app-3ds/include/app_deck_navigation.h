#ifndef ANKI3DS_APP_DECK_NAVIGATION_H
#define ANKI3DS_APP_DECK_NAVIGATION_H

#include <stdbool.h>
#include <stddef.h>

enum app_deck_navigation_action
{
	APP_DECK_NAVIGATION_PREVIOUS,
	APP_DECK_NAVIGATION_NEXT,
	APP_DECK_NAVIGATION_PAGE_PREVIOUS,
	APP_DECK_NAVIGATION_PAGE_NEXT,
};

size_t app_deck_navigation_visible_start(
	size_t deck_count,
	size_t selected_index,
	size_t visible_rows
);
bool app_deck_navigation_apply(
	size_t deck_count,
	size_t visible_rows,
	size_t *selected_index,
	enum app_deck_navigation_action action
);

#endif
