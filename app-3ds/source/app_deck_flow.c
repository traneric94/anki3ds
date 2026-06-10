#include "app_deck_flow.h"

#include "app_deck_select_action.h"
#include "app_deck_select_contract.h"
#include "app_learning_policy.h"
#include "app_state_save.h"

#include <stdio.h>

static struct study_backend_deck app_deck_flow_loaded_deck;
static struct study_backend app_deck_flow_loaded_backend_snapshot;

static void app_deck_flow_copy_string(
	char *destination,
	size_t destination_size,
	const char *source
)
{
	if (destination == NULL || destination_size == 0)
		return;

	snprintf(destination, destination_size, "%s", source != NULL ? source : "");
}

static void app_deck_flow_clear_paths(const struct app_deck_flow_paths *paths)
{
	if (paths == NULL)
		return;

	app_deck_flow_copy_string(paths->state_path, paths->state_path_size, "");
	app_deck_flow_copy_string(paths->settings_path, paths->settings_path_size, "");
	app_deck_flow_copy_string(
		paths->review_log_path,
		paths->review_log_path_size,
		""
	);
	app_deck_flow_copy_string(paths->deck_id, paths->deck_id_size, "");
}

static void app_deck_flow_copy_entry_paths(
	const struct app_deck_flow_paths *paths,
	const struct study_deck_entry *entry
)
{
	if (paths == NULL || entry == NULL)
		return;

	app_deck_flow_copy_string(
		paths->state_path,
		paths->state_path_size,
		entry->state_path
	);
	app_deck_flow_copy_string(
		paths->settings_path,
		paths->settings_path_size,
		entry->settings_path
	);
	app_deck_flow_copy_string(
		paths->review_log_path,
		paths->review_log_path_size,
		entry->review_log_path
	);
}

static void app_deck_flow_init_empty_backend(
	struct study_backend *backend,
	const char *status
)
{
	study_backend_init(backend, NULL, 0);
	study_backend_set_status(backend, status);
}

static void app_deck_flow_set_scan_status(
	struct study_backend *backend,
	const struct study_deck_index *index
)
{
	char status[STUDY_BACKEND_STATUS_SIZE];

	if (backend == NULL || index == NULL)
		return;

	app_deck_select_contract_build_scan_status(index, status, sizeof(status));
	study_backend_set_status(backend, status);
}

static void app_deck_flow_record_scan(
	struct study_session *session,
	bool *session_dirty,
	const struct study_deck_index *deck_index,
	unsigned long timestamp,
	unsigned int day
)
{
	if (session == NULL || deck_index == NULL)
		return;

	study_session_record_scan(
		session,
		deck_index->count,
		deck_index->ignored_count,
		timestamp,
		day
	);
	if (session_dirty != NULL)
		*session_dirty = true;
}

void app_deck_flow_rescan(
	struct study_deck_index *index,
	size_t *selected_deck_index,
	struct study_backend *backend,
	const char *root_path,
	unsigned int current_day
)
{
	char previous_id[STUDY_DECK_INDEX_ID_SIZE] = "";
	const struct study_deck_entry *selected_entry;

	if (index == NULL || selected_deck_index == NULL)
		return;

	selected_entry = study_deck_index_get(index, *selected_deck_index);
	if (selected_entry != NULL)
	{
		app_deck_flow_copy_string(
			previous_id,
			sizeof(previous_id),
			selected_entry->id
		);
	}

	study_deck_index_scan_for_day(index, root_path, current_day);
	if (
		previous_id[0] == '\0' ||
		!study_deck_index_find(index, previous_id, selected_deck_index)
	)
	{
		if (*selected_deck_index >= index->count)
			*selected_deck_index = index->count > 0 ? index->count - 1 : 0;
	}
	app_deck_flow_set_scan_status(backend, index);
}

void app_deck_flow_startup_scan(
	const char *root_path,
	struct study_deck_index *deck_index,
	size_t *selected_deck_index,
	struct study_backend *backend,
	struct study_session *session,
	bool *session_dirty,
	unsigned long timestamp,
	unsigned int day
)
{
	if (backend != NULL)
		app_deck_flow_init_empty_backend(backend, "Scanning decks");
	if (deck_index == NULL || selected_deck_index == NULL)
		return;

	app_deck_flow_rescan(
		deck_index,
		selected_deck_index,
		backend,
		root_path,
		day
	);
	app_deck_flow_record_scan(
		session,
		session_dirty,
		deck_index,
		timestamp,
		day
	);
}

