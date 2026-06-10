import importlib.util
import io
import tempfile
import unittest
from contextlib import redirect_stderr
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
VERIFY_APP_THEME_PATH = ROOT / "tools" / "verify_app_theme.py"
VERIFY_APP_THEME_SPEC = importlib.util.spec_from_file_location(
    "verify_app_theme",
    VERIFY_APP_THEME_PATH,
)
verify_app_theme = importlib.util.module_from_spec(VERIFY_APP_THEME_SPEC)
assert VERIFY_APP_THEME_SPEC.loader is not None
VERIFY_APP_THEME_SPEC.loader.exec_module(verify_app_theme)

VALID_CONTRACT_HEADER = """
#include <stdbool.h>
#include <stddef.h>

struct app_review_front_contract
{
\tbool has_card;
\tconst char *front_text;
\tsize_t scroll_offset;
\tsize_t legend_line_count;
\tconst char *legend_lines[8];
\tconst char *status_line;
\tconst char *battery_line;
};
"""

VALID_FE_RENDERER_HEADER = """
#include "app_review_front_contract.h"

void app_fe_renderer_draw_review_front_overlay(
\tconst struct app_review_front_contract *contract
);
"""

VALID_FE_RENDERER_SOURCE = """
#include "app_fe_renderer.h"

#define APP_FE_REVIEW_FRONT_LEGEND_COLUMN_LEFT 3

static void app_fe_draw_wrapped_front_text(
\tconst struct app_review_front_contract *contract
)
{
\t(void)contract->front_text;
\t(void)contract->scroll_offset;
\t(void)app_text_max_scroll_offset(contract->front_text, 30, 10);
}

static int app_fe_review_front_legend_column(size_t index)
{
\t(void)index;
\treturn APP_FE_REVIEW_FRONT_LEGEND_COLUMN_LEFT;
}

void app_fe_renderer_draw_review_front_overlay(
\tconst struct app_review_front_contract *contract
)
{
\tapp_fe_draw_wrapped_front_text(contract);
\t(void)app_fe_review_front_legend_column(0);
\t(void)contract->legend_lines[0];
\t(void)contract->status_line;
\t(void)contract->battery_line;
}
"""

VALID_VIEW_MODEL_HEADER = """
#include "app_review_front_contract.h"

void app_review_front_contract_build(
\tstruct app_review_front_contract *contract,
\tconst struct deck *deck
);
"""

VALID_VIEW_MODEL_SOURCE = """
#include "app_view_model.h"

void app_review_front_contract_build(
\tstruct app_review_front_contract *contract,
\tconst struct deck *deck
)
{
\t(void)contract;
\t(void)deck;
}
"""


VALID_THEME_SOURCE = """
enum app_theme
{
\tAPP_THEME_PLAIN,
\tAPP_THEME_AMBER,
\tAPP_THEME_FOREST,
\tAPP_THEME_RUBY,
\tAPP_THEME_CHALK,
\tAPP_THEME_COUNT,
};

static const struct app_visual_theme app_visual_themes[] = {
\t{ "Plain", APP_COLOR_ACCENT, APP_COLOR_CARD_PAPER },
};

static const struct app_theme_background_paths app_theme_backgrounds[] = {
\t{
\t\tNULL,
\t\tNULL,
\t},
\t{
\t\t"sdmc:/3ds/anki3ds/fe-themes/layers/fe_bg_amber_top_layer_background_400x240_bgr888_fb.bin",
\t\t"sdmc:/3ds/anki3ds/fe-themes/layers/fe_bg_amber_bottom_layer_background_320x240_bgr888_fb.bin",
\t},
};

static const struct app_theme_background_paths app_theme_review_front_backgrounds[] = {
\t{
\t\tNULL,
\t\tNULL,
\t},
\t{
\t\t"sdmc:/3ds/anki3ds/fe-themes/layers/fe_bg_amber_top_layer_legend_400x240_bgr888_fb.bin",
\t\t"sdmc:/3ds/anki3ds/fe-themes/layers/fe_bg_amber_bottom_layer_legend_320x240_bgr888_fb.bin",
\t},
};

static const struct app_visual_theme *app_theme_visual(const struct app_state *app)
{
\tenum app_theme theme = APP_THEME_PLAIN;
\treturn &app_visual_themes[theme];
}

static enum app_theme app_sanitized_theme(enum app_theme theme)
{
\tif (theme < 0 || theme >= APP_THEME_COUNT)
\t\treturn APP_THEME_PLAIN;
\treturn theme;
}

static bool app_read_theme_background_pair(
	const struct app_theme_background_paths *paths
)
{
	return paths != NULL;
}

static const struct app_theme_background_paths *app_theme_background_paths_for_kind(
	enum app_theme theme,
	enum app_theme_background_kind kind
)
{
	if (kind == APP_THEME_BACKGROUND_REVIEW_FRONT)
		return &app_theme_review_front_backgrounds[theme];

	return &app_theme_backgrounds[theme];
}

static void app_load_theme_background(
	enum app_theme theme,
	enum app_theme_background_kind kind
)
{
\tconst struct app_theme_background_paths *paths;

\tpaths = app_theme_background_paths_for_kind(theme, kind);
\tif (
\t\tpaths->top_path == NULL ||
\t\tpaths->bottom_path == NULL
\t)
\t{
\t\treturn;
\t}
}

#define APP_FE_FONT_PATH "sdmc:/3ds/anki3ds/fe-themes/fe_font_review_8x14_alpha.bin"

static bool app_load_fe_font_atlas(void)
{
\treturn false;
}

static void app_fe_renderer_draw_review_front_overlay(
\tconst struct app_review_front_contract *contract
)
{
\t(void)contract;
}

static void app_build_fe_review_front_overlay(
\tstruct app_review_front_contract *contract
)
{
\tapp_review_front_contract_build(contract, NULL);
}

static void app_draw_fe_review_front_overlay(struct app_state *app)
{
\tstruct app_review_front_contract contract;
\t(void)app;
\tapp_build_fe_review_front_overlay(&contract);
\tapp_fe_renderer_draw_review_front_overlay(&contract);
}

static void draw_app(struct app_state *app)
{
\tapp_draw_fe_review_front_overlay(app);
}

static enum app_theme app_initial_theme(void)
{
\tapp_load_theme_background(APP_THEME_PLAIN, APP_THEME_BACKGROUND_FINAL);
\treturn APP_THEME_PLAIN;
}

static void app_init(struct app_state *app)
{
\tapp->theme = app_initial_theme();
}
"""

VALID_STUDY_BACKEND_HEADER = """
#include <stdbool.h>
#include <stddef.h>

enum study_backend_rating
{
\tSTUDY_BACKEND_RATING_GOOD,
};

struct study_backend_card
{
\tconst char *front;
\tconst char *back;
};

struct study_backend
{
\tsize_t current_index;
};

struct study_backend_view
{
\tbool has_active_card;
\tbool answer_visible;
\tbool undo_available;
\tconst char *primary_text;
\tconst char *status_text;
};

void study_backend_init(
\tstruct study_backend *backend,
\tconst struct study_backend_card *cards,
\tsize_t card_count
);
void study_backend_build_view(
\tconst struct study_backend *backend,
\tstruct study_backend_view *view
);
bool study_backend_show_answer(struct study_backend *backend);
bool study_backend_rate_current(
\tstruct study_backend *backend,
\tenum study_backend_rating rating
);
"""

VALID_STUDY_BACKEND_SOURCE = """
#include "study_backend.h"
"""

VALID_APP_REVIEW_ACTION_HEADER = """
#include <stdbool.h>

struct study_backend;

bool app_review_action_apply(struct study_backend *backend);
"""

