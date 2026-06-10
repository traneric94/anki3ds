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

struct app_text_wrapped_row
{
	size_t start;
	size_t end;
	size_t next;
	bool ended_by_newline;
};

static bool app_text_break_space(char value)
{
	return value == ' ' || value == '\t';
}

static size_t app_text_trim_break_spaces(
	const char *text,
	size_t start,
	size_t end
)
{
	if (text == NULL)
		return end;

	while (
		end > start &&
		app_text_break_space(text[end - 1])
	)
	{
		end--;
	}

	return end;
}

static void app_text_set_wrapped_row(
	struct app_text_wrapped_row *row,
	const char *text,
	size_t start,
	size_t end,
	size_t next,
	bool ended_by_newline
)
{
	if (row == NULL)
		return;

	row->start = start;
	row->end = app_text_trim_break_spaces(text, start, end);
	row->next = next;
	row->ended_by_newline = ended_by_newline;
}

static bool app_text_next_wrapped_row(
	const char *text,
	size_t max_columns,
	size_t *index,
	struct app_text_wrapped_row *row
)
{
	size_t start;
	size_t end;
	size_t columns = 0;
	size_t last_break_end = (size_t)-1;
	size_t last_break_next = (size_t)-1;

	if (text == NULL || index == NULL || row == NULL || max_columns == 0)
		return false;

	start = *index;
	end = start;
	while (text[start] != '\0')
	{
		size_t char_length = app_text_utf8_char_length(&text[start]);
		char value = text[start];

		if (char_length == 0)
			return false;
		if (value != '\r' && !app_text_break_space(value))
			break;
		start += char_length;
		end = start;
	}
	if (text[start] == '\0')
		return false;

	while (text[end] != '\0')
	{
		char value = text[end];
		size_t char_length = app_text_utf8_char_length(&text[end]);

		if (char_length == 0)
			break;
		if (value == '\r')
		{
			end += char_length;
			continue;
		}
		if (value == '\n')
		{
			*index = end + char_length;
			app_text_set_wrapped_row(
				row,
				text,
				start,
				end,
				*index,
				true
			);
			return true;
		}
		if (columns >= max_columns)
		{
			if (last_break_end != (size_t)-1)
			{
				*index = last_break_next;
				app_text_set_wrapped_row(
					row,
					text,
					start,
					last_break_end,
					*index,
					false
				);
				return true;
			}

			*index = end;
			app_text_set_wrapped_row(
				row,
				text,
				start,
				end,
				*index,
				false
			);
			return true;
		}
		if (app_text_break_space(value))
		{
			last_break_end = end;
			last_break_next = end + char_length;
		}

		end += char_length;
		columns++;
	}

	*index = end;
	app_text_set_wrapped_row(row, text, start, end, *index, false);
	return true;
}

