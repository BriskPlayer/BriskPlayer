
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
#include "CompositeFile.h"
#include "CPString.h"
#include <zlib.h>


#define CPC_PKFILE_MAGIC  0x04034B50
#define CPC_PKFILE_DIRMAGIC  0x02014B50
#define CPC_PKFILE_BITS_ENCRYPTED  0x1
#define CPC_PKFILE_BITS_STREAMED  0x8
//
#define CPC_PKFILE_METHOD_STORED  0x0
#define CPC_PKFILE_METHOD_DEFLATED  0x8
#define CPC_PKFILE_MAX_UNCOMPRESSED  0x02000000  // 32MB max decompressed size per entry
////////////////////////////////////////////////////////////////////////////////
//
#pragma pack(push, 1)

typedef struct _CPs_PKFILE_HEADER
{
	DWORD m_dwSig;
	WORD m_wVersion;
	WORD m_wBITs;
	WORD m_wMethod;
	WORD m_wModifyTime;
	WORD m_wModifyDate;
	DWORD m_dwCRC32;
	DWORD m_dwCompressedSize;
	DWORD m_dwDecompressedSize;
	WORD m_wFilenameLen;
	WORD m_wExtraFieldLen;
} CPs_PKFILE_HEADER;

//

typedef struct _CPs_PKFILE_DESCRIPTOR
{
	DWORD m_dwCRC32;
	DWORD m_dwCompressedSize;
	DWORD m_dwDecompressedSize;
	
} CPs_PKFILE_DESCRIPTOR;

#pragma pack(pop)
//
//
//

typedef struct _CPs_SubFile
{
	char* m_pcName;
	DWORD m_dwCRC32;
	unsigned int m_iFileOffset;
	unsigned int m_iCompressedSize;
	unsigned int m_iUncompressedSize;
	WORD m_wMethod;
	
	void* m_pNext;
	
} CPs_SubFile;

//

typedef struct _CPs_CompositeContext
{
	HANDLE m_hFileMapping;
	BYTE* m_pFileBase;
	DWORD m_dwFileSize;
	BOOL m_bMemMappedFile;
	
	CPs_SubFile* m_pFirstSubFile;
	
} CPs_CompositeContext;

//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////


BOOL CP_BuildDirectory(CP_COMPOSITEFILE hComposite);
const CPs_SubFile* CP_FindFile(CP_COMPOSITEFILE hComposite, const char* pcFilename);
////////////////////////////////////////////////////////////////////////////////
//
//
//
CP_COMPOSITEFILE CF_Create_FromFile(const char* pcPath)
{
	CPs_CompositeContext* pContext;
	HANDLE hFile;
	DWORD dwFileSize;
	
	// Convert to Unicode for better filename support
	WCHAR* pwcPath = STR_ConvertToUnicode(pcPath);
	if (!pwcPath)
		return NULL;
	
	// Try to open file
	hFile = CreateFileW(pwcPath,
					   GENERIC_READ,
					   FILE_SHARE_READ,
					   NULL,
					   OPEN_EXISTING,
					   FILE_ATTRIBUTE_NORMAL,
					   NULL);
	
	free(pwcPath);
	                   
	if (hFile == INVALID_HANDLE_VALUE)
		return NULL;
		
	// Check that this file isn't too big (more than 8Mb)
	dwFileSize = GetFileSize(hFile, NULL);
	
	if (dwFileSize > 0x00800000)
	{
		CloseHandle(hFile);
		return NULL;
	}
	
	// Form a memory mapped file from the source
	pContext = (CPs_CompositeContext*)SAFE_MALLOC(sizeof(CPs_CompositeContext));
	if (!pContext)
	{
		CloseHandle(hFile);
		return NULL;
	}
	
	pContext->m_dwFileSize = dwFileSize;
	
	pContext->m_hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	
	CloseHandle(hFile);
	
	if (pContext->m_hFileMapping == INVALID_HANDLE_VALUE)
	{
		// Failed to open file
		free(pContext);
		return NULL;
	}
	
	pContext->m_pFileBase = (BYTE*)MapViewOfFile(pContext->m_hFileMapping, FILE_MAP_READ, 0, 0, 0);
	
	if (!pContext->m_pFileBase)
	{
		// Failed to map file
		CloseHandle(pContext->m_hFileMapping);
		free(pContext);
		return NULL;
	}
	
	// Form a directory over
	
	if (CP_BuildDirectory(pContext) == FALSE)
	{
		CF_Destroy(pContext);
		return NULL;
	}
	
	// Success
	return pContext;
}

