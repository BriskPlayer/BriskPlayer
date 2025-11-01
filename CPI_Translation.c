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

#include "stdafx.h"
#include "CPI_Translation.h"
#include <stdio.h>
#include <stdlib.h>

// Static variables
static char g_currentLanguage[8] = "";  // Initialize empty to trigger auto-detection
static char g_translationStrings[STR_COUNT][512];
static BOOL g_initialized = FALSE;

// Default English strings - fallback if INI file is not found
static const char* g_defaultStrings[STR_COUNT] = {
    // Application constants
    "BriskPlayer",                                    // STR_APP_NAME
    "BriskPlayer - Blazing fast audio player",       // STR_APP_DESCRIPTION
    "BriskPlayer Audio file",                         // STR_APP_AUDIO_FILE_DESC
    "BriskPlayer Playlist",                           // STR_APP_PLAYLIST_DESC
    
    // Menu items
    "&Open...\tL",                                   // STR_MENU_OPEN
    "Open &URL...",                                   // STR_MENU_OPEN_URL
    "&Add...\tA",                                    // STR_MENU_ADD
    "Playlist &Editor\tP",                           // STR_MENU_PLAYLIST_EDITOR
    "&Skin",                                          // STR_MENU_SKIN
    "Default",                                        // STR_MENU_SKIN_DEFAULT
    "&Language",                                      // STR_MENU_LANGUAGE
    "English",                                        // STR_MENU_LANGUAGE_ENGLISH
    "Deutsch",                                        // STR_MENU_LANGUAGE_GERMAN
    "Play &Control",                                  // STR_MENU_PLAY_CONTROL
    "&Play\tX",                                      // STR_MENU_PLAY
    "&Stop\\tV",                                      // STR_MENU_STOP
    "Pa&use\\tC",                                     // STR_MENU_PAUSE
    "&Next\\tB",                                      // STR_MENU_NEXT
    "Pre&vious\\tZ",                                  // STR_MENU_PREVIOUS
    "O&ptions",                                       // STR_MENU_OPTIONS
    "A&bout...\\tF1",                                 // STR_MENU_ABOUT
    "E&xit...\\tESC",                                 // STR_MENU_EXIT
    
    // Dialog titles
    "About BriskPlayer",                              // STR_DLG_ABOUT_TITLE
    "BriskPlayer Options",                            // STR_DLG_OPTIONS_TITLE
    "Open URL",                                       // STR_DLG_URL_TITLE
    "Quick Find",                                     // STR_DLG_QUICKFIND_TITLE
    
    // About dialog
    "&About",                                         // STR_ABOUT_TAB_ABOUT
    "&Keyboard Shortcuts",                            // STR_ABOUT_TAB_KEYBOARD
    "C&hanges",                                       // STR_ABOUT_TAB_CHANGES
    "C&lose",                                         // STR_ABOUT_CLOSE
    
    // Options dialog - General section
    "General",                                        // STR_OPTIONS_GENERAL
    "Always on top",                                  // STR_OPTIONS_ALWAYS_ON_TOP
    "Exit after playing",                             // STR_OPTIONS_EXIT_AFTER_PLAYING
    "Rotate systemtray icon",                         // STR_OPTIONS_ROTATE_SYSTRAY
    "Scroll Songtitle",                              // STR_OPTIONS_SCROLL_TITLE
    "Allow file once in playlist",                    // STR_OPTIONS_FILE_ONCE_PLAYLIST
    "Autoplay on startup",                           // STR_OPTIONS_AUTOPLAY_STARTUP
    "Allow multiple instances",                       // STR_OPTIONS_MULTIPLE_INSTANCES
    "Show remaining time",                           // STR_OPTIONS_SHOW_REMAINING_TIME
    "Show on taskbar",                               // STR_OPTIONS_SHOW_ON_TASKBAR
    "Register Filetypes",                            // STR_OPTIONS_REGISTER_FILETYPES
    "Add to Start Menu",                             // STR_OPTIONS_ADD_START_MENU
    "Read ID3 Tag (if any)",                         // STR_OPTIONS_READ_ID3_TAG
    "Read ID3 Tag of selected",                      // STR_OPTIONS_READ_ID3_SELECTED
    "Support ID3v2",                                 // STR_OPTIONS_SUPPORT_ID3V2
    "Prefer native OGG tags",                        // STR_OPTIONS_PREFER_NATIVE_OGG
    "Load ID3 tags in background",                   // STR_OPTIONS_READ_ID3_BACKGROUND
    "Work out track lengths",                        // STR_OPTIONS_WORK_OUT_LENGTHS
    "Easy move",                                     // STR_OPTIONS_EASY_MOVE
    "Remember playlist",                             // STR_OPTIONS_REMEMBER_PLAYLIST
    "Remember last played",                          // STR_OPTIONS_REMEMBER_LAST_PLAYED
    "Track Delay (sec)",                             // STR_OPTIONS_TRACK_DELAY_SEC
    "Skinlist length",                               // STR_OPTIONS_SKINLIST_LENGTH
    "Flush",                                         // STR_OPTIONS_FLUSH
    
    // Options dialog - Output section
    "Output",                                        // STR_OPTIONS_OUTPUT
    "Volume controls",                               // STR_OPTIONS_VOLUME_CONTROLS
    
    // Options dialog - Skin section
    "Skin",                                          // STR_OPTIONS_SKIN
    "Player",                                        // STR_OPTIONS_PLAYER
    "Open",                                          // STR_OPTIONS_OPEN
    
    // Options dialog - Buttons
    "OK",                                            // STR_OPTIONS_OK
    "Cancel",                                        // STR_OPTIONS_CANCEL
    
    // URL dialog
    "Type the URL you would like to play. Supported formats are MP3 and OGG streaming, and M3U playlists.", // STR_URL_DESCRIPTION
    "&URL:",                                         // STR_URL_LABEL
    
    // Quick Find dialog
    "Find Text",                                     // STR_QUICKFIND_TEXT
    "Look in",                                       // STR_QUICKFIND_LOOK_IN
    "&Titles",                                       // STR_QUICKFIND_TITLES
    "Artist &Names",                                 // STR_QUICKFIND_ARTISTS
    "&Album Names",                                  // STR_QUICKFIND_ALBUMS
    "C&lose",                                        // STR_QUICKFIND_CLOSE
    
    // Volume control options
    "System MASTER volume",                          // STR_VOLUME_SYSTEM_MASTER
    "System WAVE volume",                            // STR_VOLUME_SYSTEM_WAVE
    "Internal volume",                               // STR_VOLUME_INTERNAL
    
    // Message box messages
    "Filetypes are registered.\\nYou can doubleclick a supported file to run BriskPlayer.", // STR_MSG_FILETYPES_REGISTERED
    "An icon for BriskPlayer has been created in the StartMenu and Desktop.",                // STR_MSG_ICONS_CREATED
    
    // File filters
    "BriskPlayer Skin Initialization Files (*.ini)",  // STR_FILTER_SKIN_FILES
    "All Files (*.*)"                                 // STR_FILTER_ALL_FILES
};