VALID_APP_REVIEW_ACTION_SOURCE = """
#include "app_review_action.h"
#include "study_backend.h"

bool app_review_action_apply(struct study_backend *backend)
{
\tstudy_backend_show_answer(backend);
\treturn study_backend_rate_current(backend, STUDY_BACKEND_RATING_GOOD);
}
"""

VALID_APP_REVIEW_FLOW_HEADER = """
#include <stdbool.h>

struct study_backend;

bool app_review_flow_handle_input(struct study_backend *backend);
"""

VALID_APP_REVIEW_FLOW_SOURCE = """
#include "app_review_flow.h"
#include "app_review_action.h"

bool app_review_flow_handle_input(struct study_backend *backend)
{
\treturn app_review_action_apply(backend);
}
"""

VALID_APP_SCREEN_MODEL_HEADER = """
#ifndef APP_SCREEN_MODEL_H
#define APP_SCREEN_MODEL_H

struct study_backend;

struct app_flashcard_text_view
{
\tconst char *display_text;
\tconst char *status_text;
};

struct app_screen_model
{
\tstruct app_flashcard_text_view flashcard;
};

void app_screen_model_build(
\tstruct app_screen_model *model,
\tconst struct study_backend *backend
);

#endif
"""

VALID_APP_SCREEN_MODEL_SOURCE = """
#include "app_screen_model.h"
#include "study_backend.h"

void app_flashcard_text_view_build(
\tstruct app_flashcard_text_view *text_view,
\tconst struct study_backend_view *view
);

void app_screen_model_build(
\tstruct app_screen_model *model,
\tconst struct study_backend *backend
)
{
\tstruct study_backend_view view;

\tstudy_backend_build_view(backend, &view);
\tapp_flashcard_text_view_build(&model->flashcard, &view);
}
"""

VALID_APP_RENDERER_C2D_HEADER = """
#ifndef APP_RENDERER_C2D_H
#define APP_RENDERER_C2D_H

#include <stdbool.h>
#include <stddef.h>

struct app_screen_model;

#define APP_RENDERER_C2D_STATE_BYTES 256

union app_renderer_c2d_storage
{
\tvoid *pointer_alignment;
\tdouble double_alignment;
\tlong long integer_alignment;
\tunsigned char bytes[APP_RENDERER_C2D_STATE_BYTES];
};

struct app_renderer_c2d
{
\tunion app_renderer_c2d_storage storage;
};

bool app_renderer_c2d_init(struct app_renderer_c2d *renderer);
size_t app_renderer_c2d_review_body_max_scroll_offset(const char *text);
void app_renderer_c2d_draw(
\tstruct app_renderer_c2d *renderer,
\tconst struct app_screen_model *screen_model
);
void app_renderer_c2d_fini(struct app_renderer_c2d *renderer);

#endif
"""

VALID_APP_RENDERER_C2D_SOURCE = """
#include <3ds.h>
#include <citro2d.h>

#include "app_renderer_c2d.h"
#include "app_screen_model.h"
#include "app_text.h"

#define APP_TEXT_WRAP_COLUMNS 42
#define APP_TEXT_VISIBLE_ROWS 9

size_t app_renderer_c2d_review_body_max_scroll_offset(const char *text)
{
\treturn app_text_max_scroll_offset(
\t\ttext != 0 ? text : "",
\t\tAPP_TEXT_WRAP_COLUMNS,
\t\tAPP_TEXT_VISIBLE_ROWS
\t);
}

static void app_renderer_c2d_draw_text(const char *text)
{
\tC2D_Text c2d_text;
\tC2D_TextBuf text_buffer = C2D_TextBufNew(128);

\tC2D_TextParse(&c2d_text, text_buffer, text);
\tC2D_TextOptimize(&c2d_text);
\tC2D_DrawText(
\t\t&c2d_text,
\t\tC2D_WithColor,
\t\t24.0f,
\t\t48.0f,
\t\t0.5f,
\t\t0.5f,
\t\t0.5f,
\t\t0xFFFFFFFF
\t);
\tC2D_TextBufDelete(text_buffer);
}

bool app_renderer_c2d_init(struct app_renderer_c2d *renderer)
{
\t(void)renderer;
\tC3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
\tC2D_Init(C2D_DEFAULT_MAX_OBJECTS);
\tC2D_Prepare();
\tC2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
\tC2D_SpriteSheetLoadFromMem(theme_t3x, theme_t3x_size);
\treturn true;
}

void app_renderer_c2d_draw(
\tstruct app_renderer_c2d *renderer,
\tconst struct app_screen_model *screen_model
)
{
\t(void)renderer;
\t(void)app_renderer_c2d_review_body_max_scroll_offset(
\t\tscreen_model != 0 ? screen_model->flashcard.display_text : ""
\t);
\tC2D_TextBufClear(C2D_TextBufNew(128));
\tC3D_FrameBegin(C3D_FRAME_SYNCDRAW);
\tC2D_TargetClear(0, 0x000000FF);
\tC2D_SceneBegin(0);
\tC2D_DrawImageAt((C2D_Image){0}, 0.0f, 0.0f, 0.1f, 0, 1.0f, 1.0f);
\tapp_renderer_c2d_draw_text(screen_model->flashcard.display_text);
\tapp_renderer_c2d_draw_text(screen_model->flashcard.status_text);
\tC3D_FrameEnd(0);
}

void app_renderer_c2d_fini(struct app_renderer_c2d *renderer)
{
\t(void)renderer;
\tC2D_Fini();
\tC3D_Fini();
}
"""

VALID_APP_RENDERER_C2D_LAYOUT = """
struct app_render_text_box
{
\tfloat x;
\tfloat y;
\tfloat scale;
\tfloat wrap_width;
};

static const struct app_render_text_box app_menu_top_meta_box =
{
\t49.0f,
\t178.0f,
\t0.50f,
\t302.0f,
};
static const struct app_render_text_box app_review_top_meta_box =
{
\t49.0f,
\t178.0f,
\t0.50f,
\t302.0f,
};
static const struct app_render_text_box app_bottom_footer_box =
{
\t34.0f,
\t174.0f,
\t0.50f,
\t252.0f,
};

static void app_renderer_c2d_write_layout_fingerprint(void)
{
\t(void)"menu_top_meta";
\t(void)"review_top_meta";
\t(void)"bottom_footer";
}
"""

VALID_RENDERER_HANDOFF_MAIN_SOURCE = """
#include <3ds.h>

#include "app_renderer_c2d.h"
#include "app_screen_model.h"
#include "study_backend.h"

int main(int argc, char *argv[])
{
\tstruct app_renderer_c2d renderer;
\tstruct app_screen_model screen_model;
\tstruct study_backend backend;

\t(void)argc;
\t(void)argv;

\tgfxInitDefault();
\tapp_renderer_c2d_init(&renderer);
\tstudy_backend_show_answer(&backend);
\tstudy_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD);
\tapp_screen_model_build(&screen_model, &backend);
\tapp_renderer_c2d_draw(&renderer, &screen_model);
\tapp_renderer_c2d_fini(&renderer);
\tgfxExit();
\treturn 0;
}
"""

