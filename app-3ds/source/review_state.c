#include "review_state.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STATE_LEGACY_FIELD_COUNT 4
#define STATE_PREVIOUS_FIELD_COUNT 7
#define STATE_CURRENT_FIELD_COUNT 8
#define STATE_MAX_LINE_LENGTH 192
#define STATE_MAX_PATH_LENGTH 256

struct parsed_state
{
	char *card_id;
	bool suspended;
	unsigned int review_count;
	enum scheduler_rating last_rating;
	unsigned int due_day;
	unsigned int interval_days;
	unsigned int ease_permille;
	unsigned int lapses;
};

static const char *rating_to_field(enum scheduler_rating rating)
{
	switch (rating)
	{
	case SCHEDULER_RATING_AGAIN:
		return "0";
	case SCHEDULER_RATING_HARD:
		return "1";
	case SCHEDULER_RATING_GOOD:
		return "2";
	case SCHEDULER_RATING_EASY:
		return "3";
	case SCHEDULER_RATING_COUNT:
		break;
	}

	return "0";
}

static bool parse_unsigned_field(const char *field, unsigned int max, unsigned int *value)
{
	char *end = NULL;
	unsigned long parsed;

	errno = 0;
	parsed = strtoul(field, &end, 10);

	if (errno != 0 || end == field || *end != '\0' || parsed > max)
		return false;

	*value = (unsigned int)parsed;
	return true;
}

static void trim_line_end(char *line)
{
	size_t length = strlen(line);

	while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
	{
		line[length - 1] = '\0';
		length--;
	}
}

