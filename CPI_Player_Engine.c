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
#include "CPI_Player_Engine.h"

////////////////////////////////////////////////////////////////////////////////
//
// This is the Cooler play engine.  It runs in a seperate thread to the UI and
// receives it's messages through the Windows messaging system.  In this system
// CP_HPLAYER will cast into a custom struct.
//
// This file contains the actual engine implementation.
//
// The convention we use (throughout) in async message data is that the caller
// allocates and the callee frees.  The convention for sync message data is
// that the caller allocates and frees (so we can use stack variables).
//
////////////////////////////////////////////////////////////////////////////////


void UpdateProgress(CPs_PlayerContext* pContext);
void EmptyOutputStream(CPs_PlayerContext* pContext);
void StartPlay(CPs_CoDecModule* pCoDec, CPs_PlayerContext* pContext);
void EnumOutputDevices(CPs_PlayerContext* pContext);
CPs_CoDecModule* OpenCoDec(CPs_PlayerContext* pContext, const char* pcFilename);
void CleanupCoDecs(CPs_PlayerContext* pContext);
void SetCurrentOutputModule(CPs_PlayerContext* pContext, CPs_OutputModule* pNewOuputModule, BOOL* pbForceRefill);
void AssociateFileExtensions(CPs_PlayerContext* pContext);

// Playlist support
char* ExtractStreamURLFromPlaylist(const char* pcPlaylistURL);

