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
#include "CPI_Playlist.h"
#include "CPI_PlaylistItem.h"
#include "CPI_PlaylistItem_Internal.h"
#include "CPI_TagLib.h"
#include "ogg/ogg.h"
#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"
#include "CP_RIFFStructs.h"

void CPLI_OGG_SkipOverTab(FILE* pFile);
void CPLI_SetPath(CPs_PlaylistItem* pItem, const char* pcNewPath);
void CPLI_ReadTag_TagLib(CPs_PlaylistItem* pItem);
void CPLI_WriteTag_TagLib(CPs_PlaylistItem* pItem);
void CPLI_ReadTag_OGG(CPs_PlaylistItem* pItem);
void CPLI_CalculateLength_OGG(CPs_PlaylistItem* pItem);
void CPLI_CalculateLength_MP3(CPs_PlaylistItem* pItem);
void CPLI_CalculateLength_WAV(CPs_PlaylistItem* pItem);
////////////////////////////////////////////////////////////////////////////////
//
//
//
CP_HPLAYLISTITEM CPLII_CreateItem(const char* pcPath)
{
	CPs_PlaylistItem* pNewItem = (CPs_PlaylistItem*)malloc(sizeof(CPs_PlaylistItem));
	
	pNewItem->m_pcPath = NULL;
	CPLI_SetPath(pNewItem, pcPath);
	
	pNewItem->m_cTrackStackPos_AsText[0] = '\0';
	pNewItem->m_iTrackStackPos = CIC_TRACKSTACK_UNSTACKED;
	pNewItem->m_enTagType = ttUnread;
	pNewItem->m_bID3Tag_SaveRequired = FALSE;
	pNewItem->m_bDestroyOnDeactivate = FALSE;
	pNewItem->m_pcArtist = NULL;
	pNewItem->m_pcAlbum = NULL;
	pNewItem->m_pcTrackName = NULL;
	pNewItem->m_pcComment = NULL;
	pNewItem->m_pcYear = NULL;
	pNewItem->m_cGenre = CIC_INVALIDGENRE;
	pNewItem->m_cTrackNum = CIC_INVALIDTRACKNUM;
	pNewItem->m_pcTrackNum_AsText = NULL;
	pNewItem->m_iTrackLength = 0;
	pNewItem->m_pcTrackLength_AsText = NULL;
	
	pNewItem->m_iCookie = -1;
	
	pNewItem->m_hNext = NULL;
	pNewItem->m_hPrev = NULL;
	
	return pNewItem;
}

//
//
//
void CPLII_RemoveTagInfo(CPs_PlaylistItem* pItem)
{
	if (pItem->m_pcArtist)
	{
		free(pItem->m_pcArtist);
		pItem->m_pcArtist = NULL;
	}
	
	if (pItem->m_pcAlbum)
	{
		free(pItem->m_pcAlbum);
		pItem->m_pcAlbum = NULL;
	}
	
	if (pItem->m_pcTrackName)
	{
		free(pItem->m_pcTrackName);
		pItem->m_pcTrackName = NULL;
	}
	
	if (pItem->m_pcComment)
	{
		free(pItem->m_pcComment);
		pItem->m_pcComment = NULL;
	}
	
	if (pItem->m_pcYear)
	{
		free(pItem->m_pcYear);
		pItem->m_pcYear = NULL;
	}
	
	if (pItem->m_pcTrackNum_AsText)
	{
		free(pItem->m_pcTrackNum_AsText);
		pItem->m_pcTrackNum_AsText = NULL;
	}
	
	if (pItem->m_pcTrackLength_AsText)
	{
		free(pItem->m_pcTrackLength_AsText);
		pItem->m_pcTrackLength_AsText = NULL;
	}
	
	pItem->m_cGenre = CIC_INVALIDGENRE;
	
	pItem->m_iTrackLength = 0;
	pItem->m_cTrackNum = CIC_INVALIDTRACKNUM;
	pItem->m_enTagType = ttUnread;
	pItem->m_bID3Tag_SaveRequired = FALSE;
}

//
//
//
void CPLII_DestroyItem(CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	CPLII_RemoveTagInfo(pItem);
	free(pItem->m_pcPath);
	free(pItem);
}

//
//
//
const char* CPLI_GetPath(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	return pItem->m_pcPath;
}

//
//
//
const char* CPLI_GetFilename(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	return pItem->m_pcFilename;
}

//
//
//
int CPLI_GetTrackStackPos(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	return pItem->m_iTrackStackPos;
}

//
//
//
const char* CPLI_GetTrackStackPos_AsText(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	return pItem->m_cTrackStackPos_AsText;
}

//
//
//
const char* CPLI_GetArtist(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	return pItem->m_pcArtist;
}

//
//
//
const char* CPLI_GetAlbum(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	return pItem->m_pcAlbum;
}

//
//
//
const char* CPLI_GetTrackName(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	return pItem->m_pcTrackName;
}

//
//
//
const char* CPLI_GetComment(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	return pItem->m_pcComment;
}

//
//
//
const char* CPLI_GetYear(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	return pItem->m_pcYear;
}

//
//
//
const char* CPLI_GetGenre(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	if (pItem->m_cGenre == CIC_INVALIDGENRE)
		return NULL;
		
	return glb_pcGenres[pItem->m_cGenre];
}

//
//
//
unsigned char CPLI_GetTrackNum(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	return pItem->m_cTrackNum;
}

//
//
//
const char* CPLI_GetTrackNum_AsText(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	return pItem->m_pcTrackNum_AsText;
}

//
//
//
const char* CPLI_GetTrackLength_AsText(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	return pItem->m_pcTrackLength_AsText;
}

//
//
//
int CPLI_GetTrackLength(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	return pItem->m_iTrackLength;
}

//
//
//
void CPLI_SetCookie(CP_HPLAYLISTITEM hItem, const int iCookie)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	pItem->m_iCookie = iCookie;
}

//
//
//
int CPLI_GetCookie(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	return pItem->m_iCookie;
}

