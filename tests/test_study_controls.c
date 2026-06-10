#include "study_controls.h"

#include <assert.h>

static unsigned int test_direction_repeat_mask(void)
{
	return
		STUDY_CONTROL_BUTTON_UP |
		STUDY_CONTROL_BUTTON_DOWN |
		STUDY_CONTROL_BUTTON_LEFT |
		STUDY_CONTROL_BUTTON_RIGHT;
}

static void test_unrevealed_controls(void)
{
	enum study_control_rating rating = STUDY_CONTROL_RATING_EASY;

	assert(
		study_controls_interpret(
			STUDY_CONTROL_BUTTON_A,
			false,
			false,
			&rating
		) == STUDY_CONTROL_ACTION_REVEAL
	);
	assert(rating == STUDY_CONTROL_RATING_NONE);
	assert(
		study_controls_interpret(
			STUDY_CONTROL_BUTTON_B,
			false,
			true,
			&rating
		) == STUDY_CONTROL_ACTION_UNDO
	);
	assert(
		study_controls_interpret(
			STUDY_CONTROL_BUTTON_B,
			false,
			false,
			&rating
		) == STUDY_CONTROL_ACTION_NONE
	);
}

static void test_revealed_ratings(void)
{
	enum study_control_rating rating = STUDY_CONTROL_RATING_NONE;

	assert(
		study_controls_interpret(
			STUDY_CONTROL_BUTTON_A,
			true,
			false,
			&rating
		) == STUDY_CONTROL_ACTION_RATE
	);
	assert(rating == STUDY_CONTROL_RATING_AGAIN);

	assert(
		study_controls_interpret(
			STUDY_CONTROL_BUTTON_L,
			true,
			false,
			&rating
		) == STUDY_CONTROL_ACTION_RATE
	);
	assert(rating == STUDY_CONTROL_RATING_HARD);

	assert(
		study_controls_interpret(
			STUDY_CONTROL_BUTTON_X,
			true,
			false,
			&rating
		) == STUDY_CONTROL_ACTION_RATE
	);
	assert(rating == STUDY_CONTROL_RATING_GOOD);

	assert(
		study_controls_interpret(
			STUDY_CONTROL_BUTTON_Y,
			true,
			false,
			&rating
		) == STUDY_CONTROL_ACTION_RATE
	);
	assert(rating == STUDY_CONTROL_RATING_EASY);
}

static void test_chords_are_ignored(void)
{
	enum study_control_rating rating = STUDY_CONTROL_RATING_GOOD;
	unsigned int face_chord = STUDY_CONTROL_BUTTON_A | STUDY_CONTROL_BUTTON_B;
	unsigned int command_chord = STUDY_CONTROL_BUTTON_R | STUDY_CONTROL_BUTTON_A;
	unsigned int nav_chord = STUDY_CONTROL_BUTTON_DOWN | STUDY_CONTROL_BUTTON_A;

	assert(
		study_controls_interpret(face_chord, true, true, &rating) ==
		STUDY_CONTROL_ACTION_NONE
	);
	assert(rating == STUDY_CONTROL_RATING_NONE);
	assert(
		study_controls_interpret(command_chord, false, true, &rating) ==
		STUDY_CONTROL_ACTION_NONE
	);
	assert(
		study_controls_interpret(nav_chord, false, true, &rating) ==
		STUDY_CONTROL_ACTION_NONE
	);
	assert(
		study_controls_interpret(
			STUDY_CONTROL_BUTTON_START | STUDY_CONTROL_BUTTON_A,
			true,
			true,
			&rating
		) == STUDY_CONTROL_ACTION_NONE
	);
}

