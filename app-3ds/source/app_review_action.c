#include "app_review_action.h"

static bool app_review_action_backend_rating_from_control(
	enum study_control_rating control_rating,
	enum study_backend_rating *backend_rating
)
{
	if (backend_rating == NULL)
		return false;

	switch (control_rating)
	{
	case STUDY_CONTROL_RATING_AGAIN:
		*backend_rating = STUDY_BACKEND_RATING_AGAIN;
		return true;
	case STUDY_CONTROL_RATING_HARD:
		*backend_rating = STUDY_BACKEND_RATING_HARD;
		return true;
	case STUDY_CONTROL_RATING_GOOD:
		*backend_rating = STUDY_BACKEND_RATING_GOOD;
		return true;
	case STUDY_CONTROL_RATING_EASY:
		*backend_rating = STUDY_BACKEND_RATING_EASY;
		return true;
	case STUDY_CONTROL_RATING_NONE:
		return false;
	}

	return false;
}

static bool app_review_action_log_rating_from_control(
	enum study_control_rating control_rating,
	enum study_review_log_rating *log_rating
)
{
	if (log_rating == NULL)
		return false;

	switch (control_rating)
	{
	case STUDY_CONTROL_RATING_AGAIN:
		*log_rating = STUDY_REVIEW_LOG_RATING_AGAIN;
		return true;
	case STUDY_CONTROL_RATING_HARD:
		*log_rating = STUDY_REVIEW_LOG_RATING_HARD;
		return true;
	case STUDY_CONTROL_RATING_GOOD:
		*log_rating = STUDY_REVIEW_LOG_RATING_GOOD;
		return true;
	case STUDY_CONTROL_RATING_EASY:
		*log_rating = STUDY_REVIEW_LOG_RATING_EASY;
		return true;
	case STUDY_CONTROL_RATING_NONE:
		return false;
	}

	return false;
}

enum app_review_undo_target app_review_action_undo_target(
	const struct study_backend_view *view
)
{
	if (view == NULL || view->card_count == 0)
		return APP_REVIEW_UNDO_NONE;
	if (view->undo_available)
		return APP_REVIEW_UNDO_APPLY;

	return APP_REVIEW_UNDO_UNAVAILABLE;
}

enum app_review_suspend_restore_target app_review_action_suspend_restore_target(
	const struct study_backend_view *view
)
{
	if (view == NULL || view->card_count == 0)
		return APP_REVIEW_SUSPEND_RESTORE_NONE;
	if (view->has_active_card)
		return APP_REVIEW_SUSPEND_RESTORE_SUSPEND;
	if (view->suspended_count > 0)
		return APP_REVIEW_SUSPEND_RESTORE_RESTORE;

	return APP_REVIEW_SUSPEND_RESTORE_UNAVAILABLE;
}

bool app_review_action_apply(
	struct study_backend *backend,
	enum study_control_action action,
	enum study_control_rating control_rating,
	size_t max_scroll_offset,
	size_t *scroll_offset,
	bool *state_dirty,
	bool *log_dirty,
	enum study_review_log_event *log_event,
	enum study_review_log_rating *log_rating,
	bool *exit_requested
)
{
	if (
		scroll_offset == NULL ||
		state_dirty == NULL ||
		exit_requested == NULL
	)
	{
		return false;
	}

	*state_dirty = false;
	*exit_requested = false;
	if (log_dirty != NULL)
		*log_dirty = false;
	if (log_event != NULL)
		*log_event = STUDY_REVIEW_LOG_EVENT_RATING;
	if (log_rating != NULL)
		*log_rating = STUDY_REVIEW_LOG_RATING_NONE;

	switch (action)
	{
	case STUDY_CONTROL_ACTION_EXIT:
		*exit_requested = true;
		return false;
	case STUDY_CONTROL_ACTION_DECKS:
		return false;
	case STUDY_CONTROL_ACTION_THEME:
		return false;
	case STUDY_CONTROL_ACTION_TOGGLE_HELP:
		return false;
	case STUDY_CONTROL_ACTION_UNDO:
		if (study_backend_undo_last_rating(backend))
		{
			*scroll_offset = 0;
			*state_dirty = true;
			if (log_dirty != NULL)
				*log_dirty = true;
			if (log_event != NULL)
				*log_event = STUDY_REVIEW_LOG_EVENT_UNDO;
			return true;
		}
		return false;
	case STUDY_CONTROL_ACTION_SUSPEND_RESTORE:
		return false;
	case STUDY_CONTROL_ACTION_REVEAL:
		if (study_backend_show_answer(backend))
		{
			*scroll_offset = 0;
			*state_dirty = true;
			return true;
		}
		return false;
	case STUDY_CONTROL_ACTION_RATE:
	{
		enum study_backend_rating backend_rating;

		if (
			app_review_action_backend_rating_from_control(
				control_rating,
				&backend_rating
			) &&
			app_review_action_log_rating_from_control(control_rating, log_rating) &&
			study_backend_rate_current(backend, backend_rating)
		)
		{
			*scroll_offset = 0;
			*state_dirty = true;
			if (log_dirty != NULL)
				*log_dirty = true;
			if (log_event != NULL)
				*log_event = STUDY_REVIEW_LOG_EVENT_RATING;
			return true;
		}
		return false;
	}
	case STUDY_CONTROL_ACTION_SCROLL_DOWN:
		if (*scroll_offset >= max_scroll_offset)
			return false;
		(*scroll_offset)++;
		return true;
	case STUDY_CONTROL_ACTION_SCROLL_UP:
		if (*scroll_offset == 0)
			return false;
		(*scroll_offset)--;
		return true;
	case STUDY_CONTROL_ACTION_NONE:
		return false;
	}

	return false;
}
