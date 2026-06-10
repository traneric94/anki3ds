#include "app_controls.h"

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>

static void check(bool condition, const char *message)
{
	if (!condition)
	{
		fprintf(stderr, "FAIL: %s\n", message);
		abort();
	}
}

static void test_maps_3ds_keys_to_app_buttons(void)
{
	check(
		app_controls_buttons_from_3ds_keys(KEY_A) == APP_CONTROL_BUTTON_A,
		"KEY_A maps to A"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_B) == APP_CONTROL_BUTTON_B,
		"KEY_B maps to B"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_X) == APP_CONTROL_BUTTON_X,
		"KEY_X maps to X"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_Y) == APP_CONTROL_BUTTON_Y,
		"KEY_Y maps to Y"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_L) == APP_CONTROL_BUTTON_L,
		"KEY_L maps to L"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_R) == APP_CONTROL_BUTTON_R,
		"KEY_R maps to R"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_DUP) == APP_CONTROL_BUTTON_UP,
		"KEY_DUP maps to Up"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_DDOWN) == APP_CONTROL_BUTTON_DOWN,
		"KEY_DDOWN maps to Down"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_DLEFT) == APP_CONTROL_BUTTON_LEFT,
		"KEY_DLEFT maps to Left"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_DRIGHT) == APP_CONTROL_BUTTON_RIGHT,
		"KEY_DRIGHT maps to Right"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_CPAD_UP) == APP_CONTROL_BUTTON_UP,
		"KEY_CPAD_UP maps to Up"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_CPAD_DOWN) == APP_CONTROL_BUTTON_DOWN,
		"KEY_CPAD_DOWN maps to Down"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_CPAD_LEFT) == APP_CONTROL_BUTTON_LEFT,
		"KEY_CPAD_LEFT maps to Left"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_CPAD_RIGHT) == APP_CONTROL_BUTTON_RIGHT,
		"KEY_CPAD_RIGHT maps to Right"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_UP) == APP_CONTROL_BUTTON_UP,
		"KEY_UP alias maps to Up"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_DOWN) == APP_CONTROL_BUTTON_DOWN,
		"KEY_DOWN alias maps to Down"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_LEFT) == APP_CONTROL_BUTTON_LEFT,
		"KEY_LEFT alias maps to Left"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_RIGHT) == APP_CONTROL_BUTTON_RIGHT,
		"KEY_RIGHT alias maps to Right"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_SELECT) == APP_CONTROL_BUTTON_SELECT,
		"KEY_SELECT maps to Select"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_START) == APP_CONTROL_BUTTON_START,
		"KEY_START maps to Start"
	);
}

static void test_maps_combined_3ds_keys_to_combined_app_buttons(void)
{
	unsigned int keys =
		KEY_A |
		KEY_B |
		KEY_X |
		KEY_Y |
		KEY_L |
		KEY_R |
		KEY_UP |
		KEY_DOWN |
		KEY_LEFT |
		KEY_RIGHT |
		KEY_SELECT |
		KEY_START;

	check(
		app_controls_buttons_from_3ds_keys(keys) ==
			APP_CONTROL_BUTTON_INPUT_MASK,
		"combined 3DS keys map to combined app buttons"
	);
}

static void check_revealed_rating_key(
	unsigned int key,
	enum scheduler_rating expected_rating,
	const char *message
)
{
	enum scheduler_rating rating = SCHEDULER_RATING_COUNT;
	unsigned int buttons = app_controls_buttons_from_3ds_keys(key);
	enum app_control_action action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		true,
		true,
		buttons,
		buttons,
		&rating
	);

	check(action == APP_CONTROL_ACTION_RATE, message);
	check(rating == expected_rating, message);
}

