/*
 * BriskPlayer - Blazing fast audio player.
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

#ifndef WIN_MODERN_H
#define WIN_MODERN_H

////////////////////////////////////////////////////////////////////////////////
//
// Modern Windows API Wrappers
//
// This module provides modern Windows API functionality with graceful
// fallback for older Windows versions:
//
// 1. WIC Image Loading - PNG/JPEG/GIF support via Windows Imaging Component
// 2. Simple Background Worker - Thread wrapper for async tasks
// 3. Shell Notifications - Balloon notifications via Shell_NotifyIcon
// 4. File Dialogs - Simple wrapper with easier API
//
////////////////////////////////////////////////////////////////////////////////

// Note: This header should be included after stdafx.h or after windows.h
// to ensure all Windows types are defined

#ifndef _WINDOWS_
#include <windows.h>
#endif
#include <shellapi.h>
#include <commdlg.h>

////////////////////////////////////////////////////////////////////////////////
// WIC Image Loading (Windows Vista+)
//
// Loads images using Windows Imaging Component for PNG, JPEG, GIF, BMP support.
// Falls back to LoadImage for BMP on older systems.
////////////////////////////////////////////////////////////////////////////////

// Load image from file path and convert to HBITMAP
// Supports PNG, JPEG, GIF, BMP, TIFF, ICO formats
// Returns NULL on failure. Caller must DeleteObject() the returned HBITMAP
HBITMAP WIC_LoadImageFromFile(const wchar_t* pwcFilePath, int* pWidth, int* pHeight);

// Load image from memory buffer
HBITMAP WIC_LoadImageFromMemory(const void* pData, size_t dataSize, int* pWidth, int* pHeight);

// Load image from an RCDATA resource embedded in the executable
HBITMAP WIC_LoadImageFromResource(UINT uiResourceID, int* pWidth, int* pHeight);

// Check if WIC is available on this system
BOOL WIC_IsAvailable(void);

// Cleanup WIC factory (call on app exit)
void WIC_Cleanup(void);

////////////////////////////////////////////////////////////////////////////////
// Simple Background Worker
//
// Wrapper for thread creation with simpler API
////////////////////////////////////////////////////////////////////////////////

// Opaque handle to a background worker
typedef struct _CP_BackgroundWorker* CP_HWORKER;

// Work callback function type
typedef void (*CP_WorkCallback)(void* pContext);

// Create and start a background worker
CP_HWORKER BackgroundWorker_Start(CP_WorkCallback callback, void* pContext);

// Wait for worker to complete (blocks)
void BackgroundWorker_Wait(CP_HWORKER hWorker);

// Check if worker is still running
BOOL BackgroundWorker_IsRunning(CP_HWORKER hWorker);

// Close worker handle (must call after worker completes)
void BackgroundWorker_Close(CP_HWORKER hWorker);

////////////////////////////////////////////////////////////////////////////////
// Shell Balloon Notifications
//
// Show balloon tips from system tray icons
////////////////////////////////////////////////////////////////////////////////

// Balloon icon types
#define CP_BALLOON_INFO     0x00000001  // NIIF_INFO
#define CP_BALLOON_WARNING  0x00000002  // NIIF_WARNING  
#define CP_BALLOON_ERROR    0x00000003  // NIIF_ERROR
#define CP_BALLOON_NONE     0x00000000  // No icon

// Show a balloon notification from an existing tray icon
// hWnd: Window that owns the tray icon
// uID: Tray icon identifier (same as passed to Shell_NotifyIcon)
// pwcTitle: Balloon title (max 63 chars)
// pwcMessage: Balloon message (max 255 chars)
// dwFlags: CP_BALLOON_* flags
// dwTimeout: Timeout in milliseconds (system may override)
BOOL ShellBalloon_Show(HWND hWnd, UINT uID, 
                       const wchar_t* pwcTitle,
                       const wchar_t* pwcMessage,
                       DWORD dwFlags, DWORD dwTimeout);

// Hide any currently displayed balloon for the tray icon
BOOL ShellBalloon_Hide(HWND hWnd, UINT uID);

////////////////////////////////////////////////////////////////////////////////
// Enhanced File Dialogs
//
// Wrappers around GetOpenFileName/GetSaveFileName with easier API
////////////////////////////////////////////////////////////////////////////////

// File dialog filter structure
typedef struct {
    const wchar_t* pwcDescription;  // e.g., L"Audio Files"
    const wchar_t* pwcPattern;      // e.g., L"*.mp3;*.flac;*.ogg"
} CP_FileDialogFilter;

// File dialog options
typedef enum {
    CP_FD_NONE             = 0,
    CP_FD_MULTISELECT      = 0x0001,  // Allow multiple selection
    CP_FD_MUST_EXIST       = 0x0002,  // File must exist
    CP_FD_PATH_MUST_EXIST  = 0x0004,  // Path must exist
    CP_FD_OVERWRITE_PROMPT = 0x0008,  // Prompt before overwrite
    CP_FD_NO_READONLY      = 0x0010,  // Hide read-only files
} CP_FileDialogOptions;

// Show Open File dialog
// Returns allocated string with selected path (caller must free)
// For multiselect, paths are separated by '|' character
// Returns NULL if cancelled or error
wchar_t* FileDialog_OpenFile(HWND hWndOwner,
                             const wchar_t* pwcTitle,
                             const wchar_t* pwcInitialDir,
                             const CP_FileDialogFilter* pFilters,
                             int filterCount,
                             CP_FileDialogOptions options);

// Show Save File dialog
// Returns allocated string with selected path (caller must free)
// Returns NULL if cancelled or error
wchar_t* FileDialog_SaveFile(HWND hWndOwner,
                             const wchar_t* pwcTitle,
                             const wchar_t* pwcDefaultName,
                             const wchar_t* pwcInitialDir,
                             const CP_FileDialogFilter* pFilters,
                             int filterCount,
                             const wchar_t* pwcDefaultExt,
                             CP_FileDialogOptions options);

// Show folder browser dialog
// Returns allocated string with selected path (caller must free)
// Returns NULL if cancelled
wchar_t* FileDialog_BrowseFolder(HWND hWndOwner, 
                                 const wchar_t* pwcTitle,
                                 const wchar_t* pwcInitialDir);

// Parse multiselect result from FileDialog_OpenFile
// Returns array of paths (NULL terminated), caller must free each and the array
wchar_t** FileDialog_ParseMultiSelect(const wchar_t* pwcResult);

// Free array returned by FileDialog_ParseMultiSelect
void FileDialog_FreeMultiSelect(wchar_t** ppPaths);

// Build filter string from filter array (internal helper, exposed for flexibility)
// Returns allocated string in OPENFILENAME format, caller must free
wchar_t* FileDialog_BuildFilterString(const CP_FileDialogFilter* pFilters, int filterCount);

////////////////////////////////////////////////////////////////////////////////

#endif // WIN_MODERN_H
