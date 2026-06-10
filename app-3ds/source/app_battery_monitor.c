#include "app_battery_monitor.h"

#include <3ds.h>

#include <stdio.h>
#include <string.h>

#include "app_battery_status.h"
#include "study_backend.h"

static void app_battery_monitor_copy_string(
	char *destination,
	size_t destination_size,
	const char *source
)
{
	if (destination == NULL || destination_size == 0)
		return;

	snprintf(destination, destination_size, "%s", source != NULL ? source : "");
}

void app_battery_monitor_init(struct app_battery_monitor *monitor)
{
	if (monitor == NULL)
		return;

	memset(monitor, 0, sizeof(*monitor));
}

enum app_power_battery_sample_result app_battery_monitor_sample(
	struct app_battery_monitor *monitor
)
{
	u8 shell_state;
	u8 level;
	u8 charge_state;
	bool changed;
	bool old_status_available;
	bool old_charging;
	unsigned int old_level;

	if (monitor == NULL)
		return APP_POWER_BATTERY_SAMPLE_UNAVAILABLE;

	if (!monitor->service_available)
	{
		if (R_FAILED(ptmuInit()))
			return APP_POWER_BATTERY_SAMPLE_UNAVAILABLE;

		monitor->service_available = true;
	}

	if (R_FAILED(PTMU_GetShellState(&shell_state)))
		return APP_POWER_BATTERY_SAMPLE_READ_FAILED;
	if (shell_state == 0)
		return APP_POWER_BATTERY_SAMPLE_SKIPPED_CLOSED;

	old_status_available = monitor->status_available;
	old_charging = monitor->charging;
	old_level = monitor->level;

	if (
		R_FAILED(PTMU_GetBatteryLevel(&level)) ||
		R_FAILED(PTMU_GetBatteryChargeState(&charge_state))
	)
	{
		return APP_POWER_BATTERY_SAMPLE_READ_FAILED;
	}

	monitor->status_available = true;
	monitor->charging = charge_state != 0;
	monitor->level = level;
	changed =
		!old_status_available ||
		old_charging != monitor->charging ||
		old_level != monitor->level;

	return changed ?
		APP_POWER_BATTERY_SAMPLE_CHANGED :
		APP_POWER_BATTERY_SAMPLE_UNCHANGED;
}

bool app_battery_monitor_apply_warning(
	struct app_battery_monitor *monitor,
	struct study_backend *backend,
	enum app_power_battery_sample_result sample_result
)
{
	char status_text[STUDY_BACKEND_STATUS_SIZE];

	if (monitor == NULL || backend == NULL)
		return false;

	app_battery_monitor_copy_string(
		status_text,
		sizeof(status_text),
		backend->status_text
	);
	if (
		!app_battery_status_apply_sample(
			status_text,
			sizeof(status_text),
			&monitor->low_warning_announced,
			sample_result,
			monitor->status_available,
			monitor->charging,
			monitor->level
		)
	)
	{
		return false;
	}

	study_backend_set_status(backend, status_text);
	return true;
}

bool app_battery_monitor_save_warning_needed(
	const struct app_battery_monitor *monitor
)
{
	if (monitor == NULL)
		return false;

	return app_power_battery_save_warning_needed(
		monitor->status_available,
		monitor->charging,
		monitor->level
	);
}

void app_battery_monitor_shutdown(struct app_battery_monitor *monitor)
{
	if (monitor == NULL || !monitor->service_available)
		return;

	ptmuExit();
	monitor->service_available = false;
}
