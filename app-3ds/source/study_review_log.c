#include "study_review_log.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STUDY_REVIEW_LOG_PATH_SIZE 512
#define STUDY_REVIEW_LOG_ROW_SIZE 160

struct study_review_log_buffer
{
	char *data;
	size_t size;
};

const char *study_review_log_event_name(enum study_review_log_event event)
{
	switch (event)
	{
	case STUDY_REVIEW_LOG_EVENT_RATING:
		return "rating";
	case STUDY_REVIEW_LOG_EVENT_UNDO:
		return "undo";
	case STUDY_REVIEW_LOG_EVENT_SUSPEND:
		return "suspend";
	case STUDY_REVIEW_LOG_EVENT_RESTORE:
		return "restore";
	}

	return NULL;
}

const char *study_review_log_rating_name(enum study_review_log_rating rating)
{
	switch (rating)
	{
	case STUDY_REVIEW_LOG_RATING_NONE:
		return "-";
	case STUDY_REVIEW_LOG_RATING_AGAIN:
		return "again";
	case STUDY_REVIEW_LOG_RATING_HARD:
		return "hard";
	case STUDY_REVIEW_LOG_RATING_GOOD:
		return "good";
	case STUDY_REVIEW_LOG_RATING_EASY:
		return "easy";
	}

	return NULL;
}

static bool study_review_log_artifact_path(
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

static bool study_review_log_file_exists(const char *path);

static bool study_review_log_remove_if_present(const char *path)
{
	if (path == NULL || path[0] == '\0')
		return false;
	if (!study_review_log_file_exists(path))
		return true;

	errno = 0;
	if (remove(path) == 0)
		return true;

	return errno == ENOENT;
}

static bool study_review_log_file_exists(const char *path)
{
	FILE *file;

	if (path == NULL || path[0] == '\0')
		return false;

	file = fopen(path, "r");
	if (file == NULL)
		return false;

	return fclose(file) == 0;
}

static void study_review_log_free_buffer(
	struct study_review_log_buffer *buffer
)
{
	if (buffer == NULL)
		return;

	free(buffer->data);
	buffer->data = NULL;
	buffer->size = 0;
}

static bool study_review_log_read_file(
	const char *path,
	struct study_review_log_buffer *buffer
)
{
	FILE *file;
	long file_size;
	size_t bytes_read;

	if (buffer == NULL)
		return false;
	buffer->data = NULL;
	buffer->size = 0;
	if (path == NULL || path[0] == '\0')
		return false;

	file = fopen(path, "rb");
	if (file == NULL)
		return errno == ENOENT;
	if (fseek(file, 0, SEEK_END) != 0)
	{
		fclose(file);
		return false;
	}
	file_size = ftell(file);
	if (file_size < 0 || (unsigned long)file_size > STUDY_REVIEW_LOG_MAX_BYTES)
	{
		fclose(file);
		return false;
	}
	if (fseek(file, 0, SEEK_SET) != 0)
	{
		fclose(file);
		return false;
	}

	buffer->data = malloc((size_t)file_size + 1);
	if (buffer->data == NULL)
	{
		fclose(file);
		return false;
	}
	bytes_read = fread(buffer->data, 1, (size_t)file_size, file);
	if (bytes_read != (size_t)file_size || ferror(file))
	{
		fclose(file);
		study_review_log_free_buffer(buffer);
		return false;
	}
	fclose(file);

	buffer->data[file_size] = '\0';
	buffer->size = (size_t)file_size;
	return true;
}

static void study_review_log_trim_partial_row(
	struct study_review_log_buffer *buffer
)
{
	size_t index;

	if (buffer == NULL || buffer->size == 0 || buffer->data == NULL)
		return;
	if (buffer->data[buffer->size - 1] == '\n')
		return;

	index = buffer->size;
	while (index > 0 && buffer->data[index - 1] != '\n')
		index--;

	buffer->size = index;
	buffer->data[index] = '\0';
}

static bool study_review_log_parse_unsigned(const char *text)
{
	char *end = NULL;

	if (text == NULL || text[0] == '\0')
		return false;

	errno = 0;
	(void)strtoul(text, &end, 10);
	return errno == 0 && end != text && *end == '\0';
}

static bool study_review_log_event_text_is_valid(const char *text)
{
	for (
		enum study_review_log_event event = STUDY_REVIEW_LOG_EVENT_RATING;
		event <= STUDY_REVIEW_LOG_EVENT_RESTORE;
		event++
	)
	{
		const char *name = study_review_log_event_name(event);

		if (name != NULL && text != NULL && strcmp(text, name) == 0)
			return true;
	}

	return false;
}

static bool study_review_log_rating_text_is_valid(const char *text)
{
	for (
		enum study_review_log_rating rating = STUDY_REVIEW_LOG_RATING_NONE;
		rating <= STUDY_REVIEW_LOG_RATING_EASY;
		rating++
	)
	{
		const char *name = study_review_log_rating_name(rating);

		if (name != NULL && text != NULL && strcmp(text, name) == 0)
			return true;
	}

	return false;
}

static bool study_review_log_row_is_valid(char *row)
{
	char *fields[6];
	size_t field_count = 1;

	if (row == NULL || row[0] == '\0')
		return false;

	fields[0] = row;
	for (char *cursor = row; *cursor != '\0'; cursor++)
	{
		if (*cursor != '\t')
			continue;
		*cursor = '\0';
		if (field_count >= sizeof(fields) / sizeof(fields[0]))
			return false;
		fields[field_count++] = cursor + 1;
	}
	if (field_count != sizeof(fields) / sizeof(fields[0]))
		return false;
	if (!study_review_log_parse_unsigned(fields[0]))
		return false;
	if (!study_review_log_event_text_is_valid(fields[1]))
		return false;
	if (!study_review_log_rating_text_is_valid(fields[2]))
		return false;
	if (
		strcmp(fields[1], "rating") != 0 &&
		strcmp(fields[2], "-") != 0
	)
	{
		return false;
	}

	return (
		study_review_log_parse_unsigned(fields[3]) &&
		study_review_log_parse_unsigned(fields[4]) &&
		study_review_log_parse_unsigned(fields[5])
	);
}

static bool study_review_log_rows_are_valid(
	const struct study_review_log_buffer *buffer
)
{
	char row[STUDY_REVIEW_LOG_ROW_SIZE];
	size_t row_size = 0;

	if (buffer == NULL || buffer->data == NULL)
		return false;

	for (size_t index = 0; index < buffer->size; index++)
	{
		char value = buffer->data[index];

		if (value != '\n')
		{
			if (row_size + 1 >= sizeof(row))
				return false;
			row[row_size++] = value;
			continue;
		}

		row[row_size] = '\0';
		if (!study_review_log_row_is_valid(row))
			return false;
		row_size = 0;
	}

	return row_size == 0;
}

static bool study_review_log_read_valid_repairable(
	const char *path,
	struct study_review_log_buffer *buffer
)
{
	if (!study_review_log_read_file(path, buffer))
		return false;
	if (buffer->data == NULL)
		return false;

	study_review_log_trim_partial_row(buffer);
	if (study_review_log_rows_are_valid(buffer))
		return true;

	study_review_log_free_buffer(buffer);
	return false;
}

static bool study_review_log_load_repairable(
	const char *path,
	struct study_review_log_buffer *buffer
)
{
	char artifact_path[STUDY_REVIEW_LOG_PATH_SIZE];

	if (buffer == NULL)
		return false;
	if (study_review_log_read_valid_repairable(path, buffer))
		return true;

	if (
		study_review_log_artifact_path(
			artifact_path,
			sizeof(artifact_path),
			path,
			".tmp"
		) &&
		study_review_log_read_valid_repairable(artifact_path, buffer)
	)
	{
		return true;
	}
	study_review_log_free_buffer(buffer);
	if (
		study_review_log_artifact_path(
			artifact_path,
			sizeof(artifact_path),
			path,
			".bak"
		) &&
		study_review_log_read_valid_repairable(artifact_path, buffer)
	)
	{
		return true;
	}
	study_review_log_free_buffer(buffer);

	buffer->data = malloc(1);
	if (buffer->data == NULL)
		return false;
	buffer->data[0] = '\0';
	buffer->size = 0;
	return true;
}

static bool study_review_log_write_file(
	const char *path,
	const char *prefix,
	size_t prefix_size,
	const char *row,
	size_t row_size
)
{
	FILE *file = fopen(path, "wb");

	if (file == NULL)
		return false;
	if (
		(prefix_size > 0 && fwrite(prefix, 1, prefix_size, file) != prefix_size) ||
		fwrite(row, 1, row_size, file) != row_size
	)
	{
		fclose(file);
		return false;
	}
	if (fclose(file) != 0)
		return false;

	return true;
}

bool study_review_log_append(
	const char *path,
	const struct study_review_log_entry *entry
)
{
	char tmp_path[STUDY_REVIEW_LOG_PATH_SIZE];
	char bak_path[STUDY_REVIEW_LOG_PATH_SIZE];
	char row[STUDY_REVIEW_LOG_ROW_SIZE];
	struct study_review_log_buffer buffer;
	const char *event_name;
	const char *rating_name;
	int written;
	bool backup_created = false;

	if (path == NULL || path[0] == '\0' || entry == NULL)
		return false;

	event_name = study_review_log_event_name(entry->event);
	rating_name = study_review_log_rating_name(entry->rating);
	if (event_name == NULL || rating_name == NULL)
		return false;
	if (entry->event != STUDY_REVIEW_LOG_EVENT_RATING &&
		entry->rating != STUDY_REVIEW_LOG_RATING_NONE)
		return false;

	written = snprintf(
		row,
		sizeof(row),
		"%lu\t%s\t%s\t%lu\t%u\t%u\n",
		entry->timestamp,
		event_name,
		rating_name,
		(unsigned long)entry->card_index,
		entry->reviewed_count,
		entry->suspended_count
	);
	if (written < 0 || (size_t)written >= sizeof(row))
		return false;

	if (
		!study_review_log_artifact_path(
			tmp_path,
			sizeof(tmp_path),
			path,
			".tmp"
		) ||
		!study_review_log_artifact_path(
			bak_path,
			sizeof(bak_path),
			path,
			".bak"
		)
	)
	{
		return false;
	}

	memset(&buffer, 0, sizeof(buffer));
	if (!study_review_log_load_repairable(path, &buffer))
		return false;
	if (buffer.size + (size_t)written > STUDY_REVIEW_LOG_MAX_BYTES)
	{
		study_review_log_free_buffer(&buffer);
		return false;
	}

	if (
		!study_review_log_write_file(
			tmp_path,
			buffer.data,
			buffer.size,
			row,
			(size_t)written
		)
	)
	{
		study_review_log_free_buffer(&buffer);
		return false;
	}
	study_review_log_free_buffer(&buffer);

	if (!study_review_log_remove_if_present(bak_path))
	{
		(void)remove(tmp_path);
		return false;
	}
	if (study_review_log_file_exists(path))
	{
		if (rename(path, bak_path) != 0)
		{
			(void)remove(tmp_path);
			return false;
		}
		backup_created = true;
	}
	if (rename(tmp_path, path) != 0)
	{
		if (backup_created)
			(void)rename(bak_path, path);
		(void)remove(tmp_path);
		return false;
	}
	(void)remove(bak_path);

	return true;
}

bool study_review_log_delete(const char *path)
{
	char artifact_path[STUDY_REVIEW_LOG_PATH_SIZE];
	bool ok = true;

	if (path == NULL || path[0] == '\0')
		return false;

	ok = study_review_log_remove_if_present(path) && ok;
	if (study_review_log_artifact_path(
		artifact_path,
		sizeof(artifact_path),
		path,
		".tmp"
	))
	{
		ok = study_review_log_remove_if_present(artifact_path) && ok;
	}
	else
	{
		ok = false;
	}
	if (study_review_log_artifact_path(
		artifact_path,
		sizeof(artifact_path),
		path,
		".bak"
	))
	{
		ok = study_review_log_remove_if_present(artifact_path) && ok;
	}
	else
	{
		ok = false;
	}

	return ok;
}
