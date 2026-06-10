#include "study_deck_index.h"

#include "study_backend.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STUDY_DECK_INDEX_STATS_LINE_SIZE 512

struct study_deck_index_state_summary
{
	bool introduced[STUDY_BACKEND_MAX_CARDS];
	bool completed_today[STUDY_BACKEND_MAX_CARDS];
	bool suspended[STUDY_BACKEND_MAX_CARDS];
	bool scheduled[STUDY_BACKEND_MAX_CARDS];
	unsigned long due_day[STUDY_BACKEND_MAX_CARDS];
	unsigned long schedule_indices[STUDY_BACKEND_MAX_CARDS];
	unsigned long schedule_due_days[STUDY_BACKEND_MAX_CARDS];
	unsigned long schedule_interval_days[STUDY_BACKEND_MAX_CARDS];
	unsigned long state_card_count;
	unsigned long progress_day;
	size_t schedule_index_count;
	size_t schedule_due_day_count;
	size_t schedule_interval_day_count;
	bool seen_card_count;
	bool seen_progress_day;
	bool present;
	bool bad;
};

static void study_deck_index_copy_string(
	char *destination,
	size_t destination_size,
	const char *source
)
{
	if (destination == NULL || destination_size == 0)
		return;

	snprintf(destination, destination_size, "%s", source != NULL ? source : "");
}

static bool study_deck_index_join_path(
	char *destination,
	size_t destination_size,
	const char *root_path,
	const char *deck_id,
	const char *filename
)
{
	int written;

	if (
		destination == NULL ||
		root_path == NULL ||
		deck_id == NULL ||
		filename == NULL
	)
	{
		return false;
	}

	written = snprintf(
		destination,
		destination_size,
		"%s/%s/%s",
		root_path,
		deck_id,
		filename
	);

	return written >= 0 && (size_t)written < destination_size;
}

static bool study_deck_index_id_character_is_valid(char value)
{
	if (value >= 'a' && value <= 'z')
		return true;
	if (value >= 'A' && value <= 'Z')
		return true;
	if (value >= '0' && value <= '9')
		return true;

	return value == '-' || value == '_';
}

static bool study_deck_index_parse_unsigned(
	const char *text,
	unsigned long *value
)
{
	unsigned long parsed = 0;

	if (text == NULL || text[0] == '\0')
		return false;

	for (size_t index = 0; text[index] != '\0'; index++)
	{
		unsigned int digit;

		if (text[index] < '0' || text[index] > '9')
			return false;
		digit = (unsigned int)(text[index] - '0');
		if (parsed > (unsigned long)(~0UL) / 10UL)
			return false;
		parsed *= 10UL;
		if (parsed > (unsigned long)(~0UL) - digit)
			return false;
		parsed += digit;
	}

	if (value != NULL)
		*value = parsed;
	return true;
}

static bool study_deck_index_id_is_valid(const char *deck_id)
{
	if (deck_id == NULL || deck_id[0] == '\0' || deck_id[0] == '.')
		return false;
	if (strlen(deck_id) >= STUDY_DECK_INDEX_ID_SIZE)
		return false;

	for (size_t index = 0; deck_id[index] != '\0'; index++)
	{
		if (!study_deck_index_id_character_is_valid(deck_id[index]))
			return false;
	}

	return true;
}

static bool study_deck_index_file_exists(const char *path)
{
	FILE *file = fopen(path, "r");

	if (file == NULL)
		return false;

	fclose(file);
	return true;
}

static bool study_deck_index_line_complete(FILE *file, const char *line)
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

static size_t study_deck_index_tsv_field_count(const char *line)
{
	size_t field_count = 1;

	if (line == NULL || line[0] == '\0')
		return 0;

	for (size_t index = 0; line[index] != '\0'; index++)
	{
		if (line[index] == '\t')
			field_count++;
	}

	return field_count;
}

