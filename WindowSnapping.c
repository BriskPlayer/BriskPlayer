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
// Based on Audacious's dock.cc algorithm for improved snapping behavior
//
// Key improvements over original:
// - Uses "least absolute distance" approach for finding best snap
// - Supports simultaneous horizontal AND vertical snapping
// - Tests all edge combinations for better accuracy
// - Recursive dock group detection for moving connected windows
//
////////////////////////////////////////////////////////////////////////////////

#include "WindowSnapping.h"
#include "globals.h"
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
// Module-level state

static WindowDockInfo g_dockInfo[MAX_DOCKED_WINDOWS] = {0};
static BOOL g_movingDockedWindows = FALSE;  // Prevent infinite recursion

// Audacious-style movement tracking
static int g_lastMoveX = 0;
static int g_lastMoveY = 0;
static HWND g_movingWindow = NULL;

////////////////////////////////////////////////////////////////////////////////
// Helper Functions

// Returns the value with the smallest absolute value (Audacious's least_abs)
static inline int least_abs(int a, int b)
{
    return (abs(a) < abs(b)) ? a : b;
}

// Check if two windows are edge-to-edge docked (Audacious-style)
static BOOL __attribute__((unused)) IsDockedToWindow(RECT* pWindow, RECT* pTarget, int dockType)
{
    int wndRight = pWindow->right;
    int wndBottom = pWindow->bottom;
    int tgtRight = pTarget->right;
    int tgtBottom = pTarget->bottom;
    
    // Check each dock type
    if ((dockType & DOCK_TYPE_LEFT) && wndRight == pTarget->left)
        return TRUE;
    if ((dockType & DOCK_TYPE_RIGHT) && pWindow->left == tgtRight)
        return TRUE;
    if ((dockType & DOCK_TYPE_TOP) && wndBottom == pTarget->top)
        return TRUE;
    if ((dockType & DOCK_TYPE_BOTTOM) && pWindow->top == tgtBottom)
        return TRUE;
    
    return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// Docking Management Functions

void InitializeDocking(void)
{
    for (int i = 0; i < MAX_DOCKED_WINDOWS; i++) {
        g_dockInfo[i].window = NULL;
        g_dockInfo[i].dockedTo = NULL;
        g_dockInfo[i].offsetX = 0;
        g_dockInfo[i].offsetY = 0;
        g_dockInfo[i].isDocked = FALSE;
        g_dockInfo[i].width = 0;
        g_dockInfo[i].height = 0;
    }
    g_movingWindow = NULL;
    g_lastMoveX = 0;
    g_lastMoveY = 0;
}

void SetWindowDocking(HWND window, HWND dockedTo, int offsetX, int offsetY)
{
    // First try to find existing entry for this window
    for (int i = 0; i < MAX_DOCKED_WINDOWS; i++) {
        if (g_dockInfo[i].window == window) {
            g_dockInfo[i].dockedTo = dockedTo;
            g_dockInfo[i].offsetX = offsetX;
            g_dockInfo[i].offsetY = offsetY;
            g_dockInfo[i].isDocked = TRUE;
            
            // Update size info
            RECT rect;
            if (GetWindowRect(window, &rect)) {
                g_dockInfo[i].width = rect.right - rect.left;
                g_dockInfo[i].height = rect.bottom - rect.top;
            }
            return;
        }
    }
    
    // Otherwise find empty slot
    for (int i = 0; i < MAX_DOCKED_WINDOWS; i++) {
        if (g_dockInfo[i].window == NULL) {
            g_dockInfo[i].window = window;
            g_dockInfo[i].dockedTo = dockedTo;
            g_dockInfo[i].offsetX = offsetX;
            g_dockInfo[i].offsetY = offsetY;
            g_dockInfo[i].isDocked = TRUE;
            
            // Update size info
            RECT rect;
            if (GetWindowRect(window, &rect)) {
                g_dockInfo[i].width = rect.right - rect.left;
                g_dockInfo[i].height = rect.bottom - rect.top;
            }
            return;
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
            g_dockInfo[i].width = 0;
            g_dockInfo[i].height = 0;
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
                    
                    // Recursively move windows docked to this one
                    MoveDockedWindows(dockedWindow, deltaX, deltaY);
                }
            }
        }
    }
    
    g_movingDockedWindows = FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// Audacious-style Movement Functions

void DockMoveStart(HWND hWnd, int x, int y)
{
    g_movingWindow = hWnd;
    g_lastMoveX = x;
    g_lastMoveY = y;
}

void DockMove(HWND hWnd, int x, int y)
{
    if (!options.sticky_windows || hWnd != g_movingWindow) return;
    if (x == g_lastMoveX && y == g_lastMoveY) return;
    
    // Calculate movement delta
    int deltaX = x - g_lastMoveX;
    int deltaY = y - g_lastMoveY;
    
    g_lastMoveX = x;
    g_lastMoveY = y;
    
    // Move the window and its docked children
    RECT rect;
    if (GetWindowRect(hWnd, &rect)) {
        SetWindowPos(hWnd, NULL, rect.left + deltaX, rect.top + deltaY,
                    0, 0, SWP_NOSIZE | SWP_NOZORDER);
        MoveDockedWindows(hWnd, deltaX, deltaY);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Window Snapping Implementation (Audacious-style algorithm)

void SnapWindow(HWND hWnd, RECT* pMovingRect)
{
    // Check if sticky windows option is enabled
    if (!options.sticky_windows || !pMovingRect) return;
    
    int snapTolerance = SNAP_DISTANCE;
    
    // Horizontal and vertical snap offsets (Audacious uses separate tracking)
    int horiSnap = snapTolerance + 1;  // Start beyond tolerance (no snap)
    int vertSnap = snapTolerance + 1;
    
    HWND dockedToWindow = NULL;
    
    // Check if we're currently docked and should break free
    // Audacious-style: docking is broken simply by moving - no resistance
    WindowDockInfo* currentDock = GetWindowDockInfo(hWnd);
    if (currentDock && currentDock->dockedTo) {
        RECT dockedToRect;
        if (GetWindowRect(currentDock->dockedTo, &dockedToRect)) {
            // Check if we're still edge-to-edge with the docked window
            // If not, immediately break the dock (no fighting the user)
            BOOL stillDocked = FALSE;
            
            int myRight = pMovingRect->right;
            int myBottom = pMovingRect->bottom;
            int theirRight = dockedToRect.right;
            int theirBottom = dockedToRect.bottom;
            
            // Check if any edges are still touching (within 2 pixels)
            if (abs(pMovingRect->left - theirRight) <= 2 ||    // my left to their right
                abs(myRight - dockedToRect.left) <= 2 ||       // my right to their left
                abs(pMovingRect->top - theirBottom) <= 2 ||    // my top to their bottom
                abs(myBottom - dockedToRect.top) <= 2) {       // my bottom to their top
                stillDocked = TRUE;
            }
            
            if (!stillDocked) {
                ClearWindowDocking(hWnd);
                currentDock = NULL;
            }
        }
    }
    
    // ========================================================================
    // SCREEN EDGE SNAPPING (gentler than window-to-window)
    // ========================================================================
    if (SNAP_TO_SCREEN_EDGES) {
        int screenSnapTolerance = SCREEN_SNAP_DISTANCE;  // Gentler for screen edges
        
        // Get work area for primary monitor (excludes taskbar)
        RECT workArea;
        if (SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0)) {
            int testSnap;
            
            // Left screen edge
            testSnap = workArea.left - pMovingRect->left;
            if (abs(testSnap) <= screenSnapTolerance && abs(testSnap) < abs(horiSnap))
                horiSnap = testSnap;
            
            // Right screen edge
            testSnap = workArea.right - pMovingRect->right;
            if (abs(testSnap) <= screenSnapTolerance && abs(testSnap) < abs(horiSnap))
                horiSnap = testSnap;
            
            // Top screen edge
            testSnap = workArea.top - pMovingRect->top;
            if (abs(testSnap) <= screenSnapTolerance && abs(testSnap) < abs(vertSnap))
                vertSnap = testSnap;
            
            // Bottom screen edge
            testSnap = workArea.bottom - pMovingRect->bottom;
            if (abs(testSnap) <= screenSnapTolerance && abs(testSnap) < abs(vertSnap))
                vertSnap = testSnap;
        }
        
        // Also check current monitor
        POINT pt = { pMovingRect->left, pMovingRect->top };
        HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        if (hMon) {
            MONITORINFO mi = { sizeof(mi) };
            if (GetMonitorInfo(hMon, &mi)) {
                RECT* pWork = &mi.rcWork;
                int testSnap;
                
                testSnap = pWork->left - pMovingRect->left;
                if (abs(testSnap) <= screenSnapTolerance && abs(testSnap) < abs(horiSnap))
                    horiSnap = testSnap;
                    
                testSnap = pWork->right - pMovingRect->right;
                if (abs(testSnap) <= screenSnapTolerance && abs(testSnap) < abs(horiSnap))
                    horiSnap = testSnap;
                    
                testSnap = pWork->top - pMovingRect->top;
                if (abs(testSnap) <= screenSnapTolerance && abs(testSnap) < abs(vertSnap))
                    vertSnap = testSnap;
                    
                testSnap = pWork->bottom - pMovingRect->bottom;
                if (abs(testSnap) <= screenSnapTolerance && abs(testSnap) < abs(vertSnap))
                    vertSnap = testSnap;
            }
        }
    }
    
    // ========================================================================
    // WINDOW-TO-WINDOW SNAPPING (Audacious-style: test all edge combinations)
    // ========================================================================
    if (SNAP_TO_OTHER_WINDOWS) {
        HWND snapTargets[] = {
            windows.wnd_main,
            windows.dlg_playlist,
            windows.m_hWndPlaylist,
            windows.dlg_options
        };
        
        for (size_t i = 0; i < sizeof(snapTargets) / sizeof(HWND); i++) {
            HWND targetWnd = snapTargets[i];
            if (!targetWnd || targetWnd == hWnd || !IsWindowVisible(targetWnd)) {
                continue;
            }
            
            RECT targetRect;
            if (!GetWindowRect(targetWnd, &targetRect)) {
                continue;
            }
            
            // Audacious-style: Test ALL 4 horizontal edge combinations
            // This allows snapping left-to-left, left-to-right, right-to-left, right-to-right
            
            int testHori, testVert;
            
            // Only test horizontal snapping if there's some vertical overlap
            BOOL verticalOverlap = (pMovingRect->bottom > targetRect.top - snapTolerance && 
                                   pMovingRect->top < targetRect.bottom + snapTolerance);
            
            // Only test vertical snapping if there's some horizontal overlap  
            BOOL horizontalOverlap = (pMovingRect->right > targetRect.left - snapTolerance && 
                                     pMovingRect->left < targetRect.right + snapTolerance);
            
            if (verticalOverlap) {
                // Moving window's LEFT edge to target's LEFT edge
                testHori = targetRect.left - pMovingRect->left;
                horiSnap = least_abs(horiSnap, testHori);
                
                // Moving window's LEFT edge to target's RIGHT edge (dock to right side)
                testHori = targetRect.right - pMovingRect->left;
                if (abs(testHori) < abs(horiSnap)) {
                    horiSnap = testHori;
                    dockedToWindow = targetWnd;
                }
                
                // Moving window's RIGHT edge to target's LEFT edge (dock to left side)
                testHori = targetRect.left - pMovingRect->right;
                if (abs(testHori) < abs(horiSnap)) {
                    horiSnap = testHori;
                    dockedToWindow = targetWnd;
                }
                
                // Moving window's RIGHT edge to target's RIGHT edge
                testHori = targetRect.right - pMovingRect->right;
                horiSnap = least_abs(horiSnap, testHori);
            }
            
            if (horizontalOverlap) {
                // Moving window's TOP edge to target's TOP edge
                testVert = targetRect.top - pMovingRect->top;
                vertSnap = least_abs(vertSnap, testVert);
                
                // Moving window's TOP edge to target's BOTTOM edge (dock below)
                testVert = targetRect.bottom - pMovingRect->top;
                if (abs(testVert) < abs(vertSnap)) {
                    vertSnap = testVert;
                    dockedToWindow = targetWnd;
                }
                
                // Moving window's BOTTOM edge to target's TOP edge (dock above)
                testVert = targetRect.top - pMovingRect->bottom;
                if (abs(testVert) < abs(vertSnap)) {
                    vertSnap = testVert;
                    dockedToWindow = targetWnd;
                }
                
                // Moving window's BOTTOM edge to target's BOTTOM edge
                testVert = targetRect.bottom - pMovingRect->bottom;
                vertSnap = least_abs(vertSnap, testVert);
            }
        }
    }
    
    // ========================================================================
    // APPLY SNAP OFFSETS (only if within tolerance)
    // ========================================================================
    
    BOOL snapped = FALSE;
    BOOL edgeToEdgeSnap = FALSE;  // Only dock when edges are exactly touching
    
    // Apply horizontal snap if within tolerance
    if (abs(horiSnap) <= snapTolerance) {
        pMovingRect->left += horiSnap;
        pMovingRect->right += horiSnap;
        snapped = TRUE;
        // Check if this results in edge-to-edge contact (for docking)
        if (horiSnap != 0 && abs(horiSnap) <= 2) {
            edgeToEdgeSnap = TRUE;
        }
    }
    
    // Apply vertical snap if within tolerance (INDEPENDENT of horizontal!)
    if (abs(vertSnap) <= snapTolerance) {
        pMovingRect->top += vertSnap;
        pMovingRect->bottom += vertSnap;
        snapped = TRUE;
        // Check if this results in edge-to-edge contact (for docking)
        if (vertSnap != 0 && abs(vertSnap) <= 2) {
            edgeToEdgeSnap = TRUE;
        }
    }
    
    // ========================================================================
    // UPDATE DOCKING RELATIONSHIP (only for edge-to-edge contact)
    // ========================================================================
    
    // Only establish docking when edges are nearly touching (gentle dock)
    // This makes it easy to pull apart - just move a few pixels
    if (edgeToEdgeSnap && dockedToWindow) {
        RECT dockedToRect;
        if (GetWindowRect(dockedToWindow, &dockedToRect)) {
            int offsetX = pMovingRect->left - dockedToRect.left;
            int offsetY = pMovingRect->top - dockedToRect.top;
            SetWindowDocking(hWnd, dockedToWindow, offsetX, offsetY);
        }
    }
    else if (!snapped) {
        // Clear docking if we're not snapped to anything
        ClearWindowDocking(hWnd);
    }
}

////////////////////////////////////////////////////////////////////////////////
