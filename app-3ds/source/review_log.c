#include "review_log.h"

#include "storage.h"

#include <stddef.h>
#include <stdio.h>

#define REVIEW_LOG_ROW_FORMAT \
	"%ld\t%u\t%s\t%s\t%s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n"

static const char *review_log_rating_field(enum scheduler_rating rating)
{
	switch (rating)
	{
	case SCHEDULER_RATING_AGAIN:
		return "again";
	case SCHEDULER_RATING_HARD:
		return "hard";
	case SCHEDULER_RATING_GOOD:
		return "good";
	case SCHEDULER_RATING_EASY:
		return "easy";
	case SCHEDULER_RATING_COUNT:
		break;
	}

	return "-";
}

static bool review_log_rating_is_valid(enum scheduler_rating rating)
{
	return (
		rating == SCHEDULER_RATING_AGAIN ||
		rating == SCHEDULER_RATING_HARD ||
		rating == SCHEDULER_RATING_GOOD ||
		rating == SCHEDULER_RATING_EASY
	);
}

static bool review_log_card_id_is_valid(const char *card_id)
{
	if (card_id == NULL || card_id[0] == '\0')
		return false;

	for (size_t index = 0; card_id[index] != '\0'; index++)
	{
		if (
			card_id[index] == '\t' ||
			card_id[index] == '\n' ||
			card_id[index] == '\r'
		)
		{
			return false;
		}
	}

	return true;
}

const char *review_log_event_name(enum review_log_event event)
{
	switch (event)
	{
	case REVIEW_LOG_EVENT_RATING:
		return "rating";
	case REVIEW_LOG_EVENT_SUSPEND:
		return "suspend";
	case REVIEW_LOG_EVENT_UNDO:
		return "undo";
	case REVIEW_LOG_EVENT_RESTORE:
		return "restore";
	}

	return NULL;
}

static int review_log_format_length(
	const struct review_log_entry *entry,
	const char *event_name,
	const char *rating
)
{
	return snprintf(
		NULL,
		0,
		REVIEW_LOG_ROW_FORMAT,
		(long)entry->timestamp,
		entry->day,
		event_name,
		entry->card_id,
		rating,
		entry->before.review_count,
		entry->before.due_day,
		entry->before.interval_days,
		entry->before.ease_permille,
		entry->before.lapses,
		entry->before.suspended ? 1u : 0u,
		entry->after.review_count,
		entry->after.due_day,
		entry->after.interval_days,
		entry->after.ease_permille,
		entry->after.lapses,
		entry->after.suspended ? 1u : 0u
	);
}

static int review_log_write_entry(
	FILE *file,
	const struct review_log_entry *entry,
	const char *event_name,
	const char *rating
)
{
	return fprintf(
		file,
		REVIEW_LOG_ROW_FORMAT,
		(long)entry->timestamp,
		entry->day,
		event_name,
		entry->card_id,
		rating,
		entry->before.review_count,
		entry->before.due_day,
		entry->before.interval_days,
		entry->before.ease_permille,
		entry->before.lapses,
		entry->before.suspended ? 1u : 0u,
		entry->after.review_count,
		entry->after.due_day,
		entry->after.interval_days,
		entry->after.ease_permille,
		entry->after.lapses,
		entry->after.suspended ? 1u : 0u
	);
}

static bool review_log_final_row_is_complete(FILE *file, long file_size)
{
	int final_byte;

	if (file_size == 0)
		return true;
	if (fseek(file, -1, SEEK_END) != 0)
		return false;

	final_byte = fgetc(file);
	return final_byte == '\n';
}

static bool review_log_complete_prefix_size(
	FILE *file,
	long file_size,
	long *complete_size
)
{
	long complete = 0;

	if (fseek(file, 0, SEEK_SET) != 0)
		return false;

	for (long position = 0; position < file_size; position++)
	{
		int value = fgetc(file);

		if (value == EOF)
			return false;
		if (value == '\n')
			complete = position + 1;
	}

	if (ferror(file))
		return false;

	*complete_size = complete;
	return true;
}

static bool review_log_remove_if_present(const char *path)
{
	FILE *file = fopen(path, "rb");

	if (file == NULL)
		return true;

	fclose(file);
	return remove(path) == 0;
}

static bool review_log_file_exists(const char *path)
{
	FILE *file = fopen(path, "rb");

	if (file == NULL)
		return false;

	fclose(file);
	return true;
}

static bool review_log_copy_prefix(
	const char *source_path,
	const char *temp_path,
	long byte_count
)
{
	FILE *source;
	FILE *destination;
	char buffer[512];
	long remaining = byte_count;
	bool ok = true;

	source = fopen(source_path, "rb");
	if (source == NULL)
		return false;

	destination = fopen(temp_path, "wb");
	if (destination == NULL)
	{
		fclose(source);
		return false;
	}

	while (remaining > 0)
	{
		size_t chunk_size = remaining > (long)sizeof(buffer) ?
			sizeof(buffer) :
			(size_t)remaining;
		size_t bytes_read = fread(buffer, 1, chunk_size, source);

		if (bytes_read != chunk_size)
		{
			ok = false;
			break;
		}
		if (fwrite(buffer, 1, bytes_read, destination) != bytes_read)
		{
			ok = false;
			break;
		}

		remaining -= (long)bytes_read;
	}

	if (ferror(source))
		ok = false;
	if (fclose(source) != 0)
		ok = false;
	if (fclose(destination) != 0)
		ok = false;
	if (!ok)
		remove(temp_path);

	return ok;
}

static bool review_log_repair_partial_final_row(
	const char *path,
	long complete_size
)
{
	char temp_path[STORAGE_MAX_PATH_LENGTH];
	char backup_path[STORAGE_MAX_PATH_LENGTH];
	bool moved_original = false;

	if (!storage_build_suffixed_path(
		temp_path,
		sizeof(temp_path),
		path,
		STORAGE_TEMP_SUFFIX
	))
	{
		return false;
	}
	if (!storage_build_suffixed_path(
		backup_path,
		sizeof(backup_path),
		path,
		STORAGE_BACKUP_SUFFIX
	))
	{
		return false;
	}
	if (!review_log_remove_if_present(temp_path))
		return false;
	if (!review_log_remove_if_present(backup_path))
		return false;
	if (!review_log_copy_prefix(path, temp_path, complete_size))
		return false;

	if (rename(path, backup_path) == 0)
	{
		moved_original = true;
	}
	else
	{
		remove(temp_path);
		return false;
	}

	if (rename(temp_path, path) != 0)
	{
		if (moved_original)
			rename(backup_path, path);
		remove(temp_path);
		return false;
	}

	if (!review_log_remove_if_present(backup_path))
		return false;

	return true;
}

static bool review_log_recover_pending_repair(const char *path)
{
	char temp_path[STORAGE_MAX_PATH_LENGTH];
	char backup_path[STORAGE_MAX_PATH_LENGTH];

	if (!storage_build_suffixed_path(
		temp_path,
		sizeof(temp_path),
		path,
		STORAGE_TEMP_SUFFIX
	))
	{
		return false;
	}
	if (!storage_build_suffixed_path(
		backup_path,
		sizeof(backup_path),
		path,
		STORAGE_BACKUP_SUFFIX
	))
	{
		return false;
	}
	if (review_log_file_exists(path))
	{
		if (!review_log_remove_if_present(temp_path))
			return false;
		if (!review_log_remove_if_present(backup_path))
			return false;

		return true;
	}
	if (review_log_file_exists(temp_path))
	{
		if (!storage_promote_recovery_file(path, STORAGE_TEMP_SUFFIX))
			return false;

		return review_log_remove_if_present(backup_path);
	}
	if (review_log_file_exists(backup_path))
		return storage_promote_recovery_file(path, STORAGE_BACKUP_SUFFIX);

	return true;
}

bool review_log_append(const char *path, const struct review_log_entry *entry)
{
	FILE *file;
	const char *event_name;
	const char *rating;
	long file_size;
	long complete_size;
	int row_size;

	if (path == NULL || entry == NULL)
		return false;
	if (!review_log_card_id_is_valid(entry->card_id))
		return false;
	event_name = review_log_event_name(entry->event);
	if (event_name == NULL)
		return false;
	if (entry->event == REVIEW_LOG_EVENT_RATING)
	{
		if (!review_log_rating_is_valid(entry->rating))
			return false;
		rating = review_log_rating_field(entry->rating);
	}
	else
	{
		rating = "-";
	}

	row_size = review_log_format_length(entry, event_name, rating);
	if (row_size < 0 || row_size > REVIEW_LOG_MAX_BYTES)
		return false;
	if (!review_log_recover_pending_repair(path))
		return false;

	file = fopen(path, "a+");
	if (file == NULL)
		return false;

	if (fseek(file, 0, SEEK_END) != 0)
	{
		fclose(file);
		return false;
	}

	file_size = ftell(file);
	if (file_size < 0)
	{
		fclose(file);
		return false;
	}
	if (!review_log_final_row_is_complete(file, file_size))
	{
		if (!review_log_complete_prefix_size(file, file_size, &complete_size))
		{
			fclose(file);
			return false;
		}
		fclose(file);
		if (!review_log_repair_partial_final_row(path, complete_size))
			return false;

		file = fopen(path, "a+");
		if (file == NULL)
			return false;
		if (fseek(file, 0, SEEK_END) != 0)
		{
			fclose(file);
			return false;
		}
		file_size = ftell(file);
		if (file_size < 0)
		{
			fclose(file);
			return false;
		}
	}
	if (file_size > REVIEW_LOG_MAX_BYTES - (long)row_size)
	{
		fclose(file);
		return false;
	}
	if (fseek(file, 0, SEEK_END) != 0)
	{
		fclose(file);
		return false;
	}

	if (review_log_write_entry(file, entry, event_name, rating) < 0)
	{
		fclose(file);
		return false;
	}

	if (fclose(file) != 0)
		return false;

	return true;
}

bool review_log_delete(const char *path)
{
	char temp_path[STORAGE_MAX_PATH_LENGTH];
	char backup_path[STORAGE_MAX_PATH_LENGTH];

	if (path == NULL)
		return false;
	if (!storage_build_suffixed_path(
		temp_path,
		sizeof(temp_path),
		path,
		STORAGE_TEMP_SUFFIX
	))
	{
		return false;
	}
	if (!storage_build_suffixed_path(
		backup_path,
		sizeof(backup_path),
		path,
		STORAGE_BACKUP_SUFFIX
	))
	{
		return false;
	}

	if (!review_log_remove_if_present(temp_path))
		return false;
	if (!review_log_remove_if_present(backup_path))
		return false;

	return review_log_remove_if_present(path);
}
