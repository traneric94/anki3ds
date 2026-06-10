#ifndef ANKI3DS_APP_POWER_H
#define ANKI3DS_APP_POWER_H

#include <stdbool.h>
#include <time.h>

#define APP_POWER_BATTERY_LOW_LEVEL 1
#define APP_POWER_BATTERY_POLL_INTERVAL_SECONDS 600
#define APP_POWER_BATTERY_RETRY_INTERVAL_SECONDS 60
#define APP_POWER_IDLE_INPUT_WAIT_INITIAL_NS 100000000LL
#define APP_POWER_IDLE_INPUT_WAIT_MID_NS 250000000LL
#define APP_POWER_IDLE_INPUT_WAIT_MAX_NS 2000000000LL
#define APP_POWER_IDLE_INPUT_FAST_WAIT_COUNT 10
#define APP_POWER_IDLE_INPUT_MID_WAIT_COUNT 30
#define APP_POWER_IDLE_INPUT_MAX_WAIT_COUNT 60

enum app_power_battery_sample_result
{
	APP_POWER_BATTERY_SAMPLE_SKIPPED_CLOSED,
	APP_POWER_BATTERY_SAMPLE_UNAVAILABLE,
	APP_POWER_BATTERY_SAMPLE_READ_FAILED,
	APP_POWER_BATTERY_SAMPLE_UNCHANGED,
	APP_POWER_BATTERY_SAMPLE_CHANGED,
};

enum app_power_battery_display_state
{
	APP_POWER_BATTERY_DISPLAY_UNAVAILABLE,
	APP_POWER_BATTERY_DISPLAY_NORMAL,
	APP_POWER_BATTERY_DISPLAY_LOW,
	APP_POWER_BATTERY_DISPLAY_CHARGING,
};

void app_power_schedule_next_battery_poll(time_t *next_poll_time, time_t now);
void app_power_schedule_next_battery_poll_after_sample(
	time_t *next_poll_time,
	time_t now,
	enum app_power_battery_sample_result result
);
bool app_power_battery_poll_is_due(time_t *next_poll_time, time_t now);
bool app_power_battery_sample_changes_display(
	enum app_power_battery_sample_result result
);
enum app_power_battery_display_state app_power_battery_display_state(
	bool status_available,
	bool charging,
	unsigned int level
);
bool app_power_battery_save_warning_needed(
	bool status_available,
	bool charging,
	unsigned int level
);
bool app_power_battery_low_warning_due(
	bool *announced,
	bool status_available,
	bool charging,
	unsigned int level
);
long long app_power_idle_input_wait_ns(unsigned int idle_wait_count);
unsigned int app_power_next_idle_input_wait_count(unsigned int idle_wait_count);

#endif
