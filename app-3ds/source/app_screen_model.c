#include "app_screen_model.h"

#include "study_backend.h"
#include "study_deck_index.h"
#include "study_settings.h"

#include <string.h>

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
)
{
	struct study_backend_view view;
	const char *status_text;

	if (model == NULL)
		return;

	memset(model, 0, sizeof(*model));
	model->kind = kind;
	model->front_scroll_offset = front_scroll_offset;
	model->answer_scroll_offset = answer_scroll_offset;

	study_backend_build_view(backend, &view);
	status_text = backend != NULL ? backend->status_text : "";
	app_flashcard_text_view_build(&model->flashcard, &view, settings, help_visible);
	app_deck_select_contract_build(
		&model->deck_select,
		deck_index,
		selected_deck_index,
		status_text,
		help_visible
	);
	app_settings_contract_build(
		&model->settings,
		draft_settings,
		selected_setting_index,
		status_text
	);
	app_confirm_contract_build(
		&model->confirm,
		confirm_kind,
		view.status_text,
		view.suspended_count
	);
}
