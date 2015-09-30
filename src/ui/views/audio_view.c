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
#include "media_storage.h"

#include <Elementary.h>
#include <storage.h>
#include <app.h>

#include "ui/interface.h"
#include "ui/views/audio_view.h"
#include "ui/audio_player.h"

#include "ui/utils.h"

#include "media_item.h"

typedef struct audio_view
{
    interface *p_intf;
    Evas_Object *nf_toolbar;

} audio_view;

typedef struct audio_list_item
{
    audio_view *p_av;

    media_item *mitem;

    const Elm_Genlist_Item_Class *itc;

} audio_list_item;

static Evas_Object*
create_audio_list(const char* path, audio_view *av);

static void
audio_gl_selected_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
    audio_list_item *ald = data;
    struct stat sb;
    stat(ald->mitem->psz_path, &sb);

    if (S_ISREG(sb.st_mode))
    {
        /* Launch the media player */
        create_base_player(intf_get_mini_player(ald->p_av->p_intf), ald->mitem->psz_path);
        LOGI("VLC Player launch");
    }

    else if (S_ISDIR(sb.st_mode))
    {
        /* Continue to browse media folder */
        create_audio_list(ald->mitem->psz_path, ald->p_av);
    }
    else
    {
        LOGI("This is not a file or a folder");
    }

}

static void
free_list_item_data(void *data, Evas_Object *obj, void *event_info)
{
    audio_list_item *ald = data;
    /* Free the file path when the current genlist is deleted */
    /* For example when the player is launched or a new genlist is created */
    free(ald->mitem->psz_path);
    LOGD("Path free");

}

static Evas_Object*
create_icon(Evas_Object *parent)
{
    return create_image(parent, "background_cone.png" );
}

static char *
gl_text_get_cb(void *data, Evas_Object *obj, const char *part)
{
    audio_list_item *ald = data;
    const Elm_Genlist_Item_Class *itc = ald->itc;
    char *buf;

    /* Check the item class style and put the current folder or file name as a string */
    /* Then put this string as the genlist item label */
    if (itc->item_style && !strcmp(itc->item_style, "2line.top.3")) {
        if (part && !strcmp(part, "elm.text.main.left.top")) {
            asprintf(&buf, "<b>%s</b>", ald->mitem->psz_title);
            return buf;
        }
        else if (!strcmp(part, "elm.text.sub.left.bottom")) {
            return strdup("Artist");
        }
        else if (!strcmp(part, "elm.text.sub.right.bottom")) {
            int m = 4;
            int s = 56;
            asprintf(&buf,"%d m %d s", m, s);
            return buf;
        }
    }
    return NULL;
}

static Evas_Object*
gl_content_get_cb(void *data, Evas_Object *obj, const char *part)
{
    audio_list_item *ald = data;
    const Elm_Genlist_Item_Class *itc = ald->itc;
    Evas_Object *content = NULL;

    /* Check the item class style and add the object needed in the item class*/
    /* Here, puts the icon in the item class to add it to genlist items */
    if (itc->item_style && !strcmp(itc->item_style, "2line.top.3")) {
        if (part && !strcmp(part, "elm.icon.1")) {
            content = elm_layout_add(obj);
            elm_layout_theme_set(content, "layout", "list/B/type.1", "default");
            Evas_Object *icon = create_icon(content);
            elm_layout_content_set(content, "elm.swallow.content", icon);
        }
    }

    return content;
}

Evas_Object*
create_audio_list(const char* path, audio_view *av)
{
    char *buff;
    audio_list_item *ald = malloc(sizeof(*ald));
    ald->p_av = av;
    const char *str = NULL;
    DIR* rep = NULL;
    struct dirent* current_folder = NULL;

    /* Set then create the Genlist object */
    Evas_Object *parent = av->nf_toolbar;
    Evas_Object *genlist;
    Elm_Object_Item *it;
    Elm_Genlist_Item_Class *itc = elm_genlist_item_class_new();
    itc->item_style = "2line.top.3";
    itc->func.text_get = gl_text_get_cb;
    itc->func.content_get = gl_content_get_cb;
    genlist = elm_genlist_add(parent);

    /* Set the genlist scoller mode */
    elm_scroller_single_direction_set(genlist, ELM_SCROLLER_SINGLE_DIRECTION_HARD);
    /* Enable the genlist HOMOGENEOUS mode */
    elm_genlist_homogeneous_set(genlist, EINA_TRUE);
    /* Enable the genlist COMPRESS mode */
    elm_genlist_mode_set(genlist, ELM_LIST_COMPRESS);

    /* Make a realpath to use a clean path in the function */
    buff = realpath(path, NULL);
    path = buff;

    /* Open the path repository then put it as a dirent variable */
    rep = opendir(path);

    if (path == NULL)
    {
        LOGI("No path");
        return NULL ;
    }

    if  (rep == NULL)
    {
        char *error;
        error = strerror(errno);
        LOGI("Empty repository or Error due to %s", error);

        return NULL ;
    }

    /* Stop when the readdir have red the all repository */
    while ((current_folder = readdir(rep)) != NULL)
    {
        audio_list_item *ald = malloc(sizeof(*ald));
        ald->p_av = av;

        char *psz_path;
        /* Concatenate the file path and the selected folder or file name */
        asprintf(&psz_path, "%s/%s", path, current_folder->d_name);
        ald->mitem = media_item_create(psz_path, MEDIA_ITEM_TYPE_AUDIO);

        /* Put the folder or file name as a usable string for genlist item label */
        str = current_folder->d_name;
        /* Put the folder or file name in the audio_list_data struct for callbacks */
        ald->mitem->psz_title = str;

        /* Avoid genlist item append for "." and ".." d_name */
        if (str && (strcmp(str, ".")==0 || strcmp(str, "..")==0))
        {
            continue;
        }

        /* Set and append new item in the genlist */
        it = elm_genlist_item_append(genlist,
                itc,                            /* genlist item class               */
                ald,                            /* genlist item class user data     */
                NULL,                            /* genlist parent item              */
                ELM_GENLIST_ITEM_NONE,            /* genlist item type                */
                audio_gl_selected_cb,            /* genlist select smart callback    */
                ald);                            /* genlist smart callback user data */

        /* Put genlist item in the audio_list_data struct for callbacks */
        ald->itc = itc;
        /* */
        elm_object_item_del_cb_set(it, free_list_item_data);
    }
    /* */
    elm_genlist_item_class_free(itc);
    return genlist;
}

