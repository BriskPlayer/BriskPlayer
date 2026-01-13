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

#ifndef WINDOW_SNAPPING_H
#define WINDOW_SNAPPING_H

////////////////////////////////////////////////////////////////////////////////
//
// Window Snapping and Docking Module
// Provides "sticky windows" functionality for window snapping and docking
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

// Window snapping configuration
#define SNAP_DISTANCE 6             // Pixels within which windows will snap
#define UNDOCK_DISTANCE 12          // Pixels to drag before breaking docking
#define SNAP_TO_SCREEN_EDGES 0      // Disable screen edge snapping
#define SNAP_TO_OTHER_WINDOWS 1     // Enable snapping to other BriskPlayer windows
#define MAX_DOCKED_WINDOWS 4        // Maximum number of windows to track

// Window docking tracking structure
typedef struct {
    HWND window;        // The docked window
    HWND dockedTo;      // The window it's docked to
    int offsetX;        // Horizontal offset from docked window
    int offsetY;        // Vertical offset from docked window
    BOOL isDocked;      // Whether the window is currently docked
} WindowDockInfo;

////////////////////////////////////////////////////////////////////////////////
// Public API Functions

// Set docking relationship between two windows
// window: The window being docked
// dockedTo: The target window to dock to
// offsetX, offsetY: Position offset from the target window
void SetWindowDocking(HWND window, HWND dockedTo, int offsetX, int offsetY);

// Clear docking for a specific window
void ClearWindowDocking(HWND window);

// Get docking info for a window (returns NULL if not docked)
WindowDockInfo* GetWindowDockInfo(HWND window);

// Move all windows that are docked to the specified window
// movedWindow: The window that was moved
// deltaX, deltaY: How much the window was moved
void MoveDockedWindows(HWND movedWindow, int deltaX, int deltaY);

// Snap a window to screen edges and/or other BriskPlayer windows
// hWnd: The window being moved
// pMovingRect: The proposed new position (will be modified if snapping occurs)
void SnapWindow(HWND hWnd, RECT* pMovingRect);

////////////////////////////////////////////////////////////////////////////////

#endif // WINDOW_SNAPPING_H
