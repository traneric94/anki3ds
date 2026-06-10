#include "app_renderer_c2d.h"

#include "app_text.h"
#include "fe_forest_bottom_legend_t3x.h"
#include "fe_forest_top_legend_t3x.h"

#include <citro2d.h>
#include <stdio.h>
#include <string.h>

#define APP_RENDERER_C2D_TEXT_BUFFER_GLYPHS 8192
#define APP_RENDERER_C2D_REVIEW_FRONT_WINDOW_COLUMNS 40
#define APP_RENDERER_C2D_REVIEW_FRONT_WINDOW_ROWS 7
#define APP_RENDERER_C2D_REVIEW_ANSWER_WINDOW_COLUMNS 31
#define APP_RENDERER_C2D_REVIEW_ANSWER_WINDOW_ROWS 6
#define APP_RENDERER_C2D_TEXT_SCALE 0.50f
#define APP_RENDERER_C2D_TEXT_NATIVE_CHAR_WIDTH 16.0f
#define APP_RENDERER_C2D_FIXED_LINE_TEXT_SIZE 256
#define APP_RENDERER_C2D_FINGERPRINT_PATH "sdmc:/3ds/anki3ds/renderer.tsv"
#define APP_RENDERER_C2D_SCROLLBAR_MIN_THUMB_HEIGHT 12.0f
#define APP_RENDERER_C2D_TAG_CHIP_SCALE 0.46f
#define APP_RENDERER_C2D_TAG_CHIP_CHAR_WIDTH 9.0f
#define APP_RENDERER_C2D_TAG_CHIP_PADDING_X 8.0f
#define APP_RENDERER_C2D_TAG_CHIP_HEIGHT 20.0f
#define APP_RENDERER_C2D_TAG_CHIP_GAP 4.0f
#define APP_RENDERER_C2D_TAG_CHIP_LINE_HEIGHT 22.0f
#define APP_RENDERER_C2D_TAG_CHIP_MAX_CHARS 48

struct app_renderer_c2d_state
{
	C3D_RenderTarget *top_target;
	C3D_RenderTarget *bottom_target;
	C2D_TextBuf text_buffer;
	C2D_SpriteSheet top_sheet;
	C2D_SpriteSheet bottom_sheet;
	C2D_Image top_legend;
	C2D_Image bottom_legend;
	bool assets_loaded;
};

_Static_assert(
	sizeof(struct app_renderer_c2d_state) <= APP_RENDERER_C2D_STATE_BYTES,
	"app_renderer_c2d opaque storage is too small"
);
_Static_assert(
	__alignof__(struct app_renderer_c2d_state) <=
		__alignof__(union app_renderer_c2d_storage),
	"app_renderer_c2d opaque storage is under-aligned"
);

static struct app_renderer_c2d_state *app_renderer_c2d_mutable_state(
	struct app_renderer_c2d *renderer
)
{
	if (renderer == NULL)
		return NULL;

	return (struct app_renderer_c2d_state *)renderer->storage.bytes;
}

enum app_render_screen
{
	APP_RENDER_SCREEN_TOP,
	APP_RENDER_SCREEN_BOTTOM,
};

struct app_render_rect
{
	float x;
	float y;
	float width;
	float height;
};

struct app_render_text_box
{
	float x;
	float y;
	float scale;
	float wrap_width;
};

static const struct app_render_rect app_render_top_panel =
{
	25.0f,
	32.0f,
	350.0f,
	176.0f,
};
static const struct app_render_rect app_render_bottom_panel =
{
	14.0f,
	28.0f,
	292.0f,
	174.0f,
};
static const struct app_render_text_box app_review_top_body_box =
{
	49.0f,
	52.0f,
	APP_RENDERER_C2D_TEXT_SCALE,
	0.0f,
};
static const struct app_render_text_box app_review_top_meta_box =
{
	49.0f,
	178.0f,
	APP_RENDERER_C2D_TEXT_SCALE,
	302.0f,
};
static const struct app_render_text_box app_menu_top_title_box =
{
	49.0f,
	50.0f,
	APP_RENDERER_C2D_TEXT_SCALE,
	0.0f,
};
static const struct app_render_text_box app_menu_top_body_box =
{
	49.0f,
	78.0f,
	APP_RENDERER_C2D_TEXT_SCALE,
	302.0f,
};
static const struct app_render_text_box app_menu_top_meta_box =
{
	49.0f,
	178.0f,
	APP_RENDERER_C2D_TEXT_SCALE,
	302.0f,
};
static const struct app_render_text_box app_bottom_status_box =
{
	34.0f,
	46.0f,
	APP_RENDERER_C2D_TEXT_SCALE,
	252.0f,
};
static const struct app_render_text_box app_bottom_body_box =
{
	34.0f,
	82.0f,
	APP_RENDERER_C2D_TEXT_SCALE,
	252.0f,
};
static const struct app_render_text_box app_review_bottom_answer_box =
{
	34.0f,
	72.0f,
	APP_RENDERER_C2D_TEXT_SCALE,
	0.0f,
};
static const struct app_render_text_box app_bottom_footer_box =
{
	34.0f,
	174.0f,
	APP_RENDERER_C2D_TEXT_SCALE,
	252.0f,
};

