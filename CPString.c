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
// CPString.c - String Utilities and Unicode/ANSI Conversion
//
// UNICODE MIGRATION STRATEGY:
// ---------------------------
// BriskPlayer is gradually transitioning from ANSI to Unicode:
//
// CURRENT STATE (Mixed):
//   - Internal processing: Mostly ANSI (char*, CP_ACP code page)
//   - File I/O: Unicode (WCHAR*, CreateFileW for international filenames)
//   - UI: Mixed (some ANSI dialogs, some Unicode)
//
// CONVERSION FUNCTIONS:
//   - STR_ConvertToUnicode():   ANSI -> Unicode (for file operations)
//   - STR_ConvertFromUnicode(): Unicode -> ANSI (for internal processing)
//   - STR_ConvertToUnicodeAlloc(): Unicode with memory allocation
//
// FUTURE DIRECTION:
//   - Move all string handling to Unicode (WCHAR*)
//   - Use UTF-8 for external files (playlists, configs)
//   - Replace CP_ACP with UTF-8 code page (CP_UTF8)
//   - Use Windows W-suffix APIs exclusively (CreateFileW, etc.)
//
// LEGACY CONCERNS:
//   - Older code uses ANSI lstrcpy/lstrcat (see stdafx.h)
//   - Many functions still use char* for track names, URLs
//   - Gradual migration required to avoid breaking existing code
////////////////////////////////////////////////////////////////////////////////



#include "stdafx.h"
#include "globals.h"


////////////////////////////////////////////////////////////////////////////////
//
//
//
unsigned int STR_AllocSetString(char** ppcDest, const char* pcSource, const BOOL bFreeExisting)
{
	if (bFreeExisting == TRUE && *ppcDest)
		free(*ppcDest);
		
	if (pcSource)
	{
		unsigned int uStringLength;
		
		uStringLength = strlen(pcSource) + 1;
		*ppcDest = CALLOC_TYPE(char, uStringLength);
		
		if (!*ppcDest)
		{
			// Failed to allocate memory, a memcpy here would be fatal.
			return 0;
		}
		
		memcpy(*ppcDest, pcSource, uStringLength);
		
		return uStringLength;
	}
	
	*ppcDest = NULL;
	
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Unicode utility functions for filename support
//

//
// Convert ANSI string to Unicode (wide character)
//
WCHAR* STR_ConvertToUnicode(const char* pcSource)
{
	if (!pcSource)
		return NULL;
		
	int iLength = MultiByteToWideChar(CP_ACP, 0, pcSource, -1, NULL, 0);
	if (iLength == 0)
		return NULL;
		
	WCHAR* pwcResult = CALLOC_TYPE(WCHAR, iLength);
	if (!pwcResult)
		return NULL;
		
	if (MultiByteToWideChar(CP_ACP, 0, pcSource, -1, pwcResult, iLength) == 0)
	{
		free(pwcResult);
		return NULL;
	}
	
	return pwcResult;
}

//
// Convert Unicode (wide character) to ANSI string
//
char* STR_ConvertFromUnicode(const WCHAR* pwcSource)
{
	if (!pwcSource)
		return NULL;
		
	int iLength = WideCharToMultiByte(CP_ACP, 0, pwcSource, -1, NULL, 0, NULL, NULL);
	if (iLength == 0)
		return NULL;
		
	char* pcResult = CALLOC_TYPE(char, iLength);
	if (!pcResult)
		return NULL;
		
	if (WideCharToMultiByte(CP_ACP, 0, pwcSource, -1, pcResult, iLength, NULL, NULL) == 0)
	{
		free(pcResult);
		return NULL;
	}
	
	return pcResult;
}

//
// Allocate and set wide character string (similar to STR_AllocSetString but for Unicode)
//
unsigned int STR_AllocSetStringW(WCHAR** ppwcDest, const WCHAR* pwcSource, const BOOL bFreeExisting)
{
	if (bFreeExisting == TRUE && *ppwcDest)
		free(*ppwcDest);
		
	if (pwcSource)
	{
		unsigned int uStringLength;
		
		uStringLength = (wcslen(pwcSource) + 1) * sizeof(WCHAR);
		*ppwcDest = CALLOC_TYPE(WCHAR, wcslen(pwcSource) + 1);
		
		if (!*ppwcDest)
		{
			// Failed to allocate memory
			return 0;
		}
		
		memcpy(*ppwcDest, pwcSource, uStringLength);
		
		return uStringLength;
	}
	
	*ppwcDest = NULL;
	return 0;
}

//
// Convert Unicode string to ANSI and allocate memory for it
//
char* STR_AllocConvertFromUnicode(const WCHAR* pwcSource)
{
	return STR_ConvertFromUnicode(pwcSource);
}

//
//
//

