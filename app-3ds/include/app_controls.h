#ifndef ANKI3DS_APP_CONTROLS_H
#define ANKI3DS_APP_CONTROLS_H

#include <stdbool.h>

#include "scheduler.h"

#define APP_CONTROL_BUTTON_A      (1u << 0)
#define APP_CONTROL_BUTTON_B      (1u << 1)
#define APP_CONTROL_BUTTON_X      (1u << 2)
#define APP_CONTROL_BUTTON_Y      (1u << 3)
#define APP_CONTROL_BUTTON_L      (1u << 4)
#define APP_CONTROL_BUTTON_R      (1u << 5)
#define APP_CONTROL_BUTTON_UP     (1u << 6)
#define APP_CONTROL_BUTTON_DOWN   (1u << 7)
#define APP_CONTROL_BUTTON_LEFT   (1u << 8)
#define APP_CONTROL_BUTTON_RIGHT  (1u << 9)
#define APP_CONTROL_BUTTON_SELECT (1u << 10)
#define APP_CONTROL_BUTTON_START  (1u << 11)
#define APP_CONTROL_FACE_BUTTON_MASK \
	(APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_B | \
	APP_CONTROL_BUTTON_X | APP_CONTROL_BUTTON_Y)
#define APP_CONTROL_COMMAND_BUTTON_MASK \
	(APP_CONTROL_FACE_BUTTON_MASK | APP_CONTROL_BUTTON_L | APP_CONTROL_BUTTON_R | \
	APP_CONTROL_BUTTON_SELECT | APP_CONTROL_BUTTON_START)
#define APP_CONTROL_BUTTON_NAVIGATION_MASK \
	(APP_CONTROL_BUTTON_UP | APP_CONTROL_BUTTON_DOWN | \
	APP_CONTROL_BUTTON_LEFT | APP_CONTROL_BUTTON_RIGHT)
#define APP_CONTROL_BUTTON_INPUT_MASK \
	(APP_CONTROL_COMMAND_BUTTON_MASK | APP_CONTROL_BUTTON_NAVIGATION_MASK)
#define APP_CONTROL_REPEAT_INITIAL_TICKS 6
#define APP_CONTROL_REPEAT_INTERVAL_TICKS 2

enum app_control_mode
{
	APP_CONTROL_MODE_DECK_SELECT,
	APP_CONTROL_MODE_LOAD_ERROR,
	APP_CONTROL_MODE_REVIEW,
	APP_CONTROL_MODE_SUMMARY,
	APP_CONTROL_MODE_ACTIONS,
	APP_CONTROL_MODE_SETTINGS,
	APP_CONTROL_MODE_CONTROLS,
	APP_CONTROL_MODE_CONFIRM_RESET,
	APP_CONTROL_MODE_CONFIRM_EXIT,
};

struct app_control_repeat
{
	unsigned int buttons;
	unsigned int tick_count;
};

unsigned int app_controls_buttons_from_3ds_keys(unsigned int keys);
void app_controls_repeat_init(struct app_control_repeat *repeat);
void app_controls_repeat_reset(struct app_control_repeat *repeat);
unsigned int app_controls_repeat_buttons(
	struct app_control_repeat *repeat,
	unsigned int buttons_down,
	unsigned int buttons_held
);
bool app_controls_can_open(
	enum app_control_mode mode,
	bool review_answer_revealed
);
bool app_controls_up_down_direction(unsigned int buttons, bool *down);
bool app_controls_left_right_direction(unsigned int buttons, bool *right);
bool app_controls_single_command(
	unsigned int buttons,
	unsigned int command_button,
	unsigned int command_mask
);
bool app_controls_should_show_answer(unsigned int buttons, bool review_answer_revealed);
bool app_controls_rating_for_buttons(
	unsigned int buttons,
	bool review_answer_revealed,
	enum scheduler_rating *rating
);

#endif
