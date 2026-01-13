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

#ifndef CP_CLEANUP_H
#define CP_CLEANUP_H

////////////////////////////////////////////////////////////////////////////////
//
// Resource Cleanup Helpers for BriskPlayer
//
// These macros provide safe, consistent cleanup patterns for Windows resources.
// They prevent double-free issues and NULL pointer dereferencing.
//
// Usage:
//   HBITMAP hBmp = LoadBitmap(...);
//   // ... use bitmap ...
//   SAFE_DELETE_OBJECT(hBmp);  // hBmp is now NULL
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>

////////////////////////////////////////////////////////////////////////////////
// Memory Cleanup
////////////////////////////////////////////////////////////////////////////////

// Safe free with NULL check and pointer reset
#ifndef SAFE_FREE
#define SAFE_FREE(ptr) do { \
    if ((ptr) != NULL) { \
        free(ptr); \
        (ptr) = NULL; \
    } \
} while(0)
#endif

// Safe CoTaskMemFree (for COM-allocated memory)
#define SAFE_COTASKMEMFREE(ptr) do { \
    if ((ptr) != NULL) { \
        CoTaskMemFree(ptr); \
        (ptr) = NULL; \
    } \
} while(0)

// Safe LocalFree
#define SAFE_LOCALFREE(ptr) do { \
    if ((ptr) != NULL) { \
        LocalFree(ptr); \
        (ptr) = NULL; \
    } \
} while(0)

////////////////////////////////////////////////////////////////////////////////
// GDI Object Cleanup
////////////////////////////////////////////////////////////////////////////////

// Safe DeleteObject for GDI objects (HBITMAP, HBRUSH, HPEN, HFONT, HRGN)
#define SAFE_DELETE_OBJECT(hObj) do { \
    if ((hObj) != NULL) { \
        DeleteObject(hObj); \
        (hObj) = NULL; \
    } \
} while(0)

// Safe DeleteDC
#define SAFE_DELETE_DC(hDC) do { \
    if ((hDC) != NULL) { \
        DeleteDC(hDC); \
        (hDC) = NULL; \
    } \
} while(0)

// Safe ReleaseDC (requires window handle)
#define SAFE_RELEASE_DC(hWnd, hDC) do { \
    if ((hDC) != NULL) { \
        ReleaseDC(hWnd, hDC); \
        (hDC) = NULL; \
    } \
} while(0)

// Safe DestroyIcon
#define SAFE_DESTROY_ICON(hIcon) do { \
    if ((hIcon) != NULL) { \
        DestroyIcon(hIcon); \
        (hIcon) = NULL; \
    } \
} while(0)

// Safe DestroyCursor
#define SAFE_DESTROY_CURSOR(hCursor) do { \
    if ((hCursor) != NULL) { \
        DestroyCursor(hCursor); \
        (hCursor) = NULL; \
    } \
} while(0)

// Safe DestroyMenu
#define SAFE_DESTROY_MENU(hMenu) do { \
    if ((hMenu) != NULL) { \
        DestroyMenu(hMenu); \
        (hMenu) = NULL; \
    } \
} while(0)

// Safe ImageList_Destroy
#define SAFE_IMAGELIST_DESTROY(hImageList) do { \
    if ((hImageList) != NULL) { \
        ImageList_Destroy(hImageList); \
        (hImageList) = NULL; \
    } \
} while(0)

////////////////////////////////////////////////////////////////////////////////
// Handle Cleanup
////////////////////////////////////////////////////////////////////////////////

// Safe CloseHandle (for HANDLE types)
#define SAFE_CLOSE_HANDLE(hHandle) do { \
    if ((hHandle) != NULL && (hHandle) != INVALID_HANDLE_VALUE) { \
        CloseHandle(hHandle); \
        (hHandle) = NULL; \
    } \
} while(0)

// Safe FindClose
#define SAFE_FIND_CLOSE(hFind) do { \
    if ((hFind) != INVALID_HANDLE_VALUE) { \
        FindClose(hFind); \
        (hFind) = INVALID_HANDLE_VALUE; \
    } \
} while(0)

// Safe UnmapViewOfFile
#define SAFE_UNMAP_VIEW(pView) do { \
    if ((pView) != NULL) { \
        UnmapViewOfFile(pView); \
        (pView) = NULL; \
    } \
} while(0)

////////////////////////////////////////////////////////////////////////////////
// Window Cleanup
////////////////////////////////////////////////////////////////////////////////

// Safe DestroyWindow
#define SAFE_DESTROY_WINDOW(hWnd) do { \
    if ((hWnd) != NULL && IsWindow(hWnd)) { \
        DestroyWindow(hWnd); \
        (hWnd) = NULL; \
    } \
} while(0)

// Safe UnhookWindowsHookEx
#define SAFE_UNHOOK(hHook) do { \
    if ((hHook) != NULL) { \
        UnhookWindowsHookEx(hHook); \
        (hHook) = NULL; \
    } \
} while(0)

// Safe KillTimer
#define SAFE_KILL_TIMER(hWnd, timerId) do { \
    KillTimer(hWnd, timerId); \
} while(0)

////////////////////////////////////////////////////////////////////////////////
// COM Object Cleanup
////////////////////////////////////////////////////////////////////////////////

// Safe Release for COM objects
#define SAFE_RELEASE(pInterface) do { \
    if ((pInterface) != NULL) { \
        (pInterface)->lpVtbl->Release(pInterface); \
        (pInterface) = NULL; \
    } \
} while(0)

// C++ compatible COM release
#ifdef __cplusplus
#define SAFE_RELEASE_CPP(pInterface) do { \
    if ((pInterface) != NULL) { \
        (pInterface)->Release(); \
        (pInterface) = NULL; \
    } \
} while(0)
#endif

////////////////////////////////////////////////////////////////////////////////
// File Handle Cleanup
////////////////////////////////////////////////////////////////////////////////

// Safe fclose
#define SAFE_FCLOSE(fp) do { \
    if ((fp) != NULL) { \
        fclose(fp); \
        (fp) = NULL; \
    } \
} while(0)

////////////////////////////////////////////////////////////////////////////////
// Library Cleanup
////////////////////////////////////////////////////////////////////////////////

// Safe FreeLibrary
#define SAFE_FREE_LIBRARY(hLib) do { \
    if ((hLib) != NULL) { \
        FreeLibrary(hLib); \
        (hLib) = NULL; \
    } \
} while(0)

////////////////////////////////////////////////////////////////////////////////
// Critical Section Helpers
////////////////////////////////////////////////////////////////////////////////

// Initialize and track critical section
#define INIT_CRITICAL_SECTION(cs) do { \
    InitializeCriticalSection(&(cs)); \
} while(0)

// Safe DeleteCriticalSection
#define SAFE_DELETE_CRITICAL_SECTION(cs) do { \
    DeleteCriticalSection(&(cs)); \
} while(0)

////////////////////////////////////////////////////////////////////////////////
// Cleanup Scope Helper (Deferred Cleanup Pattern)
//
// For more complex cleanup scenarios, use this pattern:
//
//   int result = CPC_ERROR;
//   HBITMAP hBmp = NULL;
//   HDC hDC = NULL;
//   
//   hBmp = LoadBitmap(...);
//   if (!hBmp) goto cleanup;
//   
//   hDC = CreateCompatibleDC(...);
//   if (!hDC) goto cleanup;
//   
//   // ... do work ...
//   result = CPC_SUCCESS;
//   
// cleanup:
//   SAFE_DELETE_OBJECT(hBmp);
//   SAFE_DELETE_DC(hDC);
//   return result;
//
////////////////////////////////////////////////////////////////////////////////

// Validate pointer before use
#define CP_VALIDATE_PTR(ptr, retval) do { \
    if ((ptr) == NULL) { \
        return (retval); \
    } \
} while(0)

// Validate pointer with custom error handling
#define CP_VALIDATE_PTR_GOTO(ptr, label) do { \
    if ((ptr) == NULL) { \
        goto label; \
    } \
} while(0)

// Validate handle before use
#define CP_VALIDATE_HANDLE(handle, retval) do { \
    if ((handle) == NULL || (handle) == INVALID_HANDLE_VALUE) { \
        return (retval); \
    } \
} while(0)

////////////////////////////////////////////////////////////////////////////////
// GDI Select/Restore Helpers
//
// RAII-style helpers for GDI object selection
////////////////////////////////////////////////////////////////////////////////

// Save old object when selecting new one, restore at cleanup
typedef struct {
    HDC hDC;
    HGDIOBJ hOldObject;
} CP_GDIObjectScope;

// Begin GDI object scope
#define CP_GDI_SELECT_BEGIN(scope, dc, newObj) \
    (scope).hDC = (dc); \
    (scope).hOldObject = SelectObject((dc), (newObj))

// End GDI object scope (restore original)
#define CP_GDI_SELECT_END(scope) \
    if ((scope).hDC && (scope).hOldObject) { \
        SelectObject((scope).hDC, (scope).hOldObject); \
    }

////////////////////////////////////////////////////////////////////////////////
// Bitmap GDI Helper
//
// Common pattern: create memory DC, select bitmap, do work, cleanup
////////////////////////////////////////////////////////////////////////////////

typedef struct {
    HDC hMemDC;
    HBITMAP hOldBitmap;
} CP_BitmapDC;

// Create compatible DC and select bitmap
static inline BOOL CP_BitmapDC_Create(CP_BitmapDC* pBmpDC, HDC hDC, HBITMAP hBitmap)
{
    if (!pBmpDC || !hBitmap) return FALSE;
    
    pBmpDC->hMemDC = CreateCompatibleDC(hDC);
    if (!pBmpDC->hMemDC) return FALSE;
    
    pBmpDC->hOldBitmap = (HBITMAP)SelectObject(pBmpDC->hMemDC, hBitmap);
    return TRUE;
}

// Cleanup bitmap DC
static inline void CP_BitmapDC_Destroy(CP_BitmapDC* pBmpDC)
{
    if (pBmpDC && pBmpDC->hMemDC) {
        if (pBmpDC->hOldBitmap) {
            SelectObject(pBmpDC->hMemDC, pBmpDC->hOldBitmap);
        }
        DeleteDC(pBmpDC->hMemDC);
        pBmpDC->hMemDC = NULL;
        pBmpDC->hOldBitmap = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////

#endif // CP_CLEANUP_H
