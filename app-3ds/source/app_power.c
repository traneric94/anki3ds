#include "app_power.h"

#define APP_POWER_BATTERY_POLL_WHEN_CLOCK_AVAILABLE ((time_t)-2)

void app_power_schedule_next_battery_poll(time_t *next_poll_time, time_t now)
{
	if (next_poll_time == NULL || now == (time_t)-1)
		return;

	*next_poll_time = now + APP_POWER_BATTERY_POLL_INTERVAL_SECONDS;
}

void app_power_schedule_next_battery_poll_after_sample(
	time_t *next_poll_time,
	time_t now,
	enum app_power_battery_sample_result result
)
{
	if (next_poll_time == NULL)
		return;

	if (result == APP_POWER_BATTERY_SAMPLE_READ_FAILED)
	{
		if (now == (time_t)-1)
		{
			/* Retry as soon as a real clock reading appears. */
			*next_poll_time = APP_POWER_BATTERY_POLL_WHEN_CLOCK_AVAILABLE;
			return;
		}

		*next_poll_time = now + APP_POWER_BATTERY_RETRY_INTERVAL_SECONDS;
		return;
	}

	if (now == (time_t)-1)
		return;

	app_power_schedule_next_battery_poll(next_poll_time, now);
}

bool app_power_battery_poll_is_due(time_t *next_poll_time, time_t now)
{
	if (next_poll_time == NULL || now == (time_t)-1)
		return false;
	if (*next_poll_time == APP_POWER_BATTERY_POLL_WHEN_CLOCK_AVAILABLE)
		return true;
	if (*next_poll_time == 0)
	{
		app_power_schedule_next_battery_poll(next_poll_time, now);
		return false;
	}

	return now >= *next_poll_time;
}

bool app_power_battery_sample_changes_display(enum app_power_battery_sample_result result)
{
	return result == APP_POWER_BATTERY_SAMPLE_CHANGED;
}

enum app_power_battery_display_state app_power_battery_display_state(
	bool status_available,
	bool charging,
	unsigned int level
)
{
	if (!status_available)
		return APP_POWER_BATTERY_DISPLAY_UNAVAILABLE;
	if (charging)
		return APP_POWER_BATTERY_DISPLAY_CHARGING;
	if (level <= APP_POWER_BATTERY_LOW_LEVEL)
		return APP_POWER_BATTERY_DISPLAY_LOW;

	return APP_POWER_BATTERY_DISPLAY_NORMAL;
}

bool app_power_battery_save_warning_needed(
	bool status_available,
	bool charging,
	unsigned int level
)
{
	return app_power_battery_display_state(status_available, charging, level) ==
		APP_POWER_BATTERY_DISPLAY_LOW;
}

long long app_power_idle_input_wait_ns(unsigned int idle_wait_count)
{
	if (idle_wait_count < APP_POWER_IDLE_INPUT_FAST_WAIT_COUNT)
		return APP_POWER_IDLE_INPUT_WAIT_INITIAL_NS;
	if (idle_wait_count < APP_POWER_IDLE_INPUT_MID_WAIT_COUNT)
		return APP_POWER_IDLE_INPUT_WAIT_MID_NS;

	return APP_POWER_IDLE_INPUT_WAIT_MAX_NS;
}

unsigned int app_power_next_idle_input_wait_count(unsigned int idle_wait_count)
{
	if (idle_wait_count >= APP_POWER_IDLE_INPUT_MAX_WAIT_COUNT)
		return APP_POWER_IDLE_INPUT_MAX_WAIT_COUNT;

	return idle_wait_count + 1;
}
