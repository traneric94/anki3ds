#include "study_backend.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STUDY_BACKEND_TSV_FIELD_COUNT 5
#define STUDY_BACKEND_LINE_SIZE 2048
#define STUDY_BACKEND_STATE_LINE_SIZE 128
#define STUDY_BACKEND_STATE_PATH_SIZE 512
#define STUDY_BACKEND_STATE_VERSION 2
#define STUDY_BACKEND_MIN_STATE_VERSION 1
#define STUDY_BACKEND_MAX_INTERVAL_DAYS 36500u

struct study_backend_state_values
{
	unsigned long version;
	unsigned long card_count;
	unsigned long current_index;
	unsigned long progress_day;
	unsigned long reviewed_count;
	unsigned long reviewed_today_count;
	unsigned long introduced_count;
	unsigned long introduced_today_count;
	unsigned long completed_today_count;
	unsigned long again_count;
	unsigned long hard_count;
	unsigned long good_count;
	unsigned long easy_count;
	unsigned long suspended_count;
	unsigned long introduced_indices[STUDY_BACKEND_MAX_CARDS];
	unsigned long completed_today_indices[STUDY_BACKEND_MAX_CARDS];
	unsigned long suspended_indices[STUDY_BACKEND_MAX_CARDS];
	unsigned long schedule_indices[STUDY_BACKEND_MAX_CARDS];
	unsigned long schedule_due_days[STUDY_BACKEND_MAX_CARDS];
	unsigned long schedule_interval_days[STUDY_BACKEND_MAX_CARDS];
	size_t introduced_index_count;
	size_t completed_today_index_count;
	size_t suspended_index_count;
	size_t schedule_index_count;
	size_t schedule_due_day_count;
	size_t schedule_interval_day_count;
	bool seen_version;
	bool seen_card_count;
	bool seen_current_index;
	bool seen_progress_day;
	bool seen_reviewed_count;
	bool seen_reviewed_today_count;
	bool seen_introduced_count;
	bool seen_introduced_today_count;
	bool seen_completed_today_count;
	bool seen_again_count;
	bool seen_hard_count;
	bool seen_good_count;
	bool seen_easy_count;
	bool seen_suspended_count;
};

static void study_backend_copy_status(
	char *destination,
	size_t destination_size,
	const char *status
)
{
	if (destination == NULL || destination_size == 0)
		return;
	if (status == NULL)
	{
		destination[0] = '\0';
		return;
	}

	snprintf(destination, destination_size, "%s", status);
}

static void study_backend_set_deck_status(
	struct study_backend_deck *deck,
	const char *status
)
{
	if (deck == NULL)
		return;

	study_backend_copy_status(
		deck->status_text,
		sizeof(deck->status_text),
		status
	);
}

void study_backend_set_status(
	struct study_backend *backend,
	const char *status
)
{
	if (backend == NULL)
		return;

	study_backend_copy_status(
		backend->status_text,
		sizeof(backend->status_text),
		status
	);
}

const char *study_backend_rating_name(enum study_backend_rating rating)
{
	switch (rating)
	{
	case STUDY_BACKEND_RATING_AGAIN:
		return "Again";
	case STUDY_BACKEND_RATING_HARD:
		return "Hard";
	case STUDY_BACKEND_RATING_GOOD:
		return "Good";
	case STUDY_BACKEND_RATING_EASY:
		return "Easy";
	}

	return "Rating";
}

const char *study_backend_scheduler_policy_name(
	enum study_backend_scheduler_policy policy
)
{
	switch (policy)
	{
	case STUDY_BACKEND_SCHEDULER_DUE_FIRST:
		return "Due first";
	case STUDY_BACKEND_SCHEDULER_CARD_COUNT_COOLDOWN:
		return "Card cooldown";
	}

	return "Scheduler";
}

const char *study_backend_load_result_name(
	enum study_backend_load_result result
)
{
	switch (result)
	{
	case STUDY_BACKEND_LOAD_OK:
		return "Loaded";
	case STUDY_BACKEND_LOAD_NOT_FOUND:
		return "Not found";
	case STUDY_BACKEND_LOAD_EMPTY:
		return "Empty";
	case STUDY_BACKEND_LOAD_BAD_FORMAT:
		return "Bad format";
	case STUDY_BACKEND_LOAD_TOO_MANY_CARDS:
		return "Too many cards";
	case STUDY_BACKEND_LOAD_TEXT_TOO_LONG:
		return "Text too long";
	}

	return "Load error";
}

const char *study_backend_state_result_name(
	enum study_backend_state_result result
)
{
	switch (result)
	{
	case STUDY_BACKEND_STATE_OK:
		return "State loaded";
	case STUDY_BACKEND_STATE_NOT_FOUND:
		return "No saved state";
	case STUDY_BACKEND_STATE_BAD_FORMAT:
		return "Bad saved state";
	case STUDY_BACKEND_STATE_WRITE_FAILED:
		return "Save failed";
	}

	return "State error";
}

void study_backend_init(
	struct study_backend *backend,
	const struct study_backend_card *cards,
	size_t card_count
)
{
	if (backend == NULL)
		return;

	memset(backend, 0, sizeof(*backend));
	backend->cards = cards;
	backend->card_count = card_count <= STUDY_BACKEND_MAX_CARDS ?
		card_count :
		STUDY_BACKEND_MAX_CARDS;
	backend->scheduler_policy = STUDY_BACKEND_SCHEDULER_DUE_FIRST;
	backend->scheduler_cooldown_steps = STUDY_BACKEND_DEFAULT_CARD_COOLDOWN;
	study_backend_copy_status(
		backend->status_text,
		sizeof(backend->status_text),
		backend->card_count > 0 ? "Question" : "No cards loaded"
	);
}

void study_backend_set_scheduler_policy(
	struct study_backend *backend,
	enum study_backend_scheduler_policy policy,
	unsigned int cooldown_steps
)
{
	if (backend == NULL)
		return;

	switch (policy)
	{
	case STUDY_BACKEND_SCHEDULER_DUE_FIRST:
	case STUDY_BACKEND_SCHEDULER_CARD_COUNT_COOLDOWN:
		backend->scheduler_policy = policy;
		break;
	default:
		backend->scheduler_policy = STUDY_BACKEND_SCHEDULER_DUE_FIRST;
		break;
	}
	backend->scheduler_cooldown_steps = cooldown_steps;
	memset(backend->cooldown_remaining, 0, sizeof(backend->cooldown_remaining));
	memset(
		backend->undo_cooldown_remaining,
		0,
		sizeof(backend->undo_cooldown_remaining)
	);
}

static bool study_backend_escape_value(char escape, char *value)
{
	if (value == NULL)
		return false;

	switch (escape)
	{
	case 'n':
		*value = '\n';
		return true;
	case 'r':
		*value = '\r';
		return true;
	case 't':
		*value = '\t';
		return true;
	case '\\':
		*value = '\\';
		return true;
	}

	return false;
}

static enum study_backend_load_result study_backend_decode_text(
	const char *source,
	char *destination,
	size_t destination_size
)
{
	size_t output_index = 0;

	if (source == NULL || destination == NULL || destination_size == 0)
		return STUDY_BACKEND_LOAD_TEXT_TOO_LONG;

	for (size_t input_index = 0; source[input_index] != '\0'; input_index++)
	{
		char value = source[input_index];

		if (value == '\\')
		{
			input_index++;
			if (source[input_index] == '\0')
				return STUDY_BACKEND_LOAD_BAD_FORMAT;
			if (!study_backend_escape_value(source[input_index], &value))
				return STUDY_BACKEND_LOAD_BAD_FORMAT;
		}

		if (output_index + 1 >= destination_size)
			return STUDY_BACKEND_LOAD_TEXT_TOO_LONG;

		destination[output_index++] = value;
	}

	destination[output_index] = '\0';
	return STUDY_BACKEND_LOAD_OK;
}

