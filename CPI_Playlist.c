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
/*
 * CPI_Playlist.c — Win32 shell layer.
 *
 * The playlist data model (linked list, track stack, path hash, sort,
 * navigation) now lives in rust/codecs/src/playlist.rs.  This file keeps only
 * the Win32-specific concerns:
 *   - CPL_CreatePlaylist / CPL_DestroyPlaylist (thread management)
 *   - CPL_PlayActiveItem (player engine bridge)
 *   - CPL_AddSingleFile (tag-read dispatch to worker thread)
 *   - CPL_HandleAsyncNotify (post-tag-read completion)
 *   - CPL_QueueNextForGapless (gapless pre-buffer)
 *   - File I/O: CPL_AddFile, CPL_ExportPlaylist, M3U/PLS parsers
 *   - Directory scan: CPL_AddDirectory_Recurse, CPL_AddDroppedFiles
 *   - Option accessors called by Rust (CP_opt_*)
 *   - Miscellaneous helpers that stay in C
 */

#include "stdafx.h"
#include "globals.h"
#include "CPI_Playlist.h"
#include "CPI_PlaylistItem.h"
#include "CPI_PlaylistItem_Internal.h"
#include "CPI_Player.h"
#include "CPI_Player_Engine.h"
#include "CPString.h"
#include "CPI_Gettext.h"
#include "CPI_ReplayGain.h"

int __cdecl exp_CompareStrings(const void *elem1, const void *elem2);
DWORD WINAPI CPI_PlaylistWorkerThreadEP(void* pCookie);

////////////////////////////////////////////////////////////////////////////////
// Notify chunk (unchanged — used by worker thread and HandleAsyncNotify)
////////////////////////////////////////////////////////////////////////////////

#define CPC_PLAYLISTWORKER_NOTIFYCHUNKSIZE 32

typedef struct _CPs_NotifyChunk
{
	int m_iNumberInChunk;
	CP_HPLAYLISTITEM m_aryItems[CPC_PLAYLISTWORKER_NOTIFYCHUNKSIZE];
	DWORD m_aryBatchIDs[CPC_PLAYLISTWORKER_NOTIFYCHUNKSIZE];
} CPs_NotifyChunk;

////////////////////////////////////////////////////////////////////////////////
// Playlist file type
////////////////////////////////////////////////////////////////////////////////

typedef enum _CPe_PlayListFileType
{
	pftUnknown,
	pftPLS,
	pftM3U
} CPe_PlayListFileType;

typedef struct _CPs_FilenameLLItem
{
	char* m_pcFilename;
	void* m_pNextItem;
} CPs_FilenameLLItem;

////////////////////////////////////////////////////////////////////////////////
// Forward declarations for Rust-exported functions
////////////////////////////////////////////////////////////////////////////////

/* Allocation */
CP_HPLAYLIST CPPL_AllocPlaylist(void);
void         CPPL_FreePlaylist(CP_HPLAYLIST hPlaylist);

/* Worker-thread field accessors */
void         CPPL_SetWorkerThread(CP_HPLAYLIST hPlaylist, uintptr_t hThread, DWORD dwThreadID);
void         CPPL_SetHostThreadID(CP_HPLAYLIST hPlaylist, DWORD dwHostID);
uintptr_t    CPPL_GetWorkerThread(CP_HPLAYLIST hPlaylist);
DWORD        CPPL_GetWorkerThreadID(CP_HPLAYLIST hPlaylist);
DWORD        CPPL_GetHostThreadID(CP_HPLAYLIST hPlaylist);
DWORD        CPPL_GetBatchID(CP_HPLAYLIST hPlaylist);
void         CPPL_IncrBatchID(CP_HPLAYLIST hPlaylist);
BOOL         CPPL_GetSyncLoadNextFile(CP_HPLAYLIST hPlaylist);
void         CPPL_SetSyncLoadNextFile(CP_HPLAYLIST hPlaylist, BOOL val);
BOOL         CPPL_GetAutoActivateInitial(CP_HPLAYLIST hPlaylist);
void         CPPL_SetAutoActivateInitial(CP_HPLAYLIST hPlaylist, BOOL val);

/* Data-model functions (implemented in Rust, declared in CPI_Playlist.h) */
/* CPL_Empty, CPL_RemoveItem, CPL_SetActiveItem, CPL_AddSingleFile_pt2, etc. */
void CPL_AddSingleFile_pt2(CP_HPLAYLIST hPlaylist, CP_HPLAYLISTITEM hNewFile, DWORD dwBatchID);

////////////////////////////////////////////////////////////////////////////////
// Option accessors — called by Rust via FFI to avoid mirroring CPs_Settings
////////////////////////////////////////////////////////////////////////////////

BOOL CP_opt_allow_file_once(void)
{
	return options.allow_file_once_in_playlist;
}

BOOL CP_opt_read_id3_tag_of_selected(void)
{
	return options.read_id3_tag_of_selected;
}

BOOL CP_opt_shuffle_play(void)
{
	return options.shuffle_play;
}

BOOL CP_opt_repeat_playlist(void)
{
	return options.repeat_playlist;
}

void CP_opt_set_initial_file(const char* pcPath)
{
	if (!pcPath) return;
	strncpy(options.initial_file, pcPath, sizeof(options.initial_file) - 1);
	options.initial_file[sizeof(options.initial_file) - 1] = '\0';
}

////////////////////////////////////////////////////////////////////////////////
// Create / destroy playlist
////////////////////////////////////////////////////////////////////////////////

CP_HPLAYLIST CPL_CreatePlaylist(void)
{
	HANDLE hThread;
	DWORD dwThreadID;
	CP_HPLAYLIST hPlaylist = CPPL_AllocPlaylist();
	if (!hPlaylist)
		return NULL;

	CPPL_SetHostThreadID(hPlaylist, GetCurrentThreadId());

	hThread = CreateThread(NULL, 0, CPI_PlaylistWorkerThreadEP, hPlaylist, 0, &dwThreadID);
	CP_ASSERT(hThread);

	CPPL_SetWorkerThread(hPlaylist, (uintptr_t)hThread, dwThreadID);

	return hPlaylist;
}

