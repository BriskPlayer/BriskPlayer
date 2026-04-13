/*
 * BriskPlayer - Blazing fast audio player.
 * Copyright (C) 2025 Zach Bacon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Windows taskbar thumbnail toolbar integration (ITaskbarList3).
 * Provides prev/play-pause/next transport buttons in the taskbar preview.
 */
#define COBJMACROS
#include "stdafx.h"
#include <shobjidl.h>
#include "CPI_TaskbarIntegration.h"
#include "globals.h"
#include "resource.h"

static ITaskbarList3* g_pTaskbarList = NULL;
static UINT g_uTaskbarCreatedMsg = 0;
static HICON g_hIconPrev = NULL;
static HICON g_hIconPlay = NULL;
static HICON g_hIconPause = NULL;
static HICON g_hIconNext = NULL;
static BOOL g_bButtonsAdded = FALSE;

enum {
    TBICON_PREV = 0,
    TBICON_PLAY,
    TBICON_PAUSE,
    TBICON_NEXT
};

//
// Draw a filled triangle into a 32-bit ARGB pixel buffer using barycentric test
//
static void FillTriangle(DWORD* pixels, int stride,
                         int x0, int y0, int x1, int y1, int x2, int y2,
                         int clipW, int clipH, DWORD color)
{
    // Bounding box
    int minX = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    int maxX = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int minY = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int maxY = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);

    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= clipW) maxX = clipW - 1;
    if (maxY >= clipH) maxY = clipH - 1;

    for (int y = minY; y <= maxY; y++)
    {
        for (int x = minX; x <= maxX; x++)
        {
            int d0 = (x - x1) * (y0 - y1) - (x0 - x1) * (y - y1);
            int d1 = (x - x2) * (y1 - y2) - (x1 - x2) * (y - y2);
            int d2 = (x - x0) * (y2 - y0) - (x2 - x0) * (y - y0);
            BOOL has_neg = (d0 < 0) || (d1 < 0) || (d2 < 0);
            BOOL has_pos = (d0 > 0) || (d1 > 0) || (d2 > 0);
            if (!(has_neg && has_pos))
                pixels[y * stride + x] = color;
        }
    }
}

//
// Fill a rectangle in a 32-bit ARGB pixel buffer
//
static void FillBox(DWORD* pixels, int stride,
                    int left, int top, int right, int bottom,
                    int clipW, int clipH, DWORD color)
{
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > clipW) right = clipW;
    if (bottom > clipH) bottom = clipH;

    for (int y = top; y < bottom; y++)
        for (int x = left; x < right; x++)
            pixels[y * stride + x] = color;
}

//
// Create a 16x16 32-bit ARGB icon for a media transport button
//
static HICON CreateMediaIcon(int iconType)
{
    const int size = 16;
    const DWORD white = 0xFFFFFFFF;

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = -size; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    DWORD* pvBits = NULL;
    HDC hdcScreen = GetDC(NULL);
    HBITMAP hbmColor = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS,
                                        (void**)&pvBits, NULL, 0);
    if (!hbmColor || !pvBits)
    {
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }

    ZeroMemory(pvBits, (size_t)size * size * sizeof(DWORD));

    const int m = 2;               // margin
    const int top = m;
    const int bot = size - m - 1;  // 13
    const int mid = size / 2;      // 8

    switch (iconType)
    {
        case TBICON_PREV:
            // |◀  bar on left + left-pointing triangle
            FillBox(pvBits, size, m, top, m + 2, bot + 1, size, size, white);
            FillTriangle(pvBits, size,
                         size - m - 1, top,
                         size - m - 1, bot,
                         m + 3, mid,
                         size, size, white);
            break;

        case TBICON_PLAY:
            // ▶  right-pointing triangle
            FillTriangle(pvBits, size,
                         m + 1, top,
                         m + 1, bot,
                         size - m - 1, mid,
                         size, size, white);
            break;

        case TBICON_PAUSE:
            // ||  two vertical bars
            FillBox(pvBits, size, m + 1, top, m + 5, bot + 1, size, size, white);
            FillBox(pvBits, size, size - m - 5, top, size - m - 1, bot + 1, size, size, white);
            break;

        case TBICON_NEXT:
            // ▶|  right-pointing triangle + bar on right
            FillTriangle(pvBits, size,
                         m, top,
                         m, bot,
                         size - m - 3, mid,
                         size, size, white);
            FillBox(pvBits, size, size - m - 2, top, size - m, bot + 1, size, size, white);
            break;
    }

    HBITMAP hbmMask = CreateBitmap(size, size, 1, 1, NULL);

    ICONINFO ii;
    ii.fIcon = TRUE;
    ii.xHotspot = 0;
    ii.yHotspot = 0;
    ii.hbmMask = hbmMask;
    ii.hbmColor = hbmColor;
    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hbmColor);
    DeleteObject(hbmMask);
    ReleaseDC(NULL, hdcScreen);

    return hIcon;
}