VALID_CITRO2D_CLEAN_FE_SOURCE = """
#include <3ds.h>
#include <citro2d.h>

#include "app_text.h"
#include "study_backend.h"

#define APP_TEXT_WRAP_COLUMNS 42
#define APP_TEXT_VISIBLE_ROWS 9
#define APP_TEXT_BUFFER_GLYPHS 8192

static const struct study_backend_card app_sample_cards[] = {
\t{ "Question", "Answer" },
};

static size_t app_view_max_scroll_offset(const struct study_backend_view *view)
{
\treturn app_text_max_scroll_offset(
\t\tview != NULL ? view->primary_text : "",
\t\tAPP_TEXT_WRAP_COLUMNS,
\t\tAPP_TEXT_VISIBLE_ROWS
\t);
}

static void app_draw_text(C2D_TextBuf text_buffer, const char *text)
{
\tC2D_Text c2d_text;

\tC2D_TextParse(&c2d_text, text_buffer, text);
\tC2D_TextOptimize(&c2d_text);
\tC2D_DrawText(
\t\t&c2d_text,
\t\tC2D_WithColor,
\t\t24.0f,
\t\t48.0f,
\t\t0.5f,
\t\t0.5f,
\t\t0.5f,
\t\t0xFFFFFFFF
\t);
}

static void app_draw_top_screen(
\tC2D_Image image,
\tC2D_TextBuf text_buffer,
\tconst struct study_backend_view *view
)
{
\tC2D_DrawImageAt(image, 0.0f, 0.0f, 0.1f, NULL, 1.0f, 1.0f);
\tapp_draw_text(text_buffer, view->primary_text);
}

static void app_draw_bottom_screen(
\tC2D_TextBuf text_buffer,
\tconst struct study_backend_view *view
)
{
\tapp_draw_text(text_buffer, view->status_text);
}

static void app_draw(
\tC3D_RenderTarget *top_target,
\tC3D_RenderTarget *bottom_target,
\tC2D_Image image,
\tC2D_TextBuf text_buffer,
\tconst struct study_backend *backend
)
{
\tstruct study_backend_view view;
\tstudy_backend_build_view(backend, &view);
\tC2D_TextBufClear(text_buffer);
\tC3D_FrameBegin(C3D_FRAME_SYNCDRAW);
\tC2D_TargetClear(top_target, 0x000000FF);
\tC2D_SceneBegin(top_target);
\tapp_draw_top_screen(image, text_buffer, &view);
\tC2D_TargetClear(bottom_target, 0x000000FF);
\tC2D_SceneBegin(bottom_target);
\tapp_draw_bottom_screen(text_buffer, &view);
\tC3D_FrameEnd(0);
}

static C2D_Image app_load_image(void)
{
\tC2D_SpriteSheet sheet;

\tsheet = C2D_SpriteSheetLoadFromMem(theme_t3x, theme_t3x_size);
\treturn C2D_SpriteSheetGetImage(sheet, 0);
}

static void app_apply_review_actions(struct study_backend *backend)
{
\tstudy_backend_show_answer(backend);
\tstudy_backend_rate_current(backend, STUDY_BACKEND_RATING_GOOD);
}

int main(int argc, char *argv[])
{
\tC3D_RenderTarget *top_target;
\tC3D_RenderTarget *bottom_target;
\tC2D_TextBuf text_buffer;
\tC2D_Image image;
\tstruct study_backend backend;
\tstruct study_backend_view view;

\t(void)argc;
\t(void)argv;

\tgfxInitDefault();
\tC3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
\tC2D_Init(C2D_DEFAULT_MAX_OBJECTS);
\tC2D_Prepare();
\ttop_target = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
\tbottom_target = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
\ttext_buffer = C2D_TextBufNew(APP_TEXT_BUFFER_GLYPHS);
\timage = app_load_image();
\tstudy_backend_init(&backend, app_sample_cards, 1);
\tstudy_backend_build_view(&backend, &view);
\t(void)app_view_max_scroll_offset(&view);
\tapp_apply_review_actions(&backend);
\tapp_draw(top_target, bottom_target, image, text_buffer, &backend);
\tC2D_TextBufDelete(text_buffer);
\tC2D_Fini();
\tC3D_Fini();
\tgfxExit();
\treturn 0;
}
"""


