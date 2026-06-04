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

void app_settings_load_report_clear(struct app_settings_load_report *report)
{
	if (report == NULL)
		return;

	report->line_number = 0;
	report->parse_result = APP_SETTINGS_PARSE_OK;
}

static void app_settings_load_report_set(
	struct app_settings_load_report *report,
	unsigned int line_number,
	enum app_settings_parse_result parse_result
)
{
	if (report == NULL)
		return;

	report->line_number = line_number;
	report->parse_result = parse_result;
}

static void app_settings_remember_bad_report(
	struct app_settings_load_report *destination,
	bool *has_destination,
	const struct app_settings_load_report *source
)
{
	if (destination == NULL || has_destination == NULL || source == NULL)
		return;
	if (*has_destination)
		return;

	*destination = *source;
	*has_destination = true;
}

static bool parse_unsigned_field(const char *field, unsigned int max, unsigned int *value)
{
	char *end = NULL;
	unsigned long parsed;

	if (field == NULL || value == NULL || field[0] == '\0')
		return false;
	for (size_t index = 0; field[index] != '\0'; index++)
	{
		if (field[index] < '0' || field[index] > '9')
			return false;
	}

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

static enum app_settings_parse_result parse_settings_line(
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
		return APP_SETTINGS_PARSE_OK;
	if (!split_settings_line(line, fields, &field_count))
		return APP_SETTINGS_PARSE_BAD_FIELD_COUNT;
	if (field_count != SETTINGS_FIELD_COUNT)
		return APP_SETTINGS_PARSE_BAD_FIELD_COUNT;
	if (!parse_unsigned_field(fields[1], APP_SETTINGS_MAX_DAILY_LIMIT, &value))
		return APP_SETTINGS_PARSE_BAD_VALUE;

	if (strcmp(fields[0], "new_limit") == 0)
	{
		if (*read_new_limit)
			return APP_SETTINGS_PARSE_DUPLICATE_KEY;

		settings->new_limit = value;
		*read_new_limit = true;
		return APP_SETTINGS_PARSE_OK;
	}
	if (strcmp(fields[0], "review_limit") == 0)
	{
		if (*read_review_limit)
			return APP_SETTINGS_PARSE_DUPLICATE_KEY;

		settings->review_limit = value;
		*read_review_limit = true;
		return APP_SETTINGS_PARSE_OK;
	}

	return APP_SETTINGS_PARSE_UNKNOWN_KEY;
}

static enum app_settings_load_result app_settings_load_file(
	struct app_settings *settings,
	const char *path,
	bool *loaded_file,
	struct app_settings_load_report *report
)
{
	FILE *file;
	char line[SETTINGS_MAX_LINE_LENGTH];
	struct app_settings staged;
	bool read_new_limit = false;
	bool read_review_limit = false;
	unsigned int line_number = 0;

	app_settings_load_report_clear(report);
	app_settings_default(&staged);

	file = fopen(path, "r");
	if (file == NULL)
		return APP_SETTINGS_LOAD_NOT_FOUND;

	*loaded_file = true;

	while (fgets(line, sizeof(line), file) != NULL)
	{
		enum app_settings_parse_result parse_result;

		line_number++;
		if (line_needs_more_input(file, line))
		{
			consume_line_remainder(file);
			app_settings_load_report_set(
				report,
				line_number,
				APP_SETTINGS_PARSE_LINE_TOO_LONG
			);
			fclose(file);
			return APP_SETTINGS_LOAD_BAD_FORMAT;
		}

		parse_result = parse_settings_line(
			line,
			&staged,
			&read_new_limit,
			&read_review_limit
		);
		if (parse_result != APP_SETTINGS_PARSE_OK)
		{
			app_settings_load_report_set(report, line_number, parse_result);
			fclose(file);
			return APP_SETTINGS_LOAD_BAD_FORMAT;
		}
	}

	if (ferror(file))
	{
		app_settings_load_report_set(report, 0, APP_SETTINGS_PARSE_READ_ERROR);
		fclose(file);
		return APP_SETTINGS_LOAD_BAD_FORMAT;
	}
	if (!read_new_limit)
	{
		app_settings_load_report_set(
			report,
			0,
			APP_SETTINGS_PARSE_MISSING_NEW_LIMIT
		);
		fclose(file);
		return APP_SETTINGS_LOAD_BAD_FORMAT;
	}
	if (!read_review_limit)
	{
		app_settings_load_report_set(
			report,
			0,
			APP_SETTINGS_PARSE_MISSING_REVIEW_LIMIT
		);
		fclose(file);
		return APP_SETTINGS_LOAD_BAD_FORMAT;
	}

	fclose(file);
	*settings = staged;
	return APP_SETTINGS_LOAD_OK;
}

enum app_settings_load_result app_settings_load_with_report(
	struct app_settings *settings,
	const char *path,
	struct app_settings_load_report *report
)
{
	char temp_path[STORAGE_MAX_PATH_LENGTH];
	char backup_path[STORAGE_MAX_PATH_LENGTH];
	bool loaded_file = false;
	bool has_bad_report = false;
	bool has_temp_path;
	bool has_backup_path;
	bool primary_missing;
	enum app_settings_load_result result;
	struct app_settings_load_report bad_report;
	struct app_settings_load_report attempt_report;

	app_settings_load_report_clear(report);
	app_settings_load_report_clear(&bad_report);
	if (settings == NULL)
	{
		app_settings_load_report_set(report, 0, APP_SETTINGS_PARSE_READ_ERROR);
		return APP_SETTINGS_LOAD_BAD_FORMAT;
	}
	if (path == NULL)
	{
		app_settings_default(settings);
		app_settings_load_report_set(report, 0, APP_SETTINGS_PARSE_READ_ERROR);
		return APP_SETTINGS_LOAD_BAD_FORMAT;
	}

	result = app_settings_load_file(settings, path, &loaded_file, &attempt_report);
	if (result == APP_SETTINGS_LOAD_OK)
		return APP_SETTINGS_LOAD_OK;
	if (result == APP_SETTINGS_LOAD_BAD_FORMAT)
	{
		app_settings_remember_bad_report(
			&bad_report,
			&has_bad_report,
			&attempt_report
		);
	}
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
		result = app_settings_load_file(
			settings,
			temp_path,
			&loaded_file,
			&attempt_report
		);
		if (result == APP_SETTINGS_LOAD_OK)
		{
			if (!storage_promote_recovery_file(path, STORAGE_TEMP_SUFFIX))
			{
				app_settings_default(settings);
				app_settings_load_report_set(
					report,
					0,
					APP_SETTINGS_PARSE_RECOVERY_ERROR
				);
				return APP_SETTINGS_LOAD_BAD_FORMAT;
			}

			return APP_SETTINGS_LOAD_OK;
		}
		if (result == APP_SETTINGS_LOAD_BAD_FORMAT)
		{
			app_settings_remember_bad_report(
				&bad_report,
				&has_bad_report,
				&attempt_report
			);
		}
	}

	if (has_backup_path)
	{
		result = app_settings_load_file(
			settings,
			backup_path,
			&loaded_file,
			&attempt_report
		);
		if (result == APP_SETTINGS_LOAD_OK)
		{
			if (
				primary_missing &&
				!storage_promote_recovery_file(path, STORAGE_BACKUP_SUFFIX)
			)
			{
				app_settings_default(settings);
				app_settings_load_report_set(
					report,
					0,
					APP_SETTINGS_PARSE_RECOVERY_ERROR
				);
				return APP_SETTINGS_LOAD_BAD_FORMAT;
			}

			return APP_SETTINGS_LOAD_OK;
		}
		if (result == APP_SETTINGS_LOAD_BAD_FORMAT)
		{
			app_settings_remember_bad_report(
				&bad_report,
				&has_bad_report,
				&attempt_report
			);
		}
	}

	if (!primary_missing && has_temp_path)
	{
		result = app_settings_load_file(
			settings,
			temp_path,
			&loaded_file,
			&attempt_report
		);
		if (result == APP_SETTINGS_LOAD_OK)
			return APP_SETTINGS_LOAD_OK;
		if (result == APP_SETTINGS_LOAD_BAD_FORMAT)
		{
			app_settings_remember_bad_report(
				&bad_report,
				&has_bad_report,
				&attempt_report
			);
		}
	}

	app_settings_default(settings);
	if (!loaded_file)
		return APP_SETTINGS_LOAD_NOT_FOUND;

	if (has_bad_report && report != NULL)
		*report = bad_report;
	return APP_SETTINGS_LOAD_BAD_FORMAT;
}

enum app_settings_load_result app_settings_load(struct app_settings *settings, const char *path)
{
	return app_settings_load_with_report(settings, path, NULL);
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

const char *app_settings_parse_result_name(enum app_settings_parse_result result)
{
	switch (result)
	{
	case APP_SETTINGS_PARSE_OK:
		return "ok";
	case APP_SETTINGS_PARSE_LINE_TOO_LONG:
		return "line too long";
	case APP_SETTINGS_PARSE_BAD_FIELD_COUNT:
		return "bad field count";
	case APP_SETTINGS_PARSE_BAD_VALUE:
		return "bad value";
	case APP_SETTINGS_PARSE_UNKNOWN_KEY:
		return "unknown key";
	case APP_SETTINGS_PARSE_DUPLICATE_KEY:
		return "duplicate key";
	case APP_SETTINGS_PARSE_MISSING_NEW_LIMIT:
		return "missing new_limit";
	case APP_SETTINGS_PARSE_MISSING_REVIEW_LIMIT:
		return "missing review_limit";
	case APP_SETTINGS_PARSE_READ_ERROR:
		return "read error";
	case APP_SETTINGS_PARSE_RECOVERY_ERROR:
		return "recovery error";
	}

	return "unknown";
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
