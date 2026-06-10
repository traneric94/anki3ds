#ifndef ANKI3DS_STUDY_DECK_INDEX_H
#define ANKI3DS_STUDY_DECK_INDEX_H

#include <stdbool.h>
#include <stddef.h>

#define STUDY_DECK_INDEX_ROOT_PATH "sdmc:/3ds/anki3ds/decks"
#define STUDY_DECK_INDEX_MAX_DECKS 64
#define STUDY_DECK_INDEX_ID_SIZE 64
#define STUDY_DECK_INDEX_PATH_SIZE 240

struct study_deck_entry
{
	char id[STUDY_DECK_INDEX_ID_SIZE];
	char display_name[STUDY_DECK_INDEX_ID_SIZE];
	char cards_path[STUDY_DECK_INDEX_PATH_SIZE];
	char state_path[STUDY_DECK_INDEX_PATH_SIZE];
	char settings_path[STUDY_DECK_INDEX_PATH_SIZE];
	char review_log_path[STUDY_DECK_INDEX_PATH_SIZE];
	char metadata_path[STUDY_DECK_INDEX_PATH_SIZE];
	unsigned int card_count;
	unsigned int due_count;
	unsigned int new_count;
	unsigned int suspended_count;
	bool stats_loaded;
	bool state_bad;
};

struct study_deck_index
{
	size_t count;
	size_t total_count;
	size_t ignored_count;
	bool overflowed;
	struct study_deck_entry entries[STUDY_DECK_INDEX_MAX_DECKS];
};

void study_deck_index_init(struct study_deck_index *index);
bool study_deck_index_build_entry(
	struct study_deck_entry *entry,
	const char *root_path,
	const char *deck_id
);
void study_deck_index_scan(
	struct study_deck_index *index,
	const char *root_path
);
void study_deck_index_scan_for_day(
	struct study_deck_index *index,
	const char *root_path,
	unsigned int current_day
);
bool study_deck_index_refresh_entry_for_day(
	struct study_deck_index *index,
	const char *deck_id,
	unsigned int current_day
);
const struct study_deck_entry *study_deck_index_get(
	const struct study_deck_index *index,
	size_t entry_index
);
bool study_deck_index_find(
	const struct study_deck_index *index,
	const char *deck_id,
	size_t *entry_index
);

#endif
