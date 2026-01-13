/*
 * CoolPlayer - Blazing fast audio player.
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

#ifndef SAFE_STRING_H
#define SAFE_STRING_H

////////////////////////////////////////////////////////////////////////////////
//
// Safe String Functions
// Cross-platform safe string handling with buffer overflow protection
//
// This header provides safe alternatives to dangerous C string functions:
//   - cp_strcpy_s()  instead of strcpy/lstrcpy
//   - cp_strcat_s()  instead of strcat/lstrcat
//   - cp_strncpy_s() instead of strncpy
//   - cp_snprintf()  instead of sprintf
//
// All functions include buffer size checks and null-termination guarantees.
//
////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <string.h>
#include <wchar.h>  // For wcslen, wmemcpy

// For MSVC, we can use the _s variants directly
// For MinGW/GCC, we provide compatible implementations
#ifdef _MSC_VER
    // MSVC has secure CRT functions
    #include <errno.h>
    
    // Use MSVC's built-in secure functions
    #define cp_strcpy_s(dest, dest_size, src)    strcpy_s(dest, dest_size, src)
    #define cp_strcat_s(dest, dest_size, src)    strcat_s(dest, dest_size, src)
    #define cp_strncpy_s(dest, dest_size, src, count)  strncpy_s(dest, dest_size, src, count)
    #define cp_snprintf(dest, dest_size, ...)    _snprintf_s(dest, dest_size, _TRUNCATE, __VA_ARGS__)
    #define cp_wcsncpy_s(dest, dest_size, src, count)  wcsncpy_s(dest, dest_size, src, count)

#else
    // MinGW/GCC - provide safe implementations

////////////////////////////////////////////////////////////////////////////////
// Error codes (C11 Annex K compatible)
#ifndef EINVAL
    #define EINVAL 22
#endif
#ifndef ERANGE
    #define ERANGE 34
#endif

////////////////////////////////////////////////////////////////////////////////
// cp_strcpy_s - Safe string copy
// Returns 0 on success, non-zero on error
// Always null-terminates dest (if dest_size > 0)

static inline int cp_strcpy_s(char* dest, size_t dest_size, const char* src)
{
    if (dest == NULL) {
        return EINVAL;
    }
    if (dest_size == 0) {
        return ERANGE;
    }
    if (src == NULL) {
        dest[0] = '\0';
        return EINVAL;
    }
    
    size_t src_len = strlen(src);
    if (src_len >= dest_size) {
        // Source too long - truncate and null-terminate
        dest[0] = '\0';
        return ERANGE;
    }
    
    memcpy(dest, src, src_len + 1);  // Include null terminator
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// cp_strcat_s - Safe string concatenation
// Returns 0 on success, non-zero on error
// Always null-terminates dest (if dest_size > 0)

static inline int cp_strcat_s(char* dest, size_t dest_size, const char* src)
{
    if (dest == NULL) {
        return EINVAL;
    }
    if (dest_size == 0) {
        return ERANGE;
    }
    if (src == NULL) {
        return EINVAL;
    }
    
    size_t dest_len = strlen(dest);
    size_t src_len = strlen(src);
    
    if (dest_len + src_len >= dest_size) {
        // Combined length too long - leave dest unchanged
        return ERANGE;
    }
    
    memcpy(dest + dest_len, src, src_len + 1);  // Include null terminator
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// cp_strncpy_s - Safe string copy with count
// Returns 0 on success, non-zero on error
// Always null-terminates dest (if dest_size > 0)

static inline int cp_strncpy_s(char* dest, size_t dest_size, const char* src, size_t count)
{
    if (dest == NULL) {
        return EINVAL;
    }
    if (dest_size == 0) {
        return ERANGE;
    }
    if (src == NULL) {
        dest[0] = '\0';
        return EINVAL;
    }
    
    // Determine how many characters to copy
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < count) ? src_len : count;
    
    if (copy_len >= dest_size) {
        // Source too long for buffer - truncate to fit
        copy_len = dest_size - 1;
    }
    
    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// cp_snprintf - Safe sprintf with buffer size
// Returns number of characters written (not including null terminator)
// or negative value on error

#include <stdarg.h>
#include <stdio.h>

static inline int cp_snprintf(char* dest, size_t dest_size, const char* format, ...)
{
    if (dest == NULL || dest_size == 0) {
        return -1;
    }
    if (format == NULL) {
        dest[0] = '\0';
        return -1;
    }
    
    va_list args;
    va_start(args, format);
    int result = vsnprintf(dest, dest_size, format, args);
    va_end(args);
    
    // Ensure null-termination (vsnprintf does this, but be safe)
    if (result >= 0 && (size_t)result >= dest_size) {
        dest[dest_size - 1] = '\0';
    }
    
    return result;
}

////////////////////////////////////////////////////////////////////////////////
// cp_wcsncpy_s - Safe wide string copy with count

static inline int cp_wcsncpy_s(wchar_t* dest, size_t dest_size, const wchar_t* src, size_t count)
{
    if (dest == NULL) {
        return EINVAL;
    }
    if (dest_size == 0) {
        return ERANGE;
    }
    if (src == NULL) {
        dest[0] = L'\0';
        return EINVAL;
    }
    
    // Determine how many characters to copy
    size_t src_len = wcslen(src);
    size_t copy_len = (src_len < count) ? src_len : count;
    
    if (copy_len >= dest_size) {
        // Source too long for buffer - truncate to fit
        copy_len = dest_size - 1;
    }
    
    wmemcpy(dest, src, copy_len);
    dest[copy_len] = L'\0';
    return 0;
}

#endif // _MSC_VER

////////////////////////////////////////////////////////////////////////////////
// Convenience macros for common patterns

// Copy string literal into fixed-size buffer
#define CP_STRCPY(dest, src)    cp_strcpy_s(dest, sizeof(dest), src)

// Concatenate string to fixed-size buffer
#define CP_STRCAT(dest, src)    cp_strcat_s(dest, sizeof(dest), src)

// Copy string with length limit into fixed-size buffer
#define CP_STRNCPY(dest, src, n)  cp_strncpy_s(dest, sizeof(dest), src, n)

// Safe sprintf into fixed-size buffer
#define CP_SPRINTF(dest, ...)     cp_snprintf(dest, sizeof(dest), __VA_ARGS__)

////////////////////////////////////////////////////////////////////////////////
// String length with maximum (safer than strlen for untrusted input)

static inline size_t cp_strnlen(const char* str, size_t max_len)
{
    if (str == NULL) {
        return 0;
    }
    size_t len = 0;
    while (len < max_len && str[len] != '\0') {
        len++;
    }
    return len;
}

////////////////////////////////////////////////////////////////////////////////

#endif // SAFE_STRING_H
