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

////////////////////////////////////////////////////////////////////////////////
//
// Main Menu Module Implementation
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MainMenu.h"
#include "globals.h"
#include "resource.h"
#include "CPI_Gettext.h"
#include "CPI_Translation.h"

////////////////////////////////////////////////////////////////////////////////
// Module State
////////////////////////////////////////////////////////////////////////////////

// Language codes for menu items (indexed by menu ID offset from MENU_LANGUAGE_BASE)
static char g_LanguageCodes[CPC_MAX_MENU_ITEMS][16];
static int g_LanguageCount = 0;

////////////////////////////////////////////////////////////////////////////////
// Menu Initialization
////////////////////////////////////////////////////////////////////////////////

void MainMenu_Initialize(HMENU hMenu)
{
    // Store reference in globals for compatibility
    globals.main_menu_popup = hMenu;
    g_LanguageCount = 0;
    
    // Initialize skin menu tracking
    globals.main_int_skin_last_number = MENU_SKIN_DEFAULT + 1;
    
    CP_LOG_DEBUG("MainMenu initialized\n");
}

void MainMenu_Cleanup(void)
{
    // Menu will be destroyed with window, just clear state
    g_LanguageCount = 0;
    CP_LOG_DEBUG("MainMenu cleaned up\n");
}

////////////////////////////////////////////////////////////////////////////////
// Menu Translation
////////////////////////////////////////////////////////////////////////////////

