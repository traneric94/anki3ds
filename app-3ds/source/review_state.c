#include "review_state.h"

#include "storage.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STATE_LEGACY_FIELD_COUNT 4
#define STATE_PREVIOUS_FIELD_COUNT 7
#define STATE_SUSPENDED_FIELD_COUNT 8
#define STATE_CURRENT_FIELD_COUNT 10
#define STATE_MAX_LINE_LENGTH 192
#define STATE_FILE_HEADER "#anki3ds-state-v1"
#define STATE_FILE_FOOTER "#anki3ds-state-complete"

struct parsed_state
{
	char *card_id;
	bool suspended;
	unsigned int review_count;
	enum scheduler_rating last_rating;
	unsigned int first_review_day;
	unsigned int last_review_day;
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
	if (!parse_unsigned_field(fields[2], SCHEDULER_MAX_REVIEW_COUNT, &state->review_count))
		return false;
	if (!parse_unsigned_field(fields[3], SCHEDULER_RATING_COUNT - 1, &rating_value))
		return false;

	state->card_id = fields[0];
	state->last_rating = (enum scheduler_rating)rating_value;
	state->suspended = false;
	state->first_review_day = 0;
	state->last_review_day = 0;
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
	unsigned int first_review_day = 0;
	unsigned int last_review_day = 0;

	if (fields[0][0] == '\0')
		return false;
	if (!parse_unsigned_field(fields[1], SCHEDULER_MAX_REVIEW_COUNT, &state->review_count))
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
	if (!parse_unsigned_field(fields[6], SCHEDULER_MAX_LAPSES, &state->lapses))
		return false;
	if (
		(field_count == STATE_SUSPENDED_FIELD_COUNT || field_count == STATE_CURRENT_FIELD_COUNT) &&
		!parse_unsigned_field(fields[7], 1, &suspended_value)
	)
	{
		return false;
	}
	if (
		field_count == STATE_CURRENT_FIELD_COUNT &&
		(
			!parse_unsigned_field(fields[8], SCHEDULER_MAX_DAY, &first_review_day) ||
			!parse_unsigned_field(fields[9], SCHEDULER_MAX_DAY, &last_review_day)
		)
	)
	{
		return false;
	}

	state->card_id = fields[0];
	state->last_rating = (enum scheduler_rating)rating_value;
	state->suspended = suspended_value != 0;
	state->first_review_day = first_review_day;
	state->last_review_day = last_review_day;
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
		field_count == STATE_SUSPENDED_FIELD_COUNT ||
		field_count == STATE_CURRENT_FIELD_COUNT
	)
	{
		return parse_current_state_fields(fields, state, field_count);
	}

	return false;
}

