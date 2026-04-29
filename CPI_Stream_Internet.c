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
#include "CPI_CircleBuffer.h"
#include "CPI_Player_Messages.h"

#ifndef _stdcall
#define _stdcall __stdcall
#endif

////////////////////////////////////////////////////////////////////////////////
// Streaming Constants - Network and Buffer Configuration
////////////////////////////////////////////////////////////////////////////////

// Circular buffer configuration for streaming audio
#define CIC_STREAMBUFFERSIZE  0x40000  // 262,144 bytes (256KB) - Main stream buffer
#define CIC_PREBUFFERAMOUNT   0x8000   // 32,768 bytes (32KB) - Pre-buffer before playback
#define CIC_READCHUNKSIZE     0x1000   // 4,096 bytes (4KB) - Network read chunk size

// Security and resource limits
#define CIC_MAX_METADATA_SIZE 4096     // 4KB - Maximum Icecast metadata block size
#define CIC_MAX_PLAYLIST_SIZE (10 * 1024 * 1024)  // 10MB - Max playlist file size
#define CIC_MAX_LINE_LENGTH   8192     // 8KB - Maximum line length in playlist files
#define CIC_NETWORK_TIMEOUT_MS 15000   // 15 seconds - Consistent network timeout

////////////////////////////////////////////////////////////////////////////////
//

typedef struct _CPs_BufferFillerContext
{
	char* m_pcFlexiURL;
	CPs_CircleBuffer* m_pCircleBuffer;
	BOOL m_bTerminate;
	HWND m_hWndNotify;
	DWORD m_dwIcyMetaInt;      // Metadata interval from server
	DWORD m_dwAudioBytesRead;  // Audio bytes read since last metadata
	DWORD m_dwTotalBytesRead;  // Total bytes read (for progress logging)
	
} CPs_BufferFillerContext;

//
//

typedef struct _CPs_InStream_Internet
{
	CPs_CircleBuffer* m_pCircleBuffer;
	HANDLE m_hFillerThread;
	CPs_BufferFillerContext* m_pBufferFillContext;
	
} CPs_InStream_Internet;

//
//
////////////////////////////////////////////////////////////////////////////////