size_t app_text_wrapped_row_count(const char *text, size_t max_columns)
{
	size_t rows = 0;
	size_t index = 0;
	bool trailing_empty_row = false;

	if (text == NULL || max_columns == 0)
		return 0;
	if (text[0] == '\0')
		return 1;

	while (text[index] != '\0')
	{
		struct app_text_wrapped_row row;

		if (!app_text_next_wrapped_row(text, max_columns, &index, &row))
			break;
		rows++;
		trailing_empty_row = row.ended_by_newline && text[row.next] == '\0';
	}
	if (trailing_empty_row)
		rows++;

	return rows > 0 ? rows : 1;
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

static void app_text_append_bytes(
	char *destination,
	size_t destination_size,
	size_t *used,
	const char *source,
	size_t byte_count
)
{
	if (
		destination == NULL ||
		destination_size == 0 ||
		used == NULL ||
		source == NULL ||
		byte_count == 0 ||
		*used + 1 >= destination_size
	)
	{
		return;
	}

	if (byte_count > destination_size - *used - 1)
		byte_count = destination_size - *used - 1;

	for (size_t index = 0; index < byte_count; index++)
		destination[*used + index] = source[index];
	*used += byte_count;
	destination[*used] = '\0';
}

size_t app_text_copy_truncated_line(
	const char *text,
	size_t max_columns,
	char *destination,
	size_t destination_size
)
{
	size_t used = 0;
	size_t bytes = 0;
	size_t columns = 0;
	bool truncated = false;

	if (destination == NULL || destination_size == 0)
		return 0;

	destination[0] = '\0';
	if (text == NULL || max_columns == 0)
		return 0;

	while (text[bytes] != '\0')
	{
		size_t char_length = app_text_utf8_char_length(&text[bytes]);
		char value = text[bytes];

		if (char_length == 0)
			break;
		if (value == '\r' || value == '\n')
		{
			truncated = true;
			break;
		}
		if (columns >= max_columns)
		{
			truncated = true;
			break;
		}

		bytes += char_length;
		columns++;
	}

	if (!truncated)
	{
		app_text_append_bytes(
			destination,
			destination_size,
			&used,
			text,
			bytes
		);
		return used;
	}

	if (max_columns <= 3)
	{
		for (size_t index = 0; index < max_columns; index++)
			app_text_append_bytes(
				destination,
				destination_size,
				&used,
				".",
				1
			);
		return used;
	}

	bytes = 0;
	columns = 0;
	while (text[bytes] != '\0' && columns < max_columns - 3)
	{
		size_t char_length = app_text_utf8_char_length(&text[bytes]);
		char value = text[bytes];

		if (char_length == 0 || value == '\r' || value == '\n')
			break;

		bytes += char_length;
		columns++;
	}
	bytes = app_text_trim_break_spaces(text, 0, bytes);
	app_text_append_bytes(destination, destination_size, &used, text, bytes);
	app_text_append_bytes(destination, destination_size, &used, "...", 3);
	return used;
}

static void app_text_append_wrapped_row(
	char *destination,
	size_t destination_size,
	size_t *used,
	const char *text,
	size_t start,
	size_t end,
	size_t row,
	size_t scroll_offset,
	size_t visible_rows,
	size_t *visible_row_count
)
{
	bool row_visible;

	if (visible_row_count == NULL)
		return;

	row_visible = row >= scroll_offset && *visible_row_count < visible_rows;
	if (row_visible)
	{
		if (*visible_row_count > 0)
			app_text_append_bytes(destination, destination_size, used, "\n", 1);
		for (size_t index = start; index < end; )
		{
			size_t char_length = app_text_utf8_char_length(&text[index]);

			if (char_length == 0)
				break;
			if (text[index] != '\r')
			{
				app_text_append_bytes(
					destination,
					destination_size,
					used,
					&text[index],
					char_length
				);
			}
			index += char_length;
		}
		(*visible_row_count)++;
	}
}

size_t app_text_copy_wrapped_window(
	const char *text,
	size_t max_columns,
	size_t scroll_offset,
	size_t visible_rows,
	char *destination,
	size_t destination_size
)
{
	size_t used = 0;
	size_t row = 0;
	size_t visible_row_count = 0;
	size_t index = 0;
	bool trailing_empty_row = false;

	if (destination == NULL || destination_size == 0)
		return 0;

	destination[0] = '\0';
	if (text == NULL || max_columns == 0 || visible_rows == 0)
		return 0;
	if (text[0] == '\0')
	{
		app_text_append_wrapped_row(
			destination,
			destination_size,
			&used,
			text,
			0,
			0,
			0,
			scroll_offset,
			visible_rows,
			&visible_row_count
		);
		return used;
	}

	while (text[index] != '\0' && visible_row_count < visible_rows)
	{
		struct app_text_wrapped_row wrapped_row;

		if (
			!app_text_next_wrapped_row(
				text,
				max_columns,
				&index,
				&wrapped_row
			)
		)
		{
			break;
		}
		app_text_append_wrapped_row(
			destination,
			destination_size,
			&used,
			text,
			wrapped_row.start,
			wrapped_row.end,
			row,
			scroll_offset,
			visible_rows,
			&visible_row_count
		);
		trailing_empty_row =
			wrapped_row.ended_by_newline && text[wrapped_row.next] == '\0';
		row++;
	}
	if (trailing_empty_row && visible_row_count < visible_rows)
		app_text_append_wrapped_row(
			destination,
			destination_size,
			&used,
			text,
			index,
			index,
			row,
			scroll_offset,
			visible_rows,
			&visible_row_count
		);

	return used;
}
