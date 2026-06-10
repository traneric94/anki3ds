#ifndef ANKI3DS_APP_TEXT_H
#define ANKI3DS_APP_TEXT_H

#include <stddef.h>

size_t app_text_utf8_char_length(const char *text);
size_t app_text_column_count(const char *text);
size_t app_text_byte_count_for_columns(const char *text, size_t max_columns);
size_t app_text_wrapped_row_count(const char *text, size_t max_columns);
size_t app_text_max_scroll_offset(
	const char *text,
	size_t max_columns,
	size_t visible_rows
);
size_t app_text_copy_truncated_line(
	const char *text,
	size_t max_columns,
	char *destination,
	size_t destination_size
);
size_t app_text_copy_wrapped_window(
	const char *text,
	size_t max_columns,
	size_t scroll_offset,
	size_t visible_rows,
	char *destination,
	size_t destination_size
);

#endif
