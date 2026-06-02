#ifndef ANKI3DS_DECK_H
#define ANKI3DS_DECK_H

#include <stddef.h>

#define DECK_MAX_CARDS 256
#define DECK_MAX_ID_LENGTH 32
#define DECK_MAX_TEXT_LENGTH 384
#define DECK_MAX_TAGS_LENGTH 128
#define DECK_MAX_NAME_LENGTH 64
#define DECK_MAX_MEDIA_NAME_LENGTH 96
#define DECK_MAX_LINE_LENGTH 1024

struct card
{
	char card_id[DECK_MAX_ID_LENGTH];
	char note_id[DECK_MAX_ID_LENGTH];
	char front[DECK_MAX_TEXT_LENGTH];
	char back[DECK_MAX_TEXT_LENGTH];
	char tags[DECK_MAX_TAGS_LENGTH];
	char front_media[DECK_MAX_MEDIA_NAME_LENGTH];
	char back_media[DECK_MAX_MEDIA_NAME_LENGTH];
};

struct deck
{
	char name[DECK_MAX_NAME_LENGTH];
	size_t card_count;
	struct card cards[DECK_MAX_CARDS];
};

enum deck_parse_result
{
	DECK_PARSE_OK,
	DECK_PARSE_EMPTY,
	DECK_PARSE_BAD_FIELD_COUNT,
	DECK_PARSE_FIELD_TOO_LONG,
	DECK_PARSE_MISSING_REQUIRED_FIELD,
	DECK_PARSE_BAD_ESCAPE,
	DECK_PARSE_BAD_MEDIA_NAME,
};

enum deck_load_result
{
	DECK_LOAD_OK,
	DECK_LOAD_NOT_FOUND,
	DECK_LOAD_BAD_FORMAT,
	DECK_LOAD_TOO_LARGE,
	DECK_LOAD_OUT_OF_MEMORY,
};

void deck_init(struct deck *deck, const char *name);
enum deck_parse_result deck_parse_card_line(struct card *card, const char *line);
enum deck_load_result deck_load_cards(struct deck *deck, const char *path);
const char *deck_parse_result_name(enum deck_parse_result result);
const char *deck_load_result_name(enum deck_load_result result);

#endif