static void test_chord_safe_buttons_include_held_context(void)
{
	enum study_control_rating rating = STUDY_CONTROL_RATING_GOOD;
	unsigned int chord_safe;

	assert(
		study_controls_chord_safe_buttons(
			STUDY_CONTROL_BUTTON_A,
			STUDY_CONTROL_BUTTON_A
		) == STUDY_CONTROL_BUTTON_A
	);
	assert(
		study_controls_chord_safe_buttons(
			STUDY_CONTROL_BUTTON_A,
			STUDY_CONTROL_BUTTON_A | STUDY_CONTROL_BUTTON_DOWN
		) == (STUDY_CONTROL_BUTTON_A | STUDY_CONTROL_BUTTON_DOWN)
	);
	assert(
		study_controls_chord_safe_buttons(
			0,
			STUDY_CONTROL_BUTTON_DOWN
		) == 0
	);

	chord_safe = study_controls_chord_safe_buttons(
		STUDY_CONTROL_BUTTON_A,
		STUDY_CONTROL_BUTTON_A | STUDY_CONTROL_BUTTON_DOWN
	);
	assert(
		study_controls_interpret(chord_safe, false, true, &rating) ==
		STUDY_CONTROL_ACTION_NONE
	);
	assert(rating == STUDY_CONTROL_RATING_NONE);
}

static void test_navigation_repeat_wait_needed(void)
{
	unsigned int repeat_mask = test_direction_repeat_mask();

	assert(
		study_control_repeat_wait_needed(
			STUDY_CONTROL_BUTTON_DOWN,
			repeat_mask
		)
	);
	assert(
		!study_control_repeat_wait_needed(
			STUDY_CONTROL_BUTTON_DOWN | STUDY_CONTROL_BUTTON_UP,
			repeat_mask
		)
	);
	assert(
		!study_control_repeat_wait_needed(
			STUDY_CONTROL_BUTTON_DOWN | STUDY_CONTROL_BUTTON_A,
			repeat_mask
		)
	);
	assert(
		!study_control_repeat_wait_needed(
			STUDY_CONTROL_BUTTON_LEFT,
			STUDY_CONTROL_BUTTON_UP | STUDY_CONTROL_BUTTON_DOWN
		)
	);
	assert(!study_control_repeat_wait_needed(0, repeat_mask));
}

static void test_navigation_repeat_buttons(void)
{
	struct study_control_repeat repeat;
	unsigned int repeat_mask = test_direction_repeat_mask();
	unsigned int repeated;

	study_control_repeat_init(&repeat);
	repeated = study_control_repeat_buttons(
		&repeat,
		STUDY_CONTROL_BUTTON_DOWN,
		STUDY_CONTROL_BUTTON_DOWN,
		repeat_mask
	);
	assert(repeated == 0);
	for (
		unsigned int frame = 1;
		frame < STUDY_CONTROL_REPEAT_INITIAL_FRAMES;
		frame++
	)
	{
		repeated = study_control_repeat_buttons(
			&repeat,
			0,
			STUDY_CONTROL_BUTTON_DOWN,
			repeat_mask
		);
		assert(repeated == 0);
	}
	repeated = study_control_repeat_buttons(
		&repeat,
		0,
		STUDY_CONTROL_BUTTON_DOWN,
		repeat_mask
	);
	assert(repeated == STUDY_CONTROL_BUTTON_DOWN);
	for (
		unsigned int frame = 1;
		frame < STUDY_CONTROL_REPEAT_INTERVAL_FRAMES;
		frame++
	)
	{
		repeated = study_control_repeat_buttons(
			&repeat,
			0,
			STUDY_CONTROL_BUTTON_DOWN,
			repeat_mask
		);
		assert(repeated == 0);
	}
	repeated = study_control_repeat_buttons(
		&repeat,
		0,
		STUDY_CONTROL_BUTTON_DOWN,
		repeat_mask
	);
	assert(repeated == STUDY_CONTROL_BUTTON_DOWN);

	repeated = study_control_repeat_buttons(
		&repeat,
		0,
		STUDY_CONTROL_BUTTON_DOWN | STUDY_CONTROL_BUTTON_A,
		repeat_mask
	);
	assert(repeated == 0);
	repeated = study_control_repeat_buttons(
		&repeat,
		0,
		STUDY_CONTROL_BUTTON_DOWN | STUDY_CONTROL_BUTTON_UP,
		repeat_mask
	);
	assert(repeated == 0);
	repeated = study_control_repeat_buttons(
		&repeat,
		0,
		STUDY_CONTROL_BUTTON_DOWN,
		repeat_mask
	);
	assert(repeated == 0);
}

