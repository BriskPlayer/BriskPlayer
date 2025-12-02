
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

#ifndef CPSTRING_H
#define CPSTRING_H

// Forward declaration for Windows types
#ifndef _WINDEF_
typedef int BOOL;
typedef unsigned short WCHAR;
#endif

////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
// Some basic string utility stuff
unsigned int STR_AllocSetString(char** ppcDest, const char* pcSource, const BOOL bFreeExisting);

// Unicode utility functions for filename support
WCHAR* STR_ConvertToUnicode(const char* pcSource);
char* STR_ConvertFromUnicode(const WCHAR* pwcSource);
unsigned int STR_AllocSetStringW(WCHAR** ppwcDest, const WCHAR* pwcSource, const BOOL bFreeExisting);
char* STR_AllocConvertFromUnicode(const WCHAR* pwcSource);

#endif // CPSTRING_H