static bool study_deck_index_count_cards(
	const char *path,
	unsigned int *card_count
)
{
	FILE *file;
	char line[STUDY_DECK_INDEX_STATS_LINE_SIZE];
	unsigned int count = 0;

	if (card_count != NULL)
		*card_count = 0;
	if (path == NULL || path[0] == '\0')
		return false;

	file = fopen(path, "r");
	if (file == NULL)
		return false;

	while (fgets(line, sizeof(line), file) != NULL)
	{
		if (!study_deck_index_line_complete(file, line))
		{
			fclose(file);
			return false;
		}
		line[strcspn(line, "\r\n")] = '\0';
		if (line[0] == '\0')
			continue;
		if (study_deck_index_tsv_field_count(line) != 5)
		{
			fclose(file);
			return false;
		}
		if (count >= STUDY_BACKEND_MAX_CARDS)
		{
			fclose(file);
			return false;
		}
		count++;
	}

	if (ferror(file))
	{
		fclose(file);
		return false;
	}
	fclose(file);
	if (card_count != NULL)
		*card_count = count;
	return true;
}

static bool study_deck_index_entry_name_is_hidden(const char *name)
{
	return name == NULL || name[0] == '.';
}

static const char *study_deck_index_find_json_name_value(const char *line)
{
	const char *cursor;
	const char *colon;
	const char *value;

	if (line == NULL)
		return NULL;

	cursor = strstr(line, "\"name\"");
	if (cursor == NULL)
		return NULL;

	colon = strchr(cursor + 6, ':');
	if (colon == NULL)
		return NULL;

	value = colon + 1;
	while (*value == ' ' || *value == '\t')
		value++;
	if (*value != '"')
		return NULL;

	return value + 1;
}

