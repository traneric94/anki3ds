#include "study_session.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STUDY_SESSION_LINE_SIZE 128
#define STUDY_SESSION_PATH_SIZE 512
#define STUDY_SESSION_HEADER "#anki3ds-session-v1"
#define STUDY_SESSION_FOOTER "#anki3ds-session-complete"
#define STUDY_SESSION_MAX_COUNTER 1000000u

struct study_session_values_seen
{
	bool started_at;
	bool updated_at;
	bool launch_count;
	bool started_day;
	bool current_day;
	bool scan_completed;
	bool deck_count;
	bool ignored_count;
	bool deck_open_count;
	bool review_screen_count;
	bool summary_screen_count;
	bool load_error_count;
	bool answer_shown_count;
	bool rating_saved_count;
	bool undo_saved_count;
	bool suspend_saved_count;
	bool restore_saved_count;
	bool settings_saved_count;
	bool reset_progress_count;
	bool exit_confirmed;
	bool last_deck_id;
	bool last_event;
};

const char *study_session_event_name(enum study_session_event event)
{
	switch (event)
	{
	case STUDY_SESSION_EVENT_BOOT:
		return "boot";
	case STUDY_SESSION_EVENT_SCAN:
		return "scan";
	case STUDY_SESSION_EVENT_DECK_OPEN:
		return "deck_open";
	case STUDY_SESSION_EVENT_DECK_REVIEW:
		return "deck_review";
	case STUDY_SESSION_EVENT_DECK_SUMMARY:
		return "deck_summary";
	case STUDY_SESSION_EVENT_DECK_LOAD_ERROR:
		return "deck_load_error";
	case STUDY_SESSION_EVENT_ANSWER_SHOWN:
		return "answer_shown";
	case STUDY_SESSION_EVENT_RATING_SAVED:
		return "rating_saved";
	case STUDY_SESSION_EVENT_UNDO_SAVED:
		return "undo_saved";
	case STUDY_SESSION_EVENT_SUSPEND_SAVED:
		return "suspend_saved";
	case STUDY_SESSION_EVENT_RESTORE_SAVED:
		return "restore_saved";
	case STUDY_SESSION_EVENT_SETTINGS_SAVED:
		return "settings_saved";
	case STUDY_SESSION_EVENT_RESET_PROGRESS:
		return "reset_progress";
	case STUDY_SESSION_EVENT_EXIT_CONFIRMED:
		return "exit_confirmed";
	}

	return NULL;
}

static bool study_session_event_from_name(
	const char *name,
	enum study_session_event *event
)
{
	if (name == NULL || event == NULL)
		return false;

	for (
		enum study_session_event candidate = STUDY_SESSION_EVENT_BOOT;
		candidate <= STUDY_SESSION_EVENT_EXIT_CONFIRMED;
		candidate++
	)
	{
		const char *candidate_name = study_session_event_name(candidate);

		if (candidate_name != NULL && strcmp(name, candidate_name) == 0)
		{
			*event = candidate;
			return true;
		}
	}

	return false;
}

const char *study_session_result_name(enum study_session_result result)
{
	switch (result)
	{
	case STUDY_SESSION_OK:
		return "Session saved";
	case STUDY_SESSION_NOT_FOUND:
		return "New session";
	case STUDY_SESSION_BAD_FORMAT:
		return "Session ignored";
	case STUDY_SESSION_WRITE_FAILED:
		return "Session save failed";
	}

	return "Session error";
}

static void study_session_copy_deck_id(
	char *destination,
	size_t destination_size,
	const char *deck_id
)
{
	if (destination == NULL || destination_size == 0)
		return;

	snprintf(destination, destination_size, "%s", deck_id != NULL ? deck_id : "-");
	if (destination[0] == '\0')
		snprintf(destination, destination_size, "-");
}

void study_session_init(
	struct study_session *session,
	unsigned long timestamp,
	unsigned int day
)
{
	if (session == NULL)
		return;

	memset(session, 0, sizeof(*session));
	session->started_at = timestamp;
	session->updated_at = timestamp;
	session->started_day = day;
	session->current_day = day;
	study_session_copy_deck_id(
		session->last_deck_id,
		sizeof(session->last_deck_id),
		"-"
	);
	session->last_event = STUDY_SESSION_EVENT_BOOT;
}

