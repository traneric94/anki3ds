#include "app_battery_monitor.h"

#include <3ds.h>

#include "app_battery_status.h"
#include "study_backend.h"

#include <assert.h>
#include <string.h>

struct fake_ptmu_state
{
	Result init_result;
	Result shell_result;
	u8 shell_state;
	Result level_result;
	u8 level;
	Result charge_result;
	u8 charge_state;
	unsigned int init_count;
	unsigned int exit_count;
	unsigned int shell_count;
	unsigned int level_count;
	unsigned int charge_count;
};

static struct fake_ptmu_state fake_ptmu;

Result ptmuInit(void)
{
	fake_ptmu.init_count++;
	return fake_ptmu.init_result;
}

void ptmuExit(void)
{
	fake_ptmu.exit_count++;
}

Result PTMU_GetShellState(u8 *state)
{
	fake_ptmu.shell_count++;
	if (state != NULL)
		*state = fake_ptmu.shell_state;
	return fake_ptmu.shell_result;
}

Result PTMU_GetBatteryLevel(u8 *level)
{
	fake_ptmu.level_count++;
	if (level != NULL)
		*level = fake_ptmu.level;
	return fake_ptmu.level_result;
}

Result PTMU_GetBatteryChargeState(u8 *state)
{
	fake_ptmu.charge_count++;
	if (state != NULL)
		*state = fake_ptmu.charge_state;
	return fake_ptmu.charge_result;
}

static void reset_fake_ptmu(void)
{
	memset(&fake_ptmu, 0, sizeof(fake_ptmu));
	fake_ptmu.shell_state = 1;
	fake_ptmu.level = APP_POWER_BATTERY_LOW_LEVEL + 2;
}

static void test_init_fail_and_null_are_unavailable(void)
{
	struct app_battery_monitor monitor;

	reset_fake_ptmu();
	app_battery_monitor_init(&monitor);
	fake_ptmu.init_result = -1;

	assert(
		app_battery_monitor_sample(NULL) ==
			APP_POWER_BATTERY_SAMPLE_UNAVAILABLE
	);
	assert(
		app_battery_monitor_sample(&monitor) ==
			APP_POWER_BATTERY_SAMPLE_UNAVAILABLE
	);
	assert(fake_ptmu.init_count == 1);
	assert(!monitor.service_available);
	assert(!monitor.status_available);
}

static void test_open_sample_changes_then_stabilizes(void)
{
	struct app_battery_monitor monitor;

	reset_fake_ptmu();
	app_battery_monitor_init(&monitor);
	fake_ptmu.level = 3;
	fake_ptmu.charge_state = 0;

	assert(
		app_battery_monitor_sample(&monitor) ==
			APP_POWER_BATTERY_SAMPLE_CHANGED
	);
	assert(monitor.service_available);
	assert(monitor.status_available);
	assert(!monitor.charging);
	assert(monitor.level == 3);
	assert(fake_ptmu.init_count == 1);

	assert(
		app_battery_monitor_sample(&monitor) ==
			APP_POWER_BATTERY_SAMPLE_UNCHANGED
	);
	assert(fake_ptmu.init_count == 1);

	fake_ptmu.charge_state = 1;
	assert(
		app_battery_monitor_sample(&monitor) ==
			APP_POWER_BATTERY_SAMPLE_CHANGED
	);
	assert(monitor.charging);
}

static void test_closed_shell_and_read_failures_do_not_overwrite_status(void)
{
	struct app_battery_monitor monitor;

	reset_fake_ptmu();
	app_battery_monitor_init(&monitor);
	fake_ptmu.shell_state = 0;
	assert(
		app_battery_monitor_sample(&monitor) ==
			APP_POWER_BATTERY_SAMPLE_SKIPPED_CLOSED
	);
	assert(monitor.service_available);
	assert(!monitor.status_available);
	assert(fake_ptmu.level_count == 0);

	fake_ptmu.shell_state = 1;
	fake_ptmu.level = 4;
	assert(
		app_battery_monitor_sample(&monitor) ==
			APP_POWER_BATTERY_SAMPLE_CHANGED
	);
	assert(monitor.status_available);
	assert(monitor.level == 4);

	fake_ptmu.level_result = -1;
	assert(
		app_battery_monitor_sample(&monitor) ==
			APP_POWER_BATTERY_SAMPLE_READ_FAILED
	);
	assert(monitor.status_available);
	assert(monitor.level == 4);
}

static void test_warning_status_and_save_suffix_policy(void)
{
	struct app_battery_monitor monitor;
	struct study_backend backend;
	enum app_power_battery_sample_result sample_result;

	reset_fake_ptmu();
	app_battery_monitor_init(&monitor);
	study_backend_init(&backend, NULL, 0);
	study_backend_set_status(&backend, "Reviewing");
	fake_ptmu.level = APP_POWER_BATTERY_LOW_LEVEL;

	sample_result = app_battery_monitor_sample(&monitor);
	assert(sample_result == APP_POWER_BATTERY_SAMPLE_CHANGED);
	assert(
		app_battery_monitor_apply_warning(
			&monitor,
			&backend,
			sample_result
		)
	);
	assert(strcmp(backend.status_text, APP_BATTERY_STATUS_LOW) == 0);
	assert(monitor.low_warning_announced);
	assert(app_battery_monitor_save_warning_needed(&monitor));

	sample_result = app_battery_monitor_sample(&monitor);
	assert(sample_result == APP_POWER_BATTERY_SAMPLE_UNCHANGED);
	assert(
		!app_battery_monitor_apply_warning(
			&monitor,
			&backend,
			sample_result
		)
	);
	assert(strcmp(backend.status_text, APP_BATTERY_STATUS_LOW) == 0);

	fake_ptmu.charge_state = 1;
	sample_result = app_battery_monitor_sample(&monitor);
	assert(sample_result == APP_POWER_BATTERY_SAMPLE_CHANGED);
	assert(
		app_battery_monitor_apply_warning(
			&monitor,
			&backend,
			sample_result
		)
	);
	assert(strcmp(backend.status_text, APP_BATTERY_STATUS_OK) == 0);
	assert(!monitor.low_warning_announced);
	assert(!app_battery_monitor_save_warning_needed(&monitor));
}

static void test_shutdown_exits_service_once(void)
{
	struct app_battery_monitor monitor;

	reset_fake_ptmu();
	app_battery_monitor_init(&monitor);
	assert(
		app_battery_monitor_sample(&monitor) ==
			APP_POWER_BATTERY_SAMPLE_CHANGED
	);

	app_battery_monitor_shutdown(&monitor);
	assert(fake_ptmu.exit_count == 1);
	assert(!monitor.service_available);

	app_battery_monitor_shutdown(&monitor);
	assert(fake_ptmu.exit_count == 1);
}

int main(void)
{
	test_init_fail_and_null_are_unavailable();
	test_open_sample_changes_then_stabilizes();
	test_closed_shell_and_read_failures_do_not_overwrite_status();
	test_warning_status_and_save_suffix_policy();
	test_shutdown_exits_service_once();
	return 0;
}
