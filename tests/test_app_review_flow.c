#include "app_review_flow.h"

#include "study_backend.h"
#include "study_controls.h"
#include "study_session.h"
#include "study_settings.h"

#include <assert.h>
#include <string.h>

static const struct study_backend_card cards[] = {
	{ "Front 1", "Back 1", NULL },
	{ "Front 2", "Back 2", NULL },
};

struct flow_state
{
	struct study_backend backend;
	struct study_settings active_settings;
	struct study_settings draft_settings;
	struct study_session session;
	size_t selected_setting_index;
	size_t scroll_offset;
	bool help_visible;
	enum app_mode mode;
	enum app_mode exit_return_mode;
	bool session_dirty;
	bool state_dirty;
	bool log_dirty;
	enum study_review_log_event log_event;
	enum study_review_log_rating log_rating;
	bool exit_requested;
};

static void init_flow_state(struct flow_state *flow)
{
	study_backend_init(&flow->backend, cards, 2);
	study_settings_defaults(&flow->active_settings);
	study_settings_defaults(&flow->draft_settings);
	study_session_init(&flow->session, 10, 1);
	flow->selected_setting_index = 1;
	flow->scroll_offset = 2;
	flow->help_visible = false;
	flow->mode = APP_MODE_REVIEW;
	flow->exit_return_mode = APP_MODE_DECK_SELECT;
	flow->session_dirty = false;
	flow->state_dirty = true;
	flow->log_dirty = true;
	flow->log_event = STUDY_REVIEW_LOG_EVENT_RESTORE;
	flow->log_rating = STUDY_REVIEW_LOG_RATING_EASY;
	flow->exit_requested = false;
}

static bool apply_flow(
	struct flow_state *flow,
	unsigned int buttons,
	const char *settings_path
)
{
	return app_review_flow_handle_input(
		buttons,
		&flow->backend,
		&flow->active_settings,
		&flow->draft_settings,
		settings_path,
		&flow->selected_setting_index,
		4,
		&flow->scroll_offset,
		&flow->help_visible,
		&flow->mode,
		&flow->exit_return_mode,
		&flow->session,
		"alpha",
		&flow->session_dirty,
		&flow->state_dirty,
		&flow->log_dirty,
		&flow->log_event,
		&flow->log_rating,
		&flow->exit_requested,
		100,
		2
	);
}

static void test_start_toggles_help_in_review(void)
{
	struct flow_state flow;
	struct study_backend_view view;

	init_flow_state(&flow);
	flow.exit_requested = true;

	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_START, "settings.tsv"));
	assert(flow.help_visible);
	assert(!flow.state_dirty);
	assert(!flow.log_dirty);
	assert(!flow.exit_requested);
	assert(flow.mode == APP_MODE_REVIEW);

	assert(study_backend_show_answer(&flow.backend));
	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_START, "settings.tsv"));
	assert(!flow.help_visible);
	assert(!flow.state_dirty);
	assert(!flow.log_dirty);

	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_L, "settings.tsv"));
	study_backend_build_view(&flow.backend, &view);
	assert(flow.state_dirty);
	assert(flow.log_dirty);
	assert(flow.log_event == STUDY_REVIEW_LOG_EVENT_RATING);
	assert(flow.log_rating == STUDY_REVIEW_LOG_RATING_HARD);
	assert(view.hard_count == 1);
}

static void test_open_settings_copies_draft_and_preserves_warning(void)
{
	struct flow_state flow;

	init_flow_state(&flow);
	flow.active_settings.new_limit = 5;
	flow.active_settings.review_limit = 10;
	flow.draft_settings.new_limit = 20;
	flow.draft_settings.review_limit = 200;
	flow.exit_requested = true;
	study_backend_set_status(&flow.backend, "Settings ignored");

	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_X, "settings.tsv"));
	assert(flow.mode == APP_MODE_SETTINGS);
	assert(flow.selected_setting_index == 0);
	assert(flow.draft_settings.new_limit == 5);
	assert(flow.draft_settings.review_limit == 10);
	assert(strcmp(flow.backend.status_text, "no changes; Settings ignored") == 0);
	assert(!flow.state_dirty);
	assert(!flow.log_dirty);
	assert(!flow.exit_requested);
}

static void test_open_settings_without_path_reports_unavailable(void)
{
	struct flow_state flow;

	init_flow_state(&flow);
	study_backend_set_status(&flow.backend, "Settings ignored");

	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_X, ""));
	assert(flow.mode == APP_MODE_REVIEW);
	assert(
		strcmp(
			flow.backend.status_text,
			"Settings unavailable; Settings ignored"
		) == 0
	);
	assert(!flow.state_dirty);
	assert(!flow.log_dirty);
}

static void test_review_mode_transitions_and_unavailable_statuses(void)
{
	struct flow_state flow;

	init_flow_state(&flow);
	flow.exit_requested = true;
	study_backend_set_status(&flow.backend, "Settings ignored");
	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_B, "settings.tsv"));
	assert(strcmp(flow.backend.status_text, "Nothing to undo; Settings ignored") == 0);
	assert(!flow.exit_requested);

	init_flow_state(&flow);
	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_Y, "settings.tsv"));
	assert(flow.mode == APP_MODE_CONFIRM_RESET);

	init_flow_state(&flow);
	flow.exit_requested = true;
	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_SELECT, "settings.tsv"));
	assert(flow.mode == APP_MODE_DECK_SELECT);
	assert(flow.scroll_offset == 0);
	assert(!flow.exit_requested);

	init_flow_state(&flow);
	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_START, "settings.tsv"));
	assert(flow.mode == APP_MODE_REVIEW);
	assert(flow.help_visible);
}

