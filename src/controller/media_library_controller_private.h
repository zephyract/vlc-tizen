/*****************************************************************************
 * Copyright © 2015-2016 VideoLAN, VideoLabs SAS
 *****************************************************************************
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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

#ifndef MEDIA_LIBRARY_CONTROLLER_PRIVATE_H_
# define MEDIA_LIBRARY_CONTROLLER_PRIVATE_H_

#include "application.h"
#include "media/library/library_item.h"

struct library_item
{
    LIBRARY_ITEM_COMMON
};

typedef void                (*pf_media_library_get_content_cb)( media_library* p_ml, media_library_list_cb cb, void* p_user_data );
typedef bool                (*pf_item_compare_cb)(const void* p_left, const void* p_right);
typedef void*               (*pf_item_duplicate_cb)( const void* p_item );
typedef bool                (*pf_accept_item_cb)( const library_item* p_item );

struct media_library_controller
{
    /**
     * Content Management
     */
    application*    p_app;
    list_view*      p_list_view;
    Eina_List*      p_content;
    void*           p_user_data;

    /**
     * Callbacks & settings
     */
    pf_media_library_get_content_cb pf_media_library_get_content;
    pf_item_compare_cb              pf_item_compare;
    pf_item_duplicate_cb            pf_item_duplicate;
    pf_accept_item_cb               pf_accept_item;
};

#endif //MEDIA_LIBRARY_CONTROLLER_PRIVATE_H_
