#include "app_confirm_action.h"

#include "study_controls.h"

#include <assert.h>
#include <string.h>

static void test_confirm_action_rejects_chords(void)
{
	assert(
		app_confirm_action_apply(
			APP_CONFIRM_ACTION_RESET,
			STUDY_CONTROL_BUTTON_X | STUDY_CONTROL_BUTTON_B
		) == APP_CONFIRM_ACTION_NONE
	);
	assert(
		app_confirm_action_apply(
			APP_CONFIRM_ACTION_SUSPEND,
			STUDY_CONTROL_BUTTON_START | STUDY_CONTROL_BUTTON_A
		) == APP_CONFIRM_ACTION_NONE
	);
}

static void test_confirm_action_cancels_with_b_or_select(void)
{
	assert(
		app_confirm_action_apply(
			APP_CONFIRM_ACTION_RESTORE,
			STUDY_CONTROL_BUTTON_B
		) == APP_CONFIRM_ACTION_CANCEL
	);
	assert(
		app_confirm_action_apply(
			APP_CONFIRM_ACTION_EXIT,
			STUDY_CONTROL_BUTTON_SELECT
		) == APP_CONFIRM_ACTION_CANCEL
	);
}

static void test_confirm_action_uses_contextual_confirm_buttons(void)
{
	assert(
		app_confirm_action_apply(
			APP_CONFIRM_ACTION_SUSPEND,
			STUDY_CONTROL_BUTTON_X
		) == APP_CONFIRM_ACTION_CONFIRM
	);
	assert(
		app_confirm_action_apply(
			APP_CONFIRM_ACTION_EXIT,
			STUDY_CONTROL_BUTTON_A
		) == APP_CONFIRM_ACTION_CONFIRM
	);
	assert(
		app_confirm_action_apply(
			APP_CONFIRM_ACTION_EXIT,
			STUDY_CONTROL_BUTTON_X
		) == APP_CONFIRM_ACTION_NONE
	);
	assert(
		app_confirm_action_apply(
			APP_CONFIRM_ACTION_RESET,
			STUDY_CONTROL_BUTTON_A
		) == APP_CONFIRM_ACTION_NONE
	);
}

static void test_confirm_action_start_opens_exit_from_non_exit(void)
{
	assert(
		app_confirm_action_apply(
			APP_CONFIRM_ACTION_RESET,
			STUDY_CONTROL_BUTTON_START
		) == APP_CONFIRM_ACTION_OPEN_EXIT
	);
	assert(
		app_confirm_action_apply(
			APP_CONFIRM_ACTION_EXIT,
			STUDY_CONTROL_BUTTON_START
		) == APP_CONFIRM_ACTION_NONE
	);
}

static void test_confirm_action_unknown_kind_cannot_confirm(void)
{
	enum app_confirm_action_kind unknown_kind =
		(enum app_confirm_action_kind)99;

	assert(
		app_confirm_action_apply(
			unknown_kind,
			STUDY_CONTROL_BUTTON_X
		) == APP_CONFIRM_ACTION_NONE
	);
	assert(
		app_confirm_action_apply(
			unknown_kind,
			STUDY_CONTROL_BUTTON_A
		) == APP_CONFIRM_ACTION_NONE
	);
	assert(
		app_confirm_action_apply(
			unknown_kind,
			STUDY_CONTROL_BUTTON_START
		) == APP_CONFIRM_ACTION_NONE
	);
	assert(
		app_confirm_action_apply(
			unknown_kind,
			STUDY_CONTROL_BUTTON_B
		) == APP_CONFIRM_ACTION_CANCEL
	);
}

static void test_confirm_action_cancel_statuses(void)
{
	assert(
		strcmp(
			app_confirm_action_cancel_status(APP_CONFIRM_ACTION_EXIT),
			"Exit canceled"
		) == 0
	);
	assert(
		strcmp(
			app_confirm_action_cancel_status(APP_CONFIRM_ACTION_RESET),
			"Reset canceled"
		) == 0
	);
	assert(
		strcmp(
			app_confirm_action_cancel_status(APP_CONFIRM_ACTION_RESTORE),
			"Restore canceled"
		) == 0
	);
	assert(
		strcmp(
			app_confirm_action_cancel_status(APP_CONFIRM_ACTION_SUSPEND),
			"Suspend canceled"
		) == 0
	);
}

int main(void)
{
	test_confirm_action_rejects_chords();
	test_confirm_action_cancels_with_b_or_select();
	test_confirm_action_uses_contextual_confirm_buttons();
	test_confirm_action_start_opens_exit_from_non_exit();
	test_confirm_action_unknown_kind_cannot_confirm();
	test_confirm_action_cancel_statuses();
	return 0;
}
