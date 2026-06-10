#include "study_backend.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const struct study_backend_card cards[] = {
	{ "Front 1", "Back 1", NULL },
	{ "Front 2", "Back 2", NULL },
};

static const struct study_backend_card three_cards[] = {
	{ "Front 1", "Back 1", NULL },
	{ "Front 2", "Back 2", NULL },
	{ "Front 3", "Back 3", NULL },
};

static const struct study_backend_card four_cards[] = {
	{ "Front 1", "Back 1", NULL },
	{ "Front 2", "Back 2", NULL },
	{ "Front 3", "Back 3", NULL },
	{ "Front 4", "Back 4", NULL },
};

static void test_initial_view_shows_front_text(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	study_backend_build_view(&backend, &view);

	assert(view.has_active_card);
	assert(!view.answer_visible);
	assert(view.card_index == 0);
	assert(view.card_count == 2);
	assert(view.progress_day == 0);
	assert(view.introduced_count == 0);
	assert(view.introduced_today_count == 0);
	assert(view.reviewed_today_count == 0);
	assert(!view.current_card_introduced);
	assert(strcmp(view.front_text, "Front 1") == 0);
	assert(strcmp(view.back_text, "Back 1") == 0);
	assert(strcmp(view.primary_text, "Front 1") == 0);
	assert(strcmp(view.status_text, "Question") == 0);
}

static void test_show_answer_switches_to_back_text(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	assert(study_backend_show_answer(&backend));
	study_backend_build_view(&backend, &view);

	assert(view.has_active_card);
	assert(view.answer_visible);
	assert(view.introduced_count == 1);
	assert(view.introduced_today_count == 1);
	assert(view.current_card_introduced);
	assert(strcmp(view.front_text, "Front 1") == 0);
	assert(strcmp(view.back_text, "Back 1") == 0);
	assert(strcmp(view.primary_text, "Back 1") == 0);
	assert(strcmp(view.status_text, "Answer") == 0);
}

static void test_reveal_introduces_each_card_once(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_AGAIN));
	study_backend_build_view(&backend, &view);
	assert(view.introduced_count == 1);
	assert(view.introduced_today_count == 1);
	assert(view.current_card_introduced);

	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	study_backend_build_view(&backend, &view);
	assert(view.has_active_card);
	assert(view.card_index == 1);
	assert(view.introduced_count == 1);
	assert(view.introduced_today_count == 1);
	assert(!view.current_card_introduced);

	assert(study_backend_show_answer(&backend));
	study_backend_build_view(&backend, &view);
	assert(view.introduced_count == 2);
	assert(view.introduced_today_count == 2);
	assert(view.current_card_introduced);
}

static void test_again_repeats_same_card(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_AGAIN));
	study_backend_build_view(&backend, &view);

	assert(view.has_active_card);
	assert(!view.answer_visible);
	assert(view.card_index == 0);
	assert(view.reviewed_count == 1);
	assert(view.reviewed_today_count == 1);
	assert(view.introduced_count == 1);
	assert(view.introduced_today_count == 1);
	assert(view.again_count == 1);
	assert(strcmp(view.primary_text, "Front 1") == 0);
}

static void test_card_count_cooldown_interleaves_new_cards_after_again(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, four_cards, 4);
	study_backend_set_scheduler_policy(
		&backend,
		STUDY_BACKEND_SCHEDULER_CARD_COUNT_COOLDOWN,
		2
	);
	assert(
		strcmp(
			study_backend_scheduler_policy_name(backend.scheduler_policy),
			"Card cooldown"
		) == 0
	);

	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_AGAIN));
	study_backend_build_view(&backend, &view);
	assert(view.has_active_card);
	assert(view.card_index == 1);
	assert(!view.current_card_introduced);
	assert(backend.cooldown_remaining[0] == 2);

	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	study_backend_build_view(&backend, &view);
	assert(view.has_active_card);
	assert(view.card_index == 2);
	assert(!view.current_card_introduced);
	assert(backend.cooldown_remaining[0] == 1);

	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_EASY));
	study_backend_build_view(&backend, &view);
	assert(view.has_active_card);
	assert(view.card_index == 0);
	assert(view.current_card_introduced);
	assert(backend.cooldown_remaining[0] == 0);
}

static void test_card_count_cooldown_falls_back_when_no_other_card_is_available(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	study_backend_set_scheduler_policy(
		&backend,
		STUDY_BACKEND_SCHEDULER_CARD_COUNT_COOLDOWN,
		STUDY_BACKEND_DEFAULT_CARD_COOLDOWN
	);

	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_AGAIN));
	study_backend_build_view(&backend, &view);
	assert(view.card_index == 1);

	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	study_backend_build_view(&backend, &view);
	assert(view.has_active_card);
	assert(view.card_index == 0);
	assert(view.current_card_introduced);
	assert(backend.cooldown_remaining[0] > 0);
}

static void test_undo_restores_scheduler_cooldowns(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, three_cards, 3);
	study_backend_set_scheduler_policy(
		&backend,
		STUDY_BACKEND_SCHEDULER_CARD_COUNT_COOLDOWN,
		2
	);

	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_AGAIN));
	study_backend_build_view(&backend, &view);
	assert(view.card_index == 1);
	assert(backend.cooldown_remaining[0] == 2);

	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	study_backend_build_view(&backend, &view);
	assert(view.card_index == 2);
	assert(backend.cooldown_remaining[0] == 1);

	assert(study_backend_undo_last_rating(&backend));
	study_backend_build_view(&backend, &view);
	assert(view.card_index == 1);
	assert(backend.cooldown_remaining[0] == 2);
	assert(!view.undo_available);
}

