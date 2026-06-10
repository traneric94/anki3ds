#include "app_power.h"

#include <assert.h>
#include <stdbool.h>
#include <time.h>

static void test_battery_poll_schedule(void)
{
	time_t next_poll_time = 0;

	app_power_schedule_next_battery_poll(&next_poll_time, 1000);
	assert(next_poll_time == 1000 + APP_POWER_BATTERY_POLL_INTERVAL_SECONDS);
	assert(!app_power_battery_poll_is_due(&next_poll_time, 1599));
	assert(app_power_battery_poll_is_due(&next_poll_time, 1600));
}

static void test_battery_poll_arms_after_missing_clock(void)
{
	time_t next_poll_time = 0;

	app_power_schedule_next_battery_poll(&next_poll_time, (time_t)-1);
	assert(next_poll_time == 0);
	assert(!app_power_battery_poll_is_due(&next_poll_time, (time_t)-1));
	assert(!app_power_battery_poll_is_due(&next_poll_time, 2000));
	assert(next_poll_time == 2000 + APP_POWER_BATTERY_POLL_INTERVAL_SECONDS);
}

static void test_battery_poll_is_due_after_clock_moves_back(void)
{
	time_t next_poll_time =
		2000 + APP_POWER_BATTERY_POLL_INTERVAL_SECONDS;

	assert(!app_power_battery_poll_is_due(&next_poll_time, 2200));
	assert(next_poll_time == 2000 + APP_POWER_BATTERY_POLL_INTERVAL_SECONDS);
	assert(app_power_battery_poll_is_due(&next_poll_time, 1000));
	assert(next_poll_time == 2000 + APP_POWER_BATTERY_POLL_INTERVAL_SECONDS);
}

static void test_battery_sample_policy(void)
{
	time_t next_poll_time = 1600;

	app_power_schedule_next_battery_poll_after_sample(
		&next_poll_time,
		1600,
		APP_POWER_BATTERY_SAMPLE_UNCHANGED
	);
	assert(next_poll_time == 1600 + APP_POWER_BATTERY_POLL_INTERVAL_SECONDS);
	assert(
		!app_power_battery_sample_changes_display(
			APP_POWER_BATTERY_SAMPLE_SKIPPED_CLOSED
		)
	);
	assert(
		!app_power_battery_sample_changes_display(
			APP_POWER_BATTERY_SAMPLE_READ_FAILED
		)
	);
	assert(
		!app_power_battery_sample_changes_display(
			APP_POWER_BATTERY_SAMPLE_UNCHANGED
		)
	);
	assert(
		app_power_battery_sample_changes_display(
			APP_POWER_BATTERY_SAMPLE_CHANGED
		)
	);

	app_power_schedule_next_battery_poll_after_sample(
		&next_poll_time,
		1600,
		APP_POWER_BATTERY_SAMPLE_READ_FAILED
	);
	assert(next_poll_time == 1600 + APP_POWER_BATTERY_RETRY_INTERVAL_SECONDS);

	app_power_schedule_next_battery_poll_after_sample(
		&next_poll_time,
		1600,
		APP_POWER_BATTERY_SAMPLE_UNAVAILABLE
	);
	assert(next_poll_time == 1600 + APP_POWER_BATTERY_RETRY_INTERVAL_SECONDS);

	app_power_schedule_next_battery_poll_after_sample(
		&next_poll_time,
		(time_t)-1,
		APP_POWER_BATTERY_SAMPLE_UNAVAILABLE
	);
	assert(app_power_battery_poll_is_due(&next_poll_time, 1700));
}

static void test_battery_display_state(void)
{
	assert(
		app_power_battery_display_state(false, false, 0) ==
		APP_POWER_BATTERY_DISPLAY_UNAVAILABLE
	);
	assert(
		app_power_battery_display_state(true, true, 1) ==
		APP_POWER_BATTERY_DISPLAY_CHARGING
	);
	assert(
		app_power_battery_display_state(
			true,
			false,
			APP_POWER_BATTERY_LOW_LEVEL
		) == APP_POWER_BATTERY_DISPLAY_LOW
	);
	assert(
		app_power_battery_display_state(
			true,
			false,
			APP_POWER_BATTERY_LOW_LEVEL + 1
		) == APP_POWER_BATTERY_DISPLAY_NORMAL
	);
	assert(
		app_power_battery_save_warning_needed(
			true,
			false,
			APP_POWER_BATTERY_LOW_LEVEL
		)
	);
	assert(
		!app_power_battery_save_warning_needed(
			true,
			true,
			APP_POWER_BATTERY_LOW_LEVEL
		)
	);
	assert(!app_power_battery_save_warning_needed(false, false, 0));
}

static void test_low_battery_warning_latch(void)
{
	bool announced = false;

	assert(
		app_power_battery_low_warning_due(
			&announced,
			true,
			false,
			APP_POWER_BATTERY_LOW_LEVEL
		)
	);
	assert(announced);
	assert(
		!app_power_battery_low_warning_due(
			&announced,
			true,
			false,
			APP_POWER_BATTERY_LOW_LEVEL
		)
	);
	assert(
		!app_power_battery_low_warning_due(
			&announced,
			true,
			true,
			APP_POWER_BATTERY_LOW_LEVEL
		)
	);
	assert(!announced);
	assert(
		app_power_battery_low_warning_due(
			&announced,
			true,
			false,
			APP_POWER_BATTERY_LOW_LEVEL
		)
	);
	assert(
		!app_power_battery_low_warning_due(
			NULL,
			true,
			false,
			APP_POWER_BATTERY_LOW_LEVEL
		)
	);
}

static void test_idle_input_backoff(void)
{
	assert(
		app_power_idle_input_wait_ns(0) ==
		APP_POWER_IDLE_INPUT_WAIT_INITIAL_NS
	);
	assert(
		app_power_idle_input_wait_ns(
			APP_POWER_IDLE_INPUT_FAST_WAIT_COUNT - 1
		) == APP_POWER_IDLE_INPUT_WAIT_INITIAL_NS
	);
	assert(
		app_power_idle_input_wait_ns(APP_POWER_IDLE_INPUT_FAST_WAIT_COUNT) ==
		APP_POWER_IDLE_INPUT_WAIT_MID_NS
	);
	assert(
		app_power_idle_input_wait_ns(
			APP_POWER_IDLE_INPUT_MID_WAIT_COUNT - 1
		) == APP_POWER_IDLE_INPUT_WAIT_MID_NS
	);
	assert(
		app_power_idle_input_wait_ns(APP_POWER_IDLE_INPUT_MID_WAIT_COUNT) ==
		APP_POWER_IDLE_INPUT_WAIT_MAX_NS
	);
	assert(APP_POWER_IDLE_INPUT_WAIT_MAX_NS >= 1000000000LL);
	assert(app_power_next_idle_input_wait_count(0) == 1);
	assert(
		app_power_next_idle_input_wait_count(
			APP_POWER_IDLE_INPUT_MAX_WAIT_COUNT - 1
		) == APP_POWER_IDLE_INPUT_MAX_WAIT_COUNT
	);
	assert(
		app_power_next_idle_input_wait_count(
			APP_POWER_IDLE_INPUT_MAX_WAIT_COUNT
		) == APP_POWER_IDLE_INPUT_MAX_WAIT_COUNT
	);
}

int main(void)
{
	test_battery_poll_schedule();
	test_battery_poll_arms_after_missing_clock();
	test_battery_poll_is_due_after_clock_moves_back();
	test_battery_sample_policy();
	test_battery_display_state();
	test_low_battery_warning_latch();
	test_idle_input_backoff();
	return 0;
}
