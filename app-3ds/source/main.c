#include <3ds.h>

#include <stdbool.h>
#include <time.h>

#include "app_input_frame.h"
#include "app_power.h"
#include "app_renderer_c2d.h"
#include "app_shell.h"
#include "study_3ds_key_map.h"

static void app_wait_for_idle_input(unsigned int idle_wait_count)
{
	hidWaitForAnyEvent(
		true,
		0,
		app_power_idle_input_wait_ns(idle_wait_count)
	);
}

int main(int argc, char *argv[])
{
	static struct app_renderer_c2d renderer;
	static struct app_shell shell;
	static struct study_control_repeat navigation_repeat;
	static struct app_screen_model screen_model;
	unsigned int idle_wait_count = 0;
	bool held_navigation_wait = false;

	(void)argc;
	(void)argv;

	gfxInitDefault();
	if (!app_renderer_c2d_init(&renderer))
	{
		gfxExit();
		return 1;
	}

	study_control_repeat_init(&navigation_repeat);
	app_shell_init(&shell, NULL, time(NULL), time(NULL), time(NULL));

	while (aptMainLoop())
	{
		u32 keys_down;
		u32 keys_held;
		struct app_input_frame input_frame;
		size_t max_answer_scroll_offset;
		size_t max_front_scroll_offset;
		time_t now;

		if (app_shell_redraw_needed(&shell))
		{
			app_shell_build_screen_model(&shell, &screen_model);
			app_renderer_c2d_draw(&renderer, &screen_model);
			app_shell_mark_drawn(&shell);
			idle_wait_count = 0;
		}
		else
		{
			if (held_navigation_wait)
			{
				gspWaitForVBlank();
			}
			else
			{
				app_wait_for_idle_input(idle_wait_count);
				idle_wait_count =
					app_power_next_idle_input_wait_count(idle_wait_count);
			}
		}

		hidScanInput();
		keys_down = hidKeysDown();
		keys_held = hidKeysHeld();
		app_input_frame_build(
			&input_frame,
			&navigation_repeat,
			app_shell_screen_kind(&shell),
			study_3ds_key_map_buttons_from_keys(keys_down),
			study_3ds_key_map_buttons_from_keys(keys_held),
			study_3ds_key_map_dpad_buttons_from_keys(keys_down),
			study_3ds_key_map_dpad_buttons_from_keys(keys_held),
			study_3ds_key_map_cpad_buttons_from_keys(keys_down),
			study_3ds_key_map_cpad_buttons_from_keys(keys_held)
		);
		if (input_frame.buttons != 0 || input_frame.held_navigation_wait)
			idle_wait_count = 0;

		now = time(NULL);
		max_answer_scroll_offset = app_renderer_c2d_review_body_max_scroll_offset(
			app_shell_review_answer_scroll_text(&shell),
			true
		);
		max_front_scroll_offset = app_renderer_c2d_review_body_max_scroll_offset(
			app_shell_review_front_scroll_text(&shell),
			false
		);
		if (
			app_shell_handle_frame(
				&shell,
				input_frame.buttons,
				input_frame.dpad_buttons,
				input_frame.cpad_buttons,
				now,
				max_answer_scroll_offset,
				max_front_scroll_offset
			)
		)
		{
			break;
		}
		held_navigation_wait = input_frame.held_navigation_wait;
	}

	app_shell_shutdown(&shell);
	app_renderer_c2d_fini(&renderer);
	gfxExit();
	return 0;
}