// Download playlist to temp file and return temp file path
//
[[nodiscard]] char* DownloadPlaylistToTempFile(const char* pcPlaylistURL)
{
	auto pcTempPath = (char*)NULL;
	auto szTempDir = (WCHAR[MAX_PATH]){0};  // Unicode for international paths
	auto szTempFile = (WCHAR[MAX_PATH]){0}; // Unicode for international paths
	auto hFile = INVALID_HANDLE_VALUE;
	auto dwBytesRead = (DWORD)0;
	auto dwBytesWritten = (DWORD)0;
	const auto dwChunkSize = (DWORD)4096;
	auto buffer = (BYTE[4096]){0};
	
	printf("DownloadPlaylistToTempFile: Downloading %s\n", pcPlaylistURL);
	
	// Get temp directory (Unicode)
	if (GetTempPathW(MAX_PATH, szTempDir) == 0)
	{
		printf("DownloadPlaylistToTempFile: GetTempPathW failed\n");
		return NULL;
	}
	
	// Generate temp filename (Unicode)
	if (GetTempFileNameW(szTempDir, L"BRP", 0, szTempFile) == 0)
	{
		printf("DownloadPlaylistToTempFile: GetTempFileNameW failed\n");
		return NULL;
	}
	
	// Determine file extension from URL and append (Unicode)
	const WCHAR* pwcExt = L".tmp";
	if (strstr(pcPlaylistURL, ".pls")) pwcExt = L".pls";
	else if (strstr(pcPlaylistURL, ".m3u")) pwcExt = L".m3u";
	else if (strstr(pcPlaylistURL, ".m3u8")) pwcExt = L".m3u8";
	
	// Append proper extension
	wcscat_s(szTempFile, MAX_PATH, pwcExt);
	
	// Convert to ANSI for debug output only
	char szTempFileAnsi[MAX_PATH];
	WideCharToMultiByte(CP_ACP, 0, szTempFile, -1, szTempFileAnsi, MAX_PATH, NULL, NULL);
	printf("DownloadPlaylistToTempFile: Temp file: %s\n", szTempFileAnsi);
	
	// Download the playlist
	auto hInternet = InternetOpen("BriskPlayer/3.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0L);
	if (!hInternet) 
	{
		printf("DownloadPlaylistToTempFile: InternetOpen failed\n");
		return NULL;
	}
	
	auto hURL = InternetOpenUrl(hInternet, pcPlaylistURL, NULL, 0, 
						   INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD, 0);
	if (!hURL)
	{
		printf("DownloadPlaylistToTempFile: InternetOpenUrl failed\n");
		InternetCloseHandle(hInternet);
		return NULL;
	}
	
	// Create temp file with Unicode filename for international character support
	hFile = CreateFileW(szTempFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf("DownloadPlaylistToTempFile: CreateFileW failed\n");
		InternetCloseHandle(hURL);
		InternetCloseHandle(hInternet);
		return NULL;
	}
	
	// Download and write to file
	auto dwTotalBytes = (DWORD)0;
	while (InternetReadFile(hURL, buffer, dwChunkSize, &dwBytesRead) && dwBytesRead > 0)
	{
		if (!WriteFile(hFile, buffer, dwBytesRead, &dwBytesWritten, NULL) || dwBytesWritten != dwBytesRead)
		{
			printf("DownloadPlaylistToTempFile: WriteFile failed\n");
			CloseHandle(hFile);
			InternetCloseHandle(hURL);
			InternetCloseHandle(hInternet);
			DeleteFileW(szTempFile);
			return NULL;
		}
		dwTotalBytes += dwBytesRead;
	}
	
	CloseHandle(hFile);
	InternetCloseHandle(hURL);
	InternetCloseHandle(hInternet);
	
	printf("DownloadPlaylistToTempFile: Downloaded %lu bytes\n", dwTotalBytes);
	
	if (dwTotalBytes == 0)
	{
		printf("DownloadPlaylistToTempFile: No content downloaded\n");
		DeleteFileW(szTempFile);
		return NULL;
	}
	
	// Convert Unicode path to ANSI for return (legacy interface compatibility)
	pcTempPath = STR_ConvertFromUnicode(szTempFile);
	if (!pcTempPath)
	{
		printf("DownloadPlaylistToTempFile: Failed to convert path\n");
		DeleteFileW(szTempFile);
		return NULL;
	}
	return pcTempPath;
}
////////////////////////////////////////////////////////////////////////////////
//
//
//
DWORD WINAPI CPI_Player__EngineEP(void* pCookie)
{
	auto bTerminateThread = FALSE;
	auto hr_ComState = CoInitialize(NULL);
	auto playercontext = (CPs_PlayerContext){0}; // C23 compound literal with zero initialization
	
	playercontext.m_pBaseEngineParams = (CPs_PlayEngine*)pCookie;
	playercontext.m_bOutputActive = FALSE;
	playercontext.m_iProportion_TrackLength = 0;
	playercontext.m_iLastSentTime_Secs = -1;
	playercontext.m_iLastSentTime_Proportion = -1;
	playercontext.m_iInternalVolume = 100;
	CP_CHECKOBJECT(playercontext.m_pBaseEngineParams);
	
	CP_TRACE0("Cooler Engine Startup");
		
	// Initialise CoDecs
	// Native MPEG/OGG/AAC/FLAC codecs removed - using FFmpeg for all modern formats
	// Initialize disabled codec slots as empty
	memset(&playercontext.m_CoDecs[CP_CODEC_MPEG], 0, sizeof(CPs_CoDecModule));
	memset(&playercontext.m_CoDecs[CP_CODEC_OGG], 0, sizeof(CPs_CoDecModule));
	memset(&playercontext.m_CoDecs[CP_CODEC_AAC], 0, sizeof(CPs_CoDecModule));
	memset(&playercontext.m_CoDecs[CP_CODEC_FLAC], 0, sizeof(CPs_CoDecModule));
	
	// Active codecs: WAV (basic PCM), WinAmp plugins, and FFmpeg (primary)
	CP_InitialiseCodec_WAV(&playercontext.m_CoDecs[CP_CODEC_WAV]);
	CP_InitialiseCodec_WinAmpPlugin(&playercontext.m_CoDecs[CP_CODEC_WINAMPPLUGIN]);
	CP_InitialiseCodec_FFmpeg(&playercontext.m_CoDecs[CP_CODEC_FFMPEG]);
	
	// Initialise output module
	
	if (options.decoder_output_mode > CP_OUTPUT_last)
		options.decoder_output_mode = CP_OUTPUT_last;
		
	playercontext.m_dwCurrentOutputModule = options.decoder_output_mode;
	
	CPI_Player_Output_Initialise_WaveMapper(&playercontext.m_OutputModules[CP_OUTPUT_WAVE]);
	CPI_Player_Output_Initialise_DirectSound(&playercontext.m_OutputModules[CP_OUTPUT_DIRECTSOUND]);
	CPI_Player_Output_Initialise_File(&playercontext.m_OutputModules[CP_OUTPUT_FILE]);
	CPI_Player_Output_Initialise_FAudio(&playercontext.m_OutputModules[CP_OUTPUT_FAUDIO]);
	
	playercontext.m_pCurrentOutputModule = &playercontext.m_OutputModules[playercontext.m_dwCurrentOutputModule];
	
	// Initialise EQ
	CPI_Player_Equaliser_Initialise_Basic(&playercontext.m_Equaliser);
	
	{
		CPs_PlayEngine* player = (CPs_PlayEngine*)pCookie;
		
		// Validate that the player structure is accessible before writing to it
		if (!player) {
			CP_TRACE0("Player pointer is NULL");
			return 1;
		}
		
		player->m_pContext = &playercontext;
	}
	
	// Initialise USER32.DLL for this thread
	{
		MSG msgDummy;
		PeekMessage(&msgDummy, 0, WM_USER, WM_USER, PM_NOREMOVE);
		
		// Signal this thread ready for input
		SetEvent(playercontext.m_pBaseEngineParams->m_hEvtThreadReady);
	}
	
	do
	{
		// Process any pending messages
		BOOL bForceRefill = FALSE;
		MSG msg;
		DWORD dwWaitResult;
		
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			// Decode engine message
			switch (msg.message)
			{
			
				case CPTM_QUIT:
					bTerminateThread = TRUE;
					break;
					
				case CPTM_OPENFILE:
				{
					char* pcFilename = (char*)msg.wParam;
					
					// If there is another pending openfile then ignore this one
					// This helps when this thread is non responsive (on an http connect for example)
					// and the user is hammering the hell out of the play button (as I always
					// do) - this will cause a number of open/closes to be placed into the
					// message queue which will tie up this thread for ages!!
					MSG msg2;
					
					if (PeekMessage(&msg2, NULL, CPTM_OPENFILE, CPTM_OPENFILE, PM_NOREMOVE) == FALSE)
					{
						CPs_CoDecModule* pNewCoDec;
						
						// If there is a CoDec playing then shut it down
						
						if (playercontext.m_pCurrentOutputModule->m_pCoDec)
						{
							playercontext.m_pCurrentOutputModule->m_pCoDec->CloseFile(playercontext.m_pCurrentOutputModule->m_pCoDec);
							playercontext.m_pCurrentOutputModule->m_pCoDec = NULL;
						}
						
						CP_TRACE1("Openfile \"%s\"", pcFilename);
						printf("Player Engine: Opening file: %s\n", pcFilename);
						
						// Check if this is a playlist file and download it to temp folder
						char* pcActualFilename = (char*)pcFilename;
						char* pcTempPlaylistFile = NULL;
						
						if (strstr(pcFilename, ".pls") != NULL || strstr(pcFilename, ".PLS") != NULL ||
							strstr(pcFilename, ".m3u") != NULL || strstr(pcFilename, ".M3U") != NULL ||
							strstr(pcFilename, ".m3u8") != NULL || strstr(pcFilename, ".M3U8") != NULL)
						{
							printf("Player Engine: Detected playlist file, downloading to temp folder...\n");
							pcTempPlaylistFile = DownloadPlaylistToTempFile(pcFilename);
							if (pcTempPlaylistFile)
							{
								printf("Player Engine: Downloaded playlist to: %s\n", pcTempPlaylistFile);
								pcActualFilename = pcTempPlaylistFile;
							}
							else
							{
								printf("Player Engine: Failed to download playlist file\n");
								// Continue with original URL as fallback
							}
						}
						
						pNewCoDec = OpenCoDec(&playercontext, pcActualFilename);
						
				// Clean up temp file if we created one
				if (pcTempPlaylistFile)
				{
					// Convert ANSI path back to Unicode for deletion
					WCHAR* pwcTempFile = STR_ConvertToUnicode(pcTempPlaylistFile);
					if (pwcTempFile)
					{
						// Delete the temporary playlist file from disk
						DeleteFileW(pwcTempFile);
						free(pwcTempFile);
					}
					// Free the allocated path string
					free(pcTempPlaylistFile);
					pcTempPlaylistFile = NULL;
				}						// If the open failed then request a new stream from the interface
						
						if (pNewCoDec == NULL)
						{
							PostMessage(playercontext.m_pBaseEngineParams->m_hWndNotify, CPNM_PLAYERSTATE, (WPARAM)cppsEndOfStream, 0);
						}
						
						// Check the file's format - if the sample rate, nChannels or sample size has changed
						// then clear the current output and shutdown output device (this will cause a gap
						// - but only when the format changes)
						
						else if (playercontext.m_bOutputActive == TRUE)
						{
							CPs_FileInfo FileInfo;
							pNewCoDec->GetFileInfo(pNewCoDec, &FileInfo);
							
							if (FileInfo.m_iFreq_Hz != playercontext.m_iOpenDevice_Freq_Hz
									|| FileInfo.m_bStereo != playercontext.m_bOpenDevice_Stereo
									|| FileInfo.m_b16bit != playercontext.m_bOpenDevice_16bit)
							{
								CP_TRACE0("Stream format changes - clearing stream");
								EmptyOutputStream(&playercontext);
								StartPlay(pNewCoDec, &playercontext);
								bForceRefill = TRUE;
							}
						}
						
						playercontext.m_pCurrentOutputModule->m_pCoDec = pNewCoDec;
					}
					
					#ifdef _DEBUG
					else
					{
						CP_TRACE1("Openfile of \"%s\" ignored due to other opens in the queue", pcFilename);
					}
					#endif
					
					// Cleanup
					free(pcFilename);
				}
				
				break;
				
				case CPTM_SEEK:
				
					if (playercontext.m_bOutputActive == TRUE)
					{
						// Ignore message if there is another on it's way!
						MSG msg2;
						
						if (PeekMessage(&msg2, NULL, CPTM_SEEK, CPTM_SEEK, PM_NOREMOVE) == FALSE)
						{
							if (playercontext.m_pCurrentOutputModule->m_pCoDec)
								playercontext.m_pCurrentOutputModule->m_pCoDec->Seek(playercontext.m_pCurrentOutputModule->m_pCoDec, (int)msg.wParam, (int)msg.lParam);
								
							playercontext.m_pCurrentOutputModule->Flush(playercontext.m_pCurrentOutputModule);
							
							bForceRefill = TRUE;
						}
					}
					// FALLTHROUGH - to let coolplayer know playing has resumed (bugfix from seeking when paused)  */
					
				case CPTM_PLAY:
					if (playercontext.m_pCurrentOutputModule->m_pCoDec)
					{
						// If we don't have an output stage - initialise one now
						if (playercontext.m_bOutputActive == FALSE)
						{
							StartPlay(playercontext.m_pCurrentOutputModule->m_pCoDec, &playercontext);
							bForceRefill = TRUE;
						}
						
						playercontext.m_pCurrentOutputModule->SetPause(playercontext.m_pCurrentOutputModule, FALSE);
						
						PostMessage(playercontext.m_pBaseEngineParams->m_hWndNotify, CPNM_PLAYERSTATE, (WPARAM)cppsPlaying, 0);
						playercontext.m_iLastSentTime_Secs = -1;
						playercontext.m_iLastSentTime_Proportion = -1;
						UpdateProgress(&playercontext);
					}
					
					break;
					
				case CPTM_STOP:
				
					if (playercontext.m_pCurrentOutputModule->m_pCoDec)
					{
						if (playercontext.m_pCurrentOutputModule->m_pCoDec->CloseFile)
							playercontext.m_pCurrentOutputModule->m_pCoDec->CloseFile(playercontext.m_pCurrentOutputModule->m_pCoDec);
						playercontext.m_pCurrentOutputModule->m_pCoDec = NULL;
					}
					
					if (playercontext.m_bOutputActive == TRUE)
					{
						playercontext.m_bOutputActive = FALSE;
						playercontext.m_pCurrentOutputModule->Uninitialise(playercontext.m_pCurrentOutputModule);
					}
					
					PostMessage(playercontext.m_pBaseEngineParams->m_hWndNotify, CPNM_PLAYERSTATE, (WPARAM)cppsStopped, 0);
					
					break;
					
				case CPTM_PAUSE:
					CP_TRACE0("Pause");
					
					if (playercontext.m_bOutputActive == TRUE)
						playercontext.m_pCurrentOutputModule->SetPause(playercontext.m_pCurrentOutputModule, TRUE);
						
					PostMessage(playercontext.m_pBaseEngineParams->m_hWndNotify, CPNM_PLAYERSTATE, (WPARAM)cppsPaused, 0);
					
					break;
					
				case CPTM_SETPROGRESSTRACKLENGTH:
					playercontext.m_iProportion_TrackLength = (int)msg.wParam;
					
					break;
					
				case CPTM_SENDSYNCCOOKIE:
					PostMessage(playercontext.m_pBaseEngineParams->m_hWndNotify, CPNM_SYNCCOOKIE, msg.wParam, 0);
					
					break;
					
				case CPTM_BLOCKMSGUNTILENDOFSTREAM:
					EmptyOutputStream(&playercontext);
					
					break;
					
				case CPTM_ENUMOUTPUTDEVICES:
					EnumOutputDevices(&playercontext);
					
					break;
					
				case CPTM_SETEQSETTINGS:
				{
					MSG msg2;
					CPs_EQSettings* pEQ = (CPs_EQSettings*)msg.wParam;
					
					// If there is another pending EQ message do no processing for this one (try to reduce noise)
					
					if (PeekMessage(&msg2, NULL, CPTM_SETEQSETTINGS, CPTM_OPENFILE, PM_NOREMOVE) == FALSE)
					{
						BOOL bEQEnableStateChanged;
						playercontext.m_Equaliser.ApplySettings(&playercontext.m_Equaliser, pEQ, &bEQEnableStateChanged);
						
						// Empty the buffers (this will cause a discontinuity in the music but at least
						// the EQ setting change will be immediate
						
						if (playercontext.m_bOutputActive == TRUE && playercontext.m_pCurrentOutputModule->OnEQChanged)
							playercontext.m_pCurrentOutputModule->OnEQChanged(playercontext.m_pCurrentOutputModule);
					}
					
					free(pEQ);
				}
				
				break;
				
				case CPTM_ONOUTPUTMODULECHANGE:
				{
					playercontext.m_dwCurrentOutputModule = options.decoder_output_mode;
					SetCurrentOutputModule(&playercontext, NULL, &bForceRefill);
				}
				
				break;
				
				case CPTM_ASSOCIATEFILEEXTENSIONS:
					AssociateFileExtensions(&playercontext);
					break;
					
				case CPTM_SETINTERNALVOLUME:
					playercontext.m_iInternalVolume = (int)msg.wParam;
					
					if (playercontext.m_bOutputActive == TRUE && playercontext.m_pCurrentOutputModule->SetInternalVolume)
						playercontext.m_pCurrentOutputModule->SetInternalVolume(playercontext.m_pCurrentOutputModule, playercontext.m_iInternalVolume);
						
					break;
			}
		}
		
		if (bTerminateThread)
			break;
			
		// Wait for either another message or a buffer expiry (if we have a player)
		if (playercontext.m_bOutputActive)
		{
			dwWaitResult = 0L;
			
			if (bForceRefill == FALSE)
			{
				if (playercontext.m_pCurrentOutputModule->m_evtBlockFree)
					dwWaitResult = MsgWaitForMultipleObjects(1, &playercontext.m_pCurrentOutputModule->m_evtBlockFree, FALSE, 1000, QS_POSTMESSAGE);
				else
					dwWaitResult = WAIT_OBJECT_0;
			}
			
			// If the buffer event is signaled then request a refill
			
			if (bForceRefill == TRUE || dwWaitResult == WAIT_OBJECT_0)
			{
				if (playercontext.m_pCurrentOutputModule && playercontext.m_pCurrentOutputModule->m_pCoDec)
				{
					playercontext.m_pCurrentOutputModule->RefillBuffers(playercontext.m_pCurrentOutputModule);
					
					if (playercontext.m_pCurrentOutputModule->m_pCoDec == NULL)
					{
						// Tell UI that we need another file to play
						PostMessage(playercontext.m_pBaseEngineParams->m_hWndNotify, CPNM_PLAYERSTATE, (WPARAM)cppsEndOfStream, 0);
					}
					
					else
						UpdateProgress(&playercontext);
				}
				
				// If output has finished everything that it was doing - close the engine
				
				else if (playercontext.m_pCurrentOutputModule->IsOutputComplete(playercontext.m_pCurrentOutputModule) == TRUE)
				{
					playercontext.m_bOutputActive = FALSE;
					playercontext.m_pCurrentOutputModule->Uninitialise(playercontext.m_pCurrentOutputModule);
					PostMessage(playercontext.m_pBaseEngineParams->m_hWndNotify, CPNM_PLAYERSTATE, (WPARAM)cppsStopped, 0);
				}
			}
		}
		
		else
		{
			WaitMessage();
		}
	}
	
	while (bTerminateThread == FALSE);
	
	// Clean up output (if it's still active)
	if (playercontext.m_pCurrentOutputModule->m_pCoDec)
	{
		playercontext.m_pCurrentOutputModule->m_pCoDec->CloseFile(playercontext.m_pCurrentOutputModule->m_pCoDec);
		playercontext.m_pCurrentOutputModule->m_pCoDec = NULL;
	}
	
	if (playercontext.m_bOutputActive == TRUE)
		playercontext.m_pCurrentOutputModule->Uninitialise(playercontext.m_pCurrentOutputModule);
		
	// Clean up modules
	playercontext.m_Equaliser.Uninitialise(&playercontext.m_Equaliser);
	
	CleanupCoDecs(&playercontext);
	
	if (hr_ComState == S_OK)
		CoUninitialize();
		
	CP_TRACE0("Cooler Engine terminating");
	
	return 0;
}

