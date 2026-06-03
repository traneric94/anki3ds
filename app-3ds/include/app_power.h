#ifndef ANKI3DS_APP_POWER_H
#define ANKI3DS_APP_POWER_H

#include <stdbool.h>
#include <time.h>

#define APP_POWER_BATTERY_LOW_LEVEL 1
#define APP_POWER_BATTERY_POLL_INTERVAL_SECONDS 600

void app_power_schedule_next_battery_poll(time_t *next_poll_time, time_t now);
bool app_power_battery_poll_is_due(time_t *next_poll_time, time_t now);

#endif