static bool study_backend_line_complete(FILE *file, const char *line)
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

static size_t study_backend_split_tsv(
	char *row,
	char *fields[STUDY_BACKEND_TSV_FIELD_COUNT]
)
{
	size_t field_count = 1;

	fields[0] = row;
	for (char *cursor = row; *cursor != '\0'; cursor++)
	{
		if (*cursor != '\t')
			continue;

		*cursor = '\0';
		if (field_count >= STUDY_BACKEND_TSV_FIELD_COUNT)
			return STUDY_BACKEND_TSV_FIELD_COUNT + 1;

		fields[field_count++] = cursor + 1;
	}

	return field_count;
}

enum study_backend_load_result study_backend_load_cards_tsv(
	struct study_backend_deck *deck,
	const char *path
)
{
	FILE *file;
	char line[STUDY_BACKEND_LINE_SIZE];
	unsigned long line_number = 0;
	enum study_backend_load_result result = STUDY_BACKEND_LOAD_OK;

	if (deck == NULL)
		return STUDY_BACKEND_LOAD_BAD_FORMAT;

	memset(deck, 0, sizeof(*deck));
	if (path == NULL || path[0] == '\0')
	{
		study_backend_set_deck_status(deck, "No deck path");
		return STUDY_BACKEND_LOAD_NOT_FOUND;
	}

	file = fopen(path, "r");
	if (file == NULL)
	{
		study_backend_set_deck_status(deck, "No deck file");
		return STUDY_BACKEND_LOAD_NOT_FOUND;
	}

	while (fgets(line, sizeof(line), file) != NULL)
	{
		char *fields[STUDY_BACKEND_TSV_FIELD_COUNT];
		size_t field_count;
		size_t card_index;

		line_number++;
		if (!study_backend_line_complete(file, line))
		{
			snprintf(
				deck->status_text,
				sizeof(deck->status_text),
				"Line %lu too long",
				line_number
			);
			result = STUDY_BACKEND_LOAD_TEXT_TOO_LONG;
			break;
		}

		line[strcspn(line, "\r\n")] = '\0';
		if (line[0] == '\0' || line[0] == '#')
			continue;

		field_count = study_backend_split_tsv(line, fields);
		if (field_count != STUDY_BACKEND_TSV_FIELD_COUNT)
		{
			snprintf(
				deck->status_text,
				sizeof(deck->status_text),
				"Bad line %lu",
				line_number
			);
			result = STUDY_BACKEND_LOAD_BAD_FORMAT;
			break;
		}

		if (deck->card_count >= STUDY_BACKEND_MAX_CARDS)
		{
			snprintf(
				deck->status_text,
				sizeof(deck->status_text),
				"Too many cards"
			);
			result = STUDY_BACKEND_LOAD_TOO_MANY_CARDS;
			break;
		}

		card_index = deck->card_count;
		result = study_backend_decode_text(
			fields[2],
			deck->front_text[card_index],
			sizeof(deck->front_text[card_index])
		);
		if (result == STUDY_BACKEND_LOAD_OK)
			result = study_backend_decode_text(
				fields[3],
				deck->back_text[card_index],
				sizeof(deck->back_text[card_index])
			);
		if (result == STUDY_BACKEND_LOAD_OK)
			result = study_backend_decode_text(
				fields[4],
				deck->tags_text[card_index],
				sizeof(deck->tags_text[card_index])
			);
		if (result != STUDY_BACKEND_LOAD_OK)
		{
			if (result == STUDY_BACKEND_LOAD_TEXT_TOO_LONG)
				snprintf(
					deck->status_text,
					sizeof(deck->status_text),
					"Text too long line %lu",
					line_number
				);
			else
				snprintf(
					deck->status_text,
					sizeof(deck->status_text),
					"Bad escape line %lu",
					line_number
				);
			break;
		}

		deck->cards[card_index].front = deck->front_text[card_index];
		deck->cards[card_index].back = deck->back_text[card_index];
		deck->cards[card_index].tags = deck->tags_text[card_index];
		deck->card_count++;
	}

	if (ferror(file) && result == STUDY_BACKEND_LOAD_OK)
	{
		study_backend_set_deck_status(deck, "Read error");
		result = STUDY_BACKEND_LOAD_BAD_FORMAT;
	}
	fclose(file);

	if (result == STUDY_BACKEND_LOAD_OK && deck->card_count == 0)
	{
		study_backend_set_deck_status(deck, "No cards in deck");
		result = STUDY_BACKEND_LOAD_EMPTY;
	}
	else if (result == STUDY_BACKEND_LOAD_OK)
	{
		snprintf(
			deck->status_text,
			sizeof(deck->status_text),
			"Loaded %lu cards",
			(unsigned long)deck->card_count
		);
	}

	if (result != STUDY_BACKEND_LOAD_OK)
		deck->card_count = 0;

	return result;
}

static bool study_backend_parse_unsigned(
	const char *text,
	unsigned long *value
)
{
	char *end = NULL;
	unsigned long parsed;

	if (text == NULL || text[0] == '\0' || value == NULL)
		return false;

	errno = 0;
	parsed = strtoul(text, &end, 10);
	if (errno != 0 || end == text || *end != '\0')
		return false;

	*value = parsed;
	return true;
}

