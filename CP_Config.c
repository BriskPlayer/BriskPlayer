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

static char g_szConfigPath[CPC_PATH_BUFFER] = {0};
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

////////////////////////////////////////////////////////////////////////////////
// Initialization / Cleanup
////////////////////////////////////////////////////////////////////////////////

CP_Result CPConfig_Initialize(void)
{
    if (g_bInitialized) {
        return CP_WARN_ALREADY_DONE;
    }
    
    // Get executable directory
    DWORD len = GetModuleFileNameA(NULL, g_szConfigPath, CPC_PATH_BUFFER);
    if (len == 0) {
        return CP_ERROR_FILE_NOT_FOUND;
    }
    
    // Remove filename, keep directory
    char* pLastSlash = strrchr(g_szConfigPath, '\\');
    if (pLastSlash) {
        *(pLastSlash + 1) = '\0';
    }
    
    // Append INI filename
    strcat_s(g_szConfigPath, sizeof(g_szConfigPath), "briskplayer.ini");
    
    g_bInitialized = TRUE;
    g_bDirty = FALSE;
    
    CP_LOG_DEBUG("Config initialized: %s\n", g_szConfigPath);
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
    WritePrivateProfileStringA(NULL, NULL, NULL, g_szConfigPath);
    g_bDirty = FALSE;
    
    return CP_OK;
}

const char* CPConfig_GetFilePath(void)
{
    EnsureInitialized();
    return g_szConfigPath;
}

////////////////////////////////////////////////////////////////////////////////
// Integer Values
////////////////////////////////////////////////////////////////////////////////

int CPConfig_GetInt(const char* section, const char* key, int defaultValue)
{
    EnsureInitialized();
    CP_RETURN_IF_NULL(section, defaultValue);
    CP_RETURN_IF_NULL(key, defaultValue);
    
    return GetPrivateProfileIntA(section, key, defaultValue, g_szConfigPath);
}

void CPConfig_SetInt(const char* section, const char* key, int value)
{
    EnsureInitialized();
    if (!section || !key) return;
    
    char buffer[32];
    sprintf_s(buffer, sizeof(buffer), "%d", value);
    WritePrivateProfileStringA(section, key, buffer, g_szConfigPath);
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
    
    return GetPrivateProfileStringA(section, key, 
                                    defaultValue ? defaultValue : "",
                                    buffer, bufferSize, g_szConfigPath);
}

void CPConfig_SetString(const char* section, const char* key, const char* value)
{
    EnsureInitialized();
    if (!section || !key) return;
    
    WritePrivateProfileStringA(section, key, value, g_szConfigPath);
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
    
    // Convert section and key to wide strings
    wchar_t wSection[256], wKey[256];
    MultiByteToWideChar(CP_ACP, 0, section, -1, wSection, 256);
    MultiByteToWideChar(CP_ACP, 0, key, -1, wKey, 256);
    
    // Convert path to wide string
    wchar_t wPath[CPC_PATH_BUFFER];
    MultiByteToWideChar(CP_ACP, 0, g_szConfigPath, -1, wPath, CPC_PATH_BUFFER);
    
    return GetPrivateProfileStringW(wSection, wKey,
                                    defaultValue ? defaultValue : L"",
                                    buffer, bufferSize, wPath);
}

void CPConfig_SetStringW(const char* section, const char* key, const wchar_t* value)
{
    EnsureInitialized();
    if (!section || !key) return;
    
    // Convert section and key to wide strings
    wchar_t wSection[256], wKey[256];
    MultiByteToWideChar(CP_ACP, 0, section, -1, wSection, 256);
    MultiByteToWideChar(CP_ACP, 0, key, -1, wKey, 256);
    
    // Convert path to wide string
    wchar_t wPath[CPC_PATH_BUFFER];
    MultiByteToWideChar(CP_ACP, 0, g_szConfigPath, -1, wPath, CPC_PATH_BUFFER);
    
    WritePrivateProfileStringW(wSection, wKey, value, wPath);
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
    
    // Get all key names
    char keyBuffer[4096];
    DWORD len = GetPrivateProfileStringA(section, NULL, "", keyBuffer, sizeof(keyBuffer), g_szConfigPath);
    
    if (len == 0) return;
    
    // Iterate through null-separated key names
    char* pKey = keyBuffer;
    while (*pKey) {
        // Get the value for this key
        char valueBuffer[1024];
        GetPrivateProfileStringA(section, pKey, "", valueBuffer, sizeof(valueBuffer), g_szConfigPath);
        
        // Call the callback
        callback(pKey, valueBuffer, userData);
        
        // Move to next key
        pKey += strlen(pKey) + 1;
    }
}

void CPConfig_DeleteKey(const char* section, const char* key)
{
    EnsureInitialized();
    if (!section || !key) return;
    
    WritePrivateProfileStringA(section, key, NULL, g_szConfigPath);
    g_bDirty = TRUE;
}

void CPConfig_DeleteSection(const char* section)
{
    EnsureInitialized();
    if (!section) return;
    
    WritePrivateProfileStringA(section, NULL, NULL, g_szConfigPath);
    g_bDirty = TRUE;
}
