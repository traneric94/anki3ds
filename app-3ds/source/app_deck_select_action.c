#include "app_deck_select_action.h"

#include "app_deck_navigation.h"
#include "study_controls.h"

#define APP_DECK_SELECT_ACTION_PAGE_ROWS 6

static bool app_deck_select_action_has_single_button(unsigned int buttons)
{
	return buttons != 0 && (buttons & (buttons - 1)) == 0;
}

enum app_deck_select_action_result app_deck_select_action_apply(
	unsigned int buttons,
	size_t deck_count,
	size_t *selected_index
)
{
	enum app_deck_navigation_action navigation_action;

	if (!app_deck_select_action_has_single_button(buttons))
		return APP_DECK_SELECT_ACTION_NONE;

	switch (buttons)
	{
	case STUDY_CONTROL_BUTTON_Y:
		return APP_DECK_SELECT_ACTION_EXIT;
	case STUDY_CONTROL_BUTTON_SELECT:
		return APP_DECK_SELECT_ACTION_RESCAN;
	case STUDY_CONTROL_BUTTON_A:
		return deck_count > 0 ?
			APP_DECK_SELECT_ACTION_OPEN_DECK :
			APP_DECK_SELECT_ACTION_NONE;
	case STUDY_CONTROL_BUTTON_UP:
		navigation_action = APP_DECK_NAVIGATION_PREVIOUS;
		break;
	case STUDY_CONTROL_BUTTON_DOWN:
		navigation_action = APP_DECK_NAVIGATION_NEXT;
		break;
	case STUDY_CONTROL_BUTTON_LEFT:
	case STUDY_CONTROL_BUTTON_L:
		navigation_action = APP_DECK_NAVIGATION_PAGE_PREVIOUS;
		break;
	case STUDY_CONTROL_BUTTON_RIGHT:
	case STUDY_CONTROL_BUTTON_R:
		navigation_action = APP_DECK_NAVIGATION_PAGE_NEXT;
		break;
	default:
		return APP_DECK_SELECT_ACTION_NONE;
	}

	return app_deck_navigation_apply(
		deck_count,
		APP_DECK_SELECT_ACTION_PAGE_ROWS,
		selected_index,
		navigation_action
	) ?
		APP_DECK_SELECT_ACTION_UPDATED :
		APP_DECK_SELECT_ACTION_NONE;
}
