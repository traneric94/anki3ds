#include "media_image.h"

#include <stdio.h>
#include <string.h>

#define MEDIA_IMAGE_HEADER_SIZE 8
#define MEDIA_IMAGE_MAGIC_0 'A'
#define MEDIA_IMAGE_MAGIC_1 '3'
#define MEDIA_IMAGE_MAGIC_2 'I'
#define MEDIA_IMAGE_MAGIC_3 '1'
#define MEDIA_IMAGE_BYTES_PER_PIXEL 2

static unsigned int read_u16_le(const unsigned char *bytes)
{
	return (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8);
}

void media_image_init(struct media_image *image)
{
	memset(image, 0, sizeof(*image));
}

enum media_image_load_result media_image_load(struct media_image *image, const char *path)
{
	FILE *file;
	unsigned char header[MEDIA_IMAGE_HEADER_SIZE];
	size_t pixel_count;
	size_t pixel_bytes;
	unsigned char *pixels;

	media_image_init(image);

	file = fopen(path, "rb");
	if (file == NULL)
		return MEDIA_IMAGE_LOAD_NOT_FOUND;

	if (fread(header, 1, sizeof(header), file) != sizeof(header))
	{
		fclose(file);
		return MEDIA_IMAGE_LOAD_BAD_FORMAT;
	}

	if (
		header[0] != MEDIA_IMAGE_MAGIC_0 ||
		header[1] != MEDIA_IMAGE_MAGIC_1 ||
		header[2] != MEDIA_IMAGE_MAGIC_2 ||
		header[3] != MEDIA_IMAGE_MAGIC_3
	)
	{
		fclose(file);
		return MEDIA_IMAGE_LOAD_BAD_FORMAT;
	}

	image->width = read_u16_le(&header[4]);
	image->height = read_u16_le(&header[6]);
	if (image->width == 0 || image->height == 0)
	{
		fclose(file);
		return MEDIA_IMAGE_LOAD_BAD_FORMAT;
	}
	if (
		image->width > MEDIA_IMAGE_MAX_WIDTH ||
		image->height > MEDIA_IMAGE_MAX_HEIGHT
	)
	{
		fclose(file);
		return MEDIA_IMAGE_LOAD_TOO_LARGE;
	}

	pixel_count = (size_t)image->width * image->height;
	if (pixel_count > MEDIA_IMAGE_MAX_PIXELS)
	{
		fclose(file);
		return MEDIA_IMAGE_LOAD_TOO_LARGE;
	}

	pixel_bytes = pixel_count * MEDIA_IMAGE_BYTES_PER_PIXEL;
	pixels = (unsigned char *)image->pixels;
	if (fread(pixels, 1, pixel_bytes, file) != pixel_bytes)
	{
		fclose(file);
		return MEDIA_IMAGE_LOAD_BAD_FORMAT;
	}

	for (size_t index = 0; index < pixel_count; index++)
	{
		image->pixels[index] =
			(uint16_t)read_u16_le(&pixels[index * MEDIA_IMAGE_BYTES_PER_PIXEL]);
	}

	if (fgetc(file) != EOF)
	{
		fclose(file);
		return MEDIA_IMAGE_LOAD_BAD_FORMAT;
	}

	fclose(file);
	image->loaded = true;
	return MEDIA_IMAGE_LOAD_OK;
}

const char *media_image_load_result_name(enum media_image_load_result result)
{
	switch (result)
	{
	case MEDIA_IMAGE_LOAD_OK:
		return "ok";
	case MEDIA_IMAGE_LOAD_NOT_FOUND:
		return "not found";
	case MEDIA_IMAGE_LOAD_BAD_FORMAT:
		return "bad format";
	case MEDIA_IMAGE_LOAD_TOO_LARGE:
		return "too large";
	}

	return "unknown";
}