//
//
//
CP_COMPOSITEFILE CF_Create_FromResource(HMODULE hModule, UINT uiResourceID, const char* pcResourceType)
{
	CPs_CompositeContext* pContext;
	HRSRC hResource;
	HGLOBAL hResourceData;
	
	// Lock resource
	hResource = FindResource(hModule, MAKEINTRESOURCE(uiResourceID), pcResourceType);
	CP_ASSERT(hResource);
	hResourceData = LoadResource(hModule, hResource);
	CP_ASSERT(hResourceData);
	
	// Form a memory mapped file from the source
	pContext = (CPs_CompositeContext*)SAFE_MALLOC(sizeof(CPs_CompositeContext));
	if (!pContext)
		return NULL;
	pContext->m_dwFileSize = SizeofResource(hModule, hResource);
	pContext->m_hFileMapping = NULL;
	pContext->m_pFileBase = LockResource(hResourceData);
	
	// Form a directory over memory
	
	if (CP_BuildDirectory(pContext) == FALSE)
	{
		CF_Destroy(pContext);
		return NULL;
	}
	
	// Success
	return pContext;
}

//
//
//
void CF_Destroy(CP_COMPOSITEFILE hComposite)
{
	CPs_CompositeContext* pContext = (CPs_CompositeContext*)hComposite;
	CPs_SubFile* pSubFile_Cursor;
	CPs_SubFile* pSubFile_Next;
	CP_CHECKOBJECT(pContext);
	
	// Clean up file mapping (if there was one
	
	if (pContext->m_hFileMapping)
	{
		UnmapViewOfFile(pContext->m_pFileBase);
		CloseHandle(pContext->m_hFileMapping);
	}
	
	// Clean up directory
	
	for (pSubFile_Cursor = pContext->m_pFirstSubFile; pSubFile_Cursor; pSubFile_Cursor = pSubFile_Next)
	{
		pSubFile_Next = (CPs_SubFile*)pSubFile_Cursor->m_pNext;
		free(pSubFile_Cursor->m_pcName);
		free(pSubFile_Cursor);
	}
	
	free(pContext);
}

//
//
//
BOOL CF_GetSubFile(CP_COMPOSITEFILE hComposite, const char* pcSubfilename, void** ppSubFile_Uncompressed, unsigned int* piSubFile_Length)
{
	CPs_CompositeContext* pContext = (CPs_CompositeContext*)hComposite;
	const CPs_SubFile* pSubFile;
	DWORD dwCRC32;
	CP_CHECKOBJECT(pContext);
	
	pSubFile = CP_FindFile(hComposite, pcSubfilename);
	
	if (!pSubFile)
	{
		*ppSubFile_Uncompressed = NULL;
		*piSubFile_Length = 0;
		return FALSE;
	}
	
	// Create dest block - reject unreasonably large decompressed sizes (ZIP bomb protection)
	if (pSubFile->m_iUncompressedSize > CPC_PKFILE_MAX_UNCOMPRESSED)
	{
		*ppSubFile_Uncompressed = NULL;
		*piSubFile_Length = 0;
		return FALSE;
	}
	
	*ppSubFile_Uncompressed = SAFE_MALLOC(pSubFile->m_iUncompressedSize);
	if (!*ppSubFile_Uncompressed)
	{
		*piSubFile_Length = 0;
		return FALSE;
	}
	
	*piSubFile_Length = pSubFile->m_iUncompressedSize;
	
	// Validate that the compressed data region is within the mapped file
	if (pSubFile->m_iFileOffset + pSubFile->m_iCompressedSize < pSubFile->m_iFileOffset
			|| pSubFile->m_iFileOffset + pSubFile->m_iCompressedSize > pContext->m_dwFileSize)
	{
		free(*ppSubFile_Uncompressed);
		*ppSubFile_Uncompressed = NULL;
		*piSubFile_Length = 0;
		return FALSE;
	}
	
	if (pSubFile->m_wMethod == CPC_PKFILE_METHOD_STORED)
	{
		if (pSubFile->m_iCompressedSize != pSubFile->m_iUncompressedSize)
		{
			free(*ppSubFile_Uncompressed);
			*ppSubFile_Uncompressed = NULL;
			*piSubFile_Length = 0;
			return FALSE;
		}
		memcpy(*ppSubFile_Uncompressed, pContext->m_pFileBase + pSubFile->m_iFileOffset, *piSubFile_Length);
	}
	
	else if (pSubFile->m_wMethod == CPC_PKFILE_METHOD_DEFLATED)
	{
		z_stream zStream;
		
		zStream.zalloc = Z_NULL;
		zStream.zfree = Z_NULL;
		zStream.opaque = Z_NULL;
		zStream.data_type = Z_BINARY;
		inflateInit2(&zStream, -15);  // 15bit window (32Kb window size) (-ve to use an undocumented zLib "no zLib headers" mode)
		
		// Decompress
		zStream.next_out = (BYTE*) * ppSubFile_Uncompressed;
		zStream.avail_out = *piSubFile_Length;
		zStream.next_in = pContext->m_pFileBase + pSubFile->m_iFileOffset;
		zStream.avail_in = pSubFile->m_iCompressedSize;
		inflate(&zStream, Z_FINISH);
		
		// Cleanup
		inflateEnd(&zStream);
	}
	
	// Check CRC32
	dwCRC32 = crc32(0, *ppSubFile_Uncompressed, *piSubFile_Length);
	
	if (dwCRC32 != pSubFile->m_dwCRC32)
	{
		// CRC32 does not match
		free(*ppSubFile_Uncompressed);
		*ppSubFile_Uncompressed = NULL;
		*piSubFile_Length = 0;
		return FALSE;
	}
	
	return TRUE;
}