static void test_undo_restores_due_schedule(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	assert(!study_backend_rollover_day(&backend, 20000));
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	assert(backend.due_day[0] == 20002);
	assert(backend.interval_days[0] == 2);
	assert(backend.completed_today[0]);

	assert(study_backend_undo_last_rating(&backend));
	study_backend_build_view(&backend, &view);
	assert(view.card_index == 0);
	assert(backend.due_day[0] == 0);
	assert(backend.interval_days[0] == 0);
	assert(!backend.completed_today[0]);
	assert(!view.undo_available);
}

static void test_good_advances_and_completes(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	study_backend_build_view(&backend, &view);

	assert(view.has_active_card);
	assert(view.card_index == 1);
	assert(view.good_count == 1);
	assert(strcmp(view.primary_text, "Front 2") == 0);

	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_EASY));
	study_backend_build_view(&backend, &view);

	assert(!view.has_active_card);
	assert(view.reviewed_count == 2);
	assert(view.reviewed_today_count == 2);
	assert(view.good_count == 1);
	assert(view.easy_count == 1);
	assert(strcmp(view.primary_text, "Session complete.") == 0);
	assert(strcmp(view.status_text, "Complete") == 0);
}

static void test_review_again_reopens_completed_cards_without_resetting_progress(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_EASY));
	study_backend_build_view(&backend, &view);
	assert(!view.has_active_card);
	assert(view.reviewed_count == 2);
	assert(view.reviewed_today_count == 2);
	assert(view.introduced_count == 2);
	assert(view.introduced_today_count == 2);

	assert(study_backend_review_again(&backend));
	study_backend_build_view(&backend, &view);

	assert(view.has_active_card);
	assert(!view.answer_visible);
	assert(!view.undo_available);
	assert(view.card_index == 0);
	assert(view.reviewed_count == 2);
	assert(view.reviewed_today_count == 0);
	assert(view.introduced_count == 2);
	assert(view.introduced_today_count == 2);
	assert(strcmp(view.primary_text, "Front 1") == 0);
	assert(strcmp(view.status_text, "Review again; card 1/2") == 0);

	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_HARD));
	study_backend_build_view(&backend, &view);
	assert(view.reviewed_count == 3);
	assert(view.reviewed_today_count == 1);
	assert(view.hard_count == 1);
}

static void test_review_again_reports_restore_when_only_suspended_cards_remain(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	assert(study_backend_suspend_current(&backend));
	assert(study_backend_suspend_current(&backend));
	study_backend_build_view(&backend, &view);
	assert(!view.has_active_card);
	assert(view.suspended_count == 2);

	assert(!study_backend_review_again(&backend));
	study_backend_build_view(&backend, &view);
	assert(!view.has_active_card);
	assert(strcmp(view.status_text, "Nothing to review; restore suspended") == 0);
}

static void test_undo_good_restores_previous_question(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	study_backend_build_view(&backend, &view);
	assert(view.undo_available);
	assert(view.card_index == 1);
	assert(view.good_count == 1);

	assert(study_backend_undo_last_rating(&backend));
	study_backend_build_view(&backend, &view);
	assert(view.has_active_card);
	assert(!view.answer_visible);
	assert(!view.undo_available);
	assert(view.card_index == 0);
	assert(view.reviewed_count == 0);
	assert(view.reviewed_today_count == 0);
	assert(view.introduced_count == 1);
	assert(view.introduced_today_count == 1);
	assert(view.good_count == 0);
	assert(strcmp(view.primary_text, "Front 1") == 0);
	assert(strcmp(view.status_text, "Undo; card 1/2") == 0);
	assert(!study_backend_undo_last_rating(&backend));
}

static void test_undo_again_reverts_review_count_without_advancing(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_AGAIN));
	assert(study_backend_undo_last_rating(&backend));
	study_backend_build_view(&backend, &view);

	assert(view.has_active_card);
	assert(view.card_index == 0);
	assert(view.reviewed_count == 0);
	assert(view.reviewed_today_count == 0);
	assert(view.introduced_count == 1);
	assert(view.introduced_today_count == 1);
	assert(view.again_count == 0);
	assert(strcmp(view.primary_text, "Front 1") == 0);
}

static void test_undo_after_completion_restores_last_card(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_EASY));
	study_backend_build_view(&backend, &view);
	assert(!view.has_active_card);
	assert(view.undo_available);

	assert(study_backend_undo_last_rating(&backend));
	study_backend_build_view(&backend, &view);

	assert(view.has_active_card);
	assert(view.card_index == 1);
	assert(view.reviewed_count == 1);
	assert(view.reviewed_today_count == 1);
	assert(view.good_count == 1);
	assert(view.easy_count == 0);
	assert(strcmp(view.primary_text, "Front 2") == 0);
}

