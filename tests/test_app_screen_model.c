#include "app_screen_model.h"
#include "study_backend.h"
#include "study_deck_index.h"
#include "study_settings.h"

#include <assert.h>
#include <string.h>

static void test_review_model_aggregates_flashcard_strings(void)
{
	struct study_backend_card cards[] = {
		{"Where is Caelin?", "Lycia", NULL}
	};
	struct study_backend backend;
	struct study_settings settings;
	struct app_screen_model model;

	study_settings_defaults(&settings);
	settings.new_limit = 2;
	settings.review_limit = 5;
	study_backend_init(&backend, cards, 1);
	study_backend_set_status(&backend, "Ready");

	app_screen_model_build(
		&model,
		APP_SCREEN_MODEL_REVIEW,
		APP_CONFIRM_CONTRACT_SUSPEND,
		NULL,
		0,
		&backend,
		&settings,
		&settings,
		0,
		3,
		4,
		false
	);

	assert(model.kind == APP_SCREEN_MODEL_REVIEW);
	assert(model.front_scroll_offset == 3);
	assert(model.answer_scroll_offset == 4);
	assert(strcmp(model.flashcard.title_text, "Question") == 0);
	assert(strcmp(model.flashcard.display_text, "Where is Caelin?") == 0);
	assert(strcmp(model.flashcard.status_text, "Ready") == 0);
	assert(strstr(model.flashcard.footer_text, "Today New 0/2") != NULL);
}

static void test_deck_select_model_uses_backend_status_and_index_strings(void)
{
	struct study_deck_index index;
	struct study_backend backend;
	struct app_screen_model model;

	study_deck_index_init(&index);
	index.count = 2;
	index.ignored_count = 1;
	strcpy(index.entries[0].display_name, "Alpha Deck");
	strcpy(index.entries[1].display_name, "Beta Deck");
	study_backend_init(&backend, NULL, 0);
	study_backend_set_status(&backend, "Scan done");

	app_screen_model_build(
		&model,
		APP_SCREEN_MODEL_DECK_SELECT,
		APP_CONFIRM_CONTRACT_EXIT,
		&index,
		1,
		&backend,
		NULL,
		NULL,
		0,
		0,
		0,
		false
	);

	assert(model.kind == APP_SCREEN_MODEL_DECK_SELECT);
	assert(strcmp(model.deck_select.title_text, "Select deck") == 0);
	assert(strstr(model.deck_select.list_text, "  Alpha Deck") != NULL);
	assert(strstr(model.deck_select.list_text, "> Beta Deck") != NULL);
	assert(strstr(model.deck_select.meta_text, "Deck 2/2") != NULL);
	assert(strcmp(model.deck_select.status_text, "Scan done") == 0);
}

static void test_settings_model_uses_draft_settings(void)
{
	struct study_backend backend;
	struct study_settings active;
	struct study_settings draft;
	struct app_screen_model model;

	study_settings_defaults(&active);
	study_settings_defaults(&draft);
	draft.new_limit = 1;
	draft.review_limit = 10;
	study_backend_init(&backend, NULL, 0);
	study_backend_set_status(&backend, "unsaved changes");

	app_screen_model_build(
		&model,
		APP_SCREEN_MODEL_SETTINGS,
		APP_CONFIRM_CONTRACT_EXIT,
		NULL,
		0,
		&backend,
		&active,
		&draft,
		1,
		0,
		0,
		false
	);

	assert(model.kind == APP_SCREEN_MODEL_SETTINGS);
	assert(strcmp(model.settings.title_text, "Study settings") == 0);
	assert(strstr(model.settings.body_text, "  New limit: 1") != NULL);
	assert(strstr(model.settings.body_text, "> Review limit: 10") != NULL);
	assert(strcmp(model.settings.status_text, "unsaved changes") == 0);
}

static void test_confirm_model_uses_backend_view_status_and_suspended_count(void)
{
	struct study_backend_card cards[] = {
		{"Front", "Back", NULL},
		{"Other", "Again", NULL}
	};
	struct study_backend backend;
	struct app_screen_model model;

	study_backend_init(&backend, cards, 2);
	assert(study_backend_suspend_current(&backend));
	study_backend_set_status(&backend, "No active cards");

	app_screen_model_build(
		&model,
		APP_SCREEN_MODEL_CONFIRM,
		APP_CONFIRM_CONTRACT_RESTORE,
		NULL,
		0,
		&backend,
		NULL,
		NULL,
		0,
		0,
		0,
		false
	);

	assert(model.kind == APP_SCREEN_MODEL_CONFIRM);
	assert(strcmp(model.confirm.status_text, "No active cards") == 0);
	assert(strstr(model.confirm.prompt_text, "Restore 1 suspended card?") != NULL);
}

int main(void)
{
	test_review_model_aggregates_flashcard_strings();
	test_deck_select_model_uses_backend_status_and_index_strings();
	test_settings_model_uses_draft_settings();
	test_confirm_model_uses_backend_view_status_and_suspended_count();
	return 0;
}
