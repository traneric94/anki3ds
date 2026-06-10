#include "app_input_frame.h"

#include <assert.h>

static void build_dpad_frame(
	struct app_input_frame *frame,
	struct study_control_repeat *repeat,
	enum app_screen_model_kind screen_kind,
	unsigned int buttons_down,
	unsigned int buttons_held
)
{
	app_input_frame_build(
		frame,
		repeat,
		screen_kind,
		buttons_down,
		buttons_held,
		buttons_down,
		buttons_held,
		0,
		0
	);
}

static void build_cpad_frame(
	struct app_input_frame *frame,
	struct study_control_repeat *repeat,
	enum app_screen_model_kind screen_kind,
	unsigned int buttons_down,
	unsigned int buttons_held
)
{
	app_input_frame_build(
		frame,
		repeat,
		screen_kind,
		buttons_down,
		buttons_held,
		0,
		0,
		buttons_down,
		buttons_held
	);
}

static void test_initial_press_is_action_and_waits_for_repeatable_hold(void)
{
	struct study_control_repeat repeat;
	struct app_input_frame frame;

	study_control_repeat_init(&repeat);
	build_dpad_frame(
		&frame,
		&repeat,
		APP_SCREEN_MODEL_REVIEW,
		STUDY_CONTROL_BUTTON_DOWN,
		STUDY_CONTROL_BUTTON_DOWN
	);

	assert(frame.buttons == STUDY_CONTROL_BUTTON_DOWN);
	assert(frame.dpad_buttons == STUDY_CONTROL_BUTTON_DOWN);
	assert(frame.cpad_buttons == 0);
	assert(frame.held_navigation_wait);
}

static void test_held_navigation_repeats_after_initial_delay(void)
{
	struct study_control_repeat repeat;
	struct app_input_frame frame;

	study_control_repeat_init(&repeat);
	build_dpad_frame(
		&frame,
		&repeat,
		APP_SCREEN_MODEL_REVIEW,
		STUDY_CONTROL_BUTTON_DOWN,
		STUDY_CONTROL_BUTTON_DOWN
	);
	for (
		unsigned int frame_index = 1;
		frame_index < STUDY_CONTROL_REPEAT_INITIAL_FRAMES;
		frame_index++
	)
	{
		build_dpad_frame(
			&frame,
			&repeat,
			APP_SCREEN_MODEL_REVIEW,
			0,
			STUDY_CONTROL_BUTTON_DOWN
		);
		assert(frame.buttons == 0);
		assert(frame.held_navigation_wait);
	}

	build_dpad_frame(
		&frame,
		&repeat,
		APP_SCREEN_MODEL_REVIEW,
		0,
		STUDY_CONTROL_BUTTON_DOWN
	);
	assert(frame.buttons == STUDY_CONTROL_BUTTON_DOWN);
	assert(frame.dpad_buttons == STUDY_CONTROL_BUTTON_DOWN);
	assert(frame.cpad_buttons == 0);
	assert(frame.held_navigation_wait);
}

static void test_cpad_source_survives_repeat_after_initial_delay(void)
{
	struct study_control_repeat repeat;
	struct app_input_frame frame;

	study_control_repeat_init(&repeat);
	build_cpad_frame(
		&frame,
		&repeat,
		APP_SCREEN_MODEL_REVIEW,
		STUDY_CONTROL_BUTTON_UP,
		STUDY_CONTROL_BUTTON_UP
	);
	for (
		unsigned int frame_index = 1;
		frame_index < STUDY_CONTROL_REPEAT_INITIAL_FRAMES;
		frame_index++
	)
	{
		build_cpad_frame(
			&frame,
			&repeat,
			APP_SCREEN_MODEL_REVIEW,
			0,
			STUDY_CONTROL_BUTTON_UP
		);
		assert(frame.buttons == 0);
	}

	build_cpad_frame(
		&frame,
		&repeat,
		APP_SCREEN_MODEL_REVIEW,
		0,
		STUDY_CONTROL_BUTTON_UP
	);
	assert(frame.buttons == STUDY_CONTROL_BUTTON_UP);
	assert(frame.dpad_buttons == 0);
	assert(frame.cpad_buttons == STUDY_CONTROL_BUTTON_UP);
}

