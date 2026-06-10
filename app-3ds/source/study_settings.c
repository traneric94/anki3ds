#include "study_settings.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STUDY_SETTINGS_LINE_SIZE 128
#define STUDY_SETTINGS_PATH_SIZE 512

static const unsigned int study_settings_limit_presets[] = {
	5,
	10,
	20,
	50,
	100,
	200,
	500,
	1000,
	0,
};

struct study_settings_values
{
	unsigned long new_limit;
	unsigned long review_limit;
	unsigned long learning_mode;
	bool seen_new_limit;
	bool seen_review_limit;
	bool seen_learning_mode;
};

void study_settings_defaults(struct study_settings *settings)
{
	if (settings == NULL)
		return;

	settings->new_limit = STUDY_SETTINGS_DEFAULT_NEW_LIMIT;
	settings->review_limit = STUDY_SETTINGS_DEFAULT_REVIEW_LIMIT;
	settings->learning_mode = STUDY_SETTINGS_DEFAULT_LEARNING_MODE;
}

unsigned int study_settings_next_limit_preset(
	unsigned int current,
	bool increase
)
{
	size_t preset_count =
		sizeof(study_settings_limit_presets) /
		sizeof(study_settings_limit_presets[0]);

	for (size_t index = 0; index < preset_count; index++)
	{
		if (study_settings_limit_presets[index] != current)
			continue;

		if (increase)
			return study_settings_limit_presets[(index + 1) % preset_count];
		if (index == 0)
			return study_settings_limit_presets[preset_count - 1];

		return study_settings_limit_presets[index - 1];
	}

	if (increase)
	{
		for (size_t index = 0; index < preset_count; index++)
		{
			unsigned int preset = study_settings_limit_presets[index];

			if (preset != 0 && preset > current)
				return preset;
		}
		return 0;
	}

	for (size_t index = preset_count; index > 0; index--)
	{
		unsigned int preset = study_settings_limit_presets[index - 1];

		if (preset != 0 && preset < current)
			return preset;
	}

	return 0;
}

enum study_settings_learning_mode study_settings_next_learning_mode(
	enum study_settings_learning_mode current
)
{
	if (current == STUDY_SETTINGS_LEARNING_DUE_FIRST)
		return STUDY_SETTINGS_LEARNING_CARD_COOLDOWN;

	return STUDY_SETTINGS_LEARNING_DUE_FIRST;
}

bool study_settings_learning_mode_is_valid(
	enum study_settings_learning_mode mode
)
{
	return (
		mode == STUDY_SETTINGS_LEARNING_DUE_FIRST ||
		mode == STUDY_SETTINGS_LEARNING_CARD_COOLDOWN
	);
}

const char *study_settings_learning_mode_label(
	enum study_settings_learning_mode mode
)
{
	switch (mode)
	{
	case STUDY_SETTINGS_LEARNING_DUE_FIRST:
		return "Due first";
	case STUDY_SETTINGS_LEARNING_CARD_COOLDOWN:
		return "Cooldown";
	}

	return "Due first";
}

bool study_settings_new_limit_blocks_reveal(
	unsigned int new_limit,
	unsigned int introduced_count,
	bool current_card_introduced
)
{
	return (
		new_limit != 0 &&
		!current_card_introduced &&
		introduced_count >= new_limit
	);
}

bool study_settings_review_limit_blocks_rating(
	unsigned int review_limit,
	unsigned int reviewed_count
)
{
	return review_limit != 0 && reviewed_count >= review_limit;
}

const char *study_settings_result_name(enum study_settings_result result)
{
	switch (result)
	{
	case STUDY_SETTINGS_OK:
		return "Settings loaded";
	case STUDY_SETTINGS_NOT_FOUND:
		return "Default settings";
	case STUDY_SETTINGS_BAD_FORMAT:
		return "Settings ignored";
	case STUDY_SETTINGS_WRITE_FAILED:
		return "Settings save failed";
	}

	return "Settings error";
}

