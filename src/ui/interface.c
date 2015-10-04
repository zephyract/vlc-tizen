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

#include "system_storage.h"

#include <app.h>
#include <efl_extension.h>
#include <storage.h>
#include <system_settings.h>
#include <assert.h>

#include "interface.h"
#include "sidebar.h"
#include "audio_player.h"
#include "menu/main_popup_list.h"

#include "views/audio_view.h"
#include "views/video_view.h"
#include "views/directory_view.h"
#include "views/settings_view.h"
#include "views/about_view.h"
#include "views/video_player.h"

struct interface {
    application *p_app;      /* Reference to the application */

    Evas_Object *win;        /* Top Window, we need it to hide the system bar */

    Evas_Object *main_box;   /* Main box, contains the Naviframe and the miniplayer */

    /* Naviframe */
    Evas_Object *nf_content; /* Main naviframe */
    interface_view *nf_views[VIEW_MAX];
    interface_view *video_player;

    /* */
    Evas_Object *sidebar;    /* Sidebar panel */
    Evas_Object *sidebar_toggle_btn;

    /* Context popup-menu */
    Evas_Object *popup;
    Evas_Object *popup_toggle_btn;

    /* Miniplayer */
    mini_player *p_mini_player;
    Evas_Object *mini_player_layout;
};

struct
{
    const char* title;
    interface_view* (*pf_create)(interface *, Evas_Object *);
    void (*pf_destroy)(interface_view *);
    bool b_overflow_menu;
} interface_views[VIEW_MAX] =
{
    { "Video",     create_video_view    , destroy_video_view,     true},
    { "Audio",     create_audio_view    , destroy_audio_view,     true},
    { "Directory", create_directory_view, destroy_directory_view, false},
    { "Settings",  create_setting_view  , destroy_setting_view,   false},
    { "About",     create_about_view    , destroy_about_view,     false},
};

/* CALLBACKS */
static void
win_delete_request_cb(void *data, Evas_Object *obj, void *event_info)
{
    ui_app_exit();
}

static void
win_back_key_cb(void *data, Evas_Object *obj, void *event_info)
{
    interface *intf = data;
    /* Hide the sidebar first */
    if (!elm_object_disabled_get(intf->sidebar) && !elm_panel_hidden_get(intf->sidebar)) {
        elm_panel_hidden_set(intf->sidebar, EINA_TRUE);
    }
    /* Hide the popup menu then */
    else if (evas_object_visible_get(intf->popup)) {
        evas_object_del(intf->popup); //since elm_popup_dismiss doesn't work
    }
    /* And then the mini player (if playing) */ // FIXME
    else if (intf_mini_player_visible_get(intf) == true){
            LOGD("mini player visible");
            mini_player_stop(intf->p_mini_player);
    }
    /* Hide the audio_player then */
    else if (mini_player_fs_state(intf->p_mini_player) == true){ //FIXME
         collapse_fullscreen_player(intf->p_mini_player);
    }
    /* Finally pop out the stack */
    else {
        /* Get the top of the NaviFrame Stack */
        Elm_Object_Item *it = elm_naviframe_top_item_get(intf->nf_content);
        assert(it!=NULL); // Else we should have quit before

        interface_view *view = (interface_view *)elm_object_item_data_get(it);
        if(view){
            if(view->pf_event != NULL &&
               view->pf_event(view->p_view_sys, INTERFACE_VIEW_EVENT_BACK) == true ){
                /* View has accepted the event */
                return;
            }
            else if(view->pf_stop != NULL)
            {
                view->pf_stop(view->p_view_sys);
            }
        }

        /* Unpop the top view */
        elm_naviframe_item_pop(intf->nf_content);
        elm_win_indicator_mode_set(intf->win, ELM_WIN_INDICATOR_SHOW);

        /* If nothing left, exit */
        if (elm_naviframe_top_item_get(intf->nf_content) == NULL)
            ui_app_exit();
    }
}