static void app_renderer_c2d_unload_assets(
	struct app_renderer_c2d_state *renderer
);
static void app_renderer_c2d_write_fingerprint(
	const struct app_renderer_c2d_state *renderer
);
static void app_renderer_c2d_draw_fixed_box_text(
	C2D_TextBuf text_buffer,
	const char *text,
	const struct app_render_text_box *box,
	u32 color,
	float y_offset
);

static void app_renderer_c2d_draw_parsed_text(
	const C2D_Text *text,
	float x,
	float y,
	float scale,
	u32 color,
	float wrap_width
)
{
	if (text == NULL)
		return;

	if (wrap_width > 0.0f)
	{
		C2D_DrawText(
			text,
			C2D_WithColor | C2D_WordWrap,
			x,
			y,
			0.5f,
			scale,
			scale,
			color,
			wrap_width
		);
		return;
	}

	C2D_DrawText(
		text,
		C2D_WithColor,
		x,
		y,
		0.5f,
		scale,
		scale,
		color
	);
}

static void app_renderer_c2d_draw_text_shadowed(
	C2D_TextBuf text_buffer,
	const char *text,
	float x,
	float y,
	float scale,
	u32 color,
	u32 shadow_color,
	float wrap_width
)
{
	C2D_Text c2d_text;

	if (text == NULL)
		text = "";

	C2D_TextParse(&c2d_text, text_buffer, text);
	C2D_TextOptimize(&c2d_text);
	app_renderer_c2d_draw_parsed_text(
		&c2d_text,
		x + 1.0f,
		y + 1.0f,
		scale,
		shadow_color,
		wrap_width
	);
	app_renderer_c2d_draw_parsed_text(
		&c2d_text,
		x + 0.55f,
		y,
		scale,
		color,
		wrap_width
	);
	app_renderer_c2d_draw_parsed_text(
		&c2d_text,
		x,
		y,
		scale,
		color,
		wrap_width
	);
}

static void app_renderer_c2d_draw_text_plain(
	C2D_TextBuf text_buffer,
	const char *text,
	float x,
	float y,
	float scale,
	u32 color,
	float wrap_width
)
{
	C2D_Text c2d_text;

	if (text == NULL)
		text = "";

	C2D_TextParse(&c2d_text, text_buffer, text);
	C2D_TextOptimize(&c2d_text);
	app_renderer_c2d_draw_parsed_text(
		&c2d_text,
		x,
		y,
		scale,
		color,
		wrap_width
	);
}

static u32 app_render_ink_color(void)
{
	return C2D_Color32(0x0C, 0x0E, 0x10, 0xFF);
}

static u32 app_render_text_shadow_color(void)
{
	return C2D_Color32(0xFF, 0xF6, 0xCE, 0xD4);
}

static u32 app_render_muted_ink_color(void)
{
	return C2D_Color32(0x25, 0x20, 0x18, 0xFF);
}

static u32 app_render_faint_ink_color(void)
{
	return C2D_Color32(0x47, 0x3B, 0x24, 0xFF);
}

static bool app_renderer_c2d_text_is_no_answer(const char *text)
{
	return text != NULL && strcmp(text, APP_FLASHCARD_NO_ANSWER_TEXT) == 0;
}

static u32 app_render_scrollbar_track_color(void)
{
	return C2D_Color32(0x7B, 0x66, 0x37, 0x80);
}

static u32 app_render_scrollbar_thumb_color(void)
{
	return C2D_Color32(0x3D, 0x32, 0x20, 0xD8);
}

static u32 app_render_tag_chip_edge_color(void)
{
	return C2D_Color32(0x75, 0x5B, 0x2E, 0xD8);
}

static u32 app_render_tag_chip_fill_color(void)
{
	return C2D_Color32(0xE4, 0xCF, 0x8E, 0xD8);
}

static const struct app_render_rect *app_render_panel_for_screen(
	enum app_render_screen screen
)
{
	return screen == APP_RENDER_SCREEN_TOP ?
		&app_render_top_panel :
		&app_render_bottom_panel;
}

static void app_renderer_c2d_draw_fallback_background(
	enum app_render_screen screen
)
{
	float width = screen == APP_RENDER_SCREEN_TOP ? 400.0f : 320.0f;
	u32 sky = C2D_Color32(0xA7, 0xCE, 0xB8, 0xFF);
	u32 grass = C2D_Color32(0x78, 0xA0, 0x54, 0xFF);
	u32 grass_dark = C2D_Color32(0x52, 0x79, 0x45, 0xFF);

	C2D_DrawRectSolid(0.0f, 0.0f, 0.05f, width, 84.0f, sky);
	C2D_DrawRectSolid(0.0f, 84.0f, 0.05f, width, 156.0f, grass);
	C2D_DrawRectSolid(0.0f, 150.0f, 0.06f, width, 10.0f, grass_dark);
	C2D_DrawRectSolid(0.0f, 204.0f, 0.06f, width, 8.0f, grass_dark);
}