static void test_suspend_restore_targets(void)
{
	struct flow_state flow;

	init_flow_state(&flow);
	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_R, "settings.tsv"));
	assert(flow.mode == APP_MODE_CONFIRM_SUSPEND);

	init_flow_state(&flow);
	assert(study_backend_show_answer(&flow.backend));
	assert(study_backend_rate_current(&flow.backend, STUDY_BACKEND_RATING_GOOD));
	assert(study_backend_show_answer(&flow.backend));
	assert(study_backend_rate_current(&flow.backend, STUDY_BACKEND_RATING_GOOD));
	study_backend_set_status(&flow.backend, "Settings ignored");
	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_R, "settings.tsv"));
	assert(flow.mode == APP_MODE_REVIEW);
	assert(strcmp(flow.backend.status_text, "Nothing suspended; Settings ignored") == 0);

	init_flow_state(&flow);
	assert(study_backend_suspend_current(&flow.backend));
	assert(study_backend_show_answer(&flow.backend));
	assert(study_backend_rate_current(&flow.backend, STUDY_BACKEND_RATING_GOOD));
	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_R, "settings.tsv"));
	assert(flow.mode == APP_MODE_CONFIRM_RESTORE);
}

static void test_a_restarts_completed_review_without_resetting_progress(void)
{
	struct flow_state flow;
	struct study_backend_view view;

	init_flow_state(&flow);
	assert(study_backend_show_answer(&flow.backend));
	assert(study_backend_rate_current(&flow.backend, STUDY_BACKEND_RATING_GOOD));
	assert(study_backend_show_answer(&flow.backend));
	assert(study_backend_rate_current(&flow.backend, STUDY_BACKEND_RATING_EASY));
	study_backend_set_status(&flow.backend, "Settings ignored");
	flow.exit_requested = true;

	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_A, "settings.tsv"));
	study_backend_build_view(&flow.backend, &view);

	assert(flow.mode == APP_MODE_REVIEW);
	assert(!flow.exit_requested);
	assert(flow.state_dirty);
	assert(!flow.log_dirty);
	assert(!flow.session_dirty);
	assert(view.has_active_card);
	assert(view.card_index == 0);
	assert(view.reviewed_count == 2);
	assert(view.reviewed_today_count == 0);
	assert(
		strcmp(
			flow.backend.status_text,
			"Review again; card 1/2; Settings ignored"
		) == 0
	);
}

static void test_daily_limit_blocks_preserve_warning_context(void)
{
	struct flow_state flow;

	init_flow_state(&flow);
	assert(study_backend_show_answer(&flow.backend));
	assert(study_backend_rate_current(&flow.backend, STUDY_BACKEND_RATING_GOOD));
	flow.active_settings.new_limit = 1;
	study_backend_set_status(&flow.backend, "Settings ignored");
	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_A, "settings.tsv"));
	assert(strcmp(flow.backend.status_text, "New limit reached; Settings ignored") == 0);
	assert(!flow.state_dirty);
	assert(!flow.log_dirty);

	init_flow_state(&flow);
	assert(study_backend_show_answer(&flow.backend));
	assert(study_backend_rate_current(&flow.backend, STUDY_BACKEND_RATING_GOOD));
	assert(study_backend_show_answer(&flow.backend));
	flow.active_settings.review_limit = 1;
	study_backend_set_status(&flow.backend, "Settings ignored");
	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_A, "settings.tsv"));
	assert(strcmp(flow.backend.status_text, "Review limit reached; Settings ignored") == 0);
	assert(!flow.state_dirty);
	assert(!flow.log_dirty);
}

static void test_reveal_records_session_and_state_dirty(void)
{
	struct flow_state flow;

	init_flow_state(&flow);
	flow.help_visible = true;
	study_backend_set_status(&flow.backend, "Settings ignored");

	assert(apply_flow(&flow, STUDY_CONTROL_BUTTON_A, "settings.tsv"));
	assert(flow.state_dirty);
	assert(!flow.log_dirty);
	assert(flow.session_dirty);
	assert(flow.session.answer_shown_count == 1);
	assert(flow.session.updated_at == 100);
	assert(flow.session.current_day == 2);
	assert(strcmp(flow.session.last_deck_id, "alpha") == 0);
	assert(flow.session.last_event == STUDY_SESSION_EVENT_ANSWER_SHOWN);
	assert(!flow.help_visible);
	assert(strstr(flow.backend.status_text, "Answer") != NULL);
	assert(strstr(flow.backend.status_text, "Settings ignored") != NULL);
}

int main(void)
{
	test_start_toggles_help_in_review();
	test_open_settings_copies_draft_and_preserves_warning();
	test_open_settings_without_path_reports_unavailable();
	test_review_mode_transitions_and_unavailable_statuses();
	test_suspend_restore_targets();
	test_a_restarts_completed_review_without_resetting_progress();
	test_daily_limit_blocks_preserve_warning_context();
	test_reveal_records_session_and_state_dirty();
	return 0;
}