static bool app_deck_flow_load_deck(
	struct study_backend *backend,
	const struct study_deck_entry *entry,
	struct study_settings *settings,
	unsigned int current_day,
	const struct app_deck_flow_paths *paths,
	bool *state_enabled,
	bool *state_dirty_on_load,
	bool battery_save_warning,
	unsigned long timestamp
)
{
	enum study_backend_load_result load_result;
	enum study_settings_result settings_result;
	char status[STUDY_BACKEND_STATUS_SIZE];
	bool rollover_save_failed = false;

	if (state_enabled != NULL)
		*state_enabled = false;
	app_deck_flow_clear_paths(paths);
	if (state_dirty_on_load != NULL)
		*state_dirty_on_load = false;
	if (settings != NULL)
		study_settings_defaults(settings);
	if (backend == NULL || entry == NULL)
		return false;

	load_result = study_backend_load_cards_tsv(
		&app_deck_flow_loaded_deck,
		entry->cards_path
	);
	if (load_result == STUDY_BACKEND_LOAD_OK)
	{
		study_backend_init(
			backend,
			app_deck_flow_loaded_deck.cards,
			app_deck_flow_loaded_deck.card_count
		);
		study_backend_set_status(backend, app_deck_flow_loaded_deck.status_text);
		(void)study_backend_load_state_tsv(backend, entry->state_path);
		if (state_dirty_on_load != NULL)
		{
			bool state_dirty;

			app_deck_flow_loaded_backend_snapshot = *backend;
			state_dirty = study_backend_rollover_day(backend, current_day);
			*state_dirty_on_load = false;
			if (state_dirty)
			{
				enum app_state_save_outcome save_outcome =
					APP_STATE_SAVE_OUTCOME_NONE;

				(void)app_state_save_if_dirty_with_outcome(
					backend,
					true,
					true,
					entry->state_path,
					entry->review_log_path,
					false,
					STUDY_REVIEW_LOG_EVENT_RATING,
					STUDY_REVIEW_LOG_RATING_NONE,
					NULL,
					entry->id,
					NULL,
					battery_save_warning,
					timestamp,
					current_day,
					&save_outcome
				);
				if (save_outcome == APP_STATE_SAVE_OUTCOME_WRITE_FAILED)
				{
					*backend = app_deck_flow_loaded_backend_snapshot;
					study_backend_set_status(backend, "Save failed");
					rollover_save_failed = true;
				}
			}
		}
		else
			(void)study_backend_rollover_day(backend, current_day);
		settings_result = study_settings_load_tsv(settings, entry->settings_path);
		app_learning_policy_apply_to_backend(backend, settings);
		if (settings_result == STUDY_SETTINGS_BAD_FORMAT && !rollover_save_failed)
			study_backend_set_status(backend, "Settings ignored");
		app_deck_flow_copy_entry_paths(paths, entry);
		if (state_enabled != NULL)
			*state_enabled = true;
		return true;
	}

	app_deck_flow_init_empty_backend(backend, "Deck load failed");
	snprintf(
		status,
		sizeof(status),
		"%s: %s",
		entry->display_name,
		study_backend_load_result_name(load_result)
	);
	study_backend_set_status(backend, status);
	return false;
}

bool app_deck_flow_handle_select_input(
	unsigned int buttons,
	const char *root_path,
	struct study_deck_index *deck_index,
	size_t *selected_deck_index,
	struct study_backend *backend,
	struct study_settings *settings,
	unsigned int current_day,
	const struct app_deck_flow_paths *paths,
	struct study_session *session,
	bool *session_dirty,
	bool *state_enabled,
	bool *state_dirty,
	enum app_mode *mode,
	enum app_mode *exit_return_mode,
	bool battery_save_warning,
	unsigned long timestamp,
	unsigned int day
)
{
	const struct study_deck_entry *entry;
	bool deck_loaded;
	enum app_deck_select_action_result deck_select_action;

	if (state_dirty != NULL)
		*state_dirty = false;
	if (
		deck_index == NULL ||
		selected_deck_index == NULL ||
		backend == NULL ||
		state_enabled == NULL ||
		mode == NULL ||
		exit_return_mode == NULL
	)
	{
		return false;
	}

	deck_select_action = app_deck_select_action_apply(
		buttons,
		deck_index->count,
		selected_deck_index
	);
	if (deck_select_action == APP_DECK_SELECT_ACTION_NONE)
		return false;
	if (deck_select_action == APP_DECK_SELECT_ACTION_EXIT)
	{
		*exit_return_mode = APP_MODE_DECK_SELECT;
		*mode = APP_MODE_CONFIRM_EXIT;
		return true;
	}
	if (deck_select_action == APP_DECK_SELECT_ACTION_RESCAN)
	{
		app_deck_flow_rescan(
			deck_index,
			selected_deck_index,
			backend,
			root_path,
			day
		);
		app_deck_flow_record_scan(
			session,
			session_dirty,
			deck_index,
			timestamp,
			day
		);
		return true;
	}
	if (deck_select_action == APP_DECK_SELECT_ACTION_UPDATED)
		return true;

	entry = study_deck_index_get(deck_index, *selected_deck_index);
	if (entry == NULL)
		return false;

	deck_loaded = app_deck_flow_load_deck(
		backend,
		entry,
		settings,
		current_day,
		paths,
		state_enabled,
		state_dirty,
		battery_save_warning,
		timestamp
	);
	if (paths != NULL)
	{
		app_deck_flow_copy_string(
			paths->deck_id,
			paths->deck_id_size,
			entry->id
		);
	}
	if (session != NULL)
	{
		struct study_backend_view view;

		study_backend_build_view(backend, &view);
		study_session_record_deck_open(
			session,
			entry->id,
			deck_loaded,
			view.has_active_card,
			timestamp,
			day
		);
		if (session_dirty != NULL)
			*session_dirty = true;
	}
	*mode = APP_MODE_REVIEW;
	return true;
}
