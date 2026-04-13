/*
 * BriskPlayer - ReplayGain support
 * Copyright (C) 2025 Zach Bacon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "stdafx.h"
#include "CPI_ReplayGain.h"
#include <math.h>

float CPRG_ComputeScale(CPe_ReplayGainMode enMode,
                        float fTrackGain, float fTrackPeak,
                        float fAlbumGain, float fAlbumPeak,
                        float fPreampDb, BOOL bPreventClipping)
{
	float fGainDb, fPeak, fScale;

	if (enMode == CP_RGMODE_OFF)
		return 1.0f;

	if (enMode == CP_RGMODE_ALBUM && fAlbumGain != 0.0f)
	{
		fGainDb = fAlbumGain;
		fPeak = fAlbumPeak;
	}
	else if (fTrackGain != 0.0f)
	{
		fGainDb = fTrackGain;
		fPeak = fTrackPeak;
	}
	else
		return 1.0f;  // No gain data

	fGainDb += fPreampDb;
	fScale = powf(10.0f, fGainDb / 20.0f);

	if (bPreventClipping && fPeak > 0.0f && fPeak * fScale > 1.0f)
		fScale = 1.0f / fPeak;

	return fScale;
}

void CPRG_ApplyToBlock(void* pBlock, DWORD dwBlockSize, float fScale)
{
	short* pSamples;
	DWORD dwSampleCount, i;
	int iVal;

	if (fScale == 1.0f || dwBlockSize == 0)
		return;

	pSamples = (short*)pBlock;
	dwSampleCount = dwBlockSize / sizeof(short);

	for (i = 0; i < dwSampleCount; i++)
	{
		iVal = (int)(pSamples[i] * fScale);

		if (iVal > 32767)
			iVal = 32767;
		else if (iVal < -32768)
			iVal = -32768;

		pSamples[i] = (short)iVal;
	}
}
