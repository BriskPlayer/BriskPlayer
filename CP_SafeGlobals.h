/*
 * BriskPlayer - Blazing fast audio player.
 * Copyright (C) 2000-2001 Niek Albers
 * Copyright (C) 2025 Zach Bacon
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef CP_SAFE_GLOBALS_H
#define CP_SAFE_GLOBALS_H

////////////////////////////////////////////////////////////////////////////////
//
// Safe Global Access Macros
// 
// These macros provide NULL-safe access to global handles that may not be
// initialized during startup or may have been destroyed during shutdown.
//
// Usage:
//   SAFE_PLAYER_CALL(CPI_Player__Play);           // void return
//   SAFE_PLAYER_CALL2(CPI_Player__SetVolume, 50); // with extra arg
//   if (SAFE_HAS_PLAYER()) { ... }                // check availability
//
////////////////////////////////////////////////////////////////////////////////

#include "globals.h"

//------------------------------------------------------------------------------
// Player handle safety macros
//------------------------------------------------------------------------------

// Check if player is available
#define SAFE_HAS_PLAYER() (globals.m_hPlayer != NULL)

// Call a player function with no extra args (just the player handle)
#define SAFE_PLAYER_CALL(func) \
    do { if (globals.m_hPlayer) { func(globals.m_hPlayer); } } while(0)

// Call a player function with 1 extra arg
#define SAFE_PLAYER_CALL1(func, arg1) \
    do { if (globals.m_hPlayer) { func(globals.m_hPlayer, (arg1)); } } while(0)

// Call a player function with 2 extra args
#define SAFE_PLAYER_CALL2(func, arg1, arg2) \
    do { if (globals.m_hPlayer) { func(globals.m_hPlayer, (arg1), (arg2)); } } while(0)

// Call a player function with 3 extra args
#define SAFE_PLAYER_CALL3(func, arg1, arg2, arg3) \
    do { if (globals.m_hPlayer) { func(globals.m_hPlayer, (arg1), (arg2), (arg3)); } } while(0)

//------------------------------------------------------------------------------
// Playlist handle safety macros
//------------------------------------------------------------------------------

// Check if playlist is available
#define SAFE_HAS_PLAYLIST() (globals.m_hPlaylist != NULL)

// Call a playlist function with no extra args
#define SAFE_PLAYLIST_CALL(func) \
    do { if (globals.m_hPlaylist) { func(globals.m_hPlaylist); } } while(0)

// Call a playlist function with 1 extra arg
#define SAFE_PLAYLIST_CALL1(func, arg1) \
    do { if (globals.m_hPlaylist) { func(globals.m_hPlaylist, (arg1)); } } while(0)

// Call a playlist function with 2 extra args
#define SAFE_PLAYLIST_CALL2(func, arg1, arg2) \
    do { if (globals.m_hPlaylist) { func(globals.m_hPlaylist, (arg1), (arg2)); } } while(0)

// Call a playlist function with 3 extra args  
#define SAFE_PLAYLIST_CALL3(func, arg1, arg2, arg3) \
    do { if (globals.m_hPlaylist) { func(globals.m_hPlaylist, (arg1), (arg2), (arg3)); } } while(0)

//------------------------------------------------------------------------------
// Playlist View Control safety macros
//------------------------------------------------------------------------------

// Check if playlist view control is available
#define SAFE_HAS_PLAYLIST_VIEW() (globals.m_hPlaylistViewControl != NULL)

// Call a list view function with no extra args
#define SAFE_LISTVIEW_CALL(func) \
    do { if (globals.m_hPlaylistViewControl) { func(globals.m_hPlaylistViewControl); } } while(0)

// Call a list view function with 1 extra arg
#define SAFE_LISTVIEW_CALL1(func, arg1) \
    do { if (globals.m_hPlaylistViewControl) { func(globals.m_hPlaylistViewControl, (arg1)); } } while(0)

// Call a list view function with 2 extra args
#define SAFE_LISTVIEW_CALL2(func, arg1, arg2) \
    do { if (globals.m_hPlaylistViewControl) { func(globals.m_hPlaylistViewControl, (arg1), (arg2)); } } while(0)

//------------------------------------------------------------------------------
// Playlist Interface safety macros
//------------------------------------------------------------------------------

// Check if playlist interface is available
#define SAFE_HAS_PLAYLIST_IF() (windows.m_hifPlaylist != NULL)

// Call a playlist interface function with no extra args
#define SAFE_PLAYLIST_IF_CALL(func) \
    do { if (windows.m_hifPlaylist) { func(windows.m_hifPlaylist); } } while(0)

// Call a playlist interface function with 1 extra arg
#define SAFE_PLAYLIST_IF_CALL1(func, arg1) \
    do { if (windows.m_hifPlaylist) { func(windows.m_hifPlaylist, (arg1)); } } while(0)

//------------------------------------------------------------------------------
// Main window safety macros
//------------------------------------------------------------------------------

// Check if main window is available
#define SAFE_HAS_MAIN_WND() (windows.wnd_main != NULL && IsWindow(windows.wnd_main))

// Check if system icon is available
#define SAFE_HAS_SYSICON() (globals.m_hSysIcon != NULL)

//------------------------------------------------------------------------------
// Safe getter macros (return default value if NULL)
//------------------------------------------------------------------------------

// Get active playlist item, returns NULL if playlist unavailable
#define SAFE_GET_ACTIVE_ITEM() \
    (globals.m_hPlaylist ? CPL_GetActiveItem(globals.m_hPlaylist) : NULL)

// Get first playlist item, returns NULL if playlist unavailable
#define SAFE_GET_FIRST_ITEM() \
    (globals.m_hPlaylist ? CPL_GetFirstItem(globals.m_hPlaylist) : NULL)

// Get player volume, returns 0 if player unavailable
#define SAFE_GET_VOLUME() \
    (globals.m_hPlayer ? CPI_Player__GetVolume(globals.m_hPlayer) : 0)

#endif // CP_SAFE_GLOBALS_H