static void test_cannot_rate_before_answer(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	assert(!study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	study_backend_build_view(&backend, &view);

	assert(view.has_active_card);
	assert(view.card_index == 0);
	assert(view.reviewed_count == 0);
	assert(strcmp(view.primary_text, "Front 1") == 0);
}

static void test_day_rollover_resets_daily_counts_only(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	assert(!study_backend_rollover_day(&backend, 20000));
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	study_backend_build_view(&backend, &view);
	assert(view.progress_day == 20000);
	assert(view.reviewed_count == 1);
	assert(view.reviewed_today_count == 1);
	assert(view.introduced_count == 1);
	assert(view.introduced_today_count == 1);
	assert(view.undo_available);

	assert(study_backend_rollover_day(&backend, 20001));
	study_backend_build_view(&backend, &view);
	assert(view.progress_day == 20001);
	assert(view.reviewed_count == 1);
	assert(view.reviewed_today_count == 0);
	assert(view.introduced_count == 1);
	assert(view.introduced_today_count == 0);
	assert(!view.answer_visible);
	assert(!view.undo_available);
	assert(view.card_index == 1);
	assert(!view.current_card_introduced);
	assert(strcmp(view.status_text, "New day; limits reset") == 0);

	assert(study_backend_show_answer(&backend));
	study_backend_build_view(&backend, &view);
	assert(view.introduced_count == 2);
	assert(view.introduced_today_count == 1);
	assert(view.current_card_introduced);

	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	study_backend_build_view(&backend, &view);
	assert(!view.has_active_card);

	assert(study_backend_rollover_day(&backend, 20002));
	study_backend_build_view(&backend, &view);
	assert(view.has_active_card);
	assert(view.card_index == 0);
	assert(view.current_card_introduced);
}

static void test_day_rollover_reopens_completed_deck(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	assert(!study_backend_rollover_day(&backend, 20000));
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_EASY));
	study_backend_build_view(&backend, &view);
	assert(!view.has_active_card);
	assert(view.introduced_count == 2);
	assert(view.introduced_today_count == 2);
	assert(view.reviewed_count == 2);
	assert(view.reviewed_today_count == 2);

	assert(study_backend_rollover_day(&backend, 20001));
	study_backend_build_view(&backend, &view);
	assert(!view.has_active_card);
	assert(view.progress_day == 20001);
	assert(view.introduced_count == 2);
	assert(view.introduced_today_count == 0);
	assert(view.reviewed_count == 2);
	assert(view.reviewed_today_count == 0);
	assert(strcmp(view.primary_text, "Session complete.") == 0);
	assert(strcmp(view.status_text, "Complete") == 0);

	assert(study_backend_rollover_day(&backend, 20002));
	study_backend_build_view(&backend, &view);
	assert(view.has_active_card);
	assert(view.card_index == 0);
	assert(view.progress_day == 20002);
	assert(view.current_card_introduced);
	assert(strcmp(view.primary_text, "Front 1") == 0);
}

static void test_day_rollover_reviews_introduced_before_new_cards(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, three_cards, 3);
	assert(!study_backend_rollover_day(&backend, 20000));
	assert(study_backend_suspend_current(&backend));
	study_backend_build_view(&backend, &view);
	assert(view.card_index == 1);
	assert(!view.current_card_introduced);

	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	study_backend_build_view(&backend, &view);
	assert(view.card_index == 2);
	assert(!view.current_card_introduced);

	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_EASY));
	study_backend_build_view(&backend, &view);
	assert(!view.has_active_card);
	assert(view.introduced_count == 2);
	assert(view.introduced_today_count == 2);

	assert(study_backend_restore_suspended(&backend) == 1);
	study_backend_build_view(&backend, &view);
	assert(view.has_active_card);
	assert(view.card_index == 0);
	assert(!view.current_card_introduced);

	assert(study_backend_rollover_day(&backend, 20002));
	study_backend_build_view(&backend, &view);
	assert(view.has_active_card);
	assert(view.card_index == 1);
	assert(view.current_card_introduced);
	assert(view.reviewed_today_count == 0);
	assert(view.introduced_today_count == 0);

	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	study_backend_build_view(&backend, &view);
	assert(view.has_active_card);
	assert(view.card_index == 0);
	assert(!view.current_card_introduced);

	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_EASY));
	study_backend_build_view(&backend, &view);
	assert(!view.has_active_card);

	assert(study_backend_rollover_day(&backend, 20004));
	study_backend_build_view(&backend, &view);
	assert(view.has_active_card);
	assert(view.card_index == 2);
	assert(view.current_card_introduced);
}

static void test_day_rollover_establishes_day_for_existing_progress(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	study_backend_build_view(&backend, &view);
	assert(view.progress_day == 0);
	assert(view.reviewed_count == 1);
	assert(view.reviewed_today_count == 1);
	assert(view.introduced_count == 1);
	assert(view.introduced_today_count == 1);

	assert(study_backend_rollover_day(&backend, 20000));
	study_backend_build_view(&backend, &view);
	assert(view.progress_day == 20000);
	assert(view.reviewed_count == 1);
	assert(view.reviewed_today_count == 1);
	assert(view.introduced_count == 1);
	assert(view.introduced_today_count == 1);
	assert(view.card_index == 1);
	assert(!view.answer_visible);
	assert(view.undo_available);
	assert(strcmp(view.status_text, "Good; card 2/2") == 0);
	assert(!study_backend_rollover_day(&backend, 20000));
}

static void test_suspend_current_skips_card(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, three_cards, 3);
	assert(study_backend_suspend_current(&backend));
	study_backend_build_view(&backend, &view);

	assert(view.has_active_card);
	assert(!view.answer_visible);
	assert(!view.undo_available);
	assert(view.card_index == 1);
	assert(view.suspended_count == 1);
	assert(strcmp(view.primary_text, "Front 2") == 0);
	assert(strcmp(view.status_text, "Suspended; card 2/3") == 0);
}

static void test_suspend_last_active_card_leaves_restore_prompt_state(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	assert(study_backend_suspend_current(&backend));
	study_backend_build_view(&backend, &view);

	assert(!view.has_active_card);
	assert(view.suspended_count == 1);
	assert(strcmp(view.primary_text, "No active cards.") == 0);
	assert(strcmp(view.status_text, "Suspended; no active cards") == 0);
}

static void test_restore_suspended_returns_to_first_skipped_card(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, three_cards, 3);
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	assert(study_backend_suspend_current(&backend));
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_EASY));

	study_backend_build_view(&backend, &view);
	assert(!view.has_active_card);
	assert(view.suspended_count == 1);
	assert(study_backend_restore_suspended(&backend) == 1);
	study_backend_build_view(&backend, &view);

	assert(view.has_active_card);
	assert(view.card_index == 1);
	assert(view.suspended_count == 0);
	assert(strcmp(view.primary_text, "Front 2") == 0);
	assert(strcmp(view.status_text, "Restored 1 suspended; card 2/3") == 0);
}

