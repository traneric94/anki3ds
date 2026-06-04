#ifndef ANKI3DS_REVIEW_STATE_H
#define ANKI3DS_REVIEW_STATE_H

#include "deck.h"
#include "scheduler.h"

enum review_state_load_result
{
	REVIEW_STATE_LOAD_OK,
	REVIEW_STATE_LOAD_NOT_FOUND,
	REVIEW_STATE_LOAD_UNMATCHED,
	REVIEW_STATE_LOAD_BAD_FORMAT,
};

enum review_state_parse_result
{
	REVIEW_STATE_PARSE_OK,
	REVIEW_STATE_PARSE_LINE_TOO_LONG,
	REVIEW_STATE_PARSE_BAD_METADATA,
	REVIEW_STATE_PARSE_METADATA_AFTER_ROWS,
	REVIEW_STATE_PARSE_DATA_AFTER_FOOTER,
	REVIEW_STATE_PARSE_BAD_ROW,
	REVIEW_STATE_PARSE_DUPLICATE_CARD,
	REVIEW_STATE_PARSE_BAD_SCHEDULER_STATE,
	REVIEW_STATE_PARSE_READ_ERROR,
	REVIEW_STATE_PARSE_ROW_COUNT_MISMATCH,
	REVIEW_STATE_PARSE_EMPTY,
	REVIEW_STATE_PARSE_RECOVERY_ERROR,
};

enum review_state_save_result
{
	REVIEW_STATE_SAVE_OK,
	REVIEW_STATE_SAVE_FAILED,
};

struct review_state_load_report
{
	unsigned int line_number;
	enum review_state_parse_result parse_result;
};

void review_state_load_report_clear(struct review_state_load_report *report);
enum review_state_load_result review_state_load(
	const struct deck *deck,
	struct scheduler_session *session,
	const char *path
);
enum review_state_load_result review_state_load_with_report(
	const struct deck *deck,
	struct scheduler_session *session,
	const char *path,
	struct review_state_load_report *report
);
enum review_state_save_result review_state_save(
	const struct deck *deck,
	const struct scheduler_session *session,
	const char *path
);
bool review_state_delete(const char *path);
const char *review_state_parse_result_name(enum review_state_parse_result result);
const char *review_state_load_result_name(enum review_state_load_result result);
const char *review_state_save_result_name(enum review_state_save_result result);
bool review_state_load_result_allows_save(enum review_state_load_result result);

#endif
