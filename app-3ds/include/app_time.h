#ifndef ANKI3DS_APP_TIME_H
#define ANKI3DS_APP_TIME_H

#include <time.h>

#define APP_TIME_MAX_DAY 1000000u

unsigned int app_time_day_from_local_date(int year, int month, int month_day);
unsigned int app_time_local_day_from_time(time_t timestamp);
unsigned int app_time_current_day(void);

#endif
