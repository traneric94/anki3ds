#include <3ds.h>
#include <stdbool.h>
#include <stdio.h>

#define APP_VERSION "0.2.0-dev"
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

struct app_state
{
	const char *last_button;
	const char *last_keyboard_key;
	u32 press_count;
};

struct button_mapping
{
	u32 mask;
	const char *button;
	const char *keyboard_key;
};

static const struct button_mapping BUTTON_MAPPINGS[] = {
	{ KEY_A, "A", "A" },
	{ KEY_B, "B", "S" },
	{ KEY_X, "X", "Z" },
	{ KEY_Y, "Y", "X" },
	{ KEY_DUP, "D-pad Up", "T" },
	{ KEY_DLEFT, "D-pad Left", "F" },
	{ KEY_DDOWN, "D-pad Down", "G" },
	{ KEY_DRIGHT, "D-pad Right", "H" },
	{ KEY_L, "L", "Q" },
	{ KEY_R, "R", "W" },
	{ KEY_SELECT, "SELECT", "N" },
};

static void app_init(struct app_state *app)
{
	app->last_button = "none";
	app->last_keyboard_key = "-";
	app->press_count = 0;
}

static bool app_record_button(struct app_state *app, u32 keys_down)
{
	for (size_t i = 0; i < ARRAY_SIZE(BUTTON_MAPPINGS); i++)
	{
		if (keys_down & BUTTON_MAPPINGS[i].mask)
		{
			app->last_button = BUTTON_MAPPINGS[i].button;
			app->last_keyboard_key = BUTTON_MAPPINGS[i].keyboard_key;
			app->press_count++;
			return true;
		}
	}

	return false;
}

static void draw_input_screen(const struct app_state *app)
{
	consoleClear();
	printf("\x1b[1;1Hanki3ds");
	printf("\x1b[3;1HM2 Input Proof");
	printf("\x1b[5;1HVersion: %s", APP_VERSION);
	printf("\x1b[7;1HBuild: %s %s", __DATE__, __TIME__);
	printf("\x1b[10;1HLast button: %s", app->last_button);
	printf("\x1b[11;1HAzahar key:  %s", app->last_keyboard_key);
	printf("\x1b[12;1HPress count: %lu", (unsigned long)app->press_count);
	printf("\x1b[15;1HDefaults: A/S/Z/X -> A/B/X/Y");
	printf("\x1b[16;1H          T/F/G/H -> D-pad");
	printf("\x1b[17;1H          Q/W -> L/R, N -> SELECT");
	printf("\x1b[20;1HPress START / M to exit.");
}

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	struct app_state app;
	app_init(&app);

	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	draw_input_screen(&app);

	while (aptMainLoop())
	{
		gspWaitForVBlank();
		gfxSwapBuffers();
		hidScanInput();

		u32 keys_down = hidKeysDown();
		if (keys_down & KEY_START)
			break;

		if (app_record_button(&app, keys_down))
			draw_input_screen(&app);
	}

	gfxExit();
	return 0;
}