static void test_navigation_repeat_direction_change_resets_delay(void)
{
	struct study_control_repeat repeat;
	unsigned int repeat_mask = test_direction_repeat_mask();
	unsigned int repeated;

	study_control_repeat_init(&repeat);
	repeated = study_control_repeat_buttons(
		&repeat,
		STUDY_CONTROL_BUTTON_DOWN,
		STUDY_CONTROL_BUTTON_DOWN,
		repeat_mask
	);
	assert(repeated == 0);
	for (
		unsigned int frame = 1;
		frame < STUDY_CONTROL_REPEAT_INITIAL_FRAMES;
		frame++
	)
	{
		repeated = study_control_repeat_buttons(
			&repeat,
			0,
			STUDY_CONTROL_BUTTON_DOWN,
			repeat_mask
		);
		assert(repeated == 0);
	}
	repeated = study_control_repeat_buttons(
		&repeat,
		0,
		STUDY_CONTROL_BUTTON_RIGHT,
		repeat_mask
	);
	assert(repeated == 0);
	for (
		unsigned int frame = 1;
		frame < STUDY_CONTROL_REPEAT_INITIAL_FRAMES;
		frame++
	)
	{
		repeated = study_control_repeat_buttons(
			&repeat,
			0,
			STUDY_CONTROL_BUTTON_RIGHT,
			repeat_mask
		);
		assert(repeated == 0);
	}
	repeated = study_control_repeat_buttons(
		&repeat,
		0,
		STUDY_CONTROL_BUTTON_RIGHT,
		repeat_mask
	);
	assert(repeated == STUDY_CONTROL_BUTTON_RIGHT);
}

static void test_navigation_and_commands(void)
{
	enum study_control_rating rating = STUDY_CONTROL_RATING_NONE;

	assert(
		study_controls_interpret(
			STUDY_CONTROL_BUTTON_START,
			false,
			false,
			&rating
		) == STUDY_CONTROL_ACTION_TOGGLE_HELP
	);
	assert(
		study_controls_interpret(
			STUDY_CONTROL_BUTTON_SELECT,
			false,
			false,
			&rating
		) == STUDY_CONTROL_ACTION_DECKS
	);
	assert(
		study_controls_interpret(
			STUDY_CONTROL_BUTTON_R,
			false,
			false,
			&rating
		) == STUDY_CONTROL_ACTION_SUSPEND_RESTORE
	);
	assert(
		study_controls_interpret(
			STUDY_CONTROL_BUTTON_UP,
			false,
			false,
			&rating
		) == STUDY_CONTROL_ACTION_SCROLL_UP
	);
	assert(
		study_controls_interpret(
			STUDY_CONTROL_BUTTON_DOWN,
			false,
			false,
			&rating
		) == STUDY_CONTROL_ACTION_SCROLL_DOWN
	);
	assert(
		study_controls_interpret(
			STUDY_CONTROL_BUTTON_LEFT,
			false,
			false,
			&rating
		) == STUDY_CONTROL_ACTION_NONE
	);
	assert(
		study_controls_interpret(
			STUDY_CONTROL_BUTTON_RIGHT,
			false,
			false,
			&rating
		) == STUDY_CONTROL_ACTION_NONE
	);
}

int main(void)
{
	test_unrevealed_controls();
	test_revealed_ratings();
	test_chords_are_ignored();
	test_chord_safe_buttons_include_held_context();
	test_navigation_repeat_wait_needed();
	test_navigation_repeat_buttons();
	test_navigation_repeat_direction_change_resets_delay();
	test_navigation_and_commands();
	return 0;
}