//
//
//
void UpdateProgress(CPs_PlayerContext* pContext)
{
	int iCurrentTime_Secs;
	
	// If the file offset (in secs) has changed resend some notifies
	
	if (pContext->m_bOutputActive == TRUE)
		iCurrentTime_Secs = pContext->m_pCurrentOutputModule->m_pCoDec->GetCurrentPos_secs(pContext->m_pCurrentOutputModule->m_pCoDec);
	else
		iCurrentTime_Secs = 0;
		
	if (iCurrentTime_Secs != pContext->m_iLastSentTime_Secs)
	{
		CPs_FileInfo* pFileInfo;
		int iFileLength_Secs;
		pContext->m_iLastSentTime_Secs = iCurrentTime_Secs;
		
		// (Re)send file info first
		pFileInfo = (CPs_FileInfo*)malloc(sizeof(*pFileInfo));
		pContext->m_pCurrentOutputModule->m_pCoDec->GetFileInfo(pContext->m_pCurrentOutputModule->m_pCoDec, pFileInfo);
		iFileLength_Secs = pFileInfo->m_iFileLength_Secs;
		PostMessage(pContext->m_pBaseEngineParams->m_hWndNotify, CPNM_FILEINFO, (WPARAM)pFileInfo, 0);
		
		// Send current progress
		PostMessage(pContext->m_pBaseEngineParams->m_hWndNotify, CPNM_FILEOFFSET_SECS, (WPARAM)iCurrentTime_Secs, 0);
		
		// Send the proportion along the track (if it has changed)
		
		if (pContext->m_iProportion_TrackLength != 0 && iFileLength_Secs != 0)
		{
			// Use C23 decimal floating-point for better audio timing precision
			auto timeRatio = (audio_precision_t)iCurrentTime_Secs / (audio_precision_t)iFileLength_Secs;
			auto proportionFloat = timeRatio * (audio_precision_t)pContext->m_iProportion_TrackLength;
			auto iProportionAlongTrack = (int)proportionFloat;
			                                  
			if (iProportionAlongTrack != pContext->m_iLastSentTime_Proportion)
			{
				pContext->m_iLastSentTime_Proportion = iProportionAlongTrack;
				PostMessage(pContext->m_pBaseEngineParams->m_hWndNotify, CPNM_FILEOFFSET_PROP, (WPARAM)iProportionAlongTrack, 0);
			}
		}
	}
}

