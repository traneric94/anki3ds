#include "app_deck_select_contract.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void set_deck_name(
	struct study_deck_index *index,
	size_t entry_index,
	const char *id,
	const char *display_name
)
{
	snprintf(
		index->entries[entry_index].id,
		sizeof(index->entries[entry_index].id),
		"%s",
		id
	);
	snprintf(
		index->entries[entry_index].display_name,
		sizeof(index->entries[entry_index].display_name),
		"%s",
		display_name
	);
}

static void set_deck_stats(
	struct study_deck_index *index,
	size_t entry_index,
	unsigned int due_count,
	unsigned int new_count,
	unsigned int suspended_count
)
{
	index->entries[entry_index].stats_loaded = true;
	index->entries[entry_index].due_count = due_count;
	index->entries[entry_index].new_count = new_count;
	index->entries[entry_index].suspended_count = suspended_count;
}

static void test_empty_deck_selector_strings(void)
{
	struct study_deck_index index;
	struct app_deck_select_contract contract;
	char scan_status[APP_DECK_SELECT_STATUS_TEXT_SIZE];

	study_deck_index_init(&index);
	app_deck_select_contract_build(&contract, &index, 0, "Scanning decks", false);
	app_deck_select_contract_build_scan_status(
		&index,
		scan_status,
		sizeof(scan_status)
	);

	assert(strcmp(contract.title_text, "Select deck") == 0);
	assert(strstr(contract.list_text, "No decks found.") != NULL);
	assert(strstr(contract.list_text, STUDY_DECK_INDEX_ROOT_PATH) != NULL);
	assert(strcmp(contract.meta_text, "Decks 0") == 0);
	assert(strcmp(contract.status_text, "Scanning decks") == 0);
	assert(strcmp(contract.controls_text, "START: help") == 0);
	assert(strcmp(contract.footer_text, "All decks: none") == 0);
	assert(strcmp(scan_status, "No decks found") == 0);
}

static void test_deck_selector_window_and_meta(void)
{
	struct study_deck_index index;
	struct app_deck_select_contract contract;

	study_deck_index_init(&index);
	index.count = 8;
	index.total_count = 8;
	index.ignored_count = 2;
	index.overflowed = true;
	for (size_t entry_index = 0; entry_index < index.count; entry_index++)
	{
		char id[16];
		char name[32];

		snprintf(id, sizeof(id), "deck-%lu", (unsigned long)entry_index);
		snprintf(name, sizeof(name), "Deck %lu", (unsigned long)entry_index);
		set_deck_name(&index, entry_index, id, name);
	}

	app_deck_select_contract_build(&contract, &index, 6, "Found 8 decks", false);

	assert(strcmp(contract.title_text, "Select deck") == 0);
	assert(strstr(contract.list_text, "  Deck 2\n") != NULL);
	assert(strstr(contract.list_text, "> Deck 6\n") != NULL);
	assert(strstr(contract.list_text, "  Deck 7\n") != NULL);
	assert(strstr(contract.list_text, "Deck 0") == NULL);
	assert(strstr(contract.meta_text, "Deck 7/8") != NULL);
	assert(strstr(contract.meta_text, "Ignored 2") != NULL);
	assert(strstr(contract.meta_text, "More on SD") != NULL);
	assert(strcmp(contract.status_text, "Found 8 decks") == 0);
}

static void test_deck_selector_scan_status_pluralizes(void)
{
	struct study_deck_index index;
	char status[APP_DECK_SELECT_STATUS_TEXT_SIZE];

	study_deck_index_init(&index);
	index.count = 1;
	app_deck_select_contract_build_scan_status(&index, status, sizeof(status));
	assert(strcmp(status, "Found 1 deck") == 0);

	index.count = 2;
	app_deck_select_contract_build_scan_status(&index, status, sizeof(status));
	assert(strcmp(status, "Found 2 decks") == 0);
}

static void test_out_of_range_selection_is_clamped_for_meta(void)
{
	struct study_deck_index index;
	struct app_deck_select_contract contract;

	study_deck_index_init(&index);
	index.count = 2;
	set_deck_name(&index, 0, "alpha", "Alpha");
	set_deck_name(&index, 1, "beta", "Beta");

	app_deck_select_contract_build(&contract, &index, 99, "Found 2 decks", false);

	assert(strstr(contract.meta_text, "Deck 2/2") != NULL);
	assert(strstr(contract.list_text, "Alpha") != NULL);
	assert(strstr(contract.list_text, "> Beta") != NULL);
}

static void test_help_visible_expands_deck_selector_legend(void)
{
	struct study_deck_index index;
	struct app_deck_select_contract contract;

	study_deck_index_init(&index);
	index.count = 1;
	set_deck_name(&index, 0, "alpha", "Alpha");

	app_deck_select_contract_build(&contract, &index, 0, "Found 1 deck", true);

	assert(strstr(contract.controls_text, "A: open") != NULL);
	assert(strstr(contract.controls_text, "START: hide help") != NULL);
	assert(strstr(contract.controls_text, "↑/↓: deck") != NULL);
	assert(strstr(contract.controls_text, "←/→: page") != NULL);
	assert(strstr(contract.controls_text, "L/R: page decks") != NULL);
	assert(strstr(contract.controls_text, "SELECT: rescan") != NULL);
	assert(strstr(contract.controls_text, "Y: exit") != NULL);
	assert(strstr(contract.footer_text, "All decks:") != NULL);
	assert(strstr(contract.footer_text, "Issues 1") != NULL);
}

static void test_deck_selector_lists_daily_counts_when_available(void)
{
	struct study_deck_index index;
	struct app_deck_select_contract contract;

	study_deck_index_init(&index);
	index.count = 2;
	set_deck_name(&index, 0, "alpha", "Alpha");
	set_deck_name(&index, 1, "beta", "Beta");
	set_deck_stats(&index, 0, 1, 2, 0);
	set_deck_stats(&index, 1, 0, 1, 1);

	app_deck_select_contract_build(&contract, &index, 0, "Found 2 decks", false);

	assert(strstr(contract.list_text, "> Alpha  [Due 1 | New 2]\n") != NULL);
	assert(strstr(contract.list_text, "  Beta  [Due 0 | New 1 | Susp 1]\n") != NULL);
	assert(strcmp(contract.footer_text, "All decks: Due 1 | New 3 | Susp 1") == 0);
}

static void test_deck_selector_marks_bad_state_summary(void)
{
	struct study_deck_index index;
	struct app_deck_select_contract contract;

	study_deck_index_init(&index);
	index.count = 1;
	set_deck_name(&index, 0, "alpha", "Alpha");
	index.entries[0].state_bad = true;

	app_deck_select_contract_build(&contract, &index, 0, "Found 1 deck", false);

	assert(strstr(contract.list_text, "> Alpha  [state?]\n") != NULL);
	assert(strcmp(
		contract.footer_text,
		"All decks: Due 0 | New 0 | Susp 0 | Issues 1"
	) == 0);
}

int main(void)
{
	test_empty_deck_selector_strings();
	test_deck_selector_window_and_meta();
	test_deck_selector_scan_status_pluralizes();
	test_out_of_range_selection_is_clamped_for_meta();
	test_help_visible_expands_deck_selector_legend();
	test_deck_selector_lists_daily_counts_when_available();
	test_deck_selector_marks_bad_state_summary();
	return 0;
}
