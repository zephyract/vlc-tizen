/*****************************************************************************
 * Copyright © 2015 VideoLAN, VideoLabs SAS
 *****************************************************************************
 *
 * Authors: Nicolas Rechatin [nicolas@videolabs.io]
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

#ifndef INTERFACE_H_
#define INTERFACE_H_

#include "common.h"
#include "application.h"

#include "media/media_library.hpp"

typedef struct interface interface;
typedef struct mini_player mini_player;

interface *
intf_create(application *);

void
intf_destroy(interface *);

application *
intf_get_application(interface *);

typedef enum view_e {
    VIEW_AUTO = -1,
    VIEW_VIDEO,
    VIEW_AUDIO,
    VIEW_FILES,
    VIEW_SETTINGS,
    VIEW_ABOUT,
    VIEW_MAX,
} view_e;

void
intf_create_view(interface *, view_e);

// FIXME
void
intf_show_previous_view(interface *);

void
intf_create_video_player(interface *, const char *psz_path);

mini_player *
intf_get_mini_player(interface *);

void
intf_update_mini_player(interface *);

Evas_Object *
intf_get_root_box(interface *intf);

Evas_Object *
intf_get_main_naviframe(interface *intf);

void
intf_register_file_changed(interface *intf, view_e type,
        media_library_file_list_changed_cb cb, void* p_user_data);

void
intf_ml_file_changed( void* p_user_data );

#endif /* INTERFACE_H_ */