static void app_renderer_c2d_draw_fallback_scroll_panel(
	enum app_render_screen screen
)
{
	const struct app_render_rect *panel = app_render_panel_for_screen(screen);
	u32 shadow = C2D_Color32(0x3A, 0x2A, 0x15, 0xA0);
	u32 edge_dark = C2D_Color32(0x82, 0x63, 0x2D, 0xFF);
	u32 edge_mid = C2D_Color32(0xB2, 0x8E, 0x49, 0xFF);
	u32 edge_light = C2D_Color32(0xE8, 0xD2, 0x8F, 0xFF);
	u32 parchment = C2D_Color32(0xEC, 0xDA, 0x9D, 0xFF);
	u32 parchment_light = C2D_Color32(0xF6, 0xE8, 0xB8, 0xFF);

	C2D_DrawRectSolid(
		panel->x + 5.0f,
		panel->y + 5.0f,
		0.10f,
		panel->width,
		panel->height,
		shadow
	);
	C2D_DrawRectSolid(
		panel->x,
		panel->y,
		0.20f,
		panel->width,
		panel->height,
		edge_dark
	);
	C2D_DrawRectSolid(
		panel->x + 3.0f,
		panel->y + 3.0f,
		0.21f,
		panel->width - 6.0f,
		panel->height - 6.0f,
		edge_mid
	);
	C2D_DrawRectSolid(
		panel->x + 6.0f,
		panel->y + 6.0f,
		0.22f,
		panel->width - 12.0f,
		panel->height - 12.0f,
		parchment
	);
	C2D_DrawRectSolid(
		panel->x + 16.0f,
		panel->y + 16.0f,
		0.23f,
		panel->width - 32.0f,
		panel->height - 32.0f,
		parchment_light
	);
	C2D_DrawRectSolid(
		panel->x + 8.0f,
		panel->y + 8.0f,
		0.24f,
		panel->width - 16.0f,
		2.0f,
		edge_light
	);
	C2D_DrawRectSolid(
		panel->x + 8.0f,
		panel->y + panel->height - 10.0f,
		0.24f,
		panel->width - 16.0f,
		2.0f,
		edge_dark
	);
	C2D_DrawRectSolid(
		panel->x + 8.0f,
		panel->y + 8.0f,
		0.24f,
		2.0f,
		panel->height - 16.0f,
		edge_light
	);
	C2D_DrawRectSolid(
		panel->x + panel->width - 10.0f,
		panel->y + 8.0f,
		0.24f,
		2.0f,
		panel->height - 16.0f,
		edge_dark
	);
}

static void app_renderer_c2d_draw_parchment_readability_frame(
	enum app_render_screen screen
)
{
	const struct app_render_rect *panel = app_render_panel_for_screen(screen);
	float x;
	float y;
	float width;
	float height;
	u32 body_fill = C2D_Color32(0xF7, 0xEB, 0xBE, 0x54);

	if (panel == NULL)
		return;

	x = panel->x + 8.0f;
	y = panel->y + 10.0f;
	width = panel->width - 16.0f;
	height = panel->height - 20.0f;
	C2D_DrawRectSolid(
		x + 2.0f,
		y + 2.0f,
		0.34f,
		width - 4.0f,
		height - 4.0f,
		body_fill
	);
}

static void app_renderer_c2d_draw_theme_surface(
	const struct app_renderer_c2d_state *renderer,
	enum app_render_screen screen
)
{
	if (renderer != NULL && renderer->assets_loaded)
	{
		C2D_DrawImageAt(
			screen == APP_RENDER_SCREEN_TOP ?
				renderer->top_legend :
				renderer->bottom_legend,
			0.0f,
			0.0f,
			0.1f,
			NULL,
			1.0f,
			1.0f
		);
		app_renderer_c2d_draw_parchment_readability_frame(screen);
		return;
	}

	app_renderer_c2d_draw_fallback_background(screen);
	app_renderer_c2d_draw_fallback_scroll_panel(screen);
	app_renderer_c2d_draw_parchment_readability_frame(screen);
}

static void app_renderer_c2d_draw_box_text(
	C2D_TextBuf text_buffer,
	const char *text,
	const struct app_render_text_box *box,
	u32 color,
	float y_offset
)
{
	if (box == NULL)
		return;

	app_renderer_c2d_draw_text_shadowed(
		text_buffer,
		text,
		box->x,
		box->y + y_offset,
		box->scale,
		color,
		app_render_text_shadow_color(),
		box->wrap_width
	);
}

static void app_renderer_c2d_draw_bottom_footer_text(
	C2D_TextBuf text_buffer,
	const char *text,
	u32 color
)
{
	app_renderer_c2d_draw_fixed_box_text(
		text_buffer,
		text,
		&app_bottom_footer_box,
		color,
		0.0f
	);
}

