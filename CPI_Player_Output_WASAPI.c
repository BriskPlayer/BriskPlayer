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

#include "stdafx.h"
#include "globals.h"
#include "CPI_Player.h"
#include "CPI_Player_CoDec.h"
#include "CPI_Player_Output.h"
#include "CPI_Equaliser.h"
#include "CPI_Player_DSP.h"
#include "CPI_ReplayGain.h"

// Need COBJMACROS for C-style COM interface helper macros
#ifndef COBJMACROS
#define COBJMACROS
#endif

#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>

////////////////////////////////////////////////////////////////////////////////
//
// WASAPI output module (shared mode).
//
// Uses the Windows Audio Session API for low-latency, modern audio output.
// Shared mode allows other applications to play audio simultaneously.
//
////////////////////////////////////////////////////////////////////////////////

// Buffer timing — how much audio we request from the codec per refill pass
#define CPC_WASAPI_BUFFER_DURATION_MS  40   // 40 ms per fill pass
#define CPC_WASAPI_REFILL_THRESHOLD_MS 20   // refill when < 20 ms remains

////////////////////////////////////////////////////////////////////////////////
// GUIDs — defined here so we don't depend on extra libs

// {A95664D2-9614-4F35-A746-DE8DB63617E6}
DEFINE_GUID(CLSID_MMDeviceEnumerator_BP,
    0xBCDE0395, 0xE52F, 0x467C,
    0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);

// {BCDE0395-E52F-467C-8E3D-C4579291692E} — actually this IS the CLSID

// {A95664D2-9614-4F35-A746-DE8DB63617E6}
DEFINE_GUID(IID_IMMDeviceEnumerator_BP,
    0xA95664D2, 0x9614, 0x4F35,
    0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);

// {1CB9AD4C-DBFA-4c32-B178-C2F568A703B2}
DEFINE_GUID(IID_IAudioClient_BP,
    0x1CB9AD4C, 0xDBFA, 0x4c32,
    0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2);

// {F294ACFC-3146-4483-A7BF-ADDCA7C260E2}
DEFINE_GUID(IID_IAudioRenderClient_BP,
    0xF294ACFC, 0x3146, 0x4483,
    0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2);

// {87CE5498-68D6-44E5-9215-6DA47EF883D8}
DEFINE_GUID(IID_ISimpleAudioVolume_BP,
    0x87CE5498, 0x68D6, 0x44E5,
    0x92, 0x15, 0x6D, 0xA4, 0x7E, 0xF8, 0x83, 0xD8);

////////////////////////////////////////////////////////////////////////////////

typedef struct __CPs_OutputContext_WASAPI
{
    IMMDeviceEnumerator* m_pEnumerator;
    IMMDevice*           m_pDevice;
    IAudioClient*        m_pAudioClient;
    IAudioRenderClient*  m_pRenderClient;
    ISimpleAudioVolume*  m_pSimpleVolume;

    WAVEFORMATEX*        m_pMixFormat;       // Device mix format (allocated by WASAPI)
    WAVEFORMATEX         m_SourceFormat;     // Source PCM format from codec

    UINT32               m_uBufferFrames;    // Total buffer size in frames
    BOOL                 m_bInitialized;
    BOOL                 m_bPaused;
    BOOL                 m_bStarted;

    // Temp PCM buffer for codec → resampled copy
    BYTE*                m_pConvertBuf;
    DWORD                m_dwConvertBufSize;

    CPs_EqualiserModule* m_pEqualiser;
} CPs_OutputContext_WASAPI;

////////////////////////////////////////////////////////////////////////////////
// Forward declarations

void CPP_OMWA_Initialise(CPs_OutputModule* pModule, const CPs_FileInfo* pFileInfo, CP_HEQUALISER hEqualiser);
void CPP_OMWA_Uninitialise(CPs_OutputModule* pModule);
void CPP_OMWA_RefillBuffers(CPs_OutputModule* pModule);
void CPP_OMWA_SetPause(CPs_OutputModule* pModule, const BOOL bPause);
BOOL CPP_OMWA_IsOutputComplete(CPs_OutputModule* pModule);
void CPP_OMWA_Flush(CPs_OutputModule* pModule);
void CPP_OMWA_OnEQChanged(CPs_OutputModule* pModule);
void CPP_OMWA_SetInternalVolume(CPs_OutputModule* pModule, const int iNewVolume);