static void test_revealed_review_maps_3ds_face_keys_to_ratings(void)
{
	check_revealed_rating_key(
		KEY_A,
		SCHEDULER_RATING_AGAIN,
		"revealed KEY_A rates Again"
	);
	check_revealed_rating_key(
		KEY_B,
		SCHEDULER_RATING_HARD,
		"revealed KEY_B rates Hard"
	);
	check_revealed_rating_key(
		KEY_X,
		SCHEDULER_RATING_GOOD,
		"revealed KEY_X rates Good"
	);
	check_revealed_rating_key(
		KEY_Y,
		SCHEDULER_RATING_EASY,
		"revealed KEY_Y rates Easy"
	);
}

static void test_review_rating_context_and_chords_are_safe(void)
{
	enum scheduler_rating rating = SCHEDULER_RATING_COUNT;
	unsigned int buttons = app_controls_buttons_from_3ds_keys(KEY_A);
	enum app_control_action action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		true,
		buttons,
		buttons,
		&rating
	);

	check(action == APP_CONTROL_ACTION_SHOW_ANSWER, "unrevealed KEY_A shows answer");
	check(rating == SCHEDULER_RATING_COUNT, "unrevealed KEY_A does not rate");

	rating = SCHEDULER_RATING_COUNT;
	buttons = app_controls_buttons_from_3ds_keys(KEY_A | KEY_B);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		true,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "revealed rating chord is ignored");
	check(rating == SCHEDULER_RATING_COUNT, "revealed rating chord keeps rating unset");

	rating = SCHEDULER_RATING_COUNT;
	buttons = app_controls_buttons_from_3ds_keys(KEY_A);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		true,
		true,
		buttons,
		buttons | APP_CONTROL_BUTTON_L,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "revealed rating with held command is ignored");
	check(rating == SCHEDULER_RATING_COUNT, "held command keeps rating unset");
}

static void test_start_exit_mapping_and_chords_are_safe(void)
{
	enum scheduler_rating rating = SCHEDULER_RATING_COUNT;
	unsigned int buttons = app_controls_buttons_from_3ds_keys(KEY_START);
	enum app_control_action action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		true,
		buttons,
		buttons,
		&rating
	);

	check(action == APP_CONTROL_ACTION_OPEN_EXIT, "KEY_START opens exit");

	buttons = app_controls_buttons_from_3ds_keys(KEY_START | KEY_A);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "KEY_START with face button is ignored");

	buttons = app_controls_buttons_from_3ds_keys(KEY_START | KEY_DDOWN);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "KEY_START with D-pad is ignored");

	buttons = app_controls_buttons_from_3ds_keys(KEY_START);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_EXIT,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "KEY_START does not confirm exit");
}

static void test_review_actions_and_study_commands_are_safe(void)
{
	enum scheduler_rating rating = SCHEDULER_RATING_COUNT;
	unsigned int buttons = app_controls_buttons_from_3ds_keys(KEY_SELECT);
	enum app_control_action action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		true,
		buttons,
		buttons,
		&rating
	);

	check(action == APP_CONTROL_ACTION_OPEN_ACTIONS, "KEY_SELECT opens review actions");

	buttons = app_controls_buttons_from_3ds_keys(KEY_SELECT | KEY_A);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "KEY_SELECT with face button is ignored");

	buttons = app_controls_buttons_from_3ds_keys(KEY_L);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_UNDO, "KEY_L undoes when study is safe");

	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		false,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "KEY_L does not undo unsafe state");

	buttons = app_controls_buttons_from_3ds_keys(KEY_L | KEY_A);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "KEY_L with face button is ignored");

	buttons = app_controls_buttons_from_3ds_keys(KEY_R);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_OPEN_SUSPEND, "KEY_R opens suspend confirm");

	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		false,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "KEY_R does not suspend unsafe state");

	buttons = app_controls_buttons_from_3ds_keys(KEY_R | KEY_DDOWN);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "KEY_R with D-pad is ignored");
}