//
//
//
CP_HPLAYLISTITEM CPLI_Next(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	return pItem->m_hNext;
}

//
//
//
CP_HPLAYLISTITEM CPLI_Prev(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	return pItem->m_hPrev;
}

//
//
//
void CPLI_ReadTag(CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	
	CP_CHECKOBJECT(pItem);
	
	if (pItem->m_enTagType != ttUnread)
		return;
		
	// Use TagLib to read metadata
	CPLI_ReadTag_TagLib(pItem);
	
	// Override information with any OGG tags that may be there (if native OGG tags are preferred)
	if (options.prefer_native_ogg_tags
			&& stricmp(".ogg", CPLI_GetExtension(hItem)) == 0)
	{
		CPLI_ReadTag_OGG(pItem);
	}
	
	// Update interface
	CPL_cb_OnItemUpdated(hItem);
}

//
//
//
char* CPLI_ID3v2_DecodeString(const BYTE* pSourceText, const int iTagDataSize)
{
	int iStringLength;
	char* pcDestString;
	
	if (pSourceText[0] == '\0')
	{
		iStringLength = iTagDataSize - 1;
		pcDestString = malloc(iStringLength + 1);
		memcpy(pcDestString, pSourceText + 1, iStringLength);
		pcDestString[iStringLength] = 0;
	}
	
	else
	{
		CP_TRACE0("ID3v2 Unknown encoding");
		pcDestString = NULL;
	}
	
	return pcDestString;
}

//
//
//
void CPLI_DecodeLength(CPs_PlaylistItem* pItem, unsigned int iNewLength)
{
	int iHours, iMins, iSecs;
	
	// Free existing buffer
	
	if (pItem->m_pcTrackLength_AsText)
	{
		free(pItem->m_pcTrackLength_AsText);
		pItem->m_pcTrackLength_AsText = NULL;
	}
	
	pItem->m_iTrackLength = iNewLength;
	
	iHours = iNewLength / 3600;
	iMins = (iNewLength - (iHours * 3600)) / 60;
	iSecs = iNewLength - (iHours * 3600) - (iMins * 60);
	
	// If length has hours then format as hh:mm:ss otherwise format as mm:ss
	
	if (iHours > 0)
	{
		pItem->m_pcTrackLength_AsText = (char*)malloc(9);
		sprintf(pItem->m_pcTrackLength_AsText, "%02d:%02d:%02d", iHours, iMins, iSecs);
	}
	
	else
	{
		pItem->m_pcTrackLength_AsText = (char*)malloc(6);
		sprintf(pItem->m_pcTrackLength_AsText, "%02d:%02d", iMins, iSecs);
	}
}

//
//
//
//
//
//
// Temporary stub - old ID3v1 function replaced by TagLib  
void CPLI_ReadTag_ID3v1(CPs_PlaylistItem* pItem, HANDLE hFile)
{
	// This function is deprecated - use CPLI_ReadTag_TagLib instead
	(void)pItem;  // Suppress unused parameter warning
	(void)hFile;  // Suppress unused parameter warning
	return;
}

//
//
//
BOOL CPLI_IsTagDirty(CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	
	CP_CHECKOBJECT(pItem);
	return pItem->m_bID3Tag_SaveRequired;
}

//
//
//
void CPLI_WriteTag(CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	
	CP_CHECKOBJECT(pItem);
	
	if (pItem->m_bID3Tag_SaveRequired == FALSE)
		return;
		
	// Check if file format is supported
	if (stricmp(".ogg", CPLI_GetExtension(hItem)) != 0 &&
			stricmp(".mp3", CPLI_GetExtension(hItem)) != 0 &&
			stricmp(".flac", CPLI_GetExtension(hItem)) != 0 &&
			stricmp(".m4a", CPLI_GetExtension(hItem)) != 0 &&
			stricmp(".mp4", CPLI_GetExtension(hItem)) != 0)
		return;
		
	// Check if we can write to the file
	if (!CPTL_CanWriteToFile(pItem->m_pcPath))
		return;
		
	pItem->m_bID3Tag_SaveRequired = FALSE;

	// Use TagLib for all metadata writing
	CPLI_WriteTag_TagLib(pItem);
}

//
//
//
//
CPe_ReadWriteState CPLI_GetReadWriteState(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	HANDLE hFile;
	CP_CHECKOBJECT(pItem);
	
	// We will check this every time (and not cache the result) because the
	// file could have been played with outside of CoolPlayer
	
	// Try to open the file in RW mode
	hFile = CreateFile(pItem->m_pcPath, GENERIC_READ | GENERIC_WRITE,
					   FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
					   OPEN_EXISTING, 0, 0);
	                   
	if (hFile != INVALID_HANDLE_VALUE)
	{
		// Only cache
		CloseHandle(hFile);
		return rwsReadWrite;
	}
	
	// That didn't work - try a RO open
	hFile = CreateFile(pItem->m_pcPath, GENERIC_READ,
					   FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
					   OPEN_EXISTING, 0, 0);
	                   
	if (hFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hFile);
		return rwsReadOnly;
	}
	
	return rwsBadFile;
}

//
//
//
void CPLI_SetArtist(CP_HPLAYLISTITEM hItem, const char* pcNewValue)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	STR_AllocSetString(&pItem->m_pcArtist, pcNewValue, TRUE);
	pItem->m_bID3Tag_SaveRequired = TRUE;
	CPL_cb_OnItemUpdated(hItem);
}

//
//
//
void CPLI_SetAlbum(CP_HPLAYLISTITEM hItem, const char* pcNewValue)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	STR_AllocSetString(&pItem->m_pcAlbum, pcNewValue, TRUE);
	pItem->m_bID3Tag_SaveRequired = TRUE;
	CPL_cb_OnItemUpdated(hItem);
}

//
//
//
void CPLI_SetTrackName(CP_HPLAYLISTITEM hItem, const char* pcNewValue)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	STR_AllocSetString(&pItem->m_pcTrackName, pcNewValue, TRUE);
	pItem->m_bID3Tag_SaveRequired = TRUE;
	CPL_cb_OnItemUpdated(hItem);
}

