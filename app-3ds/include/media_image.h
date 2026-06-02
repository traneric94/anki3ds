#ifndef ANKI3DS_MEDIA_IMAGE_H
#define ANKI3DS_MEDIA_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MEDIA_IMAGE_MAX_WIDTH 160
#define MEDIA_IMAGE_MAX_HEIGHT 72
#define MEDIA_IMAGE_MAX_PIXELS (MEDIA_IMAGE_MAX_WIDTH * MEDIA_IMAGE_MAX_HEIGHT)

enum media_image_load_result
{
	MEDIA_IMAGE_LOAD_OK,
	MEDIA_IMAGE_LOAD_NOT_FOUND,
	MEDIA_IMAGE_LOAD_BAD_FORMAT,
	MEDIA_IMAGE_LOAD_TOO_LARGE,
};

struct media_image
{
	bool loaded;
	unsigned int width;
	unsigned int height;
	uint16_t pixels[MEDIA_IMAGE_MAX_PIXELS];
};

void media_image_init(struct media_image *image);
enum media_image_load_result media_image_load(struct media_image *image, const char *path);
const char *media_image_load_result_name(enum media_image_load_result result);

#endif