static void test_reset_progress_clears_counts_and_suspension(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, three_cards, 3);
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	assert(study_backend_suspend_current(&backend));
	assert(study_backend_reset_progress(&backend));
	study_backend_build_view(&backend, &view);

	assert(view.has_active_card);
	assert(!view.answer_visible);
	assert(!view.undo_available);
	assert(view.card_index == 0);
	assert(view.reviewed_count == 0);
	assert(view.introduced_count == 0);
	assert(view.good_count == 0);
	assert(view.suspended_count == 0);
	assert(strcmp(view.primary_text, "Front 1") == 0);
	assert(strcmp(view.status_text, "Progress reset") == 0);
}

static void test_reset_empty_session_reports_no_cards(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, NULL, 0);
	assert(!study_backend_reset_progress(&backend));
	study_backend_build_view(&backend, &view);

	assert(!view.has_active_card);
	assert(view.card_count == 0);
	assert(strcmp(view.front_text, "") == 0);
	assert(strcmp(view.back_text, "") == 0);
	assert(strcmp(view.primary_text, "No cards loaded.") == 0);
	assert(strcmp(view.status_text, "No cards loaded") == 0);
}

static void test_empty_session_outputs_text(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, NULL, 0);
	study_backend_build_view(&backend, &view);

	assert(!view.has_active_card);
	assert(view.card_count == 0);
	assert(strcmp(view.primary_text, "No cards loaded.") == 0);
	assert(strcmp(view.status_text, "No cards loaded") == 0);
}

static void write_test_file(const char *path, const char *contents)
{
	FILE *file = fopen(path, "w");

	assert(file != NULL);
	assert(fputs(contents, file) >= 0);
	assert(fclose(file) == 0);
}

static void remove_if_present(const char *path)
{
	(void)remove(path);
}

static bool file_exists(const char *path)
{
	FILE *file = fopen(path, "r");

	if (file == NULL)
		return false;

	assert(fclose(file) == 0);
	return true;
}

static bool file_contains(const char *path, const char *needle)
{
	char buffer[4096];
	size_t read_count;
	FILE *file = fopen(path, "r");

	assert(needle != NULL);
	if (file == NULL)
		return false;

	read_count = fread(buffer, 1, sizeof(buffer) - 1, file);
	buffer[read_count] = '\0';
	assert(fclose(file) == 0);
	return strstr(buffer, needle) != NULL;
}

static void test_load_cards_tsv_feeds_backend_strings(void)
{
	const char *path = "/tmp/anki3ds-study-backend-cards.tsv";
	struct study_backend_deck deck;
	struct study_backend backend;
	struct study_backend_view view;

	write_test_file(
		path,
		"card-1\tnote-1\tFront one\tBack one\ttag\n"
		"card-2\tnote-2\tLine 1\\nLine 2\tBack\\tTab\\\\Slash\ttag\n"
		"card-3\tnote-3\tVisible\\\\nEscape\tBack\ttag\n"
	);

	assert(study_backend_load_cards_tsv(&deck, path) == STUDY_BACKEND_LOAD_OK);
	assert(deck.card_count == 3);
	assert(strcmp(deck.status_text, "Loaded 3 cards") == 0);
	assert(strcmp(deck.cards[0].front, "Front one") == 0);
	assert(strcmp(deck.cards[0].tags, "tag") == 0);
	assert(strcmp(deck.cards[1].front, "Line 1\nLine 2") == 0);
	assert(strcmp(deck.cards[1].back, "Back\tTab\\Slash") == 0);
	assert(strcmp(deck.cards[1].tags, "tag") == 0);
	assert(strcmp(deck.cards[2].front, "Visible\\nEscape") == 0);

	study_backend_init(&backend, deck.cards, deck.card_count);
	study_backend_set_status(&backend, deck.status_text);
	study_backend_build_view(&backend, &view);
	assert(view.has_active_card);
	assert(strcmp(view.primary_text, "Front one") == 0);
	assert(strcmp(view.tags_text, "tag") == 0);
	assert(strcmp(view.status_text, "Loaded 3 cards") == 0);

	assert(remove(path) == 0);
}

static void test_load_cards_tsv_rejects_tags_that_are_too_long(void)
{
	const char *path = "/tmp/anki3ds-study-backend-long-tags.tsv";
	struct study_backend_deck deck;
	char contents[256];
	size_t used;

	used = (size_t)snprintf(
		contents,
		sizeof(contents),
		"card-1\tnote-1\tfront\tback\t"
	);
	memset(contents + used, 't', STUDY_BACKEND_TAGS_TEXT_SIZE);
	contents[used + STUDY_BACKEND_TAGS_TEXT_SIZE] = '\n';
	contents[used + STUDY_BACKEND_TAGS_TEXT_SIZE + 1] = '\0';
	write_test_file(path, contents);

	assert(
		study_backend_load_cards_tsv(&deck, path) ==
		STUDY_BACKEND_LOAD_TEXT_TOO_LONG
	);
	assert(deck.card_count == 0);
	assert(strcmp(deck.status_text, "Text too long line 1") == 0);

	assert(remove(path) == 0);
}

static void test_load_cards_tsv_rejects_bad_field_count(void)
{
	const char *path = "/tmp/anki3ds-study-backend-bad.tsv";
	struct study_backend_deck deck;

	write_test_file(path, "card-1\tnote-1\tfront only\n");

	assert(
		study_backend_load_cards_tsv(&deck, path) ==
		STUDY_BACKEND_LOAD_BAD_FORMAT
	);
	assert(deck.card_count == 0);
	assert(strcmp(deck.status_text, "Bad line 1") == 0);

	assert(remove(path) == 0);
}