//
//
//
void EmptyOutputStream(CPs_PlayerContext* pContext)
{
	if (pContext->m_bOutputActive == FALSE)
		return;
		
	while (pContext->m_pCurrentOutputModule->IsOutputComplete(pContext->m_pCurrentOutputModule) == FALSE)
	{
		WaitForSingleObject(pContext->m_pCurrentOutputModule->m_evtBlockFree, 1000);
		
		if (pContext->m_pCurrentOutputModule->m_pCoDec)
			UpdateProgress(pContext);
	}
	
	pContext->m_bOutputActive = FALSE;
	
	pContext->m_pCurrentOutputModule->Uninitialise(pContext->m_pCurrentOutputModule);
	PostMessage(pContext->m_pBaseEngineParams->m_hWndNotify, CPNM_PLAYERSTATE, (WPARAM)cppsStopped, 0);
}

//
//
//
void EnumOutputDevices(CPs_PlayerContext* pContext)
{
	// C23: Enumerate output modules with scoped loop variable and typed enum
	for (CP_OutputType iOutputModuleIDX = CP_OUTPUT_first; iOutputModuleIDX <= CP_OUTPUT_last; iOutputModuleIDX++)
	{
		const auto pOutputModule = pContext->m_OutputModules + iOutputModuleIDX;
		auto pcDeviceName = (char*)NULL;
		
		// Buffer frees in the called
		STR_AllocSetString(&pcDeviceName, pOutputModule->m_pcModuleName, FALSE);
		PostMessage(pContext->m_pBaseEngineParams->m_hWndNotify, CPNM_FOUNDOUTPUTDEVICE, (WPARAM)pcDeviceName, (LPARAM)iOutputModuleIDX);
	}
}

