/*
 * CoolPlayer - Blazing fast audio player.
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

#include "stdafx.h"
#include "globals.h"
#include "CPI_DpiScale.h"

// Function pointers for DPI APIs (loaded dynamically for compatibility)
typedef UINT (WINAPI *PFN_GetDpiForSystem)(void);
typedef UINT (WINAPI *PFN_GetDpiForWindow)(HWND);

static PFN_GetDpiForSystem  pfnGetDpiForSystem  = NULL;
static PFN_GetDpiForWindow  pfnGetDpiForWindow  = NULL;
static BOOL s_bApisResolved = FALSE;

static UINT  s_currentDPI = 96;
static float s_scaleFactor = 1.0f;

#define DPI_DEFAULT 96

// Resolve DPI API function pointers (Win10 1607+)
static void DPI_ResolveAPIs(void)
{
	if (s_bApisResolved)
		return;
	s_bApisResolved = TRUE;

	HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
	if (hUser32)
	{
		pfnGetDpiForSystem = (PFN_GetDpiForSystem)(void*)GetProcAddress(hUser32, "GetDpiForSystem");
		pfnGetDpiForWindow = (PFN_GetDpiForWindow)(void*)GetProcAddress(hUser32, "GetDpiForWindow");
	}
}

static void DPI_SetDPI(UINT dpi)
{
	if (dpi == 0)
		dpi = DPI_DEFAULT;
	s_currentDPI = dpi;
	s_scaleFactor = (float)dpi / (float)DPI_DEFAULT;
}

void DPI_Init(void)
{
	DPI_ResolveAPIs();

	if (pfnGetDpiForSystem)
	{
		DPI_SetDPI(pfnGetDpiForSystem());
	}
	else
	{
		// Fallback for older Windows
		HDC hdc = GetDC(NULL);
		if (hdc)
		{
			DPI_SetDPI((UINT)GetDeviceCaps(hdc, LOGPIXELSX));
			ReleaseDC(NULL, hdc);
		}
	}
}

void DPI_UpdateForWindow(HWND hWnd)
{
	DPI_ResolveAPIs();

	if (pfnGetDpiForWindow && hWnd)
	{
		DPI_SetDPI(pfnGetDpiForWindow(hWnd));
	}
}

UINT DPI_GetDPI(void)
{
	return s_currentDPI;
}

float DPI_GetScaleFactor(void)
{
	return s_scaleFactor;
}

int DPI_Scale(int px)
{
	if (s_currentDPI == DPI_DEFAULT)
		return px;
	return (int)((float)px * s_scaleFactor + 0.5f);
}

// Scale a single HBITMAP by the current DPI factor.
// Returns a new bitmap; the original is deleted.
HBITMAP DPI_ScaleBitmap(HBITMAP hOriginal)
{
	if (!hOriginal || s_currentDPI == DPI_DEFAULT)
		return hOriginal;

	BITMAP bm;
	if (!GetObject(hOriginal, sizeof(bm), &bm))
		return hOriginal;

	int newW = DPI_Scale(bm.bmWidth);
	int newH = DPI_Scale(bm.bmHeight);
	if (newW <= 0 || newH <= 0)
		return hOriginal;

	HDC hdcScreen = GetDC(NULL);
	HDC hdcSrc = CreateCompatibleDC(hdcScreen);
	HDC hdcDst = CreateCompatibleDC(hdcScreen);

	HBITMAP hScaled = CreateCompatibleBitmap(hdcScreen, newW, newH);
	if (!hScaled)
	{
		DeleteDC(hdcSrc);
		DeleteDC(hdcDst);
		ReleaseDC(NULL, hdcScreen);
		return hOriginal;
	}

	HBITMAP hOldSrc = (HBITMAP)SelectObject(hdcSrc, hOriginal);
	HBITMAP hOldDst = (HBITMAP)SelectObject(hdcDst, hScaled);

	// Use COLORONCOLOR (nearest-neighbor) to preserve exact transparent
	// color values at bitmap edges. HALFTONE would anti-alias the magenta
	// transparent color creating a pink fringe around the window region.
	// Nearest-neighbor is also the correct mode for pixel-art skin bitmaps.
	SetStretchBltMode(hdcDst, COLORONCOLOR);

	StretchBlt(hdcDst, 0, 0, newW, newH,
	           hdcSrc, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

	SelectObject(hdcSrc, hOldSrc);
	SelectObject(hdcDst, hOldDst);
	DeleteDC(hdcSrc);
	DeleteDC(hdcDst);
	ReleaseDC(NULL, hdcScreen);

	DeleteObject(hOriginal);
	return hScaled;
}

// Scale all skin coordinates
static void DPI_ScaleCoords(void)
{
	if (s_currentDPI == DPI_DEFAULT)
		return;

	for (int i = 0; i < Lastone; i++)
	{
		Skin.Object[i].x  = DPI_Scale(Skin.Object[i].x);
		Skin.Object[i].y  = DPI_Scale(Skin.Object[i].y);
		Skin.Object[i].w  = DPI_Scale(Skin.Object[i].w);
		Skin.Object[i].h  = DPI_Scale(Skin.Object[i].h);
		// maxw is a flag (0 or 1 for horizontal/vertical), not a pixel value — leave it
		Skin.Object[i].x2 = DPI_Scale(Skin.Object[i].x2);
		Skin.Object[i].y2 = DPI_Scale(Skin.Object[i].y2);
		Skin.Object[i].w2 = DPI_Scale(Skin.Object[i].w2);
		Skin.Object[i].h2 = DPI_Scale(Skin.Object[i].h2);
	}
}

// Scale all skin bitmaps
static void DPI_ScaleBitmaps(void)
{
	if (s_currentDPI == DPI_DEFAULT)
		return;

	graphics.bmp_main_up         = DPI_ScaleBitmap(graphics.bmp_main_up);
	graphics.bmp_main_down       = DPI_ScaleBitmap(graphics.bmp_main_down);
	graphics.bmp_main_switch     = DPI_ScaleBitmap(graphics.bmp_main_switch);

	// Track font and time font may be the same handle — scale carefully
	BOOL bSharedTrackFont = (graphics.bmp_main_track_font == graphics.bmp_main_time_font);

	graphics.bmp_main_time_font  = DPI_ScaleBitmap(graphics.bmp_main_time_font);

	if (bSharedTrackFont)
		graphics.bmp_main_track_font = graphics.bmp_main_time_font;
	else
		graphics.bmp_main_track_font = DPI_ScaleBitmap(graphics.bmp_main_track_font);

	graphics.bmp_main_title_font = DPI_ScaleBitmap(graphics.bmp_main_title_font);
}

void DPI_ApplySkinScaling(void)
{
	DPI_ScaleCoords();
	DPI_ScaleBitmaps();
}

void DPI_OnChanged(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	UINT newDPI = HIWORD(wParam);
	if (newDPI == s_currentDPI)
		return;

	DPI_SetDPI(newDPI);

	// Reload the current skin (which resets coords and reloads bitmaps
	// at their original unscaled size). DPI_ApplySkinScaling() is called
	// internally by each skin loader, so no need to call it again here.
	if (options.use_default_skin)
	{
		switch (globals.builtin_skin_variant)
		{
			case BUILTIN_SKIN_EQ:
				main_set_eq_skin();
				break;
			case BUILTIN_SKIN_SHADE:
				main_set_shade_skin();
				break;
			case BUILTIN_SKIN_NORMAL:
			default:
				main_set_default_skin();
				break;
		}
	}
	else
	{
		if (main_skin_open((char*)options.main_skin_file) == FALSE)
			main_set_default_skin();
	}

	// Use the suggested window rect from WM_DPICHANGED
	const RECT* prcNewWindow = (const RECT*)lParam;
	SetWindowPos(hWnd, NULL,
	             prcNewWindow->left, prcNewWindow->top,
	             prcNewWindow->right - prcNewWindow->left,
	             prcNewWindow->bottom - prcNewWindow->top,
	             SWP_NOZORDER | SWP_NOACTIVATE);

	// Update the window region for the scaled bitmap
	{
		HRGN winregion = main_bitmap_to_region(graphics.bmp_main_up, Skin.transparentcolor);
		SetWindowRgn(hWnd, winregion, TRUE);
	}

	// Update title text bitmap at new scale
	main_update_title_text();

	RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE);
}