static void test_load_cards_tsv_reports_missing_file(void)
{
	struct study_backend_deck deck;

	assert(
		study_backend_load_cards_tsv(
			&deck,
			"/tmp/anki3ds-study-backend-missing.tsv"
		) == STUDY_BACKEND_LOAD_NOT_FOUND
	);
	assert(deck.card_count == 0);
	assert(strcmp(deck.status_text, "No deck file") == 0);
	assert(
		strcmp(
			study_backend_load_result_name(STUDY_BACKEND_LOAD_NOT_FOUND),
			"Not found"
		) == 0
	);
}

static void test_state_round_trip_restores_progress_without_undo(void)
{
	const char *path = "/tmp/anki3ds-study-backend-state.tsv";
	const char *tmp_path = "/tmp/anki3ds-study-backend-state.tsv.tmp";
	const char *bak_path = "/tmp/anki3ds-study-backend-state.tsv.bak";
	struct study_backend backend;
	struct study_backend restored;
	struct study_backend_view view;

	remove_if_present(path);
	remove_if_present(tmp_path);
	remove_if_present(bak_path);
	study_backend_init(&backend, cards, 2);
	assert(!study_backend_rollover_day(&backend, 20000));
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	assert(backend.undo_available);
	assert(
		study_backend_save_state_tsv(&backend, path) ==
		STUDY_BACKEND_STATE_OK
	);

	study_backend_init(&restored, cards, 2);
	assert(
		study_backend_load_state_tsv(&restored, path) ==
		STUDY_BACKEND_STATE_OK
	);
	study_backend_build_view(&restored, &view);

	assert(view.has_active_card);
	assert(!view.answer_visible);
	assert(!view.undo_available);
	assert(view.card_index == 1);
	assert(view.progress_day == 20000);
	assert(view.reviewed_count == 1);
	assert(view.reviewed_today_count == 1);
	assert(view.introduced_count == 1);
	assert(view.introduced_today_count == 1);
	assert(!view.current_card_introduced);
	assert(view.good_count == 1);
	assert(strcmp(view.primary_text, "Front 2") == 0);
	assert(strcmp(view.status_text, "Restored card 2/2") == 0);
	assert(remove(path) == 0);
	remove_if_present(tmp_path);
	remove_if_present(bak_path);
}

static void test_state_round_trip_restores_suspended_cards(void)
{
	const char *path = "/tmp/anki3ds-study-backend-suspended-state.tsv";
	const char *tmp_path = "/tmp/anki3ds-study-backend-suspended-state.tsv.tmp";
	const char *bak_path = "/tmp/anki3ds-study-backend-suspended-state.tsv.bak";
	struct study_backend backend;
	struct study_backend restored;
	struct study_backend_view view;

	remove_if_present(path);
	remove_if_present(tmp_path);
	remove_if_present(bak_path);
	study_backend_init(&backend, three_cards, 3);
	assert(study_backend_suspend_current(&backend));
	assert(
		study_backend_save_state_tsv(&backend, path) ==
		STUDY_BACKEND_STATE_OK
	);

	study_backend_init(&restored, three_cards, 3);
	assert(
		study_backend_load_state_tsv(&restored, path) ==
		STUDY_BACKEND_STATE_OK
	);
	study_backend_build_view(&restored, &view);

	assert(view.has_active_card);
	assert(view.card_index == 1);
	assert(view.suspended_count == 1);
	assert(strcmp(view.primary_text, "Front 2") == 0);
	assert(strcmp(view.status_text, "Restored card 2/3") == 0);
	assert(remove(path) == 0);
	remove_if_present(tmp_path);
	remove_if_present(bak_path);
}

static void test_state_round_trip_preserves_completed_today_queue(void)
{
	const char *path = "/tmp/anki3ds-study-backend-completed-today-state.tsv";
	const char *tmp_path =
		"/tmp/anki3ds-study-backend-completed-today-state.tsv.tmp";
	const char *bak_path =
		"/tmp/anki3ds-study-backend-completed-today-state.tsv.bak";
	struct study_backend backend;
	struct study_backend restored;
	struct study_backend_view view;

	remove_if_present(path);
	remove_if_present(tmp_path);
	remove_if_present(bak_path);
	study_backend_init(&backend, three_cards, 3);
	assert(!study_backend_rollover_day(&backend, 20000));
	assert(study_backend_suspend_current(&backend));
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_EASY));
	assert(study_backend_restore_suspended(&backend) == 1);
	study_backend_build_view(&backend, &view);
	assert(view.card_index == 0);
	assert(!view.current_card_introduced);

	assert(
		study_backend_save_state_tsv(&backend, path) ==
		STUDY_BACKEND_STATE_OK
	);
	study_backend_init(&restored, three_cards, 3);
	assert(
		study_backend_load_state_tsv(&restored, path) ==
		STUDY_BACKEND_STATE_OK
	);
	study_backend_build_view(&restored, &view);
	assert(view.has_active_card);
	assert(view.card_index == 0);
	assert(!view.current_card_introduced);
	assert(view.reviewed_today_count == 2);
	assert(view.introduced_today_count == 2);

	assert(study_backend_rollover_day(&restored, 20001));
	study_backend_build_view(&restored, &view);
	assert(view.has_active_card);
	assert(view.card_index == 0);
	assert(!view.current_card_introduced);
	assert(view.reviewed_today_count == 0);
	assert(view.introduced_today_count == 0);

	assert(study_backend_rollover_day(&restored, 20002));
	study_backend_build_view(&restored, &view);
	assert(view.has_active_card);
	assert(view.card_index == 1);
	assert(view.current_card_introduced);
	assert(view.reviewed_today_count == 0);
	assert(view.introduced_today_count == 0);

	assert(remove(path) == 0);
	remove_if_present(tmp_path);
	remove_if_present(bak_path);
}

