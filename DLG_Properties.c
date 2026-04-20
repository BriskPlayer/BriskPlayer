/*
 * DLG_Properties.c
 * 
 * Track properties dialog
 */

#include "stdafx.h"
#include "globals.h"
#include "resource.h"
#include "CPI_PlaylistItem.h"
#include "CPI_TagLib.h"
#include "CPString.h"
#include <stdio.h>
#include <shellapi.h>

typedef struct _CPDlgProperties_Data
{
	CP_HPLAYLISTITEM m_hItem;
	HBITMAP m_hAlbumArt;
} CPDlgProperties_Data;

//
// Forward declarations
//
BOOL CALLBACK DlgProc_Properties(HWND hWnd, UINT uiMessage, WPARAM wParam, LPARAM lParam);
void Properties_OnInit(HWND hWnd, CP_HPLAYLISTITEM hItem);
void Properties_OnUpdateFromMusicBrainz(HWND hWnd, CP_HPLAYLISTITEM hItem);

//
// Show properties dialog for a playlist item
//
void CPDlgProperties_Show(HWND hWndParent, CP_HPLAYLISTITEM hItem)
{
	CPDlgProperties_Data* pData;
	
	if (!hItem)
		return;
		
	// Allocate dialog data
	pData = (CPDlgProperties_Data*)SAFE_MALLOC(sizeof(CPDlgProperties_Data));
	if (!pData)
		return;
	pData->m_hItem = hItem;
	pData->m_hAlbumArt = NULL;
	
	// Show dialog
	DialogBoxParamW(GetModuleHandle(NULL), 
	               MAKEINTRESOURCEW(IDD_PROPERTIES), 
	               hWndParent,
	               (DLGPROC)DlgProc_Properties,
	               (LPARAM)pData);
	
	// Cleanup
	if (pData->m_hAlbumArt)
		DeleteObject(pData->m_hAlbumArt);
	free(pData);
}

//
// Dialog procedure
//
BOOL CALLBACK DlgProc_Properties(HWND hWnd, UINT uiMessage, WPARAM wParam, LPARAM lParam)
{
	CPDlgProperties_Data* pData;
	
	switch (uiMessage)
	{
		case WM_INITDIALOG:
			SetWindowLongPtr(hWnd, DWLP_USER, lParam);
			pData = (CPDlgProperties_Data*)lParam;
			Properties_OnInit(hWnd, pData->m_hItem);
			return TRUE;
			
		case WM_COMMAND:
			pData = (CPDlgProperties_Data*)GetWindowLongPtr(hWnd, DWLP_USER);
			
			switch (LOWORD(wParam))
			{
				case IDOK:
				case IDCANCEL:
					EndDialog(hWnd, LOWORD(wParam));
					return TRUE;
					
				case IDC_PROP_UPDATE_MUSICBRAINZ:
					Properties_OnUpdateFromMusicBrainz(hWnd, pData->m_hItem);
					return TRUE;
			}
			break;
	}
	
	return FALSE;
}

