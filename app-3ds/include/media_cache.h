#ifndef ANKI3DS_MEDIA_CACHE_H
#define ANKI3DS_MEDIA_CACHE_H

#include <stdbool.h>
#include <stddef.h>

#include "media_image.h"

#define MEDIA_CACHE_MAX_PATH_LENGTH 240
#define MEDIA_CACHE_SLOT_COUNT 2

struct media_cache_slot
{
	bool occupied;
	char path[MEDIA_CACHE_MAX_PATH_LENGTH];
	enum media_image_load_result result;
	struct media_image image;
};

struct media_cache
{
	size_t next_slot;
	struct media_cache_slot slots[MEDIA_CACHE_SLOT_COUNT];
};

void media_cache_init(struct media_cache *cache);
void media_cache_clear(struct media_cache *cache);
const struct media_cache_slot *media_cache_load(
	struct media_cache *cache,
	const char *path
);

#endif
