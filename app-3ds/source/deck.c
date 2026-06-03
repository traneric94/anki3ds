#include "deck.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CARD_MIN_FIELD_COUNT 5
#define CARD_MAX_FIELD_COUNT 7

static void copy_string(char *destination, size_t destination_size, const char *source)
{
	if (destination_size == 0)
		return;

	snprintf(destination, destination_size, "%s", source);
}

static bool trim_line_end(char *line)
{
	size_t length = strlen(line);

	while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
	{
		line[length - 1] = '\0';
		length--;
	}

	return true;
}

static bool line_has_complete_read(FILE *file, const char *line)
{
	size_t length = strlen(line);

	if (length == 0)
		return true;

	return line[length - 1] == '\n' || line[length - 1] == '\r' || feof(file);
}

static void consume_line_remainder(FILE *file)
{
	int value;

	do
	{
		value = fgetc(file);
	}
	while (value != EOF && value != '\n');
}

static enum deck_parse_result copy_unescaped_field(
	char *destination,
	size_t destination_size,
	const char *source
)
{
	size_t output_index = 0;

	if (destination_size == 0)
		return DECK_PARSE_FIELD_TOO_LONG;

	for (size_t input_index = 0; source[input_index] != '\0'; input_index++)
	{
		char value = source[input_index];

		if (value == '\\')
		{
			input_index++;
			value = source[input_index];

			if (value == '\0')
				return DECK_PARSE_BAD_ESCAPE;
			if (value == 'n')
				value = '\n';
			else if (value == 't')
				value = '\t';
			else if (value != '\\')
				return DECK_PARSE_BAD_ESCAPE;
		}

		if (output_index + 1 >= destination_size)
			return DECK_PARSE_FIELD_TOO_LONG;

		destination[output_index] = value;
		output_index++;
	}

	destination[output_index] = '\0';
	return DECK_PARSE_OK;
}

static enum deck_parse_result copy_card_field(
	struct card *card,
	size_t field_index,
	const char *field
)
{
	switch (field_index)
	{
	case 0:
		return copy_unescaped_field(card->card_id, sizeof(card->card_id), field);
	case 1:
		return copy_unescaped_field(card->note_id, sizeof(card->note_id), field);
	case 2:
		return copy_unescaped_field(card->front, sizeof(card->front), field);
	case 3:
		return copy_unescaped_field(card->back, sizeof(card->back), field);
	case 4:
		return copy_unescaped_field(card->tags, sizeof(card->tags), field);
	case 5:
		return copy_unescaped_field(
			card->front_media,
			sizeof(card->front_media),
			field
		);
	case 6:
		return copy_unescaped_field(
			card->back_media,
			sizeof(card->back_media),
			field
		);
	default:
		return DECK_PARSE_BAD_FIELD_COUNT;
	}
}

static bool media_name_character_is_valid(char value)
{
	if (value >= 'a' && value <= 'z')
		return true;
	if (value >= 'A' && value <= 'Z')
		return true;
	if (value >= '0' && value <= '9')
		return true;

	return value == '-' || value == '_' || value == '.';
}

static bool card_id_is_valid(const char *card_id)
{
	for (size_t index = 0; card_id[index] != '\0'; index++)
	{
		unsigned char value = (unsigned char)card_id[index];

		if (value < 32 || value == 127)
			return false;
	}

	return true;
}

static bool media_name_is_valid(const char *name)
{
	if (name[0] == '\0')
		return true;
	if (name[0] == '.')
		return false;

	for (size_t index = 0; name[index] != '\0'; index++)
	{
		if (!media_name_character_is_valid(name[index]))
			return false;
	}

	return true;
}

static bool deck_has_card_id(const struct deck *deck, const char *card_id)
{
	for (size_t index = 0; index < deck->card_count; index++)
	{
		if (strcmp(deck->cards[index].card_id, card_id) == 0)
			return true;
	}

	return false;
}

void deck_init(struct deck *deck, const char *name)
{
	memset(deck, 0, sizeof(*deck));
	copy_string(deck->name, sizeof(deck->name), name);
}

enum deck_parse_result deck_parse_card_line(struct card *card, const char *line)
{
	char line_copy[DECK_MAX_LINE_LENGTH];
	char *field_start = line_copy;
	size_t field_index = 0;

	memset(card, 0, sizeof(*card));

	if (strlen(line) >= sizeof(line_copy))
		return DECK_PARSE_FIELD_TOO_LONG;

	copy_string(line_copy, sizeof(line_copy), line);
	trim_line_end(line_copy);

