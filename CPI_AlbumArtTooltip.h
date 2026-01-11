/*
 * BriskPlayer - Blazing fast audio player.
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
////////////////////////////////////////////////////////////////////////////////

#ifndef _CPI_ALBUMARTTOOLTIP_H_
#define _CPI_ALBUMARTTOOLTIP_H_

#include <windows.h>
#include "CPI_PlaylistItem.h"

////////////////////////////////////////////////////////////////////////////////
//
// Album Art Tooltip - Custom tooltip window for displaying album artwork
//
////////////////////////////////////////////////////////////////////////////////

// Configuration
#define CPAAT_ARTWORK_SIZE      128     // Max artwork size (width/height)
#define CPAAT_PADDING           8       // Padding around content
#define CPAAT_TEXT_WIDTH        200     // Width of text area
#define CPAAT_HOVER_DELAY_MS    500     // Delay before showing tooltip
#define CPAAT_FADE_DURATION_MS  150     // Fade in/out duration

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the album art tooltip system
void CPAAT_Initialize(void);

// Cleanup the album art tooltip system
void CPAAT_Cleanup(void);

// Show tooltip for a playlist item at the specified position
void CPAAT_ShowForItem(CP_HPLAYLISTITEM hItem, const POINT* pScreenPos);

// Hide the tooltip
void CPAAT_Hide(void);

// Update tooltip position (if visible)
void CPAAT_UpdatePosition(const POINT* pScreenPos);

// Check if tooltip is currently visible
BOOL CPAAT_IsVisible(void);

// Track item hover (manages delay before showing)
void CPAAT_TrackItemHover(CP_HPLAYLISTITEM hItem, const POINT* pScreenPos);

// Cancel current hover tracking
void CPAAT_CancelHover(void);

#ifdef __cplusplus
}
#endif

#endif // _CPI_ALBUMARTTOOLTIP_H_
