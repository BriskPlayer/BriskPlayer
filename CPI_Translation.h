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

/*
 * Translation system for BriskPlayer
 * 
 * This file now simply includes the modern gettext-based translation system.
 * All translations are handled through standard gettext functions.
 * 
 * Usage:
 * - Basic translation: _(u8"Text to translate")
 * - Context-aware: C_(CTX_MENU, u8"Menu item")
 * - Plural forms: P_(u8"1 file", u8"%d files", count)
 * 
 * See CPI_Gettext.h for full documentation.
 */

#include "CPI_Gettext.h"

// Simple wrapper functions for compatibility during transition
BOOL Translation_Initialize(void);
void Translation_SetLanguage(const char* languageCode);
const char* Translation_GetCurrentLanguage(void);
BOOL Translation_IsLanguageAvailable(const char* languageCode);
int Translation_EnumerateLanguages(LanguageInfo* languages, int maxLanguages);

#endif // CPI_TRANSLATION_H