//
//
//
void CPLI_SetYear(CP_HPLAYLISTITEM hItem, const char* pcNewValue)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	STR_AllocSetString(&pItem->m_pcYear, pcNewValue, TRUE);
	pItem->m_bID3Tag_SaveRequired = TRUE;
	CPL_cb_OnItemUpdated(hItem);
}

//
//
//
void CPLI_SetGenreIDX(CP_HPLAYLISTITEM hItem, const unsigned char iNewValue)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	pItem->m_cGenre = iNewValue;
	pItem->m_bID3Tag_SaveRequired = TRUE;
	CPL_cb_OnItemUpdated(hItem);
}

//
//
//
void CPLI_SetTrackNum(CP_HPLAYLISTITEM hItem, const unsigned char iNewValue)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	char cTempString[33];
	CP_CHECKOBJECT(pItem);
	
	pItem->m_cTrackNum = iNewValue;
	
	if (pItem->m_cTrackNum != CIC_INVALIDTRACKNUM)
	{
		if (pItem->m_pcTrackNum_AsText)
			free(pItem->m_pcTrackNum_AsText);
			
		pItem->m_pcTrackNum_AsText = (char*)malloc(CPC_TRACKNUMASTEXTBUFFERSIZE);
		
		_itoa(pItem->m_cTrackNum, cTempString, 10);
		
		strncpy(pItem->m_pcTrackNum_AsText, cTempString, CPC_TRACKNUMASTEXTBUFFERSIZE);
	}
	
	else
	{
		if (pItem->m_pcTrackNum_AsText)
		{
			free(pItem->m_pcTrackNum_AsText);
			pItem->m_pcTrackNum_AsText = NULL;
		}
	}
	
	pItem->m_bID3Tag_SaveRequired = TRUE;
	
	CPL_cb_OnItemUpdated(hItem);
}

//
//
//
void CPLI_SetTrackNum_AsText(CP_HPLAYLISTITEM hItem, const char* pcNewValue)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	if (pcNewValue[0] == '\0')
		pItem->m_cTrackNum = CIC_INVALIDTRACKNUM;
	else
		pItem->m_cTrackNum = (unsigned char)atoi(pcNewValue);
		
	if (pItem->m_pcTrackNum_AsText)
		free(pItem->m_pcTrackNum_AsText);
		
	pItem->m_pcTrackNum_AsText = (char*)malloc(CPC_TRACKNUMASTEXTBUFFERSIZE);
	strncpy(pItem->m_pcTrackNum_AsText, pcNewValue, CPC_TRACKNUMASTEXTBUFFERSIZE);
	
	pItem->m_bID3Tag_SaveRequired = TRUE;
	
	CPL_cb_OnItemUpdated(hItem);
}

//
//
//
void CPLI_SetComment(CP_HPLAYLISTITEM hItem, const char* pcNewValue)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	STR_AllocSetString(&pItem->m_pcComment, pcNewValue, TRUE);
	pItem->m_bID3Tag_SaveRequired = TRUE;
	CPL_cb_OnItemUpdated(hItem);
}

//
//
//
void CPLI_SetTrackStackPos(CP_HPLAYLISTITEM hItem, const int iNewPos)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	CP_CHECKOBJECT(pItem);
	
	pItem->m_iTrackStackPos = iNewPos;
	
	if (iNewPos == 0)
	{
		pItem->m_cTrackStackPos_AsText[0] = '>';
		pItem->m_cTrackStackPos_AsText[1] = '>';
		pItem->m_cTrackStackPos_AsText[2] = '>';
		pItem->m_cTrackStackPos_AsText[3] = '\0';
	}
	
	else if (iNewPos == (int)CIC_TRACKSTACK_UNSTACKED)
	{
		pItem->m_cTrackStackPos_AsText[0] = '\0';
	}
	
	else
	{
		_snprintf(pItem->m_cTrackStackPos_AsText, sizeof(pItem->m_cTrackStackPos_AsText), "%d", iNewPos);
	}
}

//
//
//
void CPLI_CalculateLength(CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	const char* pcExtension;
	
	CP_CHECKOBJECT(pItem);
	
	pcExtension = CPLI_GetExtension(hItem);
	
	if (stricmp(pcExtension, ".ogg") == 0)
		CPLI_CalculateLength_OGG(pItem);
	else if (stricmp(pcExtension, ".mp3") == 0
			 || stricmp(pcExtension, ".mp2") == 0)
	{
		CPLI_CalculateLength_MP3(pItem);
	}
	else if (stricmp(pcExtension, ".wav") == 0)
	{
		CPLI_CalculateLength_WAV(pItem);
	}
	
//    pItem->m_bID3Tag_SaveRequired = TRUE;
	CPL_cb_OnItemUpdated(hItem);
}

//
//
//
void CPLI_CalculateLength_OGG(CPs_PlaylistItem* pItem)
{
	FILE *hFile;
	OggVorbis_File vorbisfileinfo;
	
	hFile = fopen(pItem->m_pcPath, "rb");
	
	if (hFile == NULL)
		return;
		
	CPLI_OGG_SkipOverTab(hFile);
	
	memset(&vorbisfileinfo, 0, sizeof(vorbisfileinfo));
	
	if (ov_open(hFile, &vorbisfileinfo, NULL, 0) < 0)
	{
		fclose(hFile);
		return;
	}
	
	CPLI_DecodeLength(pItem, (int)ov_time_total(&vorbisfileinfo, -1));
	
	ov_clear(&vorbisfileinfo);
	fclose(hFile);
}

