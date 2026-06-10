#include "app_input_frame.h"

#include "app_input_policy.h"

static unsigned int app_input_frame_navigation_buttons(void)
{
	return STUDY_CONTROL_BUTTON_UP |
		STUDY_CONTROL_BUTTON_DOWN |
		STUDY_CONTROL_BUTTON_LEFT |
		STUDY_CONTROL_BUTTON_RIGHT;
}

static bool app_input_frame_has_navigation_source_conflict(
	unsigned int dpad_buttons_down,
	unsigned int dpad_buttons_held,
	unsigned int cpad_buttons_down,
	unsigned int cpad_buttons_held
)
{
	unsigned int navigation_buttons = app_input_frame_navigation_buttons();
	unsigned int dpad_buttons =
		(dpad_buttons_down | dpad_buttons_held) & navigation_buttons;
	unsigned int cpad_buttons =
		(cpad_buttons_down | cpad_buttons_held) & navigation_buttons;

	return dpad_buttons != 0 && cpad_buttons != 0;
}

static unsigned int app_input_frame_source_navigation_buttons(
	unsigned int safe_buttons,
	unsigned int source_buttons_down,
	unsigned int source_buttons_held,
	unsigned int repeat_buttons
)
{
	unsigned int navigation_buttons = app_input_frame_navigation_buttons();
	unsigned int source_buttons = source_buttons_down;

	if (
		repeat_buttons != 0 &&
		(source_buttons_held & repeat_buttons) == repeat_buttons
	)
	{
		source_buttons |= repeat_buttons;
	}

	if (
		!study_controls_has_single_button(safe_buttons) ||
		(safe_buttons & navigation_buttons) == 0
	)
	{
		return 0;
	}

	source_buttons &= navigation_buttons;
	return (source_buttons & safe_buttons) == safe_buttons ? safe_buttons : 0;
}

void app_input_frame_build(
	struct app_input_frame *frame,
	struct study_control_repeat *repeat,
	enum app_screen_model_kind screen_kind,
	unsigned int buttons_down,
	unsigned int buttons_held,
	unsigned int dpad_buttons_down,
	unsigned int dpad_buttons_held,
	unsigned int cpad_buttons_down,
	unsigned int cpad_buttons_held
)
{
	unsigned int repeat_mask;
	unsigned int repeat_buttons = 0;
	bool navigation_source_conflict;

	if (frame == NULL)
		return;

	repeat_mask = app_input_policy_repeat_mask(screen_kind);
	navigation_source_conflict = app_input_frame_has_navigation_source_conflict(
		dpad_buttons_down,
		dpad_buttons_held,
		cpad_buttons_down,
		cpad_buttons_held
	);
	if (navigation_source_conflict && repeat != NULL)
	{
		study_control_repeat_reset(repeat);
	}
	else if (repeat != NULL)
	{
		repeat_buttons = study_control_repeat_buttons(
			repeat,
			buttons_down,
			buttons_held,
			repeat_mask
		);
	}
	frame->buttons = study_controls_chord_safe_buttons(
		buttons_down | repeat_buttons,
		buttons_held
	);
	if (
		navigation_source_conflict &&
		study_controls_has_single_button(frame->buttons) &&
		(frame->buttons & app_input_frame_navigation_buttons()) != 0
	)
	{
		frame->buttons = 0;
	}
	frame->dpad_buttons = app_input_frame_source_navigation_buttons(
		frame->buttons,
		dpad_buttons_down,
		dpad_buttons_held,
		repeat_buttons
	);
	frame->cpad_buttons = app_input_frame_source_navigation_buttons(
		frame->buttons,
		cpad_buttons_down,
		cpad_buttons_held,
		repeat_buttons
	);
	frame->held_navigation_wait =
		!navigation_source_conflict &&
		repeat != NULL &&
		study_control_repeat_wait_needed(buttons_held, repeat_mask);
}
