#include "study_settings.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void write_file(const char *path, const char *contents)
{
	FILE *file = fopen(path, "w");

	assert(file != NULL);
	assert(fputs(contents, file) >= 0);
	assert(fclose(file) == 0);
}

static void remove_if_present(const char *path)
{
	(void)remove(path);
}

static void test_defaults(void)
{
	struct study_settings settings;

	study_settings_defaults(&settings);
	assert(settings.new_limit == STUDY_SETTINGS_DEFAULT_NEW_LIMIT);
	assert(settings.review_limit == STUDY_SETTINGS_DEFAULT_REVIEW_LIMIT);
	assert(settings.learning_mode == STUDY_SETTINGS_DEFAULT_LEARNING_MODE);
}

static void test_missing_file_uses_defaults(void)
{
	struct study_settings settings;

	assert(
		study_settings_load_tsv(
			&settings,
			"/tmp/anki3ds-study-settings-missing.tsv"
		) == STUDY_SETTINGS_NOT_FOUND
	);
	assert(settings.new_limit == STUDY_SETTINGS_DEFAULT_NEW_LIMIT);
	assert(settings.review_limit == STUDY_SETTINGS_DEFAULT_REVIEW_LIMIT);
	assert(settings.learning_mode == STUDY_SETTINGS_DEFAULT_LEARNING_MODE);
	assert(
		strcmp(
			study_settings_result_name(STUDY_SETTINGS_NOT_FOUND),
			"Default settings"
		) == 0
	);
}

static void test_load_old_settings_defaults_learning_mode(void)
{
	const char *path = "/tmp/anki3ds-study-settings-old.tsv";
	struct study_settings settings;

	write_file(path, "new_limit\t7\nreview_limit\t9\n");

	assert(study_settings_load_tsv(&settings, path) == STUDY_SETTINGS_OK);
	assert(settings.new_limit == 7);
	assert(settings.review_limit == 9);
	assert(settings.learning_mode == STUDY_SETTINGS_LEARNING_DUE_FIRST);
	assert(remove(path) == 0);
}

static void test_load_and_save_round_trip(void)
{
	const char *path = "/tmp/anki3ds-study-settings.tsv";
	const char *tmp_path = "/tmp/anki3ds-study-settings.tsv.tmp";
	const char *bak_path = "/tmp/anki3ds-study-settings.tsv.bak";
	struct study_settings settings;
	struct study_settings loaded;

	remove_if_present(path);
	remove_if_present(tmp_path);
	remove_if_present(bak_path);
	study_settings_defaults(&settings);
	settings.new_limit = 5;
	settings.review_limit = 10;
	settings.learning_mode = STUDY_SETTINGS_LEARNING_CARD_COOLDOWN;
	assert(study_settings_save_tsv(&settings, path) == STUDY_SETTINGS_OK);
	assert(study_settings_load_tsv(&loaded, path) == STUDY_SETTINGS_OK);
	assert(loaded.new_limit == 5);
	assert(loaded.review_limit == 10);
	assert(loaded.learning_mode == STUDY_SETTINGS_LEARNING_CARD_COOLDOWN);
	assert(remove(path) == 0);
	remove_if_present(tmp_path);
	remove_if_present(bak_path);
}

static void test_learning_mode_helpers(void)
{
	assert(
		study_settings_next_learning_mode(
			STUDY_SETTINGS_LEARNING_DUE_FIRST
		) == STUDY_SETTINGS_LEARNING_CARD_COOLDOWN
	);
	assert(
		study_settings_next_learning_mode(
			STUDY_SETTINGS_LEARNING_CARD_COOLDOWN
		) == STUDY_SETTINGS_LEARNING_DUE_FIRST
	);
	assert(
		study_settings_learning_mode_is_valid(
			STUDY_SETTINGS_LEARNING_DUE_FIRST
		)
	);
	assert(
		!study_settings_learning_mode_is_valid(
			(enum study_settings_learning_mode)99
		)
	);
	assert(
		strcmp(
			study_settings_learning_mode_label(
				STUDY_SETTINGS_LEARNING_CARD_COOLDOWN
			),
			"Cooldown"
		) == 0
	);
}