static void
left_panel_button_clicked_cb(void *data, Evas_Object * obj, void *event_info)
{
    interface *intf = data;
    /* Disable the panel when left button is pressed */
    if (!elm_object_disabled_get(intf->sidebar)) elm_panel_toggle(intf->sidebar);
}

static void
right_panel_button_clicked_cb(void *data, Evas_Object * obj, void *event_info)
{
    interface *intf = data;
    intf->popup = create_popup(intf->main_box, intf);
}

void
intf_show_previous_view(interface *intf)
{
    elm_naviframe_item_pop(intf->nf_content);
}

/* GETTERS */
application *
intf_get_application(interface *p_intf)
{
    return p_intf->p_app;
}

Evas_Object *
intf_get_main_naviframe(interface *intf)
{
    return intf->nf_content;
}

/* CREATION */
static Evas_Object *
create_button(Evas_Object *parent, char *style)
{
    Evas_Object *button = elm_button_add(parent);
    elm_object_style_set(button, style);

    /* */
    evas_object_show(button);
    return button;
}

void
intf_show_view(interface *intf, int view_type)
{
    if(view_type == VIEW_AUTO)
        view_type = VIEW_VIDEO; /* Replace by the last saved tab */

    Evas_Object *nf_content = intf->nf_content;
    interface_view *view = intf->nf_views[view_type];

    /* Create the correct view and store it */
    if(view == NULL)
    {
        LOGD("New interface view %i", view_type);
        intf->nf_views[view_type] = view = interface_views[view_type].pf_create(intf, nf_content);
    }
    else
        LOGD("Recycling interface view %i", view_type);

    /* Push the view in the naviframe with the corresponding header */
    Elm_Object_Item *nf_it = elm_naviframe_item_push(nf_content, interface_views[view_type].title, NULL, NULL, view->view, "basic");
    elm_object_item_data_set(nf_it, view);

    /* Create then set the panel toggle btn and add his callbacks */
    if(intf->sidebar_toggle_btn == NULL)
    {
        intf->sidebar_toggle_btn = create_button(nf_content, "naviframe/drawers");
        evas_object_smart_callback_add(intf->sidebar_toggle_btn, "clicked", left_panel_button_clicked_cb, intf);
    }
    elm_object_part_content_set(nf_content, "title_left_btn", intf->sidebar_toggle_btn);

    /* */
    if(interface_views[view_type].b_overflow_menu)
    {
        if(intf->popup_toggle_btn == NULL)
        {
            intf->popup_toggle_btn = create_button(nf_content, "naviframe/drawers");
            evas_object_smart_callback_add(intf->popup_toggle_btn, "clicked", right_panel_button_clicked_cb, intf);
        }
        elm_object_part_content_set(nf_content, "title_right_btn", intf->popup_toggle_btn);
    }
}

/* Video Player */
static void
intf_video_player_create(interface *intf)
{
    /* Prepare the media player */
    playback_service *p_ps = application_get_playback_service(intf->p_app);

    intf->video_player = create_video_player(intf, p_ps, intf->nf_content);
}

void
intf_video_player_play(interface *intf, const char *psz_path)
{
    if(intf->video_player == NULL)
        intf_video_player_create(intf);

    Evas_Object *view = intf->video_player->view;

    /* Push it on top of the view */
    Elm_Object_Item *it = elm_naviframe_item_push(intf->nf_content, NULL, NULL, NULL, view, NULL);
    elm_naviframe_item_title_enabled_set(it, EINA_FALSE, EINA_FALSE);

    /* */
    evas_object_show(view);

    video_player_start(intf->video_player->p_view_sys, psz_path);

    /* We want fullscreen */
    elm_win_indicator_mode_set(intf->win, ELM_WIN_INDICATOR_HIDE);
}

/* Mini Player */
bool
intf_mini_player_visible_get(interface *intf)
{
    if(intf->mini_player_layout == NULL) {
        LOGE("Mini Player not existant!");
        return false;
    }

    return evas_object_visible_get(intf->mini_player_layout);
}