//
// Initialize dialog with track information
//
void Properties_OnInit(HWND hWnd, CP_HPLAYLISTITEM hItem)
{
	char buffer[512];
	const char* pValue;
	int iValue;
	float fValue;
	
	// Basic info
	pValue = CPLI_GetPath(hItem);
	if (pValue)
		SetDlgItemTextA(hWnd, IDC_PROP_FILENAME, pValue);
		
	pValue = CPLI_GetTrackName(hItem);
	if (pValue)
		SetDlgItemTextA(hWnd, IDC_PROP_TITLE, pValue);
		
	pValue = CPLI_GetArtist(hItem);
	if (pValue)
		SetDlgItemTextA(hWnd, IDC_PROP_ARTIST, pValue);
		
	pValue = CPLI_GetAlbum(hItem);
	if (pValue)
		SetDlgItemTextA(hWnd, IDC_PROP_ALBUM, pValue);
		
	pValue = CPLI_GetYear(hItem);
	if (pValue)
		SetDlgItemTextA(hWnd, IDC_PROP_YEAR, pValue);
		
	pValue = CPLI_GetGenre(hItem);
	if (pValue)
		SetDlgItemTextA(hWnd, IDC_PROP_GENRE, pValue);
		
	pValue = CPLI_GetComment(hItem);
	if (pValue)
		SetDlgItemTextA(hWnd, IDC_PROP_COMMENT, pValue);
		
	iValue = CPLI_GetTrackNum(hItem);
	if (iValue > 0)
	{
		snprintf(buffer, sizeof(buffer), "%d", iValue);
		SetDlgItemTextA(hWnd, IDC_PROP_TRACK, buffer);
	}
	
	// Extended metadata
	pValue = CPLI_GetComposer(hItem);
	if (pValue)
		SetDlgItemTextA(hWnd, IDC_PROP_COMPOSER, pValue);
		
	pValue = CPLI_GetAlbumArtist(hItem);
	if (pValue)
		SetDlgItemTextA(hWnd, IDC_PROP_ALBUMARTIST, pValue);
		
	pValue = CPLI_GetGrouping(hItem);
	if (pValue)
		SetDlgItemTextA(hWnd, IDC_PROP_GROUPING, pValue);
		
	iValue = CPLI_GetBPM(hItem);
	if (iValue > 0)
	{
		snprintf(buffer, sizeof(buffer), "%d", iValue);
		SetDlgItemTextA(hWnd, IDC_PROP_BPM, buffer);
	}
	
	iValue = CPLI_GetDiscNumber(hItem);
	if (iValue > 0)
	{
		snprintf(buffer, sizeof(buffer), "%d", iValue);
		SetDlgItemTextA(hWnd, IDC_PROP_DISCNUMBER, buffer);
	}
	
	// Audio properties
	iValue = CPLI_GetBitrate(hItem);
	if (iValue > 0)
	{
		snprintf(buffer, sizeof(buffer), "%d kbps", iValue);
		SetDlgItemTextA(hWnd, IDC_PROP_BITRATE, buffer);
	}
	
	iValue = CPLI_GetSampleRate(hItem);
	if (iValue > 0)
	{
		snprintf(buffer, sizeof(buffer), "%d Hz", iValue);
		SetDlgItemTextA(hWnd, IDC_PROP_SAMPLERATE, buffer);
	}
	
	iValue = CPLI_GetBitDepth(hItem);
	if (iValue > 0)
	{
		snprintf(buffer, sizeof(buffer), "%d-bit", iValue);
		SetDlgItemTextA(hWnd, IDC_PROP_BITDEPTH, buffer);
	}
	
	iValue = CPLI_GetChannels(hItem);
	if (iValue > 0)
	{
		snprintf(buffer, sizeof(buffer), "%d", iValue);
		SetDlgItemTextA(hWnd, IDC_PROP_CHANNELS, buffer);
	}
	
	pValue = CPLI_GetCodec(hItem);
	if (pValue)
		SetDlgItemTextA(hWnd, IDC_PROP_CODEC, pValue);
		
	iValue = CPLI_GetFileSize(hItem);
	if (iValue > 0)
	{
		if (iValue >= 1048576)
			snprintf(buffer, sizeof(buffer), "%.2f MB", iValue / 1048576.0f);
		else if (iValue >= 1024)
			snprintf(buffer, sizeof(buffer), "%.2f KB", iValue / 1024.0f);
		else
			snprintf(buffer, sizeof(buffer), "%d bytes", iValue);
		SetDlgItemTextA(hWnd, IDC_PROP_FILESIZE, buffer);
	}
	
	// ReplayGain
	fValue = CPLI_GetReplayGain_Track_Gain(hItem);
	if (fValue != 0.0f)
	{
		snprintf(buffer, sizeof(buffer), "%.2f dB", fValue);
		SetDlgItemTextA(hWnd, IDC_PROP_REPLAYGAIN_TRACK, buffer);
	}
	
	fValue = CPLI_GetReplayGain_Album_Gain(hItem);
	if (fValue != 0.0f)
	{
		snprintf(buffer, sizeof(buffer), "%.2f dB", fValue);
		SetDlgItemTextA(hWnd, IDC_PROP_REPLAYGAIN_ALBUM, buffer);
	}
	
	// MusicBrainz IDs
	pValue = CPLI_GetMusicBrainz_TrackID(hItem);
	if (pValue)
		SetDlgItemTextA(hWnd, IDC_PROP_MUSICBRAINZ_TRACKID, pValue);
		
	pValue = CPLI_GetMusicBrainz_ReleaseID(hItem);
	if (pValue)
		SetDlgItemTextA(hWnd, IDC_PROP_MUSICBRAINZ_RELEASEID, pValue);
		
	pValue = CPLI_GetMusicBrainz_ArtistID(hItem);
	if (pValue)
		SetDlgItemTextA(hWnd, IDC_PROP_MUSICBRAINZ_ARTISTID, pValue);
	
	// Load and display album art
	pValue = CPLI_GetPath(hItem);
	if (pValue)
	{
		CPDlgProperties_Data* pData = (CPDlgProperties_Data*)GetWindowLongPtr(hWnd, DWLP_USER);
		if (pData)
		{
			unsigned int iWidth = 0, iHeight = 0;
			HWND hArtCtrl = GetDlgItem(hWnd, IDC_PROP_ALBUMART);
			RECT rcArt;
			int targetSize = 200;  // Default size
			
			// Get actual control size to fit album art properly
			if (hArtCtrl && GetClientRect(hArtCtrl, &rcArt))
			{
				targetSize = min(rcArt.right - rcArt.left, rcArt.bottom - rcArt.top);
				if (targetSize < 128) targetSize = 128;  // Minimum size
			}
			
			// Load album art at target size (bypass cache to get correct size)
			pData->m_hAlbumArt = CPTL_LoadAlbumArtBitmap(pValue, targetSize, targetSize, &iWidth, &iHeight);
			
			if (pData->m_hAlbumArt)
			{
				// Set the bitmap to the static control
				SendDlgItemMessage(hWnd, IDC_PROP_ALBUMART, STM_SETIMAGE, 
				                   IMAGE_BITMAP, (LPARAM)pData->m_hAlbumArt);
			}
		}
	}
}

