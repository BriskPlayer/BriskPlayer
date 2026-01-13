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

////////////////////////////////////////////////////////////////////////////////
//
// Window Snapping and Docking Implementation
//
////////////////////////////////////////////////////////////////////////////////

#include "WindowSnapping.h"
#include "globals.h"

////////////////////////////////////////////////////////////////////////////////
// Module-level state

static WindowDockInfo g_dockInfo[MAX_DOCKED_WINDOWS] = {0};
static BOOL g_movingDockedWindows = FALSE;  // Prevent infinite recursion

////////////////////////////////////////////////////////////////////////////////
// Docking Management Functions

void SetWindowDocking(HWND window, HWND dockedTo, int offsetX, int offsetY)
{
    for (int i = 0; i < MAX_DOCKED_WINDOWS; i++) {
        if (g_dockInfo[i].window == window || g_dockInfo[i].window == NULL) {
            g_dockInfo[i].window = window;
            g_dockInfo[i].dockedTo = dockedTo;
            g_dockInfo[i].offsetX = offsetX;
            g_dockInfo[i].offsetY = offsetY;
            g_dockInfo[i].isDocked = TRUE;
            break;
        }
    }
}

void ClearWindowDocking(HWND window)
{
    for (int i = 0; i < MAX_DOCKED_WINDOWS; i++) {
        if (g_dockInfo[i].window == window) {
            g_dockInfo[i].window = NULL;
            g_dockInfo[i].dockedTo = NULL;
            g_dockInfo[i].offsetX = 0;
            g_dockInfo[i].offsetY = 0;
            g_dockInfo[i].isDocked = FALSE;
            break;
        }
    }
}

WindowDockInfo* GetWindowDockInfo(HWND window)
{
    for (int i = 0; i < MAX_DOCKED_WINDOWS; i++) {
        if (g_dockInfo[i].window == window && g_dockInfo[i].isDocked) {
            return &g_dockInfo[i];
        }
    }
    return NULL;
}