////////////////////////////////////////////////////////////////////////////////
// Helpers

// Convert samples from source format to WASAPI mix format in-place or to dest.
// Currently handles: 8-bit unsigned → 16-bit signed, mono → stereo, and
// sample rate conversion via simple nearest-neighbour resampling.
// Returns number of bytes written to pDest.
static DWORD ConvertPCM(
    const BYTE* pSrc, DWORD dwSrcBytes,
    BYTE* pDest, DWORD dwDestMaxBytes,
    const WAVEFORMATEX* pSrcFmt,
    const WAVEFORMATEX* pDstFmt)
{
    // Determine source sample properties
    int srcChannels  = pSrcFmt->nChannels;
    int srcBits      = pSrcFmt->wBitsPerSample;
    DWORD srcRate    = pSrcFmt->nSamplesPerSec;
    int srcFrameSize = (srcBits / 8) * srcChannels;

    int dstChannels  = pDstFmt->nChannels;
    int dstBits      = pDstFmt->wBitsPerSample;
    DWORD dstRate    = pDstFmt->nSamplesPerSec;
    int dstFrameSize = (dstBits / 8) * dstChannels;

    if (srcFrameSize == 0 || dstFrameSize == 0)
        return 0;

    DWORD srcFrames = dwSrcBytes / srcFrameSize;
    // Calculate output frames with resampling ratio
    DWORD dstFrames = (DWORD)((double)srcFrames * dstRate / srcRate);
    DWORD dstBytes  = dstFrames * dstFrameSize;

    if (dstBytes > dwDestMaxBytes)
    {
        dstFrames = dwDestMaxBytes / dstFrameSize;
        dstBytes  = dstFrames * dstFrameSize;
    }

    for (DWORD d = 0; d < dstFrames; d++)
    {
        // Map destination frame back to source frame (nearest-neighbour)
        DWORD s = (DWORD)((double)d * srcRate / dstRate);
        if (s >= srcFrames) s = srcFrames - 1;

        const BYTE* pSrcFrame = pSrc + s * srcFrameSize;

        // Read source sample(s) as 16-bit signed
        short samples[2] = { 0, 0 };  // L, R
        if (srcBits == 16)
        {
            samples[0] = ((const short*)pSrcFrame)[0];
            if (srcChannels >= 2)
                samples[1] = ((const short*)pSrcFrame)[1];
            else
                samples[1] = samples[0]; // mono → duplicate
        }
        else if (srcBits == 8)
        {
            // 8-bit unsigned → 16-bit signed
            samples[0] = (short)((pSrcFrame[0] - 128) << 8);
            if (srcChannels >= 2)
                samples[1] = (short)((pSrcFrame[1] - 128) << 8);
            else
                samples[1] = samples[0];
        }

        // Write output sample(s) as 16-bit signed
        BYTE* pDstFrame = pDest + d * dstFrameSize;
        if (dstBits == 16)
        {
            ((short*)pDstFrame)[0] = samples[0];
            if (dstChannels >= 2)
                ((short*)pDstFrame)[1] = samples[1];
        }
        else if (dstBits == 8)
        {
            pDstFrame[0] = (BYTE)((samples[0] >> 8) + 128);
            if (dstChannels >= 2)
                pDstFrame[1] = (BYTE)((samples[1] >> 8) + 128);
        }
        // 32-bit float output (WASAPI often uses this in shared mode)
        else if (dstBits == 32)
        {
            ((float*)pDstFrame)[0] = samples[0] / 32768.0f;
            if (dstChannels >= 2)
                ((float*)pDstFrame)[1] = samples[1] / 32768.0f;
            // Fill remaining channels with silence
            for (int ch = 2; ch < dstChannels; ch++)
                ((float*)pDstFrame)[ch] = 0.0f;
        }
    }

    return dstBytes;
}

// Check if source and mix formats match exactly (no conversion needed)
static BOOL FormatsMatch(const WAVEFORMATEX* pSrc, const WAVEFORMATEX* pDst)
{
    return pSrc->nChannels == pDst->nChannels
        && pSrc->nSamplesPerSec == pDst->nSamplesPerSec
        && pSrc->wBitsPerSample == pDst->wBitsPerSample
        && pSrc->wFormatTag == pDst->wFormatTag;
}