//
// Update metadata from MusicBrainz
//
void Properties_OnUpdateFromMusicBrainz(HWND hWnd, CP_HPLAYLISTITEM hItem)
{
	const char* pcTrackID;
	const char* pcReleaseID;
	
	// Get MusicBrainz IDs
	pcTrackID = CPLI_GetMusicBrainz_TrackID(hItem);
	pcReleaseID = CPLI_GetMusicBrainz_ReleaseID(hItem);
	
	if (!pcTrackID && !pcReleaseID)
	{
		MessageBoxA(hWnd, 
		           "This track does not have any MusicBrainz IDs.\n\n"
		           "To use this feature, the track must first be tagged with MusicBrainz IDs using a tagger like MusicBrainz Picard.",
		           "No MusicBrainz Data",
		           MB_OK | MB_ICONINFORMATION);
		return;
	}
	
	// For now, just open the MusicBrainz page for the recording or release
	STR_OpenMusicBrainzPage(hWnd, pcTrackID, pcReleaseID, NULL);
	
	// TODO: In the future, this could use the MusicBrainz web service API
	// to fetch metadata and automatically update the tags
	MessageBoxA(hWnd,
	           "Automatic metadata update from MusicBrainz is not yet implemented.\n\n"
	           "For now, use MusicBrainz Picard to update your file tags.",
	           "Feature Coming Soon",
	           MB_OK | MB_ICONINFORMATION);
}
