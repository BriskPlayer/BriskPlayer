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

// String constants for translation compatibility
#define STR_APP_NAME N_("BriskPlayer")

// Menu strings
#define STR_MENU_OPEN N_("Open File...")
#define STR_MENU_OPEN_URL N_("Open URL...")
#define STR_MENU_ADD N_("Add Files...")
#define STR_MENU_PLAYLIST_EDITOR N_("Playlist Editor")
#define STR_MENU_OPTIONS N_("Options...")
#define STR_MENU_ABOUT N_("About")
#define STR_MENU_EXIT N_("Exit")
#define STR_MENU_PLAY N_("Play")
#define STR_MENU_STOP N_("Stop")
#define STR_MENU_PAUSE N_("Pause")
#define STR_MENU_NEXT N_("Next")
#define STR_MENU_PREVIOUS N_("Previous")
#define STR_MENU_SKIN_DEFAULT N_("Default")
#define STR_MENU_SKIN N_("Skin")
#define STR_MENU_LANGUAGE N_("Language")
#define STR_MENU_PLAY_CONTROL N_("Playback")
#define STR_MENU_DSP_PLUGINS N_("DSP Plugins")
#define STR_MENU_EQUALIZER N_("Equalizer")
#define STR_DSP_NONE N_("None (Disabled)")
#define STR_DSP_CONFIGURE N_("Configure...")

// Dialog strings
#define STR_REPLAYGAIN_OFF N_("Off")
#define STR_REPLAYGAIN_TRACK N_("Track")
#define STR_REPLAYGAIN_ALBUM N_("Album")
#define STR_OPTIONS_SKINS_FOLDER N_("Skins Folder")

// Volume control strings
#define STR_VOLUME_SYSTEM_MASTER N_("System Master Volume")
#define STR_VOLUME_SYSTEM_WAVE N_("System Wave Volume")
#define STR_VOLUME_INTERNAL N_("Internal Volume")

// Filter strings

// Application descriptions

// Message strings

// Error messages
#define STR_ERR_INVALID_SKIN N_("Not a valid BriskPlayer Skin file")
#define STR_ERR_CANT_LOAD_BITMAPS N_("Can't load bitmaps!")
#define STR_ERR_NO_AUDIO_DEVICES N_("No audio devices in this system")
#define STR_ERR_FAILED_CREATE_PLAYLIST N_("Failed to create playlist")
#define STR_ERR_COULD_NOT_OPEN_FILE N_("Could not open file")
#define STR_ERR_NOT_VALID_DIRECTORY N_("Not a valid directory.")
#define STR_ERR_COULD_NOT_SCAN N_("Could not perform scan")
#define STR_ERR_CANNOT_UPDATE_TAG N_("This file's ID3 tag cannot be updated. This is because BriskPlayer cannot write to this file.")
#define STR_ERR_CANNOT_UPDATE_TAG_TITLE N_("Cannot update tag")
#define STR_ERR_ERROR N_("Error")
#define STR_ERR_RENAME_FAILED N_("Some files could not be renamed.\n\nThis could be because they are either currently playing or are read-only.")

// File filter strings for file dialogs

// Browse dialog strings

// Language menu

// Command line help
#define STR_CMDLINE_HELP_TITLE N_("BriskPlayer command line options")

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
    // Translates a dialog's title and its label/button text in place, using
    // the English text from briskplayer.rc as msgid. Call first thing in
    // WM_INITDIALOG, before any control is filled with runtime data.
    void CPG_TranslateDialog(HWND hDlg);
#endif

// Thread-safe translation with thread_local storage
#ifdef __cplusplus
    extern thread_local char* tl_translation_buffer;
#else
    extern _Thread_local char* tl_translation_buffer;
#endif
const char* CPG_GetTranslationThreadSafe(const char* msgid);

#endif // CPI_GETTEXT_H