////////////////////////////////////////////////////////////////////////////////
// Module init (one-off function pointer setup)

void CPI_Player_Output_Initialise_WASAPI(CPs_OutputModule* pModule)
{
    pModule->Initialise       = CPP_OMWA_Initialise;
    pModule->Uninitialise     = CPP_OMWA_Uninitialise;
    pModule->RefillBuffers    = CPP_OMWA_RefillBuffers;
    pModule->SetPause         = CPP_OMWA_SetPause;
    pModule->IsOutputComplete = CPP_OMWA_IsOutputComplete;
    pModule->Flush            = CPP_OMWA_Flush;
    pModule->OnEQChanged      = CPP_OMWA_OnEQChanged;
    pModule->SetInternalVolume = CPP_OMWA_SetInternalVolume;
    pModule->m_pModuleCookie  = NULL;
    pModule->m_pcModuleName   = "WASAPI Output";
    pModule->m_pCoDec         = NULL;
    pModule->m_pEqualiser     = NULL;
    pModule->m_fReplayGainScale = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////
// Initialise — called when playback of a file begins

void CPP_OMWA_Initialise(CPs_OutputModule* pModule, const CPs_FileInfo* pFileInfo, CP_HEQUALISER hEqualiser)
{
    CPs_OutputContext_WASAPI* pContext;
    HRESULT hr;
    REFERENCE_TIME hnsRequestedDuration;

    CP_ASSERT(pModule->m_pModuleCookie == NULL);

    pContext = (CPs_OutputContext_WASAPI*)SAFE_MALLOC(sizeof(CPs_OutputContext_WASAPI));
    if (!pContext)
    {
        CP_FAIL("Failed to allocate WASAPI context");
        return;
    }
    memset(pContext, 0, sizeof(*pContext));
    pModule->m_pModuleCookie = pContext;

    CP_TRACE0("WASAPI initialising");

    // Create sync event for the engine's WaitForSingleObject loop
    pModule->m_evtBlockFree = CreateEvent(NULL, FALSE, FALSE, NULL);

    // Initialise COM for this thread (engine thread)
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    // Get default audio endpoint
    hr = CoCreateInstance(
        &CLSID_MMDeviceEnumerator_BP, NULL, CLSCTX_ALL,
        &IID_IMMDeviceEnumerator_BP, (void**)&pContext->m_pEnumerator);
    if (FAILED(hr))
    {
        CP_TRACE1("WASAPI: CoCreateInstance MMDeviceEnumerator failed 0x%X", hr);
        CPP_OMWA_Uninitialise(pModule);
        return;
    }

    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(
        pContext->m_pEnumerator, eRender, eConsole, &pContext->m_pDevice);
    if (FAILED(hr))
    {
        CP_TRACE1("WASAPI: GetDefaultAudioEndpoint failed 0x%X", hr);
        CPP_OMWA_Uninitialise(pModule);
        return;
    }

    // Activate audio client
    hr = IMMDevice_Activate(
        pContext->m_pDevice, &IID_IAudioClient_BP, CLSCTX_ALL,
        NULL, (void**)&pContext->m_pAudioClient);
    if (FAILED(hr))
    {
        CP_TRACE1("WASAPI: IMMDevice_Activate failed 0x%X", hr);
        CPP_OMWA_Uninitialise(pModule);
        return;
    }

    // Get the device's mix format (shared mode requires this)
    hr = IAudioClient_GetMixFormat(pContext->m_pAudioClient, &pContext->m_pMixFormat);
    if (FAILED(hr))
    {
        CP_TRACE1("WASAPI: GetMixFormat failed 0x%X", hr);
        CPP_OMWA_Uninitialise(pModule);
        return;
    }

    // Store the source format from the codec
    pContext->m_SourceFormat.wFormatTag     = WAVE_FORMAT_PCM;
    pContext->m_SourceFormat.nChannels      = pFileInfo->m_bStereo ? 2 : 1;
    pContext->m_SourceFormat.nSamplesPerSec = pFileInfo->m_iFreq_Hz;
    pContext->m_SourceFormat.wBitsPerSample = pFileInfo->m_b16bit ? 16 : 8;
    pContext->m_SourceFormat.nBlockAlign    = (pContext->m_SourceFormat.nChannels * pContext->m_SourceFormat.wBitsPerSample) / 8;
    pContext->m_SourceFormat.nAvgBytesPerSec = pContext->m_SourceFormat.nSamplesPerSec * pContext->m_SourceFormat.nBlockAlign;
    pContext->m_SourceFormat.cbSize         = 0;

    // For WASAPI shared mode, use the device's preferred format.
    // If the mix format uses WAVE_FORMAT_EXTENSIBLE with a PCM/IEEE_FLOAT sub-format,
    // we keep it; we'll convert our source PCM at fill time.

    // Request a 40 ms buffer (in 100-ns units)
    hnsRequestedDuration = (REFERENCE_TIME)(CPC_WASAPI_BUFFER_DURATION_MS * 10000LL);

    hr = IAudioClient_Initialize(
        pContext->m_pAudioClient,
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
        hnsRequestedDuration,
        0,  // periodicity (must be 0 for shared mode)
        pContext->m_pMixFormat,
        NULL);  // session GUID
    if (FAILED(hr))
    {
        CP_TRACE1("WASAPI: IAudioClient_Initialize failed 0x%X", hr);
        CPP_OMWA_Uninitialise(pModule);
        return;
    }

    // Set the event handle that WASAPI will signal when it needs more data
    hr = IAudioClient_SetEventHandle(pContext->m_pAudioClient, pModule->m_evtBlockFree);
    if (FAILED(hr))
    {
        CP_TRACE1("WASAPI: SetEventHandle failed 0x%X", hr);
        CPP_OMWA_Uninitialise(pModule);
        return;
    }

    // Get buffer size
    hr = IAudioClient_GetBufferSize(pContext->m_pAudioClient, &pContext->m_uBufferFrames);
    if (FAILED(hr))
    {
        CP_TRACE1("WASAPI: GetBufferSize failed 0x%X", hr);
        CPP_OMWA_Uninitialise(pModule);
        return;
    }

    // Get render client
    hr = IAudioClient_GetService(
        pContext->m_pAudioClient, &IID_IAudioRenderClient_BP,
        (void**)&pContext->m_pRenderClient);
    if (FAILED(hr))
    {
        CP_TRACE1("WASAPI: GetService IAudioRenderClient failed 0x%X", hr);
        CPP_OMWA_Uninitialise(pModule);
        return;
    }

    // Get simple volume control for per-session volume
    hr = IAudioClient_GetService(
        pContext->m_pAudioClient, &IID_ISimpleAudioVolume_BP,
        (void**)&pContext->m_pSimpleVolume);
    if (FAILED(hr))
    {
        // Non-fatal: we just won't have volume control
        CP_TRACE1("WASAPI: GetService ISimpleAudioVolume failed 0x%X", hr);
        pContext->m_pSimpleVolume = NULL;
    }

    // Allocate conversion buffer (enough for a full WASAPI buffer worth of source data)
    {
        DWORD srcBytesPerFrame = pContext->m_SourceFormat.nBlockAlign;
        // Estimate how many source frames we need for a full WASAPI buffer
        DWORD srcFramesNeeded = (DWORD)(
            (double)pContext->m_uBufferFrames
            * pContext->m_SourceFormat.nSamplesPerSec
            / pContext->m_pMixFormat->nSamplesPerSec) + 16;  // +margin
        pContext->m_dwConvertBufSize = srcFramesNeeded * srcBytesPerFrame;
        if (pContext->m_dwConvertBufSize < 0x10000)
            pContext->m_dwConvertBufSize = 0x10000;  // Min 64 KB
        pContext->m_pConvertBuf = (BYTE*)SAFE_MALLOC(pContext->m_dwConvertBufSize);
        if (!pContext->m_pConvertBuf)
        {
            CPP_OMWA_Uninitialise(pModule);
            CP_FAIL("WASAPI: Failed to allocate conversion buffer");
            return;
        }
    }

    // Store EQ reference
    pContext->m_pEqualiser = (CPs_EqualiserModule*)hEqualiser;
    pModule->m_pEqualiser  = hEqualiser;

    // Apply saved volume
    if (pContext->m_pSimpleVolume)
    {
        float fVol = (float)globals.m_iVolume / 100.0f;
        ISimpleAudioVolume_SetMasterVolume(pContext->m_pSimpleVolume, fVol, NULL);
    }

    pContext->m_bInitialized = TRUE;
    pContext->m_bPaused      = FALSE;
    pContext->m_bStarted     = FALSE;

    // Start the audio client
    hr = IAudioClient_Start(pContext->m_pAudioClient);
    if (FAILED(hr))
    {
        CP_TRACE1("WASAPI: IAudioClient_Start failed 0x%X", hr);
        CPP_OMWA_Uninitialise(pModule);
        return;
    }
    pContext->m_bStarted = TRUE;

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
}

////////////////////////////////////////////////////////////////////////////////
// Uninitialise — release all WASAPI/COM resources

void CPP_OMWA_Uninitialise(CPs_OutputModule* pModule)
{
    CPs_OutputContext_WASAPI* pContext = (CPs_OutputContext_WASAPI*)pModule->m_pModuleCookie;

    if (!pContext)
        return;

    CP_TRACE0("WASAPI uninitialising");
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

    // Stop audio client
    if (pContext->m_pAudioClient && pContext->m_bStarted)
    {
        IAudioClient_Stop(pContext->m_pAudioClient);
        pContext->m_bStarted = FALSE;
    }

    // Release COM interfaces in reverse order
    if (pContext->m_pSimpleVolume)
    {
        ISimpleAudioVolume_Release(pContext->m_pSimpleVolume);
        pContext->m_pSimpleVolume = NULL;
    }

    if (pContext->m_pRenderClient)
    {
        IAudioRenderClient_Release(pContext->m_pRenderClient);
        pContext->m_pRenderClient = NULL;
    }

    if (pContext->m_pAudioClient)
    {
        IAudioClient_Release(pContext->m_pAudioClient);
        pContext->m_pAudioClient = NULL;
    }

    if (pContext->m_pDevice)
    {
        IMMDevice_Release(pContext->m_pDevice);
        pContext->m_pDevice = NULL;
    }

    if (pContext->m_pEnumerator)
    {
        IMMDeviceEnumerator_Release(pContext->m_pEnumerator);
        pContext->m_pEnumerator = NULL;
    }

    // Free mix format (allocated by WASAPI via CoTaskMemAlloc)
    if (pContext->m_pMixFormat)
    {
        CoTaskMemFree(pContext->m_pMixFormat);
        pContext->m_pMixFormat = NULL;
    }

    // Free conversion buffer
    if (pContext->m_pConvertBuf)
    {
        free(pContext->m_pConvertBuf);
        pContext->m_pConvertBuf = NULL;
    }

    // Close event
    if (pModule->m_evtBlockFree)
    {
        CloseHandle(pModule->m_evtBlockFree);
        pModule->m_evtBlockFree = NULL;
    }

    free(pContext);
    pModule->m_pModuleCookie = NULL;

    CoUninitialize();
}

////////////////////////////////////////////////////////////////////////////////
// RefillBuffers — called by engine when the event fires

void CPP_OMWA_RefillBuffers(CPs_OutputModule* pModule)
{
    CPs_OutputContext_WASAPI* pContext = (CPs_OutputContext_WASAPI*)pModule->m_pModuleCookie;
    HRESULT hr;
    UINT32 uPadding = 0;
    UINT32 uAvailFrames;
    BYTE*  pData = NULL;
    BOOL   bNeedConvert;

    if (!pContext || !pContext->m_bInitialized || !pModule->m_pCoDec)
        return;

    // How many frames are already queued?
    hr = IAudioClient_GetCurrentPadding(pContext->m_pAudioClient, &uPadding);
    if (FAILED(hr))
        return;

    uAvailFrames = pContext->m_uBufferFrames - uPadding;
    if (uAvailFrames == 0)
        return;

    bNeedConvert = !FormatsMatch(&pContext->m_SourceFormat, pContext->m_pMixFormat);

    if (bNeedConvert)
    {
        // Read source PCM into conversion buffer, then convert into WASAPI buffer
        // Calculate how many source frames we need for uAvailFrames output frames
        DWORD srcFramesNeeded = (DWORD)(
            (double)uAvailFrames
            * pContext->m_SourceFormat.nSamplesPerSec
            / pContext->m_pMixFormat->nSamplesPerSec) + 1;
        DWORD srcBytesNeeded = srcFramesNeeded * pContext->m_SourceFormat.nBlockAlign;
        if (srcBytesNeeded > pContext->m_dwConvertBufSize)
            srcBytesNeeded = pContext->m_dwConvertBufSize;

        DWORD dwBytesRead = srcBytesNeeded;
        BOOL bMoreData = pModule->m_pCoDec->GetPCMBlock(
            pModule->m_pCoDec, pContext->m_pConvertBuf, &dwBytesRead);

        if (dwBytesRead > 0)
        {
            // Apply EQ to source data (before format conversion)
            if (pContext->m_pEqualiser)
            {
                pContext->m_pEqualiser->ApplyEQToBlock_Inplace(
                    pContext->m_pEqualiser, pContext->m_pConvertBuf, dwBytesRead);
            }

            // Apply ReplayGain to source data
            CPRG_ApplyToBlock(pContext->m_pConvertBuf, dwBytesRead, pModule->m_fReplayGainScale);

            // Apply DSP to source data
            if (CPDSP_IsActive())
            {
                int bytesPerSample = pContext->m_SourceFormat.nBlockAlign;
                int numSamples = dwBytesRead / bytesPerSample;
                CPDSP_ProcessSamples((short int*)pContext->m_pConvertBuf, numSamples,
                    pContext->m_SourceFormat.wBitsPerSample,
                    pContext->m_SourceFormat.nChannels,
                    pContext->m_SourceFormat.nSamplesPerSec);
            }

            // Get WASAPI buffer
            DWORD dstBytesMax = uAvailFrames * pContext->m_pMixFormat->nBlockAlign;
            hr = IAudioRenderClient_GetBuffer(pContext->m_pRenderClient, uAvailFrames, &pData);
            if (SUCCEEDED(hr) && pData)
            {
                DWORD dstBytes = ConvertPCM(
                    pContext->m_pConvertBuf, dwBytesRead,
                    pData, dstBytesMax,
                    &pContext->m_SourceFormat, pContext->m_pMixFormat);
                UINT32 framesWritten = (pContext->m_pMixFormat->nBlockAlign > 0)
                    ? dstBytes / pContext->m_pMixFormat->nBlockAlign : 0;

                // If we wrote fewer frames than requested, pad remainder with silence
                if (framesWritten < uAvailFrames)
                {
                    DWORD silenceBytes = (uAvailFrames - framesWritten) * pContext->m_pMixFormat->nBlockAlign;
                    memset(pData + dstBytes, 0, silenceBytes);
                }

                IAudioRenderClient_ReleaseBuffer(pContext->m_pRenderClient, uAvailFrames, 0);
            }
            else if (pData)
            {
                IAudioRenderClient_ReleaseBuffer(pContext->m_pRenderClient, 0, 0);
            }
        }

        if (!bMoreData)
        {
            pModule->m_pCoDec->CloseFile(pModule->m_pCoDec);
            pModule->m_pCoDec = NULL;
        }
    }
    else
    {
        // Formats match — write directly into WASAPI buffer
        hr = IAudioRenderClient_GetBuffer(pContext->m_pRenderClient, uAvailFrames, &pData);
        if (FAILED(hr) || !pData)
            return;

        DWORD dwBytesNeeded = uAvailFrames * pContext->m_pMixFormat->nBlockAlign;
        DWORD dwBytesRead = dwBytesNeeded;
        BOOL bMoreData = pModule->m_pCoDec->GetPCMBlock(pModule->m_pCoDec, pData, &dwBytesRead);

        // Apply EQ
        if (pContext->m_pEqualiser && dwBytesRead > 0)
        {
            pContext->m_pEqualiser->ApplyEQToBlock_Inplace(
                pContext->m_pEqualiser, pData, dwBytesRead);
        }

        // Apply ReplayGain
        if (dwBytesRead > 0)
            CPRG_ApplyToBlock(pData, dwBytesRead, pModule->m_fReplayGainScale);

        // Apply DSP
        if (dwBytesRead > 0 && CPDSP_IsActive())
        {
            int bytesPerSample = pContext->m_pMixFormat->nBlockAlign;
            int numSamples = dwBytesRead / bytesPerSample;
            CPDSP_ProcessSamples((short int*)pData, numSamples,
                pContext->m_pMixFormat->wBitsPerSample,
                pContext->m_pMixFormat->nChannels,
                pContext->m_pMixFormat->nSamplesPerSec);
        }

        // Pad any remaining space with silence
        if (dwBytesRead < dwBytesNeeded)
            memset(pData + dwBytesRead, 0, dwBytesNeeded - dwBytesRead);

        UINT32 framesWritten = (pContext->m_pMixFormat->nBlockAlign > 0)
            ? dwBytesRead / pContext->m_pMixFormat->nBlockAlign : 0;
        // Release the full buffer (silence-padded)
        IAudioRenderClient_ReleaseBuffer(pContext->m_pRenderClient, uAvailFrames, 0);

        if (!bMoreData)
        {
            pModule->m_pCoDec->CloseFile(pModule->m_pCoDec);
            pModule->m_pCoDec = NULL;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Pause/Resume

void CPP_OMWA_SetPause(CPs_OutputModule* pModule, const BOOL bPause)
{
    CPs_OutputContext_WASAPI* pContext = (CPs_OutputContext_WASAPI*)pModule->m_pModuleCookie;

    if (!pContext || !pContext->m_bInitialized || !pContext->m_pAudioClient)
        return;

    if (bPause)
    {
        IAudioClient_Stop(pContext->m_pAudioClient);
        pContext->m_bPaused  = TRUE;
        pContext->m_bStarted = FALSE;
    }
    else
    {
        IAudioClient_Start(pContext->m_pAudioClient);
        pContext->m_bPaused  = FALSE;
        pContext->m_bStarted = TRUE;
    }
}

////////////////////////////////////////////////////////////////////////////////
// IsOutputComplete — TRUE when all submitted data has been rendered

BOOL CPP_OMWA_IsOutputComplete(CPs_OutputModule* pModule)
{
    CPs_OutputContext_WASAPI* pContext = (CPs_OutputContext_WASAPI*)pModule->m_pModuleCookie;
    UINT32 uPadding = 0;

    if (!pContext || !pContext->m_bInitialized)
        return TRUE;

    if (pModule->m_pCoDec)
        return FALSE;

    // Check if any audio is still buffered
    if (SUCCEEDED(IAudioClient_GetCurrentPadding(pContext->m_pAudioClient, &uPadding)))
        return (uPadding == 0);

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Flush — discard pending audio (used on seek/stop)

void CPP_OMWA_Flush(CPs_OutputModule* pModule)
{
    CPs_OutputContext_WASAPI* pContext = (CPs_OutputContext_WASAPI*)pModule->m_pModuleCookie;

    if (!pContext || !pContext->m_bInitialized || !pContext->m_pAudioClient)
        return;

    // Stop, reset, restart
    IAudioClient_Stop(pContext->m_pAudioClient);
    IAudioClient_Reset(pContext->m_pAudioClient);

    if (!pContext->m_bPaused)
    {
        IAudioClient_Start(pContext->m_pAudioClient);
        pContext->m_bStarted = TRUE;
    }
    else
    {
        pContext->m_bStarted = FALSE;
    }
}

////////////////////////////////////////////////////////////////////////////////
// EQ changed — nothing special needed, reapplied at fill time

void CPP_OMWA_OnEQChanged(CPs_OutputModule* pModule)
{
    (void)pModule;
}

////////////////////////////////////////////////////////////////////////////////
// Volume

void CPP_OMWA_SetInternalVolume(CPs_OutputModule* pModule, const int iNewVolume)
{
    CPs_OutputContext_WASAPI* pContext = (CPs_OutputContext_WASAPI*)pModule->m_pModuleCookie;

    if (!pContext || !pContext->m_bInitialized || !pContext->m_pSimpleVolume)
        return;

    // Linear 0..100 → 0.0..1.0
    float fVol = (float)iNewVolume / 100.0f;
    if (fVol < 0.0f) fVol = 0.0f;
    if (fVol > 1.0f) fVol = 1.0f;

    ISimpleAudioVolume_SetMasterVolume(pContext->m_pSimpleVolume, fVol, NULL);
}