static void
tabbar_item_selected(audio_view *av, Elm_Object_Item *audio_it)
{
    application *p_app = intf_get_application(av->p_intf);
    const char *audio_path = application_get_media_path(p_app, MEDIA_DIRECTORY_MUSIC);
    const char *str = NULL;
    Evas_Object *current_audio_view;

    /* Get the item selected in the toolbar */
    str = elm_object_item_text_get(audio_it);

    /* Create the view depending on the item selected in the toolbar */
    if (str && !strcmp(str, "Songs")) {
        current_audio_view = create_audio_list(audio_path, av);
    }
    else if (str && !strcmp(str, "Artists")) {
        current_audio_view = create_audio_list(audio_path, av);
    }
    else if (str && !strcmp(str, "Albums")) {
        current_audio_view = create_audio_list(audio_path, av);
    }
    else     {
        current_audio_view = create_audio_list(audio_path, av);
    }

    elm_object_content_set(av->nf_toolbar, current_audio_view );
}

static void
tabbar_item_cb(void *data, Evas_Object *obj, void *event_info)
{
    audio_view *av = data;
    Elm_Object_Item *audio_it = event_info;

    /* Call the function that creates the views */
    tabbar_item_selected(av, audio_it);
}

static Evas_Object*
create_toolbar(audio_view *av)
{
    /* Create and set the toolbar */
    Evas_Object *tabbar = elm_toolbar_add(av->nf_toolbar);
    elm_object_style_set(tabbar, "tabbar");

    /* Set the toolbar shrink mode to NONE */
    elm_toolbar_shrink_mode_set(tabbar, ELM_TOOLBAR_SHRINK_SCROLL);
    /* Expand the content to fill the toolbar */
    elm_toolbar_transverse_expanded_set(tabbar, EINA_TRUE);
    /* Items will only call their selection func and callback when first becoming selected*/
    elm_toolbar_select_mode_set(tabbar, ELM_OBJECT_SELECT_MODE_DEFAULT);

    evas_object_size_hint_min_set(tabbar, 450, 400);
    evas_object_size_hint_max_set(tabbar, 450, 400);

    /* Append new entry in the toolbar with the Icon & Label wanted */
    elm_toolbar_item_append(tabbar, NULL, "Songs", tabbar_item_cb, av);
    elm_toolbar_item_append(tabbar, NULL, "Artists", tabbar_item_cb, av);
    elm_toolbar_item_append(tabbar, NULL, "Albums", tabbar_item_cb, av);
    elm_toolbar_item_append(tabbar, NULL, "Playlist", tabbar_item_cb, av);

    return tabbar;
}

Evas_Object *
create_audio_view(interface *intf, Evas_Object *parent)
{
    Elm_Object_Item *nf_it, *tabbar_it;
    Evas_Object *tabbar;

    audio_view *av = calloc(1, sizeof(*av));
    av->p_intf = intf;

    /* Toolbar Naviframe */
    av->nf_toolbar = elm_naviframe_add(parent);
    evas_object_size_hint_weight_set(av->nf_toolbar, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(av->nf_toolbar, EVAS_HINT_FILL, EVAS_HINT_FILL);

    /* Toolbar Naviframe Settings */
    elm_object_part_content_set(parent, "elm.swallow.content", av->nf_toolbar);
    evas_object_show(av->nf_toolbar);
    nf_it = elm_naviframe_item_push(av->nf_toolbar, NULL, NULL, NULL, NULL, "tabbar/icon/notitle");

    /* Push on the parent view */
    elm_naviframe_item_push(parent, _("<b>Audio</b>"), NULL, NULL, av->nf_toolbar, "basic");

    /* Create the toolbar in the view */
    tabbar = create_toolbar(av);
    elm_object_item_part_content_set(nf_it, "tabbar", tabbar);

    /* Set the first item in the toolbar */
    tabbar_it = elm_toolbar_first_item_get(tabbar);
    elm_toolbar_item_selected_set(tabbar_it, EINA_TRUE);

    return av->nf_toolbar;
}