void CPSINET_Uninitialise(CPs_InStream* pStream);
BOOL CPSINET_Read(CPs_InStream* pStream, void* pDestBuffer, const size_t iBytesToRead, size_t* piBytesRead);
void CPSINET_Seek(CPs_InStream* pStream, const size_t iNewOffset);
UINT CPSINET_Tell(CPs_InStream* pStream);
UINT CPSINET_GetLength(CPs_InStream* pStream);
BOOL CPSINET_IsSeekable(CPs_InStream* pStream);
//
// Helper function to read data while handling Icecast metadata
//
BOOL ReadStreamData(HINTERNET hURLStream, CPs_BufferFillerContext* pContext, BYTE* pBuffer, DWORD dwRequestedBytes, DWORD* pdwBytesRead)
{
	*pdwBytesRead = 0;
	
	// If no metadata interval is set, just read normally
	if (pContext->m_dwIcyMetaInt == 0)
	{
		return InternetReadFile(hURLStream, pBuffer, dwRequestedBytes, pdwBytesRead);
	}
	
	// Loop to skip metadata blocks and deliver audio data (avoids recursion)
	for (;;)
	{
	// Calculate how many audio bytes we can read before hitting metadata
	DWORD dwBytesUntilMeta = pContext->m_dwIcyMetaInt - pContext->m_dwAudioBytesRead;
	DWORD dwBytesToRead = min(dwRequestedBytes, dwBytesUntilMeta);
	
	if (dwBytesToRead > 0)
	{
		BOOL bResult = InternetReadFile(hURLStream, pBuffer, dwBytesToRead, pdwBytesRead);
		if (bResult && *pdwBytesRead > 0)
		{
			pContext->m_dwAudioBytesRead += *pdwBytesRead;
		}
		return bResult;
	}
	else
	{
		// We've hit the metadata boundary - read metadata
		BYTE bMetaLength;
		DWORD dwMetaBytes;
		
		if (!InternetReadFile(hURLStream, &bMetaLength, 1, &dwMetaBytes) || dwMetaBytes != 1)
		{
			return FALSE;
		}
		
		DWORD dwMetaDataSize = bMetaLength * 16;
		
		// Validate metadata size to prevent memory exhaustion
		if (dwMetaDataSize > CIC_MAX_METADATA_SIZE)
		{
			CP_TRACE0("EP_FillerThread::Metadata block too large");
			CP_LOG_WARNING("Metadata block size %lu exceeds maximum %d bytes\n", dwMetaDataSize, CIC_MAX_METADATA_SIZE);
			// Skip the metadata but don't fail the connection
			BYTE bDummy;
			for (DWORD i = 0; i < dwMetaDataSize; i++)
			{
				if (!InternetReadFile(hURLStream, &bDummy, 1, &dwMetaBytes))
					return FALSE;
			}
			pContext->m_dwAudioBytesRead = 0;
			continue; // Loop back to try reading audio data
		}
		
		CP_TRACE1("EP_FillerThread::Reading metadata block of %lu bytes", dwMetaDataSize);
		
		if (dwMetaDataSize > 0)
		{
			// Read and discard metadata (or parse it for song info)
			BYTE* pMetaBuffer = CALLOC_TYPE(BYTE, dwMetaDataSize);
			if (pMetaBuffer)
			{
				DWORD dwMetaRead;
				if (InternetReadFile(hURLStream, pMetaBuffer, dwMetaDataSize, &dwMetaRead))
				{
					// Parse metadata for song info (optional)
					char* pcMetadata = (char*)pMetaBuffer;
					char* pcTitle = strstr(pcMetadata, "StreamTitle='");
					if (pcTitle)
					{
						pcTitle += 13; // Skip "StreamTitle='"
						char* pcTitleEnd = strstr(pcTitle, "';");
						if (pcTitleEnd)
						{
							*pcTitleEnd = '\0';
							CP_TRACE1("EP_FillerThread::Now playing: %s", pcTitle);
						}
					}
				}
				free(pMetaBuffer);
			}
		}
		
		// Reset audio byte counter and loop to read audio data
		pContext->m_dwAudioBytesRead = 0;
		continue;
	}
	} // end for(;;)
}

