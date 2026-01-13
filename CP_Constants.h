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

#ifndef CP_CONSTANTS_H
#define CP_CONSTANTS_H

////////////////////////////////////////////////////////////////////////////////
//
// Centralized Constants for BriskPlayer
//
// This header consolidates all magic numbers, limits, and configuration
// constants used throughout the codebase. Constants are organized by category.
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Timer IDs - Used with SetTimer/KillTimer
////////////////////////////////////////////////////////////////////////////////

#define CPC_TIMER_SCROLL_TITLE      1    // Title scrolling animation
#define CPC_TIMER_TRACK_POSITION    2    // Track position update
#define CPC_TIMER_SYSTRAY_ANIMATE   3    // System tray icon animation
#define CPC_TIMER_ABOUT_ANIMATION   4    // About dialog animation
#define CPC_TIMER_EQUALIZER_UPDATE  5    // Equalizer visualization
#define CPC_TIMER_STREAMING_CHECK   6    // Streaming buffer check
#define CPC_TIMER_TOOLTIP_DELAY     7    // Tooltip show delay

////////////////////////////////////////////////////////////////////////////////
// Timer Intervals (milliseconds)
////////////////////////////////////////////////////////////////////////////////

#define CPC_INTERVAL_SCROLL_TITLE       100     // Title scroll speed
#define CPC_INTERVAL_TRACK_POSITION     500     // Half-second position update
#define CPC_INTERVAL_SYSTRAY_FAST       50      // Fast tray animation
#define CPC_INTERVAL_SYSTRAY_SLOW       100     // Normal tray animation
#define CPC_INTERVAL_ABOUT_ANIMATION    30      // ~33 FPS for about dialog
#define CPC_INTERVAL_EQUALIZER          50      // EQ visualization update
#define CPC_INTERVAL_STREAMING          1000    // 1 second stream check
#define CPC_INTERVAL_TOOLTIP            500     // Tooltip delay

////////////////////////////////////////////////////////////////////////////////
// Window & UI Constants
////////////////////////////////////////////////////////////////////////////////

// Window snapping
#define CPC_SNAP_THRESHOLD          10      // Pixels for window snapping
#define CPC_SNAP_ZONE               20      // Detection zone for snapping

// Scrolling
#define CPC_SCROLL_STEP             3       // Pixels per scroll step
#define CPC_SCROLL_PAUSE_TICKS      10      // Pause cycles at ends

// Transparency
#define CPC_ALPHA_OPAQUE            255     // Fully opaque
#define CPC_ALPHA_TRANSPARENT       0       // Fully transparent
#define CPC_ALPHA_DEFAULT           230     // Slight transparency

// Playlist
#define CPC_PLAYLIST_MIN_WIDTH      200     // Minimum playlist width
#define CPC_PLAYLIST_MIN_HEIGHT     100     // Minimum playlist height
#define CPC_PLAYLIST_ITEM_HEIGHT    18      // Default row height
#define CPC_PLAYLIST_HEADER_HEIGHT  20      // Column header height

////////////////////////////////////////////////////////////////////////////////
// Audio Engine Constants
////////////////////////////////////////////////////////////////////////////////

// Buffer sizes
#define CPC_AUDIO_BUFFER_SIZE       8192    // Samples per buffer
#define CPC_AUDIO_BUFFER_COUNT      4       // Number of output buffers
#define CPC_STREAM_BUFFER_SIZE      65536   // 64KB streaming buffer

// Seek constants
#define CPC_SEEK_STEP_SMALL         5       // Small seek (seconds)
#define CPC_SEEK_STEP_LARGE         30      // Large seek (seconds)

// Volume
#define CPC_VOLUME_MIN              0       // Minimum volume
#define CPC_VOLUME_MAX              100     // Maximum volume
#define CPC_VOLUME_STEP             5       // Volume adjustment step
#define CPC_VOLUME_MUTE_THRESHOLD   1       // Below this is muted

// Equalizer
#define CPC_EQ_BAND_COUNT           8       // Number of EQ bands
#define CPC_EQ_LEVEL_MIN            -12     // Minimum dB
#define CPC_EQ_LEVEL_MAX            12      // Maximum dB
#define CPC_EQ_LEVEL_DEFAULT        0       // Flat response

////////////////////////////////////////////////////////////////////////////////
// File & Path Constants
////////////////////////////////////////////////////////////////////////////////

