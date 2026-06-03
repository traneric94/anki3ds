#include "app_controls.h"

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>

static void check(bool condition, const char *message)
{
	if (!condition)
	{
		fprintf(stderr, "FAIL: %s\n", message);
		abort();
	}
}

static void test_maps_3ds_keys_to_app_buttons(void)
{
	check(
		app_controls_buttons_from_3ds_keys(KEY_A) == APP_CONTROL_BUTTON_A,
		"KEY_A maps to A"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_B) == APP_CONTROL_BUTTON_B,
		"KEY_B maps to B"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_X) == APP_CONTROL_BUTTON_X,
		"KEY_X maps to X"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_Y) == APP_CONTROL_BUTTON_Y,
		"KEY_Y maps to Y"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_L) == APP_CONTROL_BUTTON_L,
		"KEY_L maps to L"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_R) == APP_CONTROL_BUTTON_R,
		"KEY_R maps to R"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_DUP) == APP_CONTROL_BUTTON_UP,
		"KEY_DUP maps to Up"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_DDOWN) == APP_CONTROL_BUTTON_DOWN,
		"KEY_DDOWN maps to Down"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_DLEFT) == APP_CONTROL_BUTTON_LEFT,
		"KEY_DLEFT maps to Left"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_DRIGHT) == APP_CONTROL_BUTTON_RIGHT,
		"KEY_DRIGHT maps to Right"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_CPAD_UP) == APP_CONTROL_BUTTON_UP,
		"KEY_CPAD_UP maps to Up"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_CPAD_DOWN) == APP_CONTROL_BUTTON_DOWN,
		"KEY_CPAD_DOWN maps to Down"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_CPAD_LEFT) == APP_CONTROL_BUTTON_LEFT,
		"KEY_CPAD_LEFT maps to Left"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_CPAD_RIGHT) == APP_CONTROL_BUTTON_RIGHT,
		"KEY_CPAD_RIGHT maps to Right"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_UP) == APP_CONTROL_BUTTON_UP,
		"KEY_UP alias maps to Up"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_DOWN) == APP_CONTROL_BUTTON_DOWN,
		"KEY_DOWN alias maps to Down"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_LEFT) == APP_CONTROL_BUTTON_LEFT,
		"KEY_LEFT alias maps to Left"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_RIGHT) == APP_CONTROL_BUTTON_RIGHT,
		"KEY_RIGHT alias maps to Right"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_SELECT) == APP_CONTROL_BUTTON_SELECT,
		"KEY_SELECT maps to Select"
	);
	check(
		app_controls_buttons_from_3ds_keys(KEY_START) == APP_CONTROL_BUTTON_START,
		"KEY_START maps to Start"
	);
}

static void test_maps_combined_3ds_keys_to_combined_app_buttons(void)
{
	unsigned int keys =
		KEY_A |
		KEY_B |
		KEY_X |
		KEY_Y |
		KEY_L |
		KEY_R |
		KEY_UP |
		KEY_DOWN |
		KEY_LEFT |
		KEY_RIGHT |
		KEY_SELECT |
		KEY_START;

	check(
		app_controls_buttons_from_3ds_keys(keys) ==
			APP_CONTROL_BUTTON_INPUT_MASK,
		"combined 3DS keys map to combined app buttons"
	);
}

int main(void)
{
	test_maps_3ds_keys_to_app_buttons();
	test_maps_combined_3ds_keys_to_combined_app_buttons();
	printf("app controls 3ds key tests passed\n");
	return 0;
}
