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

#ifndef DEBUG_H
#define DEBUG_H

// Required includes for logging functions
#include <stdarg.h>
#include <stdio.h>

// Windows-specific includes for OutputDebugString
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

#if defined _DEBUG && !defined (_WIN64)

#ifndef _MSC_VER

int _CrtDbgReport(
	int reportType,
	const char *filename,
	int linenumber,
	const char *moduleName,
	const char *lpszFormat ,
	...
)
{
	char szBuffer[1024];
	va_list args;
	va_start(args, lpszFormat);
	_vsnprintf(szBuffer, sizeof(szBuffer) - 1, lpszFormat, args);
	szBuffer[sizeof(szBuffer) - 1] = '\0';
	OutputDebugString(szBuffer);
	va_end(args);
	return 1;
}

#else

#include <crtdbg.h>

#endif

#define CP_TRACE0(format) _CrtDbgReport(_CRT_WARN, __FILE__, __LINE__, NULL, format "\n")
#define CP_TRACE1(format, arg1) _CrtDbgReport(_CRT_WARN, __FILE__, __LINE__, NULL, format "\n", arg1)
#define CP_TRACE2(format, arg1, arg2) _CrtDbgReport(_CRT_WARN, __FILE__, __LINE__, NULL, format "\n", arg1, arg2)
#define CP_TRACE3(format, arg1, arg2, arg3) _CrtDbgReport(_CRT_WARN, __FILE__, __LINE__, NULL, format "\n", arg1, arg2, arg3)
#define CP_TRACE4(format, arg1, arg2, arg3, arg4) _CrtDbgReport(_CRT_WARN, __FILE__, __LINE__, NULL, format "\n", arg1, arg2, arg3, arg4)
#define CP_TRACE5(format, arg1, arg2, arg3, arg4, arg5) _CrtDbgReport(_CRT_WARN, __FILE__, __LINE__, NULL, format "\n", arg1, arg2, arg3, arg4, arg5)
//
#define CP_ASSERT(expr) if(!(expr)) { CP_TRACE1("ASSERTION %s FAILS", #expr); DebugBreak(); }
#define CP_FAIL(errstring) { CP_TRACE1("HARD FAILURE %s", #errstring); DebugBreak(); }
//
#define CP_CHECKOBJECT(obj_ptr_typed) if(!obj_ptr_typed) { CP_TRACE1("POINTER %s is NULL", #obj_ptr_typed); }




#else

// Release build: no-op macros using do-while(0) idiom for statement safety
// The (void) casts prevent unused variable warnings
#define CP_CHECKOBJECT(obj_ptr_typed) do { (void)(obj_ptr_typed); } while(0)
#define CP_ASSERT(expr)               do { (void)(expr); } while(0)
#define CP_FAIL(expr)                 do { (void)(expr); } while(0)
#define CP_TRACE0(f)                  do { (void)(f); } while(0)
#define CP_TRACE1(f, e1)              do { (void)(f); (void)(e1); } while(0)
#define CP_TRACE2(f, e1, e2)          do { (void)(f); (void)(e1); (void)(e2); } while(0)
#define CP_TRACE3(f, e1, e2, e3)      do { (void)(f); (void)(e1); (void)(e2); (void)(e3); } while(0)
#define CP_TRACE4(f, e1, e2, e3, e4)  do { (void)(f); (void)(e1); (void)(e2); (void)(e3); (void)(e4); } while(0)
#define CP_TRACE5(f, e1, e2, e3, e4, e5) do { (void)(f); (void)(e1); (void)(e2); (void)(e3); (void)(e4); (void)(e5); } while(0)

#endif

////////////////////////////////////////////////////////////////////////////////
// Runtime Logging System
// These macros provide configurable logging that can be enabled/disabled at runtime
// Use CP_LOG for general logging, CP_LOG_DEBUG for debug-only logging
////////////////////////////////////////////////////////////////////////////////

// Log levels
#define CP_LOG_LEVEL_NONE    0
#define CP_LOG_LEVEL_ERROR   1
#define CP_LOG_LEVEL_WARNING 2
#define CP_LOG_LEVEL_INFO    3
#define CP_LOG_LEVEL_DEBUG   4
#define CP_LOG_LEVEL_VERBOSE 5

