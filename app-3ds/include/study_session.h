#ifndef ANKI3DS_STUDY_SESSION_H
#define ANKI3DS_STUDY_SESSION_H

#include <stdbool.h>

#define STUDY_SESSION_PATH "sdmc:/3ds/anki3ds/session.tsv"
#define STUDY_SESSION_DECK_ID_SIZE 64

enum study_session_event
{
	STUDY_SESSION_EVENT_BOOT,
	STUDY_SESSION_EVENT_SCAN,
	STUDY_SESSION_EVENT_DECK_OPEN,
	STUDY_SESSION_EVENT_DECK_REVIEW,
	STUDY_SESSION_EVENT_DECK_SUMMARY,
	STUDY_SESSION_EVENT_DECK_LOAD_ERROR,
	STUDY_SESSION_EVENT_ANSWER_SHOWN,
	STUDY_SESSION_EVENT_RATING_SAVED,
	STUDY_SESSION_EVENT_UNDO_SAVED,
	STUDY_SESSION_EVENT_SUSPEND_SAVED,
	STUDY_SESSION_EVENT_RESTORE_SAVED,
	STUDY_SESSION_EVENT_SETTINGS_SAVED,
	STUDY_SESSION_EVENT_RESET_PROGRESS,
	STUDY_SESSION_EVENT_EXIT_CONFIRMED,
};

enum study_session_result
{
	STUDY_SESSION_OK,
	STUDY_SESSION_NOT_FOUND,
	STUDY_SESSION_BAD_FORMAT,
	STUDY_SESSION_WRITE_FAILED,
};

struct study_session
{
	unsigned long started_at;
	unsigned long updated_at;
	unsigned int launch_count;
	unsigned int started_day;
	unsigned int current_day;
	bool scan_completed;
	bool exit_confirmed;
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
	char last_deck_id[STUDY_SESSION_DECK_ID_SIZE];
	enum study_session_event last_event;
};

void study_session_init(
	struct study_session *session,
	unsigned long timestamp,
	unsigned int day
);
enum study_session_result study_session_load_tsv(
	struct study_session *session,
	const char *path
);
enum study_session_result study_session_save_tsv(
	const struct study_session *session,
	const char *path
);
void study_session_start_launch(
	struct study_session *session,
	unsigned long timestamp,
	unsigned int day
);
void study_session_record_scan(
	struct study_session *session,
	unsigned int deck_count,
	unsigned int ignored_count,
	unsigned long timestamp,
	unsigned int day
);
void study_session_record_deck_open(
	struct study_session *session,
	const char *deck_id,
	bool load_ok,
	bool has_active_card,
	unsigned long timestamp,
	unsigned int day
);
void study_session_record_event(
	struct study_session *session,
	enum study_session_event event,
	const char *deck_id,
	unsigned long timestamp,
	unsigned int day
);
bool study_session_rollover_day(
	struct study_session *session,
	unsigned long timestamp,
	unsigned int day
);
const char *study_session_event_name(enum study_session_event event);
enum study_session_result study_session_load_or_init(
	struct study_session *session,
	const char *path,
	unsigned long timestamp,
	unsigned int day
);
const char *study_session_result_name(enum study_session_result result);

#endif