void MoveDockedWindows(HWND movedWindow, int deltaX, int deltaY)
{
    // Check if sticky windows option is enabled
    if (!options.sticky_windows || g_movingDockedWindows) return;
    
    g_movingDockedWindows = TRUE;
    
    // Find all windows docked to the moved window and move them
    for (int i = 0; i < MAX_DOCKED_WINDOWS; i++) {
        if (g_dockInfo[i].isDocked && g_dockInfo[i].dockedTo == movedWindow) {
            HWND dockedWindow = g_dockInfo[i].window;
            if (IsWindow(dockedWindow) && IsWindowVisible(dockedWindow)) {
                RECT dockedRect;
                if (GetWindowRect(dockedWindow, &dockedRect)) {
                    SetWindowPos(dockedWindow, NULL, 
                               dockedRect.left + deltaX, 
                               dockedRect.top + deltaY,
                               0, 0, SWP_NOSIZE | SWP_NOZORDER);
                }
            }
        }
    }
    
    g_movingDockedWindows = FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// Window Snapping Implementation

void SnapWindow(HWND hWnd, RECT* pMovingRect)
{
    // Check if sticky windows option is enabled
    if (!options.sticky_windows || !pMovingRect) return;
    
    RECT snapRect = *pMovingRect;
    int snapTolerance = SNAP_DISTANCE;
    BOOL snapped = FALSE;
    
    // Get screen dimensions for edge snapping
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // Snap to screen edges
    if (SNAP_TO_SCREEN_EDGES) {
        // Left edge
        if (abs(snapRect.left) <= snapTolerance) {
            int offset = -snapRect.left;
            snapRect.left += offset;
            snapRect.right += offset;
            snapped = TRUE;
        }
        // Right edge
        else if (abs(snapRect.right - screenWidth) <= snapTolerance) {
            int offset = screenWidth - snapRect.right;
            snapRect.left += offset;
            snapRect.right += offset;
            snapped = TRUE;
        }
        
        // Top edge
        if (abs(snapRect.top) <= snapTolerance) {
            int offset = -snapRect.top;
            snapRect.top += offset;
            snapRect.bottom += offset;
            snapped = TRUE;
        }
        // Bottom edge
        else if (abs(snapRect.bottom - screenHeight) <= snapTolerance) {
            int offset = screenHeight - snapRect.bottom;
            snapRect.top += offset;
            snapRect.bottom += offset;
            snapped = TRUE;
        }
    }
    
    // Snap to other BriskPlayer windows
    if (SNAP_TO_OTHER_WINDOWS) {
        HWND snapTargets[] = {
            windows.wnd_main,
            windows.dlg_playlist,
            windows.m_hWndPlaylist,
            windows.dlg_options
        };
        
        HWND dockedToWindow = NULL;
        int bestSnapDistance = snapTolerance + 1;
        RECT bestSnapRect = snapRect;
        
        // Check if we're currently docked and should break free
        WindowDockInfo* currentDock = GetWindowDockInfo(hWnd);
        BOOL justUndocked = FALSE;
        if (currentDock && currentDock->dockedTo) {
            RECT dockedToRect;
            if (GetWindowRect(currentDock->dockedTo, &dockedToRect)) {
                int expectedX = dockedToRect.left + currentDock->offsetX;
                int expectedY = dockedToRect.top + currentDock->offsetY;
                int dragDistanceX = abs(snapRect.left - expectedX);
                int dragDistanceY = abs(snapRect.top - expectedY);
                
                // If dragged far enough, break the dock
                if (dragDistanceX > UNDOCK_DISTANCE || dragDistanceY > UNDOCK_DISTANCE) {
                    ClearWindowDocking(hWnd);
                    currentDock = NULL;
                    justUndocked = TRUE;  // Don't re-snap immediately
                }
            }
        }
        
        // Skip snapping if we just undocked to prevent immediate re-snap
        if (justUndocked) {
            // Just return without snapping
            return;
        }
        
        for (size_t i = 0; i < sizeof(snapTargets) / sizeof(HWND); i++) {
            HWND targetWnd = snapTargets[i];
            if (!targetWnd || targetWnd == hWnd || !IsWindowVisible(targetWnd)) {
                continue;
            }
            
            RECT targetRect;
            if (GetWindowRect(targetWnd, &targetRect)) {
                RECT candidateRect = snapRect;
                int snapDistance = snapTolerance + 1;
                BOOL candidateSnapped = FALSE;
                
                // Check if windows are vertically overlapping (for horizontal edge-to-edge snapping)
                BOOL verticalOverlap = (candidateRect.bottom > targetRect.top && candidateRect.top < targetRect.bottom);
                
                // Check if windows are horizontally overlapping (for vertical edge-to-edge snapping)
                BOOL horizontalOverlap = (candidateRect.right > targetRect.left && candidateRect.left < targetRect.right);
                
                // Only snap horizontally if there's vertical overlap (windows are adjacent)
                if (verticalOverlap) {
                    int rightEdgeDistance = abs(candidateRect.left - targetRect.right);
                    int leftEdgeDistance = abs(candidateRect.right - targetRect.left);
                    
                    if (rightEdgeDistance <= snapTolerance && rightEdgeDistance < snapDistance) {
                        // Snap to right edge of target
                        int offset = targetRect.right - candidateRect.left;
                        candidateRect.left += offset;
                        candidateRect.right += offset;
                        snapDistance = rightEdgeDistance;
                        candidateSnapped = TRUE;
                    }
                    else if (leftEdgeDistance <= snapTolerance && leftEdgeDistance < snapDistance) {
                        // Snap to left edge of target
                        int offset = targetRect.left - candidateRect.right;
                        candidateRect.left += offset;
                        candidateRect.right += offset;
                        snapDistance = leftEdgeDistance;
                        candidateSnapped = TRUE;
                    }
                }
                
                // Only snap vertically if there's horizontal overlap (windows are adjacent)
                if (!candidateSnapped && horizontalOverlap) {
                    int bottomEdgeDistance = abs(candidateRect.top - targetRect.bottom);
                    int topEdgeDistance = abs(candidateRect.bottom - targetRect.top);
                    
                    if (bottomEdgeDistance <= snapTolerance && bottomEdgeDistance < snapDistance) {
                        // Snap to bottom edge of target
                        int offset = targetRect.bottom - candidateRect.top;
                        candidateRect.top += offset;
                        candidateRect.bottom += offset;
                        snapDistance = bottomEdgeDistance;
                        candidateSnapped = TRUE;
                    }
                    else if (topEdgeDistance <= snapTolerance && topEdgeDistance < snapDistance) {
                        // Snap to top edge of target
                        int offset = targetRect.top - candidateRect.bottom;
                        candidateRect.top += offset;
                        candidateRect.bottom += offset;
                        snapDistance = topEdgeDistance;
                        candidateSnapped = TRUE;
                    }
                }
                
                // Use this snap if it's the closest one so far
                if (candidateSnapped && snapDistance < bestSnapDistance) {
                    bestSnapDistance = snapDistance;
                    bestSnapRect = candidateRect;
                    dockedToWindow = targetWnd;
                    snapped = TRUE;
                }
            }
        }
        
        // Apply the best snap if we found one
        if (snapped) {
            snapRect = bestSnapRect;
            
            // Record docking relationship
            if (dockedToWindow) {
                RECT dockedToRect;
                if (GetWindowRect(dockedToWindow, &dockedToRect)) {
                    int offsetX = snapRect.left - dockedToRect.left;
                    int offsetY = snapRect.top - dockedToRect.top;
                    SetWindowDocking(hWnd, dockedToWindow, offsetX, offsetY);
                }
            }
        }
        else if (!currentDock) {
            // Clear docking if we're not snapped to anything and not currently docked
            ClearWindowDocking(hWnd);
        }
    }
    
    // Update the rectangle if we snapped
    if (snapped) {
        *pMovingRect = snapRect;
    }
}

////////////////////////////////////////////////////////////////////////////////
