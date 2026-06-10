#include "app_deck_navigation.h"

#include <assert.h>

static void test_visible_start_tracks_selected_deck(void)
{
	assert(app_deck_navigation_visible_start(0, 0, 6) == 0);
	assert(app_deck_navigation_visible_start(4, 3, 6) == 0);
	assert(app_deck_navigation_visible_start(10, 0, 6) == 0);
	assert(app_deck_navigation_visible_start(10, 3, 6) == 0);
	assert(app_deck_navigation_visible_start(10, 4, 6) == 1);
	assert(app_deck_navigation_visible_start(10, 9, 6) == 4);
	assert(app_deck_navigation_visible_start(10, 999, 6) == 4);
}

static void test_single_step_navigation_wraps(void)
{
	size_t selected = 0;

	assert(
		app_deck_navigation_apply(
			5,
			6,
			&selected,
			APP_DECK_NAVIGATION_PREVIOUS
		)
	);
	assert(selected == 4);
	assert(
		app_deck_navigation_apply(
			5,
			6,
			&selected,
			APP_DECK_NAVIGATION_NEXT
		)
	);
	assert(selected == 0);
}

static void test_page_navigation_clamps_and_wraps_edges(void)
{
	size_t selected = 2;

	assert(
		app_deck_navigation_apply(
			10,
			6,
			&selected,
			APP_DECK_NAVIGATION_PAGE_NEXT
		)
	);
	assert(selected == 8);
	assert(
		app_deck_navigation_apply(
			10,
			6,
			&selected,
			APP_DECK_NAVIGATION_PAGE_NEXT
		)
	);
	assert(selected == 9);
	assert(
		app_deck_navigation_apply(
			10,
			6,
			&selected,
			APP_DECK_NAVIGATION_PAGE_NEXT
		)
	);
	assert(selected == 0);
	assert(
		app_deck_navigation_apply(
			10,
			6,
			&selected,
			APP_DECK_NAVIGATION_PAGE_PREVIOUS
		)
	);
	assert(selected == 9);
	assert(
		app_deck_navigation_apply(
			10,
			6,
			&selected,
			APP_DECK_NAVIGATION_PAGE_PREVIOUS
		)
	);
	assert(selected == 3);
	assert(
		app_deck_navigation_apply(
			10,
			6,
			&selected,
			APP_DECK_NAVIGATION_PAGE_PREVIOUS
		)
	);
	assert(selected == 0);
}

static void test_navigation_handles_empty_and_single_deck_lists(void)
{
	size_t selected = 4;

	assert(
		!app_deck_navigation_apply(
			0,
			6,
			&selected,
			APP_DECK_NAVIGATION_NEXT
		)
	);
	assert(selected == 4);
	selected = 0;
	assert(
		!app_deck_navigation_apply(
			1,
			6,
			&selected,
			APP_DECK_NAVIGATION_NEXT
		)
	);
	assert(selected == 0);
}

int main(void)
{
	test_visible_start_tracks_selected_deck();
	test_single_step_navigation_wraps();
	test_page_navigation_clamps_and_wraps_edges();
	test_navigation_handles_empty_and_single_deck_lists();
	return 0;
}
