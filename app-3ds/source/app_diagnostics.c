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
	diagnostics->launch_count = 1;
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

static void app_diagnostics_strip_newline(char *text)
{
	size_t length;

	if (text == NULL)
		return;

	length = strlen(text);
	while (
		length > 0 &&
		(text[length - 1] == '\n' || text[length - 1] == '\r')
	)
	{
		text[length - 1] = '\0';
		length--;
	}
}

static bool app_diagnostics_parse_unsigned(
	const char *text,
	unsigned int *value
)
{
	unsigned int parsed = 0;

	if (text == NULL || text[0] == '\0' || value == NULL)
		return false;

	for (size_t index = 0; text[index] != '\0'; index++)
	{
		unsigned int digit;

		if (text[index] < '0' || text[index] > '9')
			return false;

		digit = (unsigned int)(text[index] - '0');
		if (parsed > (4294967295u - digit) / 10u)
			return false;
		parsed = parsed * 10u + digit;
	}

	*value = parsed;
	return true;
}

static bool app_diagnostics_parse_time(const char *text, time_t *value)
{
	unsigned int parsed;

	if (value == NULL)
		return false;
	if (!app_diagnostics_parse_unsigned(text, &parsed))
		return false;

	*value = (time_t)parsed;
	return true;
}

static bool app_diagnostics_parse_bool(const char *text, bool *value)
{
	unsigned int parsed;

	if (value == NULL)
		return false;
	if (!app_diagnostics_parse_unsigned(text, &parsed))
		return false;
	if (parsed > 1)
		return false;

	*value = parsed != 0;
	return true;
}

static bool app_diagnostics_read_key_value(
	char *row,
	char **key,
	char **value
)
{
	char *separator;

	if (row == NULL || key == NULL || value == NULL)
		return false;

	app_diagnostics_strip_newline(row);
	separator = strchr(row, '\t');
	if (separator == NULL)
		return false;

	*separator = '\0';
	*key = row;
	*value = separator + 1;
	return (*key)[0] != '\0' && (*value)[0] != '\0';
}

static bool app_diagnostics_apply_field(
	struct app_diagnostics *diagnostics,
	const char *key,
	const char *value
)
{
	if (diagnostics == NULL || key == NULL || value == NULL)
		return false;

	if (strcmp(key, "started_at") == 0)
		return app_diagnostics_parse_time(value, &diagnostics->started_at);
	if (strcmp(key, "updated_at") == 0)
		return app_diagnostics_parse_time(value, &diagnostics->updated_at);
	if (strcmp(key, "launch_count") == 0)
		return app_diagnostics_parse_unsigned(value, &diagnostics->launch_count);
	if (strcmp(key, "started_day") == 0)
		return app_diagnostics_parse_unsigned(value, &diagnostics->started_day);
	if (strcmp(key, "current_day") == 0)
		return app_diagnostics_parse_unsigned(value, &diagnostics->current_day);
	if (strcmp(key, "scan_completed") == 0)
		return app_diagnostics_parse_bool(value, &diagnostics->scan_completed);
	if (strcmp(key, "deck_count") == 0)
		return app_diagnostics_parse_unsigned(value, &diagnostics->deck_count);
	if (strcmp(key, "ignored_count") == 0)
		return app_diagnostics_parse_unsigned(value, &diagnostics->ignored_count);
	if (strcmp(key, "deck_open_count") == 0)
		return app_diagnostics_parse_unsigned(value, &diagnostics->deck_open_count);
	if (strcmp(key, "review_screen_count") == 0)
	{
		return app_diagnostics_parse_unsigned(
			value,
			&diagnostics->review_screen_count
		);
	}
	if (strcmp(key, "summary_screen_count") == 0)
	{
		return app_diagnostics_parse_unsigned(
			value,
			&diagnostics->summary_screen_count
		);
	}
	if (strcmp(key, "load_error_count") == 0)
		return app_diagnostics_parse_unsigned(value, &diagnostics->load_error_count);
	if (strcmp(key, "answer_shown_count") == 0)
	{
		return app_diagnostics_parse_unsigned(
			value,
			&diagnostics->answer_shown_count
		);
	}
	if (strcmp(key, "rating_saved_count") == 0)
		return app_diagnostics_parse_unsigned(value, &diagnostics->rating_saved_count);
	if (strcmp(key, "undo_saved_count") == 0)
		return app_diagnostics_parse_unsigned(value, &diagnostics->undo_saved_count);
	if (strcmp(key, "suspend_saved_count") == 0)
	{
		return app_diagnostics_parse_unsigned(
			value,
			&diagnostics->suspend_saved_count
		);
	}
	if (strcmp(key, "restore_saved_count") == 0)
	{
		return app_diagnostics_parse_unsigned(
			value,
			&diagnostics->restore_saved_count
		);
	}
	if (strcmp(key, "settings_saved_count") == 0)
	{
		return app_diagnostics_parse_unsigned(
			value,
			&diagnostics->settings_saved_count
		);
	}
	if (strcmp(key, "reset_progress_count") == 0)
	{
		return app_diagnostics_parse_unsigned(
			value,
			&diagnostics->reset_progress_count
		);
	}
	if (strcmp(key, "exit_confirmed") == 0)
		return app_diagnostics_parse_bool(value, &diagnostics->exit_confirmed);
	if (strcmp(key, "last_deck_id") == 0)
	{
		app_diagnostics_copy_clean(
			diagnostics->last_deck_id,
			sizeof(diagnostics->last_deck_id),
			value,
			"-"
		);
		return true;
	}
	if (strcmp(key, "last_event") == 0)
	{
		app_diagnostics_copy_clean(
			diagnostics->last_event,
			sizeof(diagnostics->last_event),
			value,
			"unknown"
		);
		return true;
	}

	return true;
}

