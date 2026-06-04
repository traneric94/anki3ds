#include "app_diagnostics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void require_true(int condition, const char *message)
{
	if (!condition)
	{
		fprintf(stderr, "%s\n", message);
		exit(1);
	}
}

static void build_path(char *path, size_t path_size, const char *suffix)
{
	int written = snprintf(
		path,
		path_size,
		"/tmp/anki3ds-diagnostics-%ld%s",
		(long)getpid(),
		suffix
	);

	require_true(written >= 0 && (size_t)written < path_size, "path overflow");
}

static void remove_session_files(const char *path)
{
	char temp_path[256];
	char backup_path[256];

	snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);
	snprintf(backup_path, sizeof(backup_path), "%s.bak", path);
	remove(temp_path);
	remove(backup_path);
	remove(path);
}

static void read_file(char *buffer, size_t buffer_size, const char *path)
{
	FILE *file = fopen(path, "rb");
	size_t bytes_read;

	require_true(file != NULL, "could not open diagnostics file");
	bytes_read = fread(buffer, 1, buffer_size - 1, file);
	require_true(ferror(file) == 0, "could not read diagnostics file");
	require_true(fclose(file) == 0, "could not close diagnostics file");
	buffer[bytes_read] = '\0';
}

static void require_contains(const char *text, const char *needle)
{
	if (strstr(text, needle) == NULL)
	{
		fprintf(stderr, "missing diagnostics row: %s\n", needle);
		exit(1);
	}
}

int main(void)
{
	char path[256];
	char text[4096];
	struct app_diagnostics diagnostics;

	build_path(path, sizeof(path), ".tsv");
	remove_session_files(path);

	app_diagnostics_init(&diagnostics, 20000, 1800000000);
	app_diagnostics_mark_scan(&diagnostics, 20000, 2, 1);
	app_diagnostics_mark_deck_open(&diagnostics, "sample", true, true, false);
	app_diagnostics_mark_answer_shown(&diagnostics);
	app_diagnostics_mark_rating_saved(&diagnostics);
	app_diagnostics_mark_undo_saved(&diagnostics);
	app_diagnostics_mark_suspend_saved(&diagnostics);
	app_diagnostics_mark_restore_saved(&diagnostics, 3);
	app_diagnostics_mark_settings_saved(&diagnostics);
	app_diagnostics_mark_reset_progress(&diagnostics);
	app_diagnostics_mark_exit_confirmed(&diagnostics);

	require_true(
		app_diagnostics_write(path, &diagnostics),
		"diagnostics write failed"
	);
	read_file(text, sizeof(text), path);

	require_contains(text, "#anki3ds-session-v1\n");
	require_contains(text, "started_at\t1800000000\n");
	require_contains(text, "launch_count\t1\n");
	require_contains(text, "started_day\t20000\n");
	require_contains(text, "scan_completed\t1\n");
	require_contains(text, "deck_count\t2\n");
	require_contains(text, "ignored_count\t1\n");
	require_contains(text, "deck_open_count\t1\n");
	require_contains(text, "review_screen_count\t1\n");
	require_contains(text, "answer_shown_count\t1\n");
	require_contains(text, "rating_saved_count\t1\n");
	require_contains(text, "undo_saved_count\t1\n");
	require_contains(text, "suspend_saved_count\t1\n");
	require_contains(text, "restore_saved_count\t3\n");
	require_contains(text, "settings_saved_count\t1\n");
	require_contains(text, "reset_progress_count\t1\n");
	require_contains(text, "exit_confirmed\t1\n");
	require_contains(text, "last_deck_id\tsample\n");
	require_contains(text, "last_event\texit_confirmed\n");
	require_contains(text, "#anki3ds-session-complete\n");

	require_true(
		app_diagnostics_load(path, &diagnostics),
		"diagnostics load failed"
	);
	app_diagnostics_mark_launch(&diagnostics, 20001, 1800000100);
	require_true(
		app_diagnostics_write(path, &diagnostics),
		"relaunch diagnostics write failed"
	);
	read_file(text, sizeof(text), path);

	require_contains(text, "started_at\t1800000000\n");
	require_contains(text, "updated_at\t1800000100\n");
	require_contains(text, "launch_count\t2\n");
	require_contains(text, "current_day\t20001\n");
	require_contains(text, "rating_saved_count\t1\n");
	require_contains(text, "undo_saved_count\t1\n");
	require_contains(text, "suspend_saved_count\t1\n");
	require_contains(text, "restore_saved_count\t3\n");
	require_contains(text, "settings_saved_count\t1\n");
	require_contains(text, "reset_progress_count\t1\n");
	require_contains(text, "exit_confirmed\t1\n");
	require_contains(text, "last_event\tboot\n");

	remove_session_files(path);
	return 0;
}