bool
intf_mini_player_visible_set(interface *intf, bool visible)
{
    if(intf->mini_player_layout == NULL)
    {
        LOGE("Mini Player not existant!");
        return false;
    }
    if(visible)
    {
        /* show */
        elm_box_pack_end(intf->main_box, intf->mini_player_layout);
        evas_object_show(intf->mini_player_layout);
        elm_box_recalculate(intf->main_box);
    }
    else
    {
        elm_box_unpack(intf->main_box, intf->mini_player_layout);
        evas_object_hide(intf->mini_player_layout);
        elm_box_recalculate(intf->main_box);
    }
    evas_object_image_source_visible_set(intf->mini_player_layout, visible);
    return true;
}

//FIXME REMOVE
void
intf_update_mini_player(interface *intf)
{
    if((mini_player_play_state(intf->p_mini_player) == true) && (intf_mini_player_visible_get(intf) == false))
    {
        intf_mini_player_visible_set(intf, true);
    }

    if((mini_player_play_state(intf->p_mini_player) == false) && (mini_player_fs_state(intf->p_mini_player) == true))
    {
        mini_player_stop(intf->p_mini_player);
    }
}

void
intf_create_audio_player(interface *intf, const char *psz_path)
{
    create_base_player(intf->p_mini_player, psz_path);
}

