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

#ifndef CP_UNICODE_H
#define CP_UNICODE_H

////////////////////////////////////////////////////////////////////////////////
//
// Unicode Handling Utilities
// Provides consistent UTF-8/UTF-16 conversion and wide string helpers
//
// The codebase uses:
// - ANSI (char*) strings internally for translations via gettext (UTF-8)
// - Wide (wchar_t*) strings for file paths and Windows API calls
//
// This module provides conversion utilities and helper macros to
// ensure consistent handling across the application.
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <wchar.h>
#include <malloc.h>  // for _alloca
#include "CP_Constants.h"

// Ensure BOOL type is defined
#ifndef BOOL
typedef int BOOL;
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
// Unicode Conversion Functions
////////////////////////////////////////////////////////////////////////////////

/**
 * Convert UTF-8 string to UTF-16 (wide string).
 * 
 * @param utf8      Source UTF-8 string
 * @param wide      Destination buffer for wide string
 * @param wideLen   Size of destination buffer in wchar_t units
 * @return          Number of characters written, or 0 on failure
 */
inline int CPU_Utf8ToWide(const char* utf8, wchar_t* wide, int wideLen)
{
    if (!utf8 || !wide || wideLen <= 0) return 0;
    return MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, wideLen);
}

/**
 * Convert UTF-16 (wide string) to UTF-8.
 * 
 * @param wide      Source wide string
 * @param utf8      Destination buffer for UTF-8 string
 * @param utf8Len   Size of destination buffer in bytes
 * @return          Number of bytes written, or 0 on failure
 */
inline int CPU_WideToUtf8(const wchar_t* wide, char* utf8, int utf8Len)
{
    if (!wide || !utf8 || utf8Len <= 0) return 0;
    return WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, utf8Len, NULL, NULL);
}

/**
 * Convert ANSI string to UTF-16 (using system code page).
 * 
 * @param ansi      Source ANSI string
 * @param wide      Destination buffer for wide string
 * @param wideLen   Size of destination buffer in wchar_t units
 * @return          Number of characters written, or 0 on failure
 */
inline int CPU_AnsiToWide(const char* ansi, wchar_t* wide, int wideLen)
{
    if (!ansi || !wide || wideLen <= 0) return 0;
    return MultiByteToWideChar(CP_ACP, 0, ansi, -1, wide, wideLen);
}

/**
 * Convert UTF-16 to ANSI string (using system code page).
 * 
 * @param wide      Source wide string
 * @param ansi      Destination buffer for ANSI string
 * @param ansiLen   Size of destination buffer in bytes
 * @return          Number of bytes written, or 0 on failure
 */
inline int CPU_WideToAnsi(const wchar_t* wide, char* ansi, int ansiLen)
{
    if (!wide || !ansi || ansiLen <= 0) return 0;
    return WideCharToMultiByte(CP_ACP, 0, wide, -1, ansi, ansiLen, NULL, NULL);
}

////////////////////////////////////////////////////////////////////////////////
// Stack-Allocated Conversion Helpers
// These allocate temporary buffers on the stack - use only in function scope!
////////////////////////////////////////////////////////////////////////////////

/**
 * Create a wide string from UTF-8 on the stack.
 * Usage: wchar_t* wstr = CPU_STACK_UTF8_TO_WIDE(myUtf8String);
 * Note: The buffer is only valid until the end of the enclosing scope!
 */
#define CPU_STACK_UTF8_TO_WIDE(utf8Str) \
    (CPU_Utf8ToWide_Stack((utf8Str), (wchar_t*)_alloca((strlen(utf8Str) + 1) * sizeof(wchar_t))))

/**
 * Create a wide string from ANSI on the stack.
 * Usage: wchar_t* wstr = CPU_STACK_ANSI_TO_WIDE(myAnsiString);
 */
#define CPU_STACK_ANSI_TO_WIDE(ansiStr) \
    (CPU_AnsiToWide_Stack((ansiStr), (wchar_t*)_alloca((strlen(ansiStr) + 1) * sizeof(wchar_t))))

/**
 * Create a UTF-8 string from wide string on the stack.
 * Usage: char* str = CPU_STACK_WIDE_TO_UTF8(myWideString);
 * Note: Allocates 4 bytes per wchar_t to handle any UTF-8 sequence
 */
#define CPU_STACK_WIDE_TO_UTF8(wideStr) \
    (CPU_WideToUtf8_Stack((wideStr), (char*)_alloca((wcslen(wideStr) + 1) * 4)))

// Helper functions for stack macros (inline)
inline wchar_t* CPU_Utf8ToWide_Stack(const char* utf8, wchar_t* buffer)
{
    if (!buffer) return NULL;
    if (!utf8) {
        buffer[0] = L'\0';
        return buffer;
    }
    int len = (int)strlen(utf8) + 1;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buffer, len);
    return buffer;
}

inline wchar_t* CPU_AnsiToWide_Stack(const char* ansi, wchar_t* buffer)
{
    if (!buffer) return NULL;
    if (!ansi) {
        buffer[0] = L'\0';
        return buffer;
    }
    int len = (int)strlen(ansi) + 1;
    MultiByteToWideChar(CP_ACP, 0, ansi, -1, buffer, len);
    return buffer;
}

inline char* CPU_WideToUtf8_Stack(const wchar_t* wide, char* buffer)
{
    if (!buffer) return NULL;
    if (!wide) {
        buffer[0] = '\0';
        return buffer;
    }
    int len = (int)(wcslen(wide) + 1) * 4;
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, buffer, len, NULL, NULL);
    return buffer;
}

////////////////////////////////////////////////////////////////////////////////
// Heap-Allocated Conversion Helpers
// These allocate memory that must be freed with free()!
////////////////////////////////////////////////////////////////////////////////

/**
 * Create a heap-allocated wide string from UTF-8.
 * Caller must free() the returned string!
 * 
 * @param utf8  Source UTF-8 string
 * @return      Newly allocated wide string, or NULL on failure
 */
inline wchar_t* CPU_Utf8ToWide_Alloc(const char* utf8)
{
    if (!utf8) return NULL;
    
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (wideLen <= 0) return NULL;
    
    wchar_t* wide = (wchar_t*)malloc(wideLen * sizeof(wchar_t));
    if (!wide) return NULL;
    
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, wideLen);
    return wide;
}

/**
 * Create a heap-allocated UTF-8 string from wide string.
 * Caller must free() the returned string!
 * 
 * @param wide  Source wide string
 * @return      Newly allocated UTF-8 string, or NULL on failure
 */
inline char* CPU_WideToUtf8_Alloc(const wchar_t* wide)
{
    if (!wide) return NULL;
    
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (utf8Len <= 0) return NULL;
    
    char* utf8 = (char*)malloc(utf8Len);
    if (!utf8) return NULL;
    
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, utf8Len, NULL, NULL);
    return utf8;
}

////////////////////////////////////////////////////////////////////////////////
// Translation Wide String Helper
// Converts gettext translation to wide string for Windows API use
////////////////////////////////////////////////////////////////////////////////

/**
 * Get a translated string as wide string (for Windows API).
 * Uses the T() translation macro and converts to wide.
 * 
 * @param msgid     The message ID to translate
 * @param buffer    Destination buffer for wide string
 * @param bufferLen Size of buffer in wchar_t units
 * @return          Number of characters written
 */
#define T_W(msgid, buffer, bufferLen) \
    CPU_Utf8ToWide(T(msgid), buffer, bufferLen)

/**
 * Same as T_W but uses stack allocation.
 * Only valid until end of scope!
 */
#define T_WS(msgid) \
    CPU_STACK_UTF8_TO_WIDE(T(msgid))

////////////////////////////////////////////////////////////////////////////////
// Path Handling Utilities
////////////////////////////////////////////////////////////////////////////////

/**
 * Check if a path contains non-ASCII characters.
 * Useful for determining if wide APIs must be used.
 * 
 * @param path  Path string to check
 * @return      TRUE if path contains non-ASCII characters
 */
inline BOOL CPU_PathHasNonAscii(const char* path)
{
    if (!path) return FALSE;
    while (*path) {
        if ((unsigned char)*path > 127) return TRUE;
        path++;
    }
    return FALSE;
}

/**
 * Safely copy a wide path to a char buffer, handling long paths.
 * Ensures null termination.
 * 
 * @param widePath  Source wide path
 * @param charPath  Destination char buffer
 * @param charLen   Size of destination buffer
 * @return          TRUE on success, FALSE if truncated or failed
 */
inline BOOL CPU_CopyWidePath(const wchar_t* widePath, char* charPath, int charLen)
{
    if (!widePath || !charPath || charLen <= 0) return FALSE;
    
    int result = WideCharToMultiByte(CP_UTF8, 0, widePath, -1, charPath, charLen, NULL, NULL);
    
    if (result == 0) {
        // Failed - try ANSI as fallback
        result = WideCharToMultiByte(CP_ACP, 0, widePath, -1, charPath, charLen, NULL, NULL);
    }
    
    if (result == 0) {
        // Still failed - ensure null termination
        charPath[0] = '\0';
        return FALSE;
    }
    
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Window Text Helpers
// Consistent wrappers for setting window text with translations
////////////////////////////////////////////////////////////////////////////////

/**
 * Set window text from a translated string.
 * Handles UTF-8 to wide conversion automatically.
 */
inline BOOL CPU_SetWindowTextT(HWND hWnd, const char* msgid)
{
    wchar_t buffer[CPC_TITLE_BUFFER];
    CPU_Utf8ToWide(msgid, buffer, CPC_TITLE_BUFFER);
    return SetWindowTextW(hWnd, buffer);
}

/**
 * Set dialog item text from a translated string.
 */
inline BOOL CPU_SetDlgItemTextT(HWND hDlg, int nIDDlgItem, const char* msgid)
{
    wchar_t buffer[CPC_TITLE_BUFFER];
    CPU_Utf8ToWide(msgid, buffer, CPC_TITLE_BUFFER);
    return SetDlgItemTextW(hDlg, nIDDlgItem, buffer);
}

/**
 * Get window text as UTF-8.
 * 
 * @param hWnd      Window handle
 * @param buffer    Destination UTF-8 buffer
 * @param bufferLen Size of buffer in bytes
 * @return          Number of characters written
 */
inline int CPU_GetWindowTextUtf8(HWND hWnd, char* buffer, int bufferLen)
{
    wchar_t wbuffer[CPC_TITLE_BUFFER];
    int len = GetWindowTextW(hWnd, wbuffer, CPC_TITLE_BUFFER);
    if (len == 0) {
        if (buffer && bufferLen > 0) buffer[0] = '\0';
        return 0;
    }
    return CPU_WideToUtf8(wbuffer, buffer, bufferLen);
}

////////////////////////////////////////////////////////////////////////////////
// Message Box Helpers
////////////////////////////////////////////////////////////////////////////////

/**
 * Show a message box with translated text.
 */
inline int CPU_MessageBoxT(HWND hWnd, const char* text, const char* caption, UINT uType)
{
    wchar_t wtext[1024];
    wchar_t wcaption[256];
    CPU_Utf8ToWide(text, wtext, 1024);
    CPU_Utf8ToWide(caption, wcaption, 256);
    return MessageBoxW(hWnd, wtext, wcaption, uType);
}

#ifdef __cplusplus
}
#endif

#endif // CP_UNICODE_H
