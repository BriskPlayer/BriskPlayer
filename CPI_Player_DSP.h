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

#ifndef CPI_PLAYER_DSP_H
#define CPI_PLAYER_DSP_H

////////////////////////////////////////////////////////////////////////////////
//
// WinAmp DSP Plugin Support Module
//
// This module provides support for loading and using WinAmp-compatible DSP
// (Digital Signal Processing) plugins. DSP plugins can modify audio data
// in real-time, applying effects like reverb, echo, normalization, etc.
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>

////////////////////////////////////////////////////////////////////////////////
// Types
////////////////////////////////////////////////////////////////////////////////

// Forward declaration for the DSP plugin info structure
typedef struct _CPs_DSPPluginInfo CPs_DSPPluginInfo;

// DSP plugin information - used for UI display
struct _CPs_DSPPluginInfo
{
    char* m_pcPluginPath;       // Full path to the DLL
    char* m_pcPluginName;       // Friendly name (from header description)
    char* m_pcModuleName;       // Module name (from module description)
    int m_iModuleIndex;         // Module index within the plugin
    CPs_DSPPluginInfo* m_pNext; // Linked list next pointer
};

////////////////////////////////////////////////////////////////////////////////
// Module Lifecycle
////////////////////////////////////////////////////////////////////////////////

// Initialize the DSP plugin system
// Should be called during player initialization
void CPDSP_Initialize(HWND hWndParent);

// Shutdown the DSP plugin system
// Should be called during player shutdown
void CPDSP_Uninitialize(void);

////////////////////////////////////////////////////////////////////////////////
// Plugin Discovery
////////////////////////////////////////////////////////////////////////////////

// Scan for DSP plugins in the application directory
// Call this after CPDSP_Initialize to populate the plugin list
void CPDSP_ScanForPlugins(void);

// Get the list of available DSP plugins
// Returns a linked list of CPs_DSPPluginInfo structures
const CPs_DSPPluginInfo* CPDSP_GetPluginList(void);

// Get the number of available DSP plugins
int CPDSP_GetPluginCount(void);

////////////////////////////////////////////////////////////////////////////////
// Plugin Activation
////////////////////////////////////////////////////////////////////////////////

// Activate a DSP plugin by its index in the plugin list
// Returns TRUE on success, FALSE on failure
// Passing -1 disables any active DSP plugin
BOOL CPDSP_ActivatePlugin(int iPluginIndex);

// Get the currently active plugin index (-1 if none)
int CPDSP_GetActivePluginIndex(void);

// Check if a DSP plugin is currently active
BOOL CPDSP_IsActive(void);

////////////////////////////////////////////////////////////////////////////////
// DSP Processing
////////////////////////////////////////////////////////////////////////////////

// Process audio samples through the active DSP plugin
// This is the main function called from the audio pipeline
// samples: pointer to interleaved sample data
// numsamples: number of sample FRAMES (not individual samples)
// bps: bits per sample (8, 16, 24, 32)
// nch: number of channels (1 = mono, 2 = stereo)
// srate: sample rate in Hz
// Returns: the number of sample frames after processing (may differ from input)
int CPDSP_ProcessSamples(short int* samples, int numsamples, int bps, int nch, int srate);

// Get the maximum expansion factor for DSP processing
// Some DSP plugins may expand the number of samples (e.g., pitch shifting)
// Use this to allocate appropriately sized buffers
int CPDSP_GetMaxExpansionFactor(void);

////////////////////////////////////////////////////////////////////////////////
// Plugin Configuration
////////////////////////////////////////////////////////////////////////////////

// Show the configuration dialog for the active DSP plugin
// Returns TRUE if the plugin has a config dialog, FALSE otherwise
BOOL CPDSP_ShowConfigDialog(void);

////////////////////////////////////////////////////////////////////////////////
// Menu Support
////////////////////////////////////////////////////////////////////////////////

// Populate a menu with available DSP plugins
// The menu will be cleared and populated with plugin names
// Returns the number of items added
int CPDSP_PopulateMenu(HMENU hMenu, UINT uFirstId);

// Handle a DSP menu selection
// Returns TRUE if the menu ID was handled, FALSE otherwise
BOOL CPDSP_HandleMenuCommand(UINT uMenuId, UINT uFirstId);

////////////////////////////////////////////////////////////////////////////////

#endif // CPI_PLAYER_DSP_H
