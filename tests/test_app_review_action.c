#include "app_review_action.h"

#include <assert.h>

static const struct study_backend_card cards[] = {
	{ "Front", "Back", NULL },
	{ "Next", "Next back", NULL },
};

static void test_scroll_actions_clamp_without_dirty_state(void)
{
	struct study_backend backend;
	size_t scroll_offset = 1;
	bool state_dirty = true;
	bool log_dirty = true;
	bool exit_requested = true;
	enum study_review_log_event log_event = STUDY_REVIEW_LOG_EVENT_UNDO;
	enum study_review_log_rating log_rating = STUDY_REVIEW_LOG_RATING_GOOD;

	study_backend_init(&backend, cards, 2);
	assert(app_review_action_apply(
		&backend,
		STUDY_CONTROL_ACTION_SCROLL_DOWN,
		STUDY_CONTROL_RATING_NONE,
		2,
		&scroll_offset,
		&state_dirty,
		&log_dirty,
		&log_event,
		&log_rating,
		&exit_requested
	));
	assert(scroll_offset == 2);
	assert(!state_dirty);
	assert(!log_dirty);
	assert(!exit_requested);
	assert(log_event == STUDY_REVIEW_LOG_EVENT_RATING);
	assert(log_rating == STUDY_REVIEW_LOG_RATING_NONE);

	state_dirty = true;
	log_dirty = true;
	exit_requested = true;
	log_event = STUDY_REVIEW_LOG_EVENT_UNDO;
	log_rating = STUDY_REVIEW_LOG_RATING_GOOD;
	assert(!app_review_action_apply(
		&backend,
		STUDY_CONTROL_ACTION_SCROLL_DOWN,
		STUDY_CONTROL_RATING_NONE,
		2,
		&scroll_offset,
		&state_dirty,
		&log_dirty,
		&log_event,
		&log_rating,
		&exit_requested
	));
	assert(scroll_offset == 2);
	assert(!state_dirty);
	assert(!log_dirty);
	assert(!exit_requested);
	assert(log_event == STUDY_REVIEW_LOG_EVENT_RATING);
	assert(log_rating == STUDY_REVIEW_LOG_RATING_NONE);

	assert(app_review_action_apply(
		&backend,
		STUDY_CONTROL_ACTION_SCROLL_UP,
		STUDY_CONTROL_RATING_NONE,
		2,
		&scroll_offset,
		&state_dirty,
		&log_dirty,
		&log_event,
		&log_rating,
		&exit_requested
	));
	assert(scroll_offset == 1);

	scroll_offset = 0;
	state_dirty = true;
	log_dirty = true;
	exit_requested = true;
	log_event = STUDY_REVIEW_LOG_EVENT_UNDO;
	log_rating = STUDY_REVIEW_LOG_RATING_GOOD;
	assert(!app_review_action_apply(
		&backend,
		STUDY_CONTROL_ACTION_SCROLL_UP,
		STUDY_CONTROL_RATING_NONE,
		2,
		&scroll_offset,
		&state_dirty,
		&log_dirty,
		&log_event,
		&log_rating,
		&exit_requested
	));
	assert(scroll_offset == 0);
	assert(!state_dirty);
	assert(!log_dirty);
	assert(!exit_requested);
	assert(log_event == STUDY_REVIEW_LOG_EVENT_RATING);
	assert(log_rating == STUDY_REVIEW_LOG_RATING_NONE);
}

static void test_reveal_resets_scroll_and_marks_state_dirty(void)
{
	struct study_backend backend;
	size_t scroll_offset = 2;
	bool state_dirty = false;
	bool log_dirty = true;
	bool exit_requested = true;
	enum study_review_log_event log_event = STUDY_REVIEW_LOG_EVENT_UNDO;
	enum study_review_log_rating log_rating = STUDY_REVIEW_LOG_RATING_GOOD;

	study_backend_init(&backend, cards, 2);
	assert(app_review_action_apply(
		&backend,
		STUDY_CONTROL_ACTION_REVEAL,
		STUDY_CONTROL_RATING_NONE,
		4,
		&scroll_offset,
		&state_dirty,
		&log_dirty,
		&log_event,
		&log_rating,
		&exit_requested
	));
	assert(scroll_offset == 0);
	assert(state_dirty);
	assert(!log_dirty);
	assert(!exit_requested);
	assert(log_event == STUDY_REVIEW_LOG_EVENT_RATING);
	assert(log_rating == STUDY_REVIEW_LOG_RATING_NONE);
}

static void test_rating_resets_scroll_and_sets_log_metadata(void)
{
	struct study_backend backend;
	struct study_backend_view view;
	size_t scroll_offset = 3;
	bool state_dirty = false;
	bool log_dirty = false;
	bool exit_requested = false;
	enum study_review_log_event log_event = STUDY_REVIEW_LOG_EVENT_UNDO;
	enum study_review_log_rating log_rating = STUDY_REVIEW_LOG_RATING_NONE;

	study_backend_init(&backend, cards, 2);
	assert(study_backend_show_answer(&backend));
	assert(app_review_action_apply(
		&backend,
		STUDY_CONTROL_ACTION_RATE,
		STUDY_CONTROL_RATING_HARD,
		4,
		&scroll_offset,
		&state_dirty,
		&log_dirty,
		&log_event,
		&log_rating,
		&exit_requested
	));
	study_backend_build_view(&backend, &view);
	assert(scroll_offset == 0);
	assert(state_dirty);
	assert(log_dirty);
	assert(!exit_requested);
	assert(log_event == STUDY_REVIEW_LOG_EVENT_RATING);
	assert(log_rating == STUDY_REVIEW_LOG_RATING_HARD);
	assert(view.reviewed_count == 1);
	assert(view.hard_count == 1);
	assert(view.card_index == 1);
}

