#ifndef ANKI3DS_APP_BATTERY_STATUS_H
#define ANKI3DS_APP_BATTERY_STATUS_H

#include <stdbool.h>
#include <stddef.h>

#include "app_power.h"

#define APP_BATTERY_STATUS_LOW "Battery low; charge soon"
#define APP_BATTERY_STATUS_OK "Battery ok"
#define APP_BATTERY_STATUS_UNAVAILABLE "Battery status unavailable"

bool app_battery_status_apply_sample(
	char *status_text,
	size_t status_text_size,
	bool *low_warning_announced,
	enum app_power_battery_sample_result sample_result,
	bool monitor_status_available,
	bool monitor_charging,
	unsigned int monitor_level
);

#endif
