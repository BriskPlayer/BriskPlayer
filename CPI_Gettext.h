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

#ifndef CPI_GETTEXT_H
#define CPI_GETTEXT_H

#include "c23_compat.h"
#include <locale.h>

#ifdef ENABLE_NLS
    #ifdef __cplusplus
        // For C++: Declare libintl functions and disable format-arg warnings
        extern "C" {
            extern char* libintl_gettext(const char*);
            extern char* libintl_dgettext(const char*, const char*);
            extern char* libintl_dcgettext(const char*, const char*, int);
            extern char* libintl_ngettext(const char*, const char*, unsigned long);
            extern char* libintl_dngettext(const char*, const char*, const char*, unsigned long);
            extern char* libintl_dcngettext(const char*, const char*, const char*, unsigned long, int);
            extern char* libintl_textdomain(const char*);
            extern char* libintl_bindtextdomain(const char*, const char*);
            extern char* libintl_bind_textdomain_codeset(const char*, const char*);
        }
        
        // Disable the problematic diagnostic for macro definitions
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wattributes"
        
        // Use the libintl_ functions directly
        #define _(String) libintl_gettext(String)
        #define N_(String) String
        #define P_(Singular, Plural, N) libintl_ngettext(Singular, Plural, N)
        #define D_(Domain, String) libintl_dgettext(Domain, String)
        #define DC_(Domain, String, Category) libintl_dcgettext(Domain, String, Category)
        #define C_(Context, String) libintl_gettext(String)
        #define CP_(Context, Singular, Plural, N) libintl_ngettext(Singular, Plural, N)
        
        #pragma GCC diagnostic pop
        
        // Also define these for direct use in C++ files
        #define textdomain libintl_textdomain
        #define bindtextdomain libintl_bindtextdomain
        #define bind_textdomain_codeset libintl_bind_textdomain_codeset
    #else
        // For C: include libintl.h normally
        #include <libintl.h>
        
        // Standard C macros
        #define _(String) gettext(String)
        #define N_(String) String
        #define P_(Singular, Plural, N) ngettext(Singular, Plural, N)
        #define D_(Domain, String) dgettext(Domain, String)
        #define DC_(Domain, String, Category) dcgettext(Domain, String, Category)
        #define C_(Context, String) gettext(String)
        #define CP_(Context, Singular, Plural, N) ngettext(Singular, Plural, N)
    #endif
    
#else
    // Fallback macros when NLS is disabled
    #define _(String) String
    #define N_(String) String
    #define P_(Singular, Plural, N) ((N) == 1 ? Singular : Plural)
    #define D_(Domain, String) String
    #define DC_(Domain, String, Category) String
    #define C_(Context, String) String
    #define CP_(Context, Singular, Plural, N) P_(Singular, Plural, N)
#endif

// Application domain for translations
#define BRISKPLAYER_DOMAIN "briskplayer"

// Compatibility macros for old translation system
#define T(String) _(String)
#define TW(String) CPG_GetTranslationW(String)
#define CPT_GetCurrentLanguage() CPG_GetCurrentLanguage()
#define CPT_LoadLanguage(lang) (CPG_SetLanguage(lang), TRUE)
#define CPT_LanguageFileExists(lang) TRUE  // Simplified for compatibility
#define CPT_Initialize() Translation_Initialize()
#define CPT_SetDefaultLanguage(lang) CPG_SetLanguage(lang)
#define CPT_Cleanup() CPG_Cleanup()

// Translation context constants for better organization
#define CTX_MENU "menu"
#define CTX_DIALOG "dialog"
#define CTX_OPTIONS "options"
#define CTX_STATUS "status"
#define CTX_ERROR "error"
#define CTX_AUDIO "audio"

// String constants for translation compatibility
#define STR_APP_NAME "BriskPlayer"
#define STR_OPTIONS_TRACK_DELAY_SEC "Track delay (seconds)"
#define STR_OPTIONS_SKINLIST_LENGTH "Skin list length"
#define STR_OPTIONS_OUTPUT "Output"
#define STR_OPTIONS_VOLUME_CONTROLS "Volume controls"
#define STR_OPTIONS_SKIN "Skin"
#define STR_DLG_URL_TITLE "Open URL"
#define STR_URL_DESCRIPTION "Enter the URL of the stream to play"
#define STR_URL_LABEL "URL:"
#define STR_OPTIONS_OK "OK"
#define STR_OPTIONS_CANCEL "Cancel"

// Menu strings
#define STR_MENU_OPEN "Open File..."
#define STR_MENU_OPEN_URL "Open URL..."
#define STR_MENU_ADD "Add Files..."
#define STR_MENU_PLAYLIST_EDITOR "Playlist Editor"
#define STR_MENU_OPTIONS "Options..."
#define STR_MENU_ABOUT "About"
#define STR_MENU_EXIT "Exit"
#define STR_MENU_PLAY "Play"
#define STR_MENU_STOP "Stop"
#define STR_MENU_PAUSE "Pause"
#define STR_MENU_NEXT "Next"
#define STR_MENU_PREVIOUS "Previous"
#define STR_MENU_SKIN_DEFAULT "Default"
#define STR_MENU_SKIN "Skin"
#define STR_MENU_LANGUAGE "Language"
#define STR_MENU_PLAY_CONTROL "Playback"

