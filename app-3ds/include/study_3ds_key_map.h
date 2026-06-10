#ifndef ANKI3DS_STUDY_3DS_KEY_MAP_H
#define ANKI3DS_STUDY_3DS_KEY_MAP_H

#include <3ds.h>

unsigned int study_3ds_key_map_buttons_from_keys(u32 keys_down);
unsigned int study_3ds_key_map_dpad_buttons_from_keys(u32 keys_down);
unsigned int study_3ds_key_map_cpad_buttons_from_keys(u32 keys_down);

#endif
