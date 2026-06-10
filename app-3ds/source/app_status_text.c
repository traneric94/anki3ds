#include "app_status_text.h"

#include <stdio.h>
#include <string.h>

#define APP_STATUS_TEXT_CONTEXT_SEGMENT_SIZE 80

bool app_status_text_has_low_battery_suffix(const char *status)
{
	return status != NULL &&
		strstr(status, APP_STATUS_TEXT_LOW_BATTERY_SUFFIX) != NULL;
}

static bool app_status_text_append_low_battery_suffix(
	char *destination,
	size_t destination_size
)
{
	size_t status_length;
	size_t suffix_length;
	size_t prefix_length;

	if (destination == NULL || destination_size == 0)
		return false;

	if (app_status_text_has_low_battery_suffix(destination))
		return false;

	suffix_length = strlen(APP_STATUS_TEXT_LOW_BATTERY_SUFFIX);
	if (destination_size <= suffix_length + 1)
	{
		snprintf(
			destination,
			destination_size,
			"%s",
			APP_STATUS_TEXT_LOW_BATTERY_SUFFIX
		);
		return true;
	}

	status_length = strlen(destination);
	prefix_length = destination_size - suffix_length - 1;
	if (prefix_length > status_length)
		prefix_length = status_length;

	memcpy(
		destination + prefix_length,
		APP_STATUS_TEXT_LOW_BATTERY_SUFFIX,
		suffix_length + 1
	);
	return true;
}

bool app_status_text_copy_with_low_battery_suffix(
	char *destination,
	size_t destination_size,
	const char *status,
	bool append_suffix
)
{
	const char *safe_status = status != NULL ? status : "";

	if (destination == NULL || destination_size == 0)
		return false;

	if (destination != safe_status)
		snprintf(destination, destination_size, "%s", safe_status);
	else
		destination[destination_size - 1] = '\0';

	if (!append_suffix)
		return false;

	return app_status_text_append_low_battery_suffix(
		destination,
		destination_size
	);
}

static bool app_status_text_segment_contains(
	const char *segment,
	size_t segment_length,
	const char *needle
)
{
	size_t needle_length;

	if (segment == NULL || needle == NULL)
		return false;

	needle_length = strlen(needle);
	if (needle_length == 0 || needle_length > segment_length)
		return false;

	for (size_t index = 0; index + needle_length <= segment_length; index++)
	{
		if (memcmp(segment + index, needle, needle_length) == 0)
			return true;
	}
	return false;
}

static bool app_status_text_segment_is_warning(
	const char *segment,
	size_t segment_length
)
{
	static const char *warning_terms[] = {
		"batt low",
		"Battery low",
		"Settings ignored",
		"limit reached",
		"Limit reached",
		"Bad saved state",
		"Bad format",
		"No deck",
		"Deck load failed",
		"Save failed",
		"Session save failed",
		"State error",
		"log skipped",
		"unavailable",
		"Exit loses unsaved settings",
		"ignored",
		"unmatched",
	};

	for (size_t index = 0; index < sizeof(warning_terms) / sizeof(warning_terms[0]); index++)
	{
		if (
			app_status_text_segment_contains(
				segment,
				segment_length,
				warning_terms[index]
			)
		)
		{
			return true;
		}
	}

	return false;
}

static bool app_status_text_copy_segment(
	char *destination,
	size_t destination_size,
	const char *segment,
	size_t segment_length
)
{
	size_t copy_length;

	if (destination == NULL || destination_size == 0)
		return false;

	copy_length = segment_length;
	if (copy_length >= destination_size)
		copy_length = destination_size - 1;
	memcpy(destination, segment, copy_length);
	destination[copy_length] = '\0';
	return copy_length == segment_length;
}

static bool app_status_text_contains_segment(
	const char *status,
	const char *segment,
	size_t segment_length
)
{
	char segment_text[APP_STATUS_TEXT_CONTEXT_SEGMENT_SIZE];

	if (status == NULL || segment == NULL || segment_length == 0)
		return false;

	(void)app_status_text_copy_segment(
		segment_text,
		sizeof(segment_text),
		segment,
		segment_length
	);
	return strstr(status, segment_text) != NULL;
}

static bool app_status_text_append_context_segment(
	char *destination,
	size_t destination_size,
	const char *segment,
	size_t segment_length
)
{
	size_t used_length;
	size_t separator_length;
	size_t available;

	if (destination == NULL || destination_size == 0 || segment == NULL)
		return false;

	used_length = strlen(destination);
	if (used_length >= destination_size - 1)
		return false;

	separator_length = used_length > 0 ? 2 : 0;
	if (used_length + separator_length >= destination_size - 1)
		return false;

	if (separator_length > 0)
	{
		destination[used_length++] = ';';
		destination[used_length++] = ' ';
		destination[used_length] = '\0';
	}

	available = destination_size - used_length;
	(void)app_status_text_copy_segment(
		destination + used_length,
		available,
		segment,
		segment_length
	);
	return true;
}

bool app_status_text_copy_with_warning_context(
	char *destination,
	size_t destination_size,
	const char *status,
	const char *context
)
{
	const char *safe_status = status != NULL ? status : "";
	const char *cursor;
	bool appended = false;

	if (destination == NULL || destination_size == 0)
		return false;

	snprintf(destination, destination_size, "%s", safe_status);
	if (context == NULL || context[0] == '\0')
		return false;

	cursor = context;
	while (*cursor != '\0')
	{
		const char *segment_start;
		const char *segment_end;
		size_t segment_length;

		while (*cursor == ';' || *cursor == ' ')
			cursor++;
		segment_start = cursor;
		while (*cursor != '\0' && *cursor != ';')
			cursor++;
		segment_end = cursor;
		while (segment_end > segment_start && segment_end[-1] == ' ')
			segment_end--;
		segment_length = (size_t)(segment_end - segment_start);

		if (
			segment_length > 0 &&
			app_status_text_segment_is_warning(
				segment_start,
				segment_length
			) &&
			!app_status_text_contains_segment(
				destination,
				segment_start,
				segment_length
			)
		)
		{
			appended =
				app_status_text_append_context_segment(
					destination,
					destination_size,
					segment_start,
					segment_length
				) ||
				appended;
		}
	}

	return appended;
}

bool app_status_text_copy_with_warning_context_and_low_battery_suffix(
	char *destination,
	size_t destination_size,
	const char *status,
	const char *context,
	bool append_suffix
)
{
	bool context_appended;
	bool suffix_appended = false;

	if (destination == NULL || destination_size == 0)
		return false;

	context_appended = app_status_text_copy_with_warning_context(
		destination,
		destination_size,
		status,
		context
	);
	if (append_suffix)
	{
		suffix_appended = app_status_text_append_low_battery_suffix(
			destination,
			destination_size
		);
	}

	return context_appended || suffix_appended;
}