//
//
//
/** // TODO: - reformat and check - move all this to separate function **/
// at the moment CPI_Player_CoDec_WAV has this same code
void CPLI_CalculateLength_WAV(CPs_PlaylistItem* pItem)
{
	
	FILE* hFile;
	DWORD bps;
	extern BOOL SkipToChunk(HANDLE hFile, CPs_RIFFChunk* pChunk, const char cChunkID[4]); // in CoDec_WAV.c
	CP_TRACE1("Openfile \"%s\"", pItem->m_pcPath);
	
	hFile = CreateFile(pItem->m_pcPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
	
	if (hFile == INVALID_HANDLE_VALUE)
	{
		CP_TRACE0("Failed to open file");
		return;
	}
	
	// Skip over ID3v2 tag (if there is one) 
	// TODO: Use TagLib to detect and skip ID3v2 tags properly
	{
		char tag_header[10];
		DWORD dwBytesRead;
		int iStreamStart = 0;
		
		ReadFile(hFile, tag_header, 10, &dwBytesRead, NULL);
		
		if (dwBytesRead == 10 && memcmp(tag_header, "ID3", 3) == 0)
		{
			// Simple ID3v2 tag size calculation (sync-safe format)
			iStreamStart = 10; // Header size
			iStreamStart += ((tag_header[6] & 0x7F) << 21)
							| ((tag_header[7] & 0x7F) << 14)
							| ((tag_header[8] & 0x7F) << 7)
							| (tag_header[9] & 0x7F);
		}
		
		SetFilePointer(hFile, iStreamStart, NULL, FILE_BEGIN);
	}
	
	// Check the header
	{
		CPs_RIFFHeader RIFFHeader;
		DWORD dwBytesRead;
		ReadFile(hFile, &RIFFHeader, sizeof(RIFFHeader), &dwBytesRead, NULL);
		
		if (memcmp(RIFFHeader.m_cID, "RIFF", 4) || memcmp(RIFFHeader.m_cFileType, "WAVE", 4))
		{
			CP_TRACE0("File not of RIFF WAVE type");
			CloseHandle(hFile);
			hFile = INVALID_HANDLE_VALUE;
			return;
		}
	}
	
	// Check the format of the WAV file
	{
		CPs_RIFFChunk chunk;
		PCMWAVEFORMAT* pFormat;
		DWORD dwBytesRead;
		BOOL bSuccess = SkipToChunk(hFile, &chunk, "fmt ");
		
		if (bSuccess == FALSE || chunk.m_dwLength < sizeof(PCMWAVEFORMAT))
		{
			CP_TRACE0("Failed to find FMT chunk");
			CloseHandle(hFile);
			hFile = INVALID_HANDLE_VALUE;
			return;
		}
		
		// Get the format data
		pFormat = (PCMWAVEFORMAT*)malloc(chunk.m_dwLength);
		
		ReadFile(hFile, pFormat, chunk.m_dwLength, &dwBytesRead, NULL);
		
		// We only handle PCM encoded data
		if (dwBytesRead != chunk.m_dwLength || pFormat->wf.wFormatTag != WAVE_FORMAT_PCM)
		{
			CP_TRACE0("Only PCM data supported!");
			free(pFormat);
			CloseHandle(hFile);
			hFile = INVALID_HANDLE_VALUE;
			return;
		}
		
		// Setup file info struct - this is re-read every second
		bps = (pFormat->wBitsPerSample * pFormat->wf.nChannels * pFormat->wf.nSamplesPerSec) / 8;
		
		free(pFormat);
	}
	
	// Dip into the DATA chunk
	{
		CPs_RIFFChunk chunk;
		int iFileLength_Secs = 0;
		BOOL bSuccess = SkipToChunk(hFile, &chunk, "data");
		int iLengthOfWavData;
		
		if (bSuccess == FALSE)
		{
			CP_TRACE0("Failed to find WAVE chunk");
			CloseHandle(hFile);
			hFile = INVALID_HANDLE_VALUE;
			return;
		}
		
		// Get info about the length of this chunk
		iLengthOfWavData = chunk.m_dwLength;
		SetFilePointer(hFile, 0, NULL, FILE_CURRENT);
		iFileLength_Secs = iLengthOfWavData / bps; //iBytesPerSecond;
		
		CPLI_DecodeLength(pItem, iFileLength_Secs);
		
		return;
	}
	
}

//
//
//
void CPLI_CalculateLength_MP3(CPs_PlaylistItem* pItem)
{
	BYTE pbBuffer[0x8000];
	unsigned int iBufferCursor;
	DWORD dwBufferSize;
	HANDLE hFile;
	BOOL bFoundFrameHeader;
	int iBitRate;
	DWORD dwFileSize;
	int iMPEG_version;
	int iLayer;
	BOOL bMono;
	unsigned int iVBRHeader;
	
	// - Try to open the file
	hFile = CreateFile(pItem->m_pcPath, GENERIC_READ,
					   FILE_SHARE_READ, 0,
					   OPEN_EXISTING, 0, 0);
	dwFileSize = GetFileSize(hFile, NULL);
	
	// Cannot open - fail silently
	
	if (hFile == INVALID_HANDLE_VALUE)
		return;
		
	// Read the first 64K of the file (that should contain the first frame header!)
	ReadFile(hFile, pbBuffer, sizeof(pbBuffer), &dwBufferSize, NULL);
	
	CloseHandle(hFile);
	
	iBufferCursor = 0;
	
	// Skip over a any ID3v2 tag
	// TODO: Use TagLib to detect and skip ID3v2 tags properly
	{
		if (dwBufferSize >= 10 && memcmp(pbBuffer + iBufferCursor, "ID3", 3) == 0)
		{
			// Simple ID3v2 tag size calculation (sync-safe format)
			iBufferCursor += ((pbBuffer[iBufferCursor + 6] & 0x7F) << 21)
							 | ((pbBuffer[iBufferCursor + 7] & 0x7F) << 14)
							 | ((pbBuffer[iBufferCursor + 8] & 0x7F) << 7)
							 | (pbBuffer[iBufferCursor + 9] & 0x7F);
			iBufferCursor += 10; // count the header
		}
	}
	
	// Seek to the start of the first frame
	bFoundFrameHeader = FALSE;
	
	while (iBufferCursor < (dwBufferSize - 4))
	{
		if (pbBuffer[iBufferCursor] == 0xFF
				&& (pbBuffer[iBufferCursor+1] & 0xE0) == 0xE0)
		{
			bFoundFrameHeader = TRUE;
			break;
		}
		
		iBufferCursor++;
	}
	
	if (bFoundFrameHeader == FALSE)
		return;
		
	// Work out MPEG version
	if (((pbBuffer[iBufferCursor+1] >> 3) & 0x3) == 0x3)
		iMPEG_version = 1;
	else
		iMPEG_version = 2;
		
	// Work out layer
	iLayer = 0x4 - ((pbBuffer[iBufferCursor+1] >> 1) & 0x3);
	
	if (iLayer == 0)
		return;
		
	// Work out stereo
	if ((pbBuffer[iBufferCursor+3] >> 6) == 0x3)
		bMono = TRUE;
	else
		bMono = FALSE;
		
	// Work out the VBR header should be
	if (iMPEG_version == 1)
		iVBRHeader = (iBufferCursor + 4) + (bMono ? 17 : 32);
	else
		iVBRHeader = (iBufferCursor + 4) + (bMono ? 9 : 17);
		
		
	// Is this a VBR file
	if ((iBufferCursor + iVBRHeader + 12) < dwBufferSize
			&& pbBuffer[iVBRHeader] == 'X'
			&& pbBuffer[iVBRHeader+1] == 'i'
			&& pbBuffer[iVBRHeader+2] == 'n'
			&& pbBuffer[iVBRHeader+3] == 'g')
	{
		int iNumberOfFrames;
		int iFreq;
		int iDetailedVersion;
		const int aryFrequencies[3][3] =
		{
			{44100, 48000, 32000}, //MPEG 1
			{22050, 24000, 16000}, //MPEG 2
			{32000, 16000,  8000}  //MPEG 2.5
		};
		
		if (((pbBuffer[iBufferCursor+1] >> 3) & 0x3) == 0x3)
			iDetailedVersion = 1;
		else if (((pbBuffer[iBufferCursor+1] >> 3) & 0x3) == 0x2)
			iDetailedVersion = 2;
		else
			iDetailedVersion = 3;
			
		// Get the number of frames from the Xing header
		iNumberOfFrames = (pbBuffer[iVBRHeader+8] << 24)
						  | (pbBuffer[iVBRHeader+9] << 16)
						  | (pbBuffer[iVBRHeader+10] << 8)
						  | pbBuffer[iVBRHeader+11];
		                  
		if (((pbBuffer[iBufferCursor+2] >> 2) & 0x3) == 0x3)
			return;
			
		iFreq = aryFrequencies[iDetailedVersion-1][(pbBuffer[iBufferCursor+2] >> 2) & 0x3];
		
		CPLI_DecodeLength(pItem, (8 * iNumberOfFrames * 144) / iFreq);
	}
	
	// Work out the bit rate for a CBR file
	
	else
	{
		const int aryBitRates[2][3][16] =
		{
			{         //MPEG 2 & 2.5
				{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0}, //Layer I
				{0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}, //Layer II
				{0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}  //Layer III
			}, {      //MPEG 1
				{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0}, //Layer I
				{0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0}, //Layer II
				{0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}  //Layer III
			}
		};
		
		iBitRate = aryBitRates[2-iMPEG_version][iLayer-1][pbBuffer[iBufferCursor+2] >> 4];
		
		if (iBitRate)
			CPLI_DecodeLength(pItem, (dwFileSize*8) / (iBitRate*1000));
	}
}

//
//
//
BOOL CPLI_RenameTrack(CP_HPLAYLISTITEM hItem, const CPe_FilenameFormat enFormat)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	char cPath[MAX_PATH];
	char cNewPath[MAX_PATH];
	BOOL bMoved;
	const char* pcExtension;
	
	CP_CHECKOBJECT(pItem);
	
	strncpy(cPath, pItem->m_pcPath, MAX_PATH);
	
	// Remove the filename from the path
	{
		int iLastSlashIDX, iCharIDX;
		iLastSlashIDX = CPC_INVALIDCHAR;
		
		for (iCharIDX = 0; cPath[iCharIDX]; iCharIDX++)
		{
			if (cPath[iCharIDX] == '\\')
				iLastSlashIDX = iCharIDX;
		}
		
		if (iLastSlashIDX != CPC_INVALIDCHAR)
			cPath[iLastSlashIDX] = '\0';
	}
	
	pcExtension = CPLI_GetExtension(hItem);
	
	// Apply the name format
	{
		char cNewFilename[MAX_PATH];
		const char* pcTitle;
		const char* pcArtist;
		const char* pcAlbum;
		
		if (pItem->m_pcTrackName)
			pcTitle = pItem->m_pcTrackName;
		else
			pcTitle = "<title>";
			
		if (pItem->m_pcArtist)
			pcArtist = pItem->m_pcArtist;
		else
			pcArtist = "<title>";
			
		if (pItem->m_pcAlbum)
			pcAlbum = pItem->m_pcAlbum;
		else
			pcAlbum = "<album>";
			
			
		switch (enFormat)
		{
		
			case rwsArtistAlbumNumberTitle:
				sprintf(cNewFilename, "%s - %s - %02d - %s%s", pcArtist, pcAlbum, (int)pItem->m_cTrackNum, pcTitle, pcExtension);
				break;
				
			case rwsArtistNumberTitle:
				sprintf(cNewFilename, "%s - %02d - %s%s", pcArtist, (int)pItem->m_cTrackNum, pcTitle, pcExtension);
				break;
				
			case rwsAlbumNumberTitle:
				sprintf(cNewFilename, "%s - %02d - %s%s", pcAlbum, (int)pItem->m_cTrackNum, pcTitle, pcExtension);
				break;
				
			case rwsAlbumNumber:
				sprintf(cNewFilename, "%s - %02d%s", pcAlbum, (int)pItem->m_cTrackNum, pcExtension);
				break;
				
			case rwsNumberTitle:
				sprintf(cNewFilename, "%02d - %s%s", (int)pItem->m_cTrackNum, pcTitle, pcExtension);
				break;
				
			case rwsTitle:
				sprintf(cNewFilename, "%s%s", pcTitle, pcExtension);
				break;
				
			default:
				CP_FAIL("Unknown rename format");
		}
		
		// Replace illegal chars with _
		{
			int iCharIDX;
			
			for (iCharIDX = 0; cNewFilename[iCharIDX]; iCharIDX++)
			{
				if (cNewFilename[iCharIDX] == '\\'
						|| cNewFilename[iCharIDX] == '/'
						|| cNewFilename[iCharIDX] == ':'
						|| cNewFilename[iCharIDX] == '"')
				{
					cNewFilename[iCharIDX] = '_';
				}
			}
		}
		
		sprintf(cNewPath, "%s\\%s", cPath, cNewFilename);
	}
	
	CP_TRACE2("Rename \"%s\" to \"%s\"", pItem->m_pcPath, cNewPath);
	bMoved = MoveFile(pItem->m_pcPath, cNewPath);
	
	if (bMoved)
	{
		CPLI_SetPath(pItem, cNewPath);
		
		// Update interface
		CPL_cb_OnItemUpdated(hItem);
	}
	
	return bMoved;
}