static void app_renderer_c2d_draw_box_text_shadowed(
	C2D_TextBuf text_buffer,
	const char *text,
	const struct app_render_text_box *box,
	u32 color,
	float y_offset
)
{
	if (box == NULL)
		return;

	app_renderer_c2d_draw_text_shadowed(
		text_buffer,
		text,
		box->x,
		box->y + y_offset,
		box->scale,
		color,
		app_render_text_shadow_color(),
		box->wrap_width
	);
}

static size_t app_renderer_c2d_box_columns(
	const struct app_render_text_box *box
)
{
	float scaled_char_width;
	size_t columns;

	if (box == NULL || box->wrap_width <= 0.0f || box->scale <= 0.0f)
		return 0;

	scaled_char_width = APP_RENDERER_C2D_TEXT_NATIVE_CHAR_WIDTH * box->scale;
	if (scaled_char_width <= 0.0f)
		return 0;

	columns = (size_t)(box->wrap_width / scaled_char_width);
	return columns > 0 ? columns : 1;
}

static void app_renderer_c2d_draw_fixed_box_text(
	C2D_TextBuf text_buffer,
	const char *text,
	const struct app_render_text_box *box,
	u32 color,
	float y_offset
)
{
	char line[APP_RENDERER_C2D_FIXED_LINE_TEXT_SIZE];
	struct app_render_text_box fixed_box;
	size_t columns;
	const char *draw_text = text;

	if (box == NULL)
		return;

	fixed_box = *box;
	fixed_box.wrap_width = 0.0f;
	columns = app_renderer_c2d_box_columns(box);
	if (columns > 0)
	{
		(void)app_text_copy_truncated_line(
			text,
			columns,
			line,
			sizeof(line)
		);
		draw_text = line;
	}

	app_renderer_c2d_draw_box_text(
		text_buffer,
		draw_text,
		&fixed_box,
		color,
		y_offset
	);
}

static size_t app_renderer_c2d_review_text_max_scroll_offset(
	const char *text,
	size_t columns,
	size_t rows
)
{
	return app_text_max_scroll_offset(
		text != NULL ? text : "",
		columns,
		rows
	);
}

size_t app_renderer_c2d_review_body_max_scroll_offset(
	const char *text,
	bool answer_visible
)
{
	return app_renderer_c2d_review_text_max_scroll_offset(
		text,
		answer_visible ?
			APP_RENDERER_C2D_REVIEW_ANSWER_WINDOW_COLUMNS :
			APP_RENDERER_C2D_REVIEW_FRONT_WINDOW_COLUMNS,
		answer_visible ?
			APP_RENDERER_C2D_REVIEW_ANSWER_WINDOW_ROWS :
			APP_RENDERER_C2D_REVIEW_FRONT_WINDOW_ROWS
	);
}

static size_t app_renderer_c2d_copy_visible_review_text(
	const char *text,
	size_t columns,
	size_t rows,
	size_t scroll_offset,
	char *destination,
	size_t destination_size
)
{
	size_t max_scroll_offset;
	size_t window_scroll_offset;

	if (destination == NULL || destination_size == 0)
		return 0;

	destination[0] = '\0';
	max_scroll_offset = app_renderer_c2d_review_text_max_scroll_offset(
		text,
		columns,
		rows
	);
	window_scroll_offset = scroll_offset;
	if (window_scroll_offset > max_scroll_offset)
		window_scroll_offset = max_scroll_offset;

	(void)app_text_copy_wrapped_window(
		text != NULL ? text : "",
		columns,
		window_scroll_offset,
		rows,
		destination,
		destination_size
	);
	return window_scroll_offset;
}

static const char *app_flashcard_text_view_front_surface_text(
	const struct app_flashcard_text_view *text_view
)
{
	if (text_view == NULL)
		return "";
	if (!text_view->has_active_card)
		return text_view->display_text;
	return text_view->front_text;
}

static void app_flashcard_text_view_format_meta(
	const struct app_flashcard_text_view *text_view,
	size_t scroll_offset,
	char *destination,
	size_t destination_size
)
{
	const char *progress_text;

	(void)scroll_offset;

	if (destination == NULL || destination_size == 0)
		return;

	progress_text = text_view != NULL ? text_view->progress_text : "";
	snprintf(destination, destination_size, "%s", progress_text);
}