static void study_session_touch(
	struct study_session *session,
	unsigned long timestamp,
	unsigned int day,
	enum study_session_event event,
	const char *deck_id
)
{
	if (session == NULL)
		return;

	session->updated_at = timestamp;
	session->current_day = day;
	session->last_event = event;
	if (deck_id != NULL)
	{
		study_session_copy_deck_id(
			session->last_deck_id,
			sizeof(session->last_deck_id),
			deck_id
		);
	}
}

void study_session_start_launch(
	struct study_session *session,
	unsigned long timestamp,
	unsigned int day
)
{
	if (session == NULL)
		return;
	if (session->started_at == 0)
		session->started_at = timestamp;
	if (session->started_day == 0)
		session->started_day = day;
	if (session->launch_count < STUDY_SESSION_MAX_COUNTER)
		session->launch_count++;
	session->scan_completed = false;
	session->exit_confirmed = false;
	study_session_touch(session, timestamp, day, STUDY_SESSION_EVENT_BOOT, NULL);
}

void study_session_record_scan(
	struct study_session *session,
	unsigned int deck_count,
	unsigned int ignored_count,
	unsigned long timestamp,
	unsigned int day
)
{
	if (session == NULL)
		return;

	session->scan_completed = true;
	session->deck_count = deck_count;
	session->ignored_count = ignored_count;
	study_session_touch(session, timestamp, day, STUDY_SESSION_EVENT_SCAN, NULL);
}

void study_session_record_deck_open(
	struct study_session *session,
	const char *deck_id,
	bool load_ok,
	bool has_active_card,
	unsigned long timestamp,
	unsigned int day
)
{
	if (session == NULL)
		return;

	if (session->deck_open_count < STUDY_SESSION_MAX_COUNTER)
		session->deck_open_count++;
	if (!load_ok)
	{
		if (session->load_error_count < STUDY_SESSION_MAX_COUNTER)
			session->load_error_count++;
		study_session_touch(
			session,
			timestamp,
			day,
			STUDY_SESSION_EVENT_DECK_LOAD_ERROR,
			deck_id
		);
		return;
	}
	if (has_active_card)
	{
		if (session->review_screen_count < STUDY_SESSION_MAX_COUNTER)
			session->review_screen_count++;
		study_session_touch(
			session,
			timestamp,
			day,
			STUDY_SESSION_EVENT_DECK_REVIEW,
			deck_id
		);
		return;
	}

	if (session->summary_screen_count < STUDY_SESSION_MAX_COUNTER)
		session->summary_screen_count++;
	study_session_touch(
		session,
		timestamp,
		day,
		STUDY_SESSION_EVENT_DECK_SUMMARY,
		deck_id
	);
}

void study_session_record_event(
	struct study_session *session,
	enum study_session_event event,
	const char *deck_id,
	unsigned long timestamp,
	unsigned int day
)
{
	if (session == NULL)
		return;

	switch (event)
	{
	case STUDY_SESSION_EVENT_ANSWER_SHOWN:
		if (session->answer_shown_count < STUDY_SESSION_MAX_COUNTER)
			session->answer_shown_count++;
		break;
	case STUDY_SESSION_EVENT_RATING_SAVED:
		if (session->rating_saved_count < STUDY_SESSION_MAX_COUNTER)
			session->rating_saved_count++;
		break;
	case STUDY_SESSION_EVENT_UNDO_SAVED:
		if (session->undo_saved_count < STUDY_SESSION_MAX_COUNTER)
			session->undo_saved_count++;
		break;
	case STUDY_SESSION_EVENT_SUSPEND_SAVED:
		if (session->suspend_saved_count < STUDY_SESSION_MAX_COUNTER)
			session->suspend_saved_count++;
		break;
	case STUDY_SESSION_EVENT_RESTORE_SAVED:
		if (session->restore_saved_count < STUDY_SESSION_MAX_COUNTER)
			session->restore_saved_count++;
		break;
	case STUDY_SESSION_EVENT_SETTINGS_SAVED:
		if (session->settings_saved_count < STUDY_SESSION_MAX_COUNTER)
			session->settings_saved_count++;
		break;
	case STUDY_SESSION_EVENT_RESET_PROGRESS:
		if (session->reset_progress_count < STUDY_SESSION_MAX_COUNTER)
			session->reset_progress_count++;
		break;
	case STUDY_SESSION_EVENT_EXIT_CONFIRMED:
		session->exit_confirmed = true;
		break;
	case STUDY_SESSION_EVENT_BOOT:
	case STUDY_SESSION_EVENT_SCAN:
	case STUDY_SESSION_EVENT_DECK_OPEN:
	case STUDY_SESSION_EVENT_DECK_REVIEW:
	case STUDY_SESSION_EVENT_DECK_SUMMARY:
	case STUDY_SESSION_EVENT_DECK_LOAD_ERROR:
		break;
	}

	study_session_touch(session, timestamp, day, event, deck_id);
}

