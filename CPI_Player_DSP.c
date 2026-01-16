/*
 * BriskPlayer - Blazing fast audio player.
 * Copyright (C) 2000-2001 Niek Albers
 * Copyright (C) 2025-2026 Zach Bacon
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
// WinAmp DSP Plugin Support Implementation
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "globals.h"
#include "CPI_Player_DSP.h"
#include "CP_WinAmpStructs.h"
#include "CPI_Gettext.h"

////////////////////////////////////////////////////////////////////////////////
// Module State
////////////////////////////////////////////////////////////////////////////////

typedef struct
{
    // Parent window for plugin dialogs
    HWND m_hWndParent;
    
    // Linked list of discovered plugins
    CPs_DSPPluginInfo* m_pFirstPlugin;
    int m_iPluginCount;
    
    // Currently active plugin
    HMODULE m_hActiveModule;
    winampDSPHeader* m_pActiveHeader;
    winampDSPModule* m_pActiveModule;
    int m_iActivePluginIndex;
    
    // Initialized flag
    BOOL m_bInitialized;
    
} CPs_DSPModuleState;

static CPs_DSPModuleState g_DSPState = {0};

////////////////////////////////////////////////////////////////////////////////
// Internal Functions - Forward Declarations
////////////////////////////////////////////////////////////////////////////////

static void CPDSP_FreePluginList(void);
static void CPDSP_ProbePlugin(const char* pcPluginPath);
static void CPDSP_DeactivateCurrentPlugin(void);
static BOOL CALLBACK CPDSP_EnumWindowsCallback(HWND hwnd, LPARAM lParam);

////////////////////////////////////////////////////////////////////////////////
// Helper: Enum windows callback to find and show plugin windows
////////////////////////////////////////////////////////////////////////////////

static BOOL CALLBACK CPDSP_EnumWindowsCallback(HWND hwnd, LPARAM lParam)
{
    HMODULE hPluginModule = (HMODULE)lParam;
    DWORD dwProcessId = 0;
    
    GetWindowThreadProcessId(hwnd, &dwProcessId);
    
    // Check if window belongs to our process
    if (dwProcessId == GetCurrentProcessId())
    {
        // Check if window is visible - if not, try to show it
        if (!IsWindowVisible(hwnd))
        {
            // Get window class name to filter
            char szClassName[256];
            if (GetClassNameA(hwnd, szClassName, sizeof(szClassName)))
            {
                // Skip known BriskPlayer windows and common system classes
                if (strstr(szClassName, "BRISKPLAYER") == NULL &&
                    strstr(szClassName, "tooltips_class") == NULL &&
                    strstr(szClassName, "IME") == NULL)
                {
                    // This might be a hidden plugin window - show it
                    ShowWindow(hwnd, SW_SHOW);
                    SetForegroundWindow(hwnd);
                }
            }
        }
        else
        {
            // Window is visible - bring to front if it might be the plugin
            char szClassName[256];
            if (GetClassNameA(hwnd, szClassName, sizeof(szClassName)))
            {
                if (strstr(szClassName, "BRISKPLAYER") == NULL)
                {
                    SetForegroundWindow(hwnd);
                }
            }
        }
    }
    
    return TRUE;  // Continue enumeration
}

////////////////////////////////////////////////////////////////////////////////
// Module Lifecycle
////////////////////////////////////////////////////////////////////////////////

void CPDSP_Initialize(HWND hWndParent)
{
    if (g_DSPState.m_bInitialized)
        return;
    
    memset(&g_DSPState, 0, sizeof(g_DSPState));
    g_DSPState.m_hWndParent = hWndParent;
    g_DSPState.m_iActivePluginIndex = -1;
    g_DSPState.m_bInitialized = TRUE;
    
    CP_TRACE0("DSP Plugin system initialized");
}

void CPDSP_Uninitialize(void)
{
    if (!g_DSPState.m_bInitialized)
        return;
    
    // Deactivate any active plugin
    CPDSP_DeactivateCurrentPlugin();
    
    // Free the plugin list
    CPDSP_FreePluginList();
    
    g_DSPState.m_bInitialized = FALSE;
    
    CP_TRACE0("DSP Plugin system uninitialized");
}

////////////////////////////////////////////////////////////////////////////////
// Plugin Discovery
////////////////////////////////////////////////////////////////////////////////

void CPDSP_ScanForPlugins(void)
{
    WIN32_FIND_DATAA finddata;
    HANDLE hFileFind;
    char pcSearchPath[MAX_PATH];
    char pcModuleDirectory[MAX_PATH];
    FILE* debugFile;
    
    if (!g_DSPState.m_bInitialized)
        return;
    
    // Free any existing plugin list
    CPDSP_FreePluginList();
    
    // Open debug log file
    debugFile = fopen("dsp_debug.log", "w");
    if (debugFile) fprintf(debugFile, "DSP Plugin Scan Starting...\n");
    
    // Get the application directory
    memset(pcModuleDirectory, 0, sizeof(pcModuleDirectory));
    DWORD pathLen = main_get_program_path(GetModuleHandle(NULL), pcModuleDirectory, MAX_PATH);
    
    if (debugFile) fprintf(debugFile, "Program path returned: '%s' (length: %lu)\n", pcModuleDirectory, pathLen);
    
    if (pathLen == 0)
    {
        if (debugFile) { fprintf(debugFile, "Failed to get program path!\n"); fclose(debugFile); }
        return;
    }
    
    // Build the search path for dsp_*.dll
    snprintf(pcSearchPath, MAX_PATH, "%sdsp_*.dll", pcModuleDirectory);
    
    if (debugFile) fprintf(debugFile, "Search path: '%s'\n", pcSearchPath);
    
    // Find all matching files
    hFileFind = FindFirstFileA(pcSearchPath, &finddata);
    
    if (hFileFind == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        if (debugFile) fprintf(debugFile, "FindFirstFileA failed with error: %lu\n", err);
        if (debugFile) fclose(debugFile);
        return;
    }
    
    if (debugFile) fprintf(debugFile, "FindFirstFileA succeeded!\n");
    
    do
    {
        char pcFullPath[MAX_PATH];
        snprintf(pcFullPath, MAX_PATH, "%s%s", pcModuleDirectory, finddata.cFileName);
        
        if (debugFile) fprintf(debugFile, "Found file: '%s'\n", pcFullPath);
        
        CPDSP_ProbePlugin(pcFullPath);
    }
    while (FindNextFileA(hFileFind, &finddata) != 0);
    
    FindClose(hFileFind);
    
    if (debugFile) { 
        fprintf(debugFile, "Scan complete. Found %d plugin(s)\n", g_DSPState.m_iPluginCount);
        fclose(debugFile);
    }
}

static void CPDSP_ProbePlugin(const char* pcPluginPath)
{
    HMODULE hModule;
    winampDSPGetHeaderType pfnGetHeader;
    winampDSPHeader* pHeader;
    int iModuleIndex;
    char szDirectory[MAX_PATH];
    char* pLastSlash;
    
    // Open debug log file (append mode)
    FILE* debugFile = fopen("K:\\msys64\\home\\wowza\\BriskPlayer\\build\\dsp_debug.txt", "a");
    if (debugFile) fprintf(debugFile, "Probing plugin: '%s'\n", pcPluginPath);
    
    // Extract the directory from the plugin path and add it to DLL search path
    strncpy(szDirectory, pcPluginPath, MAX_PATH - 1);
    szDirectory[MAX_PATH - 1] = '\0';
    pLastSlash = strrchr(szDirectory, '\\');
    if (!pLastSlash) pLastSlash = strrchr(szDirectory, '/');
    if (pLastSlash) *pLastSlash = '\0';
    
    if (debugFile) fprintf(debugFile, "  Setting DLL directory to: '%s'\n", szDirectory);
    SetDllDirectoryA(szDirectory);
    
    // Load the DLL
    hModule = LoadLibraryA(pcPluginPath);
    
    // Reset DLL directory
    SetDllDirectoryA(NULL);
    
    if (!hModule)
    {
        DWORD err = GetLastError();
        if (debugFile) { 
            fprintf(debugFile, "  LoadLibrary FAILED (error %lu)\n", err);
            if (err == 126) fprintf(debugFile, "  ERROR_MOD_NOT_FOUND - missing dependency DLL (e.g. MSVCR90.dll)\n");
            if (err == 193) fprintf(debugFile, "  ERROR_BAD_EXE_FORMAT - 32/64-bit mismatch\n");
            fclose(debugFile); 
        }
        return;
    }
    
    if (debugFile) fprintf(debugFile, "  LoadLibrary succeeded\n");
    
    // Get the header export function
    pfnGetHeader = (winampDSPGetHeaderType)GetProcAddress(hModule, "winampDSPGetHeader2");
    if (!pfnGetHeader)
    {
        if (debugFile) { fprintf(debugFile, "  GetProcAddress FAILED - no winampDSPGetHeader2\n"); fclose(debugFile); }
        FreeLibrary(hModule);
        return;
    }
    
    if (debugFile) fprintf(debugFile, "  GetProcAddress succeeded, calling header function...\n");
    
    // Get the header
    pHeader = pfnGetHeader();
    if (!pHeader)
    {
        if (debugFile) { fprintf(debugFile, "  Header function returned NULL\n"); fclose(debugFile); }
        FreeLibrary(hModule);
        return;
    }
    
    if (debugFile) fprintf(debugFile, "  Header returned: version=0x%x, description='%s'\n", 
                          pHeader->version, pHeader->description ? pHeader->description : "(null)");
    
    if (!pHeader->getModule)
    {
        if (debugFile) { fprintf(debugFile, "  Header getModule is NULL\n"); fclose(debugFile); }
        FreeLibrary(hModule);
        return;
    }
    
    // Check version
    if (debugFile) fprintf(debugFile, "  Checking version: 0x%x vs required 0x%x\n", pHeader->version, DSP_HDRVER);
    
    if (pHeader->version < DSP_HDRVER)
    {
        if (debugFile) { fprintf(debugFile, "  Version too old!\n"); fclose(debugFile); }
        FreeLibrary(hModule);
        return;
    }
    
    if (debugFile) fprintf(debugFile, "  Plugin loaded successfully!\n");
    
    // Enumerate all modules in this plugin
    for (iModuleIndex = 0; ; iModuleIndex++)
    {
        if (debugFile) fprintf(debugFile, "  Getting module %d...\n", iModuleIndex);
        
        winampDSPModule* pModule = pHeader->getModule(iModuleIndex);
        if (!pModule)
        {
            if (debugFile) fprintf(debugFile, "  Module %d is NULL, done.\n", iModuleIndex);
            break;
        }
        
        if (debugFile) fprintf(debugFile, "  Module %d: description='%s'\n", iModuleIndex,
                              pModule->description ? pModule->description : "(null)");
        
        // Create a plugin info entry
        CPs_DSPPluginInfo* pInfo = (CPs_DSPPluginInfo*)malloc(sizeof(CPs_DSPPluginInfo));
        if (!pInfo)
        {
            if (debugFile) fprintf(debugFile, "  malloc failed for plugin info\n");
            break;
        }
        
        memset(pInfo, 0, sizeof(*pInfo));
        
        // Copy the path
        pInfo->m_pcPluginPath = _strdup(pcPluginPath);
        
        // Copy the plugin name (header description)
        pInfo->m_pcPluginName = _strdup(pHeader->description ? pHeader->description : "Unknown Plugin");
        
        // Copy the module name
        pInfo->m_pcModuleName = _strdup(pModule->description ? pModule->description : "Unknown Module");
        
        // Store the module index
        pInfo->m_iModuleIndex = iModuleIndex;
        
        // Add to the front of the list
        pInfo->m_pNext = g_DSPState.m_pFirstPlugin;
        g_DSPState.m_pFirstPlugin = pInfo;
        g_DSPState.m_iPluginCount++;
        
        if (debugFile) fprintf(debugFile, "  Added module: '%s'\n", pInfo->m_pcModuleName);
    }
    
    if (debugFile) { fprintf(debugFile, "  Probe complete, total plugins now: %d\n", g_DSPState.m_iPluginCount); fclose(debugFile); }
    
    // Unload the DLL (we'll reload when activating)
    FreeLibrary(hModule);
}

static void CPDSP_FreePluginList(void)
{
    CPs_DSPPluginInfo* pCurrent = g_DSPState.m_pFirstPlugin;
    
    while (pCurrent)
    {
        CPs_DSPPluginInfo* pNext = pCurrent->m_pNext;
        
        if (pCurrent->m_pcPluginPath)
            free(pCurrent->m_pcPluginPath);
        if (pCurrent->m_pcPluginName)
            free(pCurrent->m_pcPluginName);
        if (pCurrent->m_pcModuleName)
            free(pCurrent->m_pcModuleName);
        
        free(pCurrent);
        pCurrent = pNext;
    }
    
    g_DSPState.m_pFirstPlugin = NULL;
    g_DSPState.m_iPluginCount = 0;
}

const CPs_DSPPluginInfo* CPDSP_GetPluginList(void)
{
    return g_DSPState.m_pFirstPlugin;
}

int CPDSP_GetPluginCount(void)
{
    return g_DSPState.m_iPluginCount;
}

////////////////////////////////////////////////////////////////////////////////
// Plugin Activation
////////////////////////////////////////////////////////////////////////////////

static void CPDSP_DeactivateCurrentPlugin(void)
{
    if (g_DSPState.m_pActiveModule)
    {
        // Call the quit function
        if (g_DSPState.m_pActiveModule->Quit)
        {
            g_DSPState.m_pActiveModule->Quit(g_DSPState.m_pActiveModule);
        }
        g_DSPState.m_pActiveModule = NULL;
    }
    
    g_DSPState.m_pActiveHeader = NULL;
    
    if (g_DSPState.m_hActiveModule)
    {
        FreeLibrary(g_DSPState.m_hActiveModule);
        g_DSPState.m_hActiveModule = NULL;
    }
    
    g_DSPState.m_iActivePluginIndex = -1;
}

BOOL CPDSP_ActivatePlugin(int iPluginIndex)
{
    CPs_DSPPluginInfo* pInfo;
    HMODULE hModule;
    winampDSPGetHeaderType pfnGetHeader;
    winampDSPHeader* pHeader;
    winampDSPModule* pModule;
    int iIndex;
    
    // Deactivate current plugin first
    CPDSP_DeactivateCurrentPlugin();
    
    // -1 means disable DSP
    if (iPluginIndex < 0)
    {
        CP_TRACE0("DSP disabled");
        return TRUE;
    }
    
    // Find the plugin by index
    pInfo = g_DSPState.m_pFirstPlugin;
    for (iIndex = 0; pInfo && iIndex < iPluginIndex; iIndex++)
    {
        pInfo = pInfo->m_pNext;
    }
    
    if (!pInfo)
    {
        CP_TRACE1("Invalid DSP plugin index: %d", iPluginIndex);
        return FALSE;
    }
    
    // Load the DLL
    hModule = LoadLibrary(pInfo->m_pcPluginPath);
    if (!hModule)
    {
        CP_TRACE1("Failed to load DSP plugin: %s", pInfo->m_pcPluginPath);
        return FALSE;
    }
    
    // Get the header
    pfnGetHeader = (winampDSPGetHeaderType)GetProcAddress(hModule, "winampDSPGetHeader2");
    if (!pfnGetHeader)
    {
        FreeLibrary(hModule);
        return FALSE;
    }
    
    pHeader = pfnGetHeader();
    if (!pHeader || !pHeader->getModule)
    {
        FreeLibrary(hModule);
        return FALSE;
    }
    
    // Get the specific module
    pModule = pHeader->getModule(pInfo->m_iModuleIndex);
    if (!pModule)
    {
        FreeLibrary(hModule);
        return FALSE;
    }
    
    // Set up the module
    pModule->hwndParent = g_DSPState.m_hWndParent;
    pModule->hDllInstance = hModule;
    
    // Initialize the module
    if (pModule->Init)
    {
        int iResult = pModule->Init(pModule);
        if (iResult != 0)
        {
            CP_TRACE2("DSP plugin init failed: %s (error %d)", pInfo->m_pcModuleName, iResult);
            FreeLibrary(hModule);
            return FALSE;
        }
    }
    
    // Store the active plugin info
    g_DSPState.m_hActiveModule = hModule;
    g_DSPState.m_pActiveHeader = pHeader;
    g_DSPState.m_pActiveModule = pModule;
    g_DSPState.m_iActivePluginIndex = iPluginIndex;
    
    CP_TRACE1("DSP plugin activated: %s", pInfo->m_pcModuleName);
    
    return TRUE;
}

int CPDSP_GetActivePluginIndex(void)
{
    return g_DSPState.m_iActivePluginIndex;
}

BOOL CPDSP_IsActive(void)
{
    return (g_DSPState.m_pActiveModule != NULL);
}

////////////////////////////////////////////////////////////////////////////////
// DSP Processing
////////////////////////////////////////////////////////////////////////////////

int CPDSP_ProcessSamples(short int* samples, int numsamples, int bps, int nch, int srate)
{
    if (!g_DSPState.m_pActiveModule || !g_DSPState.m_pActiveModule->ModifySamples)
    {
        return numsamples;
    }
    
    return g_DSPState.m_pActiveModule->ModifySamples(
        g_DSPState.m_pActiveModule,
        samples,
        numsamples,
        bps,
        nch,
        srate
    );
}

int CPDSP_GetMaxExpansionFactor(void)
{
    // DSP plugins can potentially double the number of samples
    // (e.g., pitch shifting down by half)
    return 2;
}

////////////////////////////////////////////////////////////////////////////////
// Plugin Configuration
////////////////////////////////////////////////////////////////////////////////

BOOL CPDSP_ShowConfigDialog(void)
{
    if (!g_DSPState.m_pActiveModule || !g_DSPState.m_pActiveModule->Config)
    {
        return FALSE;
    }
    
    // Call the plugin's Config function
    // Some plugins toggle visibility, others always show the window
    g_DSPState.m_pActiveModule->Config(g_DSPState.m_pActiveModule);
    
    // Try to find and bring any windows owned by the plugin DLL to the front
    if (g_DSPState.m_hActiveModule)
    {
        // Enumerate all top-level windows and look for ones that belong to our process
        // and might be the plugin's config window
        EnumWindows(CPDSP_EnumWindowsCallback, (LPARAM)g_DSPState.m_hActiveModule);
    }
    
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Menu Support
////////////////////////////////////////////////////////////////////////////////

int CPDSP_PopulateMenu(HMENU hMenu, UINT uFirstId)
{
    CPs_DSPPluginInfo* pInfo;
    int iCount = 0;
    int iIndex;
    
    if (!hMenu)
        return 0;
    
    // Clear the existing menu items
    while (GetMenuItemCount(hMenu) > 0)
    {
        RemoveMenu(hMenu, 0, MF_BYPOSITION);
    }
    
    // Add "None (Disabled)" option
    {
        UINT uFlags = MF_STRING;
        if (g_DSPState.m_iActivePluginIndex < 0)
            uFlags |= MF_CHECKED;
        
        AppendMenuW(hMenu, uFlags, uFirstId, TW(STR_DSP_NONE));
        iCount++;
    }
    
    // Add separator if we have plugins
    if (g_DSPState.m_pFirstPlugin)
    {
        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
        iCount++;
    }
    
    // Add each plugin
    pInfo = g_DSPState.m_pFirstPlugin;
    for (iIndex = 0; pInfo; pInfo = pInfo->m_pNext, iIndex++)
    {
        UINT uFlags = MF_STRING;
        wchar_t wcDisplayName[256];
        
        if (iIndex == g_DSPState.m_iActivePluginIndex)
            uFlags |= MF_CHECKED;
        
        // Convert module name to wide string
        MultiByteToWideChar(CP_ACP, 0, pInfo->m_pcModuleName, -1, wcDisplayName, 256);
        
        AppendMenuW(hMenu, uFlags, uFirstId + iIndex + 1, wcDisplayName);
        iCount++;
    }
    
    // Add separator and Configure option if a plugin is active
    if (g_DSPState.m_pActiveModule && g_DSPState.m_pActiveModule->Config)
    {
        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hMenu, MF_STRING, uFirstId + 1000, TW(STR_DSP_CONFIGURE));
        iCount += 2;
    }
    
    return iCount;
}

BOOL CPDSP_HandleMenuCommand(UINT uMenuId, UINT uFirstId)
{
    // Handle "Configure" option
    if (uMenuId == uFirstId + 1000)
    {
        CPDSP_ShowConfigDialog();
        return TRUE;
    }
    
    // Handle "None (Disabled)" option
    if (uMenuId == uFirstId)
    {
        CPDSP_ActivatePlugin(-1);
        return TRUE;
    }
    
    // Handle plugin selection
    if (uMenuId > uFirstId && uMenuId < uFirstId + 1000)
    {
        int iPluginIndex = uMenuId - uFirstId - 1;
        CPDSP_ActivatePlugin(iPluginIndex);
        return TRUE;
    }
    
    return FALSE;
}
