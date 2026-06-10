#include "app_deck_select_action.h"

#include "study_controls.h"

#include <assert.h>

static void test_deck_select_action_rejects_chords(void)
{
	size_t selected = 1;

	assert(
		app_deck_select_action_apply(
			STUDY_CONTROL_BUTTON_A | STUDY_CONTROL_BUTTON_START,
			3,
			&selected
		) == APP_DECK_SELECT_ACTION_NONE
	);
	assert(selected == 1);
}

static void test_deck_select_action_reports_commands(void)
{
	size_t selected = 0;

	assert(
		app_deck_select_action_apply(
			STUDY_CONTROL_BUTTON_Y,
			3,
			&selected
		) == APP_DECK_SELECT_ACTION_EXIT
	);
	assert(
		app_deck_select_action_apply(
			STUDY_CONTROL_BUTTON_SELECT,
			3,
			&selected
		) == APP_DECK_SELECT_ACTION_RESCAN
	);
	assert(
		app_deck_select_action_apply(
			STUDY_CONTROL_BUTTON_A,
			3,
			&selected
		) == APP_DECK_SELECT_ACTION_OPEN_DECK
	);
}

static void test_deck_select_action_moves_selection(void)
{
	size_t selected = 0;

	assert(
		app_deck_select_action_apply(
			STUDY_CONTROL_BUTTON_UP,
			5,
			&selected
		) == APP_DECK_SELECT_ACTION_UPDATED
	);
	assert(selected == 4);
	assert(
		app_deck_select_action_apply(
			STUDY_CONTROL_BUTTON_DOWN,
			5,
			&selected
		) == APP_DECK_SELECT_ACTION_UPDATED
	);
	assert(selected == 0);
}

static void test_deck_select_action_pages_selection(void)
{
	size_t selected = 2;

	assert(
		app_deck_select_action_apply(
			STUDY_CONTROL_BUTTON_RIGHT,
			10,
			&selected
		) == APP_DECK_SELECT_ACTION_UPDATED
	);
	assert(selected == 8);
	assert(
		app_deck_select_action_apply(
			STUDY_CONTROL_BUTTON_LEFT,
			10,
			&selected
		) == APP_DECK_SELECT_ACTION_UPDATED
	);
	assert(selected == 2);
}

static void test_deck_select_action_pages_selection_with_shoulders(void)
{
	size_t selected = 2;

	assert(
		app_deck_select_action_apply(
			STUDY_CONTROL_BUTTON_R,
			10,
			&selected
		) == APP_DECK_SELECT_ACTION_UPDATED
	);
	assert(selected == 8);
	assert(
		app_deck_select_action_apply(
			STUDY_CONTROL_BUTTON_L,
			10,
			&selected
		) == APP_DECK_SELECT_ACTION_UPDATED
	);
	assert(selected == 2);
}

static void test_deck_select_action_ignores_navigation_without_decks(void)
{
	size_t selected = 0;

	assert(
		app_deck_select_action_apply(
			STUDY_CONTROL_BUTTON_DOWN,
			0,
			&selected
		) == APP_DECK_SELECT_ACTION_NONE
	);
	assert(selected == 0);
}

static void test_deck_select_action_ignores_open_without_decks(void)
{
	size_t selected = 0;

	assert(
		app_deck_select_action_apply(
			STUDY_CONTROL_BUTTON_A,
			0,
			&selected
		) == APP_DECK_SELECT_ACTION_NONE
	);
	assert(selected == 0);
}

int main(void)
{
	test_deck_select_action_rejects_chords();
	test_deck_select_action_reports_commands();
	test_deck_select_action_moves_selection();
	test_deck_select_action_pages_selection();
	test_deck_select_action_pages_selection_with_shoulders();
	test_deck_select_action_ignores_navigation_without_decks();
	test_deck_select_action_ignores_open_without_decks();
	return 0;
}