static void test_mixed_navigation_sources_are_inert(void)
{
	struct study_control_repeat repeat;
	struct app_input_frame frame;

	study_control_repeat_init(&repeat);
	app_input_frame_build(
		&frame,
		&repeat,
		APP_SCREEN_MODEL_REVIEW,
		STUDY_CONTROL_BUTTON_DOWN,
		STUDY_CONTROL_BUTTON_DOWN,
		STUDY_CONTROL_BUTTON_DOWN,
		STUDY_CONTROL_BUTTON_DOWN,
		STUDY_CONTROL_BUTTON_DOWN,
		STUDY_CONTROL_BUTTON_DOWN
	);

	assert(frame.buttons == 0);
	assert(frame.dpad_buttons == 0);
	assert(frame.cpad_buttons == 0);
	assert(!frame.held_navigation_wait);
}

static void test_held_cpad_plus_new_dpad_navigation_is_inert(void)
{
	struct study_control_repeat repeat;
	struct app_input_frame frame;

	study_control_repeat_init(&repeat);
	app_input_frame_build(
		&frame,
		&repeat,
		APP_SCREEN_MODEL_REVIEW,
		STUDY_CONTROL_BUTTON_DOWN,
		STUDY_CONTROL_BUTTON_DOWN,
		STUDY_CONTROL_BUTTON_DOWN,
		STUDY_CONTROL_BUTTON_DOWN,
		0,
		STUDY_CONTROL_BUTTON_DOWN
	);

	assert(frame.buttons == 0);
	assert(frame.dpad_buttons == 0);
	assert(frame.cpad_buttons == 0);
	assert(!frame.held_navigation_wait);
}

static void test_held_extra_button_keeps_new_press_chord_safe(void)
{
	struct study_control_repeat repeat;
	struct app_input_frame frame;
	enum study_control_rating rating = STUDY_CONTROL_RATING_NONE;

	study_control_repeat_init(&repeat);
	build_dpad_frame(
		&frame,
		&repeat,
		APP_SCREEN_MODEL_REVIEW,
		STUDY_CONTROL_BUTTON_A,
		STUDY_CONTROL_BUTTON_A | STUDY_CONTROL_BUTTON_DOWN
	);

	assert(
		frame.buttons ==
		(STUDY_CONTROL_BUTTON_A | STUDY_CONTROL_BUTTON_DOWN)
	);
	assert(frame.dpad_buttons == 0);
	assert(frame.cpad_buttons == 0);
	assert(!frame.held_navigation_wait);
	assert(
		study_controls_interpret(frame.buttons, false, true, &rating) ==
		STUDY_CONTROL_ACTION_NONE
	);
}

static void test_confirmation_screen_does_not_wait_or_repeat(void)
{
	struct study_control_repeat repeat;
	struct app_input_frame frame;

	study_control_repeat_init(&repeat);
	build_dpad_frame(
		&frame,
		&repeat,
		APP_SCREEN_MODEL_CONFIRM,
		STUDY_CONTROL_BUTTON_DOWN,
		STUDY_CONTROL_BUTTON_DOWN
	);
	assert(frame.buttons == STUDY_CONTROL_BUTTON_DOWN);
	assert(!frame.held_navigation_wait);

	for (
		unsigned int frame_index = 0;
		frame_index <= STUDY_CONTROL_REPEAT_INITIAL_FRAMES;
		frame_index++
	)
	{
		build_dpad_frame(
			&frame,
			&repeat,
			APP_SCREEN_MODEL_CONFIRM,
			0,
			STUDY_CONTROL_BUTTON_DOWN
		);
		assert(frame.buttons == 0);
		assert(!frame.held_navigation_wait);
	}
}

static void test_screen_repeat_mask_controls_held_wait(void)
{
	struct study_control_repeat repeat;
	struct app_input_frame frame;

	study_control_repeat_init(&repeat);
	build_dpad_frame(
		&frame,
		&repeat,
		APP_SCREEN_MODEL_SETTINGS,
		0,
		STUDY_CONTROL_BUTTON_UP
	);
	assert(frame.buttons == 0);
	assert(!frame.held_navigation_wait);

	build_dpad_frame(
		&frame,
		&repeat,
		APP_SCREEN_MODEL_SETTINGS,
		STUDY_CONTROL_BUTTON_LEFT,
		STUDY_CONTROL_BUTTON_LEFT
	);
	assert(frame.buttons == STUDY_CONTROL_BUTTON_LEFT);
	assert(frame.held_navigation_wait);
}

int main(void)
{
	test_initial_press_is_action_and_waits_for_repeatable_hold();
	test_held_navigation_repeats_after_initial_delay();
	test_cpad_source_survives_repeat_after_initial_delay();
	test_mixed_navigation_sources_are_inert();
	test_held_cpad_plus_new_dpad_navigation_is_inert();
	test_held_extra_button_keeps_new_press_chord_safe();
	test_confirmation_screen_does_not_wait_or_repeat();
	test_screen_repeat_mask_controls_held_wait();
	return 0;
}
