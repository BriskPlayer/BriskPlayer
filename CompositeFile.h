
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
////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////
//
// Support for loading PKZIP files
//
////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////
//
CP_COMPOSITEFILE CF_Create_FromFile(const char* pcPath);
CP_COMPOSITEFILE CF_Create_FromResource(HMODULE hModule, UINT uiResourceID, const char* pcResourceType);
// Opens the composite skin currently in effect: the external file named by
// options.active_skin_path if one is set (falling back to the embedded
// default and clearing that field in memory if the file can't be opened),
// or the embedded default resource otherwise. Shared by skin.c and
// CPSK_Skin.c so both the main-window and playlist skins stay in sync.
CP_COMPOSITEFILE CF_Create_ForActiveSkin(void);
void CF_Destroy(CP_COMPOSITEFILE hComposite);
BOOL CF_GetSubFile(CP_COMPOSITEFILE hComposite, const char* pcSubfilename, void** ppSubFile_Uncompressed, unsigned int* piSubFile_Length);
//
////////////////////////////////////////////////////////////////////////////////
