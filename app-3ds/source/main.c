#include <3ds.h>
#include <stdio.h>

#define APP_VERSION "0.1.0-dev"

static void draw_proof_screen(void)
{
	consoleClear();
	printf("\x1b[1;1Hanki3ds");
	printf("\x1b[3;1HM1 Toolchain Proof");
	printf("\x1b[5;1HVersion: %s", APP_VERSION);
	printf("\x1b[7;1HBuild: %s %s", __DATE__, __TIME__);
	printf("\x1b[10;1HPress START to exit.");
	printf("\x1b[12;1HNext checkpoint: button input.");
}

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	draw_proof_screen();

	while (aptMainLoop())
	{
		gspWaitForVBlank();
		gfxSwapBuffers();
		hidScanInput();

		u32 keys_down = hidKeysDown();
		if (keys_down & KEY_START)
			break;
	}

	gfxExit();
	return 0;
}
