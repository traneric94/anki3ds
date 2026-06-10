#include "app_deck_navigation.h"

size_t app_deck_navigation_visible_start(
	size_t deck_count,
	size_t selected_index,
	size_t visible_rows
)
{
	size_t start;

	if (deck_count == 0 || visible_rows == 0 || deck_count <= visible_rows)
		return 0;
	if (selected_index >= deck_count)
		selected_index = deck_count - 1;
	if (selected_index < visible_rows / 2)
		return 0;

	start = selected_index - visible_rows / 2;
	if (start + visible_rows > deck_count)
		start = deck_count - visible_rows;

	return start;
}

bool app_deck_navigation_apply(
	size_t deck_count,
	size_t visible_rows,
	size_t *selected_index,
	enum app_deck_navigation_action action
)
{
	size_t old_index;
	size_t page_size;

	if (selected_index == NULL || deck_count == 0)
		return false;

	old_index = *selected_index;
	if (*selected_index >= deck_count)
		*selected_index = deck_count - 1;
	page_size = visible_rows > 0 ? visible_rows : 1;

	switch (action)
	{
	case APP_DECK_NAVIGATION_PREVIOUS:
		*selected_index =
			*selected_index == 0 ? deck_count - 1 : *selected_index - 1;
		break;
	case APP_DECK_NAVIGATION_NEXT:
		*selected_index =
			*selected_index + 1 >= deck_count ? 0 : *selected_index + 1;
		break;
	case APP_DECK_NAVIGATION_PAGE_PREVIOUS:
		if (*selected_index == 0)
			*selected_index = deck_count - 1;
		else if (*selected_index > page_size)
			*selected_index -= page_size;
		else
			*selected_index = 0;
		break;
	case APP_DECK_NAVIGATION_PAGE_NEXT:
		if (*selected_index + 1 >= deck_count)
			*selected_index = 0;
		else if (*selected_index + page_size < deck_count)
			*selected_index += page_size;
		else
			*selected_index = deck_count - 1;
		break;
	}

	return *selected_index != old_index;
}
