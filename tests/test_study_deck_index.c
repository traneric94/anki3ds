#include "study_deck_index.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_ROOT "/tmp/anki3ds-study-deck-index-test"

static void write_file(const char *path, const char *contents)
{
	FILE *file = fopen(path, "w");

	assert(file != NULL);
	assert(fputs(contents, file) >= 0);
	assert(fclose(file) == 0);
}

static void remove_test_tree(void)
{
	(void)remove(TEST_ROOT "/Alpha/cards.tsv");
	(void)remove(TEST_ROOT "/Alpha/deck.json");
	(void)remove(TEST_ROOT "/Alpha/state.tsv");
	(void)remove(TEST_ROOT "/Alpha/state.tsv.tmp");
	(void)remove(TEST_ROOT "/Alpha/state.tsv.bak");
	(void)remove(TEST_ROOT "/beta/cards.tsv");
	(void)remove(TEST_ROOT "/bad name/cards.tsv");
	(void)remove(TEST_ROOT "/missing/deck.json");
	(void)rmdir(TEST_ROOT "/Alpha");
	(void)rmdir(TEST_ROOT "/beta");
	(void)rmdir(TEST_ROOT "/bad name");
	(void)rmdir(TEST_ROOT "/missing");
	(void)rmdir(TEST_ROOT);
}

static void make_test_tree(void)
{
	remove_test_tree();
	assert(mkdir(TEST_ROOT, 0700) == 0);
	assert(mkdir(TEST_ROOT "/beta", 0700) == 0);
	assert(mkdir(TEST_ROOT "/Alpha", 0700) == 0);
	assert(mkdir(TEST_ROOT "/bad name", 0700) == 0);
	assert(mkdir(TEST_ROOT "/missing", 0700) == 0);
	write_file(TEST_ROOT "/beta/cards.tsv", "id\tnote\tfront\tback\ttag\n");
	write_file(
		TEST_ROOT "/Alpha/cards.tsv",
		"id-1\tnote\tfront 1\tback 1\ttag\n"
		"id-2\tnote\tfront 2\tback 2\ttag\n"
	);
	write_file(TEST_ROOT "/Alpha/deck.json", "{ \"name\": \"Daily Limits\" }\n");
	write_file(
		TEST_ROOT "/Alpha/state.tsv",
		"version\t1\n"
		"card_count\t2\n"
		"current_index\t1\n"
		"progress_day\t10\n"
		"reviewed_count\t1\n"
		"reviewed_today_count\t1\n"
		"introduced_count\t1\n"
		"introduced_today_count\t1\n"
		"completed_today_count\t1\n"
		"again_count\t0\n"
		"hard_count\t0\n"
		"good_count\t1\n"
		"easy_count\t0\n"
		"suspended_count\t0\n"
		"introduced_index\t0\n"
		"completed_today_index\t0\n"
	);
	write_file(TEST_ROOT "/bad name/cards.tsv", "id\tnote\tfront\tback\ttag\n");
	write_file(TEST_ROOT "/missing/deck.json", "{ \"name\": \"No Cards\" }\n");
}

static void test_build_entry_rejects_bad_ids(void)
{
	struct study_deck_entry entry;

	assert(
		study_deck_index_build_entry(&entry, TEST_ROOT, "sample_deck-1")
	);
	assert(strcmp(entry.id, "sample_deck-1") == 0);
	assert(
		strcmp(
			entry.cards_path,
			TEST_ROOT "/sample_deck-1/cards.tsv"
		) == 0
	);
	assert(
		strcmp(
			entry.state_path,
			TEST_ROOT "/sample_deck-1/state.tsv"
		) == 0
	);
	assert(
		strcmp(
			entry.settings_path,
			TEST_ROOT "/sample_deck-1/settings.tsv"
		) == 0
	);
	assert(
		strcmp(
			entry.review_log_path,
			TEST_ROOT "/sample_deck-1/review-log.tsv"
		) == 0
	);
	assert(!study_deck_index_build_entry(&entry, TEST_ROOT, "bad name"));
	assert(!study_deck_index_build_entry(&entry, TEST_ROOT, ".hidden"));
	assert(!study_deck_index_build_entry(&entry, TEST_ROOT, ""));
}

static void test_scan_keeps_sorted_valid_decks(void)
{
	struct study_deck_index index;
	size_t found_index = 99;

	make_test_tree();
	study_deck_index_scan(&index, TEST_ROOT);

	assert(index.count == 2);
	assert(index.total_count == 2);
	assert(index.ignored_count == 2);
	assert(!index.overflowed);
	assert(strcmp(index.entries[0].id, "Alpha") == 0);
	assert(strcmp(index.entries[0].display_name, "Daily Limits") == 0);
	assert(index.entries[0].stats_loaded);
	assert(!index.entries[0].state_bad);
	assert(index.entries[0].card_count == 2);
	assert(index.entries[0].due_count == 0);
	assert(index.entries[0].new_count == 1);
	assert(index.entries[0].suspended_count == 0);
	assert(strcmp(index.entries[1].id, "beta") == 0);
	assert(strcmp(index.entries[1].display_name, "beta") == 0);
	assert(index.entries[1].stats_loaded);
	assert(index.entries[1].card_count == 1);
	assert(index.entries[1].due_count == 0);
	assert(index.entries[1].new_count == 1);
	assert(study_deck_index_get(&index, 2) == NULL);
	assert(study_deck_index_find(&index, "beta", &found_index));
	assert(found_index == 1);
	assert(!study_deck_index_find(&index, "missing", NULL));

	remove_test_tree();
}