// String ID to section/key mapping for INI files
typedef struct {
    const char* section;
    const char* key;
} StringMapping;

static const StringMapping g_stringMappings[STR_COUNT] = {
    // Application constants
    {"App", "Name"},                                  // STR_APP_NAME
    {"App", "Description"},                           // STR_APP_DESCRIPTION
    {"App", "AudioFileDesc"},                         // STR_APP_AUDIO_FILE_DESC
    {"App", "PlaylistDesc"},                          // STR_APP_PLAYLIST_DESC
    
    // Menu items
    {"Menu", "Open"},                                 // STR_MENU_OPEN
    {"Menu", "OpenURL"},                              // STR_MENU_OPEN_URL
    {"Menu", "Add"},                                  // STR_MENU_ADD
    {"Menu", "PlaylistEditor"},                       // STR_MENU_PLAYLIST_EDITOR
    {"Menu", "Skin"},                                 // STR_MENU_SKIN
    {"Menu", "SkinDefault"},                          // STR_MENU_SKIN_DEFAULT
    {"Menu", "Language"},                             // STR_MENU_LANGUAGE
    {"Menu", "LanguageEnglish"},                      // STR_MENU_LANGUAGE_ENGLISH
    {"Menu", "LanguageGerman"},                       // STR_MENU_LANGUAGE_GERMAN
    {"Menu", "PlayControl"},                          // STR_MENU_PLAY_CONTROL
    {"Menu", "Play"},                                 // STR_MENU_PLAY
    {"Menu", "Stop"},                                 // STR_MENU_STOP
    {"Menu", "Pause"},                                // STR_MENU_PAUSE
    {"Menu", "Next"},                                 // STR_MENU_NEXT
    {"Menu", "Previous"},                             // STR_MENU_PREVIOUS
    {"Menu", "Options"},                              // STR_MENU_OPTIONS
    {"Menu", "About"},                                // STR_MENU_ABOUT
    {"Menu", "Exit"},                                 // STR_MENU_EXIT
    
    // Dialog titles
    {"DialogTitles", "About"},                        // STR_DLG_ABOUT_TITLE
    {"DialogTitles", "Options"},                      // STR_DLG_OPTIONS_TITLE
    {"DialogTitles", "URL"},                          // STR_DLG_URL_TITLE
    {"DialogTitles", "QuickFind"},                    // STR_DLG_QUICKFIND_TITLE
    
    // About dialog
    {"AboutDialog", "TabAbout"},                      // STR_ABOUT_TAB_ABOUT
    {"AboutDialog", "TabKeyboard"},                   // STR_ABOUT_TAB_KEYBOARD
    {"AboutDialog", "TabChanges"},                    // STR_ABOUT_TAB_CHANGES
    {"AboutDialog", "Close"},                         // STR_ABOUT_CLOSE
    
    // Options dialog - General section
    {"OptionsDialog", "General"},                     // STR_OPTIONS_GENERAL
    {"OptionsDialog", "AlwaysOnTop"},                 // STR_OPTIONS_ALWAYS_ON_TOP
    {"OptionsDialog", "ExitAfterPlaying"},            // STR_OPTIONS_EXIT_AFTER_PLAYING
    {"OptionsDialog", "RotateSystray"},               // STR_OPTIONS_ROTATE_SYSTRAY
    {"OptionsDialog", "ScrollTitle"},                 // STR_OPTIONS_SCROLL_TITLE
    {"OptionsDialog", "FileOncePlaylist"},            // STR_OPTIONS_FILE_ONCE_PLAYLIST
    {"OptionsDialog", "AutoplayStartup"},             // STR_OPTIONS_AUTOPLAY_STARTUP
    {"OptionsDialog", "MultipleInstances"},           // STR_OPTIONS_MULTIPLE_INSTANCES
    {"OptionsDialog", "ShowRemainingTime"},           // STR_OPTIONS_SHOW_REMAINING_TIME
    {"OptionsDialog", "ShowOnTaskbar"},               // STR_OPTIONS_SHOW_ON_TASKBAR
    {"OptionsDialog", "RegisterFiletypes"},           // STR_OPTIONS_REGISTER_FILETYPES
    {"OptionsDialog", "AddStartMenu"},                // STR_OPTIONS_ADD_START_MENU
    {"OptionsDialog", "ReadID3Tag"},                  // STR_OPTIONS_READ_ID3_TAG
    {"OptionsDialog", "ReadID3Selected"},             // STR_OPTIONS_READ_ID3_SELECTED
    {"OptionsDialog", "SupportID3v2"},                // STR_OPTIONS_SUPPORT_ID3V2
    {"OptionsDialog", "PreferNativeOGG"},             // STR_OPTIONS_PREFER_NATIVE_OGG
    {"OptionsDialog", "ReadID3Background"},           // STR_OPTIONS_READ_ID3_BACKGROUND
    {"OptionsDialog", "WorkOutLengths"},              // STR_OPTIONS_WORK_OUT_LENGTHS
    {"OptionsDialog", "EasyMove"},                    // STR_OPTIONS_EASY_MOVE
    {"OptionsDialog", "RememberPlaylist"},            // STR_OPTIONS_REMEMBER_PLAYLIST
    {"OptionsDialog", "RememberLastPlayed"},          // STR_OPTIONS_REMEMBER_LAST_PLAYED
    {"OptionsDialog", "TrackDelaySec"},               // STR_OPTIONS_TRACK_DELAY_SEC
    {"OptionsDialog", "SkinlistLength"},              // STR_OPTIONS_SKINLIST_LENGTH
    {"OptionsDialog", "Flush"},                       // STR_OPTIONS_FLUSH
    
    // Options dialog - Output section
    {"OptionsDialog", "Output"},                      // STR_OPTIONS_OUTPUT
    {"OptionsDialog", "VolumeControls"},              // STR_OPTIONS_VOLUME_CONTROLS
    
    // Options dialog - Skin section
    {"OptionsDialog", "Skin"},                        // STR_OPTIONS_SKIN
    {"OptionsDialog", "Player"},                      // STR_OPTIONS_PLAYER
    {"OptionsDialog", "Open"},                        // STR_OPTIONS_OPEN
    
    // Options dialog - Buttons
    {"OptionsDialog", "OK"},                          // STR_OPTIONS_OK
    {"OptionsDialog", "Cancel"},                      // STR_OPTIONS_CANCEL
    
    // URL dialog
    {"URLDialog", "Description"},                     // STR_URL_DESCRIPTION
    {"URLDialog", "Label"},                           // STR_URL_LABEL
    
    // Quick Find dialog
    {"QuickFindDialog", "Text"},                      // STR_QUICKFIND_TEXT
    {"QuickFindDialog", "LookIn"},                    // STR_QUICKFIND_LOOK_IN
    {"QuickFindDialog", "Titles"},                    // STR_QUICKFIND_TITLES
    {"QuickFindDialog", "Artists"},                   // STR_QUICKFIND_ARTISTS
    {"QuickFindDialog", "Albums"},                    // STR_QUICKFIND_ALBUMS
    {"QuickFindDialog", "Close"},                     // STR_QUICKFIND_CLOSE
    
    // Volume control options
    {"VolumeControls", "SystemMaster"},               // STR_VOLUME_SYSTEM_MASTER
    {"VolumeControls", "SystemWave"},                 // STR_VOLUME_SYSTEM_WAVE
    {"VolumeControls", "Internal"},                   // STR_VOLUME_INTERNAL
    
    // Message box messages
    {"Messages", "FiletypesRegistered"},              // STR_MSG_FILETYPES_REGISTERED
    {"Messages", "IconsCreated"},                     // STR_MSG_ICONS_CREATED
    
    // File filters
    {"FileFilters", "SkinFiles"},                     // STR_FILTER_SKIN_FILES
    {"FileFilters", "AllFiles"}                       // STR_FILTER_ALL_FILES
};

