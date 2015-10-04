/*****************************************************************************
 * Copyright © 2015 VideoLAN, VideoLabs SAS
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

#include "IMediaLibrary.h"

#include <Ecore.h>

#include "common.h"

#include "IVideoTrack.h"

#include "media_library_private.hpp"
#include "system_storage.h"


media_library::media_library()
    : ml( MediaLibraryFactory::create() )
    , m_nbDiscovery( 0 )
    , m_nbElemChanged( 0 )
{
    if ( ml == nullptr )
        throw std::runtime_error( "Failed to initialize MediaLibrary" );
}

void
media_library::onFileAdded( FilePtr file )
{
    //FIXME: This seems fishy if no discovery is in progress and some media gets updated.
    //This is very unlikely to happen for a while though.
    std::unique_lock<std::mutex> lock( m_mutex );
    if ( ++m_nbElemChanged >= 50 )
    {
        LOGI("Enough changes to trigger an update.");
        onChange();
        m_nbElemChanged = 0;
    }
}

void
media_library::onFileUpdated( FilePtr file )
{
    auto item = fileToMediaItem( file );
    auto item_ptr = std::unique_ptr<media_item, void(*)(media_item*)> ( item, &media_item_destroy );
    for ( auto& p : m_onItemUpdatedCb )
    {
        if ( p.first( p.second, item ) == true )
            break;
    }
}

void
media_library::onDiscoveryStarted( const std::string& entryPoint )
{
    LOGI( "Starting [%s] discovery", entryPoint.c_str() );
    std::unique_lock<std::mutex> lock( m_mutex );
    m_nbDiscovery++;
}

void
media_library::onDiscoveryCompleted( const std::string& entryPoint )
{
    LOGI("Completed [%s] discovery", entryPoint.c_str() );
    std::unique_lock<std::mutex> lock( m_mutex );
    if ( --m_nbDiscovery == 0 )
    {
        // If this is the last discovery, and some files got updated, send a final update

        if ( m_nbElemChanged != 0 )
        {
            m_nbElemChanged = 0;
            LOGI("Changes detected, sending update to listeners");
            onChange();
        }
        LOGI( "Completed all active discovery operations" );
    }
}

void
media_library::registerOnChange(media_library_file_list_changed_cb cb, void* cbUserData)
{
    m_onChangeCb.emplace_back(cb, cbUserData);
}

void
media_library::unregisterOnChange(media_library_file_list_changed_cb cb, void* cbUserData)
{
    auto ite = end(m_onChangeCb);
    for (auto it = begin(m_onChangeCb); it != ite; ++it)
    {
        if ((*it).first == cb && (*it).second == cb)
        {
            m_onChangeCb.erase(it);
            return;
        }
    }
}

void
media_library::onChange()
{
    for (auto &p : m_onChangeCb)
    {
        ecore_main_loop_thread_safe_call_async( p.first, p.second );
    }
}

void
media_library::registerOnItemUpdated(media_library_item_updated_cb cb, void* userData)
{
    m_onItemUpdatedCb.emplace_back( cb, userData );
}

void
media_library::unregisterOnItemUpdated(media_library_item_updated_cb cb, void* userData)
{
    auto ite = end(m_onItemUpdatedCb);
    for (auto it = begin(m_onItemUpdatedCb); it != ite; ++it)
    {
        if ((*it).first == cb && (*it).second == cb)
        {
            m_onItemUpdatedCb.erase(it);
            return;
        }
    }
}

media_library *
media_library_create(application *p_app)
{
    try
    {
        return new media_library;
    }
    catch (std::exception& ex)
    {
        LOGE( "%s", ex.what() );
        return nullptr;
    }
}

bool
media_library_start(media_library* p_media_library)
{
    auto appDataCStr = std::unique_ptr<char, void(*)(void*)>( system_storage_appdata_get(), &free );
    std::string appData( appDataCStr.get() );
    if ( appDataCStr == nullptr )
    {
        LOGE( "Failed to fetch application data directory" );
        return false;
    }
    // Always ensure the folder exists
    errno = 0;
    auto res = mkdir( appData.c_str(), 0700 );
    if ( res != 0 && errno != EEXIST)
    {
        LOGE("Failed to create data directory: %s", strerror(errno));
        return false;
    }
    auto snapshotPath = appData + "/snapshots";
    res = mkdir( snapshotPath.c_str(), 0700 );
    if ( res != 0 && errno != EEXIST)
    {
        LOGE("Failed to create snapshot directory: %s", strerror(errno));
        return false;
    }
    p_media_library->logger.reset( new TizenLogger );
    p_media_library->ml->setLogger( p_media_library->logger.get() );
    return p_media_library->ml->initialize( appData + "vlc.db", snapshotPath, p_media_library );
}

void
media_library_delete(media_library* p_media_library)
{
    delete p_media_library;
}

void
media_library_discover( media_library* p_ml, const char* psz_location )
{
    p_ml->ml->discover( psz_location );
}


struct ml_callback_context
{
    ml_callback_context( media_library* ml, media_library_list_cb c, void* p_user_data )
        : p_ml( ml ), cb( c ), list(nullptr), p_data( p_user_data ) {}
    media_library* p_ml;
    media_library_list_cb cb;
    Eina_List* list;
    void* p_data;
};

void
intermediate_list_callback( void* p_data )
{
    auto ctx = reinterpret_cast<ml_callback_context*>( p_data );
    ctx->cb( ctx->list, ctx->p_data );
    delete ctx;
}

void
media_library_get_audio_files( media_library* p_ml, media_library_list_cb cb, void* p_user_data )
{
    auto ctx = new ml_callback_context( p_ml, cb, p_user_data );

    ecore_thread_run( [](void* data, Ecore_Thread* ) {
        auto ctx = reinterpret_cast<ml_callback_context*>( data );
        auto files = ctx->p_ml->ml->audioFiles();
        Eina_List *list = nullptr;
        for ( auto& f : files )
        {
            auto elem = fileToMediaItem( f );
            if ( elem == nullptr )
                continue;
            list = eina_list_append( list, elem );
        }
        ctx->list = list;
        ecore_main_loop_thread_safe_call_async( intermediate_list_callback, ctx );
    }, nullptr, nullptr, ctx );
}

void
media_library_get_video_files( media_library* p_ml, media_library_list_cb cb, void* p_user_data )
{
    auto ctx = new ml_callback_context( p_ml, cb, p_user_data );

    ecore_thread_run( [](void* data, Ecore_Thread* ) {
        auto ctx = reinterpret_cast<ml_callback_context*>( data );
        auto files = ctx->p_ml->ml->videoFiles();
        Eina_List *list = nullptr;
        for ( auto& f : files )
        {
            auto elem = fileToMediaItem( f );
            if ( elem == nullptr )
                continue;
            list = eina_list_append( list, elem );
        }
        ctx->list = list;
        ecore_main_loop_thread_safe_call_async( intermediate_list_callback, ctx );
    }, nullptr, nullptr, ctx );
}

void
media_library_register_on_change(media_library* ml, media_library_file_list_changed_cb cb, void* p_data)
{
    ml->registerOnChange(cb, p_data);
}

void
media_library_unregister_on_change(media_library* ml, media_library_file_list_changed_cb cb, void* p_data)
{
    ml->unregisterOnChange(cb, p_data);
}

void
media_library_register_item_updated(media_library* ml, media_library_item_updated_cb cb, void* p_data )
{
    ml->registerOnItemUpdated(cb, p_data);
}

void
media_library_unregister_item_updated(media_library* ml, media_library_item_updated_cb cb, void* p_data )
{
    ml->unregisterOnItemUpdated(cb, p_data);
}