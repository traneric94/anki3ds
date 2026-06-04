#include <3ds.h>

#include <stdbool.h>
#include <stdio.h>

#define TOP_SCREEN_WIDTH 400
#define BOTTOM_SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define BYTES_PER_PIXEL 3
#define TOP_FRAMEBUFFER_SIZE (TOP_SCREEN_WIDTH * SCREEN_HEIGHT * BYTES_PER_PIXEL)
#define BOTTOM_FRAMEBUFFER_SIZE (BOTTOM_SCREEN_WIDTH * SCREEN_HEIGHT * BYTES_PER_PIXEL)
#define THEME_PATH_PREFIX "sdmc:/3ds/anki3ds/fe-themes/"

struct theme_asset
{
	const char *name;
	const char *top_path;
	const char *bottom_path;
};

static const struct theme_asset themes[] = {
	{
		"Amber",
		THEME_PATH_PREFIX "fe_bg_amber_top_400x240_bgr888_fb.bin",
		THEME_PATH_PREFIX "fe_bg_amber_bottom_320x240_bgr888_fb.bin",
	},
	{
		"Forest",
		THEME_PATH_PREFIX "fe_bg_forest_top_400x240_bgr888_fb.bin",
		THEME_PATH_PREFIX "fe_bg_forest_bottom_320x240_bgr888_fb.bin",
	},
	{
		"Ruby",
		THEME_PATH_PREFIX "fe_bg_ruby_top_400x240_bgr888_fb.bin",
		THEME_PATH_PREFIX "fe_bg_ruby_bottom_320x240_bgr888_fb.bin",
	},
	{
		"Chalk",
		THEME_PATH_PREFIX "fe_bg_chalk_top_400x240_bgr888_fb.bin",
		THEME_PATH_PREFIX "fe_bg_chalk_bottom_320x240_bgr888_fb.bin",
	},
};

static size_t theme_count(void)
{
	return sizeof(themes) / sizeof(themes[0]);
}

static size_t framebuffer_offset(int x, int y)
{
	return (size_t)(x * SCREEN_HEIGHT + (SCREEN_HEIGHT - 1 - y)) * BYTES_PER_PIXEL;
}

static void write_pixel_bgr(
	u8 *framebuffer,
	int screen_width,
	int x,
	int y,
	u8 b,
	u8 g,
	u8 r
)
{
	size_t offset;

	if (x < 0 || y < 0 || x >= screen_width || y >= SCREEN_HEIGHT)
		return;

	offset = framebuffer_offset(x, y);
	framebuffer[offset] = b;
	framebuffer[offset + 1] = g;
	framebuffer[offset + 2] = r;
}

static void draw_missing_asset_pattern(
	u8 *framebuffer,
	int screen_width,
	u8 accent_b,
	u8 accent_g,
	u8 accent_r
)
{
	int x;
	int y;

	for (y = 0; y < SCREEN_HEIGHT; y++)
	{
		for (x = 0; x < screen_width; x++)
		{
			bool stripe = ((x / 24) + (y / 24)) % 2 == 0;
			u8 shade = stripe ? 0x24 : 0x0B;

			write_pixel_bgr(
				framebuffer,
				screen_width,
				x,
				y,
				stripe ? accent_b : shade,
				stripe ? accent_g : shade,
				stripe ? accent_r : shade
			);
		}
	}
}

static bool read_exact_file(const char *path, u8 *destination, size_t expected_size)
{
	FILE *file;
	size_t bytes_read;
	bool exact_size;

	file = fopen(path, "rb");
	if (file == NULL)
		return false;

	bytes_read = fread(destination, 1, expected_size, file);
	exact_size = bytes_read == expected_size && fgetc(file) == EOF && !ferror(file);
	fclose(file);

	return exact_size;
}

static bool draw_theme(size_t theme_index)
{
	u8 *top_framebuffer;
	u8 *bottom_framebuffer;
	bool top_loaded;
	bool bottom_loaded;

	top_framebuffer = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	bottom_framebuffer = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);

	top_loaded = read_exact_file(
		themes[theme_index].top_path,
		top_framebuffer,
		TOP_FRAMEBUFFER_SIZE
	);
	bottom_loaded = read_exact_file(
		themes[theme_index].bottom_path,
		bottom_framebuffer,
		BOTTOM_FRAMEBUFFER_SIZE
	);

	if (!top_loaded)
		draw_missing_asset_pattern(
			top_framebuffer,
			TOP_SCREEN_WIDTH,
			0x18,
			0x22,
			0x98
		);
	if (!bottom_loaded)
		draw_missing_asset_pattern(
			bottom_framebuffer,
			BOTTOM_SCREEN_WIDTH,
			0x88,
			0x40,
			0x12
		);

	gfxFlushBuffers();
	gfxSwapBuffers();

	return top_loaded && bottom_loaded;
}

static size_t previous_theme_index(size_t theme_index)
{
	if (theme_index == 0)
		return theme_count() - 1;

	return theme_index - 1;
}

static size_t next_theme_index(size_t theme_index)
{
	return (theme_index + 1) % theme_count();
}

int main(int argc, char **argv)
{
	size_t theme_index = 0;
	(void)argc;
	(void)argv;

	gfxInitDefault();
	gfxSetDoubleBuffering(GFX_TOP, false);
	gfxSetDoubleBuffering(GFX_BOTTOM, false);
	draw_theme(theme_index);

	while (aptMainLoop())
	{
		u32 keys_down;

		hidScanInput();
		keys_down = hidKeysDown();

		if (keys_down & KEY_START)
			break;
		if (keys_down & (KEY_DLEFT | KEY_L | KEY_B))
		{
			theme_index = previous_theme_index(theme_index);
			draw_theme(theme_index);
		}
		else if (keys_down & (KEY_DRIGHT | KEY_R | KEY_A))
		{
			theme_index = next_theme_index(theme_index);
			draw_theme(theme_index);
		}
		else if (keys_down & KEY_X)
		{
			draw_theme(theme_index);
		}

		gspWaitForVBlank();
	}

	gfxExit();
	return 0;
}
