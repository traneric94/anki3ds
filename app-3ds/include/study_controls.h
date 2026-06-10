#ifndef ANKI3DS_STUDY_CONTROLS_H
#define ANKI3DS_STUDY_CONTROLS_H

#include <stdbool.h>

enum study_control_button
{
	STUDY_CONTROL_BUTTON_A = 1u << 0,
	STUDY_CONTROL_BUTTON_B = 1u << 1,
	STUDY_CONTROL_BUTTON_X = 1u << 2,
	STUDY_CONTROL_BUTTON_Y = 1u << 3,
	STUDY_CONTROL_BUTTON_L = 1u << 4,
	STUDY_CONTROL_BUTTON_R = 1u << 5,
	STUDY_CONTROL_BUTTON_START = 1u << 6,
	STUDY_CONTROL_BUTTON_UP = 1u << 7,
	STUDY_CONTROL_BUTTON_DOWN = 1u << 8,
	STUDY_CONTROL_BUTTON_SELECT = 1u << 9,
	STUDY_CONTROL_BUTTON_LEFT = 1u << 10,
	STUDY_CONTROL_BUTTON_RIGHT = 1u << 11,
};

enum study_control_rating
{
	STUDY_CONTROL_RATING_AGAIN,
	STUDY_CONTROL_RATING_HARD,
	STUDY_CONTROL_RATING_GOOD,
	STUDY_CONTROL_RATING_EASY,
	STUDY_CONTROL_RATING_NONE,
};

enum study_control_action
{
	STUDY_CONTROL_ACTION_NONE,
	STUDY_CONTROL_ACTION_EXIT,
	STUDY_CONTROL_ACTION_DECKS,
	STUDY_CONTROL_ACTION_THEME,
	STUDY_CONTROL_ACTION_TOGGLE_HELP,
	STUDY_CONTROL_ACTION_UNDO,
	STUDY_CONTROL_ACTION_SUSPEND_RESTORE,
	STUDY_CONTROL_ACTION_REVEAL,
	STUDY_CONTROL_ACTION_RATE,
	STUDY_CONTROL_ACTION_SCROLL_UP,
	STUDY_CONTROL_ACTION_SCROLL_DOWN,
};

#define STUDY_CONTROL_REPEAT_INITIAL_FRAMES 12u
#define STUDY_CONTROL_REPEAT_INTERVAL_FRAMES 4u

struct study_control_repeat
{
	unsigned int held_buttons;
	unsigned int frame_count;
};

void study_control_repeat_init(struct study_control_repeat *repeat);
void study_control_repeat_reset(struct study_control_repeat *repeat);
bool study_controls_has_single_button(unsigned int buttons);
bool study_control_repeat_wait_needed(
	unsigned int held_buttons,
	unsigned int repeat_mask
);
unsigned int study_control_repeat_buttons(
	struct study_control_repeat *repeat,
	unsigned int buttons_down,
	unsigned int buttons_held,
	unsigned int repeat_mask
);
unsigned int study_controls_chord_safe_buttons(
	unsigned int action_buttons,
	unsigned int held_buttons
);
enum study_control_action study_controls_interpret(
	unsigned int buttons,
	bool answer_visible,
	bool undo_available,
	enum study_control_rating *rating
);

#endif