// Default log level - can be overridden before including this header
#ifndef CP_DEFAULT_LOG_LEVEL
    #ifdef _DEBUG
        #define CP_DEFAULT_LOG_LEVEL CP_LOG_LEVEL_DEBUG
    #else
        #define CP_DEFAULT_LOG_LEVEL CP_LOG_LEVEL_WARNING
    #endif
#endif

// Global log level variables - defined in globals.c
extern int g_cp_log_level;
extern int g_cp_log_to_console;

// Internal logging implementation
static inline void cp_log_impl(int level, const char* file, int line, const char* format, ...)
{
    if (level > g_cp_log_level) return;
    
    char buffer[1024];
    char prefix[64];
    const char* level_str = "";
    va_list args;
    
    switch (level) {
        case CP_LOG_LEVEL_ERROR:   level_str = "ERROR"; break;
        case CP_LOG_LEVEL_WARNING: level_str = "WARN "; break;
        case CP_LOG_LEVEL_INFO:    level_str = "INFO "; break;
        case CP_LOG_LEVEL_DEBUG:   level_str = "DEBUG"; break;
        case CP_LOG_LEVEL_VERBOSE: level_str = "VERB "; break;
    }
    
    // Extract just the filename from the path
    const char* filename = file;
    const char* p = file;
    while (*p) {
        if (*p == '\\' || *p == '/') filename = p + 1;
        p++;
    }
    
    snprintf(prefix, sizeof(prefix), "[%s] %s:%d: ", level_str, filename, line);
    
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // Output to debug console
    OutputDebugStringA(prefix);
    OutputDebugStringA(buffer);
    
    // Optionally output to console (useful for debugging)
    if (g_cp_log_to_console) {
        fprintf(stderr, "%s%s", prefix, buffer);
    }
}

// Public logging macros
#define CP_LOG_ERROR(fmt, ...)   cp_log_impl(CP_LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define CP_LOG_WARNING(fmt, ...) cp_log_impl(CP_LOG_LEVEL_WARNING, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define CP_LOG_INFO(fmt, ...)    cp_log_impl(CP_LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define CP_LOG_DEBUG(fmt, ...)   cp_log_impl(CP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define CP_LOG_VERBOSE(fmt, ...) cp_log_impl(CP_LOG_LEVEL_VERBOSE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

// Convenience macro - maps to INFO level
#define CP_LOG(fmt, ...) CP_LOG_INFO(fmt, ##__VA_ARGS__)

////////////////////////////////////////////////////////////////////////////////
// Error Handling Helpers
//
// Use these macros with a 'goto cleanup' pattern for consistent error handling.
//
// Example usage:
//   BOOL MyFunction(void) {
//       BOOL result = FALSE;
//       void* resource1 = NULL;
//       void* resource2 = NULL;
//       
//       resource1 = malloc(100);
//       CP_CHECK_GOTO(resource1 != NULL, cleanup, "Failed to allocate resource1");
//       
//       resource2 = malloc(200);
//       CP_CHECK_GOTO(resource2 != NULL, cleanup, "Failed to allocate resource2");
//       
//       // Success path
//       result = TRUE;
//       
//   cleanup:
//       if (!result) {
//           free(resource1);  // Safe - free(NULL) is a no-op
//           free(resource2);
//       }
//       return result;
//   }
//

// Check condition and goto label on failure with error message
#define CP_CHECK_GOTO(cond, label, msg) \
    do { \
        if (!(cond)) { \
            CP_LOG_ERROR("%s\n", msg); \
            goto label; \
        } \
    } while (0)

// Check condition and goto label on failure with formatted error message
#define CP_CHECK_GOTO_FMT(cond, label, fmt, ...) \
    do { \
        if (!(cond)) { \
            CP_LOG_ERROR(fmt, ##__VA_ARGS__); \
            goto label; \
        } \
    } while (0)

// Check condition and return on failure with error message
#define CP_CHECK_RETURN(cond, retval, msg) \
    do { \
        if (!(cond)) { \
            CP_LOG_ERROR("%s\n", msg); \
            return (retval); \
        } \
    } while (0)

// Check condition and return on failure with formatted error message  
#define CP_CHECK_RETURN_FMT(cond, retval, fmt, ...) \
    do { \
        if (!(cond)) { \
            CP_LOG_ERROR(fmt, ##__VA_ARGS__); \
            return (retval); \
        } \
    } while (0)

#endif // DEBUG_H