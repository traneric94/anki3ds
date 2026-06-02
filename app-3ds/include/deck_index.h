#ifndef ANKI3DS_DECK_INDEX_H
#define ANKI3DS_DECK_INDEX_H

#include <stdbool.h>
#include <stddef.h>

#include "deck.h"

#define DECK_INDEX_MAX_DECKS 16
#define DECK_INDEX_MAX_PATH_LENGTH 240
#define DECK_INDEX_ROOT_PATH "sdmc:/3ds/anki3ds/decks"

struct deck_entry
{
	char id[DECK_MAX_NAME_LENGTH];
	char display_name[DECK_MAX_NAME_LENGTH];
	char cards_path[DECK_INDEX_MAX_PATH_LENGTH];
	char deck_json_path[DECK_INDEX_MAX_PATH_LENGTH];
	char state_path[DECK_INDEX_MAX_PATH_LENGTH];
	char settings_path[DECK_INDEX_MAX_PATH_LENGTH];
};

struct deck_index
{
	size_t count;
	bool overflowed;
	struct deck_entry entries[DECK_INDEX_MAX_DECKS];
};

void deck_index_init(struct deck_index *index);
void deck_index_scan(struct deck_index *index, const char *root_path);
const struct deck_entry *deck_index_get(const struct deck_index *index, size_t entry_index);
bool deck_index_build_entry(struct deck_entry *entry, const char *root_path, const char *deck_id);

#endif
