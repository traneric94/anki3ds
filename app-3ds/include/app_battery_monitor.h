#ifndef ANKI3DS_APP_BATTERY_MONITOR_H
#define ANKI3DS_APP_BATTERY_MONITOR_H

#include <stdbool.h>

#include "app_power.h"

struct study_backend;

struct app_battery_monitor
{
	bool service_available;
	bool status_available;
	bool charging;
	bool low_warning_announced;
	unsigned int level;
};

void app_battery_monitor_init(struct app_battery_monitor *monitor);
enum app_power_battery_sample_result app_battery_monitor_sample(
	struct app_battery_monitor *monitor
);
bool app_battery_monitor_apply_warning(
	struct app_battery_monitor *monitor,
	struct study_backend *backend,
	enum app_power_battery_sample_result sample_result
);
bool app_battery_monitor_save_warning_needed(
	const struct app_battery_monitor *monitor
);
void app_battery_monitor_shutdown(struct app_battery_monitor *monitor);

#endif