static void test_undo_resets_scroll_and_sets_log_metadata(void)
{
	struct study_backend backend;
	struct study_backend_view view;
	size_t scroll_offset = 2;
	bool state_dirty = false;
	bool log_dirty = false;
	bool exit_requested = false;
	enum study_review_log_event log_event = STUDY_REVIEW_LOG_EVENT_RATING;
	enum study_review_log_rating log_rating = STUDY_REVIEW_LOG_RATING_GOOD;

	study_backend_init(&backend, cards, 2);
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	assert(app_review_action_apply(
		&backend,
		STUDY_CONTROL_ACTION_UNDO,
		STUDY_CONTROL_RATING_NONE,
		4,
		&scroll_offset,
		&state_dirty,
		&log_dirty,
		&log_event,
		&log_rating,
		&exit_requested
	));
	study_backend_build_view(&backend, &view);
	assert(scroll_offset == 0);
	assert(state_dirty);
	assert(log_dirty);
	assert(!exit_requested);
	assert(log_event == STUDY_REVIEW_LOG_EVENT_UNDO);
	assert(log_rating == STUDY_REVIEW_LOG_RATING_NONE);
	assert(view.reviewed_count == 0);
	assert(view.card_index == 0);
}

static void test_exit_sets_request_without_handling_action(void)
{
	struct study_backend backend;
	size_t scroll_offset = 2;
	bool state_dirty = true;
	bool log_dirty = true;
	bool exit_requested = false;
	enum study_review_log_event log_event = STUDY_REVIEW_LOG_EVENT_UNDO;
	enum study_review_log_rating log_rating = STUDY_REVIEW_LOG_RATING_GOOD;

	study_backend_init(&backend, cards, 2);
	assert(!app_review_action_apply(
		&backend,
		STUDY_CONTROL_ACTION_EXIT,
		STUDY_CONTROL_RATING_NONE,
		4,
		&scroll_offset,
		&state_dirty,
		&log_dirty,
		&log_event,
		&log_rating,
		&exit_requested
	));
	assert(scroll_offset == 2);
	assert(!state_dirty);
	assert(!log_dirty);
	assert(exit_requested);
	assert(log_event == STUDY_REVIEW_LOG_EVENT_RATING);
	assert(log_rating == STUDY_REVIEW_LOG_RATING_NONE);
}

static void test_invalid_rate_action_is_ignored(void)
{
	struct study_backend backend;
	size_t scroll_offset = 1;
	bool state_dirty = true;
	bool log_dirty = true;
	bool exit_requested = true;

	study_backend_init(&backend, cards, 2);
	assert(!app_review_action_apply(
		&backend,
		STUDY_CONTROL_ACTION_RATE,
		STUDY_CONTROL_RATING_NONE,
		4,
		&scroll_offset,
		&state_dirty,
		&log_dirty,
		NULL,
		NULL,
		&exit_requested
	));
	assert(scroll_offset == 1);
	assert(!state_dirty);
	assert(!log_dirty);
	assert(!exit_requested);
}

static void test_suspend_restore_target_for_review_states(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	assert(
		app_review_action_suspend_restore_target(NULL) ==
		APP_REVIEW_SUSPEND_RESTORE_NONE
	);

	study_backend_init(&backend, NULL, 0);
	study_backend_build_view(&backend, &view);
	assert(
		app_review_action_suspend_restore_target(&view) ==
		APP_REVIEW_SUSPEND_RESTORE_NONE
	);

	study_backend_init(&backend, cards, 2);
	study_backend_build_view(&backend, &view);
	assert(
		app_review_action_suspend_restore_target(&view) ==
		APP_REVIEW_SUSPEND_RESTORE_SUSPEND
	);

	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_EASY));
	study_backend_build_view(&backend, &view);
	assert(
		app_review_action_suspend_restore_target(&view) ==
		APP_REVIEW_SUSPEND_RESTORE_UNAVAILABLE
	);

	study_backend_init(&backend, cards, 2);
	assert(study_backend_suspend_current(&backend));
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	study_backend_build_view(&backend, &view);
	assert(
		app_review_action_suspend_restore_target(&view) ==
		APP_REVIEW_SUSPEND_RESTORE_RESTORE
	);
}

static void test_undo_target_for_review_states(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	assert(
		app_review_action_undo_target(NULL) ==
		APP_REVIEW_UNDO_NONE
	);

	study_backend_init(&backend, NULL, 0);
	study_backend_build_view(&backend, &view);
	assert(
		app_review_action_undo_target(&view) ==
		APP_REVIEW_UNDO_NONE
	);

	study_backend_init(&backend, cards, 2);
	study_backend_build_view(&backend, &view);
	assert(
		app_review_action_undo_target(&view) ==
		APP_REVIEW_UNDO_UNAVAILABLE
	);

	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	study_backend_build_view(&backend, &view);
	assert(
		app_review_action_undo_target(&view) ==
		APP_REVIEW_UNDO_APPLY
	);
}

int main(void)
{
	test_scroll_actions_clamp_without_dirty_state();
	test_reveal_resets_scroll_and_marks_state_dirty();
	test_rating_resets_scroll_and_sets_log_metadata();
	test_undo_resets_scroll_and_sets_log_metadata();
	test_exit_sets_request_without_handling_action();
	test_invalid_rate_action_is_ignored();
	test_suspend_restore_target_for_review_states();
	test_undo_target_for_review_states();
	return 0;
}
