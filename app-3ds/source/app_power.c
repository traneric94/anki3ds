#include "app_power.h"

void app_power_schedule_next_battery_poll(time_t *next_poll_time, time_t now)
{
	if (next_poll_time == NULL || now == (time_t)-1)
		return;

	*next_poll_time = now + APP_POWER_BATTERY_POLL_INTERVAL_SECONDS;
}

bool app_power_battery_poll_is_due(time_t *next_poll_time, time_t now)
{
	if (next_poll_time == NULL || now == (time_t)-1)
		return false;
	if (*next_poll_time == 0)
	{
		app_power_schedule_next_battery_poll(next_poll_time, now);
		return false;
	}

	return now >= *next_poll_time;
}