//
//
//
void CPLI_SetPath(CPs_PlaylistItem* pItem, const char* pcPath)
{
	int iCharIDX;
	char cFullPath[MAX_PATH];
	
	// uPathSize is the number of bytes in the path, including the ending NULL.  This is calculated by our allocation
	// routine, so we profit from the already spent CPU cycles to speed up our filename calculation.
	unsigned int uPathSize;
	
	if (pItem->m_pcPath)
		free(pItem->m_pcPath);
		
		
	// Store the full path to the file if this isn't a stream
	if (_strnicmp(CIC_HTTPHEADER, pcPath, 5) != 0
			&& _strnicmp(CIC_HTTPSHEADER, pcPath, 6) != 0
			&& _strnicmp(CIC_FTPHEADER, pcPath, 4) != 0)
	{
		_fullpath(cFullPath, pcPath, MAX_PATH);
		uPathSize = STR_AllocSetString(&pItem->m_pcPath, cFullPath, FALSE);
	}
	
	else
		uPathSize = STR_AllocSetString(&pItem->m_pcPath, pcPath, FALSE);
		
	if (1 >= uPathSize)
	{
		// We haven't any memory allocated, or an empty string, so there is no need to look for the filename
		pItem->m_pcFilename = pItem->m_pcPath;
		return;
	}
	
	// Get the filename (the string following the last slash).  Since we have our string size from the allocation
	// routine, we start from the end to limit the amount of work we do.
	
	for (iCharIDX = uPathSize - 2; iCharIDX >= 0; iCharIDX--)
	{
		if ((pItem->m_pcPath[iCharIDX] == '\\') || (pItem->m_pcPath[iCharIDX] == '/'))
		{
			pItem->m_pcFilename = pItem->m_pcPath + iCharIDX + 1;
			return;
		}
	}
	
	// There is no slash in the path, so the file must be local, and is the same as the path.
	pItem->m_pcFilename = pItem->m_pcPath;
}

