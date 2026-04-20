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

HMODULE LoadLibrarySafeA(const char* pcDllPath)
{
	char szDir[MAX_PATH];
	char* pSlash;
	HMODULE hMod;

	cp_strcpy_s(szDir, sizeof(szDir), pcDllPath);
	pSlash = strrchr(szDir, '\\');
	if (!pSlash) pSlash = strrchr(szDir, '/');
	if (pSlash) *pSlash = '\0';

	SetDllDirectoryA(szDir);
	hMod = LoadLibraryA(pcDllPath);
	SetDllDirectoryA("");
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