static void test_actions_and_settings_3ds_key_chords_are_safe(void)
{
	enum scheduler_rating rating = SCHEDULER_RATING_COUNT;
	unsigned int buttons = app_controls_buttons_from_3ds_keys(KEY_A);
	enum app_control_action action = app_controls_classify_action(
		APP_CONTROL_MODE_ACTIONS,
		false,
		true,
		buttons,
		buttons,
		&rating
	);

	check(action == APP_CONTROL_ACTION_CHOOSE_ACTION, "actions accepts KEY_A");

	buttons = app_controls_buttons_from_3ds_keys(KEY_A | KEY_DDOWN);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_ACTIONS,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "actions ignores A plus D-pad");

	buttons = app_controls_buttons_from_3ds_keys(KEY_B);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_ACTIONS,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CANCEL_ACTIONS, "actions accepts KEY_B");

	buttons = app_controls_buttons_from_3ds_keys(KEY_SELECT);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_ACTIONS,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CANCEL_ACTIONS, "actions accepts KEY_SELECT");

	buttons = app_controls_buttons_from_3ds_keys(KEY_A);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_SETTINGS,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_SAVE_SETTINGS, "settings accepts KEY_A");

	buttons = app_controls_buttons_from_3ds_keys(KEY_A | KEY_DRIGHT);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_SETTINGS,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "settings ignores A plus D-pad");

	buttons = app_controls_buttons_from_3ds_keys(KEY_B);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_SETTINGS,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CANCEL_SETTINGS, "settings accepts KEY_B");

	buttons = app_controls_buttons_from_3ds_keys(KEY_SELECT);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_SETTINGS,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CANCEL_SETTINGS, "settings accepts KEY_SELECT");
}

static void test_modal_confirm_3ds_key_chords_are_safe(void)
{
	enum scheduler_rating rating = SCHEDULER_RATING_COUNT;
	unsigned int buttons = app_controls_buttons_from_3ds_keys(KEY_A);
	enum app_control_action action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_EXIT,
		false,
		true,
		buttons,
		buttons,
		&rating
	);

	check(action == APP_CONTROL_ACTION_CONFIRM_EXIT, "exit confirm accepts KEY_A");

	buttons = app_controls_buttons_from_3ds_keys(KEY_A | KEY_SELECT);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_EXIT,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "exit confirm ignores A plus SELECT");

	buttons = app_controls_buttons_from_3ds_keys(KEY_X);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_RESET,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CONFIRM_RESET, "reset confirm accepts KEY_X");

	buttons = app_controls_buttons_from_3ds_keys(KEY_X | KEY_B);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_RESET,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "reset confirm ignores X plus B");

	buttons = app_controls_buttons_from_3ds_keys(KEY_X | KEY_START);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_RESET,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "reset confirm ignores X plus START");

	buttons = app_controls_buttons_from_3ds_keys(KEY_X);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_SUSPEND,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CONFIRM_SUSPEND, "suspend confirm accepts KEY_X");

	buttons = app_controls_buttons_from_3ds_keys(KEY_X | KEY_B);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_SUSPEND,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "suspend confirm ignores X plus B");

	buttons = app_controls_buttons_from_3ds_keys(KEY_X);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_RESTORE,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CONFIRM_RESTORE, "restore confirm accepts KEY_X");

	buttons = app_controls_buttons_from_3ds_keys(KEY_X | KEY_START);
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_RESTORE,
		false,
		true,
		buttons,
		buttons,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "restore confirm ignores X plus START");
}

int main(void)
{
	test_maps_3ds_keys_to_app_buttons();
	test_maps_combined_3ds_keys_to_combined_app_buttons();
	test_revealed_review_maps_3ds_face_keys_to_ratings();
	test_review_rating_context_and_chords_are_safe();
	test_start_exit_mapping_and_chords_are_safe();
	test_review_actions_and_study_commands_are_safe();
	test_actions_and_settings_3ds_key_chords_are_safe();
	test_modal_confirm_3ds_key_chords_are_safe();
	printf("app controls 3ds key tests passed\n");
	return 0;
}
