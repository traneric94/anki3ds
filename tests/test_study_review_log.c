#include "study_review_log.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_LOG_PATH "/tmp/anki3ds-study-review-log.tsv"
#define TEST_LOG_TEMP_PATH "/tmp/anki3ds-study-review-log.tsv.tmp"
#define TEST_LOG_BACKUP_PATH "/tmp/anki3ds-study-review-log.tsv.bak"
#define VALID_RATING_ROW "111\trating\tgood\t0\t1\t0\n"
#define VALID_PENDING_ROW "222\tundo\t-\t1\t2\t0\n"
#define VALID_BACKUP_ROW "333\tsuspend\t-\t2\t3\t1\n"

static void remove_logs(void)
{
	(void)remove(TEST_LOG_PATH);
	(void)remove(TEST_LOG_TEMP_PATH);
	(void)remove(TEST_LOG_BACKUP_PATH);
}

static void write_file(const char *path, const char *contents)
{
	FILE *file = fopen(path, "w");

	assert(file != NULL);
	assert(fputs(contents, file) >= 0);
	assert(fclose(file) == 0);
}

static void write_nearly_full_valid_log(const char *path)
{
	FILE *file = fopen(path, "wb");
	size_t size = 0;
	const char *row = "1\trating\tgood\t0\t0\t0\n";
	size_t row_size = strlen(row);

	assert(file != NULL);
	while (size + row_size <= STUDY_REVIEW_LOG_MAX_BYTES)
	{
		assert(fputs(row, file) >= 0);
		size += row_size;
	}
	assert(fclose(file) == 0);
}

static long file_size(const char *path)
{
	struct stat status;

	assert(stat(path, &status) == 0);
	return status.st_size;
}

static void read_file(char *destination, size_t destination_size)
{
	FILE *file = fopen(TEST_LOG_PATH, "rb");
	size_t bytes_read;

	assert(file != NULL);
	bytes_read = fread(destination, 1, destination_size - 1, file);
	assert(!ferror(file));
	destination[bytes_read] = '\0';
	assert(fclose(file) == 0);
}

static struct study_review_log_entry sample_entry(
	enum study_review_log_event event,
	enum study_review_log_rating rating
)
{
	struct study_review_log_entry entry;

	entry.timestamp = 12345;
	entry.event = event;
	entry.rating = rating;
	entry.card_index = 7;
	entry.reviewed_count = 3;
	entry.suspended_count = 1;
	return entry;
}

static void test_append_writes_rating_row(void)
{
	char contents[256];
	struct study_review_log_entry entry =
		sample_entry(STUDY_REVIEW_LOG_EVENT_RATING, STUDY_REVIEW_LOG_RATING_GOOD);

	remove_logs();
	assert(study_review_log_append(TEST_LOG_PATH, &entry));
	read_file(contents, sizeof(contents));

	assert(strcmp(contents, "12345\trating\tgood\t7\t3\t1\n") == 0);
	assert(access(TEST_LOG_TEMP_PATH, F_OK) != 0);
	assert(access(TEST_LOG_BACKUP_PATH, F_OK) != 0);
	remove_logs();
}

static void test_append_rejects_rating_on_non_rating_event(void)
{
	struct study_review_log_entry entry =
		sample_entry(STUDY_REVIEW_LOG_EVENT_UNDO, STUDY_REVIEW_LOG_RATING_GOOD);

	remove_logs();
	assert(!study_review_log_append(TEST_LOG_PATH, &entry));
	assert(access(TEST_LOG_PATH, F_OK) != 0);
	remove_logs();
}

static void test_append_repairs_partial_final_row(void)
{
	char contents[256];
	struct study_review_log_entry entry =
		sample_entry(STUDY_REVIEW_LOG_EVENT_SUSPEND, STUDY_REVIEW_LOG_RATING_NONE);

	remove_logs();
	write_file(TEST_LOG_PATH, VALID_RATING_ROW "partial row");
	assert(study_review_log_append(TEST_LOG_PATH, &entry));
	read_file(contents, sizeof(contents));

	assert(strcmp(contents, VALID_RATING_ROW "12345\tsuspend\t-\t7\t3\t1\n") == 0);
	remove_logs();
}