bool study_session_rollover_day(
	struct study_session *session,
	unsigned long timestamp,
	unsigned int day
)
{
	if (session == NULL || session->current_day == day)
		return false;

	session->updated_at = timestamp;
	session->current_day = day;
	return true;
}

static bool study_session_parse_unsigned(
	const char *text,
	unsigned int maximum,
	unsigned int *value
)
{
	char *end = NULL;
	unsigned long parsed;

	if (text == NULL || text[0] == '\0' || value == NULL)
		return false;

	errno = 0;
	parsed = strtoul(text, &end, 10);
	if (errno != 0 || end == text || *end != '\0' || parsed > maximum)
		return false;

	*value = (unsigned int)parsed;
	return true;
}

static bool study_session_parse_ulong(
	const char *text,
	unsigned long *value
)
{
	char *end = NULL;
	unsigned long parsed;

	if (text == NULL || text[0] == '\0' || value == NULL)
		return false;

	errno = 0;
	parsed = strtoul(text, &end, 10);
	if (errno != 0 || end == text || *end != '\0')
		return false;

	*value = parsed;
	return true;
}

static bool study_session_parse_bool(const char *text, bool *value)
{
	unsigned int parsed;

	if (!study_session_parse_unsigned(text, 1, &parsed) || value == NULL)
		return false;

	*value = parsed != 0;
	return true;
}

static bool study_session_line_complete(FILE *file, const char *line)
{
	int value;

	if (line == NULL)
		return false;
	if (strchr(line, '\n') != NULL || strchr(line, '\r') != NULL)
		return true;
	if (feof(file))
		return true;

	while ((value = fgetc(file)) != EOF && value != '\n')
	{
	}
	return false;
}

static bool study_session_assign_value(
	struct study_session *session,
	struct study_session_values_seen *seen,
	const char *key,
	const char *value
)
{
	if (session == NULL || seen == NULL || key == NULL || value == NULL)
		return false;

#define ASSIGN_UNSIGNED(field, seen_field) \
	if (strcmp(key, #field) == 0) \
	{ \
		if (seen->seen_field) \
			return false; \
		seen->seen_field = true; \
		return study_session_parse_unsigned( \
			value, \
			STUDY_SESSION_MAX_COUNTER, \
			&session->field \
		); \
	}

	if (strcmp(key, "started_at") == 0)
	{
		if (seen->started_at)
			return false;
		seen->started_at = true;
		return study_session_parse_ulong(value, &session->started_at);
	}
	if (strcmp(key, "updated_at") == 0)
	{
		if (seen->updated_at)
			return false;
		seen->updated_at = true;
		return study_session_parse_ulong(value, &session->updated_at);
	}
	ASSIGN_UNSIGNED(launch_count, launch_count)
	ASSIGN_UNSIGNED(started_day, started_day)
	ASSIGN_UNSIGNED(current_day, current_day)
	if (strcmp(key, "scan_completed") == 0)
	{
		if (seen->scan_completed)
			return false;
		seen->scan_completed = true;
		return study_session_parse_bool(value, &session->scan_completed);
	}
	ASSIGN_UNSIGNED(deck_count, deck_count)
	ASSIGN_UNSIGNED(ignored_count, ignored_count)
	ASSIGN_UNSIGNED(deck_open_count, deck_open_count)
	ASSIGN_UNSIGNED(review_screen_count, review_screen_count)
	ASSIGN_UNSIGNED(summary_screen_count, summary_screen_count)
	ASSIGN_UNSIGNED(load_error_count, load_error_count)
	ASSIGN_UNSIGNED(answer_shown_count, answer_shown_count)
	ASSIGN_UNSIGNED(rating_saved_count, rating_saved_count)
	ASSIGN_UNSIGNED(undo_saved_count, undo_saved_count)
	ASSIGN_UNSIGNED(suspend_saved_count, suspend_saved_count)
	ASSIGN_UNSIGNED(restore_saved_count, restore_saved_count)
	ASSIGN_UNSIGNED(settings_saved_count, settings_saved_count)
	ASSIGN_UNSIGNED(reset_progress_count, reset_progress_count)
	if (strcmp(key, "exit_confirmed") == 0)
	{
		if (seen->exit_confirmed)
			return false;
		seen->exit_confirmed = true;
		return study_session_parse_bool(value, &session->exit_confirmed);
	}
	if (strcmp(key, "last_deck_id") == 0)
	{
		if (seen->last_deck_id || value[0] == '\0')
			return false;
		seen->last_deck_id = true;
		study_session_copy_deck_id(
			session->last_deck_id,
			sizeof(session->last_deck_id),
			value
		);
		return true;
	}
	if (strcmp(key, "last_event") == 0)
	{
		if (seen->last_event)
			return false;
		seen->last_event = true;
		return study_session_event_from_name(value, &session->last_event);
	}

#undef ASSIGN_UNSIGNED

	return false;
}

static bool study_session_values_complete(
	const struct study_session_values_seen *seen
)
{
	return (
		seen != NULL &&
		seen->started_at &&
		seen->updated_at &&
		seen->launch_count &&
		seen->started_day &&
		seen->current_day &&
		seen->scan_completed &&
		seen->deck_count &&
		seen->ignored_count &&
		seen->deck_open_count &&
		seen->review_screen_count &&
		seen->summary_screen_count &&
		seen->load_error_count &&
		seen->answer_shown_count &&
		seen->rating_saved_count &&
		seen->undo_saved_count &&
		seen->suspend_saved_count &&
		seen->restore_saved_count &&
		seen->settings_saved_count &&
		seen->reset_progress_count &&
		seen->exit_confirmed &&
		seen->last_deck_id &&
		seen->last_event
	);
}

static bool study_session_values_consistent(
	const struct study_session *session
)
{
	unsigned int opened_screen_count;

	if (session == NULL)
		return false;

	if (
		session->last_event == STUDY_SESSION_EVENT_SCAN &&
		!session->scan_completed
	)
	{
		return false;
	}
	if (
		session->exit_confirmed !=
		(session->last_event == STUDY_SESSION_EVENT_EXIT_CONFIRMED)
	)
	{
		return false;
	}

	opened_screen_count =
		session->review_screen_count +
		session->summary_screen_count +
		session->load_error_count;
	if (session->deck_open_count == STUDY_SESSION_MAX_COUNTER)
		return opened_screen_count >= session->deck_open_count;

	return opened_screen_count == session->deck_open_count;
}

static bool study_session_artifact_path(
	char *destination,
	size_t destination_size,
	const char *path,
	const char *suffix
)
{
	int written;

	if (
		destination == NULL ||
		destination_size == 0 ||
		path == NULL ||
		path[0] == '\0' ||
		suffix == NULL
	)
	{
		return false;
	}

	written = snprintf(destination, destination_size, "%s%s", path, suffix);
	return written >= 0 && (size_t)written < destination_size;
}

static enum study_session_result study_session_load_tsv_single(
	struct study_session *session,
	const char *path
)
{
	FILE *file;
	char line[STUDY_SESSION_LINE_SIZE];
	struct study_session_values_seen seen;
	bool header_seen = false;
	bool footer_seen = false;

	if (session == NULL)
		return STUDY_SESSION_BAD_FORMAT;
	study_session_init(session, 0, 0);
	if (path == NULL || path[0] == '\0')
		return STUDY_SESSION_NOT_FOUND;

	file = fopen(path, "r");
	if (file == NULL)
		return STUDY_SESSION_NOT_FOUND;

	memset(&seen, 0, sizeof(seen));
	while (fgets(line, sizeof(line), file) != NULL)
	{
		char *separator;

		if (!study_session_line_complete(file, line))
		{
			fclose(file);
			study_session_init(session, 0, 0);
			return STUDY_SESSION_BAD_FORMAT;
		}
		line[strcspn(line, "\r\n")] = '\0';
		if (!header_seen)
		{
			if (strcmp(line, STUDY_SESSION_HEADER) != 0)
			{
				fclose(file);
				study_session_init(session, 0, 0);
				return STUDY_SESSION_BAD_FORMAT;
			}
			header_seen = true;
			continue;
		}
		if (strcmp(line, STUDY_SESSION_FOOTER) == 0)
		{
			footer_seen = true;
			break;
		}

		separator = strchr(line, '\t');
		if (separator == NULL)
		{
			fclose(file);
			study_session_init(session, 0, 0);
			return STUDY_SESSION_BAD_FORMAT;
		}
		*separator = '\0';
		if (!study_session_assign_value(session, &seen, line, separator + 1))
		{
			fclose(file);
			study_session_init(session, 0, 0);
			return STUDY_SESSION_BAD_FORMAT;
		}
	}

	if (ferror(file) || !header_seen || !footer_seen)
	{
		fclose(file);
		study_session_init(session, 0, 0);
		return STUDY_SESSION_BAD_FORMAT;
	}
	fclose(file);

	if (
		!study_session_values_complete(&seen) ||
		!study_session_values_consistent(session) ||
		session->updated_at < session->started_at ||
		session->current_day < session->started_day ||
		session->last_deck_id[0] == '\0'
	)
	{
		study_session_init(session, 0, 0);
		return STUDY_SESSION_BAD_FORMAT;
	}

	return STUDY_SESSION_OK;
}

enum study_session_result study_session_load_tsv(
	struct study_session *session,
	const char *path
)
{
	char artifact_path[STUDY_SESSION_PATH_SIZE];
	enum study_session_result primary_result;

	primary_result = study_session_load_tsv_single(session, path);
	if (primary_result == STUDY_SESSION_OK)
		return STUDY_SESSION_OK;
	if (path == NULL || path[0] == '\0')
		return primary_result;

	if (
		study_session_artifact_path(
			artifact_path,
			sizeof(artifact_path),
			path,
			".tmp"
		) &&
		study_session_load_tsv_single(session, artifact_path) == STUDY_SESSION_OK
	)
	{
		return STUDY_SESSION_OK;
	}
	if (
		study_session_artifact_path(
			artifact_path,
			sizeof(artifact_path),
			path,
			".bak"
		) &&
		study_session_load_tsv_single(session, artifact_path) == STUDY_SESSION_OK
	)
	{
		return STUDY_SESSION_OK;
	}

	return primary_result;
}

enum study_session_result study_session_load_or_init(
	struct study_session *session,
	const char *path,
	unsigned long timestamp,
	unsigned int day
)
{
	enum study_session_result result = study_session_load_tsv(session, path);

	if (result != STUDY_SESSION_OK)
		study_session_init(session, timestamp, day);

	return result;
}

static bool study_session_file_exists(const char *path);

static bool study_session_remove_if_present(const char *path)
{
	if (path == NULL || path[0] == '\0')
		return false;
	if (!study_session_file_exists(path))
		return true;

	errno = 0;
	if (remove(path) == 0)
		return true;

	return errno == ENOENT;
}

static bool study_session_file_exists(const char *path)
{
	FILE *file;

	if (path == NULL || path[0] == '\0')
		return false;

	file = fopen(path, "r");
	if (file == NULL)
		return false;

	return fclose(file) == 0;
}

static bool study_session_write_tsv_file(
	const struct study_session *session,
	const char *path
)
{
	FILE *file;
	const char *event_name;

	if (session == NULL || path == NULL || path[0] == '\0')
		return false;

	event_name = study_session_event_name(session->last_event);
	if (event_name == NULL || session->last_deck_id[0] == '\0')
		return false;

	file = fopen(path, "w");
	if (file == NULL)
		return false;

	if (
		fprintf(file, "%s\n", STUDY_SESSION_HEADER) < 0 ||
		fprintf(file, "started_at\t%lu\n", session->started_at) < 0 ||
		fprintf(file, "updated_at\t%lu\n", session->updated_at) < 0 ||
		fprintf(file, "launch_count\t%u\n", session->launch_count) < 0 ||
		fprintf(file, "started_day\t%u\n", session->started_day) < 0 ||
		fprintf(file, "current_day\t%u\n", session->current_day) < 0 ||
		fprintf(file, "scan_completed\t%u\n", session->scan_completed ? 1u : 0u) < 0 ||
		fprintf(file, "deck_count\t%u\n", session->deck_count) < 0 ||
		fprintf(file, "ignored_count\t%u\n", session->ignored_count) < 0 ||
		fprintf(file, "deck_open_count\t%u\n", session->deck_open_count) < 0 ||
		fprintf(file, "review_screen_count\t%u\n", session->review_screen_count) < 0 ||
		fprintf(file, "summary_screen_count\t%u\n", session->summary_screen_count) < 0 ||
		fprintf(file, "load_error_count\t%u\n", session->load_error_count) < 0 ||
		fprintf(file, "answer_shown_count\t%u\n", session->answer_shown_count) < 0 ||
		fprintf(file, "rating_saved_count\t%u\n", session->rating_saved_count) < 0 ||
		fprintf(file, "undo_saved_count\t%u\n", session->undo_saved_count) < 0 ||
		fprintf(file, "suspend_saved_count\t%u\n", session->suspend_saved_count) < 0 ||
		fprintf(file, "restore_saved_count\t%u\n", session->restore_saved_count) < 0 ||
		fprintf(file, "settings_saved_count\t%u\n", session->settings_saved_count) < 0 ||
		fprintf(file, "reset_progress_count\t%u\n", session->reset_progress_count) < 0 ||
		fprintf(file, "exit_confirmed\t%u\n", session->exit_confirmed ? 1u : 0u) < 0 ||
		fprintf(file, "last_deck_id\t%s\n", session->last_deck_id) < 0 ||
		fprintf(file, "last_event\t%s\n", event_name) < 0 ||
		fprintf(file, "%s\n", STUDY_SESSION_FOOTER) < 0
	)
	{
		fclose(file);
		return false;
	}
	if (fclose(file) != 0)
		return false;

	return true;
}

enum study_session_result study_session_save_tsv(
	const struct study_session *session,
	const char *path
)
{
	char tmp_path[STUDY_SESSION_PATH_SIZE];
	char bak_path[STUDY_SESSION_PATH_SIZE];
	bool backup_created = false;

	if (session == NULL || path == NULL || path[0] == '\0')
		return STUDY_SESSION_WRITE_FAILED;
	if (
		!study_session_artifact_path(
			tmp_path,
			sizeof(tmp_path),
			path,
			".tmp"
		) ||
		!study_session_artifact_path(
			bak_path,
			sizeof(bak_path),
			path,
			".bak"
		)
	)
	{
		return STUDY_SESSION_WRITE_FAILED;
	}

	if (!study_session_write_tsv_file(session, tmp_path))
		return STUDY_SESSION_WRITE_FAILED;
	if (!study_session_remove_if_present(bak_path))
	{
		(void)remove(tmp_path);
		return STUDY_SESSION_WRITE_FAILED;
	}
	if (study_session_file_exists(path))
	{
		if (rename(path, bak_path) != 0)
		{
			(void)remove(tmp_path);
			return STUDY_SESSION_WRITE_FAILED;
		}
		backup_created = true;
	}
	if (rename(tmp_path, path) != 0)
	{
		if (backup_created)
			(void)rename(bak_path, path);
		(void)remove(tmp_path);
		return STUDY_SESSION_WRITE_FAILED;
	}

	return STUDY_SESSION_OK;
}
