#include "app_status.h"

#include "app_text.h"

#include <stdio.h>
#include <string.h>

static const char *last_status_clause(const char *message)
{
	const char *clause = NULL;
	const char *cursor = message;

	while (cursor != NULL)
	{
		cursor = strstr(cursor, "; ");
		if (cursor != NULL)
		{
			clause = cursor;
			cursor += 2;
		}
	}

	return clause;
}

static void copy_status_columns(
	char *destination,
	size_t destination_size,
	const char *message,
	size_t columns
)
{
	size_t bytes = app_text_byte_count_for_columns(message, columns);

	snprintf(destination, destination_size, "%.*s", (int)bytes, message);
}

static void copy_status_prefix_ellipsis(
	char *destination,
	size_t destination_size,
	const char *message,
	size_t max_columns
)
{
	size_t bytes;

	if (max_columns <= 3)
	{
		copy_status_columns(destination, destination_size, message, max_columns);
		return;
	}

	bytes = app_text_byte_count_for_columns(message, max_columns - 3);
	snprintf(destination, destination_size, "%.*s...", (int)bytes, message);
}

enum app_status_color app_status_message_color(const char *message)
{
	if (message == NULL)
		return APP_STATUS_COLOR_NEUTRAL;

	if (
		strstr(message, "failed") != NULL ||
		strstr(message, "Failed") != NULL ||
		strstr(message, "Missing") != NULL ||
		strstr(message, "error") != NULL ||
		strstr(message, "bad") != NULL
	)
	{
		return APP_STATUS_COLOR_DANGER;
	}

	if (
		strstr(message, "requires") != NULL ||
		strstr(message, "Nothing") != NULL ||
		strstr(message, "No deck") != NULL ||
		strstr(message, "skipped") != NULL ||
		strstr(message, "same card due") != NULL ||
		strstr(message, "same due") != NULL ||
		strstr(message, "Action: Reset") != NULL ||
		strstr(message, "canceled") != NULL ||
		strstr(message, "kept") != NULL ||
		strstr(message, "reset state") != NULL ||
		strstr(message, "limit reached") != NULL ||
		strstr(message, "settings/state") != NULL ||
		strstr(message, "unmatched") != NULL ||
		strstr(message, "ignored") != NULL ||
		strstr(message, "unsaved") != NULL ||
		strstr(message, "Unsaved") != NULL
	)
	{
		return APP_STATUS_COLOR_WARNING;
	}

	if (
		strstr(message, "saved") != NULL ||
		strstr(message, "Loaded") != NULL ||
		strstr(message, "Restored") != NULL ||
		strstr(message, "cards due") != NULL ||
		strstr(message, "no cards due") != NULL ||
		strstr(message, "reset") != NULL ||
		strstr(message, "Reset") != NULL
	)
	{
		return APP_STATUS_COLOR_SUCCESS;
	}

	return APP_STATUS_COLOR_NEUTRAL;
}

void app_status_format_for_width(
	char *destination,
	size_t destination_size,
	const char *message,
	size_t max_columns
)
{
	const char *suffix;
	size_t columns;
	size_t suffix_columns;
	size_t prefix_columns;
	size_t prefix_bytes;

	if (destination_size == 0)
		return;

	destination[0] = '\0';
	if (message == NULL || max_columns == 0)
		return;

	columns = app_text_column_count(message);
	if (columns <= max_columns)
	{
		snprintf(destination, destination_size, "%s", message);
		return;
	}

	suffix = last_status_clause(message);
	if (suffix == NULL || suffix == message)
	{
		copy_status_prefix_ellipsis(
			destination,
			destination_size,
			message,
			max_columns
		);
		return;
	}

	suffix_columns = app_text_column_count(suffix);
	if (suffix_columns + 3 >= max_columns)
	{
		copy_status_prefix_ellipsis(
			destination,
			destination_size,
			message,
			max_columns
		);
		return;
	}

	prefix_columns = max_columns - suffix_columns - 3;
	prefix_bytes = app_text_byte_count_for_columns(message, prefix_columns);
	snprintf(
		destination,
		destination_size,
		"%.*s...%s",
		(int)prefix_bytes,
		message,
		suffix
	);
}
