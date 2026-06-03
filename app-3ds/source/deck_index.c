#include "deck_index.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_string(char *destination, size_t destination_size, const char *source)
{
	if (destination_size == 0)
		return;

	snprintf(destination, destination_size, "%s", source);
}

static bool path_join_deck_file(
	char *destination,
	size_t destination_size,
	const char *root_path,
	const char *deck_id,
	const char *filename
)
{
	int written = snprintf(
		destination,
		destination_size,
		"%s/%s/%s",
		root_path,
		deck_id,
		filename
	);

	return written >= 0 && (size_t)written < destination_size;
}

static bool deck_id_character_is_valid(char value)
{
	if (value >= 'a' && value <= 'z')
		return true;
	if (value >= 'A' && value <= 'Z')
		return true;
	if (value >= '0' && value <= '9')
		return true;

	return value == '-' || value == '_';
}

static bool deck_id_is_valid(const char *deck_id)
{
	if (deck_id[0] == '\0' || deck_id[0] == '.')
		return false;
	if (strlen(deck_id) >= DECK_MAX_NAME_LENGTH)
		return false;

	for (size_t index = 0; deck_id[index] != '\0'; index++)
	{
		if (!deck_id_character_is_valid(deck_id[index]))
			return false;
	}

	return true;
}

static bool file_exists(const char *path)
{
	FILE *file = fopen(path, "r");

	if (file == NULL)
		return false;

	fclose(file);
	return true;
}

static const char *find_json_string_value(const char *line, const char *key)
{
	const char *cursor = strstr(line, key);
	const char *colon;
	const char *value;

	if (cursor == NULL)
		return NULL;

	colon = strchr(cursor + strlen(key), ':');
	if (colon == NULL)
		return NULL;

	value = colon + 1;
	while (*value == ' ' || *value == '\t')
		value++;
	if (*value != '"')
		return NULL;

	return value + 1;
}

static bool copy_json_string(char *destination, size_t destination_size, const char *source)
{
	size_t output_index = 0;
	bool escaped = false;

	if (destination_size == 0)
		return false;

	for (size_t input_index = 0; source[input_index] != '\0'; input_index++)
	{
		unsigned char value = (unsigned char)source[input_index];

		if (escaped)
		{
			switch (value)
			{
			case '"':
			case '\\':
			case '/':
				break;
			case 'n':
			case 'r':
			case 't':
				value = ' ';
				break;
			default:
				return false;
			}
			escaped = false;
		}
		else if (value == '\\')
		{
			escaped = true;
			continue;
		}
		else if (value == '"')
		{
			if (output_index == 0)
				return false;

			destination[output_index] = '\0';
			return true;
		}

		if (value < ' ')
			return false;
		if (output_index + 1 >= destination_size)
			return false;

		destination[output_index] = (char)value;
		output_index++;
	}

	return false;
}

static bool deck_index_load_display_name(
	char *destination,
	size_t destination_size,
	const char *path
)
{
	FILE *file = fopen(path, "r");
	char line[256];
	bool loaded = false;

	if (file == NULL)
		return false;

	while (fgets(line, sizeof(line), file) != NULL)
	{
		const char *value = find_json_string_value(line, "\"name\"");

		if (value != NULL)
		{
			char display_name[DECK_MAX_NAME_LENGTH];

			loaded = copy_json_string(display_name, sizeof(display_name), value);
			if (loaded)
				copy_string(destination, destination_size, display_name);
			break;
		}
	}

	fclose(file);
	return loaded;
}

static int compare_deck_entries(const void *left, const void *right)
{
	const struct deck_entry *left_entry = left;
	const struct deck_entry *right_entry = right;

	return strcmp(left_entry->id, right_entry->id);
}

static void deck_index_store(struct deck_index *index, const struct deck_entry *deck)
{
	size_t largest_index = 0;

	index->total_count++;

	if (index->count < DECK_INDEX_MAX_DECKS)
	{
		index->entries[index->count] = *deck;
		index->count++;
		return;
	}

	index->overflowed = true;

	for (size_t entry_index = 1; entry_index < index->count; entry_index++)
	{
		if (strcmp(index->entries[entry_index].id, index->entries[largest_index].id) > 0)
			largest_index = entry_index;
	}

	if (strcmp(deck->id, index->entries[largest_index].id) < 0)
		index->entries[largest_index] = *deck;
}

void deck_index_init(struct deck_index *index)
{
	memset(index, 0, sizeof(*index));
}

bool deck_index_build_entry(struct deck_entry *entry, const char *root_path, const char *deck_id)
{
	if (entry == NULL || root_path == NULL || deck_id == NULL)
		return false;

	memset(entry, 0, sizeof(*entry));

	if (!deck_id_is_valid(deck_id))
		return false;
	if (
		!path_join_deck_file(
			entry->deck_json_path,
			sizeof(entry->deck_json_path),
			root_path,
			deck_id,
			"deck.json"
		)
	)
	{
		return false;
	}
	if (!path_join_deck_file(entry->cards_path, sizeof(entry->cards_path), root_path, deck_id, "cards.tsv"))
		return false;
	if (!path_join_deck_file(entry->state_path, sizeof(entry->state_path), root_path, deck_id, "state.tsv"))
		return false;
	if (
		!path_join_deck_file(
			entry->review_log_path,
			sizeof(entry->review_log_path),
			root_path,
			deck_id,
			"review-log.tsv"
		)
	)
	{
		return false;
	}
	if (
		!path_join_deck_file(
			entry->settings_path,
			sizeof(entry->settings_path),
			root_path,
			deck_id,
			"settings.tsv"
		)
	)
	{
		return false;
	}
	if (
		!path_join_deck_file(
			entry->media_path,
			sizeof(entry->media_path),
			root_path,
			deck_id,
			"media"
		)
	)
	{
		return false;
	}

	copy_string(entry->id, sizeof(entry->id), deck_id);
	copy_string(entry->display_name, sizeof(entry->display_name), deck_id);
	return true;
}

void deck_index_scan(struct deck_index *index, const char *root_path)
{
	DIR *directory;
	struct dirent *entry;

	deck_index_init(index);

	if (root_path == NULL)
		return;

	directory = opendir(root_path);
	if (directory == NULL)
		return;

	while ((entry = readdir(directory)) != NULL)
	{
		struct deck_entry deck;

		if (!deck_index_build_entry(&deck, root_path, entry->d_name))
			continue;
		if (!file_exists(deck.cards_path))
			continue;
		deck_index_load_display_name(
			deck.display_name,
			sizeof(deck.display_name),
			deck.deck_json_path
		);

		deck_index_store(index, &deck);
	}

	closedir(directory);

	qsort(index->entries, index->count, sizeof(index->entries[0]), compare_deck_entries);
}

const struct deck_entry *deck_index_get(const struct deck_index *index, size_t entry_index)
{
	if (entry_index >= index->count)
		return NULL;

	return &index->entries[entry_index];
}

bool deck_index_find(
	const struct deck_index *index,
	const char *deck_id,
	size_t *entry_index
)
{
	if (index == NULL || deck_id == NULL)
		return false;

	for (size_t index_entry = 0; index_entry < index->count; index_entry++)
	{
		if (strcmp(index->entries[index_entry].id, deck_id) == 0)
		{
			if (entry_index != NULL)
				*entry_index = index_entry;
			return true;
		}
	}

	return false;
}
