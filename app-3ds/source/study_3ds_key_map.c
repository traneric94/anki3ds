#include "study_3ds_key_map.h"

#include "study_controls.h"

unsigned int study_3ds_key_map_buttons_from_keys(u32 keys_down)
{
	unsigned int buttons = 0;

	if ((keys_down & KEY_A) != 0)
		buttons |= STUDY_CONTROL_BUTTON_A;
	if ((keys_down & KEY_B) != 0)
		buttons |= STUDY_CONTROL_BUTTON_B;
	if ((keys_down & KEY_X) != 0)
		buttons |= STUDY_CONTROL_BUTTON_X;
	if ((keys_down & KEY_Y) != 0)
		buttons |= STUDY_CONTROL_BUTTON_Y;
	if ((keys_down & KEY_L) != 0)
		buttons |= STUDY_CONTROL_BUTTON_L;
	if ((keys_down & KEY_R) != 0)
		buttons |= STUDY_CONTROL_BUTTON_R;
	if ((keys_down & KEY_START) != 0)
		buttons |= STUDY_CONTROL_BUTTON_START;
	if ((keys_down & KEY_SELECT) != 0)
		buttons |= STUDY_CONTROL_BUTTON_SELECT;

	buttons |= study_3ds_key_map_dpad_buttons_from_keys(keys_down);
	buttons |= study_3ds_key_map_cpad_buttons_from_keys(keys_down);

	return buttons;
}

unsigned int study_3ds_key_map_dpad_buttons_from_keys(u32 keys_down)
{
	unsigned int buttons = 0;

	if ((keys_down & KEY_DUP) != 0)
		buttons |= STUDY_CONTROL_BUTTON_UP;
	if ((keys_down & KEY_DDOWN) != 0)
		buttons |= STUDY_CONTROL_BUTTON_DOWN;
	if ((keys_down & KEY_DLEFT) != 0)
		buttons |= STUDY_CONTROL_BUTTON_LEFT;
	if ((keys_down & KEY_DRIGHT) != 0)
		buttons |= STUDY_CONTROL_BUTTON_RIGHT;

	return buttons;
}

unsigned int study_3ds_key_map_cpad_buttons_from_keys(u32 keys_down)
{
	unsigned int buttons = 0;

	if ((keys_down & KEY_CPAD_UP) != 0)
		buttons |= STUDY_CONTROL_BUTTON_UP;
	if ((keys_down & KEY_CPAD_DOWN) != 0)
		buttons |= STUDY_CONTROL_BUTTON_DOWN;
	if ((keys_down & KEY_CPAD_LEFT) != 0)
		buttons |= STUDY_CONTROL_BUTTON_LEFT;
	if ((keys_down & KEY_CPAD_RIGHT) != 0)
		buttons |= STUDY_CONTROL_BUTTON_RIGHT;

	return buttons;
}