//
//
//
void StartPlay(CPs_CoDecModule* pCoDec, CPs_PlayerContext* pContext)
{
	CPs_FileInfo FileInfo;
	pCoDec->GetFileInfo(pCoDec, &FileInfo);
	pContext->m_bOutputActive = TRUE;
	pContext->m_iOpenDevice_Freq_Hz = FileInfo.m_iFreq_Hz;
	pContext->m_bOpenDevice_Stereo = FileInfo.m_bStereo;
	pContext->m_bOpenDevice_16bit = FileInfo.m_b16bit;
	
	// Get module to initialise itself
	pContext->m_Equaliser.Initialise(&pContext->m_Equaliser, FileInfo.m_iFreq_Hz, FileInfo.m_b16bit);
	pContext->m_pCurrentOutputModule->Initialise(pContext->m_pCurrentOutputModule, &FileInfo, &pContext->m_Equaliser);
	
	// If the volume isn't 100% then set the volume level
//    if(!pContext->m_iInternalVolume)
	pContext->m_pCurrentOutputModule->SetInternalVolume(pContext->m_pCurrentOutputModule, pContext->m_iInternalVolume);
}

//
//
//
CPs_CoDecModule* OpenCoDec(CPs_PlayerContext* pContext, const char* pcFilename)
{
	// const char* pcLastDot = NULL;
	int iCoDecIDX = 0;
	BOOL bOpenSucceeded = FALSE;
	DWORD dwCookie = 0;
	
	printf("OpenCoDec: Attempting to open file: %s\n", pcFilename);
	
	// Find  the extension
	char *extension = NULL;
	char *dot = strrchr(pcFilename, '.');
	
	if (dot) extension = dot + 1;
	
	if (dot)
	{
		printf("OpenCoDec: Found extension: %s\n", extension);
		
		for (iCoDecIDX = CP_CODEC_first; iCoDecIDX <= CP_CODEC_last; iCoDecIDX++)
		{
			if (CPFA_IsAssociated(&pContext->m_CoDecs[iCoDecIDX], extension, &dwCookie) == TRUE)
			{
				printf("OpenCoDec: Trying codec %d for extension %s\n", iCoDecIDX, extension);
				
				// Check if codec has a valid OpenFile function (codec might be disabled)
				if (pContext->m_CoDecs[iCoDecIDX].OpenFile == NULL)
				{
					printf("OpenCoDec: Codec %d is disabled, skipping\n", iCoDecIDX);
					continue;
				}
				
				bOpenSucceeded = pContext->m_CoDecs[iCoDecIDX].OpenFile(
									 &pContext->m_CoDecs[iCoDecIDX],
									 pcFilename,
									 dwCookie,
									 pContext->m_pBaseEngineParams->m_hWndNotify);
									 
				if (bOpenSucceeded == TRUE)
				{
					printf("OpenCoDec: Successfully opened with codec %d\n", iCoDecIDX);
					return &pContext->m_CoDecs[iCoDecIDX];
				}
				else
				{
					printf("OpenCoDec: Failed to open with codec %d\n", iCoDecIDX);
				}
			}
		}
	}
	else
	{
		printf("OpenCoDec: No extension found\n");
	}

	// If no extension or no codec matched, try fallback for streaming URLs
	if (!bOpenSucceeded && (!dot || strstr(pcFilename, "http://") == pcFilename || strstr(pcFilename, "https://") == pcFilename || strstr(pcFilename, "icy://") == pcFilename))
	{
		printf("OpenCoDec: No extension found or no codec matched - trying fallback for streaming URL\n");
		
		// For streaming URLs, try codecs in order of likelihood
		// Most internet streams are MP3, then AAC, then FLAC, then OGG
		int iFallbackOrder[] = { CP_CODEC_MPEG, CP_CODEC_AAC, CP_CODEC_FFMPEG, CP_CODEC_FLAC, CP_CODEC_OGG, CP_CODEC_WINAMPPLUGIN };
		int iFallbackCount = sizeof(iFallbackOrder) / sizeof(iFallbackOrder[0]);
		
		for (int i = 0; i < iFallbackCount && !bOpenSucceeded; i++)
		{
			iCoDecIDX = iFallbackOrder[i];
			
			// Validate codec index is within bounds
			if (iCoDecIDX < CP_CODEC_first || iCoDecIDX > CP_CODEC_last)
			{
				printf("OpenCoDec: Invalid codec index %d, skipping\n", iCoDecIDX);
				continue;
			}
			
			printf("OpenCoDec: Trying fallback codec %d for streaming URL\n", iCoDecIDX);
			
			// Check if codec has a valid OpenFile function (codec might be disabled)
			if (pContext->m_CoDecs[iCoDecIDX].OpenFile == NULL)
			{
				printf("OpenCoDec: Fallback codec %d is disabled, skipping\n", iCoDecIDX);
				continue;
			}
			
			bOpenSucceeded = pContext->m_CoDecs[iCoDecIDX].OpenFile(
								 &pContext->m_CoDecs[iCoDecIDX],
								 pcFilename,
								 0, // No cookie for fallback attempts
								 pContext->m_pBaseEngineParams->m_hWndNotify);
								 
			if (bOpenSucceeded == TRUE)
			{
				printf("OpenCoDec: Successfully opened with fallback codec %d\n", iCoDecIDX);
				return &pContext->m_CoDecs[iCoDecIDX];
			}
			else
			{
				printf("OpenCoDec: Failed with fallback codec %d\n", iCoDecIDX);
			}
		}
	}

	printf("OpenCoDec: All attempts failed, returning NULL\n");
	return NULL;
}

