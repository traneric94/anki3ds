#include "app_settings.h"

#include "storage.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SETTINGS_FIELD_COUNT 2
#define SETTINGS_MAX_LINE_LENGTH 96

void app_settings_default(struct app_settings *settings)
{
	settings->new_limit = APP_SETTINGS_DEFAULT_NEW_LIMIT;
	settings->review_limit = APP_SETTINGS_DEFAULT_REVIEW_LIMIT;
}

static bool parse_unsigned_field(const char *field, unsigned int max, unsigned int *value)
{
	char *end = NULL;
	unsigned long parsed;

	errno = 0;
	parsed = strtoul(field, &end, 10);

	if (errno != 0 || end == field || *end != '\0' || parsed > max)
		return false;

	*value = (unsigned int)parsed;
	return true;
}

static void trim_line_end(char *line)
{
	size_t length = strlen(line);

	while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
	{
		line[length - 1] = '\0';
		length--;
	}
}

static bool line_needs_more_input(FILE *file, const char *line)
{
	size_t length = strlen(line);

	if (length == 0)
		return false;
	if (line[length - 1] == '\n' || line[length - 1] == '\r')
		return false;

	return !feof(file);
}

static void consume_line_remainder(FILE *file)
{
	int value;

	do
	{
		value = fgetc(file);
	}
	while (value != EOF && value != '\n');
}

static bool split_settings_line(
	char *line,
	char *fields[SETTINGS_FIELD_COUNT],
	size_t *field_count
)
{
	size_t field_index = 0;
	char *field_start = line;

	for (char *cursor = line; ; cursor++)
	{
		if (*cursor == '\t' || *cursor == '\0')
		{
			char previous = *cursor;

			if (field_index >= SETTINGS_FIELD_COUNT)
				return false;

			*cursor = '\0';
			fields[field_index] = field_start;
			field_index++;

			if (previous == '\0')
				break;

			field_start = cursor + 1;
		}
	}

	*field_count = field_index;
	return true;
}

static bool parse_settings_line(
	char *line,
	struct app_settings *settings,
	bool *read_new_limit,
	bool *read_review_limit
)
{
	char *fields[SETTINGS_FIELD_COUNT];
	size_t field_count;
	unsigned int value;

	trim_line_end(line);

	if (line[0] == '\0' || line[0] == '#')
		return true;
	if (!split_settings_line(line, fields, &field_count))
		return false;
	if (field_count != SETTINGS_FIELD_COUNT)
		return false;
	if (!parse_unsigned_field(fields[1], APP_SETTINGS_MAX_DAILY_LIMIT, &value))
		return false;

	if (strcmp(fields[0], "new_limit") == 0)
	{
		if (*read_new_limit)
			return false;

		settings->new_limit = value;
		*read_new_limit = true;
		return true;
	}
	if (strcmp(fields[0], "review_limit") == 0)
	{
		if (*read_review_limit)
			return false;

		settings->review_limit = value;
		*read_review_limit = true;
		return true;
	}

	return false;
}

static enum app_settings_load_result app_settings_load_file(
	struct app_settings *settings,
	const char *path,
	bool *loaded_file
)
{
	FILE *file;
	char line[SETTINGS_MAX_LINE_LENGTH];
	struct app_settings staged;
	bool read_new_limit = false;
	bool read_review_limit = false;

	app_settings_default(&staged);

	file = fopen(path, "r");
	if (file == NULL)
		return APP_SETTINGS_LOAD_NOT_FOUND;

	*loaded_file = true;

	while (fgets(line, sizeof(line), file) != NULL)
	{
		if (line_needs_more_input(file, line))
		{
			consume_line_remainder(file);
			fclose(file);
			return APP_SETTINGS_LOAD_BAD_FORMAT;
		}

		if (!parse_settings_line(line, &staged, &read_new_limit, &read_review_limit))
		{
			fclose(file);
			return APP_SETTINGS_LOAD_BAD_FORMAT;
		}
	}

	if (ferror(file))
	{
		fclose(file);
		return APP_SETTINGS_LOAD_BAD_FORMAT;
	}
	if (!read_new_limit || !read_review_limit)
	{
		fclose(file);
		return APP_SETTINGS_LOAD_BAD_FORMAT;
	}

	fclose(file);
	*settings = staged;
	return APP_SETTINGS_LOAD_OK;
}

