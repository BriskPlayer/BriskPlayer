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
// Thin C shims for the Rust internet-stream module.
//
// All implementation logic lives in rust/codecs/src/stream_internet.rs.
// This file exists only to expose the CPs_CircleBuffer vtable to Rust:
// the struct has two layouts (legacy vs C23 threading) so Rust cannot
// mirror it safely — we dispatch through the vtable from C instead.
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "globals.h"
#include "CPI_CircleBuffer.h"

void         CPCB_Write      (CPs_CircleBuffer* cb, const void* src, unsigned int n) { cb->Write(cb, src, n); }
BOOL         CPCB_Read       (CPs_CircleBuffer* cb, void* dst, size_t n, size_t* out) { return cb->Read(cb, dst, n, out); }
unsigned int CPCB_GetUsedSize(CPs_CircleBuffer* cb) { return cb->GetUsedSize(cb); }
unsigned int CPCB_GetFreeSize(CPs_CircleBuffer* cb) { return cb->GetFreeSize(cb); }
void         CPCB_SetComplete(CPs_CircleBuffer* cb)  { cb->SetComplete(cb); }
BOOL         CPCB_IsComplete (CPs_CircleBuffer* cb)  { return cb->IsComplete(cb); }
void         CPCB_Uninitialise(CPs_CircleBuffer* cb) { cb->Uninitialise(cb); }