static bool line_needs_more_input(FILE *file, const char *line)
{
	size_t length = strlen(line);

	if (length == 0)
		return false;
	if (line[length - 1] == '\n' || line[length - 1] == '\r')
		return false;

	return !feof(file);
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

static bool split_state_line(
	char *line,
	char *fields[STATE_CURRENT_FIELD_COUNT],
	size_t *field_count
)
{
	size_t field_index = 0;
	char *field_start = line;

	for (char *cursor = line; ; cursor++)
	{
		if (*cursor == '\t' || *cursor == '\0')
		{
			char previous = *cursor;

			if (field_index >= STATE_CURRENT_FIELD_COUNT)
				return false;

			*cursor = '\0';
			fields[field_index] = field_start;
			field_index++;

			if (previous == '\0')
				break;

			field_start = cursor + 1;
		}
	}

	*field_count = field_index;
	return true;
}

static bool parse_legacy_state_fields(
	char *fields[STATE_CURRENT_FIELD_COUNT],
	struct parsed_state *state,
	unsigned int today
)
{
	unsigned int done_value;
	unsigned int rating_value;

	if (fields[0][0] == '\0')
		return false;
	if (!parse_unsigned_field(fields[1], 1, &done_value))
		return false;
	if (!parse_unsigned_field(fields[2], 1000000, &state->review_count))
		return false;
	if (!parse_unsigned_field(fields[3], SCHEDULER_RATING_COUNT - 1, &rating_value))
		return false;

	state->card_id = fields[0];
	state->last_rating = (enum scheduler_rating)rating_value;
	state->suspended = false;
	state->due_day = done_value && today < SCHEDULER_MAX_DAY ? today + 1 : today;
	state->interval_days = done_value ? 1 : 0;
	state->ease_permille = SCHEDULER_DEFAULT_EASE_PERMILLE;
	state->lapses = 0;
	return true;
}

static bool parse_current_state_fields(
	char *fields[STATE_CURRENT_FIELD_COUNT],
	struct parsed_state *state,
	size_t field_count
)
{
	unsigned int rating_value;
	unsigned int suspended_value = 0;

	if (fields[0][0] == '\0')
		return false;
	if (!parse_unsigned_field(fields[1], 1000000, &state->review_count))
		return false;
	if (!parse_unsigned_field(fields[2], SCHEDULER_RATING_COUNT - 1, &rating_value))
		return false;
	if (!parse_unsigned_field(fields[3], SCHEDULER_MAX_DAY, &state->due_day))
		return false;
	if (!parse_unsigned_field(fields[4], SCHEDULER_MAX_INTERVAL_DAYS, &state->interval_days))
		return false;
	if (
		!parse_unsigned_field(
			fields[5],
			SCHEDULER_MAX_EASE_PERMILLE,
			&state->ease_permille
		)
	)
	{
		return false;
	}
	if (state->ease_permille < SCHEDULER_MIN_EASE_PERMILLE)
		return false;
	if (!parse_unsigned_field(fields[6], 1000000, &state->lapses))
		return false;
	if (
		field_count == STATE_CURRENT_FIELD_COUNT &&
		!parse_unsigned_field(fields[7], 1, &suspended_value)
	)
	{
		return false;
	}

	state->card_id = fields[0];
	state->last_rating = (enum scheduler_rating)rating_value;
	state->suspended = suspended_value != 0;
	return true;
}

static bool parse_state_line(
	char *line,
	struct parsed_state *state,
	unsigned int today
)
{
	char *fields[STATE_CURRENT_FIELD_COUNT];
	size_t field_count;

	trim_line_end(line);

	if (line[0] == '\0')
		return false;
	if (!split_state_line(line, fields, &field_count))
		return false;

	memset(state, 0, sizeof(*state));

	if (field_count == STATE_LEGACY_FIELD_COUNT)
		return parse_legacy_state_fields(fields, state, today);
	if (
		field_count == STATE_PREVIOUS_FIELD_COUNT ||
		field_count == STATE_CURRENT_FIELD_COUNT
	)
	{
		return parse_current_state_fields(fields, state, field_count);
	}

	return false;
}

static size_t find_card_index(const struct deck *deck, const char *card_id)
{
	for (size_t index = 0; index < deck->card_count; index++)
	{
		if (strcmp(deck->cards[index].card_id, card_id) == 0)
			return index;
	}

	return deck->card_count;
}

enum review_state_load_result review_state_load(
	const struct deck *deck,
	struct scheduler_session *session,
	const char *path
)
{
	FILE *file = fopen(path, "r");
	char line[STATE_MAX_LINE_LENGTH];
	struct scheduler_session staged = *session;

	if (file == NULL)
		return REVIEW_STATE_LOAD_NOT_FOUND;

	staged.undo.available = false;
	staged.undo.kind = SCHEDULER_UNDO_NONE;

	while (fgets(line, sizeof(line), file) != NULL)
	{
		struct parsed_state state;
		size_t card_index;

		if (line_needs_more_input(file, line))
		{
			consume_line_remainder(file);
			fclose(file);
			return REVIEW_STATE_LOAD_BAD_FORMAT;
		}

		if (!parse_state_line(line, &state, session->today))
		{
			fclose(file);
			return REVIEW_STATE_LOAD_BAD_FORMAT;
		}

		card_index = find_card_index(deck, state.card_id);
		if (card_index == deck->card_count)
			continue;

		if (
			!scheduler_restore_card(
				&staged,
				card_index,
				state.review_count,
				state.last_rating,
				state.due_day,
				state.interval_days,
				state.ease_permille,
				state.lapses,
				state.suspended
			)
		)
		{
			fclose(file);
			return REVIEW_STATE_LOAD_BAD_FORMAT;
		}
	}

	if (ferror(file))
	{
		fclose(file);
		return REVIEW_STATE_LOAD_BAD_FORMAT;
	}

	fclose(file);
	*session = staged;
	scheduler_reposition(session);
	return REVIEW_STATE_LOAD_OK;
}

enum review_state_save_result review_state_save(
	const struct deck *deck,
	const struct scheduler_session *session,
	const char *path
)
{
	char temp_path[STATE_MAX_PATH_LENGTH];
	FILE *file;

	if (snprintf(temp_path, sizeof(temp_path), "%s.tmp", path) >= (int)sizeof(temp_path))
		return REVIEW_STATE_SAVE_FAILED;

	file = fopen(temp_path, "w");
	if (file == NULL)
		return REVIEW_STATE_SAVE_FAILED;

	for (size_t index = 0; index < deck->card_count && index < session->card_count; index++)
	{
		const struct scheduler_card *state = &session->cards[index];

		if (fprintf(
			file,
			"%s\t%u\t%s\t%u\t%u\t%u\t%u\t%u\n",
			deck->cards[index].card_id,
			state->review_count,
			rating_to_field(state->last_rating),
			state->due_day,
			state->interval_days,
			state->ease_permille,
			state->lapses,
			state->suspended ? 1u : 0u
		) < 0)
		{
			fclose(file);
			remove(temp_path);
			return REVIEW_STATE_SAVE_FAILED;
		}
	}

	if (fclose(file) != 0)
	{
		remove(temp_path);
		return REVIEW_STATE_SAVE_FAILED;
	}

	remove(path);
	if (rename(temp_path, path) != 0)
	{
		remove(temp_path);
		return REVIEW_STATE_SAVE_FAILED;
	}

	return REVIEW_STATE_SAVE_OK;
}

const char *review_state_load_result_name(enum review_state_load_result result)
{
	switch (result)
	{
	case REVIEW_STATE_LOAD_OK:
		return "loaded";
	case REVIEW_STATE_LOAD_NOT_FOUND:
		return "new";
	case REVIEW_STATE_LOAD_BAD_FORMAT:
		return "ignored";
	}

	return "unknown";
}

const char *review_state_save_result_name(enum review_state_save_result result)
{
	switch (result)
	{
	case REVIEW_STATE_SAVE_OK:
		return "saved";
	case REVIEW_STATE_SAVE_FAILED:
		return "save failed";
	}

	return "unknown";
}
