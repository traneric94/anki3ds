#include "app_controls.h"

#ifdef __3DS__
#include <3ds.h>
#endif

unsigned int app_controls_buttons_from_3ds_keys(unsigned int keys)
{
	unsigned int buttons = 0;

#ifdef __3DS__
	if (keys & KEY_A)
		buttons |= APP_CONTROL_BUTTON_A;
	if (keys & KEY_B)
		buttons |= APP_CONTROL_BUTTON_B;
	if (keys & KEY_X)
		buttons |= APP_CONTROL_BUTTON_X;
	if (keys & KEY_Y)
		buttons |= APP_CONTROL_BUTTON_Y;
	if (keys & KEY_L)
		buttons |= APP_CONTROL_BUTTON_L;
	if (keys & KEY_R)
		buttons |= APP_CONTROL_BUTTON_R;
	if (keys & KEY_DUP)
		buttons |= APP_CONTROL_BUTTON_UP;
	if (keys & KEY_DDOWN)
		buttons |= APP_CONTROL_BUTTON_DOWN;
	if (keys & KEY_DLEFT)
		buttons |= APP_CONTROL_BUTTON_LEFT;
	if (keys & KEY_DRIGHT)
		buttons |= APP_CONTROL_BUTTON_RIGHT;
	if (keys & KEY_SELECT)
		buttons |= APP_CONTROL_BUTTON_SELECT;
	if (keys & KEY_START)
		buttons |= APP_CONTROL_BUTTON_START;
#else
	buttons = keys;
#endif

	return buttons;
}

void app_controls_repeat_init(struct app_control_repeat *repeat)
{
	app_controls_repeat_reset(repeat);
}

void app_controls_repeat_reset(struct app_control_repeat *repeat)
{
	if (repeat == NULL)
		return;

	repeat->buttons = 0;
	repeat->tick_count = 0;
}

unsigned int app_controls_repeat_buttons(
	struct app_control_repeat *repeat,
	unsigned int buttons_down,
	unsigned int buttons_held
)
{
	unsigned int held_navigation = buttons_held & APP_CONTROL_BUTTON_NAVIGATION_MASK;
	unsigned int down_navigation = buttons_down & APP_CONTROL_BUTTON_NAVIGATION_MASK;
	unsigned int command_buttons =
		(buttons_down | buttons_held) & APP_CONTROL_COMMAND_BUTTON_MASK;

	if (repeat == NULL)
		return 0;
	if (
		command_buttons != 0 ||
		held_navigation == 0 ||
		(held_navigation & (held_navigation - 1)) != 0
	)
	{
		app_controls_repeat_reset(repeat);
		return 0;
	}
	if (down_navigation != 0 || held_navigation != repeat->buttons)
	{
		repeat->buttons = held_navigation;
		repeat->tick_count = 0;
		return 0;
	}

	if (repeat->tick_count < APP_CONTROL_REPEAT_INITIAL_TICKS)
	{
		repeat->tick_count++;
		return repeat->tick_count == APP_CONTROL_REPEAT_INITIAL_TICKS ?
			held_navigation :
			0;
	}

	repeat->tick_count++;
	if (
		repeat->tick_count >=
		APP_CONTROL_REPEAT_INITIAL_TICKS + APP_CONTROL_REPEAT_INTERVAL_TICKS
	)
	{
		repeat->tick_count = APP_CONTROL_REPEAT_INITIAL_TICKS;
		return held_navigation;
	}

	return 0;
}

bool app_controls_input_is_active(
	unsigned int buttons_down,
	unsigned int buttons_held,
	unsigned int repeat_buttons
)
{
	return (
		(buttons_down | buttons_held | repeat_buttons) &
		APP_CONTROL_BUTTON_INPUT_MASK
	) != 0;
}