void CPL_DestroyPlaylist(CP_HPLAYLIST hPlaylist)
{
	CP_ASSERT(hPlaylist);

	/* Cancel in-flight tag reads */
	CPPL_IncrBatchID(hPlaylist);

	/* Ask worker thread to exit */
	PostThreadMessage(CPPL_GetWorkerThreadID(hPlaylist), CPPLWT_TERMINATE, 0, 0);

	/* Empty data model (Rust) */
	CPL_Empty(hPlaylist);

	/* Destroy deferred-destroy active item if still live */
	{
		CP_HPLAYLISTITEM hCurrent = CPL_GetActiveItem(hPlaylist);
		if (hCurrent && CPLI_IsDestroyOnDeactivate(hCurrent))
			CPLI_DestroyItem(hCurrent);
	}

	/* Wait for worker thread to finish */
	{
		DWORD dwWait = WaitForSingleObject((HANDLE)CPPL_GetWorkerThread(hPlaylist), 5000);
		if (dwWait == WAIT_TIMEOUT)
			CP_TRACE0("Worker thread did not exit gracefully within timeout");
	}

	CloseHandle((HANDLE)CPPL_GetWorkerThread(hPlaylist));

	/* Drain any CPPLNM_TAGREAD messages that arrived after we sent TERMINATE */
	{
		MSG msg;
		int iCount = 0;
		while (PeekMessage(&msg, NULL, CPPLNM_TAGREAD, CPPLNM_TAGREAD, PM_REMOVE) && iCount < 1000)
		{
			CPs_NotifyChunk* pChunk = (CPs_NotifyChunk*)msg.wParam;
			if (pChunk)
			{
				if (pChunk->m_iNumberInChunk > 0 && pChunk->m_iNumberInChunk <= CPC_PLAYLISTWORKER_NOTIFYCHUNKSIZE)
				{
					int i;
					for (i = 0; i < pChunk->m_iNumberInChunk; i++)
						if (pChunk->m_aryItems[i])
							CPLII_DestroyItem(pChunk->m_aryItems[i]);
				}
				free(pChunk);
			}
			iCount++;
		}
		if (iCount >= 1000)
			CP_TRACE0("Playlist cleanup: hit maximum cleanup iteration limit");
	}

	CPPL_FreePlaylist(hPlaylist);
}

////////////////////////////////////////////////////////////////////////////////
// Play the active item through the player engine
////////////////////////////////////////////////////////////////////////////////