static bool parse_state_metadata_line(
	char *line,
	bool *saw_header,
	bool *saw_footer,
	unsigned int *header_row_count,
	unsigned int *footer_row_count
)
{
	char *fields[STATE_CURRENT_FIELD_COUNT];
	size_t field_count;
	unsigned int row_count;

	trim_line_end(line);

	if (!split_state_line(line, fields, &field_count))
		return false;
	if (field_count != 2)
		return false;
	if (!parse_unsigned_field(fields[1], DECK_MAX_CARDS, &row_count))
		return false;

	if (strcmp(fields[0], STATE_FILE_HEADER) == 0)
	{
		if (*saw_header || *saw_footer)
			return false;

		*saw_header = true;
		*header_row_count = row_count;
		return true;
	}
	if (strcmp(fields[0], STATE_FILE_FOOTER) == 0)
	{
		if (!*saw_header || *saw_footer)
			return false;

		*saw_footer = true;
		*footer_row_count = row_count;
		return true;
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

static enum review_state_load_result review_state_load_file(
	const struct deck *deck,
	struct scheduler_session *session,
	const char *path,
	bool *loaded_file
)
{
	FILE *file = fopen(path, "r");
	char line[STATE_MAX_LINE_LENGTH];
	struct scheduler_session staged = *session;
	bool matched_cards[DECK_MAX_CARDS];
	size_t matched_row_count = 0;
	unsigned int parsed_row_count = 0;
	unsigned int header_row_count = 0;
	unsigned int footer_row_count = 0;
	bool saw_header = false;
	bool saw_footer = false;

	if (file == NULL)
		return REVIEW_STATE_LOAD_NOT_FOUND;

	*loaded_file = true;

	memset(matched_cards, 0, sizeof(matched_cards));
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

		if (line[0] == '#')
		{
			if (!saw_header && parsed_row_count > 0)
			{
				fclose(file);
				return REVIEW_STATE_LOAD_BAD_FORMAT;
			}
			if (
				!parse_state_metadata_line(
					line,
					&saw_header,
					&saw_footer,
					&header_row_count,
					&footer_row_count
				)
			)
			{
				fclose(file);
				return REVIEW_STATE_LOAD_BAD_FORMAT;
			}

			continue;
		}
		if (saw_footer)
		{
			fclose(file);
			return REVIEW_STATE_LOAD_BAD_FORMAT;
		}
		if (!parse_state_line(line, &state, session->today))
		{
			fclose(file);
			return REVIEW_STATE_LOAD_BAD_FORMAT;
		}

		parsed_row_count++;
		card_index = find_card_index(deck, state.card_id);
		if (card_index == deck->card_count)
			continue;
		if (matched_cards[card_index])
		{
			fclose(file);
			return REVIEW_STATE_LOAD_BAD_FORMAT;
		}

		matched_cards[card_index] = true;
		matched_row_count++;

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
				state.suspended,
				state.first_review_day,
				state.last_review_day
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
	if (
		saw_header != saw_footer ||
		(
			saw_header &&
			(
				header_row_count != parsed_row_count ||
				footer_row_count != parsed_row_count
			)
		)
	)
	{
		fclose(file);
		return REVIEW_STATE_LOAD_BAD_FORMAT;
	}
	if (matched_row_count == 0 && deck->card_count > 0)
	{
		fclose(file);
		return REVIEW_STATE_LOAD_BAD_FORMAT;
	}

	fclose(file);
	*session = staged;
	scheduler_reposition(session);
	return REVIEW_STATE_LOAD_OK;
}

enum review_state_load_result review_state_load(
	const struct deck *deck,
	struct scheduler_session *session,
	const char *path
)
{
	char temp_path[STORAGE_MAX_PATH_LENGTH];
	char backup_path[STORAGE_MAX_PATH_LENGTH];
	bool loaded_file = false;
	bool has_temp_path;
	bool has_backup_path;
	bool primary_missing;
	enum review_state_load_result result;

	result = review_state_load_file(deck, session, path, &loaded_file);
	if (result == REVIEW_STATE_LOAD_OK)
		return REVIEW_STATE_LOAD_OK;
	primary_missing = result == REVIEW_STATE_LOAD_NOT_FOUND;
	has_temp_path = storage_build_suffixed_path(
		temp_path,
		sizeof(temp_path),
		path,
		STORAGE_TEMP_SUFFIX
	);
	has_backup_path = storage_build_suffixed_path(
		backup_path,
		sizeof(backup_path),
		path,
		STORAGE_BACKUP_SUFFIX
	);

	if (primary_missing && has_temp_path)
	{
		result = review_state_load_file(deck, session, temp_path, &loaded_file);
		if (result == REVIEW_STATE_LOAD_OK)
			return REVIEW_STATE_LOAD_OK;
	}

	if (has_backup_path)
	{
		result = review_state_load_file(deck, session, backup_path, &loaded_file);
		if (result == REVIEW_STATE_LOAD_OK)
			return REVIEW_STATE_LOAD_OK;
	}

	if (!primary_missing && has_temp_path)
	{
		result = review_state_load_file(deck, session, temp_path, &loaded_file);
		if (result == REVIEW_STATE_LOAD_OK)
			return REVIEW_STATE_LOAD_OK;
	}
	if (!loaded_file)
		return REVIEW_STATE_LOAD_NOT_FOUND;

	return REVIEW_STATE_LOAD_BAD_FORMAT;
}

enum review_state_save_result review_state_save(
	const struct deck *deck,
	const struct scheduler_session *session,
	const char *path
)
{
	char temp_path[STORAGE_MAX_PATH_LENGTH];
	FILE *file;
	size_t row_count;

	if (!storage_build_suffixed_path(
		temp_path,
		sizeof(temp_path),
		path,
		STORAGE_TEMP_SUFFIX
	))
	{
		return REVIEW_STATE_SAVE_FAILED;
	}

	file = fopen(temp_path, "w");
	if (file == NULL)
		return REVIEW_STATE_SAVE_FAILED;

	row_count = deck->card_count < session->card_count ?
		deck->card_count :
		session->card_count;
	if (fprintf(file, "%s\t%u\n", STATE_FILE_HEADER, (unsigned int)row_count) < 0)
	{
		fclose(file);
		remove(temp_path);
		return REVIEW_STATE_SAVE_FAILED;
	}

	for (size_t index = 0; index < row_count; index++)
	{
		const struct scheduler_card *state = &session->cards[index];

		if (fprintf(
			file,
			"%s\t%u\t%s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n",
			deck->cards[index].card_id,
			state->review_count,
			rating_to_field(state->last_rating),
			state->due_day,
			state->interval_days,
			state->ease_permille,
			state->lapses,
			state->suspended ? 1u : 0u,
			state->first_review_day,
			state->last_review_day
		) < 0)
		{
			fclose(file);
			remove(temp_path);
			return REVIEW_STATE_SAVE_FAILED;
		}
	}
	if (fprintf(file, "%s\t%u\n", STATE_FILE_FOOTER, (unsigned int)row_count) < 0)
	{
		fclose(file);
		remove(temp_path);
		return REVIEW_STATE_SAVE_FAILED;
	}

	if (fclose(file) != 0)
	{
		remove(temp_path);
		return REVIEW_STATE_SAVE_FAILED;
	}

	if (!storage_replace_file(path))
		return REVIEW_STATE_SAVE_FAILED;

	return REVIEW_STATE_SAVE_OK;
}

bool review_state_delete(const char *path)
{
	return storage_delete_save_files(path);
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
