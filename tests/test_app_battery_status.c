#include "app_battery_status.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void test_low_sample_announces_once(void)
{
	char status[64] = "Reviewing";
	bool announced = false;

	assert(
		app_battery_status_apply_sample(
			status,
			sizeof(status),
			&announced,
			APP_POWER_BATTERY_SAMPLE_CHANGED,
			true,
			false,
			APP_POWER_BATTERY_LOW_LEVEL
		)
	);
	assert(strcmp(status, APP_BATTERY_STATUS_LOW) == 0);
	assert(announced);

	assert(
		!app_battery_status_apply_sample(
			status,
			sizeof(status),
			&announced,
			APP_POWER_BATTERY_SAMPLE_UNCHANGED,
			true,
			false,
			APP_POWER_BATTERY_LOW_LEVEL
		)
	);
	assert(strcmp(status, APP_BATTERY_STATUS_LOW) == 0);
	assert(announced);
}

static void test_low_status_clears_after_charging_or_recovery(void)
{
	char status[64] = APP_BATTERY_STATUS_LOW;
	bool announced = true;

	assert(
		app_battery_status_apply_sample(
			status,
			sizeof(status),
			&announced,
			APP_POWER_BATTERY_SAMPLE_CHANGED,
			true,
			true,
			APP_POWER_BATTERY_LOW_LEVEL
		)
	);
	assert(strcmp(status, APP_BATTERY_STATUS_OK) == 0);
	assert(!announced);

	snprintf(status, sizeof(status), "%s", APP_BATTERY_STATUS_LOW);
	assert(
		app_battery_status_apply_sample(
			status,
			sizeof(status),
			&announced,
			APP_POWER_BATTERY_SAMPLE_CHANGED,
			true,
			false,
			APP_POWER_BATTERY_LOW_LEVEL + 1
		)
	);
	assert(strcmp(status, APP_BATTERY_STATUS_OK) == 0);
	assert(!announced);
}

static void test_low_status_clears_to_unavailable_after_failed_sample(void)
{
	char status[64] = APP_BATTERY_STATUS_LOW;
	bool announced = true;

	assert(
		app_battery_status_apply_sample(
			status,
			sizeof(status),
			&announced,
			APP_POWER_BATTERY_SAMPLE_READ_FAILED,
			false,
			false,
			0
		)
	);
	assert(strcmp(status, APP_BATTERY_STATUS_UNAVAILABLE) == 0);
	assert(!announced);
}

static void test_closed_shell_does_not_change_status(void)
{
	char status[64] = APP_BATTERY_STATUS_LOW;
	bool announced = true;

	assert(
		!app_battery_status_apply_sample(
			status,
			sizeof(status),
			&announced,
			APP_POWER_BATTERY_SAMPLE_SKIPPED_CLOSED,
			true,
			false,
			APP_POWER_BATTERY_LOW_LEVEL
		)
	);
	assert(strcmp(status, APP_BATTERY_STATUS_LOW) == 0);
	assert(announced);
}

static void test_non_battery_status_stays_intact_on_normal_sample(void)
{
	char status[64] = "Saved";
	bool announced = false;

	assert(
		!app_battery_status_apply_sample(
			status,
			sizeof(status),
			&announced,
			APP_POWER_BATTERY_SAMPLE_CHANGED,
			true,
			false,
			APP_POWER_BATTERY_LOW_LEVEL + 1
		)
	);
	assert(strcmp(status, "Saved") == 0);
	assert(!announced);
}

int main(void)
{
	test_low_sample_announces_once();
	test_low_status_clears_after_charging_or_recovery();
	test_low_status_clears_to_unavailable_after_failed_sample();
	test_closed_shell_does_not_change_status();
	test_non_battery_status_stays_intact_on_normal_sample();
	return 0;
}
