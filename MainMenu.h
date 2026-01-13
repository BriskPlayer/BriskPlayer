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

#ifndef MAIN_MENU_H
#define MAIN_MENU_H

////////////////////////////////////////////////////////////////////////////////
//
// Main Menu Module
//
// Handles all menu-related functionality:
// - Menu initialization and cleanup
// - Language menu population and switching
// - Skin menu management
// - Menu translation
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include "resource.h"

////////////////////////////////////////////////////////////////////////////////
// Menu Initialization
////////////////////////////////////////////////////////////////////////////////

// Initialize the main popup menu
void MainMenu_Initialize(HMENU hMenu);

// Cleanup menu resources
void MainMenu_Cleanup(void);

////////////////////////////////////////////////////////////////////////////////
// Menu Translation
////////////////////////////////////////////////////////////////////////////////

// Translate all menu items to current language
void MainMenu_TranslateAll(void);

////////////////////////////////////////////////////////////////////////////////
// Language Menu
////////////////////////////////////////////////////////////////////////////////

// Populate language submenu with available languages
void MainMenu_PopulateLanguages(void);

// Switch to a new language (saves preference and refreshes UI)
void MainMenu_SwitchLanguage(const char* languageCode);

// Get language code from menu item ID
// Returns NULL if ID is not a language menu item
const char* MainMenu_GetLanguageFromMenuId(UINT menuId);

////////////////////////////////////////////////////////////////////////////////
// Skin Menu
////////////////////////////////////////////////////////////////////////////////

// Add a skin to the skin history menu
void MainMenu_AddSkinToHistory(const char* skinName);

// Select (check) a skin in the menu
void MainMenu_SelectSkin(const char* skinName);

// Clear all skin history items (except default)
void MainMenu_ClearSkinHistory(void);

// Get skin path from menu item ID
// Returns FALSE if ID is not a skin menu item
BOOL MainMenu_GetSkinFromMenuId(UINT menuId, char* pszBuffer, int bufferSize);

////////////////////////////////////////////////////////////////////////////////
// Menu ID Helpers
////////////////////////////////////////////////////////////////////////////////

// Check if a menu ID is in the language menu range
static inline BOOL MainMenu_IsLanguageMenuId(UINT menuId)
{
    return (menuId > MENU_LANGUAGE_BASE && menuId <= MENU_LANGUAGE_BASE + 100);
}

// Check if a menu ID is in the skin menu range  
static inline BOOL MainMenu_IsSkinMenuId(UINT menuId)
{
    return (menuId > MENU_SKIN_DEFAULT && menuId < MENU_SKIN_DEFAULT + 100);
}

////////////////////////////////////////////////////////////////////////////////

#endif // MAIN_MENU_H