//
// Helper function to download playlist content
//
char* DownloadPlaylistContent(const char* pcURL)
{
	HINTERNET hInternet, hURL;
	char* pcContent = NULL;
	DWORD dwBytesRead, dwTotalSize = 0;
	const DWORD dwChunkSize = 4096;
	
	// Simple test - output to console regardless of debug mode
	CP_LOG_DEBUG("DownloadPlaylistContent: Starting download of %s\n", pcURL);
	
	hInternet = InternetOpen("BriskPlayer/3.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0L);
	if (!hInternet) 
	{
		DWORD dwError = GetLastError();
		CP_LOG_ERROR("DownloadPlaylistContent: InternetOpen failed with error %lu\n", dwError);
		return NULL;
	}
	
	// Set consistent timeouts for playlist downloads
	DWORD dwTimeout = CIC_NETWORK_TIMEOUT_MS;
	InternetSetOption(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &dwTimeout, sizeof(dwTimeout));
	InternetSetOption(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &dwTimeout, sizeof(dwTimeout));
	
	hURL = InternetOpenUrl(hInternet, pcURL, NULL, 0, 
						   INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD, 0);
	if (!hURL)
	{
		DWORD dwError = GetLastError();
		CP_LOG_ERROR("DownloadPlaylistContent: InternetOpenUrl failed for %s with error %lu\n", pcURL, dwError);
		InternetCloseHandle(hInternet);
		return NULL;
	}
	
	CP_LOG_VERBOSE("DownloadPlaylistContent: Successfully opened URL, starting to read content\n");
	
	// Read the content in chunks
	char* pcBuffer = CALLOC_TYPE(char, dwChunkSize);
	pcContent = CALLOC_TYPE(char, 1); // Start with minimal allocation
	pcContent[0] = '\0';
	
	while (InternetReadFile(hURL, pcBuffer, dwChunkSize, &dwBytesRead) && dwBytesRead > 0)
	{
		// Enforce maximum playlist size to prevent memory exhaustion
		if (dwTotalSize + dwBytesRead > CIC_MAX_PLAYLIST_SIZE)
		{
			CP_LOG_WARNING("DownloadPlaylistContent: Playlist too large (exceeds %d bytes)\n", CIC_MAX_PLAYLIST_SIZE);
			free(pcContent);
			pcContent = NULL;
			break;
		}
		
		// Check for integer overflow before realloc
		if (dwTotalSize > (DWORD)(SIZE_MAX - dwBytesRead - 1))
		{
			CP_LOG_WARNING("DownloadPlaylistContent: Content too large, aborting\n");
			free(pcContent);
			pcContent = NULL;
			break;
		}
		
		char* pcNewContent = (char*)realloc(pcContent, dwTotalSize + dwBytesRead + 1);
		if (!pcNewContent)
		{
			CP_LOG_ERROR("DownloadPlaylistContent: Failed to allocate memory\n");
			free(pcContent);
			pcContent = NULL;
			break;
		}
		pcContent = pcNewContent;
		
		memcpy(pcContent + dwTotalSize, pcBuffer, dwBytesRead);
		dwTotalSize += dwBytesRead;
		pcContent[dwTotalSize] = '\0';
		CP_LOG_VERBOSE("DownloadPlaylistContent: Reading chunk\n");
	}
	
	free(pcBuffer);
	InternetCloseHandle(hURL);
	InternetCloseHandle(hInternet);
	
	if (!pcContent || dwTotalSize == 0)
	{
		CP_LOG_WARNING("DownloadPlaylistContent: No content downloaded or empty file\n");
		if (pcContent)
		{
			free(pcContent);
			pcContent = NULL;
		}
		return NULL;
	}
	
	CP_LOG_DEBUG("DownloadPlaylistContent: Download complete - %lu bytes from %s\n", dwTotalSize, pcURL);
	
	if (dwTotalSize > 0 && dwTotalSize < 500)
	{
		CP_LOG_VERBOSE("DownloadPlaylistContent: Content: %s\n", pcContent);
	}
	
	return pcContent;
}

//
// Helper function to parse .pls playlist files
//
char* ParsePLSPlaylist(const char* pcContent)
{
	char* pcStreamURL = NULL;
	char* pcContentCopy = _strdup(pcContent); // Work with a copy since strtok_s modifies the string
	if (!pcContentCopy)
		return NULL;
	char* pcContext = NULL;
	char* pcLine = strtok_s(pcContentCopy, "\r\n", &pcContext);
	
	CP_TRACE0("ParsePLSPlaylist: Starting to parse PLS content");
	
	while (pcLine)
	{
		// Validate line length to prevent buffer issues
		size_t iLineLen = strlen(pcLine);
		if (iLineLen > CIC_MAX_LINE_LENGTH)
		{
			CP_TRACE0("ParsePLSPlaylist: Line too long, skipping");
			pcLine = strtok_s(NULL, "\r\n", &pcContext);
			continue;
		}
		
		CP_TRACE1("ParsePLSPlaylist: Processing line: %s", pcLine);
		
		// Look for File1=URL, File2=URL, etc.
		if (_strnicmp(pcLine, "File", 4) == 0)
		{
			char* pcEquals = strchr(pcLine, '=');
			if (pcEquals)
			{
				pcEquals++; // Skip the '=' character
				while (*pcEquals == ' ' || *pcEquals == '\t') pcEquals++; // Skip whitespace
				
				if (strlen(pcEquals) > 0)
				{
					pcStreamURL = _strdup(pcEquals);
					if (pcStreamURL)
					{
						CP_TRACE1("ParsePLSPlaylist: Found stream URL: %s", pcStreamURL);
						break; // Use the first URL found
					}
					else
					{
						CP_TRACE0("ParsePLSPlaylist: Failed to allocate memory for URL");
					}
				}
			}
		}
		pcLine = strtok_s(NULL, "\r\n", &pcContext);
	}
	
	if (!pcStreamURL)
	{
		CP_TRACE0("ParsePLSPlaylist: No stream URLs found in PLS file");
	}
	
	free(pcContentCopy);
	return pcStreamURL;
}

//
// Helper function to parse .m3u/.m3u8 playlist files
//
char* ParseM3UPlaylist(const char* pcContent)
{
	char* pcStreamURL = NULL;
	char* pcContentCopy = _strdup(pcContent); // Work with a copy since strtok_s modifies the string
	if (!pcContentCopy)
		return NULL;
	char* pcContext = NULL;
	char* pcLine = strtok_s(pcContentCopy, "\r\n", &pcContext);
	
	while (pcLine)
	{
		// Validate line length
		size_t iLineLen = strlen(pcLine);
		if (iLineLen > CIC_MAX_LINE_LENGTH)
		{
			CP_TRACE0("ParseM3UPlaylist: Line too long, skipping");
			pcLine = strtok_s(NULL, "\r\n", &pcContext);
			continue;
		}
		
		// Skip comments and empty lines
		while (*pcLine == ' ' || *pcLine == '\t') pcLine++; // Skip leading whitespace
		
		if (strlen(pcLine) > 0 && pcLine[0] != '#')
		{
			// This should be a URL
			if (strstr(pcLine, "://") != NULL) // Basic URL validation
			{
				pcStreamURL = _strdup(pcLine);
				if (pcStreamURL)
				{
					CP_TRACE1("ParseM3UPlaylist: Found stream URL: %s", pcStreamURL);
					break; // Use the first URL found
				}
				else
				{
					CP_TRACE0("ParseM3UPlaylist: Failed to allocate memory for URL");
				}
			}
		}
		pcLine = strtok_s(NULL, "\r\n", &pcContext);
	}
	
	free(pcContentCopy);
	return pcStreamURL;
}

//
// Helper function to extract stream URL from playlist
//
char* ExtractStreamURLFromPlaylist(const char* pcPlaylistURL)
{
	char* pcContent = DownloadPlaylistContent(pcPlaylistURL);
	char* pcStreamURL = NULL;
	
	if (!pcContent)
	{
		CP_TRACE1("ExtractStreamURLFromPlaylist: Failed to download playlist: %s", pcPlaylistURL);
		return NULL;
	}
	
	// Determine playlist format and parse accordingly
	// Check for .pls first (case insensitive)
	if (strstr(pcPlaylistURL, ".pls") != NULL || strstr(pcPlaylistURL, ".PLS") != NULL)
	{
		CP_TRACE0("ExtractStreamURLFromPlaylist: Parsing as PLS format");
		pcStreamURL = ParsePLSPlaylist(pcContent);
	}
	// Check for .m3u formats (case insensitive)
	else if (strstr(pcPlaylistURL, ".m3u") != NULL || strstr(pcPlaylistURL, ".M3U") != NULL)
	{
		CP_TRACE0("ExtractStreamURLFromPlaylist: Parsing as M3U format");
		pcStreamURL = ParseM3UPlaylist(pcContent);
	}
	else
	{
		// Try to auto-detect format by content
		CP_TRACE0("ExtractStreamURLFromPlaylist: Auto-detecting format by content");
		if (strstr(pcContent, "File1=") != NULL || strstr(pcContent, "[playlist]") != NULL)
		{
			CP_TRACE0("ExtractStreamURLFromPlaylist: Content appears to be PLS format");
			pcStreamURL = ParsePLSPlaylist(pcContent);
		}
		else
		{
			CP_TRACE0("ExtractStreamURLFromPlaylist: Assuming M3U format");
			pcStreamURL = ParseM3UPlaylist(pcContent);
		}
	}
	
	free(pcContent);
	return pcStreamURL;
}

////////////////////////////////////////////////////////////////////////////////
//
//
//
unsigned int _stdcall EP_FillerThread(void* _pContext)
{
	CPs_BufferFillerContext* pContext = (CPs_BufferFillerContext*)_pContext;
	HINTERNET hInternet;
	HINTERNET hURLStream;
	DWORD dwTimeout;
	BOOL bStreamComplete = FALSE;
	BYTE bReadBuffer[CIC_READCHUNKSIZE];
	char* pcActualURL = NULL;
	char* pcHeaders = NULL;
	BOOL bIsIcyStream = FALSE;
	
	CP_CHECKOBJECT(pContext);
	
	PostMessage(pContext->m_hWndNotify, CPNM_SETSTREAMINGSTATE, (WPARAM)TRUE, (LPARAM)0);
	
	CP_TRACE1("EP_FillerThread::Starting stream for URL: %s", pContext->m_pcFlexiURL);
	
	// Check if this is an icy:// URL and convert to http://
	if (_strnicmp(pContext->m_pcFlexiURL, "icy://", 6) == 0)
	{
		bIsIcyStream = TRUE;
		// Convert icy:// to http://
		size_t urlLen = strlen(pContext->m_pcFlexiURL);
		pcActualURL = CALLOC_TYPE(char, urlLen + 3); // +3 for "http" vs "icy" difference
		if (pcActualURL)
		{
			strcpy_s(pcActualURL, urlLen + 3, "http://");
			strcat_s(pcActualURL, urlLen + 3, pContext->m_pcFlexiURL + 6); // Skip "icy://"
		}
	}
	else
	{
		// Check if it's a regular HTTP stream that might be Icecast/SHOUTcast
		bIsIcyStream = TRUE; // Assume all HTTP streams might have metadata
		pcActualURL = _strdup(pContext->m_pcFlexiURL);
	}
	
	// Prepare headers for Icecast/SHOUTcast metadata support
	// For testing, we can disable metadata to see if that's the issue
	if (bIsIcyStream)
	{
		// Try without metadata first to see if that fixes the issue
		pcHeaders = _strdup("User-Agent: BriskPlayer/3.0\r\n"
							"Accept: */*\r\n"
							"Connection: close\r\n");
		// TODO: Re-enable metadata later: "Icy-MetaData: 1\r\n"
	}
	
	// Validate URL format
	if (!pcActualURL)
	{
		if (pcHeaders) free(pcHeaders);
		pContext->m_pCircleBuffer->SetComplete(pContext->m_pCircleBuffer);
		CP_TRACE0("EP_FillerThread::URL allocation failed");
		return 0;
	}
	
	// Check that we can open this file
	hInternet = InternetOpen("BriskPlayer/3.0",
							 INTERNET_OPEN_TYPE_PRECONFIG,
							 NULL, NULL, 0L);
	                         
	if (hInternet == NULL)
	{
		DWORD dwError = GetLastError();
		(void)dwError; // May be used for debugging
		if (pcActualURL) free(pcActualURL);
		if (pcHeaders) free(pcHeaders);
		pContext->m_pCircleBuffer->SetComplete(pContext->m_pCircleBuffer);
		CP_TRACE1("EP_FillerThread::InternetOpen failed with error: %lu", dwError);
		return 0;
	}
	
	CP_TRACE0("EP_FillerThread::InternetOpen successful");
	
	dwTimeout = 10000; // 10 second timeout for streaming servers
	InternetSetOption(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &dwTimeout, sizeof(dwTimeout));
	InternetSetOption(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &dwTimeout, sizeof(dwTimeout));
	InternetSetOption(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &dwTimeout, sizeof(dwTimeout));
	
	CP_TRACE1("EP_FillerThread::Attempting to connect to: %s", pcActualURL);
	
	hURLStream = InternetOpenUrl(hInternet,
								 pcActualURL,
								 pcHeaders,
								 pcHeaders ? strlen(pcHeaders) : 0,
								 INTERNET_FLAG_NO_CACHE_WRITE
								 | INTERNET_FLAG_PRAGMA_NOCACHE
								 | INTERNET_FLAG_RELOAD,
								 0);
	                             
	if (hURLStream == NULL)
	{
		DWORD dwError = GetLastError();
		(void)dwError; // May be used for debugging
		InternetCloseHandle(hInternet);
		if (pcActualURL) free(pcActualURL);
		if (pcHeaders) free(pcHeaders);
		pContext->m_pCircleBuffer->SetComplete(pContext->m_pCircleBuffer);
		CP_TRACE2("EP_FillerThread::Failed to open URL %s (Error: %lu)", pcActualURL ? pcActualURL : pContext->m_pcFlexiURL, dwError);
		return 0;
	}
	
	// Clean up temporary strings
	if (pcActualURL) free(pcActualURL);
	if (pcHeaders) free(pcHeaders);
	
	CP_TRACE0("EP_FillerThread::Successfully opened URL stream");
	
	// Check if we got any response headers indicating this is a streaming server
	{
		char szBuffer[1024];
		DWORD dwBufferSize = sizeof(szBuffer);
		if (HttpQueryInfo(hURLStream, HTTP_QUERY_SERVER, szBuffer, &dwBufferSize, NULL))
		{
			CP_TRACE1("EP_FillerThread::Server: %s", szBuffer);
		}
		
		dwBufferSize = sizeof(szBuffer);
		if (HttpQueryInfo(hURLStream, HTTP_QUERY_CONTENT_TYPE, szBuffer, &dwBufferSize, NULL))
		{
			CP_TRACE1("EP_FillerThread::Content-Type: %s", szBuffer);
		}
		
		// Check for Icecast metadata interval
		dwBufferSize = sizeof(szBuffer);
		if (HttpQueryInfo(hURLStream, HTTP_QUERY_CUSTOM, szBuffer, &dwBufferSize, NULL))
		{
			// Need to use HttpQueryInfo with a custom header name
			DWORD dwIndex = 0;
			while (HttpQueryInfo(hURLStream, HTTP_QUERY_RAW_HEADERS_CRLF, szBuffer, &dwBufferSize, &dwIndex))
			{
				if (_strnicmp(szBuffer, "icy-metaint:", 12) == 0)
				{
					char* pcValue = szBuffer + 12;
					long lMetaInt;
					while (*pcValue == ' ' || *pcValue == '\t') pcValue++; // Skip whitespace
					lMetaInt = atol(pcValue);
					// Validate: must be positive and reasonable (at least 256 bytes, at most 1MB)
					if (lMetaInt >= 256 && lMetaInt <= 1048576)
					{
						pContext->m_dwIcyMetaInt = (DWORD)lMetaInt;
						CP_TRACE1("EP_FillerThread::Found icy-metaint: %lu", pContext->m_dwIcyMetaInt);
					}
					else
					{
						CP_TRACE1("EP_FillerThread::Ignoring invalid icy-metaint: %ld", lMetaInt);
						pContext->m_dwIcyMetaInt = 0;
					}
					break;
				}
				dwBufferSize = sizeof(szBuffer);
			}
		}
	}
	
	// Perform reading
	CP_TRACE0("EP_FillerThread::Starting data reading loop");
	
	while (pContext->m_bTerminate == FALSE && bStreamComplete == FALSE)
	{
		BOOL bReadResult;
		
		// Is our circle buffer full?
		
		if (pContext->m_pCircleBuffer->GetFreeSize(pContext->m_pCircleBuffer) < CIC_READCHUNKSIZE)
		{
			Sleep(20);
			continue;
		}
		
		// Read in another chunk - for now, use simple reading without metadata
		DWORD dwBytesRead = 0;
		bReadResult = InternetReadFile(hURLStream, bReadBuffer, CIC_READCHUNKSIZE, &dwBytesRead);
		
		if (bReadResult == FALSE)
		{
			DWORD dwError = GetLastError();
			CP_TRACE1("EP_FillerThread::InternetReadFile failed with error: %lu", dwError);
			bStreamComplete = TRUE;
		}
		else if (dwBytesRead == 0)
		{
			// No more data available - this might be normal for some streams
			Sleep(50);
		}
		else
		{
			DWORD dwTotalBytesRead = pContext->m_dwTotalBytesRead;
			dwTotalBytesRead += dwBytesRead;
			pContext->m_dwTotalBytesRead = dwTotalBytesRead;
			
			pContext->m_pCircleBuffer->Write(pContext->m_pCircleBuffer,
											 bReadBuffer,
											 dwBytesRead);
			                                 
			// Log every 64KB received
			if ((dwTotalBytesRead % 65536) < dwBytesRead)
			{
				CP_TRACE1("EP_FillerThread::Received %lu total bytes", dwTotalBytesRead);
			}
			
			PostMessage(pContext->m_hWndNotify,
						CPNM_SETSTREAMINGSTATE,
						(WPARAM)TRUE,
						(LPARAM)(pContext->m_pCircleBuffer->GetUsedSize(pContext->m_pCircleBuffer)*100) / CIC_STREAMBUFFERSIZE);
		}
	}
	
	InternetCloseHandle(hURLStream);
	
	InternetCloseHandle(hInternet);
	
	pContext->m_pCircleBuffer->SetComplete(pContext->m_pCircleBuffer);
	PostMessage(pContext->m_hWndNotify, CPNM_SETSTREAMINGSTATE, (WPARAM)FALSE, (LPARAM)0);
	CP_TRACE0("EP_FillerThread normal shutdown");
	return 0;
}

//
//
//
CPs_InStream* CP_CreateInStream_Internet(const char* pcFlexiURL, HWND hWndOwner)
{
	CPs_InStream* pNewStream;
	CPs_InStream_Internet* pContext;
	unsigned int iUsedSpace;
	char* pcActualURL = NULL;
	
	CP_TRACE1("CP_CreateInStream_Internet: Entry point with URL: %s", pcFlexiURL);
	
	// Check if this is a playlist file (case insensitive)
	if (strstr(pcFlexiURL, ".pls") != NULL || strstr(pcFlexiURL, ".PLS") != NULL ||
		strstr(pcFlexiURL, ".m3u") != NULL || strstr(pcFlexiURL, ".M3U") != NULL ||
		strstr(pcFlexiURL, ".m3u8") != NULL || strstr(pcFlexiURL, ".M3U8") != NULL)
	{
		CP_TRACE1("CP_CreateInStream_Internet: Detected playlist file: %s", pcFlexiURL);
		
		// Extract the actual stream URL from the playlist
		pcActualURL = ExtractStreamURLFromPlaylist(pcFlexiURL);
		if (!pcActualURL)
		{
			CP_TRACE1("CP_CreateInStream_Internet: Failed to extract stream URL from playlist: %s", pcFlexiURL);
			return NULL;
		}
		CP_TRACE1("CP_CreateInStream_Internet: Extracted stream URL: %s", pcActualURL);
	}
	else
	{
		CP_TRACE0("CP_CreateInStream_Internet: Not a playlist file, using URL directly");
		// Use the URL directly
		pcActualURL = _strdup(pcFlexiURL);
	}
	
	if (!pcActualURL)
	{
		CP_TRACE0("CP_CreateInStream_Internet: No URL to work with");
		return NULL;
	}
	
	// Setup stream object
	{
		pNewStream = MALLOC_TYPE(CPs_InStream);
		pContext = MALLOC_TYPE(CPs_InStream_Internet);
		if (!pNewStream || !pContext)
		{
			free(pNewStream);
			free(pContext);
			free(pcActualURL);
			return NULL;
		}
		
		CP_InStream_Init(pNewStream,
		    CPSINET_Uninitialise,
		    CPSINET_Read,
		    CPSINET_Seek,
		    NULL, /* Tell not supported for internet streams */
		    CPSINET_GetLength,
		    CPSINET_IsSeekable,
		    pContext);
		
		pContext->m_pCircleBuffer = CP_CreateCircleBuffer(CIC_STREAMBUFFERSIZE);
	}
	
	// Create thread to fill stream
	{
		CPs_BufferFillerContext* pBufferFillContext;
		UINT uiThreadID;
		
		// Setup context
		pBufferFillContext = MALLOC_TYPE(CPs_BufferFillerContext);
		if (!pBufferFillContext)
		{
			CP_TRACE0("CP_CreateInStream_Internet: Failed to allocate filler context");
			pContext->m_pCircleBuffer->Uninitialise(pContext->m_pCircleBuffer);
			free(pContext);
			free(pNewStream);
			free(pcActualURL);
			return NULL;
		}
		pBufferFillContext->m_pCircleBuffer = pContext->m_pCircleBuffer;
		pBufferFillContext->m_bTerminate = FALSE;
		STR_AllocSetString(&pBufferFillContext->m_pcFlexiURL, pcActualURL, FALSE); // Use the actual stream URL
		pBufferFillContext->m_hWndNotify = hWndOwner;
		pBufferFillContext->m_dwIcyMetaInt = 0;      // Will be set from server response
		pBufferFillContext->m_dwAudioBytesRead = 0;  // Reset counter
		pBufferFillContext->m_dwTotalBytesRead = 0;  // Reset total counter
		
		// Start thread
		pContext->m_hFillerThread = (HANDLE)_beginthreadex(NULL, 0, EP_FillerThread, pBufferFillContext, 0, &uiThreadID);
		pContext->m_pBufferFillContext = pBufferFillContext;
	}
	
	// Pre buffer some data
	CP_TRACE0("EP_FillerThread::Starting prebuffering");
	
	{
		int iMaxWaitIterations = 100; // Maximum 10 seconds of waiting (100 * 100ms)
		int iWaitIterations = 0;
		
		do
		{
			MSG msg;
			BOOL bMessageReceived;
			
			// Stream is never going to have more data in it
			if (pContext->m_pCircleBuffer->IsComplete(pContext->m_pCircleBuffer))
			{
				CP_TRACE0("EP_FillerThread::Stream marked complete during prebuffering");
				break;
			}
				
			Sleep(100);
			iWaitIterations++;
			
			iUsedSpace = pContext->m_pCircleBuffer->GetUsedSize(pContext->m_pCircleBuffer);
			
			// Log progress every second
			if (iWaitIterations % 10 == 0)
			{
				CP_TRACE2("EP_FillerThread::Prebuffering: %d bytes, iteration %d", iUsedSpace, iWaitIterations);
			}
			
			// Stop prebuffering if there is a stop in the queue for the engine
			bMessageReceived = PeekMessage(&msg, NULL, CPTM_STOP, CPTM_STOP, PM_NOREMOVE);
			
			if (bMessageReceived)
			{
				CP_TRACE0("EP_FillerThread::Stop message received during prebuffering");
				break;
			}
			
			// Timeout protection - don't wait forever
			if (iWaitIterations >= iMaxWaitIterations)
			{
				CP_TRACE0("EP_FillerThread::Prebuffering timeout - proceeding with available data");
				break;
			}
		}
		while (iUsedSpace < CIC_PREBUFFERAMOUNT);
	}
	
	CP_TRACE1("EP_FillerThread::Prebuffering complete - %d bytes available", iUsedSpace);
	
	// Clean up allocated URL string
	if (pcActualURL) 
		free(pcActualURL);
	
	return pNewStream;
}

//
//
//
void CPSINET_Uninitialise(CPs_InStream* pStream)
{
	CPs_InStream_Internet* pContext = (CPs_InStream_Internet*)pStream->m_pModuleCookie;
	CP_CHECKOBJECT(pContext);
	
	// Clear the thread
	pContext->m_pBufferFillContext->m_bTerminate = TRUE;
	WaitForSingleObject(pContext->m_hFillerThread, INFINITE);
	CloseHandle(pContext->m_hFillerThread);
	free(pContext->m_pBufferFillContext->m_pcFlexiURL);
	free(pContext->m_pBufferFillContext);
	
	// Free this context
	pContext->m_pCircleBuffer->Uninitialise(pContext->m_pCircleBuffer);
	free(pContext);
	free(pStream);
}

//
//
//
BOOL CPSINET_Read(CPs_InStream* pStream, void* pDestBuffer, const size_t iBytesToRead, size_t* piBytesRead)
{
	CPs_InStream_Internet* pContext = (CPs_InStream_Internet*)pStream->m_pModuleCookie;
	CP_CHECKOBJECT(pContext);
	
	return pContext->m_pCircleBuffer->Read(pContext->m_pCircleBuffer, pDestBuffer, iBytesToRead, piBytesRead);
}

//
//
//
void CPSINET_Seek(CPs_InStream* pStream, const size_t iNewOffset)
{
	(void)iNewOffset; // Unused - internet streams cannot seek
#ifdef _DEBUG
	CPs_InStream_Internet* pContext = (CPs_InStream_Internet*)pStream->m_pModuleCookie;
	CP_CHECKOBJECT(pContext);
#else
	(void)pStream; // Unused in release builds
#endif
}

//
//
//
unsigned int CPSINET_GetLength(CPs_InStream* pStream)
{
#ifdef _DEBUG
	CPs_InStream_Internet* pContext = (CPs_InStream_Internet*)pStream->m_pModuleCookie;
	CP_CHECKOBJECT(pContext);
#else
	(void)pStream; // Unused in release builds
#endif
	return 0xFFFFFFFF;
}

//
//
//
BOOL CPSINET_IsSeekable(CPs_InStream* pStream)
{
	(void)pStream;
	return FALSE;
}

//
//
//
UINT CPSINET_Tell(CPs_InStream* pStream)
{
	(void)pStream;
	return 0;
	
}