void CPL_PlayActiveItem(CP_HPLAYLIST hPlaylist, const BOOL bStopFirst)
{
	CP_ASSERT(hPlaylist);

	if (bStopFirst == TRUE)
		CPI_Player__Stop(globals.m_hPlayer);

	{
		CP_HPLAYLISTITEM hCurrent = CPL_GetActiveItem(hPlaylist);
		if (hCurrent)
		{
			float fScale = CPRG_ComputeScale(
				(CPe_ReplayGainMode)options.replaygain_mode,
				CPLI_GetReplayGain_Track_Gain(hCurrent),
				CPLI_GetReplayGain_Track_Peak(hCurrent),
				CPLI_GetReplayGain_Album_Gain(hCurrent),
				CPLI_GetReplayGain_Album_Peak(hCurrent),
				(float)options.replaygain_preamp_db,
				options.replaygain_prevent_clipping);
			CPI_Player__OpenFile(globals.m_hPlayer, CPLI_GetPath(hCurrent), fScale);
			CPI_Player__Play(globals.m_hPlayer);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// Handle the CPPLNM_TAGREAD message posted by the worker thread
////////////////////////////////////////////////////////////////////////////////

void CPL_HandleAsyncNotify(CP_HPLAYLIST hPlaylist, WPARAM wParam, LPARAM lParam)
{
	CPs_NotifyChunk* pChunk = (CPs_NotifyChunk*)wParam;
	int i;
	(void)hPlaylist;
	(void)lParam;

	if (globals.m_hPlaylistViewControl)
		CLV_BeginBatch(globals.m_hPlaylistViewControl);

	for (i = 0; i < pChunk->m_iNumberInChunk; i++)
		CPL_AddSingleFile_pt2(globals.m_hPlaylist, pChunk->m_aryItems[i], pChunk->m_aryBatchIDs[i]);

	if (globals.m_hPlaylistViewControl)
		CLV_EndBatch(globals.m_hPlaylistViewControl);

	free(pChunk);
}

////////////////////////////////////////////////////////////////////////////////
// Add a single audio file (may defer tag reading to the worker thread)
////////////////////////////////////////////////////////////////////////////////

void CPL_AddSingleFile(CP_HPLAYLIST hPlaylist, const char* pcPath, const char* pcTitle)
{
	CP_HPLAYLISTITEM hNewFile;
	CP_ASSERT(hPlaylist);

	/* Only allow file types we can play */
	{
		int i;
		BOOL valid = FALSE;
		CPs_PlayEngine* player = (CPs_PlayEngine*)globals.m_hPlayer;
		CPs_PlayerContext* pContext = (CPs_PlayerContext*)player->m_pContext;
		DWORD_PTR tempcookie;
		char* extension = NULL;

		{
			char* dot = strrchr(pcPath, '.');
			if (dot)
				extension = dot + 1;
		}

		if (extension == NULL)
			return;

		CP_LOG_VERBOSE("CPL_AddItem: Checking file extension '%s' against codecs\n", extension);

		for (i = 0; i <= CP_CODEC_last; i++)
		{
			if (CPFA_IsAssociated(&pContext->m_CoDecs[i], extension, &tempcookie))
			{
				CP_LOG_VERBOSE("CPL_AddItem: Extension '%s' is supported by codec %d\n", extension, i);
				valid = TRUE;
				break;
			}
		}

		if (!valid)
			CP_LOG_VERBOSE("CPL_AddItem: Extension '%s' is not supported by any codec\n", extension);

		if (CP_IsURL(pcPath))
			valid = TRUE;

		if (valid == FALSE)
			return;
	}

	hNewFile = CPLII_CreateItem(pcPath);

	/* If a title was passed, set it immediately */
	if (pcTitle && pcTitle[0])
		CPLI_SetTrackName(hNewFile, pcTitle);

	/* Defer to background tag-read thread (async path) */
	if (options.read_id3_tag && options.read_id3_tag_in_background && !CPPL_GetSyncLoadNextFile(hPlaylist))
	{
		DWORD dwBatchID = CPPL_GetBatchID(hPlaylist);
		DWORD dwWorkerID = CPPL_GetWorkerThreadID(hPlaylist);

		while (!PostThreadMessage(dwWorkerID, CPPLWT_READTAG, (WPARAM)dwBatchID, (LPARAM)hNewFile))
			Sleep(50);

		/* Canonicalize path for initial-file comparison */
		{
			char name[MAX_PATH];
			int pos, last, len, dst;
			len = (int)strlen(pcPath);
			last = -1;
			pos = 0;
			dst = 0;
			memset(name, 0, MAX_PATH);

			while (pos <= len && dst < MAX_PATH - 1)
			{
				if (pcPath[pos] == '\\')
				{
					if (pcPath[pos+1] == '.' && pcPath[pos+2] == '.' && last != -1)
					{
						pos += 3;
						dst = last;
						last--;
						while (last >= 0)
							if (name[last] == '\\') break; else last--;
					}
					else
					{
						last = dst;
						name[dst++] = pcPath[pos++];
					}
				}
				else
					name[dst++] = pcPath[pos++];
			}

			if (CPPL_GetAutoActivateInitial(hPlaylist) && stricmp(name, options.initial_file) == 0)
				PostThreadMessage(dwWorkerID, CPPLWT_SETACTIVE, (WPARAM)dwBatchID, (LPARAM)hNewFile);
		}
	}
	else
	{
		/* Synchronous tag-read path */
		CPPL_SetSyncLoadNextFile(hPlaylist, FALSE);

		if (options.read_id3_tag)
			CPLI_ReadTag(hNewFile);

		if (CPLI_GetTrackLength(hNewFile) == 0 && options.work_out_track_lengths)
			CPLI_CalculateLength(hNewFile);

		CPL_AddSingleFile_pt2(hPlaylist, hNewFile, CPPL_GetBatchID(hPlaylist));
	}
}

////////////////////////////////////////////////////////////////////////////////
// Gapless: pre-buffer the next file
////////////////////////////////////////////////////////////////////////////////

void CPL_QueueNextForGapless(CP_HPLAYLIST hPlaylist)
{
	CP_HPLAYLISTITEM hNext;

	if (!options.gapless_playback)
		return;

	hNext = CPL_PeekNextItem(hPlaylist);

	if (hNext)
	{
		float fScale = CPRG_ComputeScale(
			(CPe_ReplayGainMode)options.replaygain_mode,
			CPLI_GetReplayGain_Track_Gain(hNext),
			CPLI_GetReplayGain_Track_Peak(hNext),
			CPLI_GetReplayGain_Album_Gain(hNext),
			CPLI_GetReplayGain_Album_Peak(hNext),
			(float)options.replaygain_preamp_db,
			options.replaygain_prevent_clipping);
		CPI_Player__SetNextFile(globals.m_hPlayer, CPLI_GetPath(hNext), fScale);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Wrapper functions that delegate to Rust CPPL_* accessors
////////////////////////////////////////////////////////////////////////////////

void CPL_SyncLoadNextFile(CP_HPLAYLIST hPlaylist)
{
	CP_ASSERT(hPlaylist);
	CPPL_SetSyncLoadNextFile(hPlaylist, TRUE);
}

void CPL_SetAutoActivateInitial(CP_HPLAYLIST hPlaylist, const BOOL bAutoActivateInitial)
{
	(void)bAutoActivateInitial;  /* parameter was historically ignored; always sets TRUE */
	CP_ASSERT(hPlaylist);
	CPPL_SetAutoActivateInitial(hPlaylist, TRUE);
}

////////////////////////////////////////////////////////////////////////////////
// Worker thread
////////////////////////////////////////////////////////////////////////////////

DWORD WINAPI CPI_PlaylistWorkerThreadEP(void* pCookie)
{
	CP_HPLAYLIST hPlaylist = (CP_HPLAYLIST)pCookie;
	MSG msg;
	CPs_NotifyChunk* pPendingChunk;
	BOOL bRet;

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);
	pPendingChunk = NULL;

	while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (bRet == -1)
			return 0;

		if (msg.message == CPPLWT_TERMINATE)
		{
			break;
		}
		else if (msg.message == CPPLWT_READTAG)
		{
			MSG msgPeek;
			CP_HPLAYLISTITEM hNewFile = (CP_HPLAYLISTITEM)msg.lParam;

			if (CPPL_GetBatchID(hPlaylist) == (DWORD)msg.wParam)
			{
				CPLI_ReadTag(hNewFile);

				if (CPLI_GetTrackLength(hNewFile) == 0 && options.work_out_track_lengths)
					CPLI_CalculateLength(hNewFile);

				if (!pPendingChunk)
				{
					pPendingChunk = MALLOC_TYPE(CPs_NotifyChunk);
					if (!pPendingChunk)
					{
						CP_TRACE0("Playlist worker: Failed to allocate notify chunk");
						continue;
					}
					pPendingChunk->m_iNumberInChunk = 0;
				}

				pPendingChunk->m_aryItems[pPendingChunk->m_iNumberInChunk]    = hNewFile;
				pPendingChunk->m_aryBatchIDs[pPendingChunk->m_iNumberInChunk] = (DWORD)msg.wParam;
				pPendingChunk->m_iNumberInChunk++;
			}
			else
			{
				CPLII_DestroyItem(hNewFile);
			}

			if (pPendingChunk)
			{
				if (pPendingChunk->m_iNumberInChunk == CPC_PLAYLISTWORKER_NOTIFYCHUNKSIZE
					|| PeekMessage(&msgPeek, NULL, CPPLWT_READTAG, CPPLWT_READTAG, PM_NOREMOVE) == FALSE)
				{
					PostThreadMessage(CPPL_GetHostThreadID(hPlaylist), CPPLNM_TAGREAD, (WPARAM)pPendingChunk, 0L);
					pPendingChunk = NULL;
				}
			}
		}
		else if (msg.message == CPPLWT_SYNCSHUFFLE)
		{
			PostThreadMessage(CPPL_GetHostThreadID(hPlaylist), CPPLNM_SYNCSHUFFLE, 0L, 0L);
		}
		else if (msg.message == CPPLWT_SETACTIVE)
		{
			CP_HPLAYLISTITEM hFile = (CP_HPLAYLISTITEM)msg.lParam;

			if (pPendingChunk)
			{
				PostThreadMessage(CPPL_GetHostThreadID(hPlaylist), CPPLNM_TAGREAD, (WPARAM)pPendingChunk, 0L);
				pPendingChunk = NULL;
			}

			if (CPPL_GetBatchID(hPlaylist) == (DWORD)msg.wParam)
				PostThreadMessage(CPPL_GetHostThreadID(hPlaylist), CPPLNM_SYNCSETACTIVE, (WPARAM)hFile, 0L);
		}
	}

	if (pPendingChunk)
	{
		int i;
		for (i = 0; i < pPendingChunk->m_iNumberInChunk; i++)
			CPLII_DestroyItem(pPendingChunk->m_aryItems[i]);
		free(pPendingChunk);
	}

	CP_TRACE0("Playlist worker thread terminating");
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// File I/O helpers
////////////////////////////////////////////////////////////////////////////////

static void WriteFile_Text(HANDLE hFile, const char* pcLine, const BOOL bAppendCR)
{
	DWORD dwBytesWritten;
	int iLineLen = (int)strlen(pcLine);
	WriteFile(hFile, pcLine, iLineLen, &dwBytesWritten, NULL);
	if (bAppendCR)
		WriteFile(hFile, "\r\n", 2, &dwBytesWritten, NULL);
}

static CPe_PlayListFileType CPL_GetFileType(const char* pcPath)
{
	const char* pcExtension = NULL;
	int iCharIDX;

	for (iCharIDX = 0; pcPath[iCharIDX]; iCharIDX++)
	{
		if (pcPath[iCharIDX] == '.')
			pcExtension = pcPath + iCharIDX + 1;
		else if (pcPath[iCharIDX] == '\\')
			pcExtension = NULL;
	}

	if (pcExtension == NULL)
		return pftUnknown;

	if (stricmp(pcExtension, "pls") == 0)
		return pftPLS;
	else if (stricmp(pcExtension, "m3u") == 0)
		return pftM3U;

	return pftUnknown;
}

unsigned int CPL_GetPathVolumeBytes(const char* pcPath)
{
	if (pcPath[1] == ':')
		return 3;
	else if (pcPath[0] == '\\' && pcPath[1] == '\\')
	{
		int iCharIDX;
		int iNumSlashesFound = 0;
		for (iCharIDX = 2; pcPath[iCharIDX]; iCharIDX++)
		{
			if (pcPath[iCharIDX] == '\\')
				iNumSlashesFound++;
			if (iNumSlashesFound == 2)
				return iCharIDX + 1;
		}
	}
	else if (_strnicmp(pcPath, CIC_HTTPHEADER,  sizeof(CIC_HTTPHEADER)  - 1) == 0) return sizeof(CIC_HTTPHEADER);
	else if (_strnicmp(pcPath, CIC_ICYHEADER,   sizeof(CIC_ICYHEADER)   - 1) == 0) return sizeof(CIC_ICYHEADER);
	else if (_strnicmp(pcPath, CIC_HTTPSHEADER, sizeof(CIC_HTTPSHEADER) - 1) == 0) return sizeof(CIC_HTTPSHEADER);
	else if (_strnicmp(pcPath, CIC_FTPHEADER,   sizeof(CIC_FTPHEADER)   - 1) == 0) return sizeof(CIC_FTPHEADER);
	return 0;
}

unsigned int CPL_GetPathDirectoryBytes(const char* pcPath, const unsigned int iVolumeBytes)
{
	unsigned int iCharIDX;
	unsigned int iLastSlashIDX = 0;
	for (iCharIDX = iVolumeBytes; pcPath[iCharIDX]; iCharIDX++)
		if (pcPath[iCharIDX] == '\\' || pcPath[iCharIDX] == '/')
			iLastSlashIDX = iCharIDX + 1;
	return iLastSlashIDX;
}

static void CPL_CanonicalizePath(char* pcPath)
{
	char cTemp[MAX_PATH];
	char* parts[MAX_PATH];
	int nParts = 0;
	int nOut = 0;
	int prefixLen = 0;
	char* pComp;
	char* pSep;
	int i;

	cp_strcpy_s(cTemp, sizeof(cTemp), pcPath);

	if (cTemp[0] && cTemp[1] == ':' && (cTemp[2] == '\\' || cTemp[2] == '/'))
		prefixLen = 3;

	pComp = cTemp + prefixLen;
	pSep  = pComp;

	while (*pSep)
	{
		if (*pSep == '\\' || *pSep == '/')
		{
			*pSep = '\0';
			if (*pComp != '\0')
				parts[nParts++] = pComp;
			pComp = pSep + 1;
		}
		pSep++;
	}
	if (*pComp != '\0')
		parts[nParts++] = pComp;

	for (i = 0; i < nParts; i++)
	{
		if (strcmp(parts[i], "..") == 0)
		{ if (nOut > 0) nOut--; }
		else if (strcmp(parts[i], ".") != 0)
			parts[nOut++] = parts[i];
	}

	memcpy(pcPath, cTemp, prefixLen);
	pcPath[prefixLen] = '\0';
	for (i = 0; i < nOut; i++)
	{
		if (i > 0)
			cp_strcat_s(pcPath, MAX_PATH, "\\");
		cp_strcat_s(pcPath, MAX_PATH, parts[i]);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Export playlist
////////////////////////////////////////////////////////////////////////////////

void CPL_ExportPlaylist(CP_HPLAYLIST hPlaylist, const char* pcOutputName)
{
	HANDLE hOutputFile;
	CPe_PlayListFileType enFileType;
	const unsigned int iPlaylist_VolumeBytes = CPL_GetPathVolumeBytes(pcOutputName);
	const unsigned int iPlaylist_DirectoryBytes = CPL_GetPathDirectoryBytes(pcOutputName, iPlaylist_VolumeBytes);
	CP_ASSERT(hPlaylist);

	enFileType = CPL_GetFileType(pcOutputName);
	if (enFileType == pftUnknown)
		return;

	{
		WCHAR* pwcOutputName = STR_ConvertToUnicode(pcOutputName);
		if (!pwcOutputName)
			return;
		hOutputFile = CreateFileW(pwcOutputName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		free(pwcOutputName);
	}

	if (hOutputFile == INVALID_HANDLE_VALUE)
	{
		MessageBoxA(windows.wnd_main, T(STR_ERR_COULD_NOT_OPEN_FILE), T(STR_ERR_ERROR), MB_ICONERROR);
		return;
	}

	{
		CP_HPLAYLISTITEM hCursor;
		int iFileNumber;

		if (enFileType == pftPLS)
		{
			int iNumberOfEntries = 0;
			char cNumEntriesLine[32];

			WriteFile_Text(hOutputFile, "[PlayList]", TRUE);

			for (hCursor = CPL_GetFirstItem(hPlaylist); hCursor; hCursor = CPLI_Next(hCursor))
				iNumberOfEntries++;

			sprintf_s(cNumEntriesLine, sizeof(cNumEntriesLine), "NumberOfEntries=%d", iNumberOfEntries);
			WriteFile_Text(hOutputFile, cNumEntriesLine, TRUE);
		}

		iFileNumber = 0;

		for (hCursor = CPL_GetFirstItem(hPlaylist); hCursor; hCursor = CPLI_Next(hCursor), iFileNumber++)
		{
			char cRelPath[MAX_PATH];
			const char* pcFilename = CPLI_GetPath(hCursor);

			if (_strnicmp(pcFilename, pcOutputName, iPlaylist_VolumeBytes) == 0)
			{
				const char* pcLastCommonSplitPoint = pcFilename;
				unsigned int iCharIDX;

				for (iCharIDX = 0; pcFilename[iCharIDX] && iCharIDX < iPlaylist_DirectoryBytes; iCharIDX++)
				{
					if (tolower(pcFilename[iCharIDX]) != tolower(pcOutputName[iCharIDX]))
						break;
					if (pcFilename[iCharIDX] == '\\')
						pcLastCommonSplitPoint = pcFilename + iCharIDX + 1;
				}

				cRelPath[0] = '\0';
				for (; iCharIDX < iPlaylist_DirectoryBytes; iCharIDX++)
					if (pcOutputName[iCharIDX] == '\\')
						cp_strcat_s(cRelPath, sizeof(cRelPath), "..\\");

				cp_strcat_s(cRelPath, sizeof(cRelPath), pcLastCommonSplitPoint);
			}
			else
			{
				cp_strcpy_s(cRelPath, sizeof(cRelPath), pcFilename);
			}

			if (enFileType == pftPLS)
			{
				char cPlsFileHeader[32];
				sprintf_s(cPlsFileHeader, sizeof(cPlsFileHeader), "File%d=", iFileNumber + 1);
				WriteFile_Text(hOutputFile, cPlsFileHeader, FALSE);
			}

			WriteFile_Text(hOutputFile, cRelPath, TRUE);
		}
	}

	CloseHandle(hOutputFile);
}

////////////////////////////////////////////////////////////////////////////////
// Add a file resolving relative paths against the playlist's own path
////////////////////////////////////////////////////////////////////////////////

void CPL_AddPrefixedFile(CP_HPLAYLIST hPlaylist,
						  const char* pcFilename, const char* pcTitle,
						  const char* pcPlaylistFile,
						  const unsigned int iPlaylist_VolumeBytes,
						  const unsigned int iPlaylist_DirBytes)
{
	const unsigned int iFile_VolumeBytes = CPL_GetPathVolumeBytes(pcFilename);

	/* Reject UNC paths from untrusted playlists */
	if (pcFilename[0] == '\\' && pcFilename[1] == '\\')
	{
		CP_TRACE1("Rejecting playlist entry with UNC path: \"%s\"", pcFilename);
		return;
	}

	if (iFile_VolumeBytes)
	{
		CPL_AddSingleFile(hPlaylist, pcFilename, pcTitle);
	}
	else if (pcFilename[0] == '\\')
	{
		char cFullPath[MAX_PATH];
		size_t iRemaining = sizeof(cFullPath);
		if (iPlaylist_VolumeBytes < iRemaining)
		{
			memcpy(cFullPath, pcPlaylistFile, iPlaylist_VolumeBytes);
			cp_strcpy_s(cFullPath + iPlaylist_VolumeBytes, iRemaining - iPlaylist_VolumeBytes, pcFilename + 1);
			CPL_CanonicalizePath(cFullPath);
			CPL_AddSingleFile(hPlaylist, cFullPath, pcTitle);
		}
	}
	else
	{
		char cFullPath[MAX_PATH];
		size_t iRemaining = sizeof(cFullPath);
		if (iPlaylist_DirBytes < iRemaining)
		{
			memcpy(cFullPath, pcPlaylistFile, iPlaylist_DirBytes);
			cp_strcpy_s(cFullPath + iPlaylist_DirBytes, iRemaining - iPlaylist_DirBytes, pcFilename);
			CPL_CanonicalizePath(cFullPath);
			CPL_AddSingleFile(hPlaylist, cFullPath, pcTitle);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// CPL_AddFile — load a file or playlist
////////////////////////////////////////////////////////////////////////////////

void CPL_AddFile(CP_HPLAYLIST hPlaylist, const char* pcFilename)
{
	CPe_PlayListFileType enFileType;
	unsigned int iPlaylist_VolumeBytes;
	unsigned int iPlaylist_DirectoryBytes;
	CP_ASSERT(hPlaylist);

	enFileType = CPL_GetFileType(pcFilename);

	if (enFileType == pftUnknown)
	{
		CPL_AddSingleFile(hPlaylist, pcFilename, NULL);
		return;
	}

	iPlaylist_VolumeBytes    = CPL_GetPathVolumeBytes(pcFilename);
	iPlaylist_DirectoryBytes = CPL_GetPathDirectoryBytes(pcFilename, iPlaylist_VolumeBytes);

	CPL_cb_LockWindowUpdates(TRUE);

	if (enFileType == pftPLS)
	{
		/* URL PLS */
		if ((_strnicmp(pcFilename, CIC_HTTPHEADER,  sizeof(CIC_HTTPHEADER)  - 1) == 0) ||
			(_strnicmp(pcFilename, CIC_ICYHEADER,   sizeof(CIC_ICYHEADER)   - 1) == 0) ||
			(_strnicmp(pcFilename, CIC_HTTPSHEADER, sizeof(CIC_HTTPSHEADER) - 1) == 0) ||
			(_strnicmp(pcFilename, CIC_FTPHEADER,   sizeof(CIC_FTPHEADER)   - 1) == 0))
		{
			HINTERNET hInternet, hURLStream;
			DWORD dwTimeout;
			INTERNET_BUFFERS internetbuffer;
			char* pcPlaylistBuffer;

			CP_LOG_DEBUG("CPL_AddFile: Processing PLS URL: %s\n", pcFilename);

			hInternet = InternetOpen(CP_BRISKPLAYER, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0L);
			if (!hInternet) { CP_LOG_ERROR("CPL_AddFile: InternetOpen failed for PLS URL\n"); CPL_cb_LockWindowUpdates(FALSE); return; }

			dwTimeout = 5000;
			InternetSetOption(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &dwTimeout, sizeof(dwTimeout));

			hURLStream = InternetOpenUrl(hInternet, pcFilename, NULL, 0,
				INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE, 0);

			if (!hURLStream) { CP_LOG_ERROR("CPL_AddFile: InternetOpenUrl failed\n"); InternetCloseHandle(hInternet); CPL_cb_LockWindowUpdates(FALSE); return; }

			pcPlaylistBuffer = CALLOC_TYPE(char, 0x40001);
			if (!pcPlaylistBuffer) { CP_LOG_ERROR("CPL_AddFile: alloc failed\n"); InternetCloseHandle(hURLStream); InternetCloseHandle(hInternet); CPL_cb_LockWindowUpdates(FALSE); return; }

			internetbuffer.dwStructSize   = sizeof(internetbuffer);
			internetbuffer.Next           = NULL;
			internetbuffer.lpcszHeader    = NULL;
			internetbuffer.lpvBuffer      = pcPlaylistBuffer;
			internetbuffer.dwBufferLength = 0x40000;

			{
				BOOL bReadResult = InternetReadFileEx(hURLStream, &internetbuffer, IRF_NO_WAIT, 0);
				InternetCloseHandle(hURLStream);
				InternetCloseHandle(hInternet);

				if (!bReadResult || !internetbuffer.dwBufferLength)
				{
					CP_LOG_ERROR("CPL_AddFile: Failed to download PLS\n");
					free(pcPlaylistBuffer);
					CPL_cb_LockWindowUpdates(FALSE);
					return;
				}
			}

			pcPlaylistBuffer[internetbuffer.dwBufferLength] = '\0';

			{
				char* pcLine = pcPlaylistBuffer;
				char* pcNextLine;
				while (pcLine && *pcLine)
				{
					int len;
					pcNextLine = strchr(pcLine, '\n');
					if (pcNextLine) { *pcNextLine = '\0'; pcNextLine++; }

					len = (int)strlen(pcLine);
					if (len > 0 && pcLine[len-1] == '\r') pcLine[len-1] = '\0';

					if (_strnicmp(pcLine, "File", 4) == 0)
					{
						char* pcEquals = strchr(pcLine, '=');
						if (pcEquals)
						{
							pcEquals++;
							while (*pcEquals && (*pcEquals == ' ' || *pcEquals == '\t')) pcEquals++;
							if (*pcEquals)
								CPL_AddPrefixedFile(hPlaylist, pcEquals, NULL, pcFilename, iPlaylist_VolumeBytes, iPlaylist_DirectoryBytes);
						}
					}
					pcLine = pcNextLine;
				}
			}

			free(pcPlaylistBuffer);
		}
		else
		{
			/* Local PLS */
			int iNumFiles, iFileIDX;
			iNumFiles = GetPrivateProfileInt("playlist", "NumberOfEntries", 0, pcFilename);

			for (iFileIDX = 0; iFileIDX < iNumFiles; iFileIDX++)
			{
				char cPlsFileHeader[32];
				char cBuffer[MAX_PATH];
				char cTitle[1024];
				DWORD dwNumCharsRead;

				sprintf_s(cPlsFileHeader, sizeof(cPlsFileHeader), "File%d", iFileIDX + 1);
				dwNumCharsRead = GetPrivateProfileString("playlist", cPlsFileHeader, NULL, cBuffer, MAX_PATH, pcFilename);
				if (dwNumCharsRead == 0) continue;

				sprintf_s(cPlsFileHeader, sizeof(cPlsFileHeader), "Title%d", iFileIDX + 1);
				dwNumCharsRead = GetPrivateProfileString("playlist", cPlsFileHeader, NULL, cTitle, 1024, pcFilename);

				if (dwNumCharsRead == 0)
					CPL_AddPrefixedFile(hPlaylist, cBuffer, NULL, pcFilename, iPlaylist_VolumeBytes, iPlaylist_DirectoryBytes);
				else
					CPL_AddPrefixedFile(hPlaylist, cBuffer, cTitle, pcFilename, iPlaylist_VolumeBytes, iPlaylist_DirectoryBytes);
			}
		}
	}
	else /* M3U */
	{
		HINTERNET hInternet = NULL;
		HINTERNET hURLStream = NULL;
		HANDLE hFile = INVALID_HANDLE_VALUE;
		char* pcPlaylistBuffer = NULL;
		DWORD dwFileLen, dwBytesRead;
		unsigned int iLastLineStartIDX, iCharIDX;
		BOOL bReadResult;
		INTERNET_BUFFERS internetbuffer;
		DWORD dwTimeout;

		if ((_strnicmp(pcFilename, CIC_HTTPHEADER,  sizeof(CIC_HTTPHEADER)  - 1) == 0) ||
			(_strnicmp(pcFilename, CIC_ICYHEADER,   sizeof(CIC_ICYHEADER)   - 1) == 0) ||
			(_strnicmp(pcFilename, CIC_HTTPSHEADER, sizeof(CIC_HTTPSHEADER) - 1) == 0) ||
			(_strnicmp(pcFilename, CIC_FTPHEADER,   sizeof(CIC_FTPHEADER)   - 1) == 0))
		{
			hInternet = InternetOpen(CP_BRISKPLAYER, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0L);
			if (!hInternet) { CP_TRACE0("CPL_AddFile::NoInternetOpen"); CPL_cb_LockWindowUpdates(FALSE); return; }

			dwTimeout = 2000;
			InternetSetOption(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &dwTimeout, sizeof(dwTimeout));

			hURLStream = InternetOpenUrl(hInternet, pcFilename, NULL, 0,
				INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE, 0);

			if (!hURLStream) { InternetCloseHandle(hInternet); CP_TRACE1("CPL_AddFile::NoOpenURL %s", pcFilename); CPL_cb_LockWindowUpdates(FALSE); return; }

			pcPlaylistBuffer = CALLOC_TYPE(char, 0x40001);
			if (!pcPlaylistBuffer) { InternetCloseHandle(hInternet); InternetCloseHandle(hURLStream); CPL_cb_LockWindowUpdates(FALSE); return; }

			internetbuffer.dwStructSize   = sizeof(internetbuffer);
			internetbuffer.Next           = NULL;
			internetbuffer.lpcszHeader    = NULL;
			internetbuffer.lpvBuffer      = pcPlaylistBuffer;
			internetbuffer.dwBufferLength = 0x40000;

			bReadResult = InternetReadFileEx(hURLStream, &internetbuffer, IRF_NO_WAIT, 0);
			InternetCloseHandle(hURLStream);
			InternetCloseHandle(hInternet);

			if (!bReadResult || !internetbuffer.dwBufferLength)
			{
				CP_TRACE1("CPL_AddFile::NoDataReturned %s", pcFilename);
				free(pcPlaylistBuffer);
				CPL_cb_LockWindowUpdates(FALSE);
				return;
			}

			iLastLineStartIDX = 0;
			for (iCharIDX = 0; iCharIDX < internetbuffer.dwBufferLength + 1; iCharIDX++)
			{
				if ((pcPlaylistBuffer[iCharIDX] == '\r'
						|| pcPlaylistBuffer[iCharIDX] == '\n'
						|| iCharIDX == internetbuffer.dwBufferLength)
						&& iLastLineStartIDX < iCharIDX)
				{
					char cBuffer[513];
					if (sscanf_s(pcPlaylistBuffer + iLastLineStartIDX, " %511[^\r\n]", cBuffer, (unsigned)sizeof(cBuffer)) == 1)
					{
						if (cBuffer[0] != '#')
							CPL_AddPrefixedFile(hPlaylist, cBuffer, NULL, pcFilename, iPlaylist_VolumeBytes, iPlaylist_DirectoryBytes);
					}
					iLastLineStartIDX = iCharIDX + 1;
				}
			}

			free(pcPlaylistBuffer);
		}
		else
		{
			/* Local M3U */
			WCHAR* pwcFilename = STR_ConvertToUnicode(pcFilename);
			if (!pwcFilename) { CPL_cb_LockWindowUpdates(FALSE); return; }

			hFile = CreateFileW(pwcFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			free(pwcFilename);

			if (hFile == INVALID_HANDLE_VALUE) { CP_TRACE1("CPL_AddFile::OpenFailed %s", pcFilename); CPL_cb_LockWindowUpdates(FALSE); return; }

			dwFileLen = GetFileSize(hFile, NULL);
			if (dwFileLen == 0) { CloseHandle(hFile); CPL_cb_LockWindowUpdates(FALSE); return; }

			pcPlaylistBuffer = CALLOC_TYPE(char, dwFileLen + 1);
			if (!pcPlaylistBuffer) { CloseHandle(hFile); CPL_cb_LockWindowUpdates(FALSE); return; }

			ReadFile(hFile, pcPlaylistBuffer, dwFileLen, &dwBytesRead, NULL);
			CloseHandle(hFile);

			iLastLineStartIDX = 0;
			for (iCharIDX = 0; iCharIDX < dwBytesRead + 1; iCharIDX++)
			{
				if ((pcPlaylistBuffer[iCharIDX] == '\r'
						|| pcPlaylistBuffer[iCharIDX] == '\n'
						|| iCharIDX == dwBytesRead)
						&& iLastLineStartIDX < iCharIDX)
				{
					char cBuffer[513];
					if (sscanf_s(pcPlaylistBuffer + iLastLineStartIDX, " %511[^\r\n]", cBuffer, (unsigned)sizeof(cBuffer)) == 1)
					{
						if (cBuffer[0] != '#')
							CPL_AddPrefixedFile(hPlaylist, cBuffer, NULL, pcFilename, iPlaylist_VolumeBytes, iPlaylist_DirectoryBytes);
					}
					iLastLineStartIDX = iCharIDX + 1;
				}
			}

			free(pcPlaylistBuffer);
		}
	}

	CPL_cb_LockWindowUpdates(FALSE);
}

////////////////////////////////////////////////////////////////////////////////
// Directory scanning
////////////////////////////////////////////////////////////////////////////////

static void SortLList(CPs_FilenameLLItem* pFirst)
{
	char** ppStrings;
	int iNumStrings = 0;
	int iStringIDX;
	CPs_FilenameLLItem* pCursor;

	if (!pFirst) return;

	for (pCursor = pFirst; pCursor; pCursor = (CPs_FilenameLLItem*)pCursor->m_pNextItem)
		iNumStrings++;

	ppStrings = CALLOC_TYPE(char*, iNumStrings);

	iStringIDX = 0;
	for (pCursor = pFirst; pCursor; pCursor = (CPs_FilenameLLItem*)pCursor->m_pNextItem)
		ppStrings[iStringIDX++] = pCursor->m_pcFilename;

	qsort(ppStrings, iNumStrings, sizeof(char*), exp_CompareStrings);

	iStringIDX = 0;
	for (pCursor = pFirst; pCursor; pCursor = (CPs_FilenameLLItem*)pCursor->m_pNextItem)
		pCursor->m_pcFilename = ppStrings[iStringIDX++];

	free(ppStrings);
}

void CPL_AddDirectory_Recurse(CP_HPLAYLIST hPlaylist, const char* pDir)
{
	CPs_FilenameLLItem* m_pFirstFile = NULL;
	CPs_FilenameLLItem* m_pFirstDir  = NULL;
	CPs_FilenameLLItem* pCursor;
	CPs_FilenameLLItem* pNextItem;
	char cFullPath[MAX_PATH];
	char cWildCard[MAX_PATH];
	const int iDirStrLen = (int)strlen(pDir);
	WIN32_FIND_DATA finddata;
	HANDLE hFileFind;

	cp_strcpy_s(cFullPath, sizeof(cFullPath), pDir);

	if (cFullPath[iDirStrLen-1] == '\\' && iDirStrLen > 1)
		cFullPath[iDirStrLen-1] = '\0';

	if (cFullPath[0] == '\0' || path_is_directory(cFullPath) == FALSE)
	{
		MessageBoxA(NULL, T(STR_ERR_NOT_VALID_DIRECTORY), cFullPath, MB_ICONERROR);
		return;
	}

	if (strcmp(cFullPath, "\\") == 0)
		cp_strcpy_s(cWildCard, sizeof(cWildCard), "\\*.*");
	else
	{
		cp_strcpy_s(cWildCard, sizeof(cWildCard), cFullPath);
		cp_strcat_s(cWildCard, sizeof(cWildCard), "\\*.*");
	}

	cp_strcat_s(cFullPath, sizeof(cFullPath), "\\");

	hFileFind = FindFirstFile(cWildCard, &finddata);
	if (hFileFind == INVALID_HANDLE_VALUE)
	{
		MessageBoxA(NULL, T(STR_ERR_COULD_NOT_SCAN), cFullPath, MB_ICONERROR);
		return;
	}

	do
	{
		char pcFullPath[MAX_PATH];
		if (finddata.cFileName[0] == '.') continue;

		cp_strcpy_s(pcFullPath, sizeof(pcFullPath), cFullPath);
		cp_strcat_s(pcFullPath, sizeof(pcFullPath), finddata.cFileName);

		if (finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			CPs_FilenameLLItem* pNewItem = MALLOC_TYPE(CPs_FilenameLLItem);
			if (!pNewItem) continue;
			pNewItem->m_pNextItem = m_pFirstDir;
			STR_AllocSetString(&pNewItem->m_pcFilename, pcFullPath, FALSE);
			m_pFirstDir = pNewItem;
		}
		else
		{
			CPs_FilenameLLItem* pNewItem = MALLOC_TYPE(CPs_FilenameLLItem);
			if (!pNewItem) continue;
			pNewItem->m_pNextItem = m_pFirstFile;
			STR_AllocSetString(&pNewItem->m_pcFilename, pcFullPath, FALSE);
			m_pFirstFile = pNewItem;
		}
	} while (FindNextFile(hFileFind, &finddata) != 0);

	FindClose(hFileFind);

	SortLList(m_pFirstDir);
	SortLList(m_pFirstFile);

	for (pCursor = m_pFirstFile; pCursor; pCursor = (CPs_FilenameLLItem*)pCursor->m_pNextItem)
		CPL_AddFile(globals.m_hPlaylist, pCursor->m_pcFilename);

	for (pCursor = m_pFirstDir; pCursor; pCursor = (CPs_FilenameLLItem*)pCursor->m_pNextItem)
		CPL_AddDirectory_Recurse(hPlaylist, pCursor->m_pcFilename);

	for (pCursor = m_pFirstFile; pCursor; pCursor = pNextItem)
	{
		pNextItem = (CPs_FilenameLLItem*)pCursor->m_pNextItem;
		free(pCursor->m_pcFilename);
		free(pCursor);
	}

	for (pCursor = m_pFirstDir; pCursor; pCursor = pNextItem)
	{
		pNextItem = (CPs_FilenameLLItem*)pCursor->m_pNextItem;
		free(pCursor->m_pcFilename);
		free(pCursor);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Drag-and-drop
////////////////////////////////////////////////////////////////////////////////

void CPL_AddDroppedFiles(CP_HPLAYLIST hPlaylist, HDROP hDrop)
{
	int iNumFiles, iFileIDX;
	char** ppFiles;
	CP_ASSERT(hPlaylist);

	iNumFiles = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
	ppFiles = CALLOC_TYPE(char*, iNumFiles);

	for (iFileIDX = 0; iFileIDX < iNumFiles; iFileIDX++)
	{
		const int iBufferSize = DragQueryFile(hDrop, iFileIDX, NULL, 0) + 1;
		ppFiles[iFileIDX] = CALLOC_TYPE(char, iBufferSize);
		DragQueryFile(hDrop, iFileIDX, ppFiles[iFileIDX], iBufferSize);
	}

	DragFinish(hDrop);

	qsort(ppFiles, iNumFiles, sizeof(char*), exp_CompareStrings);

	CLV_BeginBatch(globals.m_hPlaylistViewControl);

	for (iFileIDX = 0; iFileIDX < iNumFiles; iFileIDX++)
	{
		if (path_is_directory(ppFiles[iFileIDX]) == TRUE)
		{
			CPL_AddDirectory_Recurse(globals.m_hPlaylist, ppFiles[iFileIDX]);
			cp_strcpy_s(options.last_used_directory, sizeof(options.last_used_directory), ppFiles[iFileIDX]);
		}
		else
		{
			CPL_AddFile(globals.m_hPlaylist, ppFiles[iFileIDX]);
		}
	}

	CLV_EndBatch(globals.m_hPlaylistViewControl);

	for (iFileIDX = 0; iFileIDX < iNumFiles; iFileIDX++)
		free(ppFiles[iFileIDX]);
	free(ppFiles);

	/* Shuffle on drop if enabled */
	if (options.shuffle_play)
		PostThreadMessage(CPPL_GetWorkerThreadID(hPlaylist), CPPLWT_SYNCSHUFFLE, 0, 0);
}