static bool study_deck_index_copy_json_string(
	char *destination,
	size_t destination_size,
	const char *source
)
{
	size_t output_index = 0;
	bool escaped = false;

	if (destination == NULL || destination_size == 0 || source == NULL)
		return false;

	for (size_t input_index = 0; source[input_index] != '\0'; input_index++)
	{
		unsigned char value = (unsigned char)source[input_index];

		if (escaped)
		{
			if (value != '"' && value != '\\' && value != '/')
				return false;
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

		destination[output_index++] = (char)value;
	}

	return false;
}

static bool study_deck_index_load_display_name(
	char *destination,
	size_t destination_size,
	const char *path
)
{
	FILE *file;
	char line[256];
	bool loaded = false;

	if (destination == NULL || destination_size == 0 || path == NULL)
		return false;

	file = fopen(path, "r");
	if (file == NULL)
		return false;

	while (fgets(line, sizeof(line), file) != NULL)
	{
		const char *value = study_deck_index_find_json_name_value(line);

		if (value == NULL)
			continue;

		loaded = study_deck_index_copy_json_string(
			destination,
			destination_size,
			value
		);
		break;
	}

	fclose(file);
	return loaded;
}

static bool study_deck_index_state_mark_index(
	bool flags[STUDY_BACKEND_MAX_CARDS],
	unsigned int card_count,
	unsigned long index
)
{
	if (index >= card_count || index >= STUDY_BACKEND_MAX_CARDS)
		return false;
	if (flags[index])
		return false;

	flags[index] = true;
	return true;
}

static bool study_deck_index_state_assign(
	struct study_deck_index_state_summary *state,
	const char *key,
	unsigned long value,
	unsigned int card_count
)
{
	if (state == NULL || key == NULL)
		return false;

	if (strcmp(key, "card_count") == 0)
	{
		state->state_card_count = value;
		state->seen_card_count = true;
		return true;
	}
	if (strcmp(key, "progress_day") == 0)
	{
		state->progress_day = value;
		state->seen_progress_day = true;
		return true;
	}
	if (strcmp(key, "introduced_index") == 0)
	{
		return study_deck_index_state_mark_index(
			state->introduced,
			card_count,
			value
		);
	}
	if (strcmp(key, "completed_today_index") == 0)
	{
		return study_deck_index_state_mark_index(
			state->completed_today,
			card_count,
			value
		);
	}
	if (strcmp(key, "suspended_index") == 0)
	{
		return study_deck_index_state_mark_index(
			state->suspended,
			card_count,
			value
		);
	}
	if (strcmp(key, "schedule_index") == 0)
	{
		if (state->schedule_index_count >= STUDY_BACKEND_MAX_CARDS)
			return false;

		state->schedule_indices[state->schedule_index_count++] = value;
		return true;
	}
	if (strcmp(key, "schedule_due_day") == 0)
	{
		if (state->schedule_due_day_count >= STUDY_BACKEND_MAX_CARDS)
			return false;

		state->schedule_due_days[state->schedule_due_day_count++] = value;
		return true;
	}
	if (strcmp(key, "schedule_interval_days") == 0)
	{
		if (state->schedule_interval_day_count >= STUDY_BACKEND_MAX_CARDS)
			return false;

		state->schedule_interval_days[
			state->schedule_interval_day_count++
		] = value;
		return true;
	}

	return true;
}

static void study_deck_index_load_state_summary(
	struct study_deck_index_state_summary *state,
	const char *path,
	unsigned int card_count
)
{
	FILE *file;
	char line[STUDY_DECK_INDEX_STATS_LINE_SIZE];

	if (state == NULL)
		return;

	memset(state, 0, sizeof(*state));
	if (path == NULL || path[0] == '\0')
		return;

	file = fopen(path, "r");
	if (file == NULL)
		return;

	state->present = true;
	while (fgets(line, sizeof(line), file) != NULL)
	{
		char *separator;
		unsigned long value;

		if (!study_deck_index_line_complete(file, line))
		{
			state->bad = true;
			break;
		}
		line[strcspn(line, "\r\n")] = '\0';
		if (line[0] == '\0' || line[0] == '#')
			continue;

		separator = strchr(line, '\t');
		if (separator == NULL)
		{
			state->bad = true;
			break;
		}
		*separator = '\0';
		if (
			!study_deck_index_parse_unsigned(separator + 1, &value) ||
			!study_deck_index_state_assign(state, line, value, card_count)
		)
		{
			state->bad = true;
			break;
		}
	}
	if (ferror(file))
		state->bad = true;
	fclose(file);

	if (
		state->seen_card_count &&
		state->state_card_count != (unsigned long)card_count
	)
	{
		state->bad = true;
	}
	for (unsigned int index = 0; index < card_count; index++)
	{
		if (state->completed_today[index] && !state->introduced[index])
			state->bad = true;
	}
	if (
		state->schedule_index_count != state->schedule_due_day_count ||
		state->schedule_index_count != state->schedule_interval_day_count
	)
	{
		state->bad = true;
	}
	for (size_t index = 0; index < state->schedule_index_count; index++)
	{
		unsigned long card_index = state->schedule_indices[index];

		if (
			card_index >= card_count ||
			card_index >= STUDY_BACKEND_MAX_CARDS ||
			state->scheduled[card_index] ||
			!state->introduced[card_index]
		)
		{
			state->bad = true;
			break;
		}

		state->scheduled[card_index] = true;
		state->due_day[card_index] = state->schedule_due_days[index];
	}
}

static void study_deck_index_clear_entry_stats(
	struct study_deck_entry *entry
)
{
	if (entry == NULL)
		return;

	entry->card_count = 0;
	entry->due_count = 0;
	entry->new_count = 0;
	entry->suspended_count = 0;
	entry->stats_loaded = false;
	entry->state_bad = false;
}

static void study_deck_index_apply_stats(
	struct study_deck_entry *entry,
	unsigned int current_day
)
{
	struct study_deck_index_state_summary *state;
	bool use_completed_today;

	if (entry == NULL)
		return;

	study_deck_index_clear_entry_stats(entry);
	entry->stats_loaded = study_deck_index_count_cards(
		entry->cards_path,
		&entry->card_count
	);
	if (!entry->stats_loaded)
	{
		entry->state_bad = true;
		return;
	}

	state = malloc(sizeof(*state));
	if (state == NULL)
	{
		entry->state_bad = true;
		return;
	}

	study_deck_index_load_state_summary(
		state,
		entry->state_path,
		entry->card_count
	);
	entry->state_bad = state->bad;
	if (state->bad)
	{
		free(state);
		return;
	}

	use_completed_today = !(
		state->seen_progress_day &&
		current_day != 0 &&
		state->progress_day != (unsigned long)current_day
	);
	for (unsigned int index = 0; index < entry->card_count; index++)
	{
		bool completed_today = use_completed_today &&
			state->completed_today[index];

		if (state->suspended[index])
			entry->suspended_count++;
		if (
			state->introduced[index] &&
			!state->suspended[index] &&
			!completed_today &&
			(
				current_day == 0 ||
				!state->scheduled[index] ||
				state->due_day[index] <= (unsigned long)current_day
			)
		)
		{
			entry->due_count++;
		}
		if (!state->introduced[index] && !state->suspended[index])
			entry->new_count++;
	}
	free(state);
}

static int study_deck_index_compare_entries(
	const void *left,
	const void *right
)
{
	const struct study_deck_entry *left_entry = left;
	const struct study_deck_entry *right_entry = right;

	return strcmp(left_entry->id, right_entry->id);
}

static void study_deck_index_store(
	struct study_deck_index *index,
	const struct study_deck_entry *entry
)
{
	size_t largest_index = 0;

	if (index == NULL || entry == NULL)
		return;

	index->total_count++;
	if (index->count < STUDY_DECK_INDEX_MAX_DECKS)
	{
		index->entries[index->count] = *entry;
		index->count++;
		return;
	}

	index->overflowed = true;
	for (size_t entry_index = 1; entry_index < index->count; entry_index++)
	{
		if (
			strcmp(
				index->entries[entry_index].id,
				index->entries[largest_index].id
			) > 0
		)
		{
			largest_index = entry_index;
		}
	}

	if (strcmp(entry->id, index->entries[largest_index].id) < 0)
		index->entries[largest_index] = *entry;
}

void study_deck_index_init(struct study_deck_index *index)
{
	if (index == NULL)
		return;

	memset(index, 0, sizeof(*index));
}

bool study_deck_index_build_entry(
	struct study_deck_entry *entry,
	const char *root_path,
	const char *deck_id
)
{
	if (entry == NULL || root_path == NULL || deck_id == NULL)
		return false;

	memset(entry, 0, sizeof(*entry));
	if (!study_deck_index_id_is_valid(deck_id))
		return false;

	if (
		!study_deck_index_join_path(
			entry->metadata_path,
			sizeof(entry->metadata_path),
			root_path,
			deck_id,
			"deck.json"
		)
	)
	{
		return false;
	}
	if (
		!study_deck_index_join_path(
			entry->cards_path,
			sizeof(entry->cards_path),
			root_path,
			deck_id,
			"cards.tsv"
		)
	)
	{
		return false;
	}
	if (
		!study_deck_index_join_path(
			entry->state_path,
			sizeof(entry->state_path),
			root_path,
			deck_id,
			"state.tsv"
		)
	)
	{
		return false;
	}
	if (
		!study_deck_index_join_path(
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
		!study_deck_index_join_path(
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

	study_deck_index_copy_string(entry->id, sizeof(entry->id), deck_id);
	study_deck_index_copy_string(
		entry->display_name,
		sizeof(entry->display_name),
		deck_id
	);
	return true;
}

void study_deck_index_scan_for_day(
	struct study_deck_index *index,
	const char *root_path,
	unsigned int current_day
)
{
	DIR *directory;
	struct dirent *entry;

	study_deck_index_init(index);
	if (index == NULL || root_path == NULL)
		return;

	directory = opendir(root_path);
	if (directory == NULL)
		return;

	while ((entry = readdir(directory)) != NULL)
	{
		struct study_deck_entry deck_entry;

		if (study_deck_index_entry_name_is_hidden(entry->d_name))
			continue;
		if (
			!study_deck_index_build_entry(
				&deck_entry,
				root_path,
				entry->d_name
			)
		)
		{
			index->ignored_count++;
			continue;
		}
		if (!study_deck_index_file_exists(deck_entry.cards_path))
		{
			index->ignored_count++;
			continue;
		}

		(void)study_deck_index_load_display_name(
			deck_entry.display_name,
			sizeof(deck_entry.display_name),
			deck_entry.metadata_path
		);
		study_deck_index_apply_stats(&deck_entry, current_day);
		study_deck_index_store(index, &deck_entry);
	}

	closedir(directory);
	qsort(
		index->entries,
		index->count,
		sizeof(index->entries[0]),
		study_deck_index_compare_entries
	);
}

void study_deck_index_scan(
	struct study_deck_index *index,
	const char *root_path
)
{
	study_deck_index_scan_for_day(index, root_path, 0);
}

const struct study_deck_entry *study_deck_index_get(
	const struct study_deck_index *index,
	size_t entry_index
)
{
	if (index == NULL || entry_index >= index->count)
		return NULL;

	return &index->entries[entry_index];
}

bool study_deck_index_find(
	const struct study_deck_index *index,
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

bool study_deck_index_refresh_entry_for_day(
	struct study_deck_index *index,
	const char *deck_id,
	unsigned int current_day
)
{
	size_t entry_index = 0;

	if (!study_deck_index_find(index, deck_id, &entry_index))
		return false;

	study_deck_index_apply_stats(&index->entries[entry_index], current_day);
	return true;
}
