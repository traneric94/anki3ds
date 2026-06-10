#ifndef ANKI3DS_APP_FLASHCARD_CONTRACT_H
#define ANKI3DS_APP_FLASHCARD_CONTRACT_H

#include <stdbool.h>
#include <stddef.h>

#include "study_backend.h"

struct study_settings;

#define APP_FLASHCARD_META_TEXT_SIZE 128
#define APP_FLASHCARD_TITLE_TEXT_SIZE 32
#define APP_FLASHCARD_BODY_TEXT_SIZE STUDY_BACKEND_CARD_TEXT_SIZE
#define APP_FLASHCARD_STATUS_TEXT_SIZE STUDY_BACKEND_STATUS_SIZE
#define APP_FLASHCARD_CONTROLS_TEXT_SIZE 192
#define APP_FLASHCARD_FOOTER_TEXT_SIZE 96
#define APP_FLASHCARD_NO_ANSWER_TEXT "(No answer)"

/*
 * Pure backend-to-renderer text handoff. This model intentionally contains no
 * screen dimensions, coordinates, colors, render handles, or drawing choices.
 */
struct app_flashcard_text_view
{
	bool has_active_card;
	bool answer_visible;
	bool undo_available;
	bool help_visible;
	char title_text[APP_FLASHCARD_TITLE_TEXT_SIZE];
	char front_text[APP_FLASHCARD_BODY_TEXT_SIZE];
	char back_text[APP_FLASHCARD_BODY_TEXT_SIZE];
	char tags_text[STUDY_BACKEND_TAGS_TEXT_SIZE];
	char display_text[APP_FLASHCARD_BODY_TEXT_SIZE];
	char progress_text[APP_FLASHCARD_META_TEXT_SIZE];
	char status_text[APP_FLASHCARD_STATUS_TEXT_SIZE];
	char help_text[APP_FLASHCARD_CONTROLS_TEXT_SIZE];
	char footer_text[APP_FLASHCARD_FOOTER_TEXT_SIZE];
};

void app_flashcard_text_view_build(
	struct app_flashcard_text_view *text_view,
	const struct study_backend_view *view,
	const struct study_settings *settings,
	bool help_visible
);

#endif