static void test_limit_presets_cycle_and_snap(void)
{
	assert(study_settings_next_limit_preset(20, true) == 50);
	assert(study_settings_next_limit_preset(20, false) == 10);
	assert(study_settings_next_limit_preset(0, true) == 5);
	assert(study_settings_next_limit_preset(0, false) == 1000);
	assert(study_settings_next_limit_preset(7, true) == 10);
	assert(study_settings_next_limit_preset(7, false) == 5);
	assert(study_settings_next_limit_preset(3, false) == 0);
	assert(study_settings_next_limit_preset(1500, true) == 0);
	assert(study_settings_next_limit_preset(1500, false) == 1000);
}

static void test_limit_policy_blocks_only_exhausted_new_cards(void)
{
	assert(!study_settings_new_limit_blocks_reveal(0, 100, false));
	assert(!study_settings_new_limit_blocks_reveal(2, 1, false));
	assert(study_settings_new_limit_blocks_reveal(2, 2, false));
	assert(study_settings_new_limit_blocks_reveal(2, 3, false));
	assert(!study_settings_new_limit_blocks_reveal(2, 2, true));
}

static void test_limit_policy_blocks_only_exhausted_reviews(void)
{
	assert(!study_settings_review_limit_blocks_rating(0, 100));
	assert(!study_settings_review_limit_blocks_rating(2, 1));
	assert(study_settings_review_limit_blocks_rating(2, 2));
	assert(study_settings_review_limit_blocks_rating(2, 3));
}

static void test_bad_settings_restore_defaults(void)
{
	const char *path = "/tmp/anki3ds-study-settings-bad.tsv";
	struct study_settings settings;

	write_file(path, "new_limit\t20\n");
	settings.new_limit = 1;
	settings.review_limit = 1;
	settings.learning_mode = STUDY_SETTINGS_LEARNING_CARD_COOLDOWN;
	assert(study_settings_load_tsv(&settings, path) == STUDY_SETTINGS_BAD_FORMAT);
	assert(settings.new_limit == STUDY_SETTINGS_DEFAULT_NEW_LIMIT);
	assert(settings.review_limit == STUDY_SETTINGS_DEFAULT_REVIEW_LIMIT);
	assert(settings.learning_mode == STUDY_SETTINGS_DEFAULT_LEARNING_MODE);
	assert(remove(path) == 0);
}

static void test_duplicate_settings_rejected(void)
{
	const char *path = "/tmp/anki3ds-study-settings-dup.tsv";
	struct study_settings settings;

	write_file(
		path,
		"new_limit\t20\n"
		"new_limit\t30\n"
		"review_limit\t200\n"
	);
	assert(study_settings_load_tsv(&settings, path) == STUDY_SETTINGS_BAD_FORMAT);
	assert(remove(path) == 0);
}

static void test_duplicate_learning_mode_rejected(void)
{
	const char *path = "/tmp/anki3ds-study-settings-dup-learning.tsv";
	struct study_settings settings;

	write_file(
		path,
		"new_limit\t20\n"
		"review_limit\t200\n"
		"learning_mode\t0\n"
		"learning_mode\t1\n"
	);
	assert(study_settings_load_tsv(&settings, path) == STUDY_SETTINGS_BAD_FORMAT);
	assert(remove(path) == 0);
}

static void test_bad_learning_mode_rejected(void)
{
	const char *path = "/tmp/anki3ds-study-settings-bad-learning.tsv";
	struct study_settings settings;

	write_file(
		path,
		"new_limit\t20\n"
		"review_limit\t200\n"
		"learning_mode\t2\n"
	);
	assert(study_settings_load_tsv(&settings, path) == STUDY_SETTINGS_BAD_FORMAT);
	assert(remove(path) == 0);
}

