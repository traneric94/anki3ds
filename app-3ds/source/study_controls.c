#include "study_controls.h"

#include <stddef.h>

bool study_controls_has_single_button(unsigned int buttons)
{
	return buttons != 0 && (buttons & (buttons - 1)) == 0;
}

void study_control_repeat_init(struct study_control_repeat *repeat)
{
	if (repeat == NULL)
		return;

	repeat->held_buttons = 0;
	repeat->frame_count = 0;
}

void study_control_repeat_reset(struct study_control_repeat *repeat)
{
	study_control_repeat_init(repeat);
}

bool study_control_repeat_wait_needed(
	unsigned int held_buttons,
	unsigned int repeat_mask
)
{
	unsigned int repeatable_held = held_buttons & repeat_mask;

	if (repeatable_held == 0 || repeatable_held != held_buttons)
		return false;

	return study_controls_has_single_button(repeatable_held);
}

unsigned int study_control_repeat_buttons(
	struct study_control_repeat *repeat,
	unsigned int buttons_down,
	unsigned int buttons_held,
	unsigned int repeat_mask
)
{
	unsigned int repeatable_held;

	if (repeat == NULL)
		return 0;
	if (!study_control_repeat_wait_needed(buttons_held, repeat_mask))
	{
		study_control_repeat_reset(repeat);
		return 0;
	}

	repeatable_held = buttons_held & repeat_mask;
	if (buttons_down != 0 || repeat->held_buttons != repeatable_held)
	{
		repeat->held_buttons = repeatable_held;
		repeat->frame_count = 0;
		return 0;
	}

	repeat->frame_count++;
	if (repeat->frame_count < STUDY_CONTROL_REPEAT_INITIAL_FRAMES)
		return 0;
	if (
		repeat->frame_count > STUDY_CONTROL_REPEAT_INITIAL_FRAMES &&
		(
			(repeat->frame_count - STUDY_CONTROL_REPEAT_INITIAL_FRAMES) %
			STUDY_CONTROL_REPEAT_INTERVAL_FRAMES
		) != 0
	)
	{
		return 0;
	}

	return repeatable_held;
}

unsigned int study_controls_chord_safe_buttons(
	unsigned int action_buttons,
	unsigned int held_buttons
)
{
	unsigned int held_extra_buttons;

	if (action_buttons == 0)
		return 0;

	held_extra_buttons = held_buttons & ~action_buttons;
	return action_buttons | held_extra_buttons;
}

static enum study_control_action study_controls_interpret_rating(
	unsigned int buttons,
	enum study_control_rating *rating
)
{
	if (!study_controls_has_single_button(buttons))
		return STUDY_CONTROL_ACTION_NONE;

	switch (buttons)
	{
	case STUDY_CONTROL_BUTTON_A:
		*rating = STUDY_CONTROL_RATING_AGAIN;
		return STUDY_CONTROL_ACTION_RATE;
	case STUDY_CONTROL_BUTTON_L:
		*rating = STUDY_CONTROL_RATING_HARD;
		return STUDY_CONTROL_ACTION_RATE;
	case STUDY_CONTROL_BUTTON_X:
		*rating = STUDY_CONTROL_RATING_GOOD;
		return STUDY_CONTROL_ACTION_RATE;
	case STUDY_CONTROL_BUTTON_Y:
		*rating = STUDY_CONTROL_RATING_EASY;
		return STUDY_CONTROL_ACTION_RATE;
	}

	return STUDY_CONTROL_ACTION_NONE;
}

enum study_control_action study_controls_interpret(
	unsigned int buttons,
	bool answer_visible,
	bool undo_available,
	enum study_control_rating *rating
)
{
	if (rating != 0)
		*rating = STUDY_CONTROL_RATING_NONE;

	if (!study_controls_has_single_button(buttons))
		return STUDY_CONTROL_ACTION_NONE;

	if (buttons == STUDY_CONTROL_BUTTON_START)
		return STUDY_CONTROL_ACTION_TOGGLE_HELP;
	if (buttons == STUDY_CONTROL_BUTTON_SELECT)
		return STUDY_CONTROL_ACTION_DECKS;
	if (buttons == STUDY_CONTROL_BUTTON_R)
		return STUDY_CONTROL_ACTION_SUSPEND_RESTORE;
	if (buttons == STUDY_CONTROL_BUTTON_UP)
		return STUDY_CONTROL_ACTION_SCROLL_UP;
	if (buttons == STUDY_CONTROL_BUTTON_DOWN)
		return STUDY_CONTROL_ACTION_SCROLL_DOWN;
	if (buttons == STUDY_CONTROL_BUTTON_B)
		return undo_available ?
			STUDY_CONTROL_ACTION_UNDO :
			STUDY_CONTROL_ACTION_NONE;

	if (answer_visible)
	{
		if (rating == 0)
			return STUDY_CONTROL_ACTION_NONE;
		return study_controls_interpret_rating(buttons, rating);
	}

	if (buttons == STUDY_CONTROL_BUTTON_A)
		return STUDY_CONTROL_ACTION_REVEAL;

	return STUDY_CONTROL_ACTION_NONE;
}