static Evas_Object*
create_main_box(interface *intf, Evas_Object *parent)
{
    /* Create a content box to display the content and the mini player */
    intf->main_box = elm_box_add(parent);
    elm_box_horizontal_set(intf->main_box, EINA_FALSE);

    /* Main View Naviframe */
    intf->nf_content = elm_naviframe_add(intf->main_box);

    /* Put the naviframe at the top of the content_box */
    evas_object_size_hint_weight_set(intf->nf_content, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(intf->nf_content, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_box_pack_end(intf->main_box, intf->nf_content);

    /* Mini Player creation */
    intf->mini_player_layout = elm_layout_add(intf->main_box);
    intf->p_mini_player = mini_player_create(intf, application_get_playback_service(intf->p_app), intf->mini_player_layout);
    evas_object_hide(intf->mini_player_layout);

    /* */
    evas_object_show(intf->nf_content);

    /* Ask the global box to recalculate her current children display */
    elm_box_recalculate(intf->main_box);

    return intf->main_box;
}

static void
create_main_layout(interface *intf, Evas_Object *conform)
{
    /* Add a layout to the conformant */
    Evas_Object *layout = elm_layout_add(conform);
    elm_layout_theme_set(layout, "layout", "drawer", "panel");
    evas_object_show(layout);

    /* Create the panel and put it in the layout */
    intf->sidebar = create_sidebar(intf, layout);
    elm_object_part_content_set(layout, "elm.swallow.left", intf->sidebar);

    /* Create the content box and put it in the layout */
    intf->main_box = create_main_box(intf, layout);
    elm_object_part_content_set(layout, "elm.swallow.content", intf->main_box);

    /* */
    evas_object_show(intf->main_box);

    elm_object_content_set(conform, layout);
}

#define VLC_ORANGE_500 255, 136, 0, 255
#define VLC_ORANGE_500_TRANSPARENT 255, 136, 0, 180
#define VLC_GREY_400_TRANSPARENT 189, 189, 189, 128
#define EDJE_COLOR_CLASS_SET_VLC_COLOR(x, b) edje_color_class_set((x), b, b, b)
#define EDJE_COLOR_CLASS_SET_VLC_ORANGE(x) EDJE_COLOR_CLASS_SET_VLC_COLOR(x, VLC_ORANGE_500)

interface *
intf_create(application *app)
{
    interface *intf = calloc(1, sizeof(*intf));
    intf->p_app = app;

#ifdef __arm__
    /* no opengl for emulator */
    elm_config_accel_preference_set("opengl");
#endif

    /* Add and set the main Window */
    intf->win = elm_win_util_standard_add(PACKAGE, PACKAGE);
    elm_win_autodel_set(intf->win, EINA_TRUE);

    /* Change colors */
    EDJE_COLOR_CLASS_SET_VLC_ORANGE("B011");    // Base class
    EDJE_COLOR_CLASS_SET_VLC_ORANGE("B0511");   // Naviframe base
    EDJE_COLOR_CLASS_SET_VLC_ORANGE("B0514");   // Naviframe tab bar
    EDJE_COLOR_CLASS_SET_VLC_ORANGE("B0514S");  // Naviframe tab bar
    EDJE_COLOR_CLASS_SET_VLC_ORANGE("B0514P");  // Naviframe tab bar
    EDJE_COLOR_CLASS_SET_VLC_ORANGE("B0517");   // Naviframe second

    /* Progress Bar Colors */
    EDJE_COLOR_CLASS_SET_VLC_COLOR("W062L1", VLC_GREY_400_TRANSPARENT);    // slider background
    EDJE_COLOR_CLASS_SET_VLC_COLOR("W062L2", VLC_ORANGE_500_TRANSPARENT);  // slider foreground
    EDJE_COLOR_CLASS_SET_VLC_COLOR("W0641P", VLC_ORANGE_500_TRANSPARENT);  // slider thumb pressed
    EDJE_COLOR_CLASS_SET_VLC_COLOR("W0641D", VLC_ORANGE_500_TRANSPARENT);  // slider thumb disabled
    EDJE_COLOR_CLASS_SET_VLC_ORANGE("W0641");   // slider thumb

    /* Handle rotations */
    if (elm_win_wm_rotation_supported_get(intf->win)) {
        int rots[4] = { 0, 90, 180, 270 };
        elm_win_wm_rotation_available_rotations_set(intf->win, (const int *)(&rots), 4);
    }

    /* Handle back buttons and delete callbacks */
    evas_object_smart_callback_add(intf->win, "delete,request", win_delete_request_cb, NULL);
    eext_object_event_callback_add(intf->win, EEXT_CALLBACK_BACK, win_back_key_cb, intf);
    eext_object_event_callback_add(intf->win, EEXT_CALLBACK_MORE, right_panel_button_clicked_cb, intf);

    /* Add and set a conformant in the main Window */
    Evas_Object *conform = elm_conformant_add(intf->win);
    elm_win_conformant_set(intf->win, EINA_TRUE);
    evas_object_size_hint_weight_set(conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

    /* */
    elm_win_indicator_mode_set(intf->win, ELM_WIN_INDICATOR_SHOW);
    elm_win_indicator_opacity_set(intf->win, ELM_WIN_INDICATOR_OPAQUE);
    elm_win_resize_object_add(intf->win, conform);
    evas_object_show(conform);

    /* Add and set a bg in the conformant */
    Evas_Object *bg = elm_bg_add(conform);
    elm_bg_color_set(bg, 255, 136, 0);

    /* Add the bg in the conformant */
    elm_object_part_content_set(conform, "elm.swallow.indicator_bg", bg);
    evas_object_show(bg);

    /* Create the main view in the conformant */
    create_main_layout(intf, conform);

    /* Create the default view in the content naviframe */
    intf_show_view(intf, VIEW_AUTO);

    /* */
    evas_object_show(intf->win);
    return intf;
}

/* DESTRUCTION */
void
intf_destroy(interface *intf)
{
    /* Destroy the views */
    for(int i = 0; i< VIEW_MAX; i++)
        if(intf->nf_views[i] != NULL)
            interface_views[i].pf_destroy(intf->nf_views[i]);

    /* Video Player */
    if(intf->video_player != NULL)
        destroy_video_player(intf->video_player);

    if(intf->p_mini_player != NULL)
        ;//FIXME

    /* The window is the parent of all the objects:
    win, layout, main_box, nf_content, sidebar, sidebar_toggle_btn,
    popup, popup_toggle_btn, mini_player_layout, no need to free them */

    free(intf);
}