static void app_renderer_c2d_draw_review_scrollbar(
	const char *text,
	size_t columns,
	size_t rows,
	size_t scroll_offset,
	float x,
	float y,
	float height
)
{
	size_t max_scroll_offset;
	float thumb_height;
	float thumb_y;

	max_scroll_offset = app_renderer_c2d_review_text_max_scroll_offset(
		text,
		columns,
		rows
	);
	if (max_scroll_offset == 0)
		return;
	if (scroll_offset > max_scroll_offset)
		scroll_offset = max_scroll_offset;

	thumb_height = height * (float)rows / (float)(rows + max_scroll_offset);
	if (thumb_height < APP_RENDERER_C2D_SCROLLBAR_MIN_THUMB_HEIGHT)
		thumb_height = APP_RENDERER_C2D_SCROLLBAR_MIN_THUMB_HEIGHT;
	if (thumb_height > height)
		thumb_height = height;
	thumb_y = y + (height - thumb_height) *
		(float)scroll_offset /
		(float)max_scroll_offset;

	C2D_DrawRectSolid(x, y, 0.64f, 3.0f, height, app_render_scrollbar_track_color());
	C2D_DrawRectSolid(
		x - 1.0f,
		thumb_y,
		0.65f,
		5.0f,
		thumb_height,
		app_render_scrollbar_thumb_color()
	);
}

static bool app_renderer_c2d_tag_separator(char value)
{
	return value == ' ' || value == '\t' || value == '\n' || value == '\r';
}

static float app_renderer_c2d_tag_chip_width(const char *tag)
{
	size_t length = tag != NULL ? strlen(tag) : 0;

	return (float)length * APP_RENDERER_C2D_TAG_CHIP_CHAR_WIDTH +
		APP_RENDERER_C2D_TAG_CHIP_PADDING_X * 2.0f;
}

static void app_renderer_c2d_draw_tag_chip(
	C2D_TextBuf text_buffer,
	const char *tag,
	float x,
	float y,
	float width
)
{
	C2D_DrawRectSolid(
		x,
		y - 2.0f,
		0.44f,
		width,
		APP_RENDERER_C2D_TAG_CHIP_HEIGHT,
		app_render_tag_chip_edge_color()
	);
	C2D_DrawRectSolid(
		x + 1.0f,
		y - 1.0f,
		0.45f,
		width - 2.0f,
		APP_RENDERER_C2D_TAG_CHIP_HEIGHT - 2.0f,
		app_render_tag_chip_fill_color()
	);
	app_renderer_c2d_draw_text_plain(
		text_buffer,
		tag,
		x + APP_RENDERER_C2D_TAG_CHIP_PADDING_X,
		y,
		APP_RENDERER_C2D_TAG_CHIP_SCALE,
		app_render_ink_color(),
		0.0f
	);
}

static void app_renderer_c2d_draw_tag_chips(
	C2D_TextBuf text_buffer,
	const char *tags_text
)
{
	char tag[APP_RENDERER_C2D_TAG_CHIP_MAX_CHARS + 1];
	float x = 34.0f;
	float y = 158.0f;
	const float start_x = 34.0f;
	const float max_x = 286.0f;
	size_t tag_length = 0;
	const char *cursor;

	if (tags_text == NULL || tags_text[0] == '\0')
		return;

	for (cursor = tags_text; ; cursor++)
	{
		char value = *cursor;
		bool separator = value == '\0' || app_renderer_c2d_tag_separator(value);

		if (!separator && tag_length < APP_RENDERER_C2D_TAG_CHIP_MAX_CHARS)
		{
			tag[tag_length++] = value;
			continue;
		}
		if (tag_length > 0)
		{
			float width;

			tag[tag_length] = '\0';
			width = app_renderer_c2d_tag_chip_width(tag);
			if (x > start_x && x + width > max_x)
			{
				x = start_x;
				y += APP_RENDERER_C2D_TAG_CHIP_LINE_HEIGHT;
			}
			if (y + APP_RENDERER_C2D_TAG_CHIP_HEIGHT <= 202.0f)
			{
				app_renderer_c2d_draw_tag_chip(text_buffer, tag, x, y, width);
				x += width + APP_RENDERER_C2D_TAG_CHIP_GAP;
			}
			tag_length = 0;
		}
		if (value == '\0')
			break;
	}
}

static void app_renderer_c2d_draw_review_top_screen(
	const struct app_renderer_c2d_state *renderer,
	const struct app_flashcard_text_view *text_view,
	size_t front_scroll_offset
)
{
	char visible_body[APP_FLASHCARD_BODY_TEXT_SIZE];
	char meta_text[APP_FLASHCARD_META_TEXT_SIZE];
	size_t window_scroll_offset;
	const char *front_text;

	app_renderer_c2d_draw_theme_surface(renderer, APP_RENDER_SCREEN_TOP);

	if (renderer == NULL || text_view == NULL)
		return;

	front_text = app_flashcard_text_view_front_surface_text(text_view);
	window_scroll_offset = app_renderer_c2d_copy_visible_review_text(
		front_text,
		APP_RENDERER_C2D_REVIEW_FRONT_WINDOW_COLUMNS,
		APP_RENDERER_C2D_REVIEW_FRONT_WINDOW_ROWS,
		front_scroll_offset,
		visible_body,
		sizeof(visible_body)
	);
	app_flashcard_text_view_format_meta(
		text_view,
		window_scroll_offset,
		meta_text,
		sizeof(meta_text)
	);
	app_renderer_c2d_draw_box_text_shadowed(
		renderer->text_buffer,
		visible_body,
		&app_review_top_body_box,
		app_render_ink_color(),
		0.0f
	);
	app_renderer_c2d_draw_box_text_shadowed(
		renderer->text_buffer,
		meta_text,
		&app_review_top_meta_box,
		app_render_faint_ink_color(),
		0.0f
	);
	app_renderer_c2d_draw_review_scrollbar(
		front_text,
		APP_RENDERER_C2D_REVIEW_FRONT_WINDOW_COLUMNS,
		APP_RENDERER_C2D_REVIEW_FRONT_WINDOW_ROWS,
		window_scroll_offset,
		359.0f,
		55.0f,
		89.0f
	);
}

