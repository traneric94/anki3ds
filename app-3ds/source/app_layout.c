#include "app_layout.h"

bool app_layout_rect_fits_top_screen(
	int x,
	int y,
	unsigned int width,
	unsigned int height
)
{
	unsigned int left;
	unsigned int top;

	if (x < 0 || y < 0 || width == 0 || height == 0)
		return false;
	if (
		width > APP_LAYOUT_TOP_SCREEN_WIDTH ||
		height > APP_LAYOUT_TOP_SCREEN_HEIGHT
	)
	{
		return false;
	}

	left = (unsigned int)x;
	top = (unsigned int)y;
	return (
		left <= APP_LAYOUT_TOP_SCREEN_WIDTH - width &&
		top <= APP_LAYOUT_TOP_SCREEN_HEIGHT - height
	);
}
