#ifndef ANKI3DS_APP_STATUS_TEXT_H
#define ANKI3DS_APP_STATUS_TEXT_H

#include <stdbool.h>
#include <stddef.h>

#define APP_STATUS_TEXT_LOW_BATTERY_SUFFIX "; batt low"

bool app_status_text_has_low_battery_suffix(const char *status);
bool app_status_text_copy_with_low_battery_suffix(
	char *destination,
	size_t destination_size,
	const char *status,
	bool append_suffix
);
bool app_status_text_copy_with_warning_context(
	char *destination,
	size_t destination_size,
	const char *status,
	const char *context
);
bool app_status_text_copy_with_warning_context_and_low_battery_suffix(
	char *destination,
	size_t destination_size,
	const char *status,
	const char *context,
	bool append_suffix
);

#endif