static void app_renderer_c2d_draw_deck_select_top_screen(
	const struct app_renderer_c2d_state *renderer,
	const struct app_deck_select_contract *contract
)
{
	app_renderer_c2d_draw_theme_surface(renderer, APP_RENDER_SCREEN_TOP);

	if (renderer == NULL || contract == NULL)
		return;

	app_renderer_c2d_draw_box_text(
		renderer->text_buffer,
		contract->title_text,
		&app_menu_top_title_box,
		app_render_muted_ink_color(),
		0.0f
	);
	app_renderer_c2d_draw_box_text(
		renderer->text_buffer,
		contract->list_text,
		&app_menu_top_body_box,
		app_render_ink_color(),
		0.0f
	);
	app_renderer_c2d_draw_box_text(
		renderer->text_buffer,
		contract->meta_text,
		&app_menu_top_meta_box,
		app_render_faint_ink_color(),
		0.0f
	);
}

static void app_renderer_c2d_draw_review_bottom_screen(
	const struct app_renderer_c2d_state *renderer,
	const struct app_flashcard_text_view *text_view,
	size_t answer_scroll_offset
)
{
	char visible_answer[APP_FLASHCARD_BODY_TEXT_SIZE];

	app_renderer_c2d_draw_theme_surface(renderer, APP_RENDER_SCREEN_BOTTOM);

	if (renderer == NULL || text_view == NULL)
		return;

	app_renderer_c2d_draw_box_text(
		renderer->text_buffer,
		text_view->status_text,
		&app_bottom_status_box,
		app_render_ink_color(),
		0.0f
	);
	if (!text_view->answer_visible && text_view->help_text[0] != '\0')
	{
		app_renderer_c2d_draw_box_text(
			renderer->text_buffer,
			text_view->help_text,
			&app_bottom_body_box,
			app_render_ink_color(),
			0.0f
		);
	}
	else if (text_view->answer_visible)
	{
		u32 answer_color = app_renderer_c2d_text_is_no_answer(text_view->back_text) ?
			app_render_faint_ink_color() :
			app_render_ink_color();

		(void)app_renderer_c2d_copy_visible_review_text(
			text_view->back_text,
			APP_RENDERER_C2D_REVIEW_ANSWER_WINDOW_COLUMNS,
			APP_RENDERER_C2D_REVIEW_ANSWER_WINDOW_ROWS,
			answer_scroll_offset,
			visible_answer,
			sizeof(visible_answer)
		);
		app_renderer_c2d_draw_box_text(
			renderer->text_buffer,
			visible_answer,
			&app_review_bottom_answer_box,
			answer_color,
			0.0f
		);
		app_renderer_c2d_draw_review_scrollbar(
			text_view->back_text,
			APP_RENDERER_C2D_REVIEW_ANSWER_WINDOW_COLUMNS,
			APP_RENDERER_C2D_REVIEW_ANSWER_WINDOW_ROWS,
			answer_scroll_offset,
			286.0f,
			75.0f,
			78.0f
		);
	}
	if (text_view->answer_visible && text_view->tags_text[0] != '\0')
		app_renderer_c2d_draw_tag_chips(renderer->text_buffer, text_view->tags_text);
	else
		app_renderer_c2d_draw_bottom_footer_text(
			renderer->text_buffer,
			text_view->footer_text,
			app_render_faint_ink_color()
		);
}

static void app_renderer_c2d_draw_confirm_bottom_screen(
	const struct app_renderer_c2d_state *renderer,
	const struct app_confirm_contract *contract
)
{
	app_renderer_c2d_draw_theme_surface(renderer, APP_RENDER_SCREEN_BOTTOM);

	if (renderer == NULL || contract == NULL)
		return;

	app_renderer_c2d_draw_box_text(
		renderer->text_buffer,
		contract->status_text,
		&app_bottom_status_box,
		app_render_ink_color(),
		0.0f
	);
	app_renderer_c2d_draw_box_text(
		renderer->text_buffer,
		contract->prompt_text,
		&app_bottom_body_box,
		app_render_ink_color(),
		0.0f
	);
	app_renderer_c2d_draw_bottom_footer_text(
		renderer->text_buffer,
		contract->footer_text,
		app_render_faint_ink_color()
	);
}

