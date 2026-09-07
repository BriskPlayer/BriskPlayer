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

////////////////////////////////////////////////////////////////////////////////
//
// Centralized Configuration Manager Implementation
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CP_Config.h"
#include "globals.h"

////////////////////////////////////////////////////////////////////////////////
// Module State
////////////////////////////////////////////////////////////////////////////////

// The authoritative INI path, kept wide throughout. An install path can
// contain characters that do not exist in the process's ANSI code page
// (e.g. a non-Latin Windows username) — there is no narrow-string
// workaround for that, so every actual file/registry-style INI operation
// in this module goes through the W (wide) Win32 profile APIs against this
// buffer rather than an ANSI path.
static WCHAR g_wszConfigPath[CPC_PATH_BUFFER] = {0};

// UTF-8 mirror of g_wszConfigPath, lazily filled in by CPConfig_GetFilePath()
// purely for display/logging via the public const char* API. Never used for
// actual file I/O.
static char g_szConfigPathUtf8[CPC_PATH_BUFFER * 3] = {0};

static BOOL g_bInitialized = FALSE;
static BOOL g_bDirty = FALSE;

////////////////////////////////////////////////////////////////////////////////
// Helper Functions
////////////////////////////////////////////////////////////////////////////////

static void EnsureInitialized(void)
{
    if (!g_bInitialized) {
        CPConfig_Initialize();
    }
}

// Section/key names in this codebase are always short plain-ASCII literals
// (or stringified struct member names via the CPCONFIG_READ_*/WRITE_*
// macros), so a bounded CP_ACP conversion into a fixed-size stack buffer is
// sufficient — unlike the config file's own path, these never carry
// arbitrary user-authored Unicode text.
#define CPCFG_IDENT_WCHARS 256

////////////////////////////////////////////////////////////////////////////////
// Initialization / Cleanup
////////////////////////////////////////////////////////////////////////////////

CP_Result CPConfig_Initialize(void)
{
    if (g_bInitialized) {
        return CP_WARN_ALREADY_DONE;
    }

    // Get executable directory. GetModuleFileNameW (not the ANSI
    // GetModuleFileNameA) is required: a path containing characters outside
    // the process's ANSI code page cannot be represented in an ANSI string
    // at all, so only the wide API can reliably identify it.
    DWORD len = GetModuleFileNameW(NULL, g_wszConfigPath, CPC_PATH_BUFFER);
    if (len == 0) {
        return CP_ERROR_FILE_NOT_FOUND;
    }
    if (len == CPC_PATH_BUFFER && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        // Truncated — bail out rather than build a path from an incomplete
        // string that would silently point at the wrong file.
        return CP_ERROR_FILE_NOT_FOUND;
    }

    // Remove filename, keep directory
    WCHAR* pLastSlash = wcsrchr(g_wszConfigPath, L'\\');
    if (pLastSlash) {
        *(pLastSlash + 1) = L'\0';
    }

    // Append INI filename
    if (wcscat_s(g_wszConfigPath, CPC_PATH_BUFFER, L"briskplayer.ini") != 0) {
        return CP_ERROR_FILE_NOT_FOUND;
    }

    g_bInitialized = TRUE;
    g_bDirty = FALSE;

    CP_LOG_DEBUG("Config initialized: %ls\n", g_wszConfigPath);
    return CP_OK;
}

void CPConfig_Cleanup(void)
{
    if (g_bDirty) {
        CPConfig_Flush();
    }
    g_bInitialized = FALSE;
}

CP_Result CPConfig_Flush(void)
{
    if (!g_bInitialized) {
        return CP_ERROR_NOT_INITIALIZED;
    }

    // WritePrivateProfileString with NULL values flushes cache
    WritePrivateProfileStringW(NULL, NULL, NULL, g_wszConfigPath);
    g_bDirty = FALSE;

    return CP_OK;
}

const char* CPConfig_GetFilePath(void)
{
    EnsureInitialized();

    // Returned as UTF-8 purely for accurate display/logging of the real
    // path; actual file I/O always goes through g_wszConfigPath directly.
    WideCharToMultiByte(CP_UTF8, 0, g_wszConfigPath, -1,
                        g_szConfigPathUtf8, sizeof(g_szConfigPathUtf8),
                        NULL, NULL);
    return g_szConfigPathUtf8;
}

////////////////////////////////////////////////////////////////////////////////
// Integer Values
////////////////////////////////////////////////////////////////////////////////

int CPConfig_GetInt(const char* section, const char* key, int defaultValue)
{
    EnsureInitialized();
    CP_RETURN_IF_NULL(section, defaultValue);
    CP_RETURN_IF_NULL(key, defaultValue);

    WCHAR wSection[CPCFG_IDENT_WCHARS], wKey[CPCFG_IDENT_WCHARS];
    MultiByteToWideChar(CP_ACP, 0, section, -1, wSection, CPCFG_IDENT_WCHARS);
    MultiByteToWideChar(CP_ACP, 0, key, -1, wKey, CPCFG_IDENT_WCHARS);

    return GetPrivateProfileIntW(wSection, wKey, defaultValue, g_wszConfigPath);
}

void CPConfig_SetInt(const char* section, const char* key, int value)
{
    EnsureInitialized();
    if (!section || !key) return;

    char buffer[32];
    sprintf_s(buffer, sizeof(buffer), "%d", value);

    WCHAR wSection[CPCFG_IDENT_WCHARS], wKey[CPCFG_IDENT_WCHARS], wValue[32];
    MultiByteToWideChar(CP_ACP, 0, section, -1, wSection, CPCFG_IDENT_WCHARS);
    MultiByteToWideChar(CP_ACP, 0, key, -1, wKey, CPCFG_IDENT_WCHARS);
    MultiByteToWideChar(CP_ACP, 0, buffer, -1, wValue, 32);

    WritePrivateProfileStringW(wSection, wKey, wValue, g_wszConfigPath);
    g_bDirty = TRUE;
}

