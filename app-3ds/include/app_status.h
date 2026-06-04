#ifndef ANKI3DS_APP_STATUS_H
#define ANKI3DS_APP_STATUS_H

#include <stddef.h>

enum app_status_color
{
	APP_STATUS_COLOR_NEUTRAL,
	APP_STATUS_COLOR_WARNING,
	APP_STATUS_COLOR_SUCCESS,
	APP_STATUS_COLOR_DANGER,
};

enum app_status_color app_status_message_color(const char *message);
void app_status_format_for_width(
	char *destination,
	size_t destination_size,
	const char *message,
	size_t max_columns
);

#endif