	if (line_copy[0] == '\0')
		return DECK_PARSE_EMPTY;

	for (char *cursor = line_copy; ; cursor++)
	{
		if (*cursor == '\t' || *cursor == '\0')
		{
			char previous = *cursor;
			enum deck_parse_result result;

			*cursor = '\0';

			if (field_index >= CARD_MAX_FIELD_COUNT)
				return DECK_PARSE_BAD_FIELD_COUNT;

			result = copy_card_field(card, field_index, field_start);
			if (result != DECK_PARSE_OK)
				return result;

			field_index++;

			if (previous == '\0')
				break;

			field_start = cursor + 1;
		}
	}

	if (field_index != CARD_MIN_FIELD_COUNT && field_index != CARD_MAX_FIELD_COUNT)
		return DECK_PARSE_BAD_FIELD_COUNT;

	if (card->card_id[0] == '\0' || card->front[0] == '\0' || card->back[0] == '\0')
		return DECK_PARSE_MISSING_REQUIRED_FIELD;
	if (!card_id_is_valid(card->card_id))
		return DECK_PARSE_BAD_CARD_ID;
	if (!media_name_is_valid(card->front_media) || !media_name_is_valid(card->back_media))
		return DECK_PARSE_BAD_MEDIA_NAME;

	return DECK_PARSE_OK;
}

enum deck_load_result deck_load_cards(struct deck *deck, const char *path)
{
	FILE *file;
	struct deck *loaded;
	char line[DECK_MAX_LINE_LENGTH];
	enum deck_load_result result = DECK_LOAD_OK;

	if (deck == NULL || path == NULL)
		return DECK_LOAD_BAD_FORMAT;

	file = fopen(path, "r");
	if (file == NULL)
		return DECK_LOAD_NOT_FOUND;

	loaded = malloc(sizeof(*loaded));
	if (loaded == NULL)
	{
		fclose(file);
		return DECK_LOAD_OUT_OF_MEMORY;
	}

	deck_init(loaded, deck->name);

	while (fgets(line, sizeof(line), file) != NULL)
	{
		struct card card;
		enum deck_parse_result parse_result = deck_parse_card_line(&card, line);

		if (!line_has_complete_read(file, line))
		{
			consume_line_remainder(file);
			result = DECK_LOAD_BAD_FORMAT;
			break;
		}

		if (parse_result == DECK_PARSE_EMPTY)
			continue;
		if (parse_result != DECK_PARSE_OK)
		{
			result = DECK_LOAD_BAD_FORMAT;
			break;
		}
		if (loaded->card_count >= DECK_MAX_CARDS)
		{
			result = DECK_LOAD_TOO_LARGE;
			break;
		}
		if (deck_has_card_id(loaded, card.card_id))
		{
			result = DECK_LOAD_BAD_FORMAT;
			break;
		}

		loaded->cards[loaded->card_count] = card;
		loaded->card_count++;
	}

	if (result == DECK_LOAD_OK && ferror(file))
		result = DECK_LOAD_BAD_FORMAT;

	fclose(file);

	if (result == DECK_LOAD_OK && loaded->card_count == 0)
		result = DECK_LOAD_BAD_FORMAT;
	if (result == DECK_LOAD_OK)
		*deck = *loaded;

	free(loaded);
	return result;
}

const char *deck_parse_result_name(enum deck_parse_result result)
{
	switch (result)
	{
	case DECK_PARSE_OK:
		return "ok";
	case DECK_PARSE_EMPTY:
		return "empty";
	case DECK_PARSE_BAD_FIELD_COUNT:
		return "bad field count";
	case DECK_PARSE_FIELD_TOO_LONG:
		return "field too long";
	case DECK_PARSE_MISSING_REQUIRED_FIELD:
		return "missing required field";
	case DECK_PARSE_BAD_ESCAPE:
		return "bad escape";
	case DECK_PARSE_BAD_CARD_ID:
		return "bad card id";
	case DECK_PARSE_BAD_MEDIA_NAME:
		return "bad media name";
	}

	return "unknown";
}

const char *deck_load_result_name(enum deck_load_result result)
{
	switch (result)
	{
	case DECK_LOAD_OK:
		return "ok";
	case DECK_LOAD_NOT_FOUND:
		return "not found";
	case DECK_LOAD_BAD_FORMAT:
		return "bad format";
	case DECK_LOAD_TOO_LARGE:
		return "too large";
	case DECK_LOAD_OUT_OF_MEMORY:
		return "out of memory";
	}

	return "unknown";
}