// String buffer sizes (prefer these over raw numbers)
#define CPC_PATH_BUFFER             MAX_PATH        // File paths
#define CPC_TITLE_BUFFER            256             // Window/track titles
#define CPC_TOOLTIP_BUFFER          256             // Tooltip text
#define CPC_ERROR_BUFFER            512             // Error messages
#define CPC_FORMAT_BUFFER           128             // Format strings
#define CPC_TAG_BUFFER              256             // ID3/metadata tags
#define CPC_URL_BUFFER              2048            // URLs
#define CPC_INI_VALUE_BUFFER        256             // INI file values
#define CPC_FILTER_BUFFER           1024            // File dialog filters

// File type limits
#define CPC_MAX_EXTENSIONS          64              // Max file extensions
#define CPC_MAX_PLAYLIST_ITEMS      50000           // Max playlist tracks
#define CPC_MAX_RECENT_FILES        10              // Recent files list
#define CPC_MAX_SKIN_HISTORY        10              // Remembered skins

////////////////////////////////////////////////////////////////////////////////
// Skin System Constants
////////////////////////////////////////////////////////////////////////////////

// Skin element indices (matching enum Objects in skin.h)
// These are used as array indices - do not change values without
// updating the Objects enum

#define CPC_SKIN_MAX_OBJECTS        32              // Maximum skin elements
#define CPC_SKIN_TOOLTIP_LENGTH     100             // Tooltip text max
#define CPC_SKIN_PATH_LENGTH        MAX_PATH        // Skin file paths

// Skin color defaults
#define CPC_SKIN_TRANSPARENT_COLOR  RGB(255, 0, 255)    // Default magenta

////////////////////////////////////////////////////////////////////////////////
// Network Constants
////////////////////////////////////////////////////////////////////////////////

#define CPC_NET_CONNECT_TIMEOUT     15000           // 15 second connect
#define CPC_NET_READ_TIMEOUT        30000           // 30 second read
#define CPC_NET_BUFFER_SIZE         8192            // 8KB network buffer
#define CPC_NET_USER_AGENT          "BriskPlayer/2.0"
#define CPC_NET_MAX_REDIRECTS       5               // Max HTTP redirects

// Streaming
#define CPC_STREAM_PREBUFFER_SIZE   65536           // 64KB prebuffer
#define CPC_STREAM_MIN_BUFFER       16384           // 16KB minimum
#define CPC_STREAM_ICY_METADATA_INT 8192            // ICY metadata interval

////////////////////////////////////////////////////////////////////////////////
// Limits & Validation
////////////////////////////////////////////////////////////////////////////////

// Input validation
#define CPC_MIN_DELAY_TIME          0               // Minimum track delay (ms)
#define CPC_MAX_DELAY_TIME          10000           // Maximum track delay (ms)
#define CPC_MIN_SCROLL_SPEED        50              // Minimum scroll interval
#define CPC_MAX_SCROLL_SPEED        500             // Maximum scroll interval

// Resource limits
#define CPC_MAX_MENU_ITEMS          100             // Max dynamic menu items
#define CPC_MAX_HOTKEYS             50              // Max keyboard shortcuts
#define CPC_MAX_CODECS              16              // Max loaded codecs
#define CPC_MAX_OUTPUT_DEVICES      16              // Max audio devices

////////////////////////////////////////////////////////////////////////////////
// Feature Flags (can be overridden at compile time)
////////////////////////////////////////////////////////////////////////////////

#ifndef CPC_ENABLE_STREAMING
#define CPC_ENABLE_STREAMING        1               // Enable internet streams
#endif

#ifndef CPC_ENABLE_EQUALIZER
#define CPC_ENABLE_EQUALIZER        1               // Enable EQ processing
#endif

#ifndef CPC_ENABLE_LYRICS
#define CPC_ENABLE_LYRICS           0               // Lyrics display (future)
#endif

#ifndef CPC_ENABLE_VISUALIZATIONS
#define CPC_ENABLE_VISUALIZATIONS   0               // Visualizations (future)
#endif

////////////////////////////////////////////////////////////////////////////////
// Debug & Logging
////////////////////////////////////////////////////////////////////////////////

#define CPC_LOG_BUFFER_SIZE         4096            // Log message buffer
#define CPC_LOG_MAX_FILE_SIZE       (10 * 1024 * 1024)  // 10MB max log file

////////////////////////////////////////////////////////////////////////////////
// Version Info
////////////////////////////////////////////////////////////////////////////////

#define CPC_VERSION_MAJOR           2
#define CPC_VERSION_MINOR           0
#define CPC_VERSION_PATCH           0
#define CPC_VERSION_STRING          "2.0.0"
#define CPC_VERSION_CODENAME        "Brisk"

////////////////////////////////////////////////////////////////////////////////

#endif // CP_CONSTANTS_H
