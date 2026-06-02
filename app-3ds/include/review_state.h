#ifndef ANKI3DS_REVIEW_STATE_H
#define ANKI3DS_REVIEW_STATE_H

#include "deck.h"
#include "scheduler.h"

enum review_state_load_result
{
	REVIEW_STATE_LOAD_OK,
	REVIEW_STATE_LOAD_NOT_FOUND,
	REVIEW_STATE_LOAD_BAD_FORMAT,
};

enum review_state_save_result
{
	REVIEW_STATE_SAVE_OK,
	REVIEW_STATE_SAVE_FAILED,
};

enum review_state_load_result review_state_load(
	const struct deck *deck,
	struct scheduler_session *session,
	const char *path
);
enum review_state_save_result review_state_save(
	const struct deck *deck,
	const struct scheduler_session *session,
	const char *path
);
bool review_state_delete(const char *path);
const char *review_state_load_result_name(enum review_state_load_result result);
const char *review_state_save_result_name(enum review_state_save_result result);

#endif
