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
	if (keys & (KEY_DUP | KEY_CPAD_UP))
		buttons |= APP_CONTROL_BUTTON_UP;
	if (keys & (KEY_DDOWN | KEY_CPAD_DOWN))
		buttons |= APP_CONTROL_BUTTON_DOWN;
	if (keys & (KEY_DLEFT | KEY_CPAD_LEFT))
		buttons |= APP_CONTROL_BUTTON_LEFT;
	if (keys & (KEY_DRIGHT | KEY_CPAD_RIGHT))
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

unsigned int app_controls_navigation_repeat_mask(enum app_control_mode mode)
{
	switch (mode)
	{
	case APP_CONTROL_MODE_DECK_SELECT:
		return APP_CONTROL_BUTTON_NAVIGATION_MASK;
	case APP_CONTROL_MODE_REVIEW:
	case APP_CONTROL_MODE_ACTIONS:
		return APP_CONTROL_BUTTON_UP | APP_CONTROL_BUTTON_DOWN;
	case APP_CONTROL_MODE_SETTINGS:
		return APP_CONTROL_BUTTON_LEFT | APP_CONTROL_BUTTON_RIGHT;
	case APP_CONTROL_MODE_LOAD_ERROR:
	case APP_CONTROL_MODE_SUMMARY:
	case APP_CONTROL_MODE_CONTROLS:
	case APP_CONTROL_MODE_CONFIRM_RESTORE:
	case APP_CONTROL_MODE_CONFIRM_SUSPEND:
	case APP_CONTROL_MODE_CONFIRM_RESET:
	case APP_CONTROL_MODE_CONFIRM_EXIT:
		break;
	}

	return 0;
}

