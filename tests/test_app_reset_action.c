#include "app_reset_action.h"

#include "app_status_text.h"
#include "study_backend.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST_STATE_PATH "/tmp/anki3ds-app-reset-action-state.tsv"
#define TEST_STATE_TMP_PATH "/tmp/anki3ds-app-reset-action-state.tsv.tmp"
#define TEST_STATE_BAK_PATH "/tmp/anki3ds-app-reset-action-state.tsv.bak"
#define TEST_LOG_PATH "/tmp/anki3ds-app-reset-action-review-log.tsv"
#define TEST_LOG_TMP_PATH "/tmp/anki3ds-app-reset-action-review-log.tsv.tmp"
#define TEST_LOG_BAK_PATH "/tmp/anki3ds-app-reset-action-review-log.tsv.bak"

static const struct study_backend_card cards[] = {
	{ "Front 1", "Back 1", NULL },
	{ "Front 2", "Back 2", NULL },
};

static void remove_file_if_present(const char *path)
{
	if (path != NULL)
		(void)remove(path);
}

static void remove_test_files(void)
{
	remove_file_if_present(TEST_STATE_PATH);
	remove_file_if_present(TEST_STATE_TMP_PATH);
	remove_file_if_present(TEST_STATE_BAK_PATH);
	remove_file_if_present(TEST_LOG_PATH);
	remove_file_if_present(TEST_LOG_TMP_PATH);
	remove_file_if_present(TEST_LOG_BAK_PATH);
}

static void write_test_file(const char *path, const char *contents)
{
	FILE *file = fopen(path, "w");

	assert(file != NULL);
	assert(fputs(contents, file) >= 0);
	assert(fclose(file) == 0);
}

static bool file_exists(const char *path)
{
	return access(path, F_OK) == 0;
}

static void make_dirty_progress(struct study_backend *backend)
{
	assert(study_backend_show_answer(backend));
	assert(study_backend_rate_current(backend, STUDY_BACKEND_RATING_GOOD));
	assert(study_backend_show_answer(backend));
	assert(study_backend_suspend_current(backend));
}

static void test_confirm_reset_deletes_state_and_log_artifacts(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	remove_test_files();
	study_backend_init(&backend, cards, 2);
	make_dirty_progress(&backend);
	write_test_file(TEST_STATE_PATH, "version\t1\n");
	write_test_file(TEST_STATE_TMP_PATH, "version\t1\n");
	write_test_file(TEST_STATE_BAK_PATH, "version\t1\n");
	write_test_file(TEST_LOG_PATH, "row\n");
	write_test_file(TEST_LOG_TMP_PATH, "tmp\n");
	write_test_file(TEST_LOG_BAK_PATH, "bak\n");

	assert(
		app_reset_action_confirm(
			&backend,
			true,
			TEST_STATE_PATH,
			TEST_LOG_PATH,
			"Settings ignored",
			true
		)
	);
	study_backend_build_view(&backend, &view);

	assert(view.has_active_card);
	assert(!view.answer_visible);
	assert(view.card_index == 0);
	assert(view.reviewed_count == 0);
	assert(view.suspended_count == 0);
	assert(strstr(view.status_text, "Progress reset") != NULL);
	assert(strstr(view.status_text, "Settings ignored") != NULL);
	assert(app_status_text_has_low_battery_suffix(view.status_text));
	assert(!file_exists(TEST_STATE_PATH));
	assert(!file_exists(TEST_STATE_TMP_PATH));
	assert(!file_exists(TEST_STATE_BAK_PATH));
	assert(!file_exists(TEST_LOG_PATH));
	assert(!file_exists(TEST_LOG_TMP_PATH));
	assert(!file_exists(TEST_LOG_BAK_PATH));
	remove_test_files();
}

static void test_confirm_reset_without_enabled_state_skips_file_cleanup(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	make_dirty_progress(&backend);

	assert(
		app_reset_action_confirm(
			&backend,
			false,
			NULL,
			NULL,
			"",
			false
		)
	);
	study_backend_build_view(&backend, &view);

	assert(view.has_active_card);
	assert(view.reviewed_count == 0);
	assert(view.suspended_count == 0);
	assert(strcmp(view.status_text, "Progress reset") == 0);
}

static void test_confirm_reset_reports_state_delete_failure(void)
{
	struct study_backend backend;
	struct study_backend_view view;

	study_backend_init(&backend, cards, 2);
	make_dirty_progress(&backend);

	assert(
		!app_reset_action_confirm(
			&backend,
			true,
			NULL,
			TEST_LOG_PATH,
			"Settings ignored",
			true
		)
	);
	study_backend_build_view(&backend, &view);

	assert(view.reviewed_count == 1);
	assert(view.suspended_count == 1);
	assert(strcmp(view.status_text, "Reset; state delete failed") == 0);
}

static void test_confirm_reset_reports_log_delete_failure_after_state_cleanup(void)
{
	struct study_backend backend;
	struct study_backend_view view;
	char long_log_path[640];

	remove_test_files();
	memset(long_log_path, 'x', sizeof(long_log_path) - 1);
	long_log_path[sizeof(long_log_path) - 1] = '\0';
	study_backend_init(&backend, cards, 2);
	make_dirty_progress(&backend);
	write_test_file(TEST_STATE_PATH, "version\t1\n");

	assert(
		app_reset_action_confirm(
			&backend,
			true,
			TEST_STATE_PATH,
			long_log_path,
			"Settings ignored",
			true
		)
	);
	study_backend_build_view(&backend, &view);

	assert(view.reviewed_count == 0);
	assert(view.suspended_count == 0);
	assert(strcmp(view.status_text, "Progress reset; log kept; Settings ignored; batt low") == 0);
	assert(app_status_text_has_low_battery_suffix(view.status_text));
	assert(!file_exists(TEST_STATE_PATH));
	remove_test_files();
}

int main(void)
{
	test_confirm_reset_deletes_state_and_log_artifacts();
	test_confirm_reset_without_enabled_state_skips_file_cleanup();
	test_confirm_reset_reports_state_delete_failure();
	test_confirm_reset_reports_log_delete_failure_after_state_cleanup();
	return 0;
}