//
//
//
const char* CPLI_GetExtension(const CP_HPLAYLISTITEM hItem)
{
	CPs_PlaylistItem* pItem = (CPs_PlaylistItem*)hItem;
	int iCharIDX;
	const char* pcLastDot;
	
	CP_CHECKOBJECT(pItem);
	
	pcLastDot = NULL;
	
	for (iCharIDX = 0; pItem->m_pcPath[iCharIDX]; iCharIDX++)
	{
		if (pItem->m_pcPath[iCharIDX] == '.')
			pcLastDot = pItem->m_pcPath + iCharIDX;
			
		// If there is a directory name with a dot in it we don't want that!
		else if (pItem->m_pcPath[iCharIDX] == '\\')
			pcLastDot = NULL;
	}
	
	// Ensure the string is valid
	
	if (!pcLastDot)
		pcLastDot = "";
		
	return pcLastDot;
}

//
//
//
void CPLI_OGG_SkipOverTab(FILE* pFile)
{
	char tag_header[10];
	int iStreamStart = 0;
	
	if (fread(tag_header, 1, 10, pFile) == 10 && memcmp(tag_header, "ID3", 3) == 0)
	{
		// Simple ID3v2 tag size calculation (sync-safe format)
		iStreamStart = 10; // Header size
		iStreamStart += ((tag_header[6] & 0x7F) << 21)
						| ((tag_header[7] & 0x7F) << 14)
						| ((tag_header[8] & 0x7F) << 7)
						| (tag_header[9] & 0x7F);
	}
	
	fseek(pFile, iStreamStart, SEEK_SET);
}

//
//
//
void CPLI_OGG_DecodeString(char** ppcString, const char* pcNewValue)
{
	int iStringLength;
	
	if (*ppcString)
		free(*ppcString);
		
	iStringLength = strlen(pcNewValue);
	
	*ppcString = malloc(iStringLength + 1);
	
	memcpy(*ppcString, pcNewValue, iStringLength + 1);
}