static void app_renderer_c2d_draw_settings_top_screen(
	const struct app_renderer_c2d_state *renderer,
	const struct app_settings_contract *contract
)
{
	app_renderer_c2d_draw_theme_surface(renderer, APP_RENDER_SCREEN_TOP);

	if (renderer == NULL || contract == NULL)
		return;

	app_renderer_c2d_draw_box_text(
		renderer->text_buffer,
		contract->title_text,
		&app_menu_top_title_box,
		app_render_muted_ink_color(),
		0.0f
	);
	app_renderer_c2d_draw_box_text(
		renderer->text_buffer,
		contract->body_text,
		&app_menu_top_body_box,
		app_render_ink_color(),
		10.0f
	);
	app_renderer_c2d_draw_box_text(
		renderer->text_buffer,
		contract->footer_text,
		&app_menu_top_meta_box,
		app_render_faint_ink_color(),
		0.0f
	);
}

static void app_renderer_c2d_draw_settings_bottom_screen(
	const struct app_renderer_c2d_state *renderer,
	const struct app_settings_contract *contract
)
{
	app_renderer_c2d_draw_theme_surface(renderer, APP_RENDER_SCREEN_BOTTOM);

	if (renderer == NULL || contract == NULL)
		return;

	app_renderer_c2d_draw_box_text(
		renderer->text_buffer,
		contract->status_text,
		&app_bottom_status_box,
		app_render_ink_color(),
		0.0f
	);
	app_renderer_c2d_draw_box_text(
		renderer->text_buffer,
		contract->controls_text,
		&app_bottom_body_box,
		app_render_ink_color(),
		0.0f
	);
	app_renderer_c2d_draw_bottom_footer_text(
		renderer->text_buffer,
		contract->help_text,
		app_render_faint_ink_color()
	);
}

static void app_renderer_c2d_draw_deck_select_bottom_screen(
	const struct app_renderer_c2d_state *renderer,
	const struct app_deck_select_contract *contract
)
{
	app_renderer_c2d_draw_theme_surface(renderer, APP_RENDER_SCREEN_BOTTOM);

	if (renderer == NULL || contract == NULL)
		return;

	app_renderer_c2d_draw_box_text(
		renderer->text_buffer,
		contract->status_text,
		&app_bottom_status_box,
		app_render_ink_color(),
		0.0f
	);
	app_renderer_c2d_draw_box_text(
		renderer->text_buffer,
		contract->controls_text,
		&app_bottom_body_box,
		app_render_ink_color(),
		0.0f
	);
	app_renderer_c2d_draw_bottom_footer_text(
		renderer->text_buffer,
		contract->footer_text,
		app_render_faint_ink_color()
	);
}

static bool app_renderer_c2d_load_assets(struct app_renderer_c2d_state *renderer)
{
	if (renderer == NULL)
		return false;

	renderer->top_sheet = C2D_SpriteSheetLoadFromMem(
		fe_forest_top_legend_t3x,
		fe_forest_top_legend_t3x_size
	);
	renderer->bottom_sheet = C2D_SpriteSheetLoadFromMem(
		fe_forest_bottom_legend_t3x,
		fe_forest_bottom_legend_t3x_size
	);
	if (renderer->top_sheet == NULL || renderer->bottom_sheet == NULL)
	{
		app_renderer_c2d_unload_assets(renderer);
		return false;
	}

	renderer->top_legend = C2D_SpriteSheetGetImage(renderer->top_sheet, 0);
	renderer->bottom_legend = C2D_SpriteSheetGetImage(renderer->bottom_sheet, 0);
	renderer->assets_loaded = true;
	return true;
}

static void app_renderer_c2d_unload_assets(struct app_renderer_c2d_state *renderer)
{
	if (renderer == NULL)
		return;
	if (renderer->top_sheet != NULL)
		C2D_SpriteSheetFree(renderer->top_sheet);
	if (renderer->bottom_sheet != NULL)
		C2D_SpriteSheetFree(renderer->bottom_sheet);

	renderer->top_sheet = NULL;
	renderer->bottom_sheet = NULL;
	renderer->assets_loaded = false;
}

static void app_renderer_c2d_write_text_box_fingerprint(
	FILE *file,
	const char *name,
	const struct app_render_text_box *box
)
{
	if (file == NULL || name == NULL || box == NULL)
		return;

	fprintf(
		file,
		"%s\t%.0f\t%.0f\t%.2f\t%.0f\n",
		name,
		(double)box->x,
		(double)box->y,
		(double)box->scale,
		(double)box->wrap_width
	);
}