static unsigned int app_controls_repeat_buttons_with_mask(
	struct app_control_repeat *repeat,
	unsigned int buttons_down,
	unsigned int buttons_held,
	unsigned int navigation_mask
)
{
	unsigned int active_input =
		(buttons_down | buttons_held) & APP_CONTROL_BUTTON_INPUT_MASK;
	unsigned int held_navigation = buttons_held & navigation_mask;
	unsigned int down_navigation = buttons_down & navigation_mask;
	unsigned int command_buttons =
		(buttons_down | buttons_held) & APP_CONTROL_COMMAND_BUTTON_MASK;
	unsigned int disallowed_navigation =
		active_input & APP_CONTROL_BUTTON_NAVIGATION_MASK & ~navigation_mask;

	if (repeat == NULL)
		return 0;
	if (
		navigation_mask == 0 ||
		command_buttons != 0 ||
		disallowed_navigation != 0 ||
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

unsigned int app_controls_repeat_buttons(
	struct app_control_repeat *repeat,
	unsigned int buttons_down,
	unsigned int buttons_held
)
{
	return app_controls_repeat_buttons_with_mask(
		repeat,
		buttons_down,
		buttons_held,
		APP_CONTROL_BUTTON_NAVIGATION_MASK
	);
}

unsigned int app_controls_repeat_buttons_for_mode(
	struct app_control_repeat *repeat,
	enum app_control_mode mode,
	unsigned int buttons_down,
	unsigned int buttons_held
)
{
	return app_controls_repeat_buttons_with_mask(
		repeat,
		buttons_down,
		buttons_held,
		app_controls_navigation_repeat_mask(mode)
	);
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

bool app_controls_mode_uses_navigation_repeat(enum app_control_mode mode)
{
	return app_controls_navigation_repeat_mask(mode) != 0;
}

static bool app_controls_repeatable_navigation_held_with_mask(
	unsigned int buttons_held,
	unsigned int navigation_mask
)
{
	unsigned int held_input = buttons_held & APP_CONTROL_BUTTON_INPUT_MASK;
	unsigned int held_navigation = held_input & navigation_mask;
	unsigned int disallowed_navigation =
		held_input & APP_CONTROL_BUTTON_NAVIGATION_MASK & ~navigation_mask;

	if (navigation_mask == 0)
		return false;
	if ((held_input & APP_CONTROL_COMMAND_BUTTON_MASK) != 0)
		return false;
	if (disallowed_navigation != 0)
		return false;
	if (held_navigation == 0)
		return false;

	return (held_navigation & (held_navigation - 1)) == 0;
}

bool app_controls_repeatable_navigation_held(unsigned int buttons_held)
{
	return app_controls_repeatable_navigation_held_with_mask(
		buttons_held,
		APP_CONTROL_BUTTON_NAVIGATION_MASK
	);
}

bool app_controls_repeatable_navigation_held_for_mode(
	enum app_control_mode mode,
	unsigned int buttons_held
)
{
	return app_controls_repeatable_navigation_held_with_mask(
		buttons_held,
		app_controls_navigation_repeat_mask(mode)
	);
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

bool app_controls_up_down_triggered(
	unsigned int trigger_buttons,
	unsigned int active_buttons,
	bool *down
)
{
	bool up_triggered = (trigger_buttons & APP_CONTROL_BUTTON_UP) != 0;
	bool down_triggered = (trigger_buttons & APP_CONTROL_BUTTON_DOWN) != 0;
	unsigned int expected_button;

	if (down == NULL)
		return false;
	if (up_triggered == down_triggered)
		return false;

	expected_button = down_triggered ?
		APP_CONTROL_BUTTON_DOWN :
		APP_CONTROL_BUTTON_UP;
	if ((trigger_buttons & APP_CONTROL_BUTTON_INPUT_MASK) != expected_button)
		return false;
	if (
		(active_buttons & APP_CONTROL_BUTTON_INPUT_MASK) !=
		expected_button
	)
	{
		return false;
	}

	*down = down_triggered;
	return true;
}

bool app_controls_up_down_direction(unsigned int buttons, bool *down)
{
	return app_controls_up_down_triggered(buttons, buttons, down);
}

bool app_controls_left_right_triggered(
	unsigned int trigger_buttons,
	unsigned int active_buttons,
	bool *right
)
{
	bool left_triggered = (trigger_buttons & APP_CONTROL_BUTTON_LEFT) != 0;
	bool right_triggered = (trigger_buttons & APP_CONTROL_BUTTON_RIGHT) != 0;
	unsigned int expected_button;

	if (right == NULL)
		return false;
	if (left_triggered == right_triggered)
		return false;

	expected_button = right_triggered ?
		APP_CONTROL_BUTTON_RIGHT :
		APP_CONTROL_BUTTON_LEFT;
	if ((trigger_buttons & APP_CONTROL_BUTTON_INPUT_MASK) != expected_button)
		return false;
	if (
		(active_buttons & APP_CONTROL_BUTTON_INPUT_MASK) !=
		expected_button
	)
	{
		return false;
	}

	*right = right_triggered;
	return true;
}

bool app_controls_left_right_direction(unsigned int buttons, bool *right)
{
	return app_controls_left_right_triggered(buttons, buttons, right);
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

bool app_controls_command_triggered(
	unsigned int trigger_buttons,
	unsigned int active_buttons,
	unsigned int command_button
)
{
	if ((trigger_buttons & command_button) == 0)
		return false;

	return app_controls_single_command(
		active_buttons,
		command_button,
		APP_CONTROL_BUTTON_INPUT_MASK
	);
}

bool app_controls_command_pressed(unsigned int buttons, unsigned int command_button)
{
	return app_controls_command_triggered(
		buttons,
		buttons,
		command_button
	);
}

static bool app_controls_cancel_command_triggered(
	unsigned int trigger_buttons,
	unsigned int active_buttons
)
{
	return (
		app_controls_command_triggered(
			trigger_buttons,
			active_buttons,
			APP_CONTROL_BUTTON_B
		) ||
		app_controls_command_triggered(
			trigger_buttons,
			active_buttons,
			APP_CONTROL_BUTTON_SELECT
		)
	);
}

static enum app_control_action app_controls_confirm_or_cancel_action(
	unsigned int trigger_buttons,
	unsigned int active_buttons,
	unsigned int confirm_button,
	enum app_control_action confirm_action,
	enum app_control_action cancel_action
)
{
	if (
		app_controls_command_triggered(
			trigger_buttons,
			active_buttons,
			confirm_button
		)
	)
	{
		return confirm_action;
	}
	if (app_controls_cancel_command_triggered(trigger_buttons, active_buttons))
		return cancel_action;

	return APP_CONTROL_ACTION_NONE;
}

bool app_controls_should_show_answer_triggered(
	unsigned int trigger_buttons,
	unsigned int active_buttons,
	bool review_answer_revealed
)
{
	return (
		!review_answer_revealed &&
		app_controls_command_triggered(
			trigger_buttons,
			active_buttons,
			APP_CONTROL_BUTTON_A
		)
	);
}

bool app_controls_should_show_answer(unsigned int buttons, bool review_answer_revealed)
{
	return app_controls_should_show_answer_triggered(
		buttons,
		buttons,
		review_answer_revealed
	);
}

bool app_controls_rating_for_trigger(
	unsigned int trigger_buttons,
	unsigned int active_buttons,
	bool review_answer_revealed,
	enum scheduler_rating *rating
)
{
	unsigned int rating_buttons =
		trigger_buttons & APP_CONTROL_FACE_BUTTON_MASK;

	if (!review_answer_revealed)
		return false;
	if (rating == NULL)
		return false;
	if (rating_buttons == 0 || (rating_buttons & (rating_buttons - 1)) != 0)
		return false;
	if ((trigger_buttons & APP_CONTROL_BUTTON_INPUT_MASK) != rating_buttons)
		return false;
	if ((active_buttons & APP_CONTROL_BUTTON_INPUT_MASK) != rating_buttons)
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

bool app_controls_rating_for_buttons(
	unsigned int buttons,
	bool review_answer_revealed,
	enum scheduler_rating *rating
)
{
	return app_controls_rating_for_trigger(
		buttons,
		buttons,
		review_answer_revealed,
		rating
	);
}

enum app_control_action app_controls_classify_action(
	enum app_control_mode mode,
	bool review_answer_revealed,
	bool study_allowed,
	unsigned int trigger_buttons,
	unsigned int active_buttons,
	enum scheduler_rating *rating
)
{
	enum scheduler_rating selected_rating;
	bool scroll_down;

	if (mode == APP_CONTROL_MODE_CONFIRM_EXIT)
		return app_controls_confirm_or_cancel_action(
			trigger_buttons,
			active_buttons,
			APP_CONTROL_BUTTON_A,
			APP_CONTROL_ACTION_CONFIRM_EXIT,
			APP_CONTROL_ACTION_CANCEL_EXIT
		);

	if (
		app_controls_command_triggered(
			trigger_buttons,
			active_buttons,
			APP_CONTROL_BUTTON_START
		)
	)
	{
		return APP_CONTROL_ACTION_OPEN_EXIT;
	}

	if (mode == APP_CONTROL_MODE_CONTROLS)
	{
		if (
			app_controls_command_triggered(
				trigger_buttons,
				active_buttons,
				APP_CONTROL_BUTTON_B
			) ||
			app_controls_command_triggered(
				trigger_buttons,
				active_buttons,
				APP_CONTROL_BUTTON_Y
			) ||
			app_controls_command_triggered(
				trigger_buttons,
				active_buttons,
				APP_CONTROL_BUTTON_SELECT
			)
		)
		{
			return APP_CONTROL_ACTION_CLOSE_CONTROLS;
		}
		if (
			app_controls_command_triggered(
				trigger_buttons,
				active_buttons,
				APP_CONTROL_BUTTON_X
			)
		)
		{
			return APP_CONTROL_ACTION_CYCLE_THEME;
		}

		return APP_CONTROL_ACTION_NONE;
	}

	if (
		app_controls_command_triggered(
			trigger_buttons,
			active_buttons,
			APP_CONTROL_BUTTON_Y
		) &&
		app_controls_can_open(mode, review_answer_revealed)
	)
	{
		return APP_CONTROL_ACTION_OPEN_CONTROLS;
	}

	if (
		mode == APP_CONTROL_MODE_LOAD_ERROR &&
		(
			app_controls_command_triggered(
				trigger_buttons,
				active_buttons,
				APP_CONTROL_BUTTON_B
			) ||
			app_controls_command_triggered(
				trigger_buttons,
				active_buttons,
				APP_CONTROL_BUTTON_SELECT
			)
		)
	)
	{
		return APP_CONTROL_ACTION_RETURN_TO_DECK_SELECT;
	}

	if (
		(
			mode == APP_CONTROL_MODE_SUMMARY ||
			(mode == APP_CONTROL_MODE_REVIEW && !review_answer_revealed)
		) &&
		app_controls_command_triggered(
			trigger_buttons,
			active_buttons,
			APP_CONTROL_BUTTON_B
		)
	)
	{
		return APP_CONTROL_ACTION_RETURN_TO_DECK_SELECT;
	}

	if (
		(mode == APP_CONTROL_MODE_REVIEW || mode == APP_CONTROL_MODE_SUMMARY) &&
		app_controls_command_triggered(
			trigger_buttons,
			active_buttons,
			APP_CONTROL_BUTTON_SELECT
		)
	)
	{
		return APP_CONTROL_ACTION_OPEN_ACTIONS;
	}

	if (
		(mode == APP_CONTROL_MODE_REVIEW || mode == APP_CONTROL_MODE_SUMMARY) &&
		study_allowed &&
		app_controls_command_triggered(
			trigger_buttons,
			active_buttons,
			APP_CONTROL_BUTTON_L
		)
	)
	{
		return APP_CONTROL_ACTION_UNDO;
	}

	if (mode == APP_CONTROL_MODE_ACTIONS)
		return app_controls_confirm_or_cancel_action(
			trigger_buttons,
			active_buttons,
			APP_CONTROL_BUTTON_A,
			APP_CONTROL_ACTION_CHOOSE_ACTION,
			APP_CONTROL_ACTION_CANCEL_ACTIONS
		);

	if (mode == APP_CONTROL_MODE_SETTINGS)
		return app_controls_confirm_or_cancel_action(
			trigger_buttons,
			active_buttons,
			APP_CONTROL_BUTTON_A,
			APP_CONTROL_ACTION_SAVE_SETTINGS,
			APP_CONTROL_ACTION_CANCEL_SETTINGS
		);

	if (mode == APP_CONTROL_MODE_CONFIRM_RESTORE)
		return app_controls_confirm_or_cancel_action(
			trigger_buttons,
			active_buttons,
			APP_CONTROL_BUTTON_X,
			APP_CONTROL_ACTION_CONFIRM_RESTORE,
			APP_CONTROL_ACTION_CANCEL_RESTORE
		);

	if (mode == APP_CONTROL_MODE_CONFIRM_SUSPEND)
		return app_controls_confirm_or_cancel_action(
			trigger_buttons,
			active_buttons,
			APP_CONTROL_BUTTON_X,
			APP_CONTROL_ACTION_CONFIRM_SUSPEND,
			APP_CONTROL_ACTION_CANCEL_SUSPEND
		);

	if (mode == APP_CONTROL_MODE_CONFIRM_RESET)
		return app_controls_confirm_or_cancel_action(
			trigger_buttons,
			active_buttons,
			APP_CONTROL_BUTTON_X,
			APP_CONTROL_ACTION_CONFIRM_RESET,
			APP_CONTROL_ACTION_CANCEL_RESET
		);

	if (mode != APP_CONTROL_MODE_REVIEW)
		return APP_CONTROL_ACTION_NONE;

	if (
		app_controls_up_down_triggered(
			trigger_buttons,
			active_buttons,
			&scroll_down
		)
	)
	{
		return scroll_down ?
			APP_CONTROL_ACTION_SCROLL_DOWN :
			APP_CONTROL_ACTION_SCROLL_UP;
	}

	if (
		study_allowed &&
		app_controls_command_triggered(
			trigger_buttons,
			active_buttons,
			APP_CONTROL_BUTTON_R
		)
	)
	{
		return APP_CONTROL_ACTION_OPEN_SUSPEND;
	}

	if (!review_answer_revealed)
	{
		if (
			app_controls_should_show_answer_triggered(
				trigger_buttons,
				active_buttons,
				review_answer_revealed
			)
		)
		{
			return APP_CONTROL_ACTION_SHOW_ANSWER;
		}

		return APP_CONTROL_ACTION_NONE;
	}

	if (
		rating != NULL &&
		app_controls_rating_for_trigger(
			trigger_buttons,
			active_buttons,
			review_answer_revealed,
			&selected_rating
		)
	)
	{
		*rating = selected_rating;
		return APP_CONTROL_ACTION_RATE;
	}

	return APP_CONTROL_ACTION_NONE;
}
