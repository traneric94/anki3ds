#include "app_battery_status.h"

#include <stdio.h>
#include <string.h>

static bool app_battery_status_copy(
	char *destination,
	size_t destination_size,
	const char *source
)
{
	int written;

	if (destination == NULL || destination_size == 0 || source == NULL)
		return false;

	written = snprintf(destination, destination_size, "%s", source);
	return written >= 0 && (size_t)written < destination_size;
}

bool app_battery_status_apply_sample(
	char *status_text,
	size_t status_text_size,
	bool *low_warning_announced,
	enum app_power_battery_sample_result sample_result,
	bool monitor_status_available,
	bool monitor_charging,
	unsigned int monitor_level
)
{
	bool status_available;
	bool charging;
	unsigned int level;
	enum app_power_battery_display_state display_state;

	if (
		status_text == NULL ||
		status_text_size == 0 ||
		low_warning_announced == NULL
	)
	{
		return false;
	}
	if (sample_result == APP_POWER_BATTERY_SAMPLE_SKIPPED_CLOSED)
		return false;

	status_available = (
		sample_result == APP_POWER_BATTERY_SAMPLE_CHANGED ||
		sample_result == APP_POWER_BATTERY_SAMPLE_UNCHANGED
	) && monitor_status_available;
	charging = status_available && monitor_charging;
	level = status_available ? monitor_level : 0;

	display_state = app_power_battery_display_state(
		status_available,
		charging,
		level
	);
	if (
		app_power_battery_low_warning_due(
			low_warning_announced,
			status_available,
			charging,
			level
		)
	)
	{
		return app_battery_status_copy(
			status_text,
			status_text_size,
			APP_BATTERY_STATUS_LOW
		);
	}

	if (strcmp(status_text, APP_BATTERY_STATUS_LOW) != 0)
		return false;
	if (display_state == APP_POWER_BATTERY_DISPLAY_LOW)
		return false;

	return app_battery_status_copy(
		status_text,
		status_text_size,
		display_state == APP_POWER_BATTERY_DISPLAY_UNAVAILABLE ?
			APP_BATTERY_STATUS_UNAVAILABLE :
			APP_BATTERY_STATUS_OK
	);
}
