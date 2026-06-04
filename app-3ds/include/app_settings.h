#ifndef ANKI3DS_APP_SETTINGS_H
#define ANKI3DS_APP_SETTINGS_H

#define APP_SETTINGS_DEFAULT_NEW_LIMIT 20
#define APP_SETTINGS_DEFAULT_REVIEW_LIMIT 200
#define APP_SETTINGS_MAX_DAILY_LIMIT 1000000

enum app_settings_load_result
{
	APP_SETTINGS_LOAD_OK,
	APP_SETTINGS_LOAD_NOT_FOUND,
	APP_SETTINGS_LOAD_BAD_FORMAT,
};

enum app_settings_parse_result
{
	APP_SETTINGS_PARSE_OK,
	APP_SETTINGS_PARSE_LINE_TOO_LONG,
	APP_SETTINGS_PARSE_BAD_FIELD_COUNT,
	APP_SETTINGS_PARSE_BAD_VALUE,
	APP_SETTINGS_PARSE_UNKNOWN_KEY,
	APP_SETTINGS_PARSE_DUPLICATE_KEY,
	APP_SETTINGS_PARSE_MISSING_NEW_LIMIT,
	APP_SETTINGS_PARSE_MISSING_REVIEW_LIMIT,
	APP_SETTINGS_PARSE_READ_ERROR,
	APP_SETTINGS_PARSE_RECOVERY_ERROR,
};

enum app_settings_save_result
{
	APP_SETTINGS_SAVE_OK,
	APP_SETTINGS_SAVE_FAILED,
};

struct app_settings
{
	unsigned int new_limit;
	unsigned int review_limit;
};

struct app_settings_load_report
{
	unsigned int line_number;
	enum app_settings_parse_result parse_result;
};

void app_settings_default(struct app_settings *settings);
enum app_settings_load_result app_settings_load(struct app_settings *settings, const char *path);
enum app_settings_load_result app_settings_load_with_report(
	struct app_settings *settings,
	const char *path,
	struct app_settings_load_report *report
);
enum app_settings_save_result app_settings_save(
	const struct app_settings *settings,
	const char *path
);
const char *app_settings_parse_result_name(enum app_settings_parse_result result);
const char *app_settings_load_result_name(enum app_settings_load_result result);
const char *app_settings_save_result_name(enum app_settings_save_result result);

#endif
