#include "study_3ds_key_map.h"

#include "study_controls.h"

#include <assert.h>

static void test_3ds_keys_map_to_logical_buttons(void)
{
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_A) ==
		STUDY_CONTROL_BUTTON_A
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_B) ==
		STUDY_CONTROL_BUTTON_B
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_X) ==
		STUDY_CONTROL_BUTTON_X
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_Y) ==
		STUDY_CONTROL_BUTTON_Y
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_L) ==
		STUDY_CONTROL_BUTTON_L
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_R) ==
		STUDY_CONTROL_BUTTON_R
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_SELECT) ==
		STUDY_CONTROL_BUTTON_SELECT
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_START) ==
		STUDY_CONTROL_BUTTON_START
	);
}

static void test_dpad_and_circle_pad_aliases_share_buttons(void)
{
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_DUP) ==
		STUDY_CONTROL_BUTTON_UP
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_CPAD_UP) ==
		STUDY_CONTROL_BUTTON_UP
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_UP) ==
		STUDY_CONTROL_BUTTON_UP
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_DDOWN) ==
		STUDY_CONTROL_BUTTON_DOWN
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_CPAD_DOWN) ==
		STUDY_CONTROL_BUTTON_DOWN
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_DOWN) ==
		STUDY_CONTROL_BUTTON_DOWN
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_DLEFT) ==
		STUDY_CONTROL_BUTTON_LEFT
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_CPAD_LEFT) ==
		STUDY_CONTROL_BUTTON_LEFT
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_LEFT) ==
		STUDY_CONTROL_BUTTON_LEFT
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_DRIGHT) ==
		STUDY_CONTROL_BUTTON_RIGHT
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_CPAD_RIGHT) ==
		STUDY_CONTROL_BUTTON_RIGHT
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_RIGHT) ==
		STUDY_CONTROL_BUTTON_RIGHT
	);
}

static void test_dpad_and_circle_pad_sources_can_be_read_separately(void)
{
	assert(
		study_3ds_key_map_dpad_buttons_from_keys(KEY_DUP | KEY_CPAD_DOWN) ==
		STUDY_CONTROL_BUTTON_UP
	);
	assert(
		study_3ds_key_map_cpad_buttons_from_keys(KEY_DUP | KEY_CPAD_DOWN) ==
		STUDY_CONTROL_BUTTON_DOWN
	);
	assert(
		study_3ds_key_map_dpad_buttons_from_keys(KEY_A | KEY_DLEFT) ==
		STUDY_CONTROL_BUTTON_LEFT
	);
	assert(
		study_3ds_key_map_cpad_buttons_from_keys(KEY_A | KEY_DLEFT) == 0
	);
	assert(
		study_3ds_key_map_buttons_from_keys(KEY_DDOWN | KEY_CPAD_DOWN) ==
		STUDY_CONTROL_BUTTON_DOWN
	);
	assert(
		study_3ds_key_map_dpad_buttons_from_keys(KEY_DDOWN | KEY_CPAD_DOWN) ==
		STUDY_CONTROL_BUTTON_DOWN
	);
	assert(
		study_3ds_key_map_cpad_buttons_from_keys(KEY_DDOWN | KEY_CPAD_DOWN) ==
		STUDY_CONTROL_BUTTON_DOWN
	);
}

static void test_physical_chords_remain_inert_after_mapping(void)
{
	enum study_control_rating rating = STUDY_CONTROL_RATING_NONE;
	unsigned int buttons;

	buttons = study_3ds_key_map_buttons_from_keys(KEY_A | KEY_B);
	assert(
		study_controls_interpret(buttons, true, true, &rating) ==
		STUDY_CONTROL_ACTION_NONE
	);

	buttons = study_3ds_key_map_buttons_from_keys(KEY_START | KEY_A);
	assert(
		study_controls_interpret(buttons, false, true, &rating) ==
		STUDY_CONTROL_ACTION_NONE
	);

	buttons = study_3ds_key_map_buttons_from_keys(KEY_R | KEY_DDOWN);
	assert(
		study_controls_interpret(buttons, false, true, &rating) ==
		STUDY_CONTROL_ACTION_NONE
	);

	buttons = study_3ds_key_map_buttons_from_keys(KEY_CPAD_UP | KEY_DDOWN);
	assert(
		study_controls_interpret(buttons, false, true, &rating) ==
		STUDY_CONTROL_ACTION_NONE
	);
}

int main(void)
{
	test_3ds_keys_map_to_logical_buttons();
	test_dpad_and_circle_pad_aliases_share_buttons();
	test_dpad_and_circle_pad_sources_can_be_read_separately();
	test_physical_chords_remain_inert_after_mapping();
	return 0;
}
