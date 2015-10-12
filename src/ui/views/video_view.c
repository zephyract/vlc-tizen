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

#include "common.h"

#include "application.h"
#include "ui/interface.h"
#include "video_view.h"
#include "video_view_list.h"
#include "ui/menu/popup_menu.h"
#include "media/library/media_library.hpp"


#include <Elementary.h>

struct view_sys
{
    application *p_app;
    Evas_Object *p_parent;
    list_view* p_list;

    Evas_Object *p_overflow_menu;
};

static void
video_view_refresh_cb(void *data, Evas_Object *obj, void *event_info)
{
    view_sys *p_sys = data;

    if (!p_sys)
        return;

    media_library* p_ml = (media_library*)application_get_media_library(p_sys->p_app);
    if (p_ml != NULL)
        media_library_reload(p_ml);

    /* */
    evas_object_del(obj);
    p_sys->p_overflow_menu = NULL;
}

static popup_menu video_view_popup_menu[] =
{
    {"Refresh", NULL, video_view_refresh_cb},
    {0}
};

static bool
video_view_callback(view_sys *p_view_sys, interface_view_event event)
{
    switch (event) {
    case INTERFACE_VIEW_EVENT_MENU:
    {
        p_view_sys->p_overflow_menu = popup_menu_add(video_view_popup_menu, p_view_sys, p_view_sys->p_parent);
        evas_object_show(p_view_sys->p_overflow_menu);
        return true;
    }
    case INTERFACE_VIEW_EVENT_BACK:
        if (p_view_sys->p_overflow_menu) {
            evas_object_del(p_view_sys->p_overflow_menu);
            p_view_sys->p_overflow_menu = NULL;
            return true;
        }
        return false;
    default:
        break;
    }

    return false;
}

static bool
video_view_has_menu(view_sys *p_view_sys)
{
    return true;
}

interface_view*
create_video_view(interface *intf, Evas_Object *parent)
{
    interface_view *view = calloc(1, sizeof(*view));

    view_sys *p_sys = view->p_view_sys = malloc(sizeof(*p_sys));
    p_sys->p_app = intf_get_application(intf);
    p_sys->p_parent = parent;
    p_sys->p_overflow_menu = NULL;

    view->pf_event = video_view_callback;
    view->pf_has_menu = video_view_has_menu;


    /* Box container */
    Evas_Object *box = elm_box_add(parent);
    evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_show(box);

    p_sys->p_list = video_view_list_create(intf, box);

    view->view = box;

    /* */
    return view;
}

void
destroy_video_view(interface_view *view)
{
    list_view* p_list_view = view->p_view_sys->p_list;
    p_list_view->pf_del(p_list_view->p_sys);
    free(p_list_view);
    free(view->p_view_sys);
    free(view);
}