static void test_state_round_trip_preserves_due_schedule(void)
{
	const char *path = "/tmp/anki3ds-study-backend-schedule-state.tsv";
	const char *tmp_path = "/tmp/anki3ds-study-backend-schedule-state.tsv.tmp";
	const char *bak_path = "/tmp/anki3ds-study-backend-schedule-state.tsv.bak";
	struct study_backend backend;
	struct study_backend restored;
	struct study_backend_view view;

	remove_if_present(path);
	remove_if_present(tmp_path);
	remove_if_present(bak_path);
	study_backend_init(&backend, cards, 2);
	assert(!study_backend_rollover_day(&backend, 20000));
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));
	assert(backend.due_day[0] == 20002);
	assert(backend.interval_days[0] == 2);

	assert(
		study_backend_save_state_tsv(&backend, path) ==
		STUDY_BACKEND_STATE_OK
	);
	assert(file_contains(path, "version\t2\n"));
	assert(file_contains(path, "schedule_index\t0\n"));
	assert(file_contains(path, "schedule_due_day\t20002\n"));
	assert(file_contains(path, "schedule_interval_days\t2\n"));

	study_backend_init(&restored, cards, 2);
	assert(
		study_backend_load_state_tsv(&restored, path) ==
		STUDY_BACKEND_STATE_OK
	);
	assert(restored.due_day[0] == 20002);
	assert(restored.interval_days[0] == 2);
	study_backend_build_view(&restored, &view);
	assert(view.card_index == 1);
	assert(!view.current_card_introduced);

	assert(study_backend_rollover_day(&restored, 20002));
	study_backend_build_view(&restored, &view);
	assert(view.card_index == 0);
	assert(view.current_card_introduced);

	assert(remove(path) == 0);
	remove_if_present(tmp_path);
	remove_if_present(bak_path);
}

static void test_state_missing_file_keeps_current_session(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	assert(
		study_backend_load_state_tsv(
			&backend,
			"/tmp/anki3ds-study-backend-missing-state.tsv"
		) == STUDY_BACKEND_STATE_NOT_FOUND
	);
	study_backend_build_view(&backend, &view);

	assert(view.has_active_card);
	assert(view.card_index == 0);
	assert(view.reviewed_count == 0);
	assert(strcmp(view.status_text, "Question") == 0);
	assert(
		strcmp(
			study_backend_state_result_name(STUDY_BACKEND_STATE_NOT_FOUND),
			"No saved state"
		) == 0
	);
}

static void test_state_rejects_mismatched_card_count(void)
{
	const char *path = "/tmp/anki3ds-study-backend-bad-state.tsv";
	struct study_backend backend;
	struct study_backend_view view;

	write_test_file(
		path,
		"version\t1\n"
		"card_count\t3\n"
		"current_index\t1\n"
		"reviewed_count\t1\n"
		"again_count\t0\n"
		"hard_count\t0\n"
		"good_count\t1\n"
		"easy_count\t0\n"
	);

	study_backend_init(&backend, cards, 2);
	assert(
		study_backend_load_state_tsv(&backend, path) ==
		STUDY_BACKEND_STATE_BAD_FORMAT
	);
	study_backend_build_view(&backend, &view);

	assert(view.has_active_card);
	assert(view.card_index == 0);
	assert(view.reviewed_count == 0);
	assert(strcmp(view.status_text, "Bad saved state") == 0);
	assert(remove(path) == 0);
}

static void test_state_rejects_inconsistent_rating_counts(void)
{
	const char *path = "/tmp/anki3ds-study-backend-count-state.tsv";
	struct study_backend backend;

	write_test_file(
		path,
		"version\t1\n"
		"card_count\t2\n"
		"current_index\t1\n"
		"reviewed_count\t4\n"
		"again_count\t0\n"
		"hard_count\t0\n"
		"good_count\t1\n"
		"easy_count\t0\n"
	);

	study_backend_init(&backend, cards, 2);
	assert(
		study_backend_load_state_tsv(&backend, path) ==
		STUDY_BACKEND_STATE_BAD_FORMAT
	);
	assert(remove(path) == 0);
}

static void test_state_rejects_duplicate_suspended_index(void)
{
	const char *path = "/tmp/anki3ds-study-backend-dup-suspended-state.tsv";
	struct study_backend backend;

	write_test_file(
		path,
		"version\t1\n"
		"card_count\t2\n"
		"current_index\t1\n"
		"reviewed_count\t0\n"
		"again_count\t0\n"
		"hard_count\t0\n"
		"good_count\t0\n"
		"easy_count\t0\n"
		"suspended_count\t2\n"
		"suspended_index\t0\n"
		"suspended_index\t0\n"
	);

	study_backend_init(&backend, cards, 2);
	assert(
		study_backend_load_state_tsv(&backend, path) ==
		STUDY_BACKEND_STATE_BAD_FORMAT
	);
	assert(remove(path) == 0);
}

static void test_state_rejects_duplicate_introduced_index(void)
{
	const char *path = "/tmp/anki3ds-study-backend-dup-introduced-state.tsv";
	struct study_backend backend;

	write_test_file(
		path,
		"version\t1\n"
		"card_count\t2\n"
		"current_index\t1\n"
		"reviewed_count\t0\n"
		"introduced_count\t2\n"
		"introduced_index\t0\n"
		"introduced_index\t0\n"
		"again_count\t0\n"
		"hard_count\t0\n"
		"good_count\t0\n"
		"easy_count\t0\n"
		"suspended_count\t0\n"
	);

	study_backend_init(&backend, cards, 2);
	assert(
		study_backend_load_state_tsv(&backend, path) ==
		STUDY_BACKEND_STATE_BAD_FORMAT
	);
	assert(remove(path) == 0);
}

