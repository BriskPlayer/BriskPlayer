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

#ifndef CP_RESULT_H
#define CP_RESULT_H

////////////////////////////////////////////////////////////////////////////////
//
// Standardized Result/Error Code System for BriskPlayer
//
// This module provides a consistent error handling pattern across the codebase.
// All functions should return CP_Result where possible.
//
// Usage:
//   CP_Result result = MyFunction();
//   if (CP_FAILED(result)) {
//       CP_LOG_ERROR("MyFunction failed: %s", CP_ResultToString(result));
//       return result;
//   }
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>

////////////////////////////////////////////////////////////////////////////////
// Result Code Type
////////////////////////////////////////////////////////////////////////////////

typedef int CP_Result;

////////////////////////////////////////////////////////////////////////////////
// Result Code Definitions
//
// Codes are organized by category:
//   0         = Success
//   1-99      = Warnings (operation succeeded with caveats)
//   -1 to -99 = General errors
//   -100+     = Category-specific errors
////////////////////////////////////////////////////////////////////////////////

// Success codes
#define CP_OK                           0       // Operation succeeded
#define CP_SUCCESS                      0       // Alias for CP_OK
#define CP_TRUE                         1       // Boolean true result

// Warning codes (positive, non-zero)
#define CP_WARN_ALREADY_DONE            1       // Operation already completed
#define CP_WARN_PARTIAL                 2       // Partial success
#define CP_WARN_EMPTY                   3       // Empty result (not an error)
#define CP_WARN_TRUNCATED               4       // Result was truncated
#define CP_WARN_DEFAULT_USED            5       // Default value was used

// General errors (-1 to -99)
#define CP_ERROR                        (-1)    // Generic error
#define CP_ERROR_INVALID_PARAM          (-2)    // Invalid parameter
#define CP_ERROR_NULL_POINTER           (-3)    // NULL pointer passed
#define CP_ERROR_OUT_OF_MEMORY          (-4)    // Memory allocation failed
#define CP_ERROR_NOT_INITIALIZED        (-5)    // Module not initialized
#define CP_ERROR_ALREADY_INITIALIZED    (-6)    // Already initialized
#define CP_ERROR_NOT_SUPPORTED          (-7)    // Operation not supported
#define CP_ERROR_NOT_FOUND              (-8)    // Item not found
#define CP_ERROR_ALREADY_EXISTS         (-9)    // Item already exists
#define CP_ERROR_BUFFER_TOO_SMALL       (-10)   // Buffer too small
#define CP_ERROR_TIMEOUT                (-11)   // Operation timed out
#define CP_ERROR_CANCELLED              (-12)   // Operation was cancelled
#define CP_ERROR_BUSY                   (-13)   // Resource is busy
#define CP_ERROR_INVALID_STATE          (-14)   // Invalid state for operation
#define CP_ERROR_OVERFLOW               (-15)   // Overflow occurred
#define CP_ERROR_UNDERFLOW              (-16)   // Underflow occurred

// File/IO errors (-100 to -149)
#define CP_ERROR_FILE_NOT_FOUND         (-100)  // File does not exist
#define CP_ERROR_FILE_OPEN              (-101)  // Cannot open file
#define CP_ERROR_FILE_READ              (-102)  // Read error
#define CP_ERROR_FILE_WRITE             (-103)  // Write error
#define CP_ERROR_FILE_SEEK              (-104)  // Seek error
#define CP_ERROR_FILE_FORMAT            (-105)  // Invalid file format
#define CP_ERROR_FILE_CORRUPT           (-106)  // File is corrupted
#define CP_ERROR_FILE_ACCESS            (-107)  // Access denied
#define CP_ERROR_FILE_LOCKED            (-108)  // File is locked
#define CP_ERROR_PATH_NOT_FOUND         (-109)  // Path does not exist
#define CP_ERROR_PATH_TOO_LONG          (-110)  // Path exceeds MAX_PATH

// Audio/Codec errors (-150 to -199)
#define CP_ERROR_CODEC_NOT_FOUND        (-150)  // No codec for format
#define CP_ERROR_CODEC_INIT             (-151)  // Codec initialization failed
#define CP_ERROR_CODEC_DECODE           (-152)  // Decoding error
#define CP_ERROR_CODEC_UNSUPPORTED      (-153)  // Unsupported codec feature
#define CP_ERROR_AUDIO_DEVICE           (-154)  // Audio device error
#define CP_ERROR_AUDIO_FORMAT           (-155)  // Unsupported audio format
#define CP_ERROR_AUDIO_BUFFER           (-156)  // Buffer error
#define CP_ERROR_AUDIO_PLAYBACK         (-157)  // Playback error

// Network errors (-200 to -249)
#define CP_ERROR_NETWORK                (-200)  // Generic network error
#define CP_ERROR_NETWORK_CONNECT        (-201)  // Connection failed
#define CP_ERROR_NETWORK_TIMEOUT        (-202)  // Network timeout
#define CP_ERROR_NETWORK_DNS            (-203)  // DNS resolution failed
#define CP_ERROR_NETWORK_PROTOCOL       (-204)  // Protocol error
#define CP_ERROR_NETWORK_SSL            (-205)  // SSL/TLS error
#define CP_ERROR_NETWORK_AUTH           (-206)  // Authentication failed
#define CP_ERROR_NETWORK_REDIRECT       (-207)  // Too many redirects
#define CP_ERROR_HTTP_ERROR             (-208)  // HTTP error response
#define CP_ERROR_STREAM_EOF             (-209)  // End of stream

// Playlist errors (-250 to -299)
#define CP_ERROR_PLAYLIST_EMPTY         (-250)  // Playlist is empty
#define CP_ERROR_PLAYLIST_FULL          (-251)  // Playlist is full
#define CP_ERROR_PLAYLIST_INVALID       (-252)  // Invalid playlist format
#define CP_ERROR_PLAYLIST_ITEM          (-253)  // Invalid playlist item

// UI/Window errors (-300 to -349)
#define CP_ERROR_WINDOW_CREATE          (-300)  // Window creation failed
#define CP_ERROR_WINDOW_NOT_FOUND       (-301)  // Window not found
#define CP_ERROR_GDI_ERROR              (-302)  // GDI operation failed
#define CP_ERROR_RESOURCE_NOT_FOUND     (-303)  // Resource not found
#define CP_ERROR_MENU_ERROR             (-304)  // Menu operation failed

// Skin errors (-350 to -399)
#define CP_ERROR_SKIN_NOT_FOUND         (-350)  // Skin file not found
#define CP_ERROR_SKIN_INVALID           (-351)  // Invalid skin format
#define CP_ERROR_SKIN_MISSING_BITMAP    (-352)  // Missing skin bitmap
#define CP_ERROR_SKIN_PARSE             (-353)  // Skin parsing error

// Configuration errors (-400 to -449)
#define CP_ERROR_CONFIG_READ            (-400)  // Config read error
#define CP_ERROR_CONFIG_WRITE           (-401)  // Config write error
#define CP_ERROR_CONFIG_INVALID         (-402)  // Invalid config value
#define CP_ERROR_CONFIG_MISSING         (-403)  // Missing config key

////////////////////////////////////////////////////////////////////////////////
// Result Checking Macros
////////////////////////////////////////////////////////////////////////////////

// Check if result indicates success (>= 0)
#define CP_SUCCEEDED(result) ((result) >= 0)

// Check if result indicates failure (< 0)
#define CP_FAILED(result) ((result) < 0)

// Check if result is exactly success
#define CP_IS_OK(result) ((result) == CP_OK)

// Check if result is a warning (> 0)
#define CP_IS_WARNING(result) ((result) > 0)

////////////////////////////////////////////////////////////////////////////////
// Error Propagation Macros
////////////////////////////////////////////////////////////////////////////////

// Return on failure
#define CP_RETURN_IF_FAILED(expr) do { \
    CP_Result _result = (expr); \
    if (CP_FAILED(_result)) { \
        return _result; \
    } \
} while(0)

// Goto cleanup on failure
#define CP_GOTO_IF_FAILED(expr, label) do { \
    result = (expr); \
    if (CP_FAILED(result)) { \
        goto label; \
    } \
} while(0)

// Return specific error if condition fails
#define CP_RETURN_IF_NULL(ptr, error_code) do { \
    if ((ptr) == NULL) { \
        return (error_code); \
    } \
} while(0)

// Log and return on failure
#define CP_LOG_RETURN_IF_FAILED(expr, msg) do { \
    CP_Result _result = (expr); \
    if (CP_FAILED(_result)) { \
        CP_LOG_ERROR("%s (error: %d)", (msg), _result); \
        return _result; \
    } \
} while(0)

////////////////////////////////////////////////////////////////////////////////
// Result to String Conversion
////////////////////////////////////////////////////////////////////////////////

// Convert result code to human-readable string
static inline const char* CP_ResultToString(CP_Result result)
{
    switch (result)
    {
        // Success/Warnings
        case CP_OK:                         return "Success";
        case CP_WARN_ALREADY_DONE:          return "Already done";
        case CP_WARN_PARTIAL:               return "Partial success";
        case CP_WARN_EMPTY:                 return "Empty result";
        case CP_WARN_TRUNCATED:             return "Result truncated";
        case CP_WARN_DEFAULT_USED:          return "Default value used";
        
        // General errors
        case CP_ERROR:                      return "Generic error";
        case CP_ERROR_INVALID_PARAM:        return "Invalid parameter";
        case CP_ERROR_NULL_POINTER:         return "Null pointer";
        case CP_ERROR_OUT_OF_MEMORY:        return "Out of memory";
        case CP_ERROR_NOT_INITIALIZED:      return "Not initialized";
        case CP_ERROR_ALREADY_INITIALIZED:  return "Already initialized";
        case CP_ERROR_NOT_SUPPORTED:        return "Not supported";
        case CP_ERROR_NOT_FOUND:            return "Not found";
        case CP_ERROR_ALREADY_EXISTS:       return "Already exists";
        case CP_ERROR_BUFFER_TOO_SMALL:     return "Buffer too small";
        case CP_ERROR_TIMEOUT:              return "Timeout";
        case CP_ERROR_CANCELLED:            return "Cancelled";
        case CP_ERROR_BUSY:                 return "Busy";
        case CP_ERROR_INVALID_STATE:        return "Invalid state";
        
        // File errors
        case CP_ERROR_FILE_NOT_FOUND:       return "File not found";
        case CP_ERROR_FILE_OPEN:            return "Cannot open file";
        case CP_ERROR_FILE_READ:            return "Read error";
        case CP_ERROR_FILE_WRITE:           return "Write error";
        case CP_ERROR_FILE_FORMAT:          return "Invalid file format";
        case CP_ERROR_FILE_CORRUPT:         return "File corrupted";
        case CP_ERROR_FILE_ACCESS:          return "Access denied";
        
        // Audio errors
        case CP_ERROR_CODEC_NOT_FOUND:      return "Codec not found";
        case CP_ERROR_CODEC_INIT:           return "Codec init failed";
        case CP_ERROR_CODEC_DECODE:         return "Decode error";
        case CP_ERROR_AUDIO_DEVICE:         return "Audio device error";
        case CP_ERROR_AUDIO_FORMAT:         return "Unsupported format";
        
        // Network errors
        case CP_ERROR_NETWORK:              return "Network error";
        case CP_ERROR_NETWORK_CONNECT:      return "Connection failed";
        case CP_ERROR_NETWORK_TIMEOUT:      return "Network timeout";
        case CP_ERROR_NETWORK_DNS:          return "DNS lookup failed";
        case CP_ERROR_HTTP_ERROR:           return "HTTP error";
        case CP_ERROR_STREAM_EOF:           return "End of stream";
        
        // Playlist errors
        case CP_ERROR_PLAYLIST_EMPTY:       return "Playlist empty";
        case CP_ERROR_PLAYLIST_FULL:        return "Playlist full";
        case CP_ERROR_PLAYLIST_INVALID:     return "Invalid playlist";
        
        // UI errors
        case CP_ERROR_WINDOW_CREATE:        return "Window creation failed";
        case CP_ERROR_GDI_ERROR:            return "GDI error";
        case CP_ERROR_RESOURCE_NOT_FOUND:   return "Resource not found";
        
        // Skin errors
        case CP_ERROR_SKIN_NOT_FOUND:       return "Skin not found";
        case CP_ERROR_SKIN_INVALID:         return "Invalid skin";
        case CP_ERROR_SKIN_MISSING_BITMAP:  return "Missing skin bitmap";
        
        // Config errors
        case CP_ERROR_CONFIG_READ:          return "Config read error";
        case CP_ERROR_CONFIG_WRITE:         return "Config write error";
        case CP_ERROR_CONFIG_INVALID:       return "Invalid config";
        
        default:
            if (result > 0) return "Warning";
            return "Unknown error";
    }
}

////////////////////////////////////////////////////////////////////////////////
// Windows Error Conversion
////////////////////////////////////////////////////////////////////////////////

// Convert Windows GetLastError() to CP_Result
static inline CP_Result CP_ResultFromWin32(DWORD dwError)
{
    switch (dwError)
    {
        case ERROR_SUCCESS:             return CP_OK;
        case ERROR_FILE_NOT_FOUND:      return CP_ERROR_FILE_NOT_FOUND;
        case ERROR_PATH_NOT_FOUND:      return CP_ERROR_PATH_NOT_FOUND;
        case ERROR_ACCESS_DENIED:       return CP_ERROR_FILE_ACCESS;
        case ERROR_INVALID_HANDLE:      return CP_ERROR_INVALID_PARAM;
        case ERROR_NOT_ENOUGH_MEMORY:   return CP_ERROR_OUT_OF_MEMORY;
        case ERROR_OUTOFMEMORY:         return CP_ERROR_OUT_OF_MEMORY;
        case ERROR_INVALID_PARAMETER:   return CP_ERROR_INVALID_PARAM;
        case ERROR_SHARING_VIOLATION:   return CP_ERROR_FILE_LOCKED;
        case ERROR_LOCK_VIOLATION:      return CP_ERROR_FILE_LOCKED;
        case ERROR_ALREADY_EXISTS:      return CP_ERROR_ALREADY_EXISTS;
        case ERROR_FILE_EXISTS:         return CP_ERROR_ALREADY_EXISTS;
        case ERROR_TIMEOUT:             return CP_ERROR_TIMEOUT;
        case ERROR_CANCELLED:           return CP_ERROR_CANCELLED;
        case ERROR_NOT_SUPPORTED:       return CP_ERROR_NOT_SUPPORTED;
        default:                        return CP_ERROR;
    }
}

// Convert last Windows error
#define CP_ResultFromLastError() CP_ResultFromWin32(GetLastError())

////////////////////////////////////////////////////////////////////////////////

#endif // CP_RESULT_H