enum app_settings_load_result app_settings_load(struct app_settings *settings, const char *path)
{
	char temp_path[STORAGE_MAX_PATH_LENGTH];
	char backup_path[STORAGE_MAX_PATH_LENGTH];
	bool loaded_file = false;
	bool has_temp_path;
	bool has_backup_path;
	bool primary_missing;
	enum app_settings_load_result result;

	if (settings == NULL)
		return APP_SETTINGS_LOAD_BAD_FORMAT;
	if (path == NULL)
	{
		app_settings_default(settings);
		return APP_SETTINGS_LOAD_BAD_FORMAT;
	}

	result = app_settings_load_file(settings, path, &loaded_file);
	if (result == APP_SETTINGS_LOAD_OK)
		return APP_SETTINGS_LOAD_OK;
	primary_missing = result == APP_SETTINGS_LOAD_NOT_FOUND;
	has_temp_path = storage_build_suffixed_path(
		temp_path,
		sizeof(temp_path),
		path,
		STORAGE_TEMP_SUFFIX
	);
	has_backup_path = storage_build_suffixed_path(
		backup_path,
		sizeof(backup_path),
		path,
		STORAGE_BACKUP_SUFFIX
	);

	if (primary_missing && has_temp_path)
	{
		result = app_settings_load_file(settings, temp_path, &loaded_file);
		if (result == APP_SETTINGS_LOAD_OK)
			return APP_SETTINGS_LOAD_OK;
	}

	if (has_backup_path)
	{
		result = app_settings_load_file(settings, backup_path, &loaded_file);
		if (result == APP_SETTINGS_LOAD_OK)
			return APP_SETTINGS_LOAD_OK;
	}

	if (!primary_missing && has_temp_path)
	{
		result = app_settings_load_file(settings, temp_path, &loaded_file);
		if (result == APP_SETTINGS_LOAD_OK)
			return APP_SETTINGS_LOAD_OK;
	}

	app_settings_default(settings);
	if (!loaded_file)
		return APP_SETTINGS_LOAD_NOT_FOUND;

	return APP_SETTINGS_LOAD_BAD_FORMAT;
}

enum app_settings_save_result app_settings_save(
	const struct app_settings *settings,
	const char *path
)
{
	char temp_path[STORAGE_MAX_PATH_LENGTH];
	FILE *file;

	if (settings == NULL || path == NULL)
		return APP_SETTINGS_SAVE_FAILED;
	if (settings->new_limit > APP_SETTINGS_MAX_DAILY_LIMIT)
		return APP_SETTINGS_SAVE_FAILED;
	if (settings->review_limit > APP_SETTINGS_MAX_DAILY_LIMIT)
		return APP_SETTINGS_SAVE_FAILED;
	if (!storage_build_suffixed_path(
		temp_path,
		sizeof(temp_path),
		path,
		STORAGE_TEMP_SUFFIX
	))
	{
		return APP_SETTINGS_SAVE_FAILED;
	}

	file = fopen(temp_path, "w");
	if (file == NULL)
		return APP_SETTINGS_SAVE_FAILED;

	if (
		fprintf(
			file,
			"new_limit\t%u\nreview_limit\t%u\n",
			settings->new_limit,
			settings->review_limit
		) < 0
	)
	{
		fclose(file);
		remove(temp_path);
		return APP_SETTINGS_SAVE_FAILED;
	}

	if (fclose(file) != 0)
	{
		remove(temp_path);
		return APP_SETTINGS_SAVE_FAILED;
	}

	if (!storage_replace_file(path))
		return APP_SETTINGS_SAVE_FAILED;

	return APP_SETTINGS_SAVE_OK;
}

const char *app_settings_load_result_name(enum app_settings_load_result result)
{
	switch (result)
	{
	case APP_SETTINGS_LOAD_OK:
		return "loaded";
	case APP_SETTINGS_LOAD_NOT_FOUND:
		return "defaults";
	case APP_SETTINGS_LOAD_BAD_FORMAT:
		return "ignored";
	}

	return "unknown";
}

const char *app_settings_save_result_name(enum app_settings_save_result result)
{
	switch (result)
	{
	case APP_SETTINGS_SAVE_OK:
		return "settings saved";
	case APP_SETTINGS_SAVE_FAILED:
		return "settings save failed";
	}

	return "unknown";
}
