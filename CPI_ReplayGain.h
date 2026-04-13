/*
 * BriskPlayer - ReplayGain support
 * Copyright (C) 2025 Zach Bacon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef CPI_REPLAYGAIN_H
#define CPI_REPLAYGAIN_H

// ReplayGain mode
typedef enum {
	CP_RGMODE_OFF = 0,
	CP_RGMODE_TRACK,
	CP_RGMODE_ALBUM
} CPe_ReplayGainMode;

// Compute the linear scale factor from ReplayGain metadata.
// Returns 1.0f if mode is off or no gain data is available.
float CPRG_ComputeScale(CPe_ReplayGainMode enMode,
                        float fTrackGain, float fTrackPeak,
                        float fAlbumGain, float fAlbumPeak,
                        float fPreampDb, BOOL bPreventClipping);

// Apply ReplayGain scaling to a PCM block in-place (16-bit only).
void CPRG_ApplyToBlock(void* pBlock, DWORD dwBlockSize, float fScale);

#endif /* CPI_REPLAYGAIN_H */