//
//
//
void CPLI_ReadTag_OGG(CPs_PlaylistItem* pItem)
{
	FILE *hFile;
	OggVorbis_File vorbisfileinfo;
	vorbis_comment* pComment;
	
	hFile = fopen(pItem->m_pcPath, "rb");
	
	if (hFile == NULL)
		return;
		
	CPLI_OGG_SkipOverTab(hFile);
	
	memset(&vorbisfileinfo, 0, sizeof(vorbisfileinfo));
	
	if (ov_open(hFile, &vorbisfileinfo, NULL, 0) < 0)
	{
		fclose(hFile);
		return;
	}
	
	// While we have the file open - we may as well get the length
	CPLI_DecodeLength(pItem, (int)ov_time_total(&vorbisfileinfo, -1));
	
	pComment = ov_comment(&vorbisfileinfo, -1);
	
	if (pComment)
	{
		int iCommentIDX;
		
		for (iCommentIDX = 0; iCommentIDX < pComment->comments; iCommentIDX++)
		{
			char* cTag = malloc(pComment->comment_lengths[iCommentIDX]+8);
			char* cValue = malloc(pComment->comment_lengths[iCommentIDX]+8);

			// find "=" character to parse tag and value data		
			{
				int i = 0;
				int equals_pos = 0;
				char* comment = pComment->user_comments[iCommentIDX];

				while (i < pComment->comment_lengths[iCommentIDX])
				{
					if (comment[i] == '=')
					{
						equals_pos = i;
						break;
					}

					i++;
				}

				if (equals_pos)
				{
					strncpy(cTag, comment, equals_pos+1);
					strncpy(cValue, comment+equals_pos+1, pComment->comment_lengths[iCommentIDX] - equals_pos);
				}
				else
					goto bottom_loop;
			}

			// SECURITY: rewritten due to exploit at
			// http://www.frsirt.com/english/advisories/2008/0008
			// original code used following commented line
            //if(sscanf(pComment->user_comments[iCommentIDX], " %[^= ] = %[^=]", cTag, cValue) == 2)
			{
				if (stricmp(cTag, "TITLE") == 0)
					CPLI_OGG_DecodeString(&pItem->m_pcTrackName, cValue);
				else if (stricmp(cTag, "ARTIST") == 0)
					CPLI_OGG_DecodeString(&pItem->m_pcArtist, cValue);
				else if (stricmp(cTag, "ALBUM") == 0)
					CPLI_OGG_DecodeString(&pItem->m_pcAlbum, cValue);
				else if (stricmp(cTag, "TRACKNUMBER") == 0)
				{
					CPLI_OGG_DecodeString(&pItem->m_pcTrackNum_AsText, cValue);
					pItem->m_cTrackNum = (unsigned char)atoi(pItem->m_pcTrackNum_AsText);
				}
				
				else if (stricmp(cTag, "GENRE") == 0)
				{
					// Search for this genre among the ID3v1 genres (don't read it if we cannot find it)
					int iGenreIDX;
					
					for (iGenreIDX = 0; iGenreIDX < CIC_NUMGENRES; iGenreIDX++)
					{
						if (stricmp(cValue, glb_pcGenres[iGenreIDX]) == 0)
						{
							pItem->m_cGenre = (unsigned char)iGenreIDX;
							break;
						}
					}
				}
			}

bottom_loop:
			free(cTag);
			free(cValue);

		} // end for loop
	}
	
	ov_clear(&vorbisfileinfo);
	
	fclose(hFile);
}

//
//
//
BOOL CPLI_OGG_SyncToNextFrame(HANDLE hFile)
{
	char pcBuffer[1024];
	DWORD dwBytesTransferred;
	int iCharCursor;
	int iChunkSize;
	int iPassIDX;
	
	dwBytesTransferred = 0;
	
	for (iPassIDX = 0; iPassIDX < 64; iPassIDX++)
	{
		if (iPassIDX == 0)
		{
			ReadFile(hFile, pcBuffer, 1024, &dwBytesTransferred, NULL);
			iChunkSize = dwBytesTransferred;
		}
		
		else
		{
			memcpy(pcBuffer, pcBuffer + (dwBytesTransferred - 4), 4);
			ReadFile(hFile, pcBuffer + 4, 1020, &dwBytesTransferred, NULL);
			iChunkSize = dwBytesTransferred + 4;
		}
		
		for (iCharCursor = 0; iCharCursor < (iChunkSize - 4); iCharCursor++)
		{
			if (pcBuffer[iCharCursor] == 'O'
					&& pcBuffer[iCharCursor+1] == 'g'
					&& pcBuffer[iCharCursor+2] == 'g'
					&& pcBuffer[iCharCursor+3] == 'S')
			{
				int iOffset = dwBytesTransferred - iCharCursor;
				SetFilePointer(hFile, -iOffset, NULL, FILE_CURRENT);
				return TRUE;
			}
		}
	}
	
	return FALSE;
}

//
//
//
void CPLI_OGG_UpdateCommentString(void* pComment, const char* pcTag, const char* _pcTagValue)
{
	// Old OGG function - replaced by TagLib
	(void)pComment;
	(void)pcTag;
	(void)_pcTagValue;
}

//
//
//
//
//

//
//
//
void CPLI_WriteTag_OGG(CPs_PlaylistItem* pItem, HANDLE hFile)
{
	// This function has been replaced by TagLib-based metadata writing
	// OGG files are now handled by the TagLib wrapper functions
	(void)pItem;  // Suppress unused parameter warning
	(void)hFile;  // Suppress unused parameter warning
}


void CPLI_ShrinkFile(HANDLE hFile, const DWORD dwStartOffset, const unsigned int iNumBytes)
{
	BYTE pBuffer[0x10000];
	DWORD dwLength;
	DWORD dwBytesTransferred;
	DWORD dwCursor;
	
	CP_TRACE1("Shrunking file by %d bytes", iNumBytes);
	
	dwLength = GetFileSize(hFile, NULL);
	CP_ASSERT((dwStartOffset + iNumBytes) < dwLength);
	dwCursor = dwStartOffset;
	
	while ((dwCursor + iNumBytes) < dwLength)
	{
		unsigned int iChunkSize;
		
		iChunkSize = 0x10000;
		
		if (iChunkSize > dwLength - (dwCursor + iNumBytes))
			iChunkSize = dwLength - (dwCursor + iNumBytes);
			
		SetFilePointer(hFile, dwCursor + iNumBytes, NULL, FILE_BEGIN);
		ReadFile(hFile, pBuffer, iChunkSize, &dwBytesTransferred, NULL);
		
		CP_ASSERT(dwBytesTransferred == iChunkSize);
		
		SetFilePointer(hFile, dwCursor, NULL, FILE_BEGIN);
		WriteFile(hFile, pBuffer, iChunkSize, &dwBytesTransferred, NULL);
		
		dwCursor += iChunkSize;
	}
	
	SetFilePointer(hFile, dwLength - iNumBytes, NULL, FILE_BEGIN);
	SetEndOfFile(hFile);
}