//
//
//
BOOL CP_BuildDirectory(CP_COMPOSITEFILE hComposite)
{
	CPs_CompositeContext* pContext = (CPs_CompositeContext*)hComposite;
	unsigned int iOffset;
	CP_CHECKOBJECT(pContext);
	
	// Initialise structure
	pContext->m_pFirstSubFile = NULL;
	
	// Scan the composite for the file headers (ignore the end directory stuff)
	iOffset = 0;
	
	while (((iOffset + sizeof(CPs_PKFILE_HEADER)) <= pContext->m_dwFileSize)
			&& *(DWORD*)(pContext->m_pFileBase + iOffset) != CPC_PKFILE_DIRMAGIC)
	{
		CPs_PKFILE_HEADER* pHeader = (CPs_PKFILE_HEADER*)(pContext->m_pFileBase + iOffset);
		CPs_SubFile* pNewSubFile;
		unsigned int iEntrySize;
		
		if (pHeader->m_dwSig != CPC_PKFILE_MAGIC
				|| (pHeader->m_wBITs & CPC_PKFILE_BITS_ENCRYPTED)
				|| (pHeader->m_wBITs & CPC_PKFILE_BITS_STREAMED)
				|| (pHeader->m_wMethod != CPC_PKFILE_METHOD_STORED && pHeader->m_wMethod != CPC_PKFILE_METHOD_DEFLATED))
		{
			CP_TRACE0("ZIP format not understood");
			return FALSE;
		}
		
		// Validate that all header-referenced regions fall within the mapped file
		iEntrySize = (unsigned int)sizeof(*pHeader)
				   + (unsigned int)pHeader->m_wFilenameLen
				   + (unsigned int)pHeader->m_wExtraFieldLen
				   + pHeader->m_dwCompressedSize;
		
		// Check for integer overflow and out-of-bounds
		if (iEntrySize < pHeader->m_dwCompressedSize
				|| iOffset + iEntrySize < iOffset
				|| iOffset + iEntrySize > pContext->m_dwFileSize)
		{
			CP_TRACE0("ZIP entry exceeds file bounds");
			return FALSE;
		}
		
		// Create subfile object
		pNewSubFile = (CPs_SubFile*)SAFE_MALLOC(sizeof(*pNewSubFile));
		if (!pNewSubFile)
		{
			CP_TRACE0("Failed to allocate subfile");
			return FALSE;
		}
		
		pNewSubFile->m_pNext = pContext->m_pFirstSubFile;
		
		pContext->m_pFirstSubFile = pNewSubFile;
		
		// Init subfile members
		pNewSubFile->m_pcName = (char*)SAFE_MALLOC(pHeader->m_wFilenameLen + 1);
		if (!pNewSubFile->m_pcName)
		{
			CP_TRACE0("Failed to allocate subfile name");
			free(pNewSubFile);
			return FALSE;
		}
		memcpy(pNewSubFile->m_pcName, pContext->m_pFileBase + iOffset + sizeof(*pHeader), pHeader->m_wFilenameLen);
		
		pNewSubFile->m_pcName[pHeader->m_wFilenameLen] = '\0';
		pNewSubFile->m_wMethod = pHeader->m_wMethod;
		pNewSubFile->m_dwCRC32 = pHeader->m_dwCRC32;
		pNewSubFile->m_iCompressedSize = pHeader->m_dwCompressedSize;
		pNewSubFile->m_iUncompressedSize = pHeader->m_dwDecompressedSize;
		pNewSubFile->m_iFileOffset = iOffset + sizeof(*pHeader) + pHeader->m_wFilenameLen + pHeader->m_wExtraFieldLen;
		CP_TRACE1("SubFile:\"%s\"", pNewSubFile->m_pcName);
		
		// Skip to next file (overflow already checked above)
		iOffset += iEntrySize;
	}
	
	
	return TRUE;
}

//
//
//
const CPs_SubFile* CP_FindFile(CP_COMPOSITEFILE hComposite, const char* pcFilename)
{
	CPs_CompositeContext* pContext = (CPs_CompositeContext*)hComposite;
	const CPs_SubFile* pSubFile_Cursor;
	CP_CHECKOBJECT(pContext);
	
	// Search for subfile
	
	for (pSubFile_Cursor = pContext->m_pFirstSubFile; pSubFile_Cursor; pSubFile_Cursor = (const CPs_SubFile*)pSubFile_Cursor->m_pNext)
	{
		if (stricmp(pSubFile_Cursor->m_pcName, pcFilename) == 0)
			return pSubFile_Cursor;
	}
	
	return NULL;
}

//
//
//
