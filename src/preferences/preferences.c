/*****************************************************************************
 * Copyright © 2015 VideoLAN, VideoLabs SAS
 *****************************************************************************
 *
 * Authors: Ludovic Fauvet <etix@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
/*
 * By committing to this project, you allow VideoLAN and VideoLabs to relicense
 * the code to a different OSI approved license, in case it is required for
 * compatibility with the Store
 *****************************************************************************/

#include "preferences/preferences.h"
#include "ui/settings/menu_id.h"
#include <app_preference.h>

typedef union pref_id {
    pref_enum t_enum;
    pref_index t_index;
    pref_bool t_bool;
    int t_int;
} pref_id;

typedef struct pref_key_id_map
{
    pref_id id;
    const char* key;
} pref_key_id_map;

pref_key_id_map mapping[] =
{
        // type enum
        {{PREF_HWACCELERATION}, "HWACCELERATION"},
        {{PREF_SUBSENC}, "SUBSENC"},
        {{PREF_ORIENTATION}, "ORIENTATION"},
        {{PREF_DEBLOCKING}, "DEBLOCKING"},

        // type bool
        {{PREF_FRAME_SKIP}, "FRAME_SKIP"},
        {{PREF_AUDIO_STRETCH}, "AUDIO_STRETCH"},
        {{PREF_DIRECTORIES_INTERNAL}, "DIRECTORIES_INTERNAL"},
        {{0}}
};

const char *
preferences_get_key(pref_id key_id)
{
    for (int i = 0; mapping[i].id.t_int != 0; i++)
    {
        if (mapping[i].id.t_int == key_id.t_int)
            return mapping[i].key;
    }
    return NULL;
}

void
preferences_set_enum(pref_enum key, menu_id value)
{
    pref_id key_id;
    key_id.t_enum = key;

    preference_set_int(preferences_get_key(key_id), value);
}

void
preferences_set_index(pref_enum key, int value)
{
    pref_id key_id;
    key_id.t_index = key;

    preference_set_int(preferences_get_key(key_id), value);
}

void
preferences_set_bool(pref_bool key, bool value)
{
    pref_id key_id;
    key_id.t_enum = key;

    preference_set_boolean(preferences_get_key(key_id), value);
}

menu_id
preferences_get_enum(pref_enum key, menu_id default_value)
{
    pref_id key_id;
    key_id.t_enum = key;

    int value;
    if (preference_get_int(preferences_get_key(key_id), &value) != PREFERENCE_ERROR_NONE)
        return default_value;

    return (menu_id)value;
}

int
preferences_get_index(pref_index key, int default_value)
{
    pref_id key_id;
    key_id.t_index = key;

    int value;
    if (preference_get_int(preferences_get_key(key_id), &value) != PREFERENCE_ERROR_NONE)
        return default_value;

    return value;
}

bool
preferences_get_bool(pref_bool key, bool default_value)
{
    pref_id key_id;
    key_id.t_bool = key;

    bool value;
    if (preference_get_boolean(preferences_get_key(key_id), &value) != PREFERENCE_ERROR_NONE)
        return default_value;

    return value;
}