//
//
//
BOOL CPLI_GrowFile(HANDLE hFile, const DWORD dwStartOffset, const unsigned int iNumBytes)
{
	DWORD dwFileSize;
	unsigned int iFileCursor;
	DWORD dwBytesTransferred;
	BYTE* pbReadBlock[0x10000];
	
	dwFileSize = GetFileSize(hFile, NULL);
	CP_TRACE1("Enlarging file by %d bytes", iNumBytes);
	
	// Try to write extra data to end of file - if we fail then clip the file and return
	// (so that we don't corrupt the file in short of space situations)
	{
		BYTE* pbExtra;
		
		pbExtra = (BYTE*)malloc(iNumBytes);
		memset(pbExtra, 0, iNumBytes);
		SetFilePointer(hFile, dwFileSize + iNumBytes, NULL, FILE_BEGIN);
		WriteFile(hFile, pbExtra, iNumBytes, &dwBytesTransferred, NULL);
		
		if (dwBytesTransferred != iNumBytes)
		{
			// Failed - clip file again and abort tag write
			SetFilePointer(hFile, dwFileSize, NULL, FILE_BEGIN);
			SetEndOfFile(hFile);
			return FALSE;
		}
	}
	
	// Enlarge tag
	iFileCursor = dwFileSize;
	
	while (iFileCursor > dwStartOffset)
	{
		unsigned int iBlockSize;
		
		iBlockSize = 0x10000;
		
		if ((iFileCursor - dwStartOffset) < iBlockSize)
			iBlockSize = iFileCursor - dwStartOffset;
			
		// Read a chunk
		SetFilePointer(hFile, iFileCursor - iBlockSize, NULL, FILE_BEGIN);
		ReadFile(hFile, pbReadBlock, iBlockSize, &dwBytesTransferred, NULL);
		
		CP_ASSERT(dwBytesTransferred == iBlockSize);
		
		// Write chunk at offsetted position
		SetFilePointer(hFile, iFileCursor - iBlockSize + iNumBytes, NULL, FILE_BEGIN);
		WriteFile(hFile, pbReadBlock, iBlockSize, &dwBytesTransferred, NULL);
		
		CP_ASSERT(dwBytesTransferred == iBlockSize);
		
		iFileCursor -= iBlockSize;
	}
	
	return TRUE;
}

//
//
// TagLib-based tag reading/writing functions
//
void CPLI_ReadTag_TagLib(CPs_PlaylistItem* pItem)
{
	char* pcTitle = NULL;
	char* pcArtist = NULL;
	char* pcAlbum = NULL;
	char* pcYear = NULL;
	char* pcComment = NULL;
	char* pcGenre = NULL;
	unsigned int iTrackNum = 0;
	unsigned int iLength = 0;
	int iTagType = 0;
	
	if (!pItem || !pItem->m_pcPath)
	{
		CPLII_RemoveTagInfo(pItem);
		pItem->m_enTagType = ttNone;
		return;
	}

	// Try to read tags using TagLib
	BOOL bSuccess = CPTL_ReadTags(pItem->m_pcPath,
								  &pcTitle, &pcArtist, &pcAlbum,
								  &pcYear, &pcComment, &pcGenre,
								  &iTrackNum, &iLength, &iTagType);

	if (bSuccess)
	{
		// Set the tag information
		STR_AllocSetString(&pItem->m_pcArtist, pcArtist, FALSE);
		STR_AllocSetString(&pItem->m_pcAlbum, pcAlbum, FALSE);
		STR_AllocSetString(&pItem->m_pcTrackName, pcTitle, FALSE);
		STR_AllocSetString(&pItem->m_pcComment, pcComment, FALSE);
		STR_AllocSetString(&pItem->m_pcYear, pcYear, FALSE);
		
		// Handle genre - convert string to genre index
		if (pcGenre)
		{
			int iGenreIndex = CPTL_GetGenreIndex(pcGenre);
			if (iGenreIndex >= 0 && iGenreIndex < 256)
				pItem->m_cGenre = (unsigned char)iGenreIndex;
			else
				pItem->m_cGenre = 255; // "Unknown" or default
		}
		else
		{
			pItem->m_cGenre = 255;
		}
		
		// Handle track number - convert to unsigned char
		if (iTrackNum > 0 && iTrackNum <= 255)
			pItem->m_cTrackNum = (unsigned char)iTrackNum;
		else
			pItem->m_cTrackNum = 0;
		
		// Set tag type
		if (iTagType == 2)
			pItem->m_enTagType = ttID3v2;
		else if (iTagType == 1)
			pItem->m_enTagType = ttID3v1;
		else
			pItem->m_enTagType = ttNone;
		
		// Clean up allocated strings from TagLib
		if (pcTitle) free(pcTitle);
		if (pcArtist) free(pcArtist);
		if (pcAlbum) free(pcAlbum);
		if (pcYear) free(pcYear);
		if (pcComment) free(pcComment);
		if (pcGenre) free(pcGenre);
	}
	else
	{
		// No tags found
		CPLII_RemoveTagInfo(pItem);
		pItem->m_enTagType = ttNone;
	}
}

void CPLI_WriteTag_TagLib(CPs_PlaylistItem* pItem)
{
	const char* pcGenre = NULL;
	
	if (!pItem || !pItem->m_pcPath)
		return;

	// Convert genre index back to string
	if (pItem->m_cGenre < CIC_NUMGENRES)
		pcGenre = CPTL_GetGenreString(pItem->m_cGenre);

	// Write tags using TagLib
	CPTL_WriteTags(pItem->m_pcPath,
				   pItem->m_pcTrackName,
				   pItem->m_pcArtist,
				   pItem->m_pcAlbum,
				   pItem->m_pcYear,
				   pItem->m_pcComment,
				   pcGenre,
				   pItem->m_cTrackNum,
				   0); // length is not typically written to tags
}

//
//
//
