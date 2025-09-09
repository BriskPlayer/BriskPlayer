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



#include "stdafx.h"
#include "globals.h"
#include "WindowsOS.h"



wp_GetMonitorInfo pfnGetMonitorInfo;
wp_MonitorFromWindow pfnMonitorFromWindow;
wp_TrackMouseEvent pfnTrackMouseEvent;
////////////////////////////////////////////////////////////////////////////////
//
//
//
void CP_InitWindowsRoutines(void)
{
    HMODULE hmUser32 = GetModuleHandle("USER32");
    
    // Use unions to safely cast function pointers
    union {
        FARPROC proc;
        wp_GetMonitorInfo func;
    } getMonitorInfo;
    
    union {
        FARPROC proc;
        wp_MonitorFromWindow func;
    } monitorFromWindow;
    
    union {
        FARPROC proc;
        wp_TrackMouseEvent func;
    } trackMouseEvent;
    
    getMonitorInfo.proc = GetProcAddress(hmUser32, "GetMonitorInfoA");
    pfnGetMonitorInfo = getMonitorInfo.func;
    
    monitorFromWindow.proc = GetProcAddress(hmUser32, "MonitorFromWindow");
    pfnMonitorFromWindow = monitorFromWindow.func;
    
    trackMouseEvent.proc = GetProcAddress(hmUser32, "TrackMouseEvent");
    pfnTrackMouseEvent = trackMouseEvent.func;
}
//
//
//
