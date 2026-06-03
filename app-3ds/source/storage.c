#include "storage.h"

#include <stdio.h>

bool storage_build_suffixed_path(
	char *destination,
	size_t destination_size,
	const char *path,
	const char *suffix
)
{
	int written;

	if (destination == NULL || path == NULL || suffix == NULL)
		return false;

	written = snprintf(destination, destination_size, "%s%s", path, suffix);

	return written >= 0 && (size_t)written < destination_size;
}

static bool remove_if_present(const char *path)
{
	FILE *file = fopen(path, "rb");

	if (file == NULL)
		return true;

	fclose(file);
	if (remove(path) != 0)
		return false;

	return true;
}

static bool file_exists(const char *path)
{
	FILE *file = fopen(path, "rb");

	if (file == NULL)
		return false;

	fclose(file);
	return true;
}

bool storage_replace_file(const char *path)
{
	char temp_path[STORAGE_MAX_PATH_LENGTH];
	char backup_path[STORAGE_MAX_PATH_LENGTH];
	bool had_previous_file = false;

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
		remove(temp_path);
		return false;
	}

	if (!remove_if_present(backup_path))
	{
		remove(temp_path);
		return false;
	}

	if (file_exists(path))
	{
		if (rename(path, backup_path) != 0)
		{
			remove(temp_path);
			return false;
		}

		had_previous_file = true;
	}

	if (rename(temp_path, path) != 0)
	{
		if (had_previous_file)
			rename(backup_path, path);

		remove(temp_path);
		return false;
	}

	return true;
}

bool storage_delete_save_files(const char *path)
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

	if (!remove_if_present(temp_path))
		return false;
	if (!remove_if_present(backup_path))
		return false;
	if (!remove_if_present(path))
		return false;

	return true;
}
