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

// Window snapping configuration (Audacious-style)
#define SNAP_DISTANCE 3             // Pixels within which windows will snap to each other
#define SCREEN_SNAP_DISTANCE 1      // Pixels for screen edge snapping (very gentle)
#define UNDOCK_DISTANCE 8           // Pixels to drag before breaking docking (same as snap for easy release)
#define SNAP_TO_SCREEN_EDGES 1      // Enable screen edge snapping
#define SNAP_TO_OTHER_WINDOWS 1     // Enable snapping to other BriskPlayer windows
#define MAX_DOCKED_WINDOWS 4        // Maximum number of windows to track

// Dock edge types for recursive docking (bitmask)
#define DOCK_TYPE_LEFT   (1 << 0)
#define DOCK_TYPE_RIGHT  (1 << 1)
#define DOCK_TYPE_TOP    (1 << 2)
#define DOCK_TYPE_BOTTOM (1 << 3)
#define DOCK_TYPE_ANY    (DOCK_TYPE_LEFT | DOCK_TYPE_RIGHT | DOCK_TYPE_TOP | DOCK_TYPE_BOTTOM)

// Window docking tracking structure
typedef struct {
    HWND window;        // The docked window
    HWND dockedTo;      // The window it's docked to
    int offsetX;        // Horizontal offset from docked window
    int offsetY;        // Vertical offset from docked window
    BOOL isDocked;      // Whether the window is currently docked
    int width;          // Window width (for edge calculations)
    int height;         // Window height (for edge calculations)
} WindowDockInfo;

////////////////////////////////////////////////////////////////////////////////
// Public API Functions

// Initialize the dock tracking (call at startup)
void InitializeDocking(void);

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

// Audacious-style: Start a window move operation
// hWnd: The window being dragged
// x, y: Starting mouse position (screen coordinates)
void DockMoveStart(HWND hWnd, int x, int y);

// Audacious-style: Continue a window move operation
// hWnd: The window being dragged
// x, y: Current mouse position (screen coordinates)
void DockMove(HWND hWnd, int x, int y);

////////////////////////////////////////////////////////////////////////////////

#endif // WINDOW_SNAPPING_H