//
// (Re-)create the ITaskbarList3 interface and add thumbnail toolbar buttons
//
static void CPI_Taskbar_Create(HWND hWnd)
{
    if (g_pTaskbarList)
    {
        ITaskbarList3_Release(g_pTaskbarList);
        g_pTaskbarList = NULL;
    }
    g_bButtonsAdded = FALSE;

    HRESULT hr = CoCreateInstance(
        &CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER,
        &IID_ITaskbarList3, (void**)&g_pTaskbarList);
    if (FAILED(hr) || !g_pTaskbarList)
        return;

    hr = ITaskbarList3_HrInit(g_pTaskbarList);
    if (FAILED(hr))
    {
        ITaskbarList3_Release(g_pTaskbarList);
        g_pTaskbarList = NULL;
        return;
    }

    BOOL bPlaying = (globals.m_enPlayerState == cppsPlaying);

    THUMBBUTTON buttons[3];
    ZeroMemory(buttons, sizeof(buttons));

    // Previous
    buttons[0].dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
    buttons[0].iId = ID_PREVIOUS;
    buttons[0].hIcon = g_hIconPrev;
    wcscpy_s(buttons[0].szTip, _countof(buttons[0].szTip), L"Previous");
    buttons[0].dwFlags = THBF_ENABLED;

    // Play / Pause
    buttons[1].dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
    buttons[1].iId = ID_PAUSE;
    buttons[1].hIcon = bPlaying ? g_hIconPause : g_hIconPlay;
    wcscpy_s(buttons[1].szTip, _countof(buttons[1].szTip),
             bPlaying ? L"Pause" : L"Play");
    buttons[1].dwFlags = THBF_ENABLED;

    // Next
    buttons[2].dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
    buttons[2].iId = ID_NEXT;
    buttons[2].hIcon = g_hIconNext;
    wcscpy_s(buttons[2].szTip, _countof(buttons[2].szTip), L"Next");
    buttons[2].dwFlags = THBF_ENABLED;

    hr = ITaskbarList3_ThumbBarAddButtons(g_pTaskbarList, hWnd, 3, buttons);
    if (SUCCEEDED(hr))
        g_bButtonsAdded = TRUE;
}

//
// Public API
//

void CPI_Taskbar_Init(void)
{
    g_uTaskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarButtonCreated");

    g_hIconPrev  = CreateMediaIcon(TBICON_PREV);
    g_hIconPlay  = CreateMediaIcon(TBICON_PLAY);
    g_hIconPause = CreateMediaIcon(TBICON_PAUSE);
    g_hIconNext  = CreateMediaIcon(TBICON_NEXT);
}

void CPI_Taskbar_Uninit(void)
{
    if (g_pTaskbarList)
    {
        ITaskbarList3_Release(g_pTaskbarList);
        g_pTaskbarList = NULL;
    }

    if (g_hIconPrev)  { DestroyIcon(g_hIconPrev);  g_hIconPrev  = NULL; }
    if (g_hIconPlay)  { DestroyIcon(g_hIconPlay);  g_hIconPlay  = NULL; }
    if (g_hIconPause) { DestroyIcon(g_hIconPause); g_hIconPause = NULL; }
    if (g_hIconNext)  { DestroyIcon(g_hIconNext);  g_hIconNext  = NULL; }

    g_bButtonsAdded = FALSE;
}

void CPI_Taskbar_UpdateButtons(void)
{
    if (!g_pTaskbarList || !g_bButtonsAdded || !windows.wnd_main)
        return;

    BOOL bPlaying = (globals.m_enPlayerState == cppsPlaying);
    BOOL bActive  = (globals.m_enPlayerState == cppsPlaying
                  || globals.m_enPlayerState == cppsPaused);

    THUMBBUTTON buttons[3];
    ZeroMemory(buttons, sizeof(buttons));

    // Previous
    buttons[0].dwMask  = THB_ICON | THB_TOOLTIP | THB_FLAGS;
    buttons[0].iId     = ID_PREVIOUS;
    buttons[0].hIcon   = g_hIconPrev;
    wcscpy_s(buttons[0].szTip, _countof(buttons[0].szTip), L"Previous");
    buttons[0].dwFlags = bActive ? THBF_ENABLED : THBF_DISABLED;

    // Play / Pause
    buttons[1].dwMask  = THB_ICON | THB_TOOLTIP | THB_FLAGS;
    buttons[1].iId     = ID_PAUSE;
    buttons[1].hIcon   = bPlaying ? g_hIconPause : g_hIconPlay;
    wcscpy_s(buttons[1].szTip, _countof(buttons[1].szTip),
             bPlaying ? L"Pause" : L"Play");
    buttons[1].dwFlags = THBF_ENABLED;

    // Next
    buttons[2].dwMask  = THB_ICON | THB_TOOLTIP | THB_FLAGS;
    buttons[2].iId     = ID_NEXT;
    buttons[2].hIcon   = g_hIconNext;
    wcscpy_s(buttons[2].szTip, _countof(buttons[2].szTip), L"Next");
    buttons[2].dwFlags = bActive ? THBF_ENABLED : THBF_DISABLED;

    ITaskbarList3_ThumbBarUpdateButtons(g_pTaskbarList, windows.wnd_main,
                                        3, buttons);
}

BOOL CPI_Taskbar_HandleMessage(HWND hWnd, UINT message)
{
    if (g_uTaskbarCreatedMsg != 0 && message == g_uTaskbarCreatedMsg)
    {
        CPI_Taskbar_Create(hWnd);
        return TRUE;
    }
    return FALSE;
}
