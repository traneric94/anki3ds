#include "media_cache.h"

#include <stdio.h>
#include <string.h>

static void copy_string(char *destination, size_t destination_size, const char *source)
{
	if (destination_size == 0)
		return;

	snprintf(destination, destination_size, "%s", source);
}

void media_cache_init(struct media_cache *cache)
{
	memset(cache, 0, sizeof(*cache));
}

void media_cache_clear(struct media_cache *cache)
{
	media_cache_init(cache);
}

const struct media_cache_slot *media_cache_load(
	struct media_cache *cache,
	const char *path
)
{
	struct media_cache_slot *slot;

	for (size_t index = 0; index < MEDIA_CACHE_SLOT_COUNT; index++)
	{
		if (cache->slots[index].occupied && strcmp(cache->slots[index].path, path) == 0)
			return &cache->slots[index];
	}

	slot = &cache->slots[cache->next_slot];
	cache->next_slot = (cache->next_slot + 1) % MEDIA_CACHE_SLOT_COUNT;
	memset(slot, 0, sizeof(*slot));
	copy_string(slot->path, sizeof(slot->path), path);
	slot->result = media_image_load(&slot->image, path);
	slot->occupied = true;
	return slot;
}