bool app_diagnostics_load(
	const char *path,
	struct app_diagnostics *diagnostics
)
{
	FILE *file;
	char row[128];
	bool footer_seen = false;
	struct app_diagnostics loaded;

	if (path == NULL || diagnostics == NULL)
		return false;

	file = fopen(path, "rb");
	if (file == NULL)
		return false;

	if (fgets(row, sizeof(row), file) == NULL)
	{
		fclose(file);
		return false;
	}
	app_diagnostics_strip_newline(row);
	if (strcmp(row, APP_DIAGNOSTICS_HEADER) != 0)
	{
		fclose(file);
		return false;
	}

	loaded = *diagnostics;
	while (fgets(row, sizeof(row), file) != NULL)
	{
		char *key;
		char *value;

		if (strchr(row, '\n') == NULL && !feof(file))
		{
			fclose(file);
			return false;
		}
		app_diagnostics_strip_newline(row);
		if (strcmp(row, APP_DIAGNOSTICS_FOOTER) == 0)
		{
			footer_seen = true;
			break;
		}
		if (!app_diagnostics_read_key_value(row, &key, &value))
		{
			fclose(file);
			return false;
		}
		if (!app_diagnostics_apply_field(&loaded, key, value))
		{
			fclose(file);
			return false;
		}
	}

	if (ferror(file))
	{
		fclose(file);
		return false;
	}
	if (fclose(file) != 0)
		return false;
	if (!footer_seen)
		return false;
	if (loaded.launch_count == 0)
		loaded.launch_count = 1;

	*diagnostics = loaded;
	return true;
}

void app_diagnostics_mark_launch(
	struct app_diagnostics *diagnostics,
	unsigned int current_day,
	time_t timestamp
)
{
	if (diagnostics == NULL)
		return;

	if (timestamp == (time_t)-1)
		timestamp = 0;
	if (diagnostics->started_at == 0)
		diagnostics->started_at = timestamp;
	if (diagnostics->started_day == 0)
		diagnostics->started_day = current_day;
	if (diagnostics->launch_count < 4294967295u)
		diagnostics->launch_count++;
	diagnostics->current_day = current_day;
	diagnostics->updated_at = timestamp;
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
	ok = ok && app_diagnostics_write_unsigned(file, "launch_count", diagnostics->launch_count);
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
