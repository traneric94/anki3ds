#ifndef ANKI3DS_STUDY_SETTINGS_H
#define ANKI3DS_STUDY_SETTINGS_H

#include <stdbool.h>

#define STUDY_SETTINGS_DEFAULT_NEW_LIMIT 20u
#define STUDY_SETTINGS_DEFAULT_REVIEW_LIMIT 200u
#define STUDY_SETTINGS_MAX_LIMIT 1000000u

enum study_settings_learning_mode
{
	STUDY_SETTINGS_LEARNING_DUE_FIRST,
	STUDY_SETTINGS_LEARNING_CARD_COOLDOWN,
};

#define STUDY_SETTINGS_DEFAULT_LEARNING_MODE \
	STUDY_SETTINGS_LEARNING_DUE_FIRST

enum study_settings_result
{
	STUDY_SETTINGS_OK,
	STUDY_SETTINGS_NOT_FOUND,
	STUDY_SETTINGS_BAD_FORMAT,
	STUDY_SETTINGS_WRITE_FAILED,
};

struct study_settings
{
	unsigned int new_limit;
	unsigned int review_limit;
	enum study_settings_learning_mode learning_mode;
};

void study_settings_defaults(struct study_settings *settings);
unsigned int study_settings_next_limit_preset(
	unsigned int current,
	bool increase
);
enum study_settings_learning_mode study_settings_next_learning_mode(
	enum study_settings_learning_mode current
);
bool study_settings_learning_mode_is_valid(
	enum study_settings_learning_mode mode
);
const char *study_settings_learning_mode_label(
	enum study_settings_learning_mode mode
);
bool study_settings_new_limit_blocks_reveal(
	unsigned int new_limit,
	unsigned int introduced_count,
	bool current_card_introduced
);
bool study_settings_review_limit_blocks_rating(
	unsigned int review_limit,
	unsigned int reviewed_count
);
enum study_settings_result study_settings_load_tsv(
	struct study_settings *settings,
	const char *path
);
enum study_settings_result study_settings_save_tsv(
	const struct study_settings *settings,
	const char *path
);
const char *study_settings_result_name(enum study_settings_result result);

#endif