class VerifyAppThemeTests(unittest.TestCase):
    def write_source(self, root: Path, source: str = VALID_THEME_SOURCE) -> Path:
        path = root / "main.c"
        path.write_text(source, encoding="utf-8")
        return path

    def write_project_source(
        self,
        root: Path,
        source: str = VALID_THEME_SOURCE,
        fe_renderer_header: str | None = VALID_FE_RENDERER_HEADER,
        fe_renderer_source: str | None = VALID_FE_RENDERER_SOURCE,
        contract_header: str | None = VALID_CONTRACT_HEADER,
        view_model_header: str | None = VALID_VIEW_MODEL_HEADER,
        view_model_source: str | None = VALID_VIEW_MODEL_SOURCE,
    ) -> Path:
        source_dir = root / "app-3ds" / "source"
        include_dir = root / "app-3ds" / "include"
        source_dir.mkdir(parents=True)
        include_dir.mkdir(parents=True)
        main_path = source_dir / "main.c"
        main_path.write_text(source, encoding="utf-8")
        if contract_header is not None:
            (include_dir / "app_review_front_contract.h").write_text(
                contract_header,
                encoding="utf-8",
            )
        if view_model_header is not None:
            (include_dir / "app_view_model.h").write_text(
                view_model_header,
                encoding="utf-8",
            )
        if view_model_source is not None:
            (source_dir / "app_view_model.c").write_text(
                view_model_source,
                encoding="utf-8",
            )
        if fe_renderer_header is not None:
            (include_dir / "app_fe_renderer.h").write_text(
                fe_renderer_header,
                encoding="utf-8",
            )
        if fe_renderer_source is not None:
            (source_dir / "app_fe_renderer.c").write_text(
                fe_renderer_source,
                encoding="utf-8",
            )
        return main_path

    def write_clean_fe_project_source(
        self,
        root: Path,
        source: str = VALID_CITRO2D_CLEAN_FE_SOURCE,
        study_backend_header: str | None = VALID_STUDY_BACKEND_HEADER,
        study_backend_source: str | None = VALID_STUDY_BACKEND_SOURCE,
        app_review_action_header: str | None = None,
        app_review_action_source: str | None = None,
        app_review_flow_header: str | None = None,
        app_review_flow_source: str | None = None,
        app_screen_model_header: str | None = None,
        app_screen_model_source: str | None = None,
        app_renderer_c2d_header: str | None = None,
        app_renderer_c2d_source: str | None = None,
    ) -> Path:
        source_dir = root / "app-3ds" / "source"
        include_dir = root / "app-3ds" / "include"
        source_dir.mkdir(parents=True)
        include_dir.mkdir(parents=True)
        main_path = source_dir / "main.c"
        main_path.write_text(source, encoding="utf-8")
        if study_backend_header is not None:
            (include_dir / "study_backend.h").write_text(
                study_backend_header,
                encoding="utf-8",
            )
        if study_backend_source is not None:
            (source_dir / "study_backend.c").write_text(
                study_backend_source,
                encoding="utf-8",
            )
        if app_review_action_header is not None:
            (include_dir / "app_review_action.h").write_text(
                app_review_action_header,
                encoding="utf-8",
            )
        if app_review_action_source is not None:
            (source_dir / "app_review_action.c").write_text(
                app_review_action_source,
                encoding="utf-8",
            )
        if app_review_flow_header is not None:
            (include_dir / "app_review_flow.h").write_text(
                app_review_flow_header,
                encoding="utf-8",
            )
        if app_review_flow_source is not None:
            (source_dir / "app_review_flow.c").write_text(
                app_review_flow_source,
                encoding="utf-8",
            )
        if app_screen_model_header is not None:
            (include_dir / "app_screen_model.h").write_text(
                app_screen_model_header,
                encoding="utf-8",
            )
        if app_screen_model_source is not None:
            (source_dir / "app_screen_model.c").write_text(
                app_screen_model_source,
                encoding="utf-8",
            )
        if app_renderer_c2d_header is not None:
            (include_dir / "app_renderer_c2d.h").write_text(
                app_renderer_c2d_header,
                encoding="utf-8",
            )
        if app_renderer_c2d_source is not None:
            (source_dir / "app_renderer_c2d.c").write_text(
                app_renderer_c2d_source,
                encoding="utf-8",
            )
        return main_path

    def test_accepts_valid_project_theme_boundary(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_project_source(Path(temp_dir))

            self.assertEqual(verify_app_theme.verify_app_theme_source(path), [])

    def test_accepts_clean_citro2d_shell_with_review_flow(self):
        source = VALID_CITRO2D_CLEAN_FE_SOURCE.replace(
            '#include "study_backend.h"\n',
            '#include "study_backend.h"\n#include "app_review_flow.h"\n',
        ).replace(
            (
                "\tstudy_backend_show_answer(backend);\n"
                "\tstudy_backend_rate_current(backend, STUDY_BACKEND_RATING_GOOD);\n"
            ),
            "\tapp_review_flow_handle_input(backend);\n",
        )
        self.assertNotIn("study_backend_show_answer", source)
        self.assertNotIn("study_backend_rate_current", source)
        self.assertIn("app_review_flow_handle_input", source)

        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                source,
                app_review_action_header=VALID_APP_REVIEW_ACTION_HEADER,
                app_review_action_source=VALID_APP_REVIEW_ACTION_SOURCE,
                app_review_flow_header=VALID_APP_REVIEW_FLOW_HEADER,
                app_review_flow_source=VALID_APP_REVIEW_FLOW_SOURCE,
            )

            self.assertEqual(verify_app_theme.verify_app_theme_source(path), [])

    def test_current_app_theme_source_passes_boundary(self):
        errors = verify_app_theme.verify_app_theme_source(
            ROOT / "app-3ds" / "source" / "main.c"
        )

        self.assertEqual(errors, [])

    def test_accepts_fixed_renderer_footer_baselines(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                source=VALID_RENDERER_HANDOFF_MAIN_SOURCE,
                app_screen_model_header=VALID_APP_SCREEN_MODEL_HEADER,
                app_screen_model_source=VALID_APP_SCREEN_MODEL_SOURCE,
                app_renderer_c2d_header=VALID_APP_RENDERER_C2D_HEADER,
                app_renderer_c2d_source=(
                    VALID_APP_RENDERER_C2D_SOURCE +
                    VALID_APP_RENDERER_C2D_LAYOUT
                ),
            )

            self.assertEqual(verify_app_theme.verify_app_theme_source(path), [])

    def test_rejects_moving_top_meta_baseline(self):
        layout = VALID_APP_RENDERER_C2D_LAYOUT.replace(
            "\t178.0f,\n\t0.50f,\n\t302.0f,\n};\n"
            "static const struct app_render_text_box app_review_top_meta_box",
            "\t166.0f,\n\t0.50f,\n\t302.0f,\n};\n"
            "static const struct app_render_text_box app_review_top_meta_box",
        )
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                source=VALID_RENDERER_HANDOFF_MAIN_SOURCE,
                app_screen_model_header=VALID_APP_SCREEN_MODEL_HEADER,
                app_screen_model_source=VALID_APP_SCREEN_MODEL_SOURCE,
                app_renderer_c2d_header=VALID_APP_RENDERER_C2D_HEADER,
                app_renderer_c2d_source=VALID_APP_RENDERER_C2D_SOURCE + layout,
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("top meta baseline must remain at y=178" in error
                    for error in errors),
                errors,
            )
            self.assertTrue(
                any("top deck and review meta baselines must match" in error
                    for error in errors),
                errors,
            )

    def test_rejects_moving_bottom_footer_baseline(self):
        layout = VALID_APP_RENDERER_C2D_LAYOUT.replace(
            "\t174.0f,\n\t0.50f,\n\t252.0f,\n};",
            "\t150.0f,\n\t0.50f,\n\t252.0f,\n};",
        )
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                source=VALID_RENDERER_HANDOFF_MAIN_SOURCE,
                app_screen_model_header=VALID_APP_SCREEN_MODEL_HEADER,
                app_screen_model_source=VALID_APP_SCREEN_MODEL_SOURCE,
                app_renderer_c2d_header=VALID_APP_RENDERER_C2D_HEADER,
                app_renderer_c2d_source=VALID_APP_RENDERER_C2D_SOURCE + layout,
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("bottom footer baseline must remain fixed at y=174" in error
                    for error in errors),
                errors,
            )

    def test_accepts_valid_clean_citro2d_shell(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(Path(temp_dir))

            self.assertEqual(verify_app_theme.verify_app_theme_source(path), [])

    def test_accepts_clean_citro2d_shell_with_review_action_reducer(self):
        source = VALID_CITRO2D_CLEAN_FE_SOURCE.replace(
            '#include "study_backend.h"\n',
            '#include "study_backend.h"\n#include "app_review_action.h"\n',
        ).replace(
            (
                "\tstudy_backend_show_answer(backend);\n"
                "\tstudy_backend_rate_current(backend, STUDY_BACKEND_RATING_GOOD);\n"
            ),
            "\tapp_review_action_apply(backend);\n",
        )
        self.assertNotIn("study_backend_show_answer", source)
        self.assertNotIn("study_backend_rate_current", source)
        self.assertIn("app_review_action_apply", source)
        self.assertIn("study_backend_show_answer", VALID_APP_REVIEW_ACTION_SOURCE)
        self.assertIn("study_backend_rate_current", VALID_APP_REVIEW_ACTION_SOURCE)

        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                source,
                app_review_action_header=VALID_APP_REVIEW_ACTION_HEADER,
                app_review_action_source=VALID_APP_REVIEW_ACTION_SOURCE,
            )

            self.assertEqual(verify_app_theme.verify_app_theme_source(path), [])

    def test_accepts_clean_citro2d_shell_with_screen_model_handoff(self):
        source = """
#include <3ds.h>
#include <citro2d.h>

#include "app_screen_model.h"
#include "app_text.h"
#include "study_backend.h"

#define APP_TEXT_WRAP_COLUMNS 42
#define APP_TEXT_VISIBLE_ROWS 9
#define APP_TEXT_BUFFER_GLYPHS 8192

static const struct study_backend_card app_sample_cards[] = {
\t{ "Question", "Answer" },
};

static size_t app_model_max_scroll_offset(
\tconst struct app_screen_model *screen_model
)
{
\treturn app_text_max_scroll_offset(
\t\tscreen_model != NULL ? screen_model->flashcard.display_text : "",
\t\tAPP_TEXT_WRAP_COLUMNS,
\t\tAPP_TEXT_VISIBLE_ROWS
\t);
}

static void app_draw_text(C2D_TextBuf text_buffer, const char *text)
{
\tC2D_Text c2d_text;

\tC2D_TextParse(&c2d_text, text_buffer, text);
\tC2D_TextOptimize(&c2d_text);
\tC2D_DrawText(
\t\t&c2d_text,
\t\tC2D_WithColor,
\t\t24.0f,
\t\t48.0f,
\t\t0.5f,
\t\t0.5f,
\t\t0.5f,
\t\t0xFFFFFFFF
\t);
}

static void app_draw_top_screen(
\tC2D_Image image,
\tC2D_TextBuf text_buffer,
\tconst struct app_screen_model *screen_model
)
{
\tC2D_DrawImageAt(image, 0.0f, 0.0f, 0.1f, NULL, 1.0f, 1.0f);
\tapp_draw_text(text_buffer, screen_model->flashcard.display_text);
}

static void app_draw_bottom_screen(
\tC2D_TextBuf text_buffer,
\tconst struct app_screen_model *screen_model
)
{
\tapp_draw_text(text_buffer, screen_model->flashcard.status_text);
}

static void app_draw(
\tC3D_RenderTarget *top_target,
\tC3D_RenderTarget *bottom_target,
\tC2D_Image image,
\tC2D_TextBuf text_buffer,
\tconst struct study_backend *backend
)
{
\tstruct app_screen_model screen_model;

\tapp_screen_model_build(&screen_model, backend);
\tC2D_TextBufClear(text_buffer);
\tC3D_FrameBegin(C3D_FRAME_SYNCDRAW);
\tC2D_TargetClear(top_target, 0x000000FF);
\tC2D_SceneBegin(top_target);
\tapp_draw_top_screen(image, text_buffer, &screen_model);
\tC2D_TargetClear(bottom_target, 0x000000FF);
\tC2D_SceneBegin(bottom_target);
\tapp_draw_bottom_screen(text_buffer, &screen_model);
\tC3D_FrameEnd(0);
}

static C2D_Image app_load_image(void)
{
\tC2D_SpriteSheet sheet;

\tsheet = C2D_SpriteSheetLoadFromMem(theme_t3x, theme_t3x_size);
\treturn C2D_SpriteSheetGetImage(sheet, 0);
}

static void app_apply_review_actions(struct study_backend *backend)
{
\tstudy_backend_show_answer(backend);
\tstudy_backend_rate_current(backend, STUDY_BACKEND_RATING_GOOD);
}

int main(int argc, char *argv[])
{
\tC3D_RenderTarget *top_target;
\tC3D_RenderTarget *bottom_target;
\tC2D_TextBuf text_buffer;
\tC2D_Image image;
\tstruct study_backend backend;
\tstruct app_screen_model screen_model;

\t(void)argc;
\t(void)argv;

\tgfxInitDefault();
\tC3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
\tC2D_Init(C2D_DEFAULT_MAX_OBJECTS);
\tC2D_Prepare();
\ttop_target = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
\tbottom_target = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
\ttext_buffer = C2D_TextBufNew(APP_TEXT_BUFFER_GLYPHS);
\timage = app_load_image();
\tstudy_backend_init(&backend, app_sample_cards, 1);
\tapp_screen_model_build(&screen_model, &backend);
\t(void)app_model_max_scroll_offset(&screen_model);
\tapp_apply_review_actions(&backend);
\tapp_draw(top_target, bottom_target, image, text_buffer, &backend);
\tC2D_TextBufDelete(text_buffer);
\tC2D_Fini();
\tC3D_Fini();
\tgfxExit();
\treturn 0;
}
"""
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                source,
                app_screen_model_header=VALID_APP_SCREEN_MODEL_HEADER,
                app_screen_model_source=VALID_APP_SCREEN_MODEL_SOURCE,
            )

            self.assertEqual(verify_app_theme.verify_app_theme_source(path), [])

    def test_accepts_clean_citro2d_shell_with_renderer_module_handoff(self):
        source = """
#include <3ds.h>

#include "app_renderer_c2d.h"
#include "app_screen_model.h"
#include "study_backend.h"

static const struct study_backend_card app_sample_cards[] = {
\t{ "Question", "Answer" },
};

static void app_apply_review_actions(struct study_backend *backend)
{
\tstudy_backend_show_answer(backend);
\tstudy_backend_rate_current(backend, STUDY_BACKEND_RATING_GOOD);
}

int main(int argc, char *argv[])
{
\tstruct study_backend backend;
\tstruct app_screen_model screen_model;
\tstruct app_renderer_c2d renderer;

\t(void)argc;
\t(void)argv;

\tgfxInitDefault();
\tapp_renderer_c2d_init(&renderer);
\tstudy_backend_init(&backend, app_sample_cards, 1);
\tapp_screen_model_build(&screen_model, &backend);
\tapp_apply_review_actions(&backend);
\tapp_renderer_c2d_draw(&renderer, &screen_model);
\tapp_renderer_c2d_fini(&renderer);
\tgfxExit();
\treturn 0;
}
"""
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                source,
                app_screen_model_header=VALID_APP_SCREEN_MODEL_HEADER,
                app_screen_model_source=VALID_APP_SCREEN_MODEL_SOURCE,
                app_renderer_c2d_header=VALID_APP_RENDERER_C2D_HEADER,
                app_renderer_c2d_source=VALID_APP_RENDERER_C2D_SOURCE,
            )

            self.assertEqual(verify_app_theme.verify_app_theme_source(path), [])

    def test_accepts_clean_citro2d_shell_with_app_shell_handoff(self):
        source = """
#include <3ds.h>

#include "app_renderer_c2d.h"
#include "app_screen_model.h"
#include "app_shell.h"

int main(int argc, char *argv[])
{
\tstruct app_shell shell;
\tstruct app_screen_model screen_model;
\tstruct app_renderer_c2d renderer;

\t(void)argc;
\t(void)argv;

\tgfxInitDefault();
\tapp_renderer_c2d_init(&renderer);
\tapp_shell_init(&shell);
\tapp_shell_build_screen_model(&shell, &screen_model);
\tapp_renderer_c2d_draw(&renderer, &screen_model);
\tapp_shell_handle_frame(&shell);
\tapp_renderer_c2d_fini(&renderer);
\tgfxExit();
\treturn 0;
}
"""
        app_shell_header = """
#include <stdbool.h>

struct app_shell
{
\tint unused;
};
struct app_screen_model;

void app_shell_init(struct app_shell *shell);
void app_shell_build_screen_model(
\tstruct app_shell *shell,
\tstruct app_screen_model *model
);
bool app_shell_handle_frame(struct app_shell *shell);
"""
        app_shell_source = """
#include "app_shell.h"

#include "app_review_flow.h"
#include "study_backend.h"

static struct study_backend backend;

void app_shell_init(struct app_shell *shell)
{
\t(void)shell;
}

void app_shell_build_screen_model(
\tstruct app_shell *shell,
\tstruct app_screen_model *model
)
{
\t(void)shell;
\t(void)model;
}

bool app_shell_handle_frame(struct app_shell *shell)
{
\t(void)shell;
\treturn app_review_flow_handle_input(&backend);
}
"""
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            path = self.write_clean_fe_project_source(
                root,
                source,
                app_review_action_header=VALID_APP_REVIEW_ACTION_HEADER,
                app_review_action_source=VALID_APP_REVIEW_ACTION_SOURCE,
                app_review_flow_header=VALID_APP_REVIEW_FLOW_HEADER,
                app_review_flow_source=VALID_APP_REVIEW_FLOW_SOURCE,
                app_screen_model_header=VALID_APP_SCREEN_MODEL_HEADER,
                app_screen_model_source=VALID_APP_SCREEN_MODEL_SOURCE,
                app_renderer_c2d_header=VALID_APP_RENDERER_C2D_HEADER,
                app_renderer_c2d_source=VALID_APP_RENDERER_C2D_SOURCE,
            )
            (root / "app-3ds" / "include" / "app_shell.h").write_text(
                app_shell_header,
                encoding="utf-8",
            )
            (root / "app-3ds" / "source" / "app_shell.c").write_text(
                app_shell_source,
                encoding="utf-8",
            )

            self.assertEqual(verify_app_theme.verify_app_theme_source(path), [])

    def test_rejects_renderer_module_with_main_citro2d_drawing(self):
        source = """
#include <3ds.h>
#include <citro2d.h>

#include "app_renderer_c2d.h"
#include "app_screen_model.h"
#include "study_backend.h"

static const struct study_backend_card app_sample_cards[] = {
\t{ "Question", "Answer" },
};

static void app_apply_review_actions(struct study_backend *backend)
{
\tstudy_backend_show_answer(backend);
\tstudy_backend_rate_current(backend, STUDY_BACKEND_RATING_GOOD);
}

int main(int argc, char *argv[])
{
\tstruct study_backend backend;
\tstruct app_screen_model screen_model;
\tstruct app_renderer_c2d renderer;

\t(void)argc;
\t(void)argv;

\tgfxInitDefault();
\tC2D_Prepare();
\tapp_renderer_c2d_init(&renderer);
\tstudy_backend_init(&backend, app_sample_cards, 1);
\tapp_screen_model_build(&screen_model, &backend);
\tapp_apply_review_actions(&backend);
\tapp_renderer_c2d_draw(&renderer, &screen_model);
\tapp_renderer_c2d_fini(&renderer);
\tgfxExit();
\treturn 0;
}
"""
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                source,
                app_screen_model_header=VALID_APP_SCREEN_MODEL_HEADER,
                app_screen_model_source=VALID_APP_SCREEN_MODEL_SOURCE,
                app_renderer_c2d_header=VALID_APP_RENDERER_C2D_HEADER,
                app_renderer_c2d_source=VALID_APP_RENDERER_C2D_SOURCE,
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("main.c must render through app_renderer_c2d" in error for error in errors),
                errors,
            )

    def test_rejects_renderer_public_header_citro2d_types(self):
        header = (
            "#include <citro2d.h>\n"
            "struct app_screen_model;\n"
            "struct app_renderer_c2d { C2D_TextBuf text_buffer; };\n"
            "void app_renderer_c2d_draw(\n"
            "\tstruct app_renderer_c2d *renderer,\n"
            "\tconst struct app_screen_model *screen_model\n"
            ");\n"
        )
        source = """
#include <3ds.h>

#include "app_renderer_c2d.h"
#include "app_screen_model.h"
#include "study_backend.h"

static const struct study_backend_card app_sample_cards[] = {
\t{ "Question", "Answer" },
};

static void app_apply_review_actions(struct study_backend *backend)
{
\tstudy_backend_show_answer(backend);
\tstudy_backend_rate_current(backend, STUDY_BACKEND_RATING_GOOD);
}

int main(int argc, char *argv[])
{
\tstruct study_backend backend;
\tstruct app_screen_model screen_model;
\tstruct app_renderer_c2d renderer;

\t(void)argc;
\t(void)argv;

\tgfxInitDefault();
\tstudy_backend_init(&backend, app_sample_cards, 1);
\tapp_screen_model_build(&screen_model, &backend);
\tapp_apply_review_actions(&backend);
\tapp_renderer_c2d_draw(&renderer, &screen_model);
\tgfxExit();
\treturn 0;
}
"""
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                source,
                app_screen_model_header=VALID_APP_SCREEN_MODEL_HEADER,
                app_screen_model_source=VALID_APP_SCREEN_MODEL_SOURCE,
                app_renderer_c2d_header=header,
                app_renderer_c2d_source=VALID_APP_RENDERER_C2D_SOURCE,
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("public header must not expose Citro2D" in error for error in errors),
                errors,
            )

    def test_rejects_renderer_backend_dependency(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                app_screen_model_header=VALID_APP_SCREEN_MODEL_HEADER,
                app_screen_model_source=VALID_APP_SCREEN_MODEL_SOURCE,
                app_renderer_c2d_header=VALID_APP_RENDERER_C2D_HEADER,
                app_renderer_c2d_source=(
                    '#include "study_backend.h"\n' +
                    VALID_APP_RENDERER_C2D_SOURCE
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("must consume app_screen_model" in error for error in errors),
                errors,
            )

    def test_rejects_renderer_backend_forward_declaration(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                app_screen_model_header=VALID_APP_SCREEN_MODEL_HEADER,
                app_screen_model_source=VALID_APP_SCREEN_MODEL_SOURCE,
                app_renderer_c2d_header=VALID_APP_RENDERER_C2D_HEADER,
                app_renderer_c2d_source=(
                    "struct study_session;\n" +
                    VALID_APP_RENDERER_C2D_SOURCE
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("must consume app_screen_model" in error for error in errors),
                errors,
            )

    def test_rejects_renderer_module_missing_display_text_draw(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                app_screen_model_header=VALID_APP_SCREEN_MODEL_HEADER,
                app_screen_model_source=VALID_APP_SCREEN_MODEL_SOURCE,
                app_renderer_c2d_header=VALID_APP_RENDERER_C2D_HEADER,
                app_renderer_c2d_source=VALID_APP_RENDERER_C2D_SOURCE.replace(
                    "screen_model->flashcard.display_text",
                    '"Hard-coded text"',
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("backend primary text" in error for error in errors),
                errors,
            )

    def test_rejects_renderer_module_missing_status_text_draw(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                app_screen_model_header=VALID_APP_SCREEN_MODEL_HEADER,
                app_screen_model_source=VALID_APP_SCREEN_MODEL_SOURCE,
                app_renderer_c2d_header=VALID_APP_RENDERER_C2D_HEADER,
                app_renderer_c2d_source=VALID_APP_RENDERER_C2D_SOURCE.replace(
                    "screen_model->flashcard.status_text",
                    '"Hard-coded status"',
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("backend status text" in error for error in errors),
                errors,
            )

    def test_rejects_public_non_renderer_header_citro2d_type(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            path = self.write_clean_fe_project_source(
                temp_root,
                app_screen_model_header=VALID_APP_SCREEN_MODEL_HEADER,
                app_screen_model_source=VALID_APP_SCREEN_MODEL_SOURCE,
                app_renderer_c2d_header=VALID_APP_RENDERER_C2D_HEADER,
                app_renderer_c2d_source=VALID_APP_RENDERER_C2D_SOURCE,
            )
            (
                temp_root /
                "app-3ds" /
                "include" /
                "app_flashcard_contract.h"
            ).write_text(
                "#include <citro2d.h>\nC2D_Image app_flashcard_image;\n",
                encoding="utf-8",
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("public non-renderer headers" in error for error in errors),
                errors,
            )

    def test_rejects_public_non_renderer_header_geometry_constant(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            path = self.write_clean_fe_project_source(
                temp_root,
                app_screen_model_header=VALID_APP_SCREEN_MODEL_HEADER,
                app_screen_model_source=VALID_APP_SCREEN_MODEL_SOURCE,
                app_renderer_c2d_header=VALID_APP_RENDERER_C2D_HEADER,
                app_renderer_c2d_source=VALID_APP_RENDERER_C2D_SOURCE,
            )
            (
                temp_root /
                "app-3ds" /
                "include" /
                "app_deck_select_contract.h"
            ).write_text(
                "#define APP_DECK_SELECT_VISIBLE_ROWS 6\n",
                encoding="utf-8",
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("display geometry constants" in error for error in errors),
                errors,
            )

    def test_rejects_non_renderer_module_direct_draw_api(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            path = self.write_clean_fe_project_source(
                temp_root,
                app_screen_model_header=VALID_APP_SCREEN_MODEL_HEADER,
                app_screen_model_source=VALID_APP_SCREEN_MODEL_SOURCE,
                app_renderer_c2d_header=VALID_APP_RENDERER_C2D_HEADER,
                app_renderer_c2d_source=VALID_APP_RENDERER_C2D_SOURCE,
            )
            (
                temp_root /
                "app-3ds" /
                "source" /
                "app_flashcard_contract.c"
            ).write_text(
                "void app_flashcard_contract_draw(void) { C2D_DrawText(); }\n",
                encoding="utf-8",
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("non-renderer app/backend modules" in error for error in errors),
                errors,
            )

    def test_rejects_backend_display_contract_include(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = Path(temp_dir)
            path = self.write_clean_fe_project_source(
                temp_root,
                app_screen_model_header=VALID_APP_SCREEN_MODEL_HEADER,
                app_screen_model_source=VALID_APP_SCREEN_MODEL_SOURCE,
                app_renderer_c2d_header=VALID_APP_RENDERER_C2D_HEADER,
                app_renderer_c2d_source=VALID_APP_RENDERER_C2D_SOURCE,
            )
            (
                temp_root /
                "app-3ds" /
                "source" /
                "study_settings.c"
            ).write_text(
                '#include "app_screen_model.h"\n'
                "void study_settings_leak(void) {}\n",
                encoding="utf-8",
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("display contracts or geometry" in error for error in errors),
                errors,
            )

    def test_accepts_clean_citro2d_shell_without_demo_cards(self):
        source = VALID_CITRO2D_CLEAN_FE_SOURCE.replace(
            (
                "static const struct study_backend_card app_sample_cards[] = {\n"
                "\t{ \"Question\", \"Answer\" },\n"
                "};\n\n"
            ),
            "enum app_mode { APP_MODE_DECK_SELECT, APP_MODE_REVIEW };\n\n",
        ).replace(
            "\tstudy_backend_init(&backend, app_sample_cards, 1);",
            "\tstudy_backend_init(&backend, NULL, 0);",
        )
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(Path(temp_dir), source)

            self.assertEqual(verify_app_theme.verify_app_theme_source(path), [])

    def test_rejects_clean_shell_without_citro2d_include(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                VALID_CITRO2D_CLEAN_FE_SOURCE.replace(
                    "#include <citro2d.h>\n",
                    "",
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("must include Citro2D" in error for error in errors),
                errors,
            )

    def test_rejects_clean_shell_without_citro2d_text_draw(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                VALID_CITRO2D_CLEAN_FE_SOURCE.replace(
                    "C2D_DrawText",
                    "C2D_DrawRectSolid",
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("must draw Citro2D text" in error for error in errors),
                errors,
            )

    def test_rejects_clean_shell_console_init(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                VALID_CITRO2D_CLEAN_FE_SOURCE.replace(
                    "\tgfxInitDefault();",
                    "\tgfxInitDefault();\n\tconsoleInit(GFX_TOP, NULL);",
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("must not initialize console" in error for error in errors),
                errors,
            )

    def test_rejects_clean_shell_direct_framebuffer(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                VALID_CITRO2D_CLEAN_FE_SOURCE.replace(
                    "\tgfxInitDefault();",
                    (
                        "\tgfxInitDefault();\n"
                        "\tgfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);"
                    ),
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("direct framebuffer" in error for error in errors),
                errors,
            )

    def test_rejects_clean_shell_without_backend_primary_text(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                VALID_CITRO2D_CLEAN_FE_SOURCE.replace(
                    "view->primary_text",
                    '"Hard-coded text"',
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("backend primary text" in error for error in errors),
                errors,
            )

    def test_rejects_clean_shell_pre_wrapped_flashcard_text(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                VALID_CITRO2D_CLEAN_FE_SOURCE.replace(
                    "view->primary_text",
                    "flashcard_contract.visible_body_text",
                ).replace(
                    "app_text_max_scroll_offset",
                    "app_flashcard_contract_max_body_scroll_offset",
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("wrap raw text locally" in error for error in errors),
                errors,
            )

    def test_rejects_clean_backend_renderer_include(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                study_backend_source=(
                    '#include "study_backend.h"\n'
                    '#include "app_fe_renderer.h"\n'
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("must not include renderer headers" in error for error in errors),
                errors,
            )

    def test_rejects_clean_backend_citro2d_dependency(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                study_backend_source=(
                    '#include "study_backend.h"\n'
                    "#include <citro2d.h>\n"
                    "void backend_render(void) { C2D_Prepare(); }\n"
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("must not depend on Citro2D" in error for error in errors),
                errors,
            )

    def test_rejects_clean_backend_console_dependency(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                study_backend_source=(
                    '#include "study_backend.h"\n'
                    "void backend_console(void) { consoleInit(GFX_TOP, NULL); }\n"
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("must not use console rendering" in error for error in errors),
                errors,
            )

    def test_rejects_clean_backend_geometry_contract(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_clean_fe_project_source(
                Path(temp_dir),
                study_backend_source=(
                    '#include "study_backend.h"\n'
                    '#include "app_review_front_contract.h"\n'
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("renderer geometry contracts" in error for error in errors),
                errors,
            )

    def test_accepts_plain_startup_fe_opt_in_invariant(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_source(Path(temp_dir))

            self.assertEqual(verify_app_theme.verify_app_theme_source(path), [])

    def test_rejects_non_plain_first_theme(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_source(
                Path(temp_dir),
                VALID_THEME_SOURCE.replace(
                    "\tAPP_THEME_PLAIN,\n\tAPP_THEME_AMBER,",
                    "\tAPP_THEME_AMBER,\n\tAPP_THEME_PLAIN,",
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("theme enum must be Plain" in error for error in errors),
                errors,
            )

    def test_rejects_plain_theme_with_framebuffer_paths(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_source(
                Path(temp_dir),
                VALID_THEME_SOURCE.replace(
                    "\t{\n\t\tNULL,\n\t\tNULL,\n\t},",
                    (
                        "\t{\n"
                        '\t\t"sdmc:/3ds/anki3ds/fe-themes/top.bin",\n'
                        '\t\t"sdmc:/3ds/anki3ds/fe-themes/bottom.bin",\n'
                        "\t},"
                    ),
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("Plain must have null top/bottom backgrounds" in error for error in errors),
                errors,
            )

    def test_rejects_missing_review_front_legend_layer_paths(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_source(
                Path(temp_dir),
                VALID_THEME_SOURCE.replace(
                    "top_layer_legend_400x240_bgr888_fb.bin",
                    "top_400x240_bgr888_fb.bin",
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("review front must use top legend layer" in error for error in errors),
                errors,
            )

    def test_rejects_missing_fe_review_front_overlay(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_source(
                Path(temp_dir),
                VALID_THEME_SOURCE.replace(
                    "\tapp_draw_fe_review_front_overlay(app);\n",
                    "",
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("FE review front overlay" in error for error in errors),
                errors,
            )

    def test_rejects_review_front_final_background_fallback(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_source(
                Path(temp_dir),
                VALID_THEME_SOURCE.replace(
                    (
                        "\tif (\n"
                        "\t\tpaths->top_path == NULL ||\n"
                        "\t\tpaths->bottom_path == NULL\n"
                        "\t)\n"
                        "\t{\n"
                        "\t\treturn;\n"
                        "\t}\n"
                    ),
                    (
                        "\tif (\n"
                        "\t\tpaths->top_path == NULL ||\n"
                        "\t\tpaths->bottom_path == NULL\n"
                        "\t)\n"
                        "\t{\n"
                        "\t\treturn;\n"
                        "\t}\n"
                        "\tif (\n"
                        "\t\tkind == APP_THEME_BACKGROUND_REVIEW_FRONT &&\n"
                        "\t\tapp_read_theme_background_pair(&app_theme_backgrounds[theme])\n"
                        "\t)\n"
                        "\t{\n"
                        "\t\treturn;\n"
                        "\t}\n"
                    ),
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("must not fall back to baked final" in error for error in errors),
                errors,
            )

    def test_rejects_missing_fe_renderer_hook(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_source(
                Path(temp_dir),
                VALID_THEME_SOURCE.replace(
                    "\tapp_fe_renderer_draw_review_front_overlay(&contract);\n",
                    "",
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("draw the strings-only contract" in error for error in errors),
                errors,
            )

    def test_rejects_missing_review_front_contract_build(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_source(
                Path(temp_dir),
                VALID_THEME_SOURCE.replace(
                    "\tapp_review_front_contract_build(contract, NULL);\n",
                    "",
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("strings-only contract" in error for error in errors),
                errors,
            )

    def test_rejects_fe_renderer_view_model_dependency(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_project_source(
                Path(temp_dir),
                fe_renderer_header='#include "app_view_model.h"\n',
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("display strings" in error for error in errors),
                errors,
            )

    def test_rejects_fe_renderer_source_view_model_dependency(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_project_source(
                Path(temp_dir),
                fe_renderer_header=(
                    '#include "app_review_front_contract.h"\n'
                    "void app_fe_renderer_draw_review_front_overlay(\n"
                    "\tconst struct app_review_front_contract *contract\n"
                    ");\n"
                ),
                fe_renderer_source='#include "app_view_model.h"\n',
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("display strings" in error for error in errors),
                errors,
            )

    def test_rejects_fe_renderer_coordinate_contract(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_project_source(
                Path(temp_dir),
                fe_renderer_header=(
                    '#include "app_review_front_contract.h"\n'
                    'struct app_fe_text_box { int row; int column; };\n'
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("must not expose layout coordinate structs" in error for error in errors),
                errors,
            )

    def test_rejects_fe_renderer_text_formatting(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_project_source(
                Path(temp_dir),
                fe_renderer_source=(
                    '#include <stdio.h>\n'
                    'static void format_status(void)\n'
                    '{\n'
                    '\tchar text[80];\n'
                    '\tsnprintf(text, sizeof(text), "STATUS: %s", "ready");\n'
                    '}\n'
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("status/battery display wording" in error for error in errors),
                errors,
            )

    def test_rejects_fe_renderer_pre_wrapped_text_lines(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_project_source(
                Path(temp_dir),
                fe_renderer_source=(
                    '#include "app_fe_renderer.h"\n'
                    "void app_fe_renderer_draw_review_front_overlay(\n"
                    "\tconst struct app_review_front_contract *contract\n"
                    ")\n"
                    "{\n"
                    "\t(void)contract->text_lines[0];\n"
                    "\t(void)contract->front_text;\n"
                    "\t(void)contract->scroll_offset;\n"
                    "\t(void)app_text_max_scroll_offset(contract->front_text, 30, 10);\n"
                    "}\n"
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("pre-wrapped lines" in error for error in errors),
                errors,
            )

    def test_rejects_fe_renderer_scroll_hint_text(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_project_source(
                Path(temp_dir),
                fe_renderer_source=(
                    '#include "app_fe_renderer.h"\n'
                    "void app_fe_renderer_draw_review_front_overlay(\n"
                    "\tconst struct app_review_front_contract *contract\n"
                    ")\n"
                    "{\n"
                    "\t(void)contract->front_text;\n"
                    "\t(void)contract->scroll_hint_text;\n"
                    "\t(void)contract->scroll_offset;\n"
                    "\t(void)app_text_max_scroll_offset(contract->front_text, 30, 10);\n"
                    "}\n"
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("scroll-hint presentation" in error for error in errors),
                errors,
            )

    def test_rejects_contract_layout_geometry(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_project_source(
                Path(temp_dir),
                contract_header=(
                    "#include <stddef.h>\n"
                    "struct app_review_front_contract\n"
                    "{\n"
                    "\tconst char *front_text;\n"
                    "\tint width;\n"
                    "};\n"
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("must not expose layout geometry" in error for error in errors),
                errors,
            )

    def test_rejects_contract_pre_wrapped_text(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_project_source(
                Path(temp_dir),
                contract_header=(
                    "#include <stddef.h>\n"
                    "struct app_review_front_contract\n"
                    "{\n"
                    "\tconst char *front_text;\n"
                    "\tchar text_lines[10][80];\n"
                    "};\n"
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("raw text, not wrapped text lines" in error for error in errors),
                errors,
            )

    def test_rejects_contract_scroll_hint_text(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_project_source(
                Path(temp_dir),
                contract_header=(
                    "#include <stddef.h>\n"
                    "struct app_review_front_contract\n"
                    "{\n"
                    "\tconst char *front_text;\n"
                    "\tchar scroll_hint_text[24];\n"
                    "};\n"
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("scroll state, not scroll-hint text" in error for error in errors),
                errors,
            )

    def test_rejects_view_model_layout_include(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_project_source(
                Path(temp_dir),
                view_model_source=(
                    '#include "app_view_model.h"\n'
                    '#include "app_layout.h"\n'
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("must not include app_layout.h" in error for error in errors),
                errors,
            )

    def test_rejects_view_model_review_front_geometry(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_project_source(
                Path(temp_dir),
                view_model_header=(
                    '#include "app_review_front_contract.h"\n'
                    "struct app_review_text_box { int row; int width; };\n"
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("must not expose layout coordinate structs" in error for error in errors),
                errors,
            )

    def test_rejects_view_model_scroll_metrics(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_project_source(
                Path(temp_dir),
                view_model_source=(
                    '#include "app_view_model.h"\n'
                    "static void build(void)\n"
                    "{\n"
                    "\t(void)app_text_max_scroll_offset(\"front\", 30, 10);\n"
                    "}\n"
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("text wrapping and scroll metrics" in error for error in errors),
                errors,
            )

    def test_rejects_missing_font_atlas_path(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_source(
                Path(temp_dir),
                VALID_THEME_SOURCE.replace(
                    "fe_font_review_8x14_alpha.bin",
                    "missing_font.bin",
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("generated font atlas" in error for error in errors),
                errors,
            )

    def test_rejects_initial_theme_that_prefers_fe(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_source(
                Path(temp_dir),
                VALID_THEME_SOURCE.replace(
                    (
                        "\tapp_load_theme_background(APP_THEME_PLAIN, APP_THEME_BACKGROUND_FINAL);\n"
                        "\treturn APP_THEME_PLAIN;"
                    ),
                    (
                        "\tapp_load_theme_background(APP_THEME_FOREST, APP_THEME_BACKGROUND_FINAL);\n"
                        "\tif (app_top_background_loaded && app_bottom_background_loaded)\n"
                        "\t\treturn APP_THEME_FOREST;\n\n"
                        "\tapp_load_theme_background(APP_THEME_PLAIN, APP_THEME_BACKGROUND_FINAL);\n"
                        "\treturn APP_THEME_PLAIN;"
                    ),
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("startup theme must be Plain" in error for error in errors),
                errors,
            )

    def test_rejects_app_init_without_initial_theme(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_source(
                Path(temp_dir),
                VALID_THEME_SOURCE.replace(
                    "app->theme = app_initial_theme();",
                    "app->theme = APP_THEME_PLAIN;",
                ),
            )

            errors = verify_app_theme.verify_app_theme_source(path)

            self.assertTrue(
                any("app_init must use app_initial_theme" in error for error in errors),
                errors,
            )

    def test_cli_reports_errors_without_traceback(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_source(Path(temp_dir), "enum app_theme { APP_THEME_COUNT };\n")
            stderr = io.StringIO()

            with mock.patch(
                "sys.argv",
                ["verify_app_theme.py", str(path)],
            ), redirect_stderr(stderr):
                exit_code = verify_app_theme.main()

            self.assertEqual(exit_code, 1)
            self.assertIn("theme enum must be Plain", stderr.getvalue())
            self.assertNotIn("Traceback", stderr.getvalue())


if __name__ == "__main__":
    unittest.main()
