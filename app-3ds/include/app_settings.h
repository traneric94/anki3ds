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

struct app_settings
{
	unsigned int new_limit;
	unsigned int review_limit;
};

void app_settings_default(struct app_settings *settings);
enum app_settings_load_result app_settings_load(struct app_settings *settings, const char *path);
const char *app_settings_load_result_name(enum app_settings_load_result result);

#endif
