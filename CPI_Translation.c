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

/*
 * Simple translation wrapper
 * 
 * This file now just provides a simple initialization function
 * that sets up the gettext system. All actual translation work
 * is done through the gettext macros defined in CPI_Gettext.h
 */

// Initialize the translation system
BOOL Translation_Initialize(void)
{
    // Initialize gettext with default configuration
    const GetTextConfig config = {
        .domain = BRISKPLAYER_DOMAIN,
        .directory = "./locale",
        .locale_category = "LC_ALL",
        .use_utf8 = true,
        .fallback_enabled = true
    };
    
    return CPG_Initialize(&config);
}

// Function to mark all translatable strings (for POT file generation)
// This function is never called at runtime - it's only used by xgettext
// to extract strings for translation
static void __translation_strings_marker(void) 
{
    // Menu strings
    _(STR_MENU_OPEN);
    _(STR_MENU_OPEN_URL);
    _(STR_MENU_ADD);
    _(STR_MENU_PLAYLIST_EDITOR);
    _(STR_MENU_OPTIONS);
    _(STR_MENU_ABOUT);
    _(STR_MENU_EXIT);
    _(STR_MENU_PLAY);
    _(STR_MENU_STOP);
    _(STR_MENU_PAUSE);
    _(STR_MENU_NEXT);
    _(STR_MENU_PREVIOUS);
    _(STR_MENU_SKIN_DEFAULT);
    _(STR_MENU_SKIN);
    _(STR_MENU_LANGUAGE);
    _(STR_MENU_PLAY_CONTROL);
    
    // Dialog strings
    _(STR_DLG_URL_TITLE);
    _(STR_URL_DESCRIPTION);
    _(STR_URL_LABEL);
    _(STR_OPTIONS_OK);
    _(STR_OPTIONS_CANCEL);
    _(STR_DLG_OPTIONS_TITLE);
    _(STR_OPTIONS_ALWAYS_ON_TOP);
    _(STR_OPTIONS_EXIT_AFTER_PLAYING);
    _(STR_OPTIONS_ROTATE_SYSTRAY);
    _(STR_OPTIONS_SCROLL_TITLE);
    _(STR_OPTIONS_FILE_ONCE_PLAYLIST);
}

// Set language
void Translation_SetLanguage(const char* languageCode)
{
    CPG_SetLanguage(languageCode);
}

// Get current language
const char* Translation_GetCurrentLanguage(void)
{
    return CPG_GetCurrentLanguage();
}

// Check if language is available
BOOL Translation_IsLanguageAvailable(const char* languageCode)
{
    return CPG_IsLanguageAvailable(languageCode);
}

// Enumerate available languages
int Translation_EnumerateLanguages(LanguageInfo* languages, int maxLanguages)
{
    return CPG_EnumerateLanguages(languages, maxLanguages);
}