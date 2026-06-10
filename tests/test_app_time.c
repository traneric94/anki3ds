#include "app_time.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TEST_SECONDS_PER_DAY 86400

static void set_test_timezone(const char *timezone)
{
	assert(setenv("TZ", timezone, 1) == 0);
	tzset();
}

static time_t make_utc_timestamp(
	int year,
	int month,
	int month_day,
	int hour,
	int minute,
	int second
)
{
	struct tm timestamp;

	set_test_timezone("UTC0");
	memset(&timestamp, 0, sizeof(timestamp));
	timestamp.tm_year = year - 1900;
	timestamp.tm_mon = month - 1;
	timestamp.tm_mday = month_day;
	timestamp.tm_hour = hour;
	timestamp.tm_min = minute;
	timestamp.tm_sec = second;
	timestamp.tm_isdst = -1;

	return mktime(&timestamp);
}

static void test_day_from_local_date(void)
{
	assert(app_time_day_from_local_date(1970, 1, 1) == 0);
	assert(app_time_day_from_local_date(1970, 3, 1) == 59);
	assert(app_time_day_from_local_date(1972, 3, 1) == 790);
	assert(app_time_day_from_local_date(1969, 12, 31) == 0);
	assert(app_time_day_from_local_date(1970, 2, 30) == 0);
	assert(app_time_day_from_local_date(5000, 1, 1) == APP_TIME_MAX_DAY);
}

static void test_local_day_differs_from_utc_rollover(void)
{
	time_t timestamp = make_utc_timestamp(2026, 6, 3, 0, 30, 0);
	unsigned int local_day;
	unsigned int utc_day;

	assert(timestamp != (time_t)-1);
	set_test_timezone("EST5EDT,M3.2.0,M11.1.0");
	local_day = app_time_local_day_from_time(timestamp);
	utc_day = (unsigned int)(timestamp / TEST_SECONDS_PER_DAY);

	assert(local_day == app_time_day_from_local_date(2026, 6, 2));
	assert(local_day != utc_day);

	set_test_timezone("UTC0");
}

static void test_invalid_timestamp_returns_epoch_day(void)
{
	assert(app_time_local_day_from_time((time_t)-1) == 0);
}

int main(void)
{
	test_day_from_local_date();
	test_local_day_differs_from_utc_rollover();
	test_invalid_timestamp_returns_epoch_day();
	return 0;
}
