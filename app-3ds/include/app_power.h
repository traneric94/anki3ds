#ifndef ANKI3DS_APP_POWER_H
#define ANKI3DS_APP_POWER_H

#include <stdbool.h>
#include <time.h>

#define APP_POWER_BATTERY_LOW_LEVEL 1
#define APP_POWER_BATTERY_POLL_INTERVAL_SECONDS 600
#define APP_POWER_BATTERY_RETRY_INTERVAL_SECONDS 60

enum app_power_battery_sample_result
{
	APP_POWER_BATTERY_SAMPLE_SKIPPED_CLOSED,
	APP_POWER_BATTERY_SAMPLE_UNAVAILABLE,
	APP_POWER_BATTERY_SAMPLE_READ_FAILED,
	APP_POWER_BATTERY_SAMPLE_UNCHANGED,
	APP_POWER_BATTERY_SAMPLE_CHANGED,
};

void app_power_schedule_next_battery_poll(time_t *next_poll_time, time_t now);
void app_power_schedule_next_battery_poll_after_sample(
	time_t *next_poll_time,
	time_t now,
	enum app_power_battery_sample_result result
);
bool app_power_battery_poll_is_due(time_t *next_poll_time, time_t now);
bool app_power_battery_sample_changes_display(enum app_power_battery_sample_result result);

#endif
