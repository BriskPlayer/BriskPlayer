
/*
 * CoolPlayer - Blazing fast audio player.
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

#ifndef ROTATING_ICON_H
#define ROTATING_ICON_H

////////////////////////////////////////////////////////////////////////////////
//
// Rotating sysicon code with modern notification support
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>

// Note: CP_HSYSICON is defined in globals.h as void*
// Forward declare the struct for internal use only
struct _CPs_SysIcon;

// Create/destroy systray icon
CP_HSYSICON CPSYSICON_Create(HWND hWnd);
void CPSYSICON_Destroy(CP_HSYSICON hSysIconData);

// Update icon animation
void CPSYSICON_AdvanceFrame(CP_HSYSICON hSysIconData);

// Update tooltip text
void CPSYSICON_SetTipText(CP_HSYSICON hSysIconData, const char* pcNewTipText);

// Show balloon notification (modern toast-style on Windows 10+)
// dwInfoFlags: NIIF_INFO, NIIF_WARNING, NIIF_ERROR, or NIIF_NONE
void CPSYSICON_ShowBalloon(CP_HSYSICON hSysIconData, 
                           const char* pcTitle, 
                           const char* pcMessage,
                           DWORD dwInfoFlags);

#endif // ROTATING_ICON_H