// Dialog strings
#define STR_DLG_OPTIONS_TITLE "Options"
#define STR_OPTIONS_ALWAYS_ON_TOP "Always on top"
#define STR_OPTIONS_EXIT_AFTER_PLAYING "Exit after playing"
#define STR_OPTIONS_ROTATE_SYSTRAY "Rotate system tray icon"
#define STR_OPTIONS_SCROLL_TITLE "Scroll title"
#define STR_OPTIONS_FILE_ONCE_PLAYLIST "Only add file once to playlist"
#define STR_OPTIONS_AUTOPLAY_STARTUP "Autoplay on startup"
#define STR_OPTIONS_MULTIPLE_INSTANCES "Allow multiple instances"
#define STR_OPTIONS_SHOW_REMAINING_TIME "Show remaining time"
#define STR_OPTIONS_SHOW_ON_TASKBAR "Show on taskbar"
#define STR_OPTIONS_STICKY_WINDOWS "Sticky windows"
#define STR_OPTIONS_REGISTER_FILETYPES "Register file types"
#define STR_OPTIONS_ADD_START_MENU "Add to start menu"
#define STR_OPTIONS_READ_ID3_TAG "Read ID3 tags"
#define STR_OPTIONS_READ_ID3_SELECTED "Read ID3 for selected files"
#define STR_OPTIONS_SUPPORT_ID3V2 "Support ID3v2"
#define STR_OPTIONS_PREFER_NATIVE_OGG "Prefer native OGG tags"
#define STR_OPTIONS_READ_ID3_BACKGROUND "Read ID3 in background"
#define STR_OPTIONS_WORK_OUT_LENGTHS "Calculate track lengths"
#define STR_OPTIONS_EASY_MOVE "Easy move mode"
#define STR_OPTIONS_REMEMBER_PLAYLIST "Remember playlist"
#define STR_OPTIONS_REMEMBER_LAST_PLAYED "Remember last played"
#define STR_OPTIONS_FLUSH "Flush"
#define STR_OPTIONS_PLAYER "Player"
#define STR_OPTIONS_OPEN "Open..."

// Volume control strings
#define STR_VOLUME_SYSTEM_MASTER "System Master Volume"
#define STR_VOLUME_SYSTEM_WAVE "System Wave Volume"
#define STR_VOLUME_INTERNAL "Internal Volume"

// Filter strings
#define STR_FILTER_SKIN_FILES "Skin Files (*.ini)"
#define STR_FILTER_ALL_FILES "All Files (*.*)"

// Application descriptions
#define STR_APP_AUDIO_FILE_DESC "Audio File"
#define STR_APP_PLAYLIST_DESC "Playlist File"

// Message strings
#define STR_MSG_FILETYPES_REGISTERED "File types registered successfully"
#define STR_MSG_ICONS_CREATED "Icons created successfully"

// Enhanced gettext initialization with C23 features
typedef struct {
    const char* domain;
    const char* directory;
    const char* locale_category;
    bool use_utf8;
    bool fallback_enabled;
} GetTextConfig;

// Default config - C++ compatible initialization
#ifdef __cplusplus
static const GetTextConfig DEFAULT_GETTEXT_CONFIG = {
    BRISKPLAYER_DOMAIN,  // domain
    "./locale",          // directory
    "LC_ALL",            // locale_category
    true,                // use_utf8
    true                 // fallback_enabled
};
#else
// C designated initializer
static const GetTextConfig DEFAULT_GETTEXT_CONFIG = {
    .domain = BRISKPLAYER_DOMAIN,
    .directory = "./locale",
    .locale_category = "LC_ALL",
    .use_utf8 = true,
    .fallback_enabled = true
};
#endif

// Function declarations
BOOL CPG_Initialize(const GetTextConfig* config);
void CPG_SetLanguage(const char* language);
const char* CPG_GetCurrentLanguage(void);
BOOL CPG_LoadDomain(const char* domain, const char* directory);
void CPG_Cleanup(void);

// Language enumeration
typedef struct {
    char code[8];      // Language code (e.g., "en", "de", "fr")
    char name[64];     // Native language name (e.g., "English", "Deutsch")
    char region[32];   // Region if applicable (e.g., "US", "CA", "AT")
    bool available;    // Whether translation files exist
} LanguageInfo;

int CPG_EnumerateLanguages(LanguageInfo* languages, int maxLanguages);
BOOL CPG_IsLanguageAvailable(const char* languageCode);

// Utility functions for Windows integration
#ifdef _WIN32
    const char* CPG_GetSystemLanguage(void);
    BOOL CPG_SetWindowsLocale(const char* languageCode);
    wchar_t* CPG_GetTranslationW(const char* msgid);
#endif

// Thread-safe translation with thread_local storage
#ifdef __cplusplus
    extern thread_local char* tl_translation_buffer;
#else
    extern _Thread_local char* tl_translation_buffer;
#endif
const char* CPG_GetTranslationThreadSafe(const char* msgid);

// Audio-specific translation helpers
#define AUDIO_FMT_TIME(hours, minutes, seconds) \
    P_("Time format", "%d:%02d:%02d", hours > 0 ? 3 : 2), hours, minutes, seconds

#define AUDIO_FMT_BITRATE(bitrate) \
    C_("audio", "Bitrate: %d kbps"), bitrate

#define AUDIO_FMT_SAMPLERATE(rate) \
    C_("audio", "Sample rate: %.1f kHz"), (float)rate / 1000.0f

// Error handling with context
#define ERROR_MSG(context, msg) DC_(BRISKPLAYER_DOMAIN, C_(context, msg), LC_MESSAGES)

// Menu translation helpers with keyboard shortcuts
#define MENU_ITEM(key, label) C_(CTX_MENU, label "\t" key)
#define DIALOG_TITLE(title) C_(CTX_DIALOG, title)
#define OPTION_LABEL(label) C_(CTX_OPTIONS, label)

#endif // CPI_GETTEXT_H
