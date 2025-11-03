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

#ifndef CPI_TRANSLATION_H
#define CPI_TRANSLATION_H

// Translation string IDs
typedef enum
{
    // Application constants
    STR_APP_NAME = 0,
    STR_APP_DESCRIPTION,
    STR_APP_AUDIO_FILE_DESC,
    STR_APP_PLAYLIST_DESC,
    
    // Menu items
    STR_MENU_OPEN,
    STR_MENU_OPEN_URL,
    STR_MENU_ADD,
    STR_MENU_PLAYLIST_EDITOR,
    STR_MENU_SKIN,
    STR_MENU_SKIN_DEFAULT,
    STR_MENU_LANGUAGE,
    STR_MENU_LANGUAGE_ENGLISH,
    STR_MENU_LANGUAGE_GERMAN,
    STR_MENU_PLAY_CONTROL,
    STR_MENU_PLAY,
    STR_MENU_STOP,
    STR_MENU_PAUSE,
    STR_MENU_NEXT,
    STR_MENU_PREVIOUS,
    STR_MENU_OPTIONS,
    STR_MENU_ABOUT,
    STR_MENU_EXIT,
    
    // Dialog titles
    STR_DLG_ABOUT_TITLE,
    STR_DLG_OPTIONS_TITLE,
    STR_DLG_URL_TITLE,
    STR_DLG_QUICKFIND_TITLE,
    
    // About dialog
    STR_ABOUT_TAB_ABOUT,
    STR_ABOUT_TAB_KEYBOARD,
    STR_ABOUT_TAB_CHANGES,
    STR_ABOUT_CLOSE,
    
    // Options dialog - General section
    STR_OPTIONS_GENERAL,
    STR_OPTIONS_ALWAYS_ON_TOP,
    STR_OPTIONS_EXIT_AFTER_PLAYING,
    STR_OPTIONS_ROTATE_SYSTRAY,
    STR_OPTIONS_SCROLL_TITLE,
    STR_OPTIONS_FILE_ONCE_PLAYLIST,
    STR_OPTIONS_AUTOPLAY_STARTUP,
    STR_OPTIONS_MULTIPLE_INSTANCES,
    STR_OPTIONS_SHOW_REMAINING_TIME,
    STR_OPTIONS_SHOW_ON_TASKBAR,
    STR_OPTIONS_REGISTER_FILETYPES,
    STR_OPTIONS_ADD_START_MENU,
    STR_OPTIONS_READ_ID3_TAG,
    STR_OPTIONS_READ_ID3_SELECTED,
    STR_OPTIONS_SUPPORT_ID3V2,
    STR_OPTIONS_PREFER_NATIVE_OGG,
    STR_OPTIONS_READ_ID3_BACKGROUND,
    STR_OPTIONS_WORK_OUT_LENGTHS,
    STR_OPTIONS_EASY_MOVE,
    STR_OPTIONS_REMEMBER_PLAYLIST,
    STR_OPTIONS_REMEMBER_LAST_PLAYED,
    STR_OPTIONS_TRACK_DELAY_SEC,
    STR_OPTIONS_SKINLIST_LENGTH,
    STR_OPTIONS_FLUSH,
    
    // Options dialog - Output section
    STR_OPTIONS_OUTPUT,
    STR_OPTIONS_VOLUME_CONTROLS,
    
    // Options dialog - Skin section
    STR_OPTIONS_SKIN,
    STR_OPTIONS_PLAYER,
    STR_OPTIONS_OPEN,
    
    // Options dialog - Buttons
    STR_OPTIONS_OK,
    STR_OPTIONS_CANCEL,
    
    // URL dialog
    STR_URL_DESCRIPTION,
    STR_URL_LABEL,
    
    // Quick Find dialog
    STR_QUICKFIND_TEXT,
    STR_QUICKFIND_LOOK_IN,
    STR_QUICKFIND_TITLES,
    STR_QUICKFIND_ARTISTS,
    STR_QUICKFIND_ALBUMS,
    STR_QUICKFIND_CLOSE,
    
    // Volume control options
    STR_VOLUME_SYSTEM_MASTER,
    STR_VOLUME_SYSTEM_WAVE,
    STR_VOLUME_INTERNAL,
    
    // Message box messages
    STR_MSG_FILETYPES_REGISTERED,
    STR_MSG_ICONS_CREATED,
    
    // File filters
    STR_FILTER_SKIN_FILES,
    STR_FILTER_ALL_FILES,
    
    // Keep this last for array sizing
    STR_COUNT
} CPT_StringID;

// Translation system functions
void CPT_Initialize(void);
void CPT_Cleanup(void);
BOOL CPT_LoadLanguage(const char* languageCode);
const char* CPT_GetString(CPT_StringID stringID);
const wchar_t* CPT_GetStringW(CPT_StringID stringID);  // Unicode version
void CPT_SetDefaultLanguage(const char* languageCode);
const char* CPT_GetCurrentLanguage(void);
BOOL CPT_LanguageFileExists(const char* languageCode);

// Language enumeration and information functions
int CPT_EnumerateLanguages(char languages[][16], int maxLanguages);
const char* CPT_GetLanguageName(const char* languageCode);

// Convenience macros for getting translated strings
#define T(id) CPT_GetString(id)
#define TW(id) CPT_GetStringW(id)  // Unicode version

// Dialog sizing functions for different languages
typedef struct {
    int width;
    int height;
} DialogSize;

DialogSize CPT_GetDialogSize(const char* dialogName);

#endif // CPI_TRANSLATION_H