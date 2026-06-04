#ifndef ANKI3DS_APP_DIAGNOSTICS_H
#define ANKI3DS_APP_DIAGNOSTICS_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define APP_DIAGNOSTICS_PATH "sdmc:/3ds/anki3ds/session.tsv"
#define APP_DIAGNOSTICS_DECK_ID_SIZE 64
#define APP_DIAGNOSTICS_EVENT_SIZE 32

struct app_diagnostics
{
	time_t started_at;
	time_t updated_at;
	unsigned int started_day;
	unsigned int current_day;
	bool scan_completed;
	unsigned int deck_count;
	unsigned int ignored_count;
	unsigned int deck_open_count;
	unsigned int review_screen_count;
	unsigned int summary_screen_count;
	unsigned int load_error_count;
	unsigned int answer_shown_count;
	unsigned int rating_saved_count;
	unsigned int undo_saved_count;
	unsigned int suspend_saved_count;
	unsigned int restore_saved_count;
	unsigned int settings_saved_count;
	unsigned int reset_progress_count;
	bool exit_confirmed;
	char last_deck_id[APP_DIAGNOSTICS_DECK_ID_SIZE];
	char last_event[APP_DIAGNOSTICS_EVENT_SIZE];
};

void app_diagnostics_init(
	struct app_diagnostics *diagnostics,
	unsigned int current_day,
	time_t timestamp
);
void app_diagnostics_mark_scan(
	struct app_diagnostics *diagnostics,
	unsigned int current_day,
	size_t deck_count,
	size_t ignored_count
);
void app_diagnostics_mark_deck_open(
	struct app_diagnostics *diagnostics,
	const char *deck_id,
	bool load_ok,
	bool review_screen,
	bool summary_screen
);
void app_diagnostics_mark_answer_shown(struct app_diagnostics *diagnostics);
void app_diagnostics_mark_rating_saved(struct app_diagnostics *diagnostics);
void app_diagnostics_mark_undo_saved(struct app_diagnostics *diagnostics);
void app_diagnostics_mark_suspend_saved(struct app_diagnostics *diagnostics);
void app_diagnostics_mark_restore_saved(
	struct app_diagnostics *diagnostics,
	unsigned int restored_count
);
void app_diagnostics_mark_settings_saved(struct app_diagnostics *diagnostics);
void app_diagnostics_mark_reset_progress(struct app_diagnostics *diagnostics);
void app_diagnostics_mark_exit_confirmed(struct app_diagnostics *diagnostics);
bool app_diagnostics_write(
	const char *path,
	const struct app_diagnostics *diagnostics
);

#endif