static bool study_settings_line_complete(FILE *file, const char *line)
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

static bool study_settings_parse_unsigned(
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
	if (parsed > STUDY_SETTINGS_MAX_LIMIT)
		return false;

	*value = parsed;
	return true;
}

static bool study_settings_assign_value(
	struct study_settings_values *values,
	const char *key,
	unsigned long value
)
{
	if (values == NULL || key == NULL)
		return false;

	if (strcmp(key, "new_limit") == 0)
	{
		if (values->seen_new_limit)
			return false;

		values->new_limit = value;
		values->seen_new_limit = true;
		return true;
	}
	if (strcmp(key, "review_limit") == 0)
	{
		if (values->seen_review_limit)
			return false;

		values->review_limit = value;
		values->seen_review_limit = true;
		return true;
	}
	if (strcmp(key, "learning_mode") == 0)
	{
		if (
			values->seen_learning_mode ||
			value > STUDY_SETTINGS_LEARNING_CARD_COOLDOWN
		)
		{
			return false;
		}

		values->learning_mode = value;
		values->seen_learning_mode = true;
		return true;
	}

	return false;
}

static bool study_settings_artifact_path(
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

static enum study_settings_result study_settings_load_tsv_single(
	struct study_settings *settings,
	const char *path
)
{
	FILE *file;
	char line[STUDY_SETTINGS_LINE_SIZE];
	struct study_settings_values values;

	if (settings == NULL)
		return STUDY_SETTINGS_BAD_FORMAT;

	study_settings_defaults(settings);
	if (path == NULL || path[0] == '\0')
		return STUDY_SETTINGS_NOT_FOUND;

	file = fopen(path, "r");
	if (file == NULL)
		return STUDY_SETTINGS_NOT_FOUND;

	memset(&values, 0, sizeof(values));
	while (fgets(line, sizeof(line), file) != NULL)
	{
		char *separator;
		unsigned long value;

		if (!study_settings_line_complete(file, line))
		{
			fclose(file);
			study_settings_defaults(settings);
			return STUDY_SETTINGS_BAD_FORMAT;
		}

		line[strcspn(line, "\r\n")] = '\0';
		if (line[0] == '\0' || line[0] == '#')
			continue;

		separator = strchr(line, '\t');
		if (separator == NULL)
		{
			fclose(file);
			study_settings_defaults(settings);
			return STUDY_SETTINGS_BAD_FORMAT;
		}
		*separator = '\0';
		if (!study_settings_parse_unsigned(separator + 1, &value))
		{
			fclose(file);
			study_settings_defaults(settings);
			return STUDY_SETTINGS_BAD_FORMAT;
		}
		if (!study_settings_assign_value(&values, line, value))
		{
			fclose(file);
			study_settings_defaults(settings);
			return STUDY_SETTINGS_BAD_FORMAT;
		}
	}

	if (ferror(file))
	{
		fclose(file);
		study_settings_defaults(settings);
		return STUDY_SETTINGS_BAD_FORMAT;
	}
	fclose(file);

	if (!values.seen_new_limit || !values.seen_review_limit)
	{
		study_settings_defaults(settings);
		return STUDY_SETTINGS_BAD_FORMAT;
	}

	settings->new_limit = (unsigned int)values.new_limit;
	settings->review_limit = (unsigned int)values.review_limit;
	settings->learning_mode = values.seen_learning_mode ?
		(enum study_settings_learning_mode)values.learning_mode :
		STUDY_SETTINGS_DEFAULT_LEARNING_MODE;
	return STUDY_SETTINGS_OK;
}

enum study_settings_result study_settings_load_tsv(
	struct study_settings *settings,
	const char *path
)
{
	char artifact_path[STUDY_SETTINGS_PATH_SIZE];
	enum study_settings_result primary_result;

	primary_result = study_settings_load_tsv_single(settings, path);
	if (primary_result == STUDY_SETTINGS_OK)
		return STUDY_SETTINGS_OK;
	if (path == NULL || path[0] == '\0')
		return primary_result;

	if (
		study_settings_artifact_path(
			artifact_path,
			sizeof(artifact_path),
			path,
			".tmp"
		) &&
		study_settings_load_tsv_single(
			settings,
			artifact_path
		) == STUDY_SETTINGS_OK
	)
	{
		return STUDY_SETTINGS_OK;
	}

	if (
		study_settings_artifact_path(
			artifact_path,
			sizeof(artifact_path),
			path,
			".bak"
		) &&
		study_settings_load_tsv_single(
			settings,
			artifact_path
		) == STUDY_SETTINGS_OK
	)
	{
		return STUDY_SETTINGS_OK;
	}

	return primary_result;
}

static bool study_settings_file_exists(const char *path);

static bool study_settings_remove_if_present(const char *path)
{
	if (path == NULL || path[0] == '\0')
		return false;
	if (!study_settings_file_exists(path))
		return true;

	errno = 0;
	if (remove(path) == 0)
		return true;

	return errno == ENOENT;
}

static bool study_settings_file_exists(const char *path)
{
	FILE *file;

	if (path == NULL || path[0] == '\0')
		return false;

	file = fopen(path, "r");
	if (file == NULL)
		return false;

	return fclose(file) == 0;
}

static bool study_settings_write_tsv_file(
	const struct study_settings *settings,
	const char *path
)
{
	FILE *file;

	file = fopen(path, "w");
	if (file == NULL)
		return false;
	if (
		fprintf(file, "new_limit\t%u\n", settings->new_limit) < 0 ||
		fprintf(file, "review_limit\t%u\n", settings->review_limit) < 0 ||
		fprintf(
			file,
			"learning_mode\t%u\n",
			(unsigned int)settings->learning_mode
		) < 0
	)
	{
		fclose(file);
		return false;
	}
	if (fclose(file) != 0)
		return false;

	return true;
}

enum study_settings_result study_settings_save_tsv(
	const struct study_settings *settings,
	const char *path
)
{
	char tmp_path[STUDY_SETTINGS_PATH_SIZE];
	char bak_path[STUDY_SETTINGS_PATH_SIZE];
	bool backup_created = false;

	if (settings == NULL || path == NULL || path[0] == '\0')
		return STUDY_SETTINGS_WRITE_FAILED;
	if (
		settings->new_limit > STUDY_SETTINGS_MAX_LIMIT ||
		settings->review_limit > STUDY_SETTINGS_MAX_LIMIT ||
		!study_settings_learning_mode_is_valid(settings->learning_mode)
	)
	{
		return STUDY_SETTINGS_WRITE_FAILED;
	}
	if (
		!study_settings_artifact_path(
			tmp_path,
			sizeof(tmp_path),
			path,
			".tmp"
		) ||
		!study_settings_artifact_path(
			bak_path,
			sizeof(bak_path),
			path,
			".bak"
		)
	)
	{
		return STUDY_SETTINGS_WRITE_FAILED;
	}

	if (!study_settings_write_tsv_file(settings, tmp_path))
		return STUDY_SETTINGS_WRITE_FAILED;
	if (!study_settings_remove_if_present(bak_path))
	{
		(void)remove(tmp_path);
		return STUDY_SETTINGS_WRITE_FAILED;
	}
	if (study_settings_file_exists(path))
	{
		if (rename(path, bak_path) != 0)
		{
			(void)remove(tmp_path);
			return STUDY_SETTINGS_WRITE_FAILED;
		}
		backup_created = true;
	}

	if (rename(tmp_path, path) != 0)
	{
		if (backup_created)
			(void)rename(bak_path, path);
		(void)remove(tmp_path);
		return STUDY_SETTINGS_WRITE_FAILED;
	}

	return STUDY_SETTINGS_OK;
}
