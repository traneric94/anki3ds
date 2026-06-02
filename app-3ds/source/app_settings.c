#include "app_settings.h"

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

static bool parse_settings_line(char *line, struct app_settings *settings)
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
		settings->new_limit = value;
		return true;
	}
	if (strcmp(fields[0], "review_limit") == 0)
	{
		settings->review_limit = value;
		return true;
	}

	return false;
}

enum app_settings_load_result app_settings_load(struct app_settings *settings, const char *path)
{
	FILE *file;
	char line[SETTINGS_MAX_LINE_LENGTH];
	struct app_settings staged;

	app_settings_default(&staged);

	if (path == NULL)
	{
		*settings = staged;
		return APP_SETTINGS_LOAD_BAD_FORMAT;
	}

	file = fopen(path, "r");
	if (file == NULL)
	{
		*settings = staged;
		return APP_SETTINGS_LOAD_NOT_FOUND;
	}

	while (fgets(line, sizeof(line), file) != NULL)
	{
		if (line_needs_more_input(file, line))
		{
			consume_line_remainder(file);
			fclose(file);
			*settings = staged;
			return APP_SETTINGS_LOAD_BAD_FORMAT;
		}

		if (!parse_settings_line(line, &staged))
		{
			fclose(file);
			app_settings_default(settings);
			return APP_SETTINGS_LOAD_BAD_FORMAT;
		}
	}

	if (ferror(file))
	{
		fclose(file);
		app_settings_default(settings);
		return APP_SETTINGS_LOAD_BAD_FORMAT;
	}

	fclose(file);
	*settings = staged;
	return APP_SETTINGS_LOAD_OK;
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
