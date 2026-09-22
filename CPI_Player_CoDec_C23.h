////////////////////////////////////////////////////////////////////////////////
// Shared include aggregator for the codec .c files (FFmpeg, Opus): bundles
// Windows headers (in the order they need to be included) with the core
// BriskPlayer codec interface.
////////////////////////////////////////////////////////////////////////////////

#ifndef CPI_PLAYER_CODEC_C23_H
#define CPI_PLAYER_CODEC_C23_H

// Ensure Windows types are available before including anything else
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>              // Must come first for BOOL, DWORD, HWND types
#include <shellapi.h>             // For HDROP
#include <commctrl.h>             // For HIMAGELIST

#include <stdint.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>              // For bool, true, false

#include "c23_compat.h"           // Include c23_compat FIRST for static_assert support
#include "globals.h"              // Include globals for CPs_FileInfo and other types
#include "CPI_Player_CoDec.h"     // Include core BriskPlayer codec interface after Windows types

#endif // CPI_PLAYER_CODEC_C23_H