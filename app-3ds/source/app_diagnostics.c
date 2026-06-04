#include "app_diagnostics.h"

#include "storage.h"

#include <stdio.h>
#include <string.h>

#define APP_DIAGNOSTICS_HEADER "#anki3ds-session-v1"
#define APP_DIAGNOSTICS_FOOTER "#anki3ds-session-complete"

static time_t app_diagnostics_current_timestamp(void)
{
	time_t timestamp = time(NULL);

	return timestamp == (time_t)-1 ? 0 : timestamp;
}

static void app_diagnostics_copy_clean(
	char *destination,
	size_t destination_size,
	const char *source,
	const char *fallback
)
{
	size_t index = 0;

	if (destination_size == 0)
		return;

	if (source == NULL || source[0] == '\0')
		source = fallback;

	while (
		source != NULL &&
		source[index] != '\0' &&
		source[index] != '\t' &&
		source[index] != '\n' &&
		source[index] != '\r' &&
		index + 1 < destination_size
	)
	{
		destination[index] = source[index];
		index++;
	}

	destination[index] = '\0';
	if (destination[0] == '\0' && fallback != NULL && fallback[0] != '\0')
		snprintf(destination, destination_size, "%s", fallback);
}

static void app_diagnostics_touch(
	struct app_diagnostics *diagnostics,
	const char *event_name
)
{
	if (diagnostics == NULL)
		return;

	diagnostics->updated_at = app_diagnostics_current_timestamp();
	app_diagnostics_copy_clean(
		diagnostics->last_event,
		sizeof(diagnostics->last_event),
		event_name,
		"unknown"
	);
}

void app_diagnostics_init(
	struct app_diagnostics *diagnostics,
	unsigned int current_day,
	time_t timestamp
)
{
	if (diagnostics == NULL)
		return;

	memset(diagnostics, 0, sizeof(*diagnostics));
	if (timestamp == (time_t)-1)
		timestamp = 0;
	diagnostics->started_at = timestamp;
	diagnostics->updated_at = timestamp;
	diagnostics->started_day = current_day;
	diagnostics->current_day = current_day;
	app_diagnostics_copy_clean(
		diagnostics->last_deck_id,
		sizeof(diagnostics->last_deck_id),
		"-",
		"-"
	);
	app_diagnostics_copy_clean(
		diagnostics->last_event,
		sizeof(diagnostics->last_event),
		"boot",
		"boot"
	);
}

void app_diagnostics_mark_scan(
	struct app_diagnostics *diagnostics,
	unsigned int current_day,
	size_t deck_count,
	size_t ignored_count
)
{
	if (diagnostics == NULL)
		return;

	diagnostics->current_day = current_day;
	diagnostics->scan_completed = true;
	diagnostics->deck_count = (unsigned int)deck_count;
	diagnostics->ignored_count = (unsigned int)ignored_count;
	app_diagnostics_touch(diagnostics, "scan");
}

void app_diagnostics_mark_deck_open(
	struct app_diagnostics *diagnostics,
	const char *deck_id,
	bool load_ok,
	bool review_screen,
	bool summary_screen
)
{
	if (diagnostics == NULL)
		return;

	diagnostics->deck_open_count++;
	app_diagnostics_copy_clean(
		diagnostics->last_deck_id,
		sizeof(diagnostics->last_deck_id),
		deck_id,
		"-"
	);
	if (!load_ok)
	{
		diagnostics->load_error_count++;
		app_diagnostics_touch(diagnostics, "deck_load_error");
	}
	else if (review_screen)
	{
		diagnostics->review_screen_count++;
		app_diagnostics_touch(diagnostics, "deck_review");
	}
	else if (summary_screen)
	{
		diagnostics->summary_screen_count++;
		app_diagnostics_touch(diagnostics, "deck_summary");
	}
	else
	{
		app_diagnostics_touch(diagnostics, "deck_open");
	}
}

void app_diagnostics_mark_answer_shown(struct app_diagnostics *diagnostics)
{
	if (diagnostics == NULL)
		return;

	diagnostics->answer_shown_count++;
	app_diagnostics_touch(diagnostics, "answer_shown");
}

void app_diagnostics_mark_rating_saved(struct app_diagnostics *diagnostics)
{
	if (diagnostics == NULL)
		return;

	diagnostics->rating_saved_count++;
	app_diagnostics_touch(diagnostics, "rating_saved");
}

void app_diagnostics_mark_undo_saved(struct app_diagnostics *diagnostics)
{
	if (diagnostics == NULL)
		return;

	diagnostics->undo_saved_count++;
	app_diagnostics_touch(diagnostics, "undo_saved");
}

void app_diagnostics_mark_suspend_saved(struct app_diagnostics *diagnostics)
{
	if (diagnostics == NULL)
		return;

	diagnostics->suspend_saved_count++;
	app_diagnostics_touch(diagnostics, "suspend_saved");
}

void app_diagnostics_mark_restore_saved(
	struct app_diagnostics *diagnostics,
	unsigned int restored_count
)
{
	if (diagnostics == NULL)
		return;

	diagnostics->restore_saved_count += restored_count;
	app_diagnostics_touch(diagnostics, "restore_saved");
}

void app_diagnostics_mark_settings_saved(struct app_diagnostics *diagnostics)
{
	if (diagnostics == NULL)
		return;

	diagnostics->settings_saved_count++;
	app_diagnostics_touch(diagnostics, "settings_saved");
}

void app_diagnostics_mark_reset_progress(struct app_diagnostics *diagnostics)
{
	if (diagnostics == NULL)
		return;

	diagnostics->reset_progress_count++;
	app_diagnostics_touch(diagnostics, "reset_progress");
}

void app_diagnostics_mark_exit_confirmed(struct app_diagnostics *diagnostics)
{
	if (diagnostics == NULL)
		return;

	diagnostics->exit_confirmed = true;
	app_diagnostics_touch(diagnostics, "exit_confirmed");
}

static bool app_diagnostics_write_header(FILE *file)
{
	return fprintf(file, "%s\n", APP_DIAGNOSTICS_HEADER) > 0;
}

static bool app_diagnostics_write_footer(FILE *file)
{
	return fprintf(file, "%s\n", APP_DIAGNOSTICS_FOOTER) > 0;
}

static bool app_diagnostics_write_unsigned(
	FILE *file,
	const char *key,
	unsigned int value
)
{
	return fprintf(file, "%s\t%u\n", key, value) > 0;
}

static bool app_diagnostics_write_time(
	FILE *file,
	const char *key,
	time_t value
)
{
	return fprintf(file, "%s\t%ld\n", key, (long)value) > 0;
}

static bool app_diagnostics_write_bool(FILE *file, const char *key, bool value)
{
	return fprintf(file, "%s\t%u\n", key, value ? 1u : 0u) > 0;
}

static bool app_diagnostics_write_text(
	FILE *file,
	const char *key,
	const char *value
)
{
	return fprintf(file, "%s\t%s\n", key, value) > 0;
}

bool app_diagnostics_write(
	const char *path,
	const struct app_diagnostics *diagnostics
)
{
	char temp_path[STORAGE_MAX_PATH_LENGTH];
	FILE *file;
	bool ok = true;

	if (path == NULL || diagnostics == NULL)
		return false;
	if (
		!storage_build_suffixed_path(
			temp_path,
			sizeof(temp_path),
			path,
			STORAGE_TEMP_SUFFIX
		)
	)
	{
		return false;
	}

	file = fopen(temp_path, "wb");
	if (file == NULL)
		return false;

	ok = ok && app_diagnostics_write_header(file);
	ok = ok && app_diagnostics_write_time(file, "started_at", diagnostics->started_at);
	ok = ok && app_diagnostics_write_time(file, "updated_at", diagnostics->updated_at);
	ok = ok && app_diagnostics_write_unsigned(file, "started_day", diagnostics->started_day);
	ok = ok && app_diagnostics_write_unsigned(file, "current_day", diagnostics->current_day);
	ok = ok && app_diagnostics_write_bool(file, "scan_completed", diagnostics->scan_completed);
	ok = ok && app_diagnostics_write_unsigned(file, "deck_count", diagnostics->deck_count);
	ok = ok && app_diagnostics_write_unsigned(file, "ignored_count", diagnostics->ignored_count);
	ok = ok && app_diagnostics_write_unsigned(file, "deck_open_count", diagnostics->deck_open_count);
	ok = ok && app_diagnostics_write_unsigned(file, "review_screen_count", diagnostics->review_screen_count);
	ok = ok && app_diagnostics_write_unsigned(file, "summary_screen_count", diagnostics->summary_screen_count);
	ok = ok && app_diagnostics_write_unsigned(file, "load_error_count", diagnostics->load_error_count);
	ok = ok && app_diagnostics_write_unsigned(file, "answer_shown_count", diagnostics->answer_shown_count);
	ok = ok && app_diagnostics_write_unsigned(file, "rating_saved_count", diagnostics->rating_saved_count);
	ok = ok && app_diagnostics_write_unsigned(file, "undo_saved_count", diagnostics->undo_saved_count);
	ok = ok && app_diagnostics_write_unsigned(file, "suspend_saved_count", diagnostics->suspend_saved_count);
	ok = ok && app_diagnostics_write_unsigned(file, "restore_saved_count", diagnostics->restore_saved_count);
	ok = ok && app_diagnostics_write_unsigned(file, "settings_saved_count", diagnostics->settings_saved_count);
	ok = ok && app_diagnostics_write_unsigned(file, "reset_progress_count", diagnostics->reset_progress_count);
	ok = ok && app_diagnostics_write_bool(file, "exit_confirmed", diagnostics->exit_confirmed);
	ok = ok && app_diagnostics_write_text(file, "last_deck_id", diagnostics->last_deck_id);
	ok = ok && app_diagnostics_write_text(file, "last_event", diagnostics->last_event);
	ok = ok && app_diagnostics_write_footer(file);

	if (fclose(file) != 0)
		ok = false;
	if (!ok)
	{
		remove(temp_path);
		return false;
	}

	if (!storage_replace_file(path))
	{
		remove(temp_path);
		return false;
	}

	return true;
}
