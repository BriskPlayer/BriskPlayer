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



#include "stdafx.h"
#include "globals.h"
#include "CPI_Stream.h"



#define CIC_MAX_URL_LENGTH 8192  // Maximum reasonable URL length
#define CIC_MAX_PATH_LENGTH 32767 // Windows extended path limit

CPs_InStream* CP_CreateInStream_LocalFile(const char* pcFlexiURL, HWND hWndOwner);
CPs_InStream* CP_CreateInStream_Internet(const char* pcFlexiURL, HWND hWndOwner);

//
// Validate and sanitize URL/path input
//
static BOOL ValidateURLInput(const char* pcFlexiURL, size_t* piLength)
{
	if (!pcFlexiURL)
		return FALSE;
	
	size_t iLen = strlen(pcFlexiURL);
	*piLength = iLen;
	
	// Check for empty input
	if (iLen == 0)
		return FALSE;
	
	// Check for excessively long URLs (potential DoS)
	if (iLen > CIC_MAX_URL_LENGTH)
	{
		CP_TRACE0("URL too long, exceeds maximum allowed length");
		printf("ValidateURLInput: URL too long: %zu bytes (max %d)\n", iLen, CIC_MAX_URL_LENGTH);
		return FALSE;
	}
	
	// Check for null bytes in the middle (security issue)
	for (size_t i = 0; i < iLen; i++)
	{
		if (pcFlexiURL[i] == '\0')
		{
			CP_TRACE0("URL contains embedded null bytes");
			return FALSE;
		}
	}
	
	return TRUE;
}

//
// Check for directory traversal attempts in file paths
//
static BOOL ContainsDirectoryTraversal(const char* pcPath)
{
	// Check for .. sequences
	if (strstr(pcPath, "..") != NULL)
		return TRUE;
	
	// Check for multiple consecutive slashes (path normalization issue)
	const char* p = pcPath;
	while (*p)
	{
		if ((*p == '\\' || *p == '/') && (*(p+1) == '\\' || *(p+1) == '/'))
			return TRUE;
		p++;
	}
	
	return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
//
//
//
CPs_InStream* CP_CreateInStream(const char* pcFlexiURL, HWND hWndOwner)
{
	CPs_InStream* pNewStream = NULL;
	size_t iURLLen;
	
	// Validate input before processing
	if (!ValidateURLInput(pcFlexiURL, &iURLLen))
	{
		CP_TRACE0("CP_CreateInStream: Invalid URL input");
		return NULL;
	}
	
	CP_TRACE1("CP_CreateInStream: Processing URL: %s", pcFlexiURL);
	printf("CP_CreateInStream: Processing URL: %s\n", pcFlexiURL);
	
	// Check for playlist files FIRST (.pls, .m3u, .m3u8) - case insensitive
	// This needs to be before HTTP/HTTPS detection because playlist URLs are often HTTP/HTTPS
	if (strstr(pcFlexiURL, ".pls") != NULL || strstr(pcFlexiURL, ".PLS") != NULL ||
		strstr(pcFlexiURL, ".m3u") != NULL || strstr(pcFlexiURL, ".M3U") != NULL ||
		strstr(pcFlexiURL, ".m3u8") != NULL || strstr(pcFlexiURL, ".M3U8") != NULL)
	{
		CP_TRACE0("CP_CreateInStream: Detected as playlist file - calling CP_CreateInStream_Internet");
		printf("CP_CreateInStream: Detected as playlist file - calling CP_CreateInStream_Internet\n");
		pNewStream = CP_CreateInStream_Internet(pcFlexiURL, hWndOwner);
		
		if (pNewStream)
		{
			CP_TRACE0("CP_CreateInStream: Successfully created stream from playlist");
			return pNewStream;
		}
		else
		{
			CP_TRACE0("CP_CreateInStream: Failed to create stream from playlist");
		}
	}
	
	// Check for HTTP URLs
	if (iURLLen > 5)
	{
		char cHeader[6];
		memcpy(cHeader, pcFlexiURL, 5);
		cHeader[5] = '\0';
		
		if (stricmp(cHeader, "http:") == 0)
		{
			pNewStream = CP_CreateInStream_Internet(pcFlexiURL, hWndOwner);
			
			if (pNewStream)
				return pNewStream;
		}
	}
	
	// Check for HTTPS URLs
	if (iURLLen > 6)
	{
		char cHttpsHeader[7];
		memcpy(cHttpsHeader, pcFlexiURL, 6);
		cHttpsHeader[6] = '\0';
		
		if (stricmp(cHttpsHeader, "https:") == 0)
		{
			pNewStream = CP_CreateInStream_Internet(pcFlexiURL, hWndOwner);
			
			if (pNewStream)
				return pNewStream;
		}
	}
	
	// Check for icy:// protocol (Icecast/SHOUTcast)
	if (iURLLen > 4)
	{
		char cIcyHeader[5];
		memcpy(cIcyHeader, pcFlexiURL, 4);
		cIcyHeader[4] = '\0';
		
		if (stricmp(cIcyHeader, "icy:") == 0)
		{
			pNewStream = CP_CreateInStream_Internet(pcFlexiURL, hWndOwner);
			
			if (pNewStream)
				return pNewStream;
		}
	}
	
	// Try the local file system
	// Check for directory traversal attempts before opening local files
	if (ContainsDirectoryTraversal(pcFlexiURL))
	{
		CP_TRACE1("CP_CreateInStream: Potential directory traversal detected in path: %s", pcFlexiURL);
		return NULL;
	}
	
	pNewStream = CP_CreateInStream_LocalFile(pcFlexiURL, hWndOwner);
	
	if (pNewStream)
		return pNewStream;
		
	return NULL;
}

//
//
//