static bool study_backend_assign_state_value(
	struct study_backend_state_values *state,
	const char *key,
	unsigned long value
)
{
	if (state == NULL || key == NULL)
		return false;

	if (strcmp(key, "version") == 0)
	{
		state->version = value;
		state->seen_version = true;
		return true;
	}
	if (strcmp(key, "card_count") == 0)
	{
		state->card_count = value;
		state->seen_card_count = true;
		return true;
	}
	if (strcmp(key, "current_index") == 0)
	{
		state->current_index = value;
		state->seen_current_index = true;
		return true;
	}
	if (strcmp(key, "progress_day") == 0)
	{
		state->progress_day = value;
		state->seen_progress_day = true;
		return true;
	}
	if (strcmp(key, "reviewed_count") == 0)
	{
		state->reviewed_count = value;
		state->seen_reviewed_count = true;
		return true;
	}
	if (strcmp(key, "reviewed_today_count") == 0)
	{
		state->reviewed_today_count = value;
		state->seen_reviewed_today_count = true;
		return true;
	}
	if (strcmp(key, "introduced_count") == 0)
	{
		state->introduced_count = value;
		state->seen_introduced_count = true;
		return true;
	}
	if (strcmp(key, "introduced_today_count") == 0)
	{
		state->introduced_today_count = value;
		state->seen_introduced_today_count = true;
		return true;
	}
	if (strcmp(key, "completed_today_count") == 0)
	{
		state->completed_today_count = value;
		state->seen_completed_today_count = true;
		return true;
	}
	if (strcmp(key, "again_count") == 0)
	{
		state->again_count = value;
		state->seen_again_count = true;
		return true;
	}
	if (strcmp(key, "hard_count") == 0)
	{
		state->hard_count = value;
		state->seen_hard_count = true;
		return true;
	}
	if (strcmp(key, "good_count") == 0)
	{
		state->good_count = value;
		state->seen_good_count = true;
		return true;
	}
	if (strcmp(key, "easy_count") == 0)
	{
		state->easy_count = value;
		state->seen_easy_count = true;
		return true;
	}
	if (strcmp(key, "suspended_count") == 0)
	{
		state->suspended_count = value;
		state->seen_suspended_count = true;
		return true;
	}
	if (strcmp(key, "introduced_index") == 0)
	{
		if (state->introduced_index_count >= STUDY_BACKEND_MAX_CARDS)
			return false;

		state->introduced_indices[state->introduced_index_count++] = value;
		return true;
	}
	if (strcmp(key, "completed_today_index") == 0)
	{
		if (state->completed_today_index_count >= STUDY_BACKEND_MAX_CARDS)
			return false;

		state->completed_today_indices[
			state->completed_today_index_count++
		] = value;
		return true;
	}
	if (strcmp(key, "suspended_index") == 0)
	{
		if (state->suspended_index_count >= STUDY_BACKEND_MAX_CARDS)
			return false;

		state->suspended_indices[state->suspended_index_count++] = value;
		return true;
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

	return false;
}

static bool study_backend_state_values_complete(
	const struct study_backend_state_values *state
)
{
	return (
		state != NULL &&
		state->seen_version &&
		state->seen_card_count &&
		state->seen_current_index &&
		state->seen_reviewed_count &&
		state->seen_again_count &&
		state->seen_hard_count &&
		state->seen_good_count &&
		state->seen_easy_count
	);
}

static bool study_backend_state_values_valid(
	const struct study_backend *backend,
	const struct study_backend_state_values *state
)
{
	unsigned long rating_total;
	bool seen_suspended[STUDY_BACKEND_MAX_CARDS];
	bool seen_introduced[STUDY_BACKEND_MAX_CARDS];
	bool seen_completed_today[STUDY_BACKEND_MAX_CARDS];
	bool seen_schedule[STUDY_BACKEND_MAX_CARDS];

	if (backend == NULL || state == NULL)
		return false;
	if (!study_backend_state_values_complete(state))
		return false;
	if (
		state->version < STUDY_BACKEND_MIN_STATE_VERSION ||
		state->version > STUDY_BACKEND_STATE_VERSION
	)
	{
		return false;
	}
	if (state->card_count != backend->card_count)
		return false;
	if (state->current_index > state->card_count)
		return false;
	if (state->card_count > STUDY_BACKEND_MAX_CARDS)
		return false;
	if (state->seen_progress_day && state->progress_day > UINT_MAX)
		return false;
	if (state->reviewed_count > UINT_MAX)
		return false;
	if (state->seen_reviewed_today_count)
	{
		if (state->reviewed_today_count > state->reviewed_count)
			return false;
		if (state->reviewed_today_count > UINT_MAX)
			return false;
	}
	if (state->seen_introduced_count)
	{
		if (state->introduced_count > state->card_count)
			return false;
		if (state->introduced_count != state->introduced_index_count)
			return false;
	}
	else if (state->introduced_index_count != 0)
	{
		return false;
	}
	if (state->seen_introduced_today_count)
	{
		if (state->introduced_today_count > state->card_count)
			return false;
		if (
			state->seen_introduced_count &&
			state->introduced_today_count > state->introduced_count
		)
		{
			return false;
		}
		if (state->introduced_today_count > UINT_MAX)
			return false;
	}
	if (state->seen_completed_today_count)
	{
		if (!state->seen_introduced_count)
			return false;
		if (state->completed_today_count > state->card_count)
			return false;
		if (state->completed_today_count != state->completed_today_index_count)
			return false;
		if (
			state->seen_introduced_count &&
			state->completed_today_count > state->introduced_count
		)
		{
			return false;
		}
		if (
			state->seen_reviewed_today_count &&
			state->completed_today_count > state->reviewed_today_count
		)
		{
			return false;
		}
		if (state->completed_today_count > UINT_MAX)
			return false;
	}
	else if (state->completed_today_index_count != 0)
	{
		return false;
	}
	if (state->seen_suspended_count)
	{
		if (state->suspended_count > state->card_count)
			return false;
		if (state->suspended_count != state->suspended_index_count)
			return false;
	}
	else if (state->suspended_index_count != 0)
	{
		return false;
	}
	if (
		state->schedule_index_count != state->schedule_due_day_count ||
		state->schedule_index_count != state->schedule_interval_day_count
	)
	{
		return false;
	}
	if (state->schedule_index_count != 0 && !state->seen_introduced_count)
		return false;
	if (state->version < 2 && state->schedule_index_count != 0)
		return false;

	rating_total = state->again_count;
	if (rating_total > state->reviewed_count)
		return false;
	if (state->hard_count > state->reviewed_count - rating_total)
		return false;
	rating_total += state->hard_count;
	if (state->good_count > state->reviewed_count - rating_total)
		return false;
	rating_total += state->good_count;
	if (state->easy_count > state->reviewed_count - rating_total)
		return false;
	rating_total += state->easy_count;
	if (rating_total != state->reviewed_count)
		return false;

	memset(seen_introduced, 0, sizeof(seen_introduced));
	for (
		size_t introduced_index = 0;
		introduced_index < state->introduced_index_count;
		introduced_index++
	)
	{
		unsigned long card_index = state->introduced_indices[introduced_index];

		if (card_index >= state->card_count)
			return false;
		if (seen_introduced[card_index])
			return false;

		seen_introduced[card_index] = true;
	}

	memset(seen_completed_today, 0, sizeof(seen_completed_today));
	for (
		size_t completed_today_index = 0;
		completed_today_index < state->completed_today_index_count;
		completed_today_index++
	)
	{
		unsigned long card_index =
			state->completed_today_indices[completed_today_index];

		if (card_index >= state->card_count)
			return false;
		if (seen_completed_today[card_index])
			return false;
		if (state->seen_introduced_count && !seen_introduced[card_index])
			return false;

		seen_completed_today[card_index] = true;
	}

	memset(seen_suspended, 0, sizeof(seen_suspended));
	for (
		size_t suspended_index = 0;
		suspended_index < state->suspended_index_count;
		suspended_index++
	)
	{
		unsigned long card_index = state->suspended_indices[suspended_index];

		if (card_index >= state->card_count)
			return false;
		if (seen_suspended[card_index])
			return false;

		seen_suspended[card_index] = true;
	}

	memset(seen_schedule, 0, sizeof(seen_schedule));
	for (
		size_t schedule_index = 0;
		schedule_index < state->schedule_index_count;
		schedule_index++
	)
	{
		unsigned long card_index = state->schedule_indices[schedule_index];

		if (card_index >= state->card_count)
			return false;
		if (seen_schedule[card_index])
			return false;
		if (state->seen_introduced_count && !seen_introduced[card_index])
			return false;
		if (state->schedule_due_days[schedule_index] > UINT_MAX)
			return false;
		if (state->schedule_interval_days[schedule_index] > UINT_MAX)
			return false;
		if (
			state->schedule_interval_days[schedule_index] >
			STUDY_BACKEND_MAX_INTERVAL_DAYS
		)
		{
			return false;
		}

		seen_schedule[card_index] = true;
	}

	return true;
}

static size_t study_backend_legacy_introduced_count(
	const struct study_backend_state_values *state
)
{
	size_t introduced_count;

	if (state == NULL)
		return 0;
	if (state->current_index >= state->card_count)
		return (size_t)state->card_count;

	introduced_count = (size_t)state->current_index;
	if (state->reviewed_count > state->current_index)
		introduced_count++;
	if (introduced_count > state->card_count)
		introduced_count = (size_t)state->card_count;

	return introduced_count;
}

static bool study_backend_card_is_available(
	const struct study_backend *backend,
	size_t card_index
)
{
	return (
		backend != NULL &&
		backend->cards != NULL &&
		card_index < backend->card_count &&
		!backend->suspended[card_index]
	);
}

static bool study_backend_card_is_due(
	const struct study_backend *backend,
	size_t card_index
)
{
	if (!study_backend_card_is_available(backend, card_index))
		return false;

	if (!backend->introduced[card_index])
		return true;
	if (
		backend->progress_day != 0 &&
		backend->due_day[card_index] > backend->progress_day
	)
	{
		return false;
	}

	return (
		!backend->completed_today[card_index]
	);
}

static bool study_backend_card_cooldown_blocks_due(
	const struct study_backend *backend,
	size_t card_index
)
{
	return (
		backend != NULL &&
		backend->scheduler_policy ==
			STUDY_BACKEND_SCHEDULER_CARD_COUNT_COOLDOWN &&
		card_index < backend->card_count &&
		backend->cooldown_remaining[card_index] > 0
	);
}

static bool study_backend_find_next_due_introduced(
	const struct study_backend *backend,
	size_t *card_index,
	bool allow_cooldown_blocked
)
{
	if (backend == NULL)
		return false;

	for (size_t index = 0; index < backend->card_count; index++)
	{
		if (
			study_backend_card_is_due(backend, index) &&
			backend->introduced[index] &&
			(
				allow_cooldown_blocked ||
				!study_backend_card_cooldown_blocks_due(backend, index)
			)
		)
		{
			if (card_index != NULL)
				*card_index = index;
			return true;
		}
	}

	return false;
}

static bool study_backend_find_next_new(
	const struct study_backend *backend,
	size_t *card_index
)
{
	if (backend == NULL)
		return false;

	for (size_t index = 0; index < backend->card_count; index++)
	{
		if (
			study_backend_card_is_available(backend, index) &&
			!backend->introduced[index]
		)
		{
			if (card_index != NULL)
				*card_index = index;
			return true;
		}
	}

	return false;
}

static void study_backend_reposition_to_due(
	struct study_backend *backend
)
{
	size_t card_index;

	if (backend == NULL)
		return;

	if (study_backend_find_next_due_introduced(
		backend,
		&card_index,
		false
	))
	{
		backend->current_index = card_index;
		return;
	}

	if (study_backend_find_next_new(backend, &card_index))
	{
		backend->current_index = card_index;
		return;
	}

	if (
		backend->scheduler_policy ==
			STUDY_BACKEND_SCHEDULER_CARD_COUNT_COOLDOWN &&
		study_backend_find_next_due_introduced(backend, &card_index, true)
	)
	{
		backend->current_index = card_index;
		return;
	}

	backend->current_index = backend->card_count;
}

static bool study_backend_state_artifact_path(
	char *destination,
	size_t destination_size,
	const char *path,
	const char *suffix
)
{
	int written;

	if (
		destination == NULL ||
		destination_size == 0 ||
		path == NULL ||
		path[0] == '\0' ||
		suffix == NULL
	)
	{
		return false;
	}

	written = snprintf(destination, destination_size, "%s%s", path, suffix);
	return written >= 0 && (size_t)written < destination_size;
}

static enum study_backend_state_result study_backend_load_state_tsv_single(
	struct study_backend *backend,
	const char *path
)
{
	FILE *file;
	char line[STUDY_BACKEND_STATE_LINE_SIZE];
	struct study_backend_state_values state;

	if (backend == NULL || path == NULL || path[0] == '\0')
		return STUDY_BACKEND_STATE_BAD_FORMAT;

	file = fopen(path, "r");
	if (file == NULL)
		return STUDY_BACKEND_STATE_NOT_FOUND;

	memset(&state, 0, sizeof(state));
	while (fgets(line, sizeof(line), file) != NULL)
	{
		char *separator;
		unsigned long value;

		if (!study_backend_line_complete(file, line))
		{
			fclose(file);
			study_backend_set_status(backend, "Bad saved state");
			return STUDY_BACKEND_STATE_BAD_FORMAT;
		}

		line[strcspn(line, "\r\n")] = '\0';
		if (line[0] == '\0' || line[0] == '#')
			continue;

		separator = strchr(line, '\t');
		if (separator == NULL)
		{
			fclose(file);
			study_backend_set_status(backend, "Bad saved state");
			return STUDY_BACKEND_STATE_BAD_FORMAT;
		}
		*separator = '\0';
		if (!study_backend_parse_unsigned(separator + 1, &value))
		{
			fclose(file);
			study_backend_set_status(backend, "Bad saved state");
			return STUDY_BACKEND_STATE_BAD_FORMAT;
		}
		if (!study_backend_assign_state_value(&state, line, value))
		{
			fclose(file);
			study_backend_set_status(backend, "Bad saved state");
			return STUDY_BACKEND_STATE_BAD_FORMAT;
		}
	}

	if (ferror(file))
	{
		fclose(file);
		study_backend_set_status(backend, "Bad saved state");
		return STUDY_BACKEND_STATE_BAD_FORMAT;
	}
	fclose(file);

	if (!study_backend_state_values_valid(backend, &state))
	{
		study_backend_set_status(backend, "Bad saved state");
		return STUDY_BACKEND_STATE_BAD_FORMAT;
	}

	backend->current_index = (size_t)state.current_index;
	backend->progress_day = state.seen_progress_day ?
		(unsigned int)state.progress_day :
		0;
	backend->reviewed_count = (unsigned int)state.reviewed_count;
	backend->reviewed_today_count = state.seen_reviewed_today_count ?
		(unsigned int)state.reviewed_today_count :
		backend->reviewed_count;
	memset(backend->introduced, 0, sizeof(backend->introduced));
	backend->introduced_count = 0;
	if (state.seen_introduced_count)
	{
		for (
			size_t introduced_index = 0;
			introduced_index < state.introduced_index_count;
			introduced_index++
		)
		{
			size_t card_index = (size_t)state.introduced_indices[introduced_index];

			backend->introduced[card_index] = true;
			backend->introduced_count++;
		}
	}
	else
	{
		size_t introduced_count = study_backend_legacy_introduced_count(&state);

		for (size_t card_index = 0; card_index < introduced_count; card_index++)
			backend->introduced[card_index] = true;
		backend->introduced_count = (unsigned int)introduced_count;
	}
		backend->introduced_today_count = state.seen_introduced_today_count ?
			(unsigned int)state.introduced_today_count :
			backend->introduced_count;
		memset(backend->completed_today, 0, sizeof(backend->completed_today));
		backend->completed_today_count = 0;
		if (state.seen_completed_today_count)
		{
			for (
				size_t completed_today_index = 0;
				completed_today_index < state.completed_today_index_count;
				completed_today_index++
			)
			{
				size_t card_index =
					(size_t)state.completed_today_indices[completed_today_index];

				backend->completed_today[card_index] = true;
				backend->completed_today_count++;
			}
		}
		else if (backend->reviewed_today_count > 0)
		{
			for (
				size_t card_index = 0;
				card_index < backend->current_index &&
					card_index < backend->card_count &&
					backend->completed_today_count < backend->reviewed_today_count;
				card_index++
			)
			{
				if (!backend->introduced[card_index])
					continue;

				backend->completed_today[card_index] = true;
				backend->completed_today_count++;
			}
		}
		memset(backend->due_day, 0, sizeof(backend->due_day));
		memset(backend->interval_days, 0, sizeof(backend->interval_days));
		for (
			size_t schedule_index = 0;
			schedule_index < state.schedule_index_count;
			schedule_index++
		)
		{
			size_t card_index = (size_t)state.schedule_indices[schedule_index];

			backend->due_day[card_index] =
				(unsigned int)state.schedule_due_days[schedule_index];
			backend->interval_days[card_index] =
				(unsigned int)state.schedule_interval_days[schedule_index];
		}
		backend->again_count = (unsigned int)state.again_count;
	backend->hard_count = (unsigned int)state.hard_count;
	backend->good_count = (unsigned int)state.good_count;
	backend->easy_count = (unsigned int)state.easy_count;
	memset(backend->suspended, 0, sizeof(backend->suspended));
	backend->suspended_count = 0;
	for (
		size_t suspended_index = 0;
		suspended_index < state.suspended_index_count;
		suspended_index++
	)
	{
		size_t card_index = (size_t)state.suspended_indices[suspended_index];

		backend->suspended[card_index] = true;
		backend->suspended_count++;
	}
	backend->answer_visible = false;
	backend->undo_available = false;
	backend->undo_card_index = 0;
	backend->undo_rating = STUDY_BACKEND_RATING_AGAIN;
	backend->undo_due_day = 0;
	backend->undo_interval_days = 0;
	memset(backend->cooldown_remaining, 0, sizeof(backend->cooldown_remaining));
	memset(
		backend->undo_cooldown_remaining,
		0,
		sizeof(backend->undo_cooldown_remaining)
	);
	study_backend_reposition_to_due(backend);
	if (backend->current_index >= backend->card_count)
	{
		snprintf(
			backend->status_text,
			sizeof(backend->status_text),
			backend->suspended_count > 0 ?
				"Restored; %u suspended" :
				"Restored complete; reviewed %u",
			backend->suspended_count > 0 ?
				backend->suspended_count :
				backend->reviewed_count
		);
	}
	else
	{
		snprintf(
			backend->status_text,
			sizeof(backend->status_text),
			"Restored card %lu/%lu",
			(unsigned long)(backend->current_index + 1),
			(unsigned long)backend->card_count
		);
	}
	return STUDY_BACKEND_STATE_OK;
}

enum study_backend_state_result study_backend_load_state_tsv(
	struct study_backend *backend,
	const char *path
)
{
	char artifact_path[STUDY_BACKEND_STATE_PATH_SIZE];
	enum study_backend_state_result primary_result;

	primary_result = study_backend_load_state_tsv_single(backend, path);
	if (primary_result == STUDY_BACKEND_STATE_OK)
		return STUDY_BACKEND_STATE_OK;
	if (path == NULL || path[0] == '\0')
		return primary_result;

	if (
		study_backend_state_artifact_path(
			artifact_path,
			sizeof(artifact_path),
			path,
			".tmp"
		) &&
		study_backend_load_state_tsv_single(
			backend,
			artifact_path
		) == STUDY_BACKEND_STATE_OK
	)
	{
		return STUDY_BACKEND_STATE_OK;
	}

	if (
		study_backend_state_artifact_path(
			artifact_path,
			sizeof(artifact_path),
			path,
			".bak"
		) &&
		study_backend_load_state_tsv_single(
			backend,
			artifact_path
		) == STUDY_BACKEND_STATE_OK
	)
	{
		return STUDY_BACKEND_STATE_OK;
	}

	return primary_result;
}

static bool study_backend_file_exists(const char *path);

static bool study_backend_remove_if_present(const char *path)
{
	if (path == NULL || path[0] == '\0')
		return false;
	if (!study_backend_file_exists(path))
		return true;

	errno = 0;
	if (remove(path) == 0)
		return true;

	return errno == ENOENT;
}

static bool study_backend_file_exists(const char *path)
{
	FILE *file;

	if (path == NULL || path[0] == '\0')
		return false;

	file = fopen(path, "r");
	if (file == NULL)
		return false;

	return fclose(file) == 0;
}

static enum study_backend_state_result study_backend_write_state_tsv_file(
	const struct study_backend *backend,
	const char *path
)
{
	FILE *file;

	file = fopen(path, "w");
	if (file == NULL)
		return STUDY_BACKEND_STATE_WRITE_FAILED;

	if (
		fprintf(file, "version\t%d\n", STUDY_BACKEND_STATE_VERSION) < 0 ||
		fprintf(file, "card_count\t%lu\n", (unsigned long)backend->card_count) < 0 ||
		fprintf(
			file,
			"current_index\t%lu\n",
			(unsigned long)backend->current_index
		) < 0 ||
		fprintf(file, "progress_day\t%u\n", backend->progress_day) < 0 ||
		fprintf(file, "reviewed_count\t%u\n", backend->reviewed_count) < 0 ||
		fprintf(
			file,
			"reviewed_today_count\t%u\n",
			backend->reviewed_today_count
		) < 0 ||
		fprintf(file, "introduced_count\t%u\n", backend->introduced_count) < 0 ||
		fprintf(
			file,
			"introduced_today_count\t%u\n",
			backend->introduced_today_count
		) < 0 ||
		fprintf(
			file,
			"completed_today_count\t%u\n",
			backend->completed_today_count
		) < 0 ||
		fprintf(file, "again_count\t%u\n", backend->again_count) < 0 ||
		fprintf(file, "hard_count\t%u\n", backend->hard_count) < 0 ||
		fprintf(file, "good_count\t%u\n", backend->good_count) < 0 ||
		fprintf(file, "easy_count\t%u\n", backend->easy_count) < 0 ||
		fprintf(file, "suspended_count\t%u\n", backend->suspended_count) < 0
	)
	{
		fclose(file);
		return STUDY_BACKEND_STATE_WRITE_FAILED;
	}
	for (size_t card_index = 0; card_index < backend->card_count; card_index++)
	{
		if (!backend->introduced[card_index])
			continue;

		if (fprintf(file, "introduced_index\t%lu\n", (unsigned long)card_index) < 0)
		{
			fclose(file);
			return STUDY_BACKEND_STATE_WRITE_FAILED;
		}
	}
	for (size_t card_index = 0; card_index < backend->card_count; card_index++)
	{
		if (!backend->introduced[card_index])
			continue;
		if (
			backend->due_day[card_index] == 0 &&
			backend->interval_days[card_index] == 0
		)
		{
			continue;
		}

		if (
			fprintf(file, "schedule_index\t%lu\n", (unsigned long)card_index) < 0 ||
			fprintf(file, "schedule_due_day\t%u\n", backend->due_day[card_index]) < 0 ||
			fprintf(
				file,
				"schedule_interval_days\t%u\n",
				backend->interval_days[card_index]
			) < 0
		)
		{
			fclose(file);
			return STUDY_BACKEND_STATE_WRITE_FAILED;
		}
	}
	for (size_t card_index = 0; card_index < backend->card_count; card_index++)
	{
		if (!backend->completed_today[card_index])
			continue;

		if (
			fprintf(
				file,
				"completed_today_index\t%lu\n",
				(unsigned long)card_index
			) < 0
		)
		{
			fclose(file);
			return STUDY_BACKEND_STATE_WRITE_FAILED;
		}
	}
	for (size_t card_index = 0; card_index < backend->card_count; card_index++)
	{
		if (!backend->suspended[card_index])
			continue;

		if (fprintf(file, "suspended_index\t%lu\n", (unsigned long)card_index) < 0)
		{
			fclose(file);
			return STUDY_BACKEND_STATE_WRITE_FAILED;
		}
	}
	if (fclose(file) != 0)
		return STUDY_BACKEND_STATE_WRITE_FAILED;

	return STUDY_BACKEND_STATE_OK;
}

enum study_backend_state_result study_backend_save_state_tsv(
	const struct study_backend *backend,
	const char *path
)
{
	char tmp_path[STUDY_BACKEND_STATE_PATH_SIZE];
	char bak_path[STUDY_BACKEND_STATE_PATH_SIZE];
	bool backup_created = false;

	if (backend == NULL || path == NULL || path[0] == '\0')
		return STUDY_BACKEND_STATE_WRITE_FAILED;
	if (
		!study_backend_state_artifact_path(
			tmp_path,
			sizeof(tmp_path),
			path,
			".tmp"
		) ||
		!study_backend_state_artifact_path(
			bak_path,
			sizeof(bak_path),
			path,
			".bak"
		)
	)
	{
		return STUDY_BACKEND_STATE_WRITE_FAILED;
	}

	if (
		study_backend_write_state_tsv_file(
			backend,
			tmp_path
		) != STUDY_BACKEND_STATE_OK
	)
	{
		return STUDY_BACKEND_STATE_WRITE_FAILED;
	}
	if (!study_backend_remove_if_present(bak_path))
	{
		(void)remove(tmp_path);
		return STUDY_BACKEND_STATE_WRITE_FAILED;
	}
	if (study_backend_file_exists(path))
	{
		if (rename(path, bak_path) != 0)
		{
			(void)remove(tmp_path);
			return STUDY_BACKEND_STATE_WRITE_FAILED;
		}
		backup_created = true;
	}

	if (rename(tmp_path, path) != 0)
	{
		if (backup_created)
			(void)rename(bak_path, path);
		(void)remove(tmp_path);
		return STUDY_BACKEND_STATE_WRITE_FAILED;
	}

	return STUDY_BACKEND_STATE_OK;
}

bool study_backend_delete_state_tsv(const char *path)
{
	char artifact_path[STUDY_BACKEND_STATE_PATH_SIZE];
	bool ok = true;

	if (path == NULL || path[0] == '\0')
		return false;

	ok = study_backend_remove_if_present(path) && ok;
	if (
		study_backend_state_artifact_path(
			artifact_path,
			sizeof(artifact_path),
			path,
			".tmp"
		)
	)
	{
		ok = study_backend_remove_if_present(artifact_path) && ok;
	}
	else
	{
		ok = false;
	}
	if (
		study_backend_state_artifact_path(
			artifact_path,
			sizeof(artifact_path),
			path,
			".bak"
		)
	)
	{
		ok = study_backend_remove_if_present(artifact_path) && ok;
	}
	else
	{
		ok = false;
	}

	return ok;
}

static bool study_backend_has_active_card(const struct study_backend *backend)
{
	return study_backend_card_is_due(
		backend,
		backend != NULL ? backend->current_index : 0
	);
}

static void study_backend_mark_current_introduced(struct study_backend *backend)
{
	if (!study_backend_has_active_card(backend))
		return;
	if (backend->introduced[backend->current_index])
		return;

	backend->introduced[backend->current_index] = true;
	backend->introduced_count++;
	backend->introduced_today_count++;
}

static void study_backend_mark_current_completed_today(struct study_backend *backend)
{
	if (!study_backend_has_active_card(backend))
		return;
	if (backend->completed_today[backend->current_index])
		return;

	backend->completed_today[backend->current_index] = true;
	backend->completed_today_count++;
}

static void study_backend_unmark_completed_today(
	struct study_backend *backend,
	size_t card_index
)
{
	if (backend == NULL || card_index >= backend->card_count)
		return;
	if (!backend->completed_today[card_index])
		return;

	backend->completed_today[card_index] = false;
	if (backend->completed_today_count > 0)
		backend->completed_today_count--;
}

static bool study_backend_has_available_completed_today(
	const struct study_backend *backend
)
{
	if (backend == NULL)
		return false;

	for (size_t card_index = 0; card_index < backend->card_count; card_index++)
	{
		if (
			study_backend_card_is_available(backend, card_index) &&
			backend->completed_today[card_index]
		)
		{
			return true;
		}
	}

	return false;
}

void study_backend_build_view(
	const struct study_backend *backend,
	struct study_backend_view *view
)
{
	if (view == NULL)
		return;

	memset(view, 0, sizeof(*view));
	view->front_text = "";
	view->back_text = "";
	view->tags_text = "";
	view->primary_text = "No cards loaded.";
	study_backend_copy_status(
		view->status_text,
		sizeof(view->status_text),
		"No cards loaded"
	);
	if (backend == NULL)
		return;

	view->card_index = backend->current_index;
	view->card_count = backend->card_count;
	view->progress_day = backend->progress_day;
	view->reviewed_count = backend->reviewed_count;
	view->reviewed_today_count = backend->reviewed_today_count;
	view->introduced_count = backend->introduced_count;
	view->introduced_today_count = backend->introduced_today_count;
	view->again_count = backend->again_count;
	view->hard_count = backend->hard_count;
	view->good_count = backend->good_count;
	view->easy_count = backend->easy_count;
	view->suspended_count = backend->suspended_count;
	view->answer_visible = backend->answer_visible;
	view->undo_available = backend->undo_available;

	if (!study_backend_has_active_card(backend))
	{
		if (backend->card_count == 0)
		{
			study_backend_copy_status(
				view->status_text,
				sizeof(view->status_text),
				backend->status_text
			);
		}
		else if (backend->suspended_count > 0)
		{
			view->primary_text = "No active cards.";
			study_backend_copy_status(
				view->status_text,
				sizeof(view->status_text),
				backend->status_text
			);
		}
		else
		{
			view->primary_text = "Session complete.";
			study_backend_copy_status(
				view->status_text,
				sizeof(view->status_text),
				"Complete"
			);
		}
		return;
	}

	view->has_active_card = true;
	view->current_card_introduced =
		backend->introduced[backend->current_index];
	view->front_text = backend->cards[backend->current_index].front != NULL ?
		backend->cards[backend->current_index].front :
		"";
	view->back_text = backend->cards[backend->current_index].back != NULL ?
		backend->cards[backend->current_index].back :
		"";
	view->tags_text = backend->cards[backend->current_index].tags != NULL ?
		backend->cards[backend->current_index].tags :
		"";
	view->primary_text = backend->answer_visible ?
		view->back_text :
		view->front_text;
	study_backend_copy_status(
		view->status_text,
		sizeof(view->status_text),
		backend->status_text
	);
}

bool study_backend_show_answer(struct study_backend *backend)
{
	if (!study_backend_has_active_card(backend))
		return false;
	if (backend->answer_visible)
		return false;

	backend->answer_visible = true;
	study_backend_mark_current_introduced(backend);
	study_backend_copy_status(
		backend->status_text,
		sizeof(backend->status_text),
		"Answer"
	);
	return true;
}

static void study_backend_record_rating(
	struct study_backend *backend,
	enum study_backend_rating rating
)
{
	backend->reviewed_count++;
	backend->reviewed_today_count++;
	switch (rating)
	{
	case STUDY_BACKEND_RATING_AGAIN:
		backend->again_count++;
		break;
	case STUDY_BACKEND_RATING_HARD:
		backend->hard_count++;
		break;
	case STUDY_BACKEND_RATING_GOOD:
		backend->good_count++;
		break;
	case STUDY_BACKEND_RATING_EASY:
		backend->easy_count++;
		break;
	}
}

static void study_backend_revert_rating(
	struct study_backend *backend,
	enum study_backend_rating rating
)
{
	if (backend->reviewed_count > 0)
		backend->reviewed_count--;
	if (backend->reviewed_today_count > 0)
		backend->reviewed_today_count--;

	switch (rating)
	{
	case STUDY_BACKEND_RATING_AGAIN:
		if (backend->again_count > 0)
			backend->again_count--;
		break;
	case STUDY_BACKEND_RATING_HARD:
		if (backend->hard_count > 0)
			backend->hard_count--;
		break;
	case STUDY_BACKEND_RATING_GOOD:
		if (backend->good_count > 0)
			backend->good_count--;
		break;
	case STUDY_BACKEND_RATING_EASY:
		if (backend->easy_count > 0)
			backend->easy_count--;
		break;
	}
}

static unsigned int study_backend_clamp_interval_days(unsigned long interval)
{
	if (interval > STUDY_BACKEND_MAX_INTERVAL_DAYS)
		return STUDY_BACKEND_MAX_INTERVAL_DAYS;

	return (unsigned int)interval;
}

static unsigned int study_backend_next_interval_days(
	const struct study_backend *backend,
	size_t card_index,
	enum study_backend_rating rating
)
{
	unsigned int previous_interval =
		backend != NULL && card_index < backend->card_count ?
			backend->interval_days[card_index] :
			0;

	switch (rating)
	{
	case STUDY_BACKEND_RATING_AGAIN:
		return 0;
	case STUDY_BACKEND_RATING_HARD:
		return previous_interval > 0 ? previous_interval : 1;
	case STUDY_BACKEND_RATING_GOOD:
		return study_backend_clamp_interval_days(
			previous_interval > 0 ?
				(unsigned long)previous_interval * 2ul :
				2ul
		);
	case STUDY_BACKEND_RATING_EASY:
		return study_backend_clamp_interval_days(
			previous_interval > 0 ?
				(unsigned long)previous_interval * 3ul :
				4ul
		);
	}

	return 1;
}

static unsigned int study_backend_due_day_after_interval(
	unsigned int progress_day,
	unsigned int interval_days
)
{
	if (progress_day == 0)
		return 0;
	if ((unsigned long)progress_day + interval_days > UINT_MAX)
		return UINT_MAX;

	return progress_day + interval_days;
}

static void study_backend_apply_rating_schedule(
	struct study_backend *backend,
	size_t rated_index,
	enum study_backend_rating rating
)
{
	unsigned int interval_days;

	if (backend == NULL || rated_index >= backend->card_count)
		return;

	interval_days = study_backend_next_interval_days(
		backend,
		rated_index,
		rating
	);
	backend->interval_days[rated_index] = interval_days;
	backend->due_day[rated_index] = study_backend_due_day_after_interval(
		backend->progress_day,
		interval_days
	);
}

static void study_backend_snapshot_cooldowns_for_undo(
	struct study_backend *backend
)
{
	if (backend == NULL)
		return;

	memcpy(
		backend->undo_cooldown_remaining,
		backend->cooldown_remaining,
		sizeof(backend->cooldown_remaining)
	);
}

static void study_backend_restore_cooldowns_from_undo(
	struct study_backend *backend
)
{
	if (backend == NULL)
		return;

	memcpy(
		backend->cooldown_remaining,
		backend->undo_cooldown_remaining,
		sizeof(backend->cooldown_remaining)
	);
}

static void study_backend_advance_card_count_cooldowns(
	struct study_backend *backend,
	size_t rated_index
)
{
	if (
		backend == NULL ||
		backend->scheduler_policy !=
			STUDY_BACKEND_SCHEDULER_CARD_COUNT_COOLDOWN
	)
	{
		return;
	}

	for (size_t card_index = 0; card_index < backend->card_count; card_index++)
	{
		if (card_index == rated_index)
			continue;
		if (backend->cooldown_remaining[card_index] > 0)
			backend->cooldown_remaining[card_index]--;
	}
}

static void study_backend_apply_rating_cooldown(
	struct study_backend *backend,
	size_t rated_index,
	enum study_backend_rating rating
)
{
	if (backend == NULL || rated_index >= backend->card_count)
		return;

	if (
		backend->scheduler_policy !=
		STUDY_BACKEND_SCHEDULER_CARD_COUNT_COOLDOWN
	)
	{
		backend->cooldown_remaining[rated_index] = 0;
		return;
	}

	if (rating == STUDY_BACKEND_RATING_AGAIN)
	{
		backend->cooldown_remaining[rated_index] =
			backend->scheduler_cooldown_steps;
	}
	else
	{
		backend->cooldown_remaining[rated_index] = 0;
	}
}

bool study_backend_rate_current(
	struct study_backend *backend,
	enum study_backend_rating rating
)
{
	size_t rated_index;

	if (!study_backend_has_active_card(backend))
		return false;
	if (!backend->answer_visible)
		return false;

	rated_index = backend->current_index;
	backend->undo_available = true;
	backend->undo_card_index = rated_index;
	backend->undo_rating = rating;
	backend->undo_due_day = backend->due_day[rated_index];
	backend->undo_interval_days = backend->interval_days[rated_index];
	study_backend_snapshot_cooldowns_for_undo(backend);
	study_backend_record_rating(backend, rating);
	study_backend_advance_card_count_cooldowns(backend, rated_index);
	backend->answer_visible = false;
	if (rating != STUDY_BACKEND_RATING_AGAIN)
	{
		study_backend_mark_current_completed_today(backend);
		backend->current_index++;
	}
	study_backend_apply_rating_schedule(backend, rated_index, rating);
	study_backend_apply_rating_cooldown(backend, rated_index, rating);
	study_backend_reposition_to_due(backend);

	if (!study_backend_has_active_card(backend))
	{
		snprintf(
			backend->status_text,
			sizeof(backend->status_text),
			"Complete; reviewed %u",
			backend->reviewed_count
		);
		return true;
	}

	snprintf(
		backend->status_text,
		sizeof(backend->status_text),
		"%s; card %lu/%lu",
		study_backend_rating_name(rating),
		(unsigned long)(backend->current_index + 1),
		(unsigned long)backend->card_count
	);
	return true;
}

bool study_backend_suspend_current(struct study_backend *backend)
{
	size_t suspended_index;

	if (!study_backend_has_active_card(backend))
		return false;

	suspended_index = backend->current_index;
	backend->suspended[suspended_index] = true;
	backend->suspended_count++;
	backend->cooldown_remaining[suspended_index] = 0;
	backend->answer_visible = false;
	backend->undo_available = false;
	backend->current_index = suspended_index + 1;
	study_backend_reposition_to_due(backend);

	if (study_backend_has_active_card(backend))
	{
		snprintf(
			backend->status_text,
			sizeof(backend->status_text),
			"Suspended; card %lu/%lu",
			(unsigned long)(backend->current_index + 1),
			(unsigned long)backend->card_count
		);
	}
	else
	{
		study_backend_copy_status(
			backend->status_text,
			sizeof(backend->status_text),
			"Suspended; no active cards"
		);
	}

	return true;
}

unsigned int study_backend_restore_suspended(struct study_backend *backend)
{
	size_t first_restored_index = STUDY_BACKEND_MAX_CARDS;
	unsigned int restored_count = 0;

	if (backend == NULL)
		return 0;

	for (size_t card_index = 0; card_index < backend->card_count; card_index++)
	{
		if (!backend->suspended[card_index])
			continue;

		if (first_restored_index == STUDY_BACKEND_MAX_CARDS)
			first_restored_index = card_index;
		backend->suspended[card_index] = false;
		restored_count++;
	}

	if (restored_count == 0)
	{
		study_backend_copy_status(
			backend->status_text,
			sizeof(backend->status_text),
			"Nothing suspended"
		);
		return 0;
	}

	backend->suspended_count = 0;
	backend->answer_visible = false;
	backend->undo_available = false;
	if (first_restored_index < backend->current_index)
		backend->current_index = first_restored_index;
	study_backend_reposition_to_due(backend);

	if (study_backend_has_active_card(backend))
	{
		snprintf(
			backend->status_text,
			sizeof(backend->status_text),
			"Restored %u suspended; card %lu/%lu",
			restored_count,
			(unsigned long)(backend->current_index + 1),
			(unsigned long)backend->card_count
		);
	}
	else
	{
		snprintf(
			backend->status_text,
			sizeof(backend->status_text),
			"Restored %u suspended",
			restored_count
		);
	}

	return restored_count;
}

bool study_backend_reset_progress(struct study_backend *backend)
{
	if (backend == NULL)
		return false;

	backend->current_index = 0;
	backend->undo_card_index = 0;
	backend->progress_day = 0;
	backend->reviewed_count = 0;
	backend->reviewed_today_count = 0;
	backend->introduced_count = 0;
	backend->introduced_today_count = 0;
	backend->completed_today_count = 0;
	backend->again_count = 0;
	backend->hard_count = 0;
	backend->good_count = 0;
	backend->easy_count = 0;
	backend->suspended_count = 0;
	backend->undo_rating = STUDY_BACKEND_RATING_AGAIN;
	backend->undo_due_day = 0;
	backend->undo_interval_days = 0;
	memset(backend->introduced, 0, sizeof(backend->introduced));
	memset(backend->completed_today, 0, sizeof(backend->completed_today));
	memset(backend->suspended, 0, sizeof(backend->suspended));
	memset(backend->due_day, 0, sizeof(backend->due_day));
	memset(backend->interval_days, 0, sizeof(backend->interval_days));
	memset(backend->cooldown_remaining, 0, sizeof(backend->cooldown_remaining));
	memset(
		backend->undo_cooldown_remaining,
		0,
		sizeof(backend->undo_cooldown_remaining)
	);
	backend->answer_visible = false;
	backend->undo_available = false;
	study_backend_copy_status(
		backend->status_text,
		sizeof(backend->status_text),
		backend->card_count > 0 ? "Progress reset" : "No cards loaded"
	);
	return backend->card_count > 0;
}

bool study_backend_review_again(struct study_backend *backend)
{
	if (backend == NULL)
		return false;
	if (backend->card_count == 0)
	{
		study_backend_copy_status(
			backend->status_text,
			sizeof(backend->status_text),
			"No cards loaded"
		);
		return false;
	}
	if (!study_backend_has_available_completed_today(backend))
	{
		study_backend_copy_status(
			backend->status_text,
			sizeof(backend->status_text),
			backend->suspended_count > 0 ?
				"Nothing to review; restore suspended" :
				"Nothing to review"
		);
		return false;
	}

	for (size_t card_index = 0; card_index < backend->card_count; card_index++)
	{
		if (!backend->completed_today[card_index])
			continue;

		backend->due_day[card_index] = backend->progress_day;
	}
	backend->reviewed_today_count = 0;
	backend->completed_today_count = 0;
	memset(backend->completed_today, 0, sizeof(backend->completed_today));
	memset(backend->cooldown_remaining, 0, sizeof(backend->cooldown_remaining));
	memset(
		backend->undo_cooldown_remaining,
		0,
		sizeof(backend->undo_cooldown_remaining)
	);
	backend->current_index = 0;
	backend->answer_visible = false;
	backend->undo_available = false;
	backend->undo_card_index = 0;
	backend->undo_rating = STUDY_BACKEND_RATING_AGAIN;
	backend->undo_due_day = 0;
	backend->undo_interval_days = 0;
	study_backend_reposition_to_due(backend);
	if (!study_backend_has_active_card(backend))
	{
		study_backend_copy_status(
			backend->status_text,
			sizeof(backend->status_text),
			"Nothing to review"
		);
		return false;
	}

	snprintf(
		backend->status_text,
		sizeof(backend->status_text),
		"Review again; card %lu/%lu",
		(unsigned long)(backend->current_index + 1),
		(unsigned long)backend->card_count
	);
	return true;
}

bool study_backend_rollover_day(
	struct study_backend *backend,
	unsigned int current_day
)
{
	bool has_progress;

	if (backend == NULL)
		return false;
	if (backend->progress_day == current_day)
		return false;

	if (backend->progress_day == 0)
	{
		has_progress = (
			backend->reviewed_count > 0 ||
			backend->introduced_count > 0 ||
			backend->suspended_count > 0 ||
			backend->again_count > 0 ||
			backend->hard_count > 0 ||
			backend->good_count > 0 ||
			backend->easy_count > 0 ||
			backend->current_index > 0
		);
		backend->progress_day = current_day;
		return has_progress;
	}

	backend->progress_day = current_day;
	backend->reviewed_today_count = 0;
	backend->introduced_today_count = 0;
	backend->completed_today_count = 0;
	memset(backend->completed_today, 0, sizeof(backend->completed_today));
	memset(backend->cooldown_remaining, 0, sizeof(backend->cooldown_remaining));
	memset(
		backend->undo_cooldown_remaining,
		0,
		sizeof(backend->undo_cooldown_remaining)
	);
	backend->current_index = 0;
	backend->answer_visible = false;
	backend->undo_available = false;
	backend->undo_card_index = 0;
	backend->undo_rating = STUDY_BACKEND_RATING_AGAIN;
	backend->undo_due_day = 0;
	backend->undo_interval_days = 0;
	study_backend_reposition_to_due(backend);
	study_backend_copy_status(
		backend->status_text,
		sizeof(backend->status_text),
		"New day; limits reset"
	);
	return true;
}

bool study_backend_undo_last_rating(struct study_backend *backend)
{
	if (backend == NULL || !backend->undo_available)
		return false;
	if (backend->cards == NULL || backend->undo_card_index >= backend->card_count)
		return false;

	study_backend_revert_rating(backend, backend->undo_rating);
	study_backend_restore_cooldowns_from_undo(backend);
	backend->due_day[backend->undo_card_index] = backend->undo_due_day;
	backend->interval_days[backend->undo_card_index] =
		backend->undo_interval_days;
	if (backend->undo_rating != STUDY_BACKEND_RATING_AGAIN)
		study_backend_unmark_completed_today(backend, backend->undo_card_index);
	backend->current_index = backend->undo_card_index;
	backend->answer_visible = false;
	backend->undo_available = false;
	snprintf(
		backend->status_text,
		sizeof(backend->status_text),
		"Undo; card %lu/%lu",
		(unsigned long)(backend->current_index + 1),
		(unsigned long)backend->card_count
	);
	return true;
}
