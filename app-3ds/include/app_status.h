#ifndef ANKI3DS_APP_STATUS_H
#define ANKI3DS_APP_STATUS_H

enum app_status_color
{
	APP_STATUS_COLOR_NEUTRAL,
	APP_STATUS_COLOR_WARNING,
	APP_STATUS_COLOR_SUCCESS,
	APP_STATUS_COLOR_DANGER,
};

enum app_status_color app_status_message_color(const char *message);

#endif
