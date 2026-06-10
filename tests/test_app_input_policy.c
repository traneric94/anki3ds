#include "app_input_policy.h"

#include "study_controls.h"

#include <assert.h>

static void test_deck_selector_repeats_all_navigation(void)
{
	assert(
		app_input_policy_repeat_mask(APP_SCREEN_MODEL_DECK_SELECT) ==
		(
			STUDY_CONTROL_BUTTON_UP |
			STUDY_CONTROL_BUTTON_DOWN |
			STUDY_CONTROL_BUTTON_LEFT |
			STUDY_CONTROL_BUTTON_RIGHT |
			STUDY_CONTROL_BUTTON_L |
			STUDY_CONTROL_BUTTON_R
		)
	);
}

static void test_review_repeats_vertical_scroll_only(void)
{
	assert(
		app_input_policy_repeat_mask(APP_SCREEN_MODEL_REVIEW) ==
		(STUDY_CONTROL_BUTTON_UP | STUDY_CONTROL_BUTTON_DOWN)
	);
}

static void test_settings_repeats_value_changes_only(void)
{
	assert(
		app_input_policy_repeat_mask(APP_SCREEN_MODEL_SETTINGS) ==
		(STUDY_CONTROL_BUTTON_LEFT | STUDY_CONTROL_BUTTON_RIGHT)
	);
}

static void test_confirmation_screens_do_not_repeat(void)
{
	assert(app_input_policy_repeat_mask(APP_SCREEN_MODEL_CONFIRM) == 0);
}

static void test_unknown_screen_kind_is_not_repeatable(void)
{
	assert(app_input_policy_repeat_mask((enum app_screen_model_kind)99) == 0);
}

int main(void)
{
	test_deck_selector_repeats_all_navigation();
	test_review_repeats_vertical_scroll_only();
	test_settings_repeats_value_changes_only();
	test_confirmation_screens_do_not_repeat();
	test_unknown_screen_kind_is_not_repeatable();
	return 0;
}
