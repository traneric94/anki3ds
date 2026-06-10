#include "app_deck_select_contract.h"

#include "app_deck_navigation.h"

#include <stdio.h>
#include <string.h>

#define APP_DECK_SELECT_VISIBLE_ROWS 6

static void app_deck_select_copy_string(
	char *destination,
	size_t destination_size,
	const char *source
)
{
	if (destination == NULL || destination_size == 0)
		return;

	snprintf(destination, destination_size, "%s", source != NULL ? source : "");
}

static size_t app_deck_select_list_start(
	const struct study_deck_index *index,
	size_t selected_deck_index
)
{
	return app_deck_navigation_visible_start(
		index != NULL ? index->count : 0,
		selected_deck_index,
		APP_DECK_SELECT_VISIBLE_ROWS
	);
}

static void app_deck_select_build_entry_summary(
	const struct study_deck_entry *entry,
	char *destination,
	size_t destination_size
)
{
	if (destination == NULL || destination_size == 0)
		return;

	destination[0] = '\0';
	if (entry == NULL)
		return;
	if (entry->state_bad)
	{
		snprintf(destination, destination_size, "  [state?]");
		return;
	}
	if (!entry->stats_loaded)
		return;

	if (entry->suspended_count > 0)
	{
		snprintf(
			destination,
			destination_size,
			"  [Due %u | New %u | Susp %u]",
			entry->due_count,
			entry->new_count,
			entry->suspended_count
		);
		return;
	}

	snprintf(
		destination,
		destination_size,
		"  [Due %u | New %u]",
		entry->due_count,
		entry->new_count
	);
}

static void app_deck_select_build_list_text(
	const struct study_deck_index *index,
	size_t selected_deck_index,
	char *destination,
	size_t destination_size
)
{
	size_t used = 0;
	size_t start;
	size_t end;
	size_t safe_selected_index = selected_deck_index;

	if (destination == NULL || destination_size == 0)
		return;

	destination[0] = '\0';
	if (index == NULL || index->count == 0)
	{
		snprintf(
			destination,
			destination_size,
			"No decks found.\n"
			"Add folders under:\n"
			"sdmc:/3ds/anki3ds/decks"
		);
		return;
	}

	if (safe_selected_index >= index->count)
		safe_selected_index = index->count - 1;
	start = app_deck_select_list_start(index, selected_deck_index);
	end = start + APP_DECK_SELECT_VISIBLE_ROWS;
	if (end > index->count)
		end = index->count;

	for (size_t entry_index = start; entry_index < end; entry_index++)
	{
		const struct study_deck_entry *entry =
			study_deck_index_get(index, entry_index);
		char summary[64];
		int written;

		if (entry == NULL || used >= destination_size)
			break;

		app_deck_select_build_entry_summary(entry, summary, sizeof(summary));
		written = snprintf(
			destination + used,
			destination_size - used,
			"%c %s%s\n",
			entry_index == safe_selected_index ? '>' : ' ',
			entry->display_name,
			summary
		);
		if (written < 0)
			break;
		if ((size_t)written >= destination_size - used)
		{
			destination[destination_size - 1] = '\0';
			break;
		}
		used += (size_t)written;
	}
}

static void app_deck_select_build_meta_text(
	const struct study_deck_index *index,
	size_t selected_deck_index,
	char *destination,
	size_t destination_size
)
{
	size_t safe_selected_index = selected_deck_index;

	if (destination == NULL || destination_size == 0)
		return;

	if (index == NULL || index->count == 0)
	{
		snprintf(destination, destination_size, "Decks 0");
		return;
	}

	if (safe_selected_index >= index->count)
		safe_selected_index = index->count - 1;
	snprintf(
		destination,
		destination_size,
		"Deck %lu/%lu  Ignored %lu%s",
		(unsigned long)(safe_selected_index + 1),
		(unsigned long)index->count,
		(unsigned long)index->ignored_count,
		index->overflowed ? "  More on SD" : ""
	);
}

static void app_deck_select_build_footer_text(
	const struct study_deck_index *index,
	char *destination,
	size_t destination_size
)
{
	unsigned long due_total = 0;
	unsigned long new_total = 0;
	unsigned long held_total = 0;
	unsigned long issue_count = 0;

	if (destination == NULL || destination_size == 0)
		return;

	if (index == NULL || index->count == 0)
	{
		snprintf(destination, destination_size, "All decks: none");
		return;
	}

	for (size_t entry_index = 0; entry_index < index->count; entry_index++)
	{
		const struct study_deck_entry *entry =
			study_deck_index_get(index, entry_index);

		if (entry == NULL || entry->state_bad || !entry->stats_loaded)
		{
			issue_count++;
			continue;
		}

		due_total += entry->due_count;
		new_total += entry->new_count;
		held_total += entry->suspended_count;
	}

	if (issue_count > 0)
	{
		snprintf(
			destination,
			destination_size,
			"All decks: Due %lu | New %lu | Susp %lu | Issues %lu",
			due_total,
			new_total,
			held_total,
			issue_count
		);
		return;
	}

	snprintf(
		destination,
		destination_size,
		"All decks: Due %lu | New %lu | Susp %lu",
		due_total,
		new_total,
		held_total
	);
}

void app_deck_select_contract_build(
	struct app_deck_select_contract *contract,
	const struct study_deck_index *index,
	size_t selected_deck_index,
	const char *status_text,
	bool help_visible
)
{
	if (contract == NULL)
		return;

	memset(contract, 0, sizeof(*contract));
	app_deck_select_copy_string(
		contract->title_text,
		sizeof(contract->title_text),
		"Select deck"
	);
	app_deck_select_build_list_text(
		index,
		selected_deck_index,
		contract->list_text,
		sizeof(contract->list_text)
	);
	app_deck_select_build_meta_text(
		index,
		selected_deck_index,
		contract->meta_text,
		sizeof(contract->meta_text)
	);
	app_deck_select_copy_string(
		contract->status_text,
		sizeof(contract->status_text),
		status_text
	);
	app_deck_select_copy_string(
		contract->controls_text,
		sizeof(contract->controls_text),
		help_visible ?
			"A: open   START: hide help\n"
			"↑/↓: deck   ←/→: page\n"
			"L/R: page decks\n"
			"SELECT: rescan   Y: exit" :
			"START: help"
	);
	app_deck_select_copy_string(
		contract->footer_text,
		sizeof(contract->footer_text),
		""
	);
	app_deck_select_build_footer_text(
		index,
		contract->footer_text,
		sizeof(contract->footer_text)
	);
}

void app_deck_select_contract_build_scan_status(
	const struct study_deck_index *index,
	char *destination,
	size_t destination_size
)
{
	if (destination == NULL || destination_size == 0)
		return;

	if (index == NULL || index->count == 0)
	{
		snprintf(destination, destination_size, "No decks found");
		return;
	}

	snprintf(
		destination,
		destination_size,
		"Found %lu deck%s",
		(unsigned long)index->count,
		index->count == 1 ? "" : "s"
	);
}