static void app_renderer_c2d_write_fingerprint(
	const struct app_renderer_c2d_state *renderer
)
{
	FILE *file = fopen(APP_RENDERER_C2D_FINGERPRINT_PATH, "w");

	if (file == NULL)
		return;

	fprintf(file, "#anki3ds-renderer-v1\n");
	fprintf(file, "renderer\tcitro2d\n");
	fprintf(
		file,
		"assets_loaded\t%d\n",
		renderer != NULL && renderer->assets_loaded ? 1 : 0
	);
	fprintf(
		file,
		"top_legend_t3x_size\t%lu\n",
		(unsigned long)fe_forest_top_legend_t3x_size
	);
	fprintf(
		file,
		"bottom_legend_t3x_size\t%lu\n",
		(unsigned long)fe_forest_bottom_legend_t3x_size
	);
	fprintf(
		file,
		"review_front_window\t%u\t%u\n",
		(unsigned int)APP_RENDERER_C2D_REVIEW_FRONT_WINDOW_COLUMNS,
		(unsigned int)APP_RENDERER_C2D_REVIEW_FRONT_WINDOW_ROWS
	);
	fprintf(
		file,
		"review_answer_window\t%u\t%u\n",
		(unsigned int)APP_RENDERER_C2D_REVIEW_ANSWER_WINDOW_COLUMNS,
		(unsigned int)APP_RENDERER_C2D_REVIEW_ANSWER_WINDOW_ROWS
	);
	fprintf(file, "#name\tx\ty\tscale\twrap_width\n");
	app_renderer_c2d_write_text_box_fingerprint(
		file,
		"menu_top_meta",
		&app_menu_top_meta_box
	);
	app_renderer_c2d_write_text_box_fingerprint(
		file,
		"bottom_footer",
		&app_bottom_footer_box
	);
	app_renderer_c2d_write_text_box_fingerprint(
		file,
		"review_top_meta",
		&app_review_top_meta_box
	);
	app_renderer_c2d_write_text_box_fingerprint(
		file,
		"review_bottom_answer",
		&app_review_bottom_answer_box
	);
	fprintf(file, "#anki3ds-renderer-complete\n");
	fclose(file);
}

bool app_renderer_c2d_init(struct app_renderer_c2d *renderer)
{
	struct app_renderer_c2d_state *state;

	if (renderer == NULL)
		return false;

	memset(renderer, 0, sizeof(*renderer));
	state = app_renderer_c2d_mutable_state(renderer);
	if (state == NULL)
		return false;

	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();

	state->top_target = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	state->bottom_target = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
	state->text_buffer =
		C2D_TextBufNew(APP_RENDERER_C2D_TEXT_BUFFER_GLYPHS);
	if (
		state->top_target == NULL ||
		state->bottom_target == NULL ||
		state->text_buffer == NULL
	)
	{
		app_renderer_c2d_fini(renderer);
		return false;
	}

	(void)app_renderer_c2d_load_assets(state);
	app_renderer_c2d_write_fingerprint(state);
	return true;
}

void app_renderer_c2d_draw(
	struct app_renderer_c2d *renderer,
	const struct app_screen_model *model
)
{
	struct app_renderer_c2d_state *state;
	u32 top_background = C2D_Color32(0x2B, 0x35, 0x33, 0xFF);
	u32 bottom_background = C2D_Color32(0x1D, 0x24, 0x28, 0xFF);

	state = app_renderer_c2d_mutable_state(renderer);
	if (state == NULL || model == NULL || state->text_buffer == NULL)
		return;

	C2D_TextBufClear(state->text_buffer);
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	C2D_TargetClear(state->top_target, top_background);
	C2D_SceneBegin(state->top_target);
	if (model->kind == APP_SCREEN_MODEL_DECK_SELECT)
	{
		app_renderer_c2d_draw_deck_select_top_screen(
			state,
			&model->deck_select
		);
	}
	else if (model->kind == APP_SCREEN_MODEL_SETTINGS)
	{
		app_renderer_c2d_draw_settings_top_screen(
			state,
			&model->settings
		);
	}
	else
	{
		app_renderer_c2d_draw_review_top_screen(
			state,
			&model->flashcard,
			model->front_scroll_offset
		);
	}

	C2D_TargetClear(state->bottom_target, bottom_background);
	C2D_SceneBegin(state->bottom_target);
	if (model->kind == APP_SCREEN_MODEL_DECK_SELECT)
		app_renderer_c2d_draw_deck_select_bottom_screen(
			state,
			&model->deck_select
		);
	else if (model->kind == APP_SCREEN_MODEL_SETTINGS)
		app_renderer_c2d_draw_settings_bottom_screen(
			state,
			&model->settings
		);
	else if (model->kind == APP_SCREEN_MODEL_CONFIRM)
	{
		app_renderer_c2d_draw_confirm_bottom_screen(
			state,
			&model->confirm
		);
	}
	else
		app_renderer_c2d_draw_review_bottom_screen(
			state,
			&model->flashcard,
			model->answer_scroll_offset
		);
	C3D_FrameEnd(0);
}

void app_renderer_c2d_fini(struct app_renderer_c2d *renderer)
{
	struct app_renderer_c2d_state *state;

	if (renderer == NULL)
		return;

	state = app_renderer_c2d_mutable_state(renderer);
	app_renderer_c2d_unload_assets(state);
	if (state != NULL && state->text_buffer != NULL)
		C2D_TextBufDelete(state->text_buffer);
	C2D_Fini();
	C3D_Fini();
	memset(renderer, 0, sizeof(*renderer));
}
