#include "app_status.h"

#include <string.h>

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
