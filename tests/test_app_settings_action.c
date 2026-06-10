#include "app_settings_action.h"

#include "study_controls.h"

#include <assert.h>
#include <string.h>

static void test_settings_action_rejects_chords(void)
{
	struct study_settings draft;
	size_t selected_index = 1;

	study_settings_defaults(&draft);
	draft.new_limit = 10;
	draft.review_limit = 20;
	assert(
		app_settings_action_apply(
			STUDY_CONTROL_BUTTON_LEFT | STUDY_CONTROL_BUTTON_X,
			&draft,
			&selected_index
		) == APP_SETTINGS_ACTION_NONE
	);
	assert(selected_index == 1);
	assert(draft.new_limit == 10);
	assert(draft.review_limit == 20);
}

static void test_settings_action_ignores_unknown_button_without_side_effects(void)
{
	struct study_settings draft;
	size_t selected_index = 99;

	study_settings_defaults(&draft);
	draft.new_limit = 10;
	draft.review_limit = 20;
	assert(
		app_settings_action_apply(
			STUDY_CONTROL_BUTTON_A,
			&draft,
			&selected_index
		) == APP_SETTINGS_ACTION_NONE
	);
	assert(selected_index == 99);
	assert(draft.new_limit == 10);
	assert(draft.review_limit == 20);
}

static void test_settings_action_moves_selected_field(void)
{
	struct study_settings draft;
	size_t selected_index = 99;

	study_settings_defaults(&draft);
	assert(
		app_settings_action_apply(
			STUDY_CONTROL_BUTTON_DOWN,
			&draft,
			&selected_index
		) == APP_SETTINGS_ACTION_UPDATED
	);
	assert(selected_index == 1);
	assert(
		app_settings_action_apply(
			STUDY_CONTROL_BUTTON_UP,
			&draft,
			&selected_index
		) == APP_SETTINGS_ACTION_UPDATED
	);
	assert(selected_index == 0);
	assert(
		app_settings_action_apply(
			STUDY_CONTROL_BUTTON_UP,
			&draft,
			&selected_index
		) == APP_SETTINGS_ACTION_UPDATED
	);
	assert(selected_index == 2);
}

static void test_settings_action_changes_selected_limit(void)
{
	struct study_settings draft;
	size_t selected_index = 0;

	study_settings_defaults(&draft);
	draft.new_limit = 10;
	draft.review_limit = 20;
	assert(
		app_settings_action_apply(
			STUDY_CONTROL_BUTTON_RIGHT,
			&draft,
			&selected_index
		) == APP_SETTINGS_ACTION_UPDATED
	);
	assert(draft.new_limit == 20);
	assert(draft.review_limit == 20);

	selected_index = 1;
	assert(
		app_settings_action_apply(
			STUDY_CONTROL_BUTTON_LEFT,
			&draft,
			&selected_index
		) == APP_SETTINGS_ACTION_UPDATED
	);
	assert(draft.new_limit == 20);
	assert(draft.review_limit == 10);
}

static void test_settings_action_changes_learning_mode(void)
{
	struct study_settings draft;
	size_t selected_index = 2;

	study_settings_defaults(&draft);
	assert(draft.learning_mode == STUDY_SETTINGS_LEARNING_DUE_FIRST);
	assert(
		app_settings_action_apply(
			STUDY_CONTROL_BUTTON_RIGHT,
			&draft,
			&selected_index
		) == APP_SETTINGS_ACTION_UPDATED
	);
	assert(draft.learning_mode == STUDY_SETTINGS_LEARNING_CARD_COOLDOWN);
	assert(
		app_settings_action_apply(
			STUDY_CONTROL_BUTTON_LEFT,
			&draft,
			&selected_index
		) == APP_SETTINGS_ACTION_UPDATED
	);
	assert(draft.learning_mode == STUDY_SETTINGS_LEARNING_DUE_FIRST);
}

static void test_settings_action_reports_commands(void)
{
	struct study_settings draft;
	size_t selected_index = 99;

	study_settings_defaults(&draft);
	assert(
		app_settings_action_apply(
			STUDY_CONTROL_BUTTON_START,
			&draft,
			&selected_index
		) == APP_SETTINGS_ACTION_EXIT
	);
	assert(
		app_settings_action_apply(
			STUDY_CONTROL_BUTTON_B,
			&draft,
			&selected_index
		) == APP_SETTINGS_ACTION_CANCEL
	);
	assert(
		app_settings_action_apply(
			STUDY_CONTROL_BUTTON_SELECT,
			&draft,
			&selected_index
		) == APP_SETTINGS_ACTION_CANCEL
	);
	assert(
		app_settings_action_apply(
			STUDY_CONTROL_BUTTON_X,
			&draft,
			&selected_index
		) == APP_SETTINGS_ACTION_SAVE
	);
	assert(selected_index == 99);
	assert(draft.new_limit == STUDY_SETTINGS_DEFAULT_NEW_LIMIT);
	assert(draft.review_limit == STUDY_SETTINGS_DEFAULT_REVIEW_LIMIT);
	assert(draft.learning_mode == STUDY_SETTINGS_DEFAULT_LEARNING_MODE);
}

static void test_settings_action_status_tracks_unsaved_changes(void)
{
	struct study_settings active;
	struct study_settings draft;

	study_settings_defaults(&active);
	draft = active;
	assert(!app_settings_action_has_unsaved_changes(&active, &draft));
	assert(
		strcmp(
			app_settings_action_edit_status(&active, &draft),
			"no changes"
		) == 0
	);

	draft.review_limit = 50;
	assert(app_settings_action_has_unsaved_changes(&active, &draft));
	assert(
		strcmp(
			app_settings_action_edit_status(&active, &draft),
			"unsaved changes"
		) == 0
	);
	draft = active;
	draft.learning_mode = STUDY_SETTINGS_LEARNING_CARD_COOLDOWN;
	assert(app_settings_action_has_unsaved_changes(&active, &draft));
}

static void test_settings_action_status_defaults_null_settings(void)
{
	struct study_settings draft;

	study_settings_defaults(&draft);
	assert(!app_settings_action_has_unsaved_changes(NULL, &draft));
	draft.new_limit = 5;
	assert(app_settings_action_has_unsaved_changes(NULL, &draft));
}

int main(void)
{
	test_settings_action_rejects_chords();
	test_settings_action_ignores_unknown_button_without_side_effects();
	test_settings_action_moves_selected_field();
	test_settings_action_changes_selected_limit();
	test_settings_action_changes_learning_mode();
	test_settings_action_reports_commands();
	test_settings_action_status_tracks_unsaved_changes();
	test_settings_action_status_defaults_null_settings();
	return 0;
}