static void test_load_recovers_from_temp_artifact(void)
{
	const char *path = "/tmp/anki3ds-study-settings-recover-temp.tsv";
	const char *tmp_path = "/tmp/anki3ds-study-settings-recover-temp.tsv.tmp";
	const char *bak_path = "/tmp/anki3ds-study-settings-recover-temp.tsv.bak";
	struct study_settings settings;

	remove_if_present(path);
	remove_if_present(tmp_path);
	remove_if_present(bak_path);
	write_file(path, "new_limit\t20\n");
	write_file(tmp_path, "new_limit\t5\nreview_limit\t10\n");

	assert(study_settings_load_tsv(&settings, path) == STUDY_SETTINGS_OK);
	assert(settings.new_limit == 5);
	assert(settings.review_limit == 10);
	assert(settings.learning_mode == STUDY_SETTINGS_DEFAULT_LEARNING_MODE);

	assert(remove(path) == 0);
	assert(remove(tmp_path) == 0);
	remove_if_present(bak_path);
}

static void test_load_recovers_from_backup_artifact(void)
{
	const char *path = "/tmp/anki3ds-study-settings-recover-backup.tsv";
	const char *tmp_path = "/tmp/anki3ds-study-settings-recover-backup.tsv.tmp";
	const char *bak_path = "/tmp/anki3ds-study-settings-recover-backup.tsv.bak";
	struct study_settings settings;

	remove_if_present(path);
	remove_if_present(tmp_path);
	remove_if_present(bak_path);
	write_file(bak_path, "new_limit\t50\nreview_limit\t100\n");

	assert(study_settings_load_tsv(&settings, path) == STUDY_SETTINGS_OK);
	assert(settings.new_limit == 50);
	assert(settings.review_limit == 100);
	assert(settings.learning_mode == STUDY_SETTINGS_DEFAULT_LEARNING_MODE);

	remove_if_present(path);
	remove_if_present(tmp_path);
	assert(remove(bak_path) == 0);
}

static void test_save_preserves_previous_primary_as_backup(void)
{
	const char *path = "/tmp/anki3ds-study-settings-backup.tsv";
	const char *tmp_path = "/tmp/anki3ds-study-settings-backup.tsv.tmp";
	const char *bak_path = "/tmp/anki3ds-study-settings-backup.tsv.bak";
	struct study_settings settings;
	struct study_settings loaded;

	remove_if_present(path);
	remove_if_present(tmp_path);
	remove_if_present(bak_path);
	write_file(path, "new_limit\t20\nreview_limit\t200\n");
	study_settings_defaults(&settings);
	settings.new_limit = 5;
	settings.review_limit = 10;
	settings.learning_mode = STUDY_SETTINGS_LEARNING_CARD_COOLDOWN;

	assert(study_settings_save_tsv(&settings, path) == STUDY_SETTINGS_OK);
	assert(study_settings_load_tsv(&loaded, path) == STUDY_SETTINGS_OK);
	assert(loaded.new_limit == 5);
	assert(loaded.review_limit == 10);
	assert(loaded.learning_mode == STUDY_SETTINGS_LEARNING_CARD_COOLDOWN);
	assert(study_settings_load_tsv(&loaded, bak_path) == STUDY_SETTINGS_OK);
	assert(loaded.new_limit == 20);
	assert(loaded.review_limit == 200);
	assert(loaded.learning_mode == STUDY_SETTINGS_DEFAULT_LEARNING_MODE);

	assert(remove(path) == 0);
	remove_if_present(tmp_path);
	assert(remove(bak_path) == 0);
}

int main(void)
{
	test_defaults();
	test_missing_file_uses_defaults();
	test_load_old_settings_defaults_learning_mode();
	test_load_and_save_round_trip();
	test_learning_mode_helpers();
	test_limit_presets_cycle_and_snap();
	test_limit_policy_blocks_only_exhausted_new_cards();
	test_limit_policy_blocks_only_exhausted_reviews();
	test_bad_settings_restore_defaults();
	test_duplicate_settings_rejected();
	test_duplicate_learning_mode_rejected();
	test_bad_learning_mode_rejected();
	test_load_recovers_from_temp_artifact();
	test_load_recovers_from_backup_artifact();
	test_save_preserves_previous_primary_as_backup();
	return 0;
}