static void test_scan_for_day_reopens_yesterday_completed_cards(void)
{
	struct study_deck_index index;

	make_test_tree();
	study_deck_index_scan_for_day(&index, TEST_ROOT, 11);

	assert(index.count == 2);
	assert(strcmp(index.entries[0].id, "Alpha") == 0);
	assert(index.entries[0].stats_loaded);
	assert(!index.entries[0].state_bad);
	assert(index.entries[0].card_count == 2);
	assert(index.entries[0].due_count == 1);
	assert(index.entries[0].new_count == 1);

	remove_test_tree();
}

static void test_scan_for_day_respects_future_schedule(void)
{
	struct study_deck_index index;

	make_test_tree();
	write_file(
		TEST_ROOT "/Alpha/state.tsv",
		"version\t2\n"
		"card_count\t2\n"
		"current_index\t1\n"
		"progress_day\t10\n"
		"reviewed_count\t1\n"
		"reviewed_today_count\t1\n"
		"introduced_count\t1\n"
		"introduced_today_count\t1\n"
		"completed_today_count\t1\n"
		"again_count\t0\n"
		"hard_count\t0\n"
		"good_count\t1\n"
		"easy_count\t0\n"
		"suspended_count\t0\n"
		"introduced_index\t0\n"
		"schedule_index\t0\n"
		"schedule_due_day\t12\n"
		"schedule_interval_days\t2\n"
		"completed_today_index\t0\n"
	);

	study_deck_index_scan_for_day(&index, TEST_ROOT, 11);
	assert(index.count == 2);
	assert(strcmp(index.entries[0].id, "Alpha") == 0);
	assert(index.entries[0].stats_loaded);
	assert(!index.entries[0].state_bad);
	assert(index.entries[0].card_count == 2);
	assert(index.entries[0].due_count == 0);
	assert(index.entries[0].new_count == 1);

	study_deck_index_scan_for_day(&index, TEST_ROOT, 12);
	assert(index.count == 2);
	assert(strcmp(index.entries[0].id, "Alpha") == 0);
	assert(index.entries[0].stats_loaded);
	assert(!index.entries[0].state_bad);
	assert(index.entries[0].due_count == 1);
	assert(index.entries[0].new_count == 1);

	remove_test_tree();
}

static void test_refresh_entry_for_day_reloads_active_stats(void)
{
	struct study_deck_index index;

	make_test_tree();
	study_deck_index_scan_for_day(&index, TEST_ROOT, 10);
	assert(index.count == 2);
	assert(strcmp(index.entries[0].id, "Alpha") == 0);
	assert(index.entries[0].due_count == 0);
	assert(index.entries[0].new_count == 1);

	write_file(
		TEST_ROOT "/Alpha/state.tsv",
		"version\t1\n"
		"card_count\t2\n"
		"current_index\t0\n"
		"progress_day\t10\n"
		"reviewed_count\t0\n"
		"reviewed_today_count\t0\n"
		"introduced_count\t1\n"
		"introduced_today_count\t1\n"
		"completed_today_count\t0\n"
		"again_count\t0\n"
		"hard_count\t0\n"
		"good_count\t0\n"
		"easy_count\t0\n"
		"suspended_count\t0\n"
		"introduced_index\t0\n"
	);
	assert(study_deck_index_refresh_entry_for_day(&index, "Alpha", 10));
	assert(index.entries[0].stats_loaded);
	assert(!index.entries[0].state_bad);
	assert(index.entries[0].card_count == 2);
	assert(index.entries[0].due_count == 1);
	assert(index.entries[0].new_count == 1);
	assert(index.entries[0].suspended_count == 0);

	write_file(
		TEST_ROOT "/Alpha/state.tsv",
		"version\t1\n"
		"card_count\t2\n"
		"current_index\t1\n"
		"progress_day\t10\n"
		"reviewed_count\t1\n"
		"reviewed_today_count\t1\n"
		"introduced_count\t1\n"
		"introduced_today_count\t1\n"
		"completed_today_count\t1\n"
		"again_count\t0\n"
		"hard_count\t0\n"
		"good_count\t1\n"
		"easy_count\t0\n"
		"suspended_count\t0\n"
		"introduced_index\t0\n"
		"completed_today_index\t0\n"
	);
	assert(study_deck_index_refresh_entry_for_day(&index, "Alpha", 10));
	assert(index.entries[0].stats_loaded);
	assert(!index.entries[0].state_bad);
	assert(index.entries[0].due_count == 0);
	assert(index.entries[0].new_count == 1);
	assert(index.entries[0].suspended_count == 0);
	assert(!study_deck_index_refresh_entry_for_day(&index, "missing", 10));

	remove_test_tree();
}

static void test_missing_root_scans_empty(void)
{
	struct study_deck_index index;

	remove_test_tree();
	study_deck_index_scan(&index, TEST_ROOT);
	assert(index.count == 0);
	assert(index.total_count == 0);
	assert(index.ignored_count == 0);
}

int main(void)
{
	test_build_entry_rejects_bad_ids();
	test_scan_keeps_sorted_valid_decks();
	test_scan_for_day_reopens_yesterday_completed_cards();
	test_scan_for_day_respects_future_schedule();
	test_refresh_entry_for_day_reloads_active_stats();
	test_missing_root_scans_empty();
	return 0;
}
