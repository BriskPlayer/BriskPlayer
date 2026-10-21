#include "stdafx.h"
#include "globals.h"

// Define log level globals (declared as extern in debug.h)
int g_cp_log_level = CP_DEFAULT_LOG_LEVEL;
int g_cp_log_to_console = 0;  // Set to 1 to enable console output

// Global variable definitions
graphics_t graphics;
windows_t windows;
drawables_t drawables;
PlayListBitmap_t PlayListBitmap;
options_t options;
globals_t globals;
CoolSkin Skin;

#ifndef LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR
#define LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR 0x00000100
#endif
#ifndef LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
#define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS 0x00001000
#endif

// Loads a plugin DLL by absolute path. Its own dependencies resolve from the
// plugin's folder, System32 and the app folder - never the current directory.
HMODULE LoadLibrarySafeA(const char* pcDllPath)
{
	WCHAR* pwcPath;
	HMODULE hMod;
	int iChars;

	if (!pcDllPath)
		return NULL;
	iChars = MultiByteToWideChar(CP_ACP, 0, pcDllPath, -1, NULL, 0);
	if (iChars <= 0)
		return NULL;
	pwcPath = (WCHAR*)malloc((size_t)iChars * sizeof(WCHAR));
	if (!pwcPath)
		return NULL;
	MultiByteToWideChar(CP_ACP, 0, pcDllPath, -1, pwcPath, iChars);

	hMod = LoadLibraryExW(pwcPath, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	// Windows 7 without KB2533623 rejects the LOAD_LIBRARY_SEARCH_* flags
	if (!hMod && GetLastError() == ERROR_INVALID_PARAMETER)
		hMod = LoadLibraryExW(pwcPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);

	free(pwcPath);
	return hMod;
}

DWORD main_get_program_file_path(const char* pcFilename, char* pszBuffer, DWORD dwSize)
{
	DWORD len = main_get_program_path(GetModuleHandle(NULL), pszBuffer, dwSize);
	if (len > 0)
		cp_strcat_s(pszBuffer, dwSize, pcFilename);
	return len;
}

BOOL CP_IsURL(const char* pcPath)
{
	if (!pcPath) return FALSE;
	return _strnicmp(pcPath, CIC_HTTPHEADER, sizeof(CIC_HTTPHEADER) - 1) == 0
		|| _strnicmp(pcPath, CIC_HTTPSHEADER, sizeof(CIC_HTTPSHEADER) - 1) == 0
		|| _strnicmp(pcPath, CIC_ICYHEADER, sizeof(CIC_ICYHEADER) - 1) == 0
		|| _strnicmp(pcPath, CIC_FTPHEADER, sizeof(CIC_FTPHEADER) - 1) == 0;
}
