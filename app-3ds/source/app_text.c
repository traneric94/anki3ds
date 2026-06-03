#include "app_text.h"

#include <stdbool.h>

static bool utf8_continuation_is_valid(unsigned char value)
{
	return value >= 0x80 && value <= 0xbf;
}

size_t app_text_utf8_char_length(const char *text)
{
	unsigned char first;
	unsigned char second;
	unsigned char third;
	unsigned char fourth;

	if (text == NULL || text[0] == '\0')
		return 0;

	first = (unsigned char)text[0];
	if (first < 0x80)
		return 1;

	second = (unsigned char)text[1];
	if (second == '\0')
		return 1;
	if (first >= 0xc2 && first <= 0xdf)
		return utf8_continuation_is_valid(second) ? 2 : 1;

	third = (unsigned char)text[2];
	if (third == '\0')
		return 1;
	if (first == 0xe0)
	{
		return (
			second >= 0xa0 &&
			second <= 0xbf &&
			utf8_continuation_is_valid(third)
		) ? 3 : 1;
	}
	if (first >= 0xe1 && first <= 0xec)
	{
		return (
			utf8_continuation_is_valid(second) &&
			utf8_continuation_is_valid(third)
		) ? 3 : 1;
	}
	if (first == 0xed)
	{
		return (
			second >= 0x80 &&
			second <= 0x9f &&
			utf8_continuation_is_valid(third)
		) ? 3 : 1;
	}
	if (first >= 0xee && first <= 0xef)
	{
		return (
			utf8_continuation_is_valid(second) &&
			utf8_continuation_is_valid(third)
		) ? 3 : 1;
	}

	fourth = (unsigned char)text[3];
	if (fourth == '\0')
		return 1;
	if (first == 0xf0)
	{
		return (
			second >= 0x90 &&
			second <= 0xbf &&
			utf8_continuation_is_valid(third) &&
			utf8_continuation_is_valid(fourth)
		) ? 4 : 1;
	}
	if (first >= 0xf1 && first <= 0xf3)
	{
		return (
			utf8_continuation_is_valid(second) &&
			utf8_continuation_is_valid(third) &&
			utf8_continuation_is_valid(fourth)
		) ? 4 : 1;
	}
	if (first == 0xf4)
	{
		return (
			second >= 0x80 &&
			second <= 0x8f &&
			utf8_continuation_is_valid(third) &&
			utf8_continuation_is_valid(fourth)
		) ? 4 : 1;
	}

	return 1;
}

size_t app_text_column_count(const char *text)
{
	size_t columns = 0;

	if (text == NULL)
		return 0;

	for (size_t index = 0; text[index] != '\0'; )
	{
		size_t char_length = app_text_utf8_char_length(&text[index]);

		if (char_length == 0)
			break;

		index += char_length;
		columns++;
	}

	return columns;
}

size_t app_text_byte_count_for_columns(const char *text, size_t max_columns)
{
	size_t bytes = 0;
	size_t columns = 0;

	if (text == NULL)
		return 0;

	while (text[bytes] != '\0' && columns < max_columns)
	{
		size_t char_length = app_text_utf8_char_length(&text[bytes]);

		if (char_length == 0)
			break;

		bytes += char_length;
		columns++;
	}

	return bytes;
}

size_t app_text_wrapped_row_count(const char *text, size_t max_columns)
{
	size_t rows = 1;
	size_t columns = 0;

	if (text == NULL || max_columns == 0)
		return 0;

	for (size_t index = 0; text[index] != '\0'; )
	{
		char value = text[index];
		size_t char_length = app_text_utf8_char_length(&text[index]);

		if (char_length == 0)
			break;
		if (value == '\r')
		{
			index += char_length;
			continue;
		}
		if (value == '\n')
		{
			rows++;
			columns = 0;
			index += char_length;
			continue;
		}
		if (columns >= max_columns)
		{
			rows++;
			columns = 0;
		}

		index += char_length;
		columns++;
	}

	return rows;
}

size_t app_text_max_scroll_offset(
	const char *text,
	size_t max_columns,
	size_t visible_rows
)
{
	size_t rows;

	if (visible_rows == 0)
		return 0;

	rows = app_text_wrapped_row_count(text, max_columns);
	if (rows <= visible_rows)
		return 0;

	return rows - visible_rows;
}