static void test_append_recovers_pending_temp(void)
{
	char contents[256];
	struct study_review_log_entry entry =
		sample_entry(STUDY_REVIEW_LOG_EVENT_RESTORE, STUDY_REVIEW_LOG_RATING_NONE);

	remove_logs();
	write_file(TEST_LOG_TEMP_PATH, VALID_PENDING_ROW);
	write_file(TEST_LOG_BACKUP_PATH, VALID_BACKUP_ROW "partial");
	assert(study_review_log_append(TEST_LOG_PATH, &entry));
	read_file(contents, sizeof(contents));

	assert(strcmp(contents, VALID_PENDING_ROW "12345\trestore\t-\t7\t3\t1\n") == 0);
	assert(access(TEST_LOG_TEMP_PATH, F_OK) != 0);
	assert(access(TEST_LOG_BACKUP_PATH, F_OK) != 0);
	remove_logs();
}

static void test_append_discards_malformed_primary_when_no_recovery_exists(void)
{
	char contents[256];
	struct study_review_log_entry entry =
		sample_entry(STUDY_REVIEW_LOG_EVENT_RATING, STUDY_REVIEW_LOG_RATING_HARD);

	remove_logs();
	write_file(TEST_LOG_PATH, "bad complete row\n");
	assert(study_review_log_append(TEST_LOG_PATH, &entry));
	read_file(contents, sizeof(contents));

	assert(strcmp(contents, "12345\trating\thard\t7\t3\t1\n") == 0);
	assert(access(TEST_LOG_TEMP_PATH, F_OK) != 0);
	assert(access(TEST_LOG_BACKUP_PATH, F_OK) != 0);
	remove_logs();
}

static void test_append_recovers_backup_when_primary_is_malformed(void)
{
	char contents[256];
	struct study_review_log_entry entry =
		sample_entry(STUDY_REVIEW_LOG_EVENT_UNDO, STUDY_REVIEW_LOG_RATING_NONE);

	remove_logs();
	write_file(TEST_LOG_PATH, "bad complete row\n");
	write_file(TEST_LOG_BACKUP_PATH, VALID_BACKUP_ROW);
	assert(study_review_log_append(TEST_LOG_PATH, &entry));
	read_file(contents, sizeof(contents));

	assert(strcmp(contents, VALID_BACKUP_ROW "12345\tundo\t-\t7\t3\t1\n") == 0);
	assert(access(TEST_LOG_TEMP_PATH, F_OK) != 0);
	assert(access(TEST_LOG_BACKUP_PATH, F_OK) != 0);
	remove_logs();
}

static void test_append_rejects_full_log(void)
{
	struct study_review_log_entry entry =
		sample_entry(STUDY_REVIEW_LOG_EVENT_RATING, STUDY_REVIEW_LOG_RATING_EASY);
	long original_size;

	remove_logs();
	write_nearly_full_valid_log(TEST_LOG_PATH);
	original_size = file_size(TEST_LOG_PATH);
	assert(!study_review_log_append(TEST_LOG_PATH, &entry));
	assert(file_size(TEST_LOG_PATH) == original_size);
	remove_logs();
}

static void test_delete_removes_log_artifacts(void)
{
	remove_logs();
	write_file(TEST_LOG_PATH, "row\n");
	write_file(TEST_LOG_TEMP_PATH, "temp\n");
	write_file(TEST_LOG_BACKUP_PATH, "backup\n");

	assert(study_review_log_delete(TEST_LOG_PATH));
	assert(access(TEST_LOG_PATH, F_OK) != 0);
	assert(access(TEST_LOG_TEMP_PATH, F_OK) != 0);
	assert(access(TEST_LOG_BACKUP_PATH, F_OK) != 0);
	assert(study_review_log_delete(TEST_LOG_PATH));
	assert(!study_review_log_delete(NULL));
}

int main(void)
{
	test_append_writes_rating_row();
	test_append_rejects_rating_on_non_rating_event();
	test_append_repairs_partial_final_row();
	test_append_recovers_pending_temp();
	test_append_discards_malformed_primary_when_no_recovery_exists();
	test_append_recovers_backup_when_primary_is_malformed();
	test_append_rejects_full_log();
	test_delete_removes_log_artifacts();
	return 0;
}