//
//
//
void CleanupCoDecs(CPs_PlayerContext* pContext)
{
	int iCoDecIDX = 0;
	
	for (iCoDecIDX = 0; iCoDecIDX <= CP_CODEC_last; iCoDecIDX++)
	{
		// Check if codec has a valid Uninitialise function (codec might be disabled)
		if (pContext->m_CoDecs[iCoDecIDX].Uninitialise != NULL)
			pContext->m_CoDecs[iCoDecIDX].Uninitialise(&pContext->m_CoDecs[iCoDecIDX]);
	}
}

//
//
//
void SetCurrentOutputModule(CPs_PlayerContext* pContext, CPs_OutputModule* pNewOuputModule, BOOL* pbForceRefill)
{
	if (!pNewOuputModule)
		pNewOuputModule = &pContext->m_OutputModules[pContext->m_dwCurrentOutputModule];
		
	// If the output module has changed then close the existing one and open the new one
	if (pContext->m_pCurrentOutputModule == pNewOuputModule)
		return;
		
	// Close existing
	if (pContext->m_bOutputActive)
	{
		pContext->m_pCurrentOutputModule->Uninitialise(pContext->m_pCurrentOutputModule);
		pContext->m_bOutputActive = FALSE;
	}
	
	// Switch the CoDec over to the new output module
	pNewOuputModule->m_pCoDec = pContext->m_pCurrentOutputModule->m_pCoDec;
	
	// Set the new module as current
	pContext->m_pCurrentOutputModule = pNewOuputModule;
	
	if (pContext->m_bOutputActive == FALSE && pContext->m_pCurrentOutputModule->m_pCoDec)
	{
		StartPlay(pContext->m_pCurrentOutputModule->m_pCoDec, pContext);
		*pbForceRefill = TRUE;
	}
}

//
//
//
void AssociateFileExtensions(CPs_PlayerContext* pContext)
{
	int iCoDecIDX;
	
	for (iCoDecIDX = 0; iCoDecIDX <= CP_CODEC_last; iCoDecIDX++)
		CPFA_AssociateWithEXE(&pContext->m_CoDecs[iCoDecIDX]);
}

//
//
//

