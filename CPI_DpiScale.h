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

#ifndef CPI_DPISCALE_H
#define CPI_DPISCALE_H

#include <windows.h>

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

// Initialize DPI state (call before window creation, uses system DPI)
void DPI_Init(void);

// Update DPI from a specific window
void DPI_UpdateForWindow(HWND hWnd);

// Get current DPI value
UINT DPI_GetDPI(void);

// Get current scale factor (1.0f at 96 DPI)
float DPI_GetScaleFactor(void);

// Scale a pixel value by current DPI factor
int DPI_Scale(int px);

// Apply DPI scaling to all skin coordinates and bitmaps.
// Call this after every skin load (main_set_default_skin, main_set_eq_skin,
// main_set_shade_skin, main_skin_open).
void DPI_ApplySkinScaling(void);

// Handle WM_DPICHANGED: update scale factor and resize window
void DPI_OnChanged(HWND hWnd, WPARAM wParam, LPARAM lParam);

// Scale an HBITMAP in-place for DPI. Returns new (scaled) bitmap,
// deletes the original. No-op at 96 DPI.
HBITMAP DPI_ScaleBitmap(HBITMAP hOriginal);

#endif