bool app_controls_can_open(
	enum app_control_mode mode,
	bool review_answer_revealed
)
{
	if (mode == APP_CONTROL_MODE_CONFIRM_EXIT || mode == APP_CONTROL_MODE_CONFIRM_RESET)
		return false;
	if (mode == APP_CONTROL_MODE_CONFIRM_RESTORE)
		return false;
	if (mode == APP_CONTROL_MODE_CONFIRM_SUSPEND)
		return false;
	if (mode == APP_CONTROL_MODE_CONTROLS)
		return false;
	if (mode == APP_CONTROL_MODE_REVIEW && review_answer_revealed)
		return false;

	return true;
}

bool app_controls_up_down_direction(unsigned int buttons, bool *down)
{
	bool up_pressed = (buttons & APP_CONTROL_BUTTON_UP) != 0;
	bool down_pressed = (buttons & APP_CONTROL_BUTTON_DOWN) != 0;
	unsigned int left_right_buttons =
		buttons & (APP_CONTROL_BUTTON_LEFT | APP_CONTROL_BUTTON_RIGHT);
	unsigned int command_buttons = buttons & APP_CONTROL_COMMAND_BUTTON_MASK;

	if (down == NULL)
		return false;
	if (command_buttons != 0)
		return false;
	if (left_right_buttons != 0)
		return false;
	if (up_pressed == down_pressed)
		return false;

	*down = down_pressed;
	return true;
}

bool app_controls_left_right_direction(unsigned int buttons, bool *right)
{
	bool left_pressed = (buttons & APP_CONTROL_BUTTON_LEFT) != 0;
	bool right_pressed = (buttons & APP_CONTROL_BUTTON_RIGHT) != 0;
	unsigned int up_down_buttons =
		buttons & (APP_CONTROL_BUTTON_UP | APP_CONTROL_BUTTON_DOWN);
	unsigned int command_buttons = buttons & APP_CONTROL_COMMAND_BUTTON_MASK;

	if (right == NULL)
		return false;
	if (command_buttons != 0)
		return false;
	if (up_down_buttons != 0)
		return false;
	if (left_pressed == right_pressed)
		return false;

	*right = right_pressed;
	return true;
}

bool app_controls_single_command(
	unsigned int buttons,
	unsigned int command_button,
	unsigned int command_mask
)
{
	if (command_button == 0 || (command_button & (command_button - 1)) != 0)
		return false;
	if ((command_button & command_mask) == 0)
		return false;

	return (buttons & command_mask) == command_button;
}

bool app_controls_command_pressed(unsigned int buttons, unsigned int command_button)
{
	return app_controls_single_command(
		buttons,
		command_button,
		APP_CONTROL_BUTTON_INPUT_MASK
	);
}

bool app_controls_should_show_answer(unsigned int buttons, bool review_answer_revealed)
{
	return (
		!review_answer_revealed &&
		app_controls_command_pressed(buttons, APP_CONTROL_BUTTON_A)
	);
}

bool app_controls_rating_for_buttons(
	unsigned int buttons,
	bool review_answer_revealed,
	enum scheduler_rating *rating
)
{
	unsigned int rating_buttons =
		buttons & APP_CONTROL_FACE_BUTTON_MASK;

	if (!review_answer_revealed)
		return false;
	if (rating == NULL)
		return false;
	if (rating_buttons == 0 || (rating_buttons & (rating_buttons - 1)) != 0)
		return false;
	if ((buttons & APP_CONTROL_BUTTON_INPUT_MASK) != rating_buttons)
		return false;

	if (rating_buttons & APP_CONTROL_BUTTON_Y)
	{
		*rating = SCHEDULER_RATING_AGAIN;
		return true;
	}
	if (rating_buttons & APP_CONTROL_BUTTON_X)
	{
		*rating = SCHEDULER_RATING_HARD;
		return true;
	}
	if (rating_buttons & APP_CONTROL_BUTTON_B)
	{
		*rating = SCHEDULER_RATING_GOOD;
		return true;
	}
	if (rating_buttons & APP_CONTROL_BUTTON_A)
	{
		*rating = SCHEDULER_RATING_EASY;
		return true;
	}

	return false;
}
