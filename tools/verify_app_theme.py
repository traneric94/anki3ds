#!/usr/bin/env python3
"""Verify app theme startup and fallback invariants."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


DEFAULT_MAIN_C = Path("app-3ds/source/main.c")
EXPECTED_THEME_ORDER = (
    "APP_THEME_PLAIN",
    "APP_THEME_AMBER",
    "APP_THEME_FOREST",
    "APP_THEME_RUBY",
    "APP_THEME_CHALK",
    "APP_THEME_COUNT",
)

CLEAN_FE_REQUIRED_CITRO2D_CALLS = (
    ("C3D_Init", "clean FE shell must initialize Citro3D"),
    ("C2D_Init", "clean FE shell must initialize Citro2D"),
    ("C2D_Prepare", "clean FE shell must prepare Citro2D"),
    ("C2D_CreateScreenTarget", "clean FE shell must create Citro2D screen targets"),
    ("C2D_TargetClear", "clean FE shell must clear Citro2D targets"),
    ("C2D_SceneBegin", "clean FE shell must begin Citro2D scenes"),
    ("C2D_SpriteSheetLoadFromMem", "clean FE shell must load Citro2D spritesheets"),
    ("C2D_DrawImageAt", "clean FE shell must draw Citro2D images"),
    ("C2D_DrawText", "clean FE shell must draw Citro2D text"),
)

CLEAN_FE_DIRECT_FRAMEBUFFER_PATTERNS = (
    r"\bgfxSetDoubleBuffering\s*\(",
    r"\bgfxGetFramebuffer\s*\(",
    r"\bgfxFlushBuffers\s*\(",
    r"\bgfxSwapBuffers\s*\(",
    r"\bgfxScreenSwapBuffers\s*\(",
    r"\bGSPGPU_FlushDataCache\s*\(",
    r"\bframebuffer\b",
)

CITRO2D_PUBLIC_TYPE_PATTERN = re.compile(
    r"#\s*include\s*<citro[23]d\.h>|"
    r"\bC[23]D_[A-Za-z0-9_]*\b|"
    r"\bC2D_(?:Image|Text|TextBuf|Sprite|SpriteSheet)\b|"
    r"\bC3D_RenderTarget\b"
)

DIRECT_RENDER_API_PATTERN = re.compile(
    r"#\s*include\s*<citro[23]d\.h>|"
    r"\bC[23]D_[A-Za-z0-9_]*\b|"
    r"\bC2D_[A-Za-z0-9_]*\b|"
    r"\bC3D_[A-Za-z0-9_]*\b|"
    r"\b(?:consoleInit|consoleSelect|consoleClear|consoleSetWindow|"
    r"PrintConsole)\b|"
    r"\bgfx(?:GetFramebuffer|FlushBuffers|SwapBuffers|ScreenSwapBuffers)"
    r"\s*\(|"
    r"\bGSPGPU_FlushDataCache\s*\(|"
    r"\bframebuffer\b"
)

PUBLIC_DISPLAY_GEOMETRY_DEFINE_PATTERN = re.compile(
    r"^\s*#\s*define\s+APP_[A-Z0-9_]*(?:VISIBLE_ROWS|VISIBLE_COLUMNS|"
    r"ROW_COUNT|COLUMN_COUNT|SCREEN_WIDTH|SCREEN_HEIGHT|PANEL_[XYWH]|"
    r"LEFT|RIGHT|TOP|BOTTOM)\b",
    re.MULTILINE,
)

BACKEND_DISPLAY_CONTRACT_PATTERN = re.compile(
    r'#\s*include\s+"app_(?:renderer|screen_model|layout|'
    r'.*(?:contract|view_model|display_model))\.h"|'
    r"\bAPP_(?:RENDERER|SCREEN_MODEL|SCREEN|LAYOUT|FLASHCARD|"
    r"DECK_SELECT|SETTINGS|CONFIRM)_|"
    r"\bapp_(?:screen_model|flashcard|deck_select|settings|confirm)_|"
    r"\bapp_text_max_scroll_offset\b"
)

RENDERER_BACKEND_PATTERN = re.compile(
    r'#\s*include\s+"(?:study_[^"]+|deck|deck_index|scheduler|'
    r'storage|review_log|review_state)\.h"|'
    r"\bstudy_backend_|\bstruct\s+study_(?:backend|deck_index|session|"
    r"settings|review_log|controls)\b|"
    r"\bstudy_deck_index_|\bstudy_session_|\bstudy_review_log_|"
    r"\bstudy_settings_|\bstudy_controls_"
)

RENDERER_TEXT_BOX_PATTERN = re.compile(
    r"static\s+const\s+struct\s+app_render_text_box\s+"
    r"(?P<name>app_[A-Za-z0-9_]+)\s*=\s*\{\s*"
    r"(?P<x>[^,]+)\s*,\s*"
    r"(?P<y>[^,]+)\s*,\s*"
    r"(?P<scale>[^,]+)\s*,\s*"
    r"(?P<wrap>[^,]+)\s*,?\s*\}\s*;",
    re.DOTALL,
)


def strip_c_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//.*", "", text)


def append_error(errors: list[str], path: Path, message: str) -> None:
    errors.append(f"{path}: {message}")


def read_theme_source(path: Path) -> str:
    source_parts = [path.read_text(encoding="utf-8")]
    display_source = path.parent / "app_display_model.c"
    display_header = path.parent.parent / "include" / "app_display_model.h"
    view_model_source = path.parent / "app_view_model.c"
    view_model_header = path.parent.parent / "include" / "app_view_model.h"
    review_front_contract_header = (
        path.parent.parent / "include" / "app_review_front_contract.h"
    )
    review_action_source = path.parent / "app_review_action.c"
    review_action_header = path.parent.parent / "include" / "app_review_action.h"
    review_flow_source = path.parent / "app_review_flow.c"
    review_flow_header = path.parent.parent / "include" / "app_review_flow.h"
    c2d_renderer_source = path.parent / "app_renderer_c2d.c"
    c2d_renderer_header = path.parent.parent / "include" / "app_renderer_c2d.h"
    app_shell_source = path.parent / "app_shell.c"
    app_shell_header = path.parent.parent / "include" / "app_shell.h"
    screen_model_source = path.parent / "app_screen_model.c"
    screen_model_header = path.parent.parent / "include" / "app_screen_model.h"
    fe_renderer_source = path.parent / "app_fe_renderer.c"
    fe_renderer_header = path.parent.parent / "include" / "app_fe_renderer.h"

    for extra_path in (
        display_header,
        display_source,
        view_model_header,
        view_model_source,
        review_front_contract_header,
        review_action_header,
        review_action_source,
        review_flow_header,
        review_flow_source,
        c2d_renderer_header,
        c2d_renderer_source,
        app_shell_header,
        app_shell_source,
        screen_model_header,
        screen_model_source,
        fe_renderer_header,
        fe_renderer_source,
    ):
        if extra_path.exists():
            source_parts.append(extra_path.read_text(encoding="utf-8"))

    return "\n".join(source_parts)


def read_optional_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError:
        return ""


def existing_paths(*patterns: str, base: Path) -> list[Path]:
    paths: list[Path] = []
    for pattern in patterns:
        for candidate in sorted(base.glob(pattern)):
            if candidate.exists() and candidate not in paths:
                paths.append(candidate)
    return paths


def read_stripped(path: Path) -> str:
    return strip_c_comments(read_optional_text(path))


def extract_theme_enum_members(text: str) -> list[str] | None:
    match = re.search(r"enum\s+app_theme\s*\{(?P<body>.*?)\};", text, re.DOTALL)
    if match is None:
        return None

    members: list[str] = []
    for member in re.findall(r"\bAPP_THEME_[A-Z0-9_]+\b", match.group("body")):
        if member not in members:
            members.append(member)

    return members


def extract_initial_theme_body(text: str) -> str | None:
    match = re.search(
        r"static\s+enum\s+app_theme\s+app_initial_theme\s*"
        r"\(\s*void\s*\)\s*\{(?P<body>.*?)\n\}",
        text,
        re.DOTALL,
    )
    if match is None:
        return None

    return match.group("body")


def initial_theme_starts_plain(body: str) -> bool:
    if not re.search(
        r"app_load_theme_background\s*\(\s*APP_THEME_PLAIN\s*"
        r"(?:,\s*APP_THEME_BACKGROUND_FINAL\s*)?\)\s*;"
        r"\s*return\s+APP_THEME_PLAIN\s*;",
        body,
        re.DOTALL,
    ):
        return False

    return all(theme not in body for theme in (
        "APP_THEME_AMBER",
        "APP_THEME_FOREST",
        "APP_THEME_RUBY",
        "APP_THEME_CHALK",
    ))


def source_is_clean_fe_shell(main_source: str, source: str) -> bool:
    direct_clean_shell = (
        '#include "study_backend.h"' in main_source and
        (
            "C2D_" in main_source or
            "C3D_" in main_source or
            source_has_call(main_source, "study_backend_build_view")
        )
    )
    renderer_module_shell = (
        (
            '#include "app_renderer_c2d.h"' in main_source or
            source_has_call(main_source, "app_renderer_c2d_draw")
        ) and
        (
            "C2D_" in source or
            "C3D_" in source or
            source_has_call(source, "study_backend_build_view")
        )
    )

    return direct_clean_shell or renderer_module_shell


def source_has_call(source: str, call_name: str) -> bool:
    return re.search(rf"\b{re.escape(call_name)}\s*\(", source) is not None


def parse_c_float(value: str) -> float | None:
    try:
        return float(value.strip().rstrip("f"))
    except ValueError:
        return None


def renderer_text_boxes(
    source: str,
) -> dict[str, tuple[float | None, float | None, float | None, float | None]]:
    boxes: dict[str, tuple[float | None, float | None, float | None, float | None]] = {}
    for match in RENDERER_TEXT_BOX_PATTERN.finditer(source):
        boxes[match.group("name")] = (
            parse_c_float(match.group("x")),
            parse_c_float(match.group("y")),
            parse_c_float(match.group("scale")),
            parse_c_float(match.group("wrap")),
        )

    return boxes


def verify_clean_fe_renderer_layout(
    path: Path,
    c2d_renderer_source: str,
) -> list[str]:
    errors: list[str] = []
    boxes = renderer_text_boxes(c2d_renderer_source)
    expected_box_names = (
        "app_menu_top_meta_box",
        "app_review_top_meta_box",
        "app_bottom_footer_box",
    )

    if not any(name in c2d_renderer_source for name in expected_box_names):
        return errors

    for name in expected_box_names:
        if name not in boxes:
            append_error(errors, path, f"renderer missing {name} layout box")
    if errors:
        return errors

    menu_top_meta_y = boxes["app_menu_top_meta_box"][1]
    review_top_meta_y = boxes["app_review_top_meta_box"][1]
    bottom_footer_y = boxes["app_bottom_footer_box"][1]

    if menu_top_meta_y != review_top_meta_y:
        append_error(
            errors,
            path,
            "top deck and review meta baselines must match",
        )
    if menu_top_meta_y != 178.0:
        append_error(
            errors,
            path,
            "top meta baseline must remain at y=178",
        )
    if bottom_footer_y != 174.0:
        append_error(
            errors,
            path,
            "bottom footer baseline must remain fixed at y=174",
        )
    if "bottom_footer" not in c2d_renderer_source:
        append_error(
            errors,
            path,
            "renderer fingerprint must include bottom_footer layout",
        )

    return errors


def clean_shell_transition_source(main_source: str, source: str) -> str:
    if (
        source_has_call(main_source, "app_review_action_apply") or
        source_has_call(main_source, "app_review_flow_handle_input") or
        source_has_call(source, "app_review_action_apply") or
        source_has_call(source, "app_review_flow_handle_input")
    ):
        return source
    return main_source


def verify_clean_fe_shell_source(path: Path, main_source: str, source: str) -> list[str]:
    errors: list[str] = []
    source_dir = path.parent
    include_dir = path.parent.parent / "include"
    study_backend_header_path = path.parent.parent / "include" / "study_backend.h"
    study_backend_source_path = path.parent / "study_backend.c"
    c2d_renderer_header_path = (
        path.parent.parent / "include" / "app_renderer_c2d.h"
    )
    c2d_renderer_source_path = path.parent / "app_renderer_c2d.c"
    study_backend_text = strip_c_comments(
        read_optional_text(study_backend_header_path) +
        "\n" +
        read_optional_text(study_backend_source_path)
    )
    c2d_renderer_header = strip_c_comments(
        read_optional_text(c2d_renderer_header_path)
    )
    c2d_renderer_source = strip_c_comments(
        read_optional_text(c2d_renderer_source_path)
    )
    c2d_renderer_text = c2d_renderer_header + "\n" + c2d_renderer_source
    has_c2d_renderer_module = bool(
        c2d_renderer_header.strip() or c2d_renderer_source.strip()
    )
    public_non_renderer_headers = [
        header_path
        for header_path in existing_paths("*.h", base=include_dir)
        if header_path.name != "app_renderer_c2d.h"
    ]
    non_renderer_module_paths = [
        module_path
        for module_path in (
            existing_paths("*.h", base=include_dir) +
            existing_paths("*.c", base=source_dir)
        )
        if module_path.name not in (
            "main.c",
            "app_renderer_c2d.h",
            "app_renderer_c2d.c",
        )
    ]
    backend_module_paths = (
        existing_paths("study_*.h", base=include_dir) +
        existing_paths("study_*.c", base=source_dir)
    )

    if re.search(r"#\s*include\s*<citro2d\.h>", source) is None:
        append_error(errors, path, "clean FE shell must include Citro2D")
    if has_c2d_renderer_module:
        if re.search(
            r"#\s*include\s*<citro2d\.h>|\bC[23]D_|\bC2D_[A-Za-z0-9_]*|"
            r"\bC3D_[A-Za-z0-9_]*|\bC2D_Image\b|\bC2D_TextBuf\b|"
            r"\bC2D_SpriteSheet\b|\bC3D_RenderTarget\b",
            main_source,
        ):
            append_error(
                errors,
                path,
                "main.c must render through app_renderer_c2d, not Citro2D directly",
            )
        if re.search(
            r"#\s*include\s*<citro2d\.h>|\bC[23]D_|\bC2D_[A-Za-z0-9_]*|"
            r"\bC3D_[A-Za-z0-9_]*|\bC2D_Image\b|\bC2D_TextBuf\b|"
            r"\bC2D_SpriteSheet\b|\bC3D_RenderTarget\b",
            c2d_renderer_header,
        ):
            append_error(
                errors,
                c2d_renderer_header_path,
                "app_renderer_c2d public header must not expose Citro2D internals",
            )
        if RENDERER_BACKEND_PATTERN.search(c2d_renderer_text):
            append_error(
                errors,
                c2d_renderer_source_path,
                "Citro2D renderer must consume app_screen_model, not backend state",
            )
        errors.extend(
            verify_clean_fe_renderer_layout(
                c2d_renderer_source_path,
                c2d_renderer_source,
            )
        )
    for header_path in public_non_renderer_headers:
        header_text = read_stripped(header_path)
        if CITRO2D_PUBLIC_TYPE_PATTERN.search(header_text):
            append_error(
                errors,
                header_path,
                "public non-renderer headers must not expose Citro2D/Citro3D types",
            )
        if PUBLIC_DISPLAY_GEOMETRY_DEFINE_PATTERN.search(header_text):
            append_error(
                errors,
                header_path,
                "public non-renderer headers must not expose display geometry constants",
            )
    for module_path in non_renderer_module_paths:
        module_text = read_stripped(module_path)
        if DIRECT_RENDER_API_PATTERN.search(module_text):
            append_error(
                errors,
                module_path,
                "non-renderer app/backend modules must not call draw APIs",
            )
    for module_path in backend_module_paths:
        module_text = read_stripped(module_path)
        if BACKEND_DISPLAY_CONTRACT_PATTERN.search(module_text):
            append_error(
                errors,
                module_path,
                "backend modules must not include display contracts or geometry",
            )
    for call_name, message in CLEAN_FE_REQUIRED_CITRO2D_CALLS:
        if not source_has_call(source, call_name):
            append_error(errors, path, message)
    if "consoleInit" in source:
        append_error(errors, path, "clean FE shell must not initialize console")
    for pattern in CLEAN_FE_DIRECT_FRAMEBUFFER_PATTERNS:
        if re.search(pattern, source):
            append_error(
                errors,
                path,
                "clean FE shell must not use direct framebuffer drawing",
            )
            break
    if "app_preview_text" in source:
        append_error(errors, path, "clean FE shell text must come from backend view")
    if "study_backend_build_view" not in source:
        append_error(errors, path, "clean FE shell must consume backend view")
    text_render_source = c2d_renderer_source if has_c2d_renderer_module else source
    renders_backend_primary_text = (
        re.search(r"\bview(?:\.|->)primary_text\b", text_render_source) is not None or
        (
            source_has_call(source, "app_flashcard_text_view_build") and
            re.search(
                r"\btext_view(?:\.|->)display_text\b|"
                r"\bflashcard_text_view\.display_text\b|"
                r"\bscreen_model(?:\.|->)flashcard\.display_text\b|"
                r"\bmodel(?:\.|->)flashcard\.display_text\b",
                text_render_source,
            ) is not None
        )
    )
    renders_backend_status_text = (
        re.search(r"\bview(?:\.|->)status_text\b", text_render_source) is not None or
        (
            source_has_call(source, "app_flashcard_text_view_build") and
            re.search(
                r"\btext_view(?:\.|->)status_text\b|"
                r"\bflashcard_text_view\.status_text\b|"
                r"\bscreen_model(?:\.|->)flashcard\.status_text\b|"
                r"\bmodel(?:\.|->)flashcard\.status_text\b",
                text_render_source,
            ) is not None
        )
    )
    if not renders_backend_primary_text:
        append_error(errors, path, "Citro2D renderer must render backend primary text")
    if not renders_backend_status_text:
        append_error(errors, path, "Citro2D renderer must render backend status text")
    if "app_text_max_scroll_offset" not in source:
        append_error(errors, path, "clean FE shell must compute scroll from text")
    if (
        "visible_body_text" in source or
        "app_flashcard_contract_max_body_scroll_offset" in source
    ):
        append_error(
            errors,
            path,
            "clean FE shell renderer must wrap raw text locally",
        )
    transition_source = clean_shell_transition_source(main_source, source)
    if not source_has_call(transition_source, "study_backend_show_answer"):
        append_error(errors, path, "clean FE shell must expose answer transition")
    if not source_has_call(transition_source, "study_backend_rate_current"):
        append_error(errors, path, "clean FE shell must expose rating transition")
    if re.search(r'#\s*include\s+"[^"]*renderer[^"]*\.h"', study_backend_text):
        append_error(
            errors,
            study_backend_source_path,
            "backend must not include renderer headers",
        )
    if re.search(
        r"#\s*include\s*<citro[23]d\.h>|\bC[23]D_",
        study_backend_text,
    ):
        append_error(
            errors,
            study_backend_source_path,
            "backend must not depend on Citro2D/Citro3D rendering",
        )
    if re.search(
        r"\b(?:consoleInit|consoleSelect|consoleClear|consoleSetWindow|PrintConsole)\b",
        study_backend_text,
    ):
        append_error(
            errors,
            study_backend_source_path,
            "backend must not use console rendering",
        )
    if re.search(
        r'#\s*include\s+"(?:app_review_front_contract|app_layout)\.h"|'
        r"\bapp_review_front_contract\b|\bAPP_LAYOUT_|\bstruct\s+app_fe_",
        study_backend_text,
    ):
        append_error(
            errors,
            study_backend_source_path,
            "backend must not include renderer geometry contracts",
        )

    return errors


def verify_app_theme_source(path: Path) -> list[str]:
    errors: list[str] = []
    try:
        main_source = strip_c_comments(path.read_text(encoding="utf-8"))
        source = strip_c_comments(read_theme_source(path))
    except OSError as error:
        return [f"{path}: {error}"]

    view_model_source_path = path.parent / "app_view_model.c"
    view_model_header_path = path.parent.parent / "include" / "app_view_model.h"
    review_front_contract_header_path = (
        path.parent.parent / "include" / "app_review_front_contract.h"
    )
    fe_renderer_source_path = path.parent / "app_fe_renderer.c"
    fe_renderer_header_path = path.parent.parent / "include" / "app_fe_renderer.h"
    view_model_source = strip_c_comments(read_optional_text(view_model_source_path))
    view_model_header = strip_c_comments(read_optional_text(view_model_header_path))
    review_front_contract_header = strip_c_comments(
        read_optional_text(review_front_contract_header_path)
    )
    fe_renderer_source = strip_c_comments(read_optional_text(fe_renderer_source_path))
    fe_renderer_header = strip_c_comments(read_optional_text(fe_renderer_header_path))

    view_model_text = view_model_header + "\n" + view_model_source
    fe_renderer_text = fe_renderer_header + "\n" + fe_renderer_source

    if source_is_clean_fe_shell(main_source, source):
        return verify_clean_fe_shell_source(path, main_source, source)

    if '#include "app_view_model.h"' in fe_renderer_header:
        append_error(
            errors,
            fe_renderer_header_path,
            "FE renderer must accept display strings, not app view-model types",
        )
    if '#include "app_view_model.h"' in fe_renderer_source:
        append_error(
            errors,
            fe_renderer_source_path,
            "FE renderer must accept display strings, not app view-model types",
        )
    if (
        fe_renderer_header and
        '#include "app_review_front_contract.h"' not in fe_renderer_header
    ):
        append_error(
            errors,
            fe_renderer_header_path,
            "FE renderer must accept the strings-only review-front contract",
        )
    if re.search(
        r"app_fe_renderer_draw_review_front_overlay\s*"
        r"\(\s*const\s+struct\s+app_review_front_contract\s*\*",
        fe_renderer_header,
    ) is None and fe_renderer_header:
        append_error(
            errors,
            fe_renderer_header_path,
            "FE renderer overlay must accept the strings-only review-front contract",
        )
    if re.search(
        r"struct\s+app_fe_(text_box|scroll_hint|legend_item|review_front_overlay)\b",
        fe_renderer_text,
    ):
        append_error(
            errors,
            fe_renderer_header_path,
            "FE renderer contract must not expose layout coordinate structs",
        )
    if "struct app_review_front_view" in fe_renderer_text:
        append_error(
            errors,
            fe_renderer_source_path,
            "FE renderer must not depend on app review-front view structs",
        )
    if "contract->text_lines" in fe_renderer_source:
        append_error(
            errors,
            fe_renderer_source_path,
            "FE renderer must wrap raw front text instead of drawing pre-wrapped lines",
        )
    if "contract->scroll_hint_text" in fe_renderer_source:
        append_error(
            errors,
            fe_renderer_source_path,
            "FE renderer must own scroll-hint presentation",
        )
    if "STATUS:" in fe_renderer_source or "BAT " in fe_renderer_source:
        append_error(
            errors,
            fe_renderer_source_path,
            "FE renderer must not own status/battery display wording",
        )
    if fe_renderer_source and "contract->front_text" not in fe_renderer_source:
        append_error(
            errors,
            fe_renderer_source_path,
            "FE renderer must render and wrap raw front text from the contract",
        )
    if (
        fe_renderer_source and
        (
            "contract->scroll_offset" not in fe_renderer_source or
            "app_text_max_scroll_offset" not in fe_renderer_source
        )
    ):
        append_error(
            errors,
            fe_renderer_source_path,
            "FE renderer must derive scroll metrics from raw text and local layout",
        )
    if (
        fe_renderer_source and
        "contract->legend_lines" in fe_renderer_source and
        "APP_FE_REVIEW_FRONT_LEGEND" not in fe_renderer_source and
        "app_fe_review_front_legend_" not in fe_renderer_source
    ):
        append_error(
            errors,
            fe_renderer_source_path,
            "FE renderer must own review-front legend placement",
        )

    if review_front_contract_header:
        if re.search(
            r"\b(?:row|rows|column|columns|left|width|height|text_box)\b",
            review_front_contract_header,
        ):
            append_error(
                errors,
                review_front_contract_header_path,
                "review-front contract must not expose layout geometry",
            )
        if "text_lines" in review_front_contract_header:
            append_error(
                errors,
                review_front_contract_header_path,
                "review-front contract must expose raw text, not wrapped text lines",
            )
        if "scroll_hint_text" in review_front_contract_header:
            append_error(
                errors,
                review_front_contract_header_path,
                "review-front contract must expose scroll state, not scroll-hint text",
            )

    if '#include "app_layout.h"' in view_model_text:
        append_error(
            errors,
            view_model_source_path,
            "app view-model must not include app_layout.h for review-front geometry",
        )
    if re.search(
        r"struct\s+app_review_(?:text_box|scroll_hint|legend_item)\b",
        view_model_text,
    ):
        append_error(
            errors,
            view_model_header_path,
            "review-front view-model must not expose layout coordinate structs",
        )
    if re.search(
        r"APP_LAYOUT_(?:FE_)?REVIEW_FRONT|APP_LAYOUT_REVIEW_CARD_TEXT",
        view_model_text,
    ):
        append_error(
            errors,
            view_model_source_path,
            "review-front view-model must not depend on layout constants",
        )
    if "app_text_max_scroll_offset" in view_model_text:
        append_error(
            errors,
            view_model_source_path,
            "FE renderer must own review-front text wrapping and scroll metrics",
        )
    if "app_review_format_scroll_hint" in view_model_text:
        append_error(
            errors,
            view_model_source_path,
            "FE renderer must own review-front scroll-hint presentation",
        )
    enum_members = extract_theme_enum_members(source)
    if enum_members is None:
        append_error(errors, path, "missing enum app_theme")
    elif tuple(enum_members) != EXPECTED_THEME_ORDER:
        append_error(
            errors,
            path,
            "theme enum must be Plain, Amber, Forest, Ruby, Chalk, Count",
        )

    if not re.search(
        r"static\s+const\s+struct\s+app_visual_theme\s+app_visual_themes\[\]\s*=\s*"
        r"\{\s*\{\s*\"Plain\"\s*,",
        source,
        re.DOTALL,
    ):
        append_error(errors, path, "Plain must be the first visual theme")

    if not re.search(
        r"static\s+const\s+struct\s+app_theme_background_paths\s+"
        r"app_theme_backgrounds\[\]\s*=\s*\{\s*\{\s*NULL\s*,\s*NULL\s*,\s*\}",
        source,
        re.DOTALL,
    ):
        append_error(errors, path, "Plain must have null top/bottom backgrounds")

    if not re.search(
        r"static\s+const\s+struct\s+app_theme_background_paths\s+"
        r"app_theme_review_front_backgrounds\[\]",
        source,
        re.DOTALL,
    ):
        append_error(errors, path, "review front must have separate FE layer paths")

    if "top_layer_background_400x240_bgr888_fb.bin" not in source:
        append_error(errors, path, "FE themes must use top background layer")
    if "bottom_layer_background_320x240_bgr888_fb.bin" not in source:
        append_error(errors, path, "FE themes must use bottom background layer")
    if "top_layer_legend_400x240_bgr888_fb.bin" not in source:
        append_error(errors, path, "review front must use top legend layer background")
    if "bottom_layer_legend_320x240_bgr888_fb.bin" not in source:
        append_error(errors, path, "review front must use bottom legend layer background")
    if "app_draw_fe_review_front_overlay(app);" not in source:
        append_error(errors, path, "FE review front overlay must draw after backgrounds")
    if not re.search(
        r"kind\s*==\s*APP_THEME_BACKGROUND_REVIEW_FRONT\s*\)\s*"
        r"return\s+&app_theme_review_front_backgrounds\[theme\]\s*;",
        source,
        re.DOTALL,
    ):
        append_error(
            errors,
            path,
            "review front must load separate legend layer backgrounds",
        )
    if re.search(
        r"kind\s*==\s*APP_THEME_BACKGROUND_REVIEW_FRONT\s*&&\s*"
        r"app_read_theme_background_pair\s*\(\s*&app_theme_backgrounds\[theme\]\s*\)",
        source,
        re.DOTALL,
    ):
        append_error(
            errors,
            path,
            "review front must not fall back to baked final theme backgrounds",
        )
    contract_build_match = re.search(
        r"app_review_front_contract_build\s*\(\s*&?contract\s*,",
        main_source,
    )
    renderer_call_match = re.search(
        r"app_fe_renderer_draw_review_front_overlay\s*\(\s*&contract\s*\)\s*;",
        main_source,
    )
    if contract_build_match is None:
        append_error(
            errors,
            path,
            "FE review front overlay must build the strings-only contract",
        )
    if renderer_call_match is None:
        append_error(
            errors,
            path,
            "FE review front overlay must draw the strings-only contract",
        )
    if (
        contract_build_match is not None and
        renderer_call_match is not None and
        contract_build_match.start() > renderer_call_match.start()
    ):
        append_error(
            errors,
            path,
            "FE review front overlay must build contract before renderer draw",
        )
    if "fe_font_review_8x14_alpha.bin" not in source:
        append_error(errors, path, "FE review front must load the generated font atlas")
    if (
        "app_load_fe_font_atlas" not in source and
        "app_fe_load_font_atlas" not in source
    ):
        append_error(errors, path, "FE review front must fall back when font atlas is missing")

    if "enum app_theme theme = APP_THEME_PLAIN;" not in source:
        append_error(errors, path, "app_theme_visual must fall back to Plain")

    if not (
        re.search(
            r"static\s+enum\s+app_theme\s+app_sanitized_theme\s*"
            r"\([^)]*\)\s*\{.*?return\s+APP_THEME_PLAIN\s*;",
            source,
            re.DOTALL,
        ) or
        re.search(
            r"enum\s+app_theme\s+app_display_sanitized_theme\s*"
            r"\([^)]*\)\s*\{.*?return\s+APP_THEME_PLAIN\s*;",
            source,
            re.DOTALL,
        )
    ):
        append_error(errors, path, "invalid themes must sanitize to Plain")

    if not re.search(
        r"paths->top_path\s*==\s*NULL\s*\|\|\s*"
        r"paths->bottom_path\s*==\s*NULL",
        source,
        re.DOTALL,
    ):
        append_error(errors, path, "Plain/null theme must skip framebuffer loading")

    initial_theme_body = extract_initial_theme_body(source)
    if initial_theme_body is None:
        append_error(errors, path, "missing app_initial_theme startup selector")
    elif not initial_theme_starts_plain(initial_theme_body):
        append_error(
            errors,
            path,
            "startup theme must be Plain; FE themes stay opt-in",
        )

    if not re.search(r"app->theme\s*=\s*app_initial_theme\s*\(\s*\)\s*;", source):
        append_error(errors, path, "app_init must use app_initial_theme")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "source",
        nargs="?",
        type=Path,
        default=DEFAULT_MAIN_C,
        help="Path to app-3ds/source/main.c.",
    )
    args = parser.parse_args()

    errors = verify_app_theme_source(args.source)
    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1

    print(f"{args.source}: ok - app theme/rendering invariants verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
