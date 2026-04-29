/*
 * BriskPlayer - Blazing fast audio player.
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
#include "CPI_AlbumArtTooltip.h"
#include "CPI_TagLib.h"
#include "globals.h"
#include <windowsx.h>

////////////////////////////////////////////////////////////////////////////////
// Tooltip State
////////////////////////////////////////////////////////////////////////////////

static void CPAAT_TextOutUTF8(HDC hDC, int x, int y, const char* pcUtf8)
{
	wchar_t wcBuffer[512];
	if (MultiByteToWideChar(CP_UTF8, 0, pcUtf8, -1, wcBuffer, 512) > 0)
		TextOutW(hDC, x, y, wcBuffer, (int)wcslen(wcBuffer));
	else
		TextOutA(hDC, x, y, pcUtf8, (int)strlen(pcUtf8));
}

typedef struct _CPs_TooltipState
{
	HWND m_hWnd;
	HBITMAP m_hArtwork;
	CP_HPLAYLISTITEM m_hCurrentItem;
	POINT m_ptPosition;
	BOOL m_bVisible;
	BOOL m_bFading;
	BYTE m_cAlpha;
	
	// Hover tracking
	CP_HPLAYLISTITEM m_hHoverItem;
	POINT m_ptHoverPos;
	DWORD m_dwHoverStartTime;
	UINT_PTR m_uiHoverTimer;
	
	// Text info
	char m_cTitle[256];
	char m_cArtist[256];
	char m_cAlbum[256];
	char m_cInfo[256];         // Duration and file size
	char m_cTechnical1[256];   // Codec and bitrate
	char m_cTechnical2[256];   // Sample rate, bit depth, channels
	char m_cArtists[256];      // Multiple artists info
	
	// Size
	int m_iWidth;
	int m_iHeight;
	
} CPs_TooltipState;

static CPs_TooltipState g_Tooltip = {0};

#define CPC_TOOLTIP_WINDOWCLASS "BriskPlayer_AlbumArtTooltip"
#define CPC_TOOLTIP_TIMER_HOVER 1
#define CPC_TOOLTIP_TIMER_FADE  2

////////////////////////////////////////////////////////////////////////////////
// Forward Declarations
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK CPAAT_WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CPAAT_UpdateContent(CP_HPLAYLISTITEM hItem);
void CPAAT_CalculateSize(void);
void CPAAT_PositionWindow(const POINT* pScreenPos);

////////////////////////////////////////////////////////////////////////////////
// Public API Implementation (C linkage for use from C code)
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

void CPAAT_Initialize(void)
{
	WNDCLASSEX wc = {0};
	
	memset(&g_Tooltip, 0, sizeof(CPs_TooltipState));
	
	// Register window class
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_DROPSHADOW | CS_SAVEBITS;
	wc.lpfnWndProc = CPAAT_WindowProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszClassName = CPC_TOOLTIP_WINDOWCLASS;
	
	RegisterClassEx(&wc);
	
	// Create layered window for alpha blending
	g_Tooltip.m_hWnd = CreateWindowEx(
		WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
		CPC_TOOLTIP_WINDOWCLASS,
		"",
		WS_POPUP,
		0, 0, 100, 100,
		NULL, NULL, GetModuleHandle(NULL), NULL);
		
	if (g_Tooltip.m_hWnd)
	{
		// Set initial alpha to 0 (invisible)
		SetLayeredWindowAttributes(g_Tooltip.m_hWnd, 0, 0, LWA_ALPHA);
	}
	
	windows.wnd_albumart_tooltip = g_Tooltip.m_hWnd;
}

void CPAAT_Cleanup(void)
{
	CPAAT_Hide();
	
	if (g_Tooltip.m_uiHoverTimer)
	{
		KillTimer(NULL, g_Tooltip.m_uiHoverTimer);
		g_Tooltip.m_uiHoverTimer = 0;
	}
	
	// Don't delete m_hArtwork - it's owned by the album art cache
	g_Tooltip.m_hArtwork = NULL;
	
	if (g_Tooltip.m_hWnd)
	{
		DestroyWindow(g_Tooltip.m_hWnd);
		g_Tooltip.m_hWnd = NULL;
		windows.wnd_albumart_tooltip = NULL;
	}
	
	UnregisterClass(CPC_TOOLTIP_WINDOWCLASS, GetModuleHandle(NULL));
	memset(&g_Tooltip, 0, sizeof(CPs_TooltipState));
}

#ifdef __cplusplus
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Internal Helper Functions (C++ linkage)
////////////////////////////////////////////////////////////////////////////////

void CPAAT_UpdateContent(CP_HPLAYLISTITEM hItem)
{
	const char* pcPath;
	const char* pcTitle;
	const char* pcArtist;
	const char* pcAlbum;
	const char* pcLength;
	const char* pcFeaturedArtist;
	const char* pcRemixer;
	unsigned int iBitrate;
	unsigned int iSampleRate;
	unsigned short iBitDepth;
	unsigned char cChannels;
	const char* pcCodec;
	const char* pcBitrateMode;
	unsigned int iFileSize;
	unsigned int iWidth = 0;
	unsigned int iHeight = 0;
	
	if (!hItem)
		return;
	
	g_Tooltip.m_hCurrentItem = hItem;
	
	// Get item information
	pcTitle = CPLI_GetTrackName(hItem);
	pcArtist = CPLI_GetArtist(hItem);
	pcAlbum = CPLI_GetAlbum(hItem);
	pcLength = CPLI_GetTrackLength_AsText(hItem);
	pcPath = CPLI_GetPath(hItem);
	
	// Get multiple artists info
	pcFeaturedArtist = CPLI_GetFeaturedArtist(hItem);
	pcRemixer = CPLI_GetRemixer(hItem);
	
	// Get audio properties
	iBitrate = CPLI_GetBitrate(hItem);
	iSampleRate = CPLI_GetSampleRate(hItem);
	iBitDepth = CPLI_GetBitDepth(hItem);
	cChannels = CPLI_GetChannels(hItem);
	pcCodec = CPLI_GetCodec(hItem);
	pcBitrateMode = CPLI_GetBitrateMode(hItem);
	iFileSize = CPLI_GetFileSize(hItem);
	
	// Copy text info
	strncpy_s(g_Tooltip.m_cTitle, sizeof(g_Tooltip.m_cTitle), 
	          pcTitle ? pcTitle : "Unknown Title", _TRUNCATE);
	strncpy_s(g_Tooltip.m_cArtist, sizeof(g_Tooltip.m_cArtist),
	          pcArtist ? pcArtist : "Unknown Artist", _TRUNCATE);
	strncpy_s(g_Tooltip.m_cAlbum, sizeof(g_Tooltip.m_cAlbum),
	          pcAlbum ? pcAlbum : "Unknown Album", _TRUNCATE);
	          
	// Format info line with length and file size
	char sizeStr[64] = "";
	if (iFileSize > 0)
	{
		if (iFileSize >= 1024 * 1024)
			sprintf_s(sizeStr, sizeof(sizeStr), " - %.2f MB", iFileSize / (1024.0 * 1024.0));
		else if (iFileSize >= 1024)
			sprintf_s(sizeStr, sizeof(sizeStr), " - %.2f KB", iFileSize / 1024.0);
		else
			sprintf_s(sizeStr, sizeof(sizeStr), " - %u bytes", iFileSize);
	}
	
	sprintf_s(g_Tooltip.m_cInfo, sizeof(g_Tooltip.m_cInfo),
	          "%s%s", pcLength ? pcLength : "", sizeStr);
	
	// Format technical info - split into two lines for better readability
	g_Tooltip.m_cTechnical1[0] = '\0';
	g_Tooltip.m_cTechnical2[0] = '\0';
	
	if (iBitrate > 0 || iSampleRate > 0)
	{
		char tempBuf[256];
		
		// Line 1: Codec and bitrate
		if (pcCodec && pcCodec[0])
		{
			strcpy_s(g_Tooltip.m_cTechnical1, sizeof(g_Tooltip.m_cTechnical1), pcCodec);
			strcat_s(g_Tooltip.m_cTechnical1, sizeof(g_Tooltip.m_cTechnical1), " ");
		}
		
		// Add bitrate mode if not CBR
		if (pcBitrateMode && pcBitrateMode[0] && strcmp(pcBitrateMode, "CBR") != 0)
		{
			strcat_s(g_Tooltip.m_cTechnical1, sizeof(g_Tooltip.m_cTechnical1), pcBitrateMode);
			strcat_s(g_Tooltip.m_cTechnical1, sizeof(g_Tooltip.m_cTechnical1), " ");
		}
		
		// Add bitrate
		if (iBitrate > 0)
		{
			sprintf_s(tempBuf, sizeof(tempBuf), "%u kbps", iBitrate);
			strcat_s(g_Tooltip.m_cTechnical1, sizeof(g_Tooltip.m_cTechnical1), tempBuf);
		}
		
		// Line 2: Sample rate, bit depth, and channels
		if (iSampleRate > 0)
		{
			sprintf_s(tempBuf, sizeof(tempBuf), "%u Hz", iSampleRate);
			strcpy_s(g_Tooltip.m_cTechnical2, sizeof(g_Tooltip.m_cTechnical2), tempBuf);
		}
		
		// Add bit depth
		if (iBitDepth > 0)
		{
			if (g_Tooltip.m_cTechnical2[0])
				strcat_s(g_Tooltip.m_cTechnical2, sizeof(g_Tooltip.m_cTechnical2), " - ");
			sprintf_s(tempBuf, sizeof(tempBuf), "%u-bit", iBitDepth);
			strcat_s(g_Tooltip.m_cTechnical2, sizeof(g_Tooltip.m_cTechnical2), tempBuf);
		}
		
		// Add channels
		if (cChannels > 0)
		{
			if (g_Tooltip.m_cTechnical2[0])
				strcat_s(g_Tooltip.m_cTechnical2, sizeof(g_Tooltip.m_cTechnical2), " - ");
			
			if (cChannels == 1)
				strcat_s(g_Tooltip.m_cTechnical2, sizeof(g_Tooltip.m_cTechnical2), "Mono");
			else if (cChannels == 2)
				strcat_s(g_Tooltip.m_cTechnical2, sizeof(g_Tooltip.m_cTechnical2), "Stereo");
			else if (cChannels == 6)
				strcat_s(g_Tooltip.m_cTechnical2, sizeof(g_Tooltip.m_cTechnical2), "5.1");
			else
			{
				sprintf_s(tempBuf, sizeof(tempBuf), "%dch", cChannels);
				strcat_s(g_Tooltip.m_cTechnical2, sizeof(g_Tooltip.m_cTechnical2), tempBuf);
			}
		}
	}
	
	// Format multiple artists line
	g_Tooltip.m_cArtists[0] = '\0';
	if (pcFeaturedArtist || pcRemixer)
	{
		if (pcFeaturedArtist)
		{
			sprintf_s(g_Tooltip.m_cArtists, sizeof(g_Tooltip.m_cArtists),
			          "feat. %s", pcFeaturedArtist);
		}
		if (pcRemixer)
		{
			if (g_Tooltip.m_cArtists[0])
				strcat_s(g_Tooltip.m_cArtists, sizeof(g_Tooltip.m_cArtists), " - ");
			strcat_s(g_Tooltip.m_cArtists, sizeof(g_Tooltip.m_cArtists), "Remix: ");
			strcat_s(g_Tooltip.m_cArtists, sizeof(g_Tooltip.m_cArtists), pcRemixer);
		}
	}
	
	// Load album art from cache (don't delete old one - it's cache-owned)
	g_Tooltip.m_hArtwork = NULL;
	
	if (pcPath)
	{
		g_Tooltip.m_hArtwork = CPTL_GetAlbumArtBitmap(pcPath, CPAAT_ARTWORK_SIZE, CPAAT_ARTWORK_SIZE, &iWidth, &iHeight);
	}
	
	// Calculate window size
	CPAAT_CalculateSize();
}

void CPAAT_CalculateSize(void)
{
	HDC hDC;
	SIZE szText;
	int iContentWidth = 0;
	int iContentHeight = 0;
	
	// Calculate text dimensions
	hDC = GetDC(NULL);
	if (hDC)
	{
		HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
		HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);
		
		// Create bold font for title measurement
		LOGFONT lf;
		GetObject(hFont, sizeof(lf), &lf);
		lf.lfWeight = FW_BOLD;
		HFONT hBoldFont = CreateFontIndirect(&lf);
		
		// Get max text width - measure title with bold font
		SelectObject(hDC, hBoldFont);
		GetTextExtentPoint32(hDC, g_Tooltip.m_cTitle, (int)strlen(g_Tooltip.m_cTitle), &szText);
		iContentWidth = szText.cx;
		SelectObject(hDC, hFont);
		
		GetTextExtentPoint32(hDC, g_Tooltip.m_cArtist, (int)strlen(g_Tooltip.m_cArtist), &szText);
		if (szText.cx > iContentWidth)
			iContentWidth = szText.cx;
			
		GetTextExtentPoint32(hDC, g_Tooltip.m_cAlbum, (int)strlen(g_Tooltip.m_cAlbum), &szText);
		if (szText.cx > iContentWidth)
			iContentWidth = szText.cx;
		
		GetTextExtentPoint32(hDC, g_Tooltip.m_cInfo, (int)strlen(g_Tooltip.m_cInfo), &szText);
		if (szText.cx > iContentWidth)
			iContentWidth = szText.cx;
		
		GetTextExtentPoint32(hDC, g_Tooltip.m_cTechnical1, (int)strlen(g_Tooltip.m_cTechnical1), &szText);
		if (szText.cx > iContentWidth)
			iContentWidth = szText.cx;
		
		GetTextExtentPoint32(hDC, g_Tooltip.m_cTechnical2, (int)strlen(g_Tooltip.m_cTechnical2), &szText);
		if (szText.cx > iContentWidth)
			iContentWidth = szText.cx;
		
		GetTextExtentPoint32(hDC, g_Tooltip.m_cArtists, (int)strlen(g_Tooltip.m_cArtists), &szText);
		if (szText.cx > iContentWidth)
			iContentWidth = szText.cx;
		
		// Calculate height (up to 7 lines of text with spacing)
		// First line (title) is 20px, rest are 18px each
		int iLineCount = 4;  // Title, Artist, Album, Info
		if (g_Tooltip.m_cTechnical1[0])
			iLineCount++;
		if (g_Tooltip.m_cTechnical2[0])
			iLineCount++;
		if (g_Tooltip.m_cArtists[0])
			iLineCount++;
		
		// First line is 20px, rest are 18px each
		iContentHeight = 20 + (iLineCount - 1) * 18;
		
		SelectObject(hDC, hOldFont);
		DeleteObject(hBoldFont);
		ReleaseDC(NULL, hDC);
	}
	
	// Limit text width
	if (iContentWidth > CPAAT_TEXT_WIDTH)
		iContentWidth = CPAAT_TEXT_WIDTH;
	
	// Calculate total window size
	if (g_Tooltip.m_hArtwork)
	{
		// With album art
		g_Tooltip.m_iWidth = CPAAT_PADDING + CPAAT_ARTWORK_SIZE + CPAAT_PADDING + iContentWidth + CPAAT_PADDING;
		g_Tooltip.m_iHeight = CPAAT_PADDING + (CPAAT_ARTWORK_SIZE > iContentHeight ? CPAAT_ARTWORK_SIZE : iContentHeight) + CPAAT_PADDING;
	}
	else
	{
		// Text only
		g_Tooltip.m_iWidth = CPAAT_PADDING + iContentWidth + CPAAT_PADDING;
		g_Tooltip.m_iHeight = CPAAT_PADDING + iContentHeight + CPAAT_PADDING;
	}
}

void CPAAT_PositionWindow(const POINT* pScreenPos)
{
	RECT rMonitor;
	HMONITOR hMonitor;
	MONITORINFO mi = {0};
	int iX, iY;
	
	if (!pScreenPos)
		return;
		
	// Get monitor dimensions
	hMonitor = MonitorFromPoint(*pScreenPos, MONITOR_DEFAULTTONEAREST);
	mi.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(hMonitor, &mi);
	rMonitor = mi.rcWork;
	
	// Position tooltip near cursor, avoiding screen edges
	iX = pScreenPos->x + 16;  // Offset from cursor
	iY = pScreenPos->y + 16;
	
	// Adjust if too close to right edge
	if (iX + g_Tooltip.m_iWidth > rMonitor.right)
		iX = pScreenPos->x - g_Tooltip.m_iWidth - 16;
		
	// Adjust if too close to bottom edge
	if (iY + g_Tooltip.m_iHeight > rMonitor.bottom)
		iY = pScreenPos->y - g_Tooltip.m_iHeight - 16;
	
	// Ensure within monitor bounds
	if (iX < rMonitor.left)
		iX = rMonitor.left;
	if (iY < rMonitor.top)
		iY = rMonitor.top;
		
	g_Tooltip.m_ptPosition.x = iX;
	g_Tooltip.m_ptPosition.y = iY;
	
	SetWindowPos(g_Tooltip.m_hWnd, HWND_TOPMOST,
	             iX, iY, g_Tooltip.m_iWidth, g_Tooltip.m_iHeight,
	             SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

////////////////////////////////////////////////////////////////////////////////
// Public API (Continued - C linkage)
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

void CPAAT_ShowForItem(CP_HPLAYLISTITEM hItem, const POINT* pScreenPos)
{
	if (!g_Tooltip.m_hWnd || !hItem)
		return;
	
	// Always hide first for clean state
	ShowWindow(g_Tooltip.m_hWnd, SW_HIDE);
	g_Tooltip.m_bVisible = FALSE;
	
	// Update content (loads album art)
	CPAAT_UpdateContent(hItem);
	
	// Position window
	CPAAT_PositionWindow(pScreenPos);
	
	// Show window
	g_Tooltip.m_bVisible = TRUE;
	g_Tooltip.m_cAlpha = 255;
	SetLayeredWindowAttributes(g_Tooltip.m_hWnd, 0, g_Tooltip.m_cAlpha, LWA_ALPHA);
	ShowWindow(g_Tooltip.m_hWnd, SW_SHOWNOACTIVATE);
	UpdateWindow(g_Tooltip.m_hWnd);
}

void CPAAT_Hide(void)
{
	if (!g_Tooltip.m_bVisible)
		return;
		
	g_Tooltip.m_bVisible = FALSE;
	ShowWindow(g_Tooltip.m_hWnd, SW_HIDE);
	
	// Don't delete m_hArtwork - it's owned by the album art cache
	// Just clear our reference
	g_Tooltip.m_hArtwork = NULL;
	
	g_Tooltip.m_hCurrentItem = NULL;
	g_Tooltip.m_hHoverItem = NULL;
}

void CPAAT_UpdatePosition(const POINT* pScreenPos)
{
	if (!g_Tooltip.m_bVisible || !pScreenPos)
		return;
		
	CPAAT_PositionWindow(pScreenPos);
}

BOOL CPAAT_IsVisible(void)
{
	return g_Tooltip.m_bVisible;
}

////////////////////////////////////////////////////////////////////////////////
// Hover Tracking
////////////////////////////////////////////////////////////////////////////////

void CALLBACK CPAAT_HoverTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	(void)hwnd;     // Unused parameters
	(void)uMsg;
	(void)idEvent;
	(void)dwTime;
	
	if (g_Tooltip.m_hHoverItem)
	{
		// Check if enough time has elapsed
		if (GetTickCount() - g_Tooltip.m_dwHoverStartTime >= CPAAT_HOVER_DELAY_MS)
		{
			CPAAT_ShowForItem(g_Tooltip.m_hHoverItem, &g_Tooltip.m_ptHoverPos);
			CPAAT_CancelHover();
		}
	}
}

void CPAAT_TrackItemHover(CP_HPLAYLISTITEM hItem, const POINT* pScreenPos)
{
	// If different item, restart tracking
	if (g_Tooltip.m_hHoverItem != hItem)
	{
		g_Tooltip.m_hHoverItem = hItem;
		g_Tooltip.m_dwHoverStartTime = GetTickCount();
		
		if (pScreenPos)
			g_Tooltip.m_ptHoverPos = *pScreenPos;
			
		// Start timer if not already running
		if (!g_Tooltip.m_uiHoverTimer)
		{
			g_Tooltip.m_uiHoverTimer = SetTimer(NULL, 0, 100, CPAAT_HoverTimerProc);
		}
	}
	else if (pScreenPos)
	{
		// Update position
		g_Tooltip.m_ptHoverPos = *pScreenPos;
	}
}

void CPAAT_CancelHover(void)
{
	if (g_Tooltip.m_uiHoverTimer)
	{
		KillTimer(NULL, g_Tooltip.m_uiHoverTimer);
		g_Tooltip.m_uiHoverTimer = 0;
	}
	
	g_Tooltip.m_hHoverItem = NULL;
	g_Tooltip.m_dwHoverStartTime = 0;
}

#ifdef __cplusplus
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Window Procedure
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK CPAAT_WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hDC = BeginPaint(hWnd, &ps);
			
			if (hDC)
			{
				RECT rClient;
				int iX, iY;
				HFONT hFont, hOldFont;
				
				GetClientRect(hWnd, &rClient);
				
				// Draw background
				FillRect(hDC, &rClient, (HBRUSH)GetStockObject(WHITE_BRUSH));
				
				// Draw border
				HPEN hPen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
				HPEN hOldPen = (HPEN)SelectObject(hDC, hPen);
				Rectangle(hDC, 0, 0, rClient.right, rClient.bottom);
				SelectObject(hDC, hOldPen);
				DeleteObject(hPen);
				
				iX = CPAAT_PADDING;
				iY = CPAAT_PADDING;
				
				// Draw album art if available
				if (g_Tooltip.m_hArtwork)
				{
					HDC hMemDC = CreateCompatibleDC(hDC);
					if (hMemDC)
					{
						BITMAP bm;
						HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, g_Tooltip.m_hArtwork);
						GetObject(g_Tooltip.m_hArtwork, sizeof(bm), &bm);
						
						// Use StretchBlt for better display
						SetStretchBltMode(hDC, HALFTONE);
						SetBrushOrgEx(hDC, 0, 0, NULL);
						StretchBlt(hDC, iX, iY, CPAAT_ARTWORK_SIZE, CPAAT_ARTWORK_SIZE,
						          hMemDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
						
						SelectObject(hMemDC, hOldBmp);
						DeleteDC(hMemDC);
					}
					
					// Adjust text position
					iX += CPAAT_ARTWORK_SIZE + CPAAT_PADDING;
				}
				
				// Draw text
				SetBkMode(hDC, TRANSPARENT);
				hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
				hOldFont = (HFONT)SelectObject(hDC, hFont);
				
				// Title (bold)
				LOGFONT lf;
				GetObject(hFont, sizeof(lf), &lf);
				lf.lfWeight = FW_BOLD;
				HFONT hBoldFont = CreateFontIndirect(&lf);
				SelectObject(hDC, hBoldFont);
				SetTextColor(hDC, RGB(0, 0, 0));  // Black for title
				
				CPAAT_TextOutUTF8(hDC, iX, iY, g_Tooltip.m_cTitle);
				iY += 20;
				
				// Normal font for rest
				SelectObject(hDC, hFont);
				SetTextColor(hDC, RGB(0, 0, 0));  // Black for artist
				CPAAT_TextOutUTF8(hDC, iX, iY, g_Tooltip.m_cArtist);
				iY += 18;
				SetTextColor(hDC, RGB(0, 0, 0));  // Black for album
				CPAAT_TextOutUTF8(hDC, iX, iY, g_Tooltip.m_cAlbum);
				iY += 18;
				
				// Multiple artists info (if available)
				if (g_Tooltip.m_cArtists[0])
				{
					SetTextColor(hDC, RGB(80, 80, 150));  // Blue-ish for featured artists
					CPAAT_TextOutUTF8(hDC, iX, iY, g_Tooltip.m_cArtists);
					iY += 18;
				}
				
				// Duration and file size
				SetTextColor(hDC, RGB(100, 100, 100));
				CPAAT_TextOutUTF8(hDC, iX, iY, g_Tooltip.m_cInfo);
				iY += 18;
				
				// Technical info line 1 (codec and bitrate)
				if (g_Tooltip.m_cTechnical1[0])
				{
					SetTextColor(hDC, RGB(120, 120, 120));
					CPAAT_TextOutUTF8(hDC, iX, iY, g_Tooltip.m_cTechnical1);
					iY += 18;
				}
				
				// Technical info line 2 (sample rate, bit depth, channels)
				if (g_Tooltip.m_cTechnical2[0])
				{
					SetTextColor(hDC, RGB(120, 120, 120));
					CPAAT_TextOutUTF8(hDC, iX, iY, g_Tooltip.m_cTechnical2);
					iY += 18;
				}
				
				SelectObject(hDC, hOldFont);
				DeleteObject(hBoldFont);
			}
			
			EndPaint(hWnd, &ps);
			return 0;
		}
		
		case WM_ERASEBKGND:
			return 1;  // We handle erasing in WM_PAINT
			
		default:
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
}
