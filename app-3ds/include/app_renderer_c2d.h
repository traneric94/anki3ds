#ifndef ANKI3DS_APP_RENDERER_C2D_H
#define ANKI3DS_APP_RENDERER_C2D_H

#include <stdbool.h>
#include <stddef.h>

#include "app_screen_model.h"

#define APP_RENDERER_C2D_STATE_BYTES 256

union app_renderer_c2d_storage
{
	void *pointer_alignment;
	double double_alignment;
	long long integer_alignment;
	unsigned char bytes[APP_RENDERER_C2D_STATE_BYTES];
};

struct app_renderer_c2d
{
	union app_renderer_c2d_storage storage;
};

bool app_renderer_c2d_init(struct app_renderer_c2d *renderer);
size_t app_renderer_c2d_review_body_max_scroll_offset(
	const char *text,
	bool answer_visible
);
void app_renderer_c2d_draw(
	struct app_renderer_c2d *renderer,
	const struct app_screen_model *model
);
void app_renderer_c2d_fini(struct app_renderer_c2d *renderer);

#endif
