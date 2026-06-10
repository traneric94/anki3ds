#ifndef ANKI3DS_STUDY_BACKEND_H
#define ANKI3DS_STUDY_BACKEND_H

#include <stdbool.h>
#include <stddef.h>

#define STUDY_BACKEND_MAX_CARDS 1024
#define STUDY_BACKEND_CARD_TEXT_SIZE 512
#define STUDY_BACKEND_TAGS_TEXT_SIZE 128
#define STUDY_BACKEND_STATUS_SIZE 80
#define STUDY_BACKEND_DEFAULT_CARD_COOLDOWN 20

enum study_backend_rating
{
	STUDY_BACKEND_RATING_AGAIN,
	STUDY_BACKEND_RATING_HARD,
	STUDY_BACKEND_RATING_GOOD,
	STUDY_BACKEND_RATING_EASY,
};

enum study_backend_scheduler_policy
{
	STUDY_BACKEND_SCHEDULER_DUE_FIRST,
	STUDY_BACKEND_SCHEDULER_CARD_COUNT_COOLDOWN,
};

enum study_backend_load_result
{
	STUDY_BACKEND_LOAD_OK,
	STUDY_BACKEND_LOAD_NOT_FOUND,
	STUDY_BACKEND_LOAD_EMPTY,
	STUDY_BACKEND_LOAD_BAD_FORMAT,
	STUDY_BACKEND_LOAD_TOO_MANY_CARDS,
	STUDY_BACKEND_LOAD_TEXT_TOO_LONG,
};

enum study_backend_state_result
{
	STUDY_BACKEND_STATE_OK,
	STUDY_BACKEND_STATE_NOT_FOUND,
	STUDY_BACKEND_STATE_BAD_FORMAT,
	STUDY_BACKEND_STATE_WRITE_FAILED,
};

struct study_backend_card
{
	const char *front;
	const char *back;
	const char *tags;
};

struct study_backend_deck
{
	struct study_backend_card cards[STUDY_BACKEND_MAX_CARDS];
	char front_text[STUDY_BACKEND_MAX_CARDS][STUDY_BACKEND_CARD_TEXT_SIZE];
	char back_text[STUDY_BACKEND_MAX_CARDS][STUDY_BACKEND_CARD_TEXT_SIZE];
	char tags_text[STUDY_BACKEND_MAX_CARDS][STUDY_BACKEND_TAGS_TEXT_SIZE];
	size_t card_count;
	char status_text[STUDY_BACKEND_STATUS_SIZE];
};

struct study_backend
{
	const struct study_backend_card *cards;
	size_t card_count;
	size_t current_index;
	size_t undo_card_index;
	unsigned int progress_day;
	unsigned int reviewed_count;
	unsigned int reviewed_today_count;
	unsigned int introduced_count;
	unsigned int introduced_today_count;
	unsigned int completed_today_count;
	unsigned int again_count;
	unsigned int hard_count;
	unsigned int good_count;
	unsigned int easy_count;
	unsigned int suspended_count;
	unsigned int scheduler_cooldown_steps;
	unsigned int due_day[STUDY_BACKEND_MAX_CARDS];
	unsigned int interval_days[STUDY_BACKEND_MAX_CARDS];
	unsigned int undo_due_day;
	unsigned int undo_interval_days;
	unsigned int cooldown_remaining[STUDY_BACKEND_MAX_CARDS];
	unsigned int undo_cooldown_remaining[STUDY_BACKEND_MAX_CARDS];
	enum study_backend_rating undo_rating;
	enum study_backend_scheduler_policy scheduler_policy;
	bool introduced[STUDY_BACKEND_MAX_CARDS];
	bool completed_today[STUDY_BACKEND_MAX_CARDS];
	bool suspended[STUDY_BACKEND_MAX_CARDS];
	bool answer_visible;
	bool undo_available;
	char status_text[STUDY_BACKEND_STATUS_SIZE];
};

struct study_backend_view
{
	bool has_active_card;
	bool answer_visible;
	bool undo_available;
	size_t card_index;
	size_t card_count;
	unsigned int progress_day;
	unsigned int reviewed_count;
	unsigned int reviewed_today_count;
	unsigned int introduced_count;
	unsigned int introduced_today_count;
	unsigned int again_count;
	unsigned int hard_count;
	unsigned int good_count;
	unsigned int easy_count;
	unsigned int suspended_count;
	bool current_card_introduced;
	const char *front_text;
	const char *back_text;
	const char *tags_text;
	const char *primary_text;
	char status_text[STUDY_BACKEND_STATUS_SIZE];
};

void study_backend_init(
	struct study_backend *backend,
	const struct study_backend_card *cards,
	size_t card_count
);
void study_backend_build_view(
	const struct study_backend *backend,
	struct study_backend_view *view
);
bool study_backend_show_answer(struct study_backend *backend);
bool study_backend_rate_current(
	struct study_backend *backend,
	enum study_backend_rating rating
);
bool study_backend_undo_last_rating(struct study_backend *backend);
bool study_backend_suspend_current(struct study_backend *backend);
unsigned int study_backend_restore_suspended(struct study_backend *backend);
bool study_backend_reset_progress(struct study_backend *backend);
bool study_backend_review_again(struct study_backend *backend);
bool study_backend_rollover_day(
	struct study_backend *backend,
	unsigned int current_day
);
void study_backend_set_scheduler_policy(
	struct study_backend *backend,
	enum study_backend_scheduler_policy policy,
	unsigned int cooldown_steps
);
const char *study_backend_rating_name(enum study_backend_rating rating);
const char *study_backend_scheduler_policy_name(
	enum study_backend_scheduler_policy policy
);
void study_backend_set_status(
	struct study_backend *backend,
	const char *status
);
enum study_backend_load_result study_backend_load_cards_tsv(
	struct study_backend_deck *deck,
	const char *path
);
const char *study_backend_load_result_name(
	enum study_backend_load_result result
);
enum study_backend_state_result study_backend_load_state_tsv(
	struct study_backend *backend,
	const char *path
);
enum study_backend_state_result study_backend_save_state_tsv(
	const struct study_backend *backend,
	const char *path
);
bool study_backend_delete_state_tsv(const char *path);
const char *study_backend_state_result_name(
	enum study_backend_state_result result
);

#endif
