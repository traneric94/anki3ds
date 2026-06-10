#ifndef ANKI3DS_APP_INPUT_FRAME_H
#define ANKI3DS_APP_INPUT_FRAME_H

#include <stdbool.h>

#include "app_screen_model.h"
#include "study_controls.h"

struct app_input_frame
{
	unsigned int buttons;
	unsigned int dpad_buttons;
	unsigned int cpad_buttons;
	bool held_navigation_wait;
};

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
);

#endif
