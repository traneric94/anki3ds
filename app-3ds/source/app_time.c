#include "app_time.h"

#include <stdbool.h>

#include "scheduler.h"

static bool is_leap_year(int year)
{
	if (year % 4 != 0)
		return false;
	if (year % 100 != 0)
		return true;

	return year % 400 == 0;
}

static unsigned int add_days_capped(unsigned int days, unsigned int addend)
{
	if (days > SCHEDULER_MAX_DAY - addend)
		return SCHEDULER_MAX_DAY;

	return days + addend;
}

static unsigned int days_before_year(int year)
{
	unsigned int days = 0;

	for (int current_year = 1970; current_year < year; current_year++)
	{
		unsigned int year_days = is_leap_year(current_year) ? 366u : 365u;

		days = add_days_capped(days, year_days);
		if (days == SCHEDULER_MAX_DAY)
			return days;
	}

	return days;
}

static unsigned int days_in_month(int year, int month)
{
	static const unsigned int month_days[] = {
		31,
		28,
		31,
		30,
		31,
		30,
		31,
		31,
		30,
		31,
		30,
		31,
	};

	if (month < 1 || month > 12)
		return 0;
	if (month == 2 && is_leap_year(year))
		return 29;

	return month_days[month - 1];
}

static unsigned int days_before_month(int year, int month)
{
	unsigned int days = 0;

	for (int current_month = 1; current_month < month; current_month++)
		days += days_in_month(year, current_month);

	return days;
}

unsigned int app_time_day_from_local_date(int year, int month, int month_day)
{
	unsigned int days;

	if (year < 1970)
		return 0;
	if (month < 1 || month > 12)
		return 0;
	if (month_day < 1 || month_day > (int)days_in_month(year, month))
		return 0;

	days = days_before_year(year);
	days = add_days_capped(days, days_before_month(year, month));
	days = add_days_capped(days, (unsigned int)(month_day - 1));

	return days;
}

unsigned int app_time_local_day_from_time(time_t timestamp)
{
	struct tm *local_time;

	if (timestamp == (time_t)-1 || timestamp < 0)
		return 0;

	local_time = localtime(&timestamp);
	if (local_time == NULL)
		return 0;

	return app_time_day_from_local_date(
		local_time->tm_year + 1900,
		local_time->tm_mon + 1,
		local_time->tm_mday
	);
}

unsigned int app_time_current_day(void)
{
	return app_time_local_day_from_time(time(NULL));
}