static void test_state_rejects_daily_counts_above_totals(void)
{
	const char *path = "/tmp/anki3ds-study-backend-bad-daily-state.tsv";
	struct study_backend backend;

	write_test_file(
		path,
		"version\t1\n"
		"card_count\t2\n"
		"current_index\t1\n"
		"progress_day\t20000\n"
		"reviewed_count\t1\n"
		"reviewed_today_count\t2\n"
		"introduced_count\t1\n"
		"introduced_today_count\t2\n"
		"introduced_index\t0\n"
		"again_count\t0\n"
		"hard_count\t0\n"
		"good_count\t1\n"
		"easy_count\t0\n"
		"suspended_count\t0\n"
	);

	study_backend_init(&backend, cards, 2);
	assert(
		study_backend_load_state_tsv(&backend, path) ==
		STUDY_BACKEND_STATE_BAD_FORMAT
	);
	assert(remove(path) == 0);
}

static void test_state_rejects_bad_completed_today_state(void)
{
	const char *path = "/tmp/anki3ds-study-backend-bad-completed-state.tsv";
	struct study_backend backend;

	write_test_file(
		path,
		"version\t1\n"
		"card_count\t2\n"
		"current_index\t1\n"
		"progress_day\t20000\n"
		"reviewed_count\t1\n"
		"reviewed_today_count\t1\n"
		"introduced_count\t1\n"
		"introduced_today_count\t1\n"
		"introduced_index\t0\n"
		"completed_today_count\t1\n"
		"completed_today_index\t1\n"
		"again_count\t0\n"
		"hard_count\t0\n"
		"good_count\t1\n"
		"easy_count\t0\n"
		"suspended_count\t0\n"
	);

	study_backend_init(&backend, cards, 2);
	assert(
		study_backend_load_state_tsv(&backend, path) ==
		STUDY_BACKEND_STATE_BAD_FORMAT
	);
	assert(remove(path) == 0);
}

static void test_state_rejects_bad_schedule_state(void)
{
	const char *path = "/tmp/anki3ds-study-backend-bad-schedule-state.tsv";
	struct study_backend backend;

	write_test_file(
		path,
		"version\t2\n"
		"card_count\t2\n"
		"current_index\t1\n"
		"progress_day\t20000\n"
		"reviewed_count\t1\n"
		"reviewed_today_count\t1\n"
		"introduced_count\t1\n"
		"introduced_today_count\t1\n"
		"introduced_index\t0\n"
		"completed_today_count\t1\n"
		"completed_today_index\t0\n"
		"again_count\t0\n"
		"hard_count\t0\n"
		"good_count\t1\n"
		"easy_count\t0\n"
		"suspended_count\t0\n"
		"schedule_index\t0\n"
		"schedule_due_day\t20002\n"
	);

	study_backend_init(&backend, cards, 2);
	assert(
		study_backend_load_state_tsv(&backend, path) ==
		STUDY_BACKEND_STATE_BAD_FORMAT
	);
	assert(remove(path) == 0);
}

static void test_state_load_recovers_from_temp_artifact(void)
{
	const char *path = "/tmp/anki3ds-study-backend-recover-temp.tsv";
	const char *tmp_path = "/tmp/anki3ds-study-backend-recover-temp.tsv.tmp";
	const char *bak_path = "/tmp/anki3ds-study-backend-recover-temp.tsv.bak";
	struct study_backend backend;
	struct study_backend_view view;

	remove_if_present(path);
	remove_if_present(tmp_path);
	remove_if_present(bak_path);
	write_test_file(
		path,
		"version\t1\n"
		"card_count\t99\n"
		"current_index\t0\n"
		"reviewed_count\t0\n"
		"again_count\t0\n"
		"hard_count\t0\n"
		"good_count\t0\n"
		"easy_count\t0\n"
	);
	write_test_file(
		tmp_path,
		"version\t1\n"
		"card_count\t2\n"
		"current_index\t1\n"
		"reviewed_count\t1\n"
		"again_count\t0\n"
		"hard_count\t0\n"
		"good_count\t1\n"
		"easy_count\t0\n"
	);

	study_backend_init(&backend, cards, 2);
	assert(
		study_backend_load_state_tsv(&backend, path) ==
		STUDY_BACKEND_STATE_OK
	);
	study_backend_build_view(&backend, &view);
	assert(view.card_index == 1);
	assert(view.reviewed_count == 1);
	assert(view.good_count == 1);
	assert(strcmp(view.status_text, "Restored card 2/2") == 0);

	assert(remove(path) == 0);
	assert(remove(tmp_path) == 0);
	remove_if_present(bak_path);
}

static void test_state_load_recovers_from_backup_artifact(void)
{
	const char *path = "/tmp/anki3ds-study-backend-recover-backup.tsv";
	const char *tmp_path = "/tmp/anki3ds-study-backend-recover-backup.tsv.tmp";
	const char *bak_path = "/tmp/anki3ds-study-backend-recover-backup.tsv.bak";
	struct study_backend backend;
	struct study_backend_view view;

	remove_if_present(path);
	remove_if_present(tmp_path);
	remove_if_present(bak_path);
	write_test_file(
		bak_path,
		"version\t1\n"
		"card_count\t2\n"
		"current_index\t1\n"
		"reviewed_count\t1\n"
		"again_count\t0\n"
		"hard_count\t0\n"
		"good_count\t1\n"
		"easy_count\t0\n"
	);

	study_backend_init(&backend, cards, 2);
	assert(
		study_backend_load_state_tsv(&backend, path) ==
		STUDY_BACKEND_STATE_OK
	);
	study_backend_build_view(&backend, &view);
	assert(view.card_index == 1);
	assert(view.reviewed_count == 1);
	assert(view.good_count == 1);

	remove_if_present(path);
	remove_if_present(tmp_path);
	assert(remove(bak_path) == 0);
}

