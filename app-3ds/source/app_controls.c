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

bool app_controls_can_open(
	enum app_control_mode mode,
	bool review_answer_revealed
)
{
	if (mode == APP_CONTROL_MODE_CONFIRM_EXIT || mode == APP_CONTROL_MODE_CONFIRM_RESET)
		return false;
	if (mode == APP_CONTROL_MODE_CONTROLS)
		return false;
	if (mode == APP_CONTROL_MODE_REVIEW && review_answer_revealed)
		return false;

	return true;
}

bool app_controls_should_show_answer(unsigned int buttons, bool review_answer_revealed)
{
	return !review_answer_revealed && (buttons & APP_CONTROL_BUTTON_A) != 0;
}

bool app_controls_rating_for_buttons(
	unsigned int buttons,
	bool review_answer_revealed,
	enum scheduler_rating *rating
)
{
	if (!review_answer_revealed)
		return false;

	if (buttons & APP_CONTROL_BUTTON_Y)
	{
		*rating = SCHEDULER_RATING_AGAIN;
		return true;
	}
	if (buttons & APP_CONTROL_BUTTON_X)
	{
		*rating = SCHEDULER_RATING_HARD;
		return true;
	}
	if (buttons & APP_CONTROL_BUTTON_B)
	{
		*rating = SCHEDULER_RATING_GOOD;
		return true;
	}
	if (buttons & APP_CONTROL_BUTTON_A)
	{
		*rating = SCHEDULER_RATING_EASY;
		return true;
	}

	return false;
}