int CPConfig_GetIntClamped(const char* section, const char* key,
                           int defaultValue, int minValue, int maxValue)
{
    int value = CPConfig_GetInt(section, key, defaultValue);

    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

////////////////////////////////////////////////////////////////////////////////
// Boolean Values
////////////////////////////////////////////////////////////////////////////////

BOOL CPConfig_GetBool(const char* section, const char* key, BOOL defaultValue)
{
    int value = CPConfig_GetInt(section, key, defaultValue ? 1 : 0);
    return value != 0;
}

void CPConfig_SetBool(const char* section, const char* key, BOOL value)
{
    CPConfig_SetInt(section, key, value ? 1 : 0);
}

////////////////////////////////////////////////////////////////////////////////
// String Values
////////////////////////////////////////////////////////////////////////////////

int CPConfig_GetString(const char* section, const char* key,
                       const char* defaultValue,
                       char* buffer, int bufferSize)
{
    EnsureInitialized();
    CP_RETURN_IF_NULL(section, 0);
    CP_RETURN_IF_NULL(key, 0);
    CP_RETURN_IF_NULL(buffer, 0);

    if (bufferSize <= 0) return 0;

    WCHAR wSection[CPCFG_IDENT_WCHARS], wKey[CPCFG_IDENT_WCHARS];
    MultiByteToWideChar(CP_ACP, 0, section, -1, wSection, CPCFG_IDENT_WCHARS);
    MultiByteToWideChar(CP_ACP, 0, key, -1, wKey, CPCFG_IDENT_WCHARS);

    WCHAR wDefaultStack[CPC_PATH_BUFFER];
    WCHAR* pwDefault = wDefaultStack;
    if (defaultValue)
        MultiByteToWideChar(CP_ACP, 0, defaultValue, -1, wDefaultStack, CPC_PATH_BUFFER);
    else
        wDefaultStack[0] = L'\0';

    // Read into a wide buffer sized to match the caller's requested
    // capacity (one wide char per narrow char is always enough headroom
    // for the profile strings this module deals with).
    WCHAR* pwValue = (WCHAR*)malloc((size_t)bufferSize * sizeof(WCHAR));
    if (!pwValue)
    {
        buffer[0] = '\0';
        return 0;
    }

    GetPrivateProfileStringW(wSection, wKey, pwDefault, pwValue, bufferSize, g_wszConfigPath);

    int result = WideCharToMultiByte(CP_ACP, 0, pwValue, -1, buffer, bufferSize, NULL, NULL);
    free(pwValue);

    if (result == 0)
    {
        // Conversion failed (e.g. result too long for the ANSI buffer);
        // leave buffer in a defined, empty state rather than partial data.
        buffer[0] = '\0';
        return 0;
    }
    return result - 1; // exclude the null terminator, matching the old *A return convention
}

void CPConfig_SetString(const char* section, const char* key, const char* value)
{
    EnsureInitialized();
    if (!section || !key) return;

    WCHAR wSection[CPCFG_IDENT_WCHARS], wKey[CPCFG_IDENT_WCHARS];
    MultiByteToWideChar(CP_ACP, 0, section, -1, wSection, CPCFG_IDENT_WCHARS);
    MultiByteToWideChar(CP_ACP, 0, key, -1, wKey, CPCFG_IDENT_WCHARS);

    WCHAR* pwValue = NULL;
    if (value)
    {
        int cch = MultiByteToWideChar(CP_ACP, 0, value, -1, NULL, 0);
        if (cch > 0)
        {
            pwValue = (WCHAR*)malloc((size_t)cch * sizeof(WCHAR));
            if (pwValue)
                MultiByteToWideChar(CP_ACP, 0, value, -1, pwValue, cch);
        }
    }

    WritePrivateProfileStringW(wSection, wKey, pwValue, g_wszConfigPath);
    free(pwValue);
    g_bDirty = TRUE;
}

int CPConfig_GetStringW(const char* section, const char* key,
                        const wchar_t* defaultValue,
                        wchar_t* buffer, int bufferSize)
{
    EnsureInitialized();
    CP_RETURN_IF_NULL(section, 0);
    CP_RETURN_IF_NULL(key, 0);
    CP_RETURN_IF_NULL(buffer, 0);

    if (bufferSize <= 0) return 0;

    wchar_t wSection[CPCFG_IDENT_WCHARS], wKey[CPCFG_IDENT_WCHARS];
    MultiByteToWideChar(CP_ACP, 0, section, -1, wSection, CPCFG_IDENT_WCHARS);
    MultiByteToWideChar(CP_ACP, 0, key, -1, wKey, CPCFG_IDENT_WCHARS);

    return GetPrivateProfileStringW(wSection, wKey,
                                    defaultValue ? defaultValue : L"",
                                    buffer, bufferSize, g_wszConfigPath);
}

void CPConfig_SetStringW(const char* section, const char* key, const wchar_t* value)
{
    EnsureInitialized();
    if (!section || !key) return;

    wchar_t wSection[CPCFG_IDENT_WCHARS], wKey[CPCFG_IDENT_WCHARS];
    MultiByteToWideChar(CP_ACP, 0, section, -1, wSection, CPCFG_IDENT_WCHARS);
    MultiByteToWideChar(CP_ACP, 0, key, -1, wKey, CPCFG_IDENT_WCHARS);

    WritePrivateProfileStringW(wSection, wKey, value, g_wszConfigPath);
    g_bDirty = TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Rectangle/Position Values
////////////////////////////////////////////////////////////////////////////////

BOOL CPConfig_GetRect(const char* section, const char* keyPrefix,
                      RECT* pRect, const RECT* pDefault)
{
    CP_RETURN_IF_NULL(section, FALSE);
    CP_RETURN_IF_NULL(keyPrefix, FALSE);
    CP_RETURN_IF_NULL(pRect, FALSE);

    char keyBuf[64];

    // Read each component
    sprintf_s(keyBuf, sizeof(keyBuf), "%sX", keyPrefix);
    pRect->left = CPConfig_GetInt(section, keyBuf, pDefault ? pDefault->left : 0);

    sprintf_s(keyBuf, sizeof(keyBuf), "%sY", keyPrefix);
    pRect->top = CPConfig_GetInt(section, keyBuf, pDefault ? pDefault->top : 0);

    sprintf_s(keyBuf, sizeof(keyBuf), "%sW", keyPrefix);
    pRect->right = pRect->left + CPConfig_GetInt(section, keyBuf,
                   pDefault ? (pDefault->right - pDefault->left) : 100);

    sprintf_s(keyBuf, sizeof(keyBuf), "%sH", keyPrefix);
    pRect->bottom = pRect->top + CPConfig_GetInt(section, keyBuf,
                    pDefault ? (pDefault->bottom - pDefault->top) : 100);

    return TRUE;
}

void CPConfig_SetRect(const char* section, const char* keyPrefix, const RECT* pRect)
{
    if (!section || !keyPrefix || !pRect) return;

    char keyBuf[64];

    sprintf_s(keyBuf, sizeof(keyBuf), "%sX", keyPrefix);
    CPConfig_SetInt(section, keyBuf, pRect->left);

    sprintf_s(keyBuf, sizeof(keyBuf), "%sY", keyPrefix);
    CPConfig_SetInt(section, keyBuf, pRect->top);

    sprintf_s(keyBuf, sizeof(keyBuf), "%sW", keyPrefix);
    CPConfig_SetInt(section, keyBuf, pRect->right - pRect->left);

    sprintf_s(keyBuf, sizeof(keyBuf), "%sH", keyPrefix);
    CPConfig_SetInt(section, keyBuf, pRect->bottom - pRect->top);
}

BOOL CPConfig_GetPoint(const char* section, const char* keyPrefix,
                       POINT* pPoint, const POINT* pDefault)
{
    CP_RETURN_IF_NULL(section, FALSE);
    CP_RETURN_IF_NULL(keyPrefix, FALSE);
    CP_RETURN_IF_NULL(pPoint, FALSE);

    char keyBuf[64];

    sprintf_s(keyBuf, sizeof(keyBuf), "%sX", keyPrefix);
    pPoint->x = CPConfig_GetInt(section, keyBuf, pDefault ? pDefault->x : 0);

    sprintf_s(keyBuf, sizeof(keyBuf), "%sY", keyPrefix);
    pPoint->y = CPConfig_GetInt(section, keyBuf, pDefault ? pDefault->y : 0);

    return TRUE;
}

void CPConfig_SetPoint(const char* section, const char* keyPrefix, const POINT* pPoint)
{
    if (!section || !keyPrefix || !pPoint) return;

    char keyBuf[64];

    sprintf_s(keyBuf, sizeof(keyBuf), "%sX", keyPrefix);
    CPConfig_SetInt(section, keyBuf, pPoint->x);

    sprintf_s(keyBuf, sizeof(keyBuf), "%sY", keyPrefix);
    CPConfig_SetInt(section, keyBuf, pPoint->y);
}

////////////////////////////////////////////////////////////////////////////////
// Color Values
////////////////////////////////////////////////////////////////////////////////

COLORREF CPConfig_GetColor(const char* section, const char* key, COLORREF defaultValue)
{
    EnsureInitialized();

    char buffer[32];
    if (CPConfig_GetString(section, key, NULL, buffer, sizeof(buffer)) == 0) {
        return defaultValue;
    }

    // Try to parse as hex (0xRRGGBB or RRGGBB)
    unsigned int r, g, b;
    if (buffer[0] == '0' && (buffer[1] == 'x' || buffer[1] == 'X')) {
        if (sscanf(buffer + 2, "%02x%02x%02x", &r, &g, &b) == 3) {
            return RGB(r, g, b);
        }
    }
    else if (sscanf(buffer, "%02x%02x%02x", &r, &g, &b) == 3) {
        return RGB(r, g, b);
    }

    // Try as decimal COLORREF
    unsigned int value;
    if (sscanf(buffer, "%u", &value) == 1) {
        return (COLORREF)value;
    }

    return defaultValue;
}

void CPConfig_SetColor(const char* section, const char* key, COLORREF value)
{
    char buffer[32];
    sprintf_s(buffer, sizeof(buffer), "0x%02X%02X%02X",
              GetRValue(value), GetGValue(value), GetBValue(value));
    CPConfig_SetString(section, key, buffer);
}

////////////////////////////////////////////////////////////////////////////////
// Array/List Values
////////////////////////////////////////////////////////////////////////////////

int CPConfig_GetIntArray(const char* section, const char* keyPrefix,
                         int* values, int maxCount, int defaultValue)
{
    CP_RETURN_IF_NULL(values, 0);
    if (maxCount <= 0) return 0;

    int count = 0;
    char keyBuf[64];

    for (int i = 0; i < maxCount; i++) {
        sprintf_s(keyBuf, sizeof(keyBuf), "%s%d", keyPrefix, i);

        // Check if key exists by getting with impossible default
        int sentinel = INT_MIN + i;  // Unique unlikely value
        int value = CPConfig_GetInt(section, keyBuf, sentinel);

        if (value == sentinel) {
            // Key doesn't exist, use default
            values[i] = defaultValue;
        } else {
            values[i] = value;
            count = i + 1;  // Track highest index found
        }
    }

    return count;
}

void CPConfig_SetIntArray(const char* section, const char* keyPrefix,
                          const int* values, int count)
{
    if (!values || count <= 0) return;

    char keyBuf[64];

    for (int i = 0; i < count; i++) {
        sprintf_s(keyBuf, sizeof(keyBuf), "%s%d", keyPrefix, i);
        CPConfig_SetInt(section, keyBuf, values[i]);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Section Enumeration
////////////////////////////////////////////////////////////////////////////////

void CPConfig_EnumerateKeys(const char* section, CPConfig_KeyCallback callback, void* userData)
{
    EnsureInitialized();
    if (!section || !callback) return;

    WCHAR wSection[CPCFG_IDENT_WCHARS];
    MultiByteToWideChar(CP_ACP, 0, section, -1, wSection, CPCFG_IDENT_WCHARS);

    // Get all key names (double-null-terminated multi-string)
    WCHAR wKeyNames[4096];
    DWORD len = GetPrivateProfileStringW(wSection, NULL, L"", wKeyNames, 4096, g_wszConfigPath);

    if (len == 0) return;

    // Iterate through null-separated key names
    WCHAR* pwKey = wKeyNames;
    while (*pwKey) {
        char keyBuffer[512];
        char valueBuffer[1024];
        WCHAR wValueBuffer[1024];

        WideCharToMultiByte(CP_ACP, 0, pwKey, -1, keyBuffer, sizeof(keyBuffer), NULL, NULL);

        GetPrivateProfileStringW(wSection, pwKey, L"", wValueBuffer, 1024, g_wszConfigPath);
        WideCharToMultiByte(CP_ACP, 0, wValueBuffer, -1, valueBuffer, sizeof(valueBuffer), NULL, NULL);

        // Call the callback
        callback(keyBuffer, valueBuffer, userData);

        // Move to next key
        pwKey += wcslen(pwKey) + 1;
    }
}

void CPConfig_DeleteKey(const char* section, const char* key)
{
    EnsureInitialized();
    if (!section || !key) return;

    WCHAR wSection[CPCFG_IDENT_WCHARS], wKey[CPCFG_IDENT_WCHARS];
    MultiByteToWideChar(CP_ACP, 0, section, -1, wSection, CPCFG_IDENT_WCHARS);
    MultiByteToWideChar(CP_ACP, 0, key, -1, wKey, CPCFG_IDENT_WCHARS);

    WritePrivateProfileStringW(wSection, wKey, NULL, g_wszConfigPath);
    g_bDirty = TRUE;
}

void CPConfig_DeleteSection(const char* section)
{
    EnsureInitialized();
    if (!section) return;

    WCHAR wSection[CPCFG_IDENT_WCHARS];
    MultiByteToWideChar(CP_ACP, 0, section, -1, wSection, CPCFG_IDENT_WCHARS);

    WritePrivateProfileStringW(wSection, NULL, NULL, g_wszConfigPath);
    g_bDirty = TRUE;
}