void MainMenu_TranslateAll(void)
{
    if (!globals.main_menu_popup) return;
    
    // Translate main menu items using Unicode
    ModifyMenuW(globals.main_menu_popup, MENU_OPENFILE, MF_BYCOMMAND | MF_STRING, 
                MENU_OPENFILE, TW(STR_MENU_OPEN));
    ModifyMenuW(globals.main_menu_popup, MENU_OPENLOC, MF_BYCOMMAND | MF_STRING, 
                MENU_OPENLOC, TW(STR_MENU_OPEN_URL));
    ModifyMenuW(globals.main_menu_popup, MENU_ADDFILE, MF_BYCOMMAND | MF_STRING, 
                MENU_ADDFILE, TW(STR_MENU_ADD));
    ModifyMenuW(globals.main_menu_popup, MENU_PLAYLIST, MF_BYCOMMAND | MF_STRING, 
                MENU_PLAYLIST, TW(STR_MENU_PLAYLIST_EDITOR));
    ModifyMenuW(globals.main_menu_popup, MENU_OPTIONS, MF_BYCOMMAND | MF_STRING, 
                MENU_OPTIONS, TW(STR_MENU_OPTIONS));
    ModifyMenuW(globals.main_menu_popup, MENU_ABOUT, MF_BYCOMMAND | MF_STRING, 
                MENU_ABOUT, TW(STR_MENU_ABOUT));
    ModifyMenuW(globals.main_menu_popup, MENU_EXIT, MF_BYCOMMAND | MF_STRING, 
                MENU_EXIT, TW(STR_MENU_EXIT));
    
    // Translate Play Control submenu items
    ModifyMenuW(globals.main_menu_popup, ID_PLAY, MF_BYCOMMAND | MF_STRING, 
                ID_PLAY, TW(STR_MENU_PLAY));
    ModifyMenuW(globals.main_menu_popup, ID_STOP, MF_BYCOMMAND | MF_STRING, 
                ID_STOP, TW(STR_MENU_STOP));
    ModifyMenuW(globals.main_menu_popup, ID_PAUSE, MF_BYCOMMAND | MF_STRING, 
                ID_PAUSE, TW(STR_MENU_PAUSE));
    ModifyMenuW(globals.main_menu_popup, ID_NEXT, MF_BYCOMMAND | MF_STRING, 
                ID_NEXT, TW(STR_MENU_NEXT));
    ModifyMenuW(globals.main_menu_popup, ID_PREVIOUS, MF_BYCOMMAND | MF_STRING, 
                ID_PREVIOUS, TW(STR_MENU_PREVIOUS));
    
    // Translate Skin submenu default item
    HMENU skinMenu = GetSubMenu(globals.main_menu_popup, SKIN_SUBMENU_INDEX);
    if (skinMenu) {
        ModifyMenuW(skinMenu, MENU_SKIN_DEFAULT, MF_BYCOMMAND | MF_STRING, 
                    MENU_SKIN_DEFAULT, TW(STR_MENU_SKIN_DEFAULT));
    }
    
    // Translate submenu titles by position
    int menuItemCount = GetMenuItemCount(globals.main_menu_popup);
    for (int i = 0; i < menuItemCount; i++) {
        HMENU subMenu = GetSubMenu(globals.main_menu_popup, i);
        if (subMenu) {
            if (i == SKIN_SUBMENU_INDEX) {
                ModifyMenu(globals.main_menu_popup, i, MF_BYPOSITION | MF_POPUP | MF_STRING, 
                           (UINT_PTR)subMenu, T(STR_MENU_SKIN));
            }
            else if (i == LANGUAGE_SUBMENU_INDEX) {
                ModifyMenu(globals.main_menu_popup, i, MF_BYPOSITION | MF_POPUP | MF_STRING, 
                           (UINT_PTR)subMenu, T(STR_MENU_LANGUAGE));
            }
            else if (i == DSP_SUBMENU_INDEX) {
                ModifyMenu(globals.main_menu_popup, i, MF_BYPOSITION | MF_POPUP | MF_STRING, 
                           (UINT_PTR)subMenu, T(STR_MENU_DSP_PLUGINS));
            }
            else {
                // Check if this is the Play Control submenu
                MENUITEMINFO mii = {0};
                mii.cbSize = sizeof(MENUITEMINFO);
                mii.fMask = MIIM_ID;
                if (GetMenuItemInfo(subMenu, 0, TRUE, &mii) && mii.wID == ID_PLAY) {
                    ModifyMenu(globals.main_menu_popup, i, MF_BYPOSITION | MF_POPUP | MF_STRING, 
                               (UINT_PTR)subMenu, T(STR_MENU_PLAY_CONTROL));
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Language Menu
////////////////////////////////////////////////////////////////////////////////

void MainMenu_PopulateLanguages(void)
{
    HMENU languageMenu = GetSubMenu(globals.main_menu_popup, LANGUAGE_SUBMENU_INDEX);
    if (!languageMenu) {
        return;
    }
    
    // Clear existing items
    while (GetMenuItemCount(languageMenu) > 0) {
        RemoveMenu(languageMenu, 0, MF_BYPOSITION);
    }
    
    // Reset language tracking
    g_LanguageCount = 0;
    
    // Get available languages from gettext system
    LanguageInfo languages[16];
    int languageCount = CPG_EnumerateLanguages(languages, 16);
    
    const char* currentLang = CPG_GetCurrentLanguage();
    
    // Add menu items for each discovered language
    for (int i = 0; i < languageCount && i < CPC_MAX_MENU_ITEMS; i++) {
        const LanguageInfo* lang = &languages[i];
        
        // Store language code for lookup
        strncpy(g_LanguageCodes[i], lang->code, sizeof(g_LanguageCodes[i]) - 1);
        g_LanguageCodes[i][sizeof(g_LanguageCodes[i]) - 1] = '\0';
        
        // Create display name
        wchar_t displayName[CPC_TITLE_BUFFER];
        char tempName[CPC_TITLE_BUFFER];
        
        if (strlen(lang->region) > 0) {
            snprintf(tempName, sizeof(tempName), "%s (%s)", lang->name, lang->region);
        } else {
            strncpy(tempName, lang->name, sizeof(tempName) - 1);
            tempName[sizeof(tempName) - 1] = '\0';
        }
        
        MultiByteToWideChar(CP_UTF8, 0, tempName, -1, displayName, CPC_TITLE_BUFFER);
        
        // Set flags
        UINT flags = MF_STRING;
        if (strcmp(currentLang, lang->code) == 0) {
            flags |= MF_CHECKED;
        }
        
        UINT menuId = MENU_LANGUAGE_BASE + i + 1;
        AppendMenuW(languageMenu, flags, menuId, displayName);
        g_LanguageCount++;
    }
    
    // Fallback if no languages found
    if (languageCount == 0) {
        strncpy(g_LanguageCodes[0], "en", sizeof(g_LanguageCodes[0]));
        AppendMenuW(languageMenu, MF_STRING | MF_CHECKED, MENU_LANGUAGE_EN, L"English (Fallback)");
        g_LanguageCount = 1;
    }
}

void MainMenu_SwitchLanguage(const char* languageCode)
{
    CP_LOG_DEBUG("Switching to language: %s\n", languageCode);
    
    // Set the language
    CPG_SetLanguage(languageCode);
    
    // Save preference
    strncpy(options.preferred_language, languageCode, sizeof(options.preferred_language) - 1);
    options.preferred_language[sizeof(options.preferred_language) - 1] = '\0';
    options_write();
    
    // Refresh menus
    MainMenu_TranslateAll();
    MainMenu_PopulateLanguages();
}

const char* MainMenu_GetLanguageFromMenuId(UINT menuId)
{
    if (!MainMenu_IsLanguageMenuId(menuId)) {
        return NULL;
    }
    
    int index = menuId - MENU_LANGUAGE_BASE - 1;
    if (index >= 0 && index < g_LanguageCount) {
        return g_LanguageCodes[index];
    }
    
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Skin Menu
////////////////////////////////////////////////////////////////////////////////

void MainMenu_AddSkinToHistory(const char* skinName)
{
    if (!skinName || !*skinName) return;
    
    HMENU skinMenu = GetSubMenu(globals.main_menu_popup, SKIN_SUBMENU_INDEX);
    if (!skinMenu) return;
    
    int itemCount = GetMenuItemCount(skinMenu);
    
    // Check if skin is already in menu
    for (int i = 0; i < itemCount; i++) {
        char skinstring[CPC_PATH_BUFFER];
        if (GetMenuStringA(skinMenu, i, skinstring, CPC_PATH_BUFFER, MF_BYPOSITION)) {
            if (strcmp(skinName, skinstring) == 0) {
                return;  // Already exists
            }
        }
    }
    
    // Add new skin entry
    MENUITEMINFO menuinfo = {0};
    menuinfo.cbSize = sizeof(MENUITEMINFO);
    menuinfo.fMask = MIIM_TYPE | MIIM_ID;
    menuinfo.fType = MFT_STRING | MFT_RADIOCHECK;
    
    // Wrap menu ID if we've reached the limit
    if (globals.main_int_skin_last_number >= MENU_SKIN_DEFAULT + 1 + options.remember_skin_count) {
        globals.main_int_skin_last_number = MENU_SKIN_DEFAULT + 1;
    }
    
    menuinfo.wID = globals.main_int_skin_last_number++;
    menuinfo.dwTypeData = (LPSTR)skinName;
    menuinfo.cch = (UINT)strlen(skinName);
    
    InsertMenuItem(globals.main_menu_popup, MENU_SKIN_DEFAULT, FALSE, &menuinfo);
    
    // Remove oldest if over limit
    if (itemCount > options.remember_skin_count) {
        RemoveMenu(skinMenu, 0, MF_BYPOSITION);
    }
}

void MainMenu_SelectSkin(const char* skinName)
{
    if (!skinName) return;
    
    HMENU skinMenu = GetSubMenu(globals.main_menu_popup, SKIN_SUBMENU_INDEX);
    if (!skinMenu) return;
    
    int itemCount = GetMenuItemCount(skinMenu);
    
    for (int i = 0; i < itemCount; i++) {
        char skinstring[CPC_PATH_BUFFER];
        if (GetMenuStringA(skinMenu, i, skinstring, CPC_PATH_BUFFER, MF_BYPOSITION)) {
            if (strcmp(skinName, skinstring) == 0) {
                CheckMenuRadioItem(skinMenu, 0, itemCount, i, MF_BYPOSITION);
                return;
            }
        }
    }
}

void MainMenu_ClearSkinHistory(void)
{
    HMENU skinMenu = GetSubMenu(globals.main_menu_popup, SKIN_SUBMENU_INDEX);
    if (!skinMenu) return;
    
    // Remove all items except the first (Default)
    while (GetMenuItemCount(skinMenu) > 1) {
        RemoveMenu(skinMenu, 1, MF_BYPOSITION);
    }
    
    globals.main_int_skin_last_number = MENU_SKIN_DEFAULT + 1;
}

BOOL MainMenu_GetSkinFromMenuId(UINT menuId, char* pszBuffer, int bufferSize)
{
    if (!MainMenu_IsSkinMenuId(menuId) || !pszBuffer || bufferSize <= 0) {
        return FALSE;
    }
    
    HMENU skinMenu = GetSubMenu(globals.main_menu_popup, SKIN_SUBMENU_INDEX);
    if (!skinMenu) return FALSE;
    
    // Find the menu item with this ID
    int itemCount = GetMenuItemCount(skinMenu);
    for (int i = 0; i < itemCount; i++) {
        MENUITEMINFO mii = {0};
        mii.cbSize = sizeof(MENUITEMINFO);
        mii.fMask = MIIM_ID;
        
        if (GetMenuItemInfo(skinMenu, i, TRUE, &mii) && mii.wID == menuId) {
            if (GetMenuStringA(skinMenu, i, pszBuffer, bufferSize, MF_BYPOSITION)) {
                return TRUE;
            }
        }
    }
    
    return FALSE;
}
