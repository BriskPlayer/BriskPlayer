#ifndef STDAFX_H
#define STDAFX_H

//#define _WIN32_WINDOWS 0x0410
#define WIN32_LEAN_AND_MEAN

//#define _WIN32_IE 0x600

// Windows 7 as minimum version (ITaskbarList3 requires Win7+)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include "debug.h"
#include "c23_compat.h"
#include "safe_string.h"  // Safe string functions (cp_strcpy_s, cp_strcat_s, etc.)
#include "CP_Constants.h" // Centralized magic number constants
#include "CP_Cleanup.h"   // Resource cleanup helper macros
#include "CP_Result.h"    // Standardized error codes
#include "CP_Unicode.h"   // Unicode handling utilities
#include <process.h>
#include <wininet.h>
// #include <search.h>
#include <malloc.h>
#include <time.h>
#include <shlobj.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <io.h>

// sprintf is NOT safe - all code must use snprintf(buf, sizeof(buf), ...)
// The unsafe cp_sprintf_wrapper has been removed. Any remaining sprintf usage
// will use the CRT function. For new code, always use snprintf() or cp_snprintf().

// String function macros - legacy compatibility layer
// NOTE: These macros map to Windows lstr* functions for compatibility.
// For new code, prefer the safe string functions from safe_string.h:
//   - cp_strcpy_s(dest, dest_size, src) instead of strcpy/lstrcpy
//   - cp_strcat_s(dest, dest_size, src) instead of strcat/lstrcat
//   - cp_snprintf(dest, dest_size, ...) instead of sprintf
//   - CP_STRCPY(dest, src) macro for fixed-size buffers
#define strcpy lstrcpy
#define strcmp lstrcmp
#define strcat lstrcat
#define stricmp lstrcmpi
#define strlen lstrlen
#define strncpy lstrcpyn

int __cdecl  memcmp(const void*, const void*, size_t);
void* __cdecl  memcpy(void*, const void*, size_t);
void* __cdecl memset(void*, int, size_t);
#ifdef __MINGW32__
void* __cdecl memmove(void*, const void*, size_t);
char* __cdecl strchr(const char*, int) ;
char* __cdecl strrchr(const char*, int) ;
char* __cdecl strstr(const char*, const char*);
// int __cdecl _stricmp(const char*, const char*); // Commented out to avoid dllimport conflict
// int __cdecl _strnicmp(const char*, const char*, size_t); // Commented out to avoid dllimport conflict
// int __cdecl tolower(int); // Commented out to avoid dllimport conflict
#endif


#ifndef OFN_ENABLESIZING
#define OFN_ENABLESIZING             0x00800000
#endif

#ifndef IRF_NO_WAIT
#define IRF_NO_WAIT     0x00000008
#endif

#ifndef IDC_STATIC  /* May be predefined by resource compiler.  */
#define IDC_STATIC (-1)
#endif

#endif /* STDAFX_H */