// Helper function to get application directory
static void GetApplicationDirectory(char* path, int maxPath)
{
    GetModuleFileName(NULL, path, maxPath);
    char* lastSlash = strrchr(path, '\\');
    if (lastSlash) {
        *(lastSlash + 1) = '\0';
    }
}

// Helper function to detect system language with robust fallback
static void DetectSystemLanguage(char* languageCode, int maxLen)
{
    // Try modern API first (Windows Vista+)
    HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32.dll"));
    if (hKernel32) {
        typedef int (WINAPI *GetUserDefaultLocaleNameFunc)(LPWSTR, int);
        GetUserDefaultLocaleNameFunc pGetUserDefaultLocaleName = 
            (GetUserDefaultLocaleNameFunc)GetProcAddress(hKernel32, "GetUserDefaultLocaleName");
            
        if (pGetUserDefaultLocaleName) {
            wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
            if (pGetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) > 0) {
                // Convert to narrow string and extract language code
                char narrowName[LOCALE_NAME_MAX_LENGTH];
                WideCharToMultiByte(CP_ACP, 0, localeName, -1, narrowName, sizeof(narrowName), NULL, NULL);
                
                // Extract first 2 characters (language code)
                if (strlen(narrowName) >= 2) {
                    strncpy(languageCode, narrowName, 2);
                    languageCode[2] = '\0';
                    return;
                }
            }
        }
    }
    
    // Fallback to older API (Windows 2000+)
    LCID lcid = GetUserDefaultLCID();
    char localeInfo[10];
    if (GetLocaleInfoA(lcid, LOCALE_SISO639LANGNAME, localeInfo, sizeof(localeInfo)) > 0) {
        strncpy(languageCode, localeInfo, maxLen - 1);
        languageCode[maxLen - 1] = '\0';
    } else {
        // Ultimate fallback to English
        strcpy(languageCode, "en");
    }
}

// Initialize the translation system
void CPT_Initialize(void)
{
    if (g_initialized) {
        return;
    }
    
    // Copy default strings first
    for (int i = 0; i < STR_COUNT; i++) {
        strncpy(g_translationStrings[i], g_defaultStrings[i], sizeof(g_translationStrings[i]) - 1);
        g_translationStrings[i][sizeof(g_translationStrings[i]) - 1] = '\0';
    }
    
    g_initialized = TRUE;
    
    // Auto-detect system language if not already set and no saved preference
    if (g_currentLanguage[0] == '\0') {
        char systemLang[8];
        DetectSystemLanguage(systemLang, sizeof(systemLang));
        
        // Try to load system language first
        if (CPT_LoadLanguage(systemLang)) {
            // Successfully loaded system language
            strncpy(g_currentLanguage, systemLang, sizeof(g_currentLanguage) - 1);
            g_currentLanguage[sizeof(g_currentLanguage) - 1] = '\0';
        } else {
            // System language not available, fall back to English
            if (CPT_LoadLanguage("en")) {
                strcpy(g_currentLanguage, "en");
            } else {
                // Even English not available, use built-in defaults
                strcpy(g_currentLanguage, "en");
            }
        }
    } else {
        // Language was already set manually, try to load it
        CPT_LoadLanguage(g_currentLanguage);
    }
}

// Cleanup the translation system
void CPT_Cleanup(void)
{
    g_initialized = FALSE;
}

// Convert UTF-8 string to ANSI (Windows code page)
static BOOL ConvertUTF8ToANSI(const char* utf8String, char* ansiString, int ansiBufferSize)
{
    // First convert UTF-8 to Unicode
    wchar_t unicodeBuffer[512];
    int unicodeLength = MultiByteToWideChar(CP_UTF8, 0, utf8String, -1, unicodeBuffer, sizeof(unicodeBuffer) / sizeof(wchar_t));
    
    if (unicodeLength == 0) {
        return FALSE;  // Conversion failed
    }
    
    // Then convert Unicode to ANSI (system default code page)
    int ansiLength = WideCharToMultiByte(CP_ACP, 0, unicodeBuffer, -1, ansiString, ansiBufferSize, NULL, NULL);
    
    return (ansiLength > 0);
}

// Process escape sequences in strings (e.g., \t -> tab character)
static void ProcessEscapeSequences(char* str)
{
    char* src = str;
    char* dst = str;
    
    while (*src) {
        if (*src == '\\' && *(src + 1)) {
            switch (*(src + 1)) {
                case 't':
                    *dst++ = '\t';  // Tab character
                    src += 2;
                    break;
                case 'n':
                    *dst++ = '\n';  // Newline character
                    src += 2;
                    break;
                case 'r':
                    *dst++ = '\r';  // Carriage return
                    src += 2;
                    break;
                case '\\':
                    *dst++ = '\\';  // Literal backslash
                    src += 2;
                    break;
                default:
                    *dst++ = *src++; // Keep as-is for unknown sequences
                    break;
            }
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

// Load a language from INI file
BOOL CPT_LoadLanguage(const char* languageCode)
{
    if (!g_initialized) {
        CPT_Initialize();
    }
    
    char iniPath[MAX_PATH];
    GetApplicationDirectory(iniPath, MAX_PATH);
    strcat(iniPath, "lang\\");
    strcat(iniPath, languageCode);
    strcat(iniPath, ".ini");
    
    // Check if file exists
    DWORD fileAttributes = GetFileAttributesA(iniPath);
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        // File doesn't exist, use defaults
        return FALSE;
    }
    
    // Load strings from INI file
    char buffer[512];
    for (int i = 0; i < STR_COUNT; i++) {
        DWORD result = GetPrivateProfileStringA(
            g_stringMappings[i].section,
            g_stringMappings[i].key,
            g_defaultStrings[i],  // Default value if key not found
            buffer,
            sizeof(buffer),
            iniPath
        );
        
        if (result > 0) {
            // Convert UTF-8 to ANSI for proper display
            char convertedBuffer[512];
            if (ConvertUTF8ToANSI(buffer, convertedBuffer, sizeof(convertedBuffer))) {
                strncpy(g_translationStrings[i], convertedBuffer, sizeof(g_translationStrings[i]) - 1);
            } else {
                strncpy(g_translationStrings[i], buffer, sizeof(g_translationStrings[i]) - 1);
            }
            g_translationStrings[i][sizeof(g_translationStrings[i]) - 1] = '\0';
            
            // Process escape sequences like \t -> tab character
            ProcessEscapeSequences(g_translationStrings[i]);
        }
    }
    
    // Update current language
    strncpy(g_currentLanguage, languageCode, sizeof(g_currentLanguage) - 1);
    g_currentLanguage[sizeof(g_currentLanguage) - 1] = '\0';
    
    return TRUE;
}

// Get a translated string
const char* CPT_GetString(CPT_StringID stringID)
{
    if (!g_initialized) {
        CPT_Initialize();
    }
    
    if (stringID < 0 || stringID >= STR_COUNT) {
        return "Invalid String ID";
    }
    
    return g_translationStrings[stringID];
}

// Set the default language
void CPT_SetDefaultLanguage(const char* languageCode)
{
    strncpy(g_currentLanguage, languageCode, sizeof(g_currentLanguage) - 1);
    g_currentLanguage[sizeof(g_currentLanguage) - 1] = '\0';
    
    if (g_initialized) {
        CPT_LoadLanguage(languageCode);
    }
}

// Get the current language code
const char* CPT_GetCurrentLanguage(void)
{
    return g_currentLanguage;
}

// Check if a language file exists
BOOL CPT_LanguageFileExists(const char* languageCode)
{
    char iniPath[MAX_PATH];
    GetApplicationDirectory(iniPath, MAX_PATH);
    strcat(iniPath, "lang\\");
    strcat(iniPath, languageCode);
    strcat(iniPath, ".ini");
    
    DWORD fileAttributes = GetFileAttributes(iniPath);
    return (fileAttributes != INVALID_FILE_ATTRIBUTES);
}

// Get language-specific dialog size
DialogSize CPT_GetDialogSize(const char* dialogName)
{
    DialogSize defaultSize = {250, 300}; // Default size
    
    if (!g_initialized) {
        return defaultSize;
    }
    
    // Language-specific adjustments
    if (strcmp(g_currentLanguage, "de") == 0) {
        // German text is typically 20-30% longer
        if (strcmp(dialogName, "Options") == 0) {
            DialogSize germanSize = {300, 320}; // Wider for German
            return germanSize;
        }
        else if (strcmp(dialogName, "URL") == 0) {
            DialogSize germanSize = {350, 120}; // Wider for German URL dialog
            return germanSize;
        }
    }
    
    return defaultSize;
}