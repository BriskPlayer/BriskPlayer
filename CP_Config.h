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

#ifndef CP_CONFIG_H
#define CP_CONFIG_H

////////////////////////////////////////////////////////////////////////////////
//
// Centralized Configuration Manager
//
// Provides a clean API for reading/writing application configuration.
// Abstracts the INI file format and provides type-safe accessors.
//
// Usage:
//   // Read a value with default
//   int volume = CPConfig_GetInt("Audio", "Volume", 100);
//   
//   // Write a value
//   CPConfig_SetInt("Audio", "Volume", newVolume);
//   
//   // Auto-save when dirty (call periodically or on exit)
//   CPConfig_Flush();
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>

////////////////////////////////////////////////////////////////////////////////
// Configuration Sections (standardized section names)
////////////////////////////////////////////////////////////////////////////////

#define CPCONFIG_SECTION_WINDOW     "WindowPos"
#define CPCONFIG_SECTION_AUDIO      "Options"
#define CPCONFIG_SECTION_PLAYLIST   "Options"
#define CPCONFIG_SECTION_SKIN       "Options"
#define CPCONFIG_SECTION_BEHAVIOR   "Options"
#define CPCONFIG_SECTION_LANGUAGE   "Options"

////////////////////////////////////////////////////////////////////////////////
// Initialization / Cleanup
////////////////////////////////////////////////////////////////////////////////

// Initialize config system (called once at startup)
// Determines INI file path based on executable location
CP_Result CPConfig_Initialize(void);

// Cleanup config system (called at shutdown)
void CPConfig_Cleanup(void);

// Force write any pending changes to disk
CP_Result CPConfig_Flush(void);

// Get the path to the INI file
const char* CPConfig_GetFilePath(void);

////////////////////////////////////////////////////////////////////////////////
// Integer Values
////////////////////////////////////////////////////////////////////////////////

// Get integer value with default
int CPConfig_GetInt(const char* section, const char* key, int defaultValue);

// Set integer value
void CPConfig_SetInt(const char* section, const char* key, int value);

// Get integer with range validation
int CPConfig_GetIntClamped(const char* section, const char* key, 
                           int defaultValue, int minValue, int maxValue);

////////////////////////////////////////////////////////////////////////////////
// Boolean Values
////////////////////////////////////////////////////////////////////////////////

// Get boolean value (stored as 0/1 in INI)
BOOL CPConfig_GetBool(const char* section, const char* key, BOOL defaultValue);

// Set boolean value
void CPConfig_SetBool(const char* section, const char* key, BOOL value);

////////////////////////////////////////////////////////////////////////////////
// String Values
////////////////////////////////////////////////////////////////////////////////

// Get string value into buffer
// Returns number of characters copied (excluding null terminator)
int CPConfig_GetString(const char* section, const char* key, 
                       const char* defaultValue,
                       char* buffer, int bufferSize);

// Set string value
void CPConfig_SetString(const char* section, const char* key, const char* value);

// Get string value (wide char version)
int CPConfig_GetStringW(const char* section, const char* key,
                        const wchar_t* defaultValue,
                        wchar_t* buffer, int bufferSize);

// Set string value (wide char version)  
void CPConfig_SetStringW(const char* section, const char* key, const wchar_t* value);

////////////////////////////////////////////////////////////////////////////////
// Rectangle/Position Values
////////////////////////////////////////////////////////////////////////////////

// Get RECT value (stored as X,Y,Width,Height)
BOOL CPConfig_GetRect(const char* section, const char* keyPrefix,
                      RECT* pRect, const RECT* pDefault);

// Set RECT value
void CPConfig_SetRect(const char* section, const char* keyPrefix, const RECT* pRect);

// Get POINT value
BOOL CPConfig_GetPoint(const char* section, const char* keyPrefix,
                       POINT* pPoint, const POINT* pDefault);

// Set POINT value
void CPConfig_SetPoint(const char* section, const char* keyPrefix, const POINT* pPoint);

////////////////////////////////////////////////////////////////////////////////
// Color Values
////////////////////////////////////////////////////////////////////////////////

// Get COLORREF value (stored as RGB hex or decimal)
COLORREF CPConfig_GetColor(const char* section, const char* key, COLORREF defaultValue);

// Set COLORREF value
void CPConfig_SetColor(const char* section, const char* key, COLORREF value);

////////////////////////////////////////////////////////////////////////////////
// Array/List Values
////////////////////////////////////////////////////////////////////////////////

// Get array of integers
// Returns number of values read
int CPConfig_GetIntArray(const char* section, const char* keyPrefix,
                         int* values, int maxCount, int defaultValue);

// Set array of integers
void CPConfig_SetIntArray(const char* section, const char* keyPrefix,
                          const int* values, int count);

////////////////////////////////////////////////////////////////////////////////
// Section Enumeration
////////////////////////////////////////////////////////////////////////////////

// Callback for enumerating keys in a section
typedef void (*CPConfig_KeyCallback)(const char* key, const char* value, void* userData);

// Enumerate all keys in a section
void CPConfig_EnumerateKeys(const char* section, CPConfig_KeyCallback callback, void* userData);

// Delete a key
void CPConfig_DeleteKey(const char* section, const char* key);

// Delete an entire section
void CPConfig_DeleteSection(const char* section);

////////////////////////////////////////////////////////////////////////////////
// Convenience Macros
////////////////////////////////////////////////////////////////////////////////

// Read config value into struct member with same name as key
#define CPCONFIG_READ_INT(section, member, def) \
    options.member = CPConfig_GetInt(section, #member, def)

#define CPCONFIG_READ_BOOL(section, member, def) \
    options.member = CPConfig_GetBool(section, #member, def)

#define CPCONFIG_READ_STRING(section, member, def) \
    CPConfig_GetString(section, #member, def, (char*)options.member, sizeof(options.member))

// Write config value from struct member with same name as key
#define CPCONFIG_WRITE_INT(section, member) \
    CPConfig_SetInt(section, #member, options.member)

#define CPCONFIG_WRITE_BOOL(section, member) \
    CPConfig_SetBool(section, #member, options.member)

#define CPCONFIG_WRITE_STRING(section, member) \
    CPConfig_SetString(section, #member, (const char*)options.member)

////////////////////////////////////////////////////////////////////////////////

#endif // CP_CONFIG_H
