#ifndef ANKI3DS_APP_SCREEN_MODEL_H
#define ANKI3DS_APP_SCREEN_MODEL_H

#include <stdbool.h>
#include <stddef.h>

#include "app_confirm_contract.h"
#include "app_deck_select_contract.h"
#include "app_flashcard_contract.h"
#include "app_settings_contract.h"

struct study_backend;
struct study_deck_index;
struct study_settings;

enum app_screen_model_kind
{
	APP_SCREEN_MODEL_DECK_SELECT,
	APP_SCREEN_MODEL_REVIEW,
	APP_SCREEN_MODEL_SETTINGS,
	APP_SCREEN_MODEL_CONFIRM,
};

/*
 * Renderer-facing aggregate. It intentionally carries only string contracts
 * and transient scroll state; coordinates, colors, textures, and fonts stay in
 * the renderer.
 */
struct app_screen_model
{
	enum app_screen_model_kind kind;
	size_t front_scroll_offset;
	size_t answer_scroll_offset;
	struct app_flashcard_text_view flashcard;
	struct app_deck_select_contract deck_select;
	struct app_settings_contract settings;
	struct app_confirm_contract confirm;
};

void app_screen_model_build(
	struct app_screen_model *model,
	enum app_screen_model_kind kind,
	enum app_confirm_contract_kind confirm_kind,
	const struct study_deck_index *deck_index,
	size_t selected_deck_index,
	const struct study_backend *backend,
	const struct study_settings *settings,
	const struct study_settings *draft_settings,
	size_t selected_setting_index,
	size_t front_scroll_offset,
	size_t answer_scroll_offset,
	bool help_visible
);

#endif
