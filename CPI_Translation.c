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