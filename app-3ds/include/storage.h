#ifndef ANKI3DS_STORAGE_H
#define ANKI3DS_STORAGE_H

#include <stdbool.h>
#include <stddef.h>

#define STORAGE_MAX_PATH_LENGTH 256
#define STORAGE_TEMP_SUFFIX ".tmp"
#define STORAGE_BACKUP_SUFFIX ".bak"

bool storage_build_suffixed_path(
	char *destination,
	size_t destination_size,
	const char *path,
	const char *suffix
);
bool storage_replace_file(const char *path);
bool storage_delete_save_files(const char *path);

#endif
