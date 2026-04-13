/*
 * BriskPlayer - Blazing fast audio player.
 * Copyright (C) 2025 Zach Bacon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef CPI_TASKBARINTEGRATION_H
#define CPI_TASKBARINTEGRATION_H

// Initialize taskbar integration (call early, registers window message)
void CPI_Taskbar_Init(void);

// Cleanup taskbar integration (call on WM_DESTROY)
void CPI_Taskbar_Uninit(void);

// Update thumbnail toolbar button states based on current player state
void CPI_Taskbar_UpdateButtons(void);

// Handle the dynamically registered TaskbarButtonCreated message.
// Returns TRUE if the message was handled.
BOOL CPI_Taskbar_HandleMessage(HWND hWnd, UINT message);

#endif
