#include "app_status_text.h"

#include <assert.h>
#include <string.h>

static void test_suffix_is_appended_when_requested(void)
{
	char status[64];

	assert(
		app_status_text_copy_with_low_battery_suffix(
			status,
			sizeof(status),
			"Good saved",
			true
		)
	);
	assert(strcmp(status, "Good saved; batt low") == 0);
	assert(app_status_text_has_low_battery_suffix(status));
}

static void test_suffix_is_not_duplicated(void)
{
	char status[64];

	assert(
		!app_status_text_copy_with_low_battery_suffix(
			status,
			sizeof(status),
			"Good saved; batt low",
			true
		)
	);
	assert(strcmp(status, "Good saved; batt low") == 0);
}

static void test_suffix_is_skipped_when_not_requested(void)
{
	char status[64];

	assert(
		!app_status_text_copy_with_low_battery_suffix(
			status,
			sizeof(status),
			"Settings saved",
			false
		)
	);
	assert(strcmp(status, "Settings saved") == 0);
}

static void test_null_status_uses_suffix_when_requested(void)
{
	char status[64];

	assert(
		app_status_text_copy_with_low_battery_suffix(
			status,
			sizeof(status),
			NULL,
			true
		)
	);
	assert(strcmp(status, APP_STATUS_TEXT_LOW_BATTERY_SUFFIX) == 0);
}

static void test_truncation_preserves_suffix_when_possible(void)
{
	char status[18];

	assert(
		app_status_text_copy_with_low_battery_suffix(
			status,
			sizeof(status),
			"Very long successful save status",
			true
		)
	);
	assert(strcmp(status, "Very lo; batt low") == 0);
}

static void test_rejects_missing_destination(void)
{
	assert(
		!app_status_text_copy_with_low_battery_suffix(
			NULL,
			0,
			"Saved",
			true
		)
	);
	assert(!app_status_text_has_low_battery_suffix(NULL));
}

static void test_warning_context_appends_relevant_segments(void)
{
	char status[80];

	assert(
		app_status_text_copy_with_warning_context(
			status,
			sizeof(status),
			"Settings saved",
			"no changes; Settings ignored"
		)
	);
	assert(strcmp(status, "Settings saved; Settings ignored") == 0);
}

static void test_warning_context_keeps_multiple_warning_segments(void)
{
	char status[80];

	assert(
		app_status_text_copy_with_warning_context(
			status,
			sizeof(status),
			"Good; card 2/3",
			"Answer; Review limit reached; batt low"
		)
	);
	assert(strcmp(status, "Good; card 2/3; Review limit reached; batt low") == 0);
}

static void test_warning_context_skips_plain_status(void)
{
	char status[80];

	assert(
		!app_status_text_copy_with_warning_context(
			status,
			sizeof(status),
			"Answer",
			"Question"
		)
	);
	assert(strcmp(status, "Answer") == 0);
}

static void test_warning_context_is_not_duplicated(void)
{
	char status[80];

	assert(
		!app_status_text_copy_with_warning_context(
			status,
			sizeof(status),
			"Settings saved; Settings ignored",
			"Settings ignored"
		)
	);
	assert(strcmp(status, "Settings saved; Settings ignored") == 0);
}

static void test_unsaved_edit_status_is_not_preserved_after_save(void)
{
	char status[80];

	assert(
		app_status_text_copy_with_warning_context(
			status,
			sizeof(status),
			"Settings saved",
			"unsaved changes; Settings ignored"
		)
	);
	assert(strcmp(status, "Settings saved; Settings ignored") == 0);
}

static void test_exit_unsaved_warning_context_is_preserved(void)
{
	char status[80];

	assert(
		app_status_text_copy_with_warning_context(
			status,
			sizeof(status),
			"Exit canceled",
			"Exit loses unsaved settings"
		)
	);
	assert(strcmp(status, "Exit canceled; Exit loses unsaved settings") == 0);
}

static void test_warning_context_and_low_battery_suffix_compose(void)
{
	char status[80];

	assert(
		app_status_text_copy_with_warning_context_and_low_battery_suffix(
			status,
			sizeof(status),
			"Settings saved",
			"unsaved changes; Settings ignored",
			true
		)
	);
	assert(strcmp(status, "Settings saved; Settings ignored; batt low") == 0);
}

static void test_warning_context_and_low_battery_suffix_skip_duplicate(void)
{
	char status[80];

	assert(
		app_status_text_copy_with_warning_context_and_low_battery_suffix(
			status,
			sizeof(status),
			"Good saved",
			"Review limit reached; batt low",
			true
		)
	);
	assert(strcmp(status, "Good saved; Review limit reached; batt low") == 0);
}

static void test_warning_context_and_low_battery_suffix_can_skip_suffix(void)
{
	char status[80];

	assert(
		app_status_text_copy_with_warning_context_and_low_battery_suffix(
			status,
			sizeof(status),
			"Settings saved",
			"Settings ignored",
			false
		)
	);
	assert(strcmp(status, "Settings saved; Settings ignored") == 0);
}

int main(void)
{
	test_suffix_is_appended_when_requested();
	test_suffix_is_not_duplicated();
	test_suffix_is_skipped_when_not_requested();
	test_null_status_uses_suffix_when_requested();
	test_truncation_preserves_suffix_when_possible();
	test_rejects_missing_destination();
	test_warning_context_appends_relevant_segments();
	test_warning_context_keeps_multiple_warning_segments();
	test_warning_context_skips_plain_status();
	test_warning_context_is_not_duplicated();
	test_unsaved_edit_status_is_not_preserved_after_save();
	test_exit_unsaved_warning_context_is_preserved();
	test_warning_context_and_low_battery_suffix_compose();
	test_warning_context_and_low_battery_suffix_skip_duplicate();
	test_warning_context_and_low_battery_suffix_can_skip_suffix();
	return 0;
}