static void test_state_save_preserves_previous_primary_as_backup(void)
{
	const char *path = "/tmp/anki3ds-study-backend-backup.tsv";
	const char *tmp_path = "/tmp/anki3ds-study-backend-backup.tsv.tmp";
	const char *bak_path = "/tmp/anki3ds-study-backend-backup.tsv.bak";
	struct study_backend backend;
	struct study_backend restored;
	struct study_backend_view view;

	remove_if_present(path);
	remove_if_present(tmp_path);
	remove_if_present(bak_path);
	write_test_file(
		path,
		"version\t1\n"
		"card_count\t2\n"
		"current_index\t0\n"
		"reviewed_count\t0\n"
		"again_count\t0\n"
		"hard_count\t0\n"
		"good_count\t0\n"
		"easy_count\t0\n"
	);
	study_backend_init(&backend, cards, 2);
	assert(study_backend_show_answer(&backend));
	assert(study_backend_rate_current(&backend, STUDY_BACKEND_RATING_GOOD));

	assert(
		study_backend_save_state_tsv(&backend, path) ==
		STUDY_BACKEND_STATE_OK
	);
	study_backend_init(&restored, cards, 2);
	assert(
		study_backend_load_state_tsv(&restored, path) ==
		STUDY_BACKEND_STATE_OK
	);
	study_backend_build_view(&restored, &view);
	assert(view.card_index == 1);
	assert(view.reviewed_count == 1);
	assert(view.good_count == 1);

	study_backend_init(&restored, cards, 2);
	assert(
		study_backend_load_state_tsv(&restored, bak_path) ==
		STUDY_BACKEND_STATE_OK
	);
	study_backend_build_view(&restored, &view);
	assert(view.card_index == 0);
	assert(view.reviewed_count == 0);

	assert(remove(path) == 0);
	remove_if_present(tmp_path);
	assert(remove(bak_path) == 0);
}

static void test_delete_state_removes_primary_and_artifacts(void)
{
	const char *path = "/tmp/anki3ds-study-backend-delete-state.tsv";
	const char *tmp_path = "/tmp/anki3ds-study-backend-delete-state.tsv.tmp";
	const char *bak_path = "/tmp/anki3ds-study-backend-delete-state.tsv.bak";

	remove_if_present(path);
	remove_if_present(tmp_path);
	remove_if_present(bak_path);
	write_test_file(path, "version\t1\n");
	write_test_file(tmp_path, "version\t1\n");
	write_test_file(bak_path, "version\t1\n");

	assert(study_backend_delete_state_tsv(path));
	assert(!file_exists(path));
	assert(!file_exists(tmp_path));
	assert(!file_exists(bak_path));
	assert(study_backend_delete_state_tsv(path));
	assert(!study_backend_delete_state_tsv(NULL));
}

int main(void)
{
	test_initial_view_shows_front_text();
	test_show_answer_switches_to_back_text();
	test_reveal_introduces_each_card_once();
	test_again_repeats_same_card();
	test_card_count_cooldown_interleaves_new_cards_after_again();
	test_card_count_cooldown_falls_back_when_no_other_card_is_available();
	test_undo_restores_scheduler_cooldowns();
	test_undo_restores_due_schedule();
	test_good_advances_and_completes();
	test_review_again_reopens_completed_cards_without_resetting_progress();
	test_review_again_reports_restore_when_only_suspended_cards_remain();
	test_undo_good_restores_previous_question();
	test_undo_again_reverts_review_count_without_advancing();
	test_undo_after_completion_restores_last_card();
	test_cannot_rate_before_answer();
	test_day_rollover_resets_daily_counts_only();
	test_day_rollover_reopens_completed_deck();
	test_day_rollover_reviews_introduced_before_new_cards();
	test_day_rollover_establishes_day_for_existing_progress();
	test_suspend_current_skips_card();
	test_suspend_last_active_card_leaves_restore_prompt_state();
	test_restore_suspended_returns_to_first_skipped_card();
	test_reset_progress_clears_counts_and_suspension();
	test_reset_empty_session_reports_no_cards();
	test_empty_session_outputs_text();
	test_load_cards_tsv_feeds_backend_strings();
	test_load_cards_tsv_rejects_tags_that_are_too_long();
	test_load_cards_tsv_rejects_bad_field_count();
	test_load_cards_tsv_reports_missing_file();
	test_state_round_trip_restores_progress_without_undo();
	test_state_round_trip_restores_suspended_cards();
	test_state_round_trip_preserves_completed_today_queue();
	test_state_round_trip_preserves_due_schedule();
	test_state_missing_file_keeps_current_session();
	test_state_rejects_mismatched_card_count();
	test_state_rejects_inconsistent_rating_counts();
	test_state_rejects_duplicate_suspended_index();
	test_state_rejects_duplicate_introduced_index();
	test_state_rejects_daily_counts_above_totals();
	test_state_rejects_bad_completed_today_state();
	test_state_rejects_bad_schedule_state();
	test_state_load_recovers_from_temp_artifact();
	test_state_load_recovers_from_backup_artifact();
	test_state_save_preserves_previous_primary_as_backup();
	test_delete_state_removes_primary_and_artifacts();
	return 0;
}
