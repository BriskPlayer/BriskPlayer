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
#include "CPI_Player.h"
#include "CPI_Player_CoDec.h"
#include "CPI_Player_Output.h"
#include "CPI_Equaliser.h"

#ifdef HAVE_FAUDIO
#include <FAudio.h>

////////////////////////////////////////////////////////////////////////////////
//
// This is an output stage that uses FAudio (cross-platform XAudio2 reimplementation).
// FAudio provides modern, low-latency audio capabilities with support for
// advanced features like hardware acceleration and 3D audio positioning.
//
////////////////////////////////////////////////////////////////////////////////

// Buffer configuration
#define CPC_FAUDIO_NUMBEROFBUFFERS 4
#define CPC_FAUDIO_BUFFERSIZE_MS 40  // 40ms per buffer for low latency
#define CPC_FAUDIO_MAX_SAMPLE_RATE 192000

////////////////////////////////////////////////////////////////////////////////
// Voice callback structure (forward declaration)

typedef struct __FAudioVoiceCallbackImpl
{
	FAudioVoiceCallback callback;
	CPs_OutputModule* pModule;
} FAudioVoiceCallbackImpl;

////////////////////////////////////////////////////////////////////////////////

typedef struct __CPs_OutputContext_FAudio
{
	FAudio* m_pFAudio;
	FAudioMasteringVoice* m_pMasteringVoice;
	FAudioSourceVoice* m_pSourceVoice;
	
	// Buffer management
	BYTE* m_pBuffers[CPC_FAUDIO_NUMBEROFBUFFERS];
	DWORD m_dwBufferSize;
	int m_iCurrentBuffer;
	BOOL m_bBuffersSubmitted[CPC_FAUDIO_NUMBEROFBUFFERS];
	
	// Format information
	FAudioWaveFormatEx m_WaveFormat;
	
	// State management
	BOOL m_bInitialized;
	BOOL m_bPaused;
	float m_fVolume;
	
	// Synchronization
	HANDLE m_evtBufferComplete;
	CRITICAL_SECTION m_csBufferLock;
	
	// Voice callback
	FAudioVoiceCallbackImpl* m_pVoiceCallback;
	
	// Equalizer
	CPs_EqualiserModule* m_pEqualiser;
	
} CPs_OutputContext_FAudio;

////////////////////////////////////////////////////////////////////////////////
// Forward declarations

void CPP_OMFA_Initialise(CPs_OutputModule* pModule, const CPs_FileInfo* pFileInfo, CP_HEQUALISER hEqualiser);
void CPP_OMFA_Uninitialise(CPs_OutputModule* pModule);
void CPP_OMFA_RefillBuffers(CPs_OutputModule* pModule);
void CPP_OMFA_SetPause(CPs_OutputModule* pModule, const BOOL bPause);
BOOL CPP_OMFA_IsOutputComplete(CPs_OutputModule* pModule);
void CPP_OMFA_Flush(CPs_OutputModule* pModule);
void CPP_OMFA_OnEQChanged(CPs_OutputModule* pModule);
void CPP_OMFA_SetInternalVolume(CPs_OutputModule* pModule, const int iNewVolume);

// FAudio callback
void FAUDIOCALL CPP_OMFA_VoiceCallback(FAudioVoiceCallback* callback, void* pBufferContext);

////////////////////////////////////////////////////////////////////////////////
// Implementation

void CPI_Player_Output_Initialise_FAudio(CPs_OutputModule* pModule)
{
	// This is a one off call to set up the function pointers
	pModule->Initialise = CPP_OMFA_Initialise;
	pModule->Uninitialise = CPP_OMFA_Uninitialise;
	pModule->RefillBuffers = CPP_OMFA_RefillBuffers;
	pModule->SetPause = CPP_OMFA_SetPause;
	pModule->IsOutputComplete = CPP_OMFA_IsOutputComplete;
	pModule->Flush = CPP_OMFA_Flush;
	pModule->OnEQChanged = CPP_OMFA_OnEQChanged;
	pModule->SetInternalVolume = CPP_OMFA_SetInternalVolume;
	pModule->m_pModuleCookie = NULL;
	pModule->m_pcModuleName = "FAudio Output";
	pModule->m_pCoDec = NULL;
	pModule->m_pEqualiser = NULL;
}

void CPP_OMFA_Initialise(CPs_OutputModule* pModule, const CPs_FileInfo* pFileInfo, CP_HEQUALISER hEqualiser)
{
	CPs_OutputContext_FAudio* pContext;
	HRESULT hr;
	FAudioVoiceCallbackImpl* pVoiceCallback;
	
	CP_ASSERT(pModule->m_pModuleCookie == NULL);
	
	// Create context
	pContext = (CPs_OutputContext_FAudio*)malloc(sizeof(CPs_OutputContext_FAudio));
	if (!pContext)
	{
		CP_FAIL("Failed to allocate FAudio context");
		return;
	}
	
	memset(pContext, 0, sizeof(*pContext));
	pModule->m_pModuleCookie = pContext;
	
	CP_TRACE0("FAudio initialising");
	
	// Initialize synchronization objects
	pContext->m_evtBufferComplete = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!pContext->m_evtBufferComplete)
	{
		CPP_OMFA_Uninitialise(pModule);
		CP_FAIL("Failed to create FAudio buffer complete event");
		return;
	}
	
	InitializeCriticalSection(&pContext->m_csBufferLock);
	pModule->m_evtBlockFree = pContext->m_evtBufferComplete;
	
	// Create FAudio instance
	hr = FAudioCreate(&pContext->m_pFAudio, 0, FAUDIO_DEFAULT_PROCESSOR);
	if (FAILED(hr))
	{
		CPP_OMFA_Uninitialise(pModule);
		CP_FAIL("Failed to create FAudio instance");
		return;
	}
	
	// Create mastering voice
	hr = FAudio_CreateMasteringVoice(
		pContext->m_pFAudio,
		&pContext->m_pMasteringVoice,
		FAUDIO_DEFAULT_CHANNELS,
		FAUDIO_DEFAULT_SAMPLERATE,
		0,
		0,
		NULL
	);
	
	if (FAILED(hr))
	{
		CPP_OMFA_Uninitialise(pModule);
		CP_FAIL("Failed to create FAudio mastering voice");
		return;
	}
	
	// Set up wave format
	pContext->m_WaveFormat.wFormatTag = FAUDIO_FORMAT_PCM;
	pContext->m_WaveFormat.nChannels = pFileInfo->m_bStereo ? 2 : 1;
	pContext->m_WaveFormat.nSamplesPerSec = pFileInfo->m_iFreq_Hz;
	pContext->m_WaveFormat.wBitsPerSample = pFileInfo->m_b16bit ? 16 : 8;
	pContext->m_WaveFormat.nBlockAlign = (pContext->m_WaveFormat.nChannels * pContext->m_WaveFormat.wBitsPerSample) / 8;
	pContext->m_WaveFormat.nAvgBytesPerSec = pContext->m_WaveFormat.nSamplesPerSec * pContext->m_WaveFormat.nBlockAlign;
	pContext->m_WaveFormat.cbSize = 0;
	
	// Calculate buffer size (40ms worth of audio data)
	pContext->m_dwBufferSize = (pContext->m_WaveFormat.nAvgBytesPerSec * CPC_FAUDIO_BUFFERSIZE_MS) / 1000;
	
	// Align buffer size to block boundary
	pContext->m_dwBufferSize = (pContext->m_dwBufferSize / pContext->m_WaveFormat.nBlockAlign) * pContext->m_WaveFormat.nBlockAlign;
	
	// Create voice callback
	pVoiceCallback = (FAudioVoiceCallbackImpl*)malloc(sizeof(FAudioVoiceCallbackImpl));
	if (!pVoiceCallback)
	{
		CPP_OMFA_Uninitialise(pModule);
		CP_FAIL("Failed to allocate voice callback");
		return;
	}
	
	pVoiceCallback->callback.OnBufferEnd = CPP_OMFA_VoiceCallback;
	pVoiceCallback->callback.OnBufferStart = NULL;
	pVoiceCallback->callback.OnLoopEnd = NULL;
	pVoiceCallback->callback.OnStreamEnd = NULL;
	pVoiceCallback->callback.OnVoiceError = NULL;
	pVoiceCallback->callback.OnVoiceProcessingPassEnd = NULL;
	pVoiceCallback->callback.OnVoiceProcessingPassStart = NULL;
	pVoiceCallback->pModule = pModule;
	
	// Create source voice
	hr = FAudio_CreateSourceVoice(
		pContext->m_pFAudio,
		&pContext->m_pSourceVoice,
		&pContext->m_WaveFormat,
		0,
		FAUDIO_DEFAULT_FREQ_RATIO,
		&pVoiceCallback->callback,
		NULL,
		NULL
	);
	
	if (FAILED(hr))
	{
		free(pVoiceCallback);
		CPP_OMFA_Uninitialise(pModule);
		CP_FAIL("Failed to create FAudio source voice");
		return;
	}
	
	// Store the voice callback in the context for proper cleanup
	pContext->m_pVoiceCallback = pVoiceCallback;
	
	// Allocate audio buffers
	for (int i = 0; i < CPC_FAUDIO_NUMBEROFBUFFERS; i++)
	{
		pContext->m_pBuffers[i] = (BYTE*)malloc(pContext->m_dwBufferSize);
		if (!pContext->m_pBuffers[i])
		{
			if (pContext->m_pVoiceCallback)
			{
				free(pContext->m_pVoiceCallback);
				pContext->m_pVoiceCallback = NULL;
			}
			CPP_OMFA_Uninitialise(pModule);
			CP_FAIL("Failed to allocate FAudio buffer");
			return;
		}
		
		memset(pContext->m_pBuffers[i], 0, pContext->m_dwBufferSize);
		pContext->m_bBuffersSubmitted[i] = FALSE;
	}
	
	// Initialize equalizer
	if (hEqualiser)
		pContext->m_pEqualiser = (CPs_EqualiserModule*)hEqualiser;
	
	// Set initial state
	pContext->m_bInitialized = TRUE;
	pContext->m_bPaused = FALSE;
	pContext->m_fVolume = 1.0f;
	pContext->m_iCurrentBuffer = 0;
	
	// Start the source voice
	FAudioSourceVoice_Start(pContext->m_pSourceVoice, 0, FAUDIO_COMMIT_NOW);
}

void CPP_OMFA_Uninitialise(CPs_OutputModule* pModule)
{
	CPs_OutputContext_FAudio* pContext = (CPs_OutputContext_FAudio*)pModule->m_pModuleCookie;
	
	if (!pContext)
		return;
		
	CP_TRACE0("FAudio uninitialising");
	
	// Stop and destroy source voice
	if (pContext->m_pSourceVoice)
	{
		FAudioSourceVoice_Stop(pContext->m_pSourceVoice, 0, FAUDIO_COMMIT_NOW);
		FAudioSourceVoice_FlushSourceBuffers(pContext->m_pSourceVoice);
		FAudioVoice_DestroyVoice(pContext->m_pSourceVoice);
		pContext->m_pSourceVoice = NULL;
	}
	
	// Destroy mastering voice
	if (pContext->m_pMasteringVoice)
	{
		FAudioVoice_DestroyVoice(pContext->m_pMasteringVoice);
		pContext->m_pMasteringVoice = NULL;
	}
	
	// Destroy FAudio instance
	if (pContext->m_pFAudio)
	{
		FAudio_Release(pContext->m_pFAudio);
		pContext->m_pFAudio = NULL;
	}
	
	// Free buffers
	for (int i = 0; i < CPC_FAUDIO_NUMBEROFBUFFERS; i++)
	{
		if (pContext->m_pBuffers[i])
		{
			free(pContext->m_pBuffers[i]);
			pContext->m_pBuffers[i] = NULL;
		}
	}
	
	// Free voice callback
	if (pContext->m_pVoiceCallback)
	{
		free(pContext->m_pVoiceCallback);
		pContext->m_pVoiceCallback = NULL;
	}
	
	// Clean up synchronization objects
	if (pContext->m_evtBufferComplete)
	{
		CloseHandle(pContext->m_evtBufferComplete);
		pContext->m_evtBufferComplete = NULL;
	}
	
	DeleteCriticalSection(&pContext->m_csBufferLock);
	
	// Free context
	free(pContext);
	pModule->m_pModuleCookie = NULL;
}

void CPP_OMFA_RefillBuffers(CPs_OutputModule* pModule)
{
	CPs_OutputContext_FAudio* pContext = (CPs_OutputContext_FAudio*)pModule->m_pModuleCookie;
	FAudioBuffer buffer;
	DWORD dwBytesRead;
	BOOL bMoreData = TRUE;
	
	if (!pContext || !pContext->m_bInitialized || !pModule->m_pCoDec)
		return;
		
	EnterCriticalSection(&pContext->m_csBufferLock);
	
	// Fill available buffers
	for (int attempts = 0; attempts < CPC_FAUDIO_NUMBEROFBUFFERS && bMoreData; attempts++)
	{
		int bufferIndex = pContext->m_iCurrentBuffer;
		
		// Skip if buffer is still submitted
		if (pContext->m_bBuffersSubmitted[bufferIndex])
		{
			pContext->m_iCurrentBuffer = (pContext->m_iCurrentBuffer + 1) % CPC_FAUDIO_NUMBEROFBUFFERS;
			continue;
		}
		
		// Get PCM data from codec
		dwBytesRead = pContext->m_dwBufferSize;
		bMoreData = pModule->m_pCoDec->GetPCMBlock(pModule->m_pCoDec, pContext->m_pBuffers[bufferIndex], &dwBytesRead);
		
		if (dwBytesRead == 0)
			break;
			
		// Apply equalizer if available
		if (pContext->m_pEqualiser && dwBytesRead > 0)
		{
			pContext->m_pEqualiser->ApplyEQToBlock_Inplace(pContext->m_pEqualiser, pContext->m_pBuffers[bufferIndex], dwBytesRead);
		}
		
		// Prepare FAudio buffer
		memset(&buffer, 0, sizeof(buffer));
		buffer.AudioBytes = dwBytesRead;
		buffer.pAudioData = pContext->m_pBuffers[bufferIndex];
		buffer.pContext = (void*)(uintptr_t)bufferIndex;  // Store buffer index for callback
		
		// Submit buffer to FAudio
		HRESULT hr = FAudioSourceVoice_SubmitSourceBuffer(pContext->m_pSourceVoice, &buffer, NULL);
		if (SUCCEEDED(hr))
		{
			pContext->m_bBuffersSubmitted[bufferIndex] = TRUE;
			pContext->m_iCurrentBuffer = (pContext->m_iCurrentBuffer + 1) % CPC_FAUDIO_NUMBEROFBUFFERS;
		}
		else
		{
			CP_TRACE1("FAudio SubmitSourceBuffer failed: 0x%X", hr);
			break;
		}
	}
	
	// If no more data, mark codec as exhausted
	if (!bMoreData)
	{
		CP_TRACE0("Stream exhausted");
		pModule->m_pCoDec->CloseFile(pModule->m_pCoDec);
		pModule->m_pCoDec = NULL;
	}
	
	LeaveCriticalSection(&pContext->m_csBufferLock);
}

void CPP_OMFA_SetPause(CPs_OutputModule* pModule, const BOOL bPause)
{
	CPs_OutputContext_FAudio* pContext = (CPs_OutputContext_FAudio*)pModule->m_pModuleCookie;
	
	if (!pContext || !pContext->m_bInitialized)
		return;
		
	pContext->m_bPaused = bPause;
	
	if (bPause)
	{
		FAudioSourceVoice_Stop(pContext->m_pSourceVoice, 0, FAUDIO_COMMIT_NOW);
	}
	else
	{
		FAudioSourceVoice_Start(pContext->m_pSourceVoice, 0, FAUDIO_COMMIT_NOW);
	}
}

BOOL CPP_OMFA_IsOutputComplete(CPs_OutputModule* pModule)
{
	CPs_OutputContext_FAudio* pContext = (CPs_OutputContext_FAudio*)pModule->m_pModuleCookie;
	FAudioVoiceState state;
	
	if (!pContext || !pContext->m_bInitialized)
		return TRUE;
		
	// If we still have a codec, we're not complete
	if (pModule->m_pCoDec)
		return FALSE;
		
	// Check if any buffers are still playing
	FAudioSourceVoice_GetState(pContext->m_pSourceVoice, &state, 0);
	return (state.BuffersQueued == 0);
}

void CPP_OMFA_Flush(CPs_OutputModule* pModule)
{
	CPs_OutputContext_FAudio* pContext = (CPs_OutputContext_FAudio*)pModule->m_pModuleCookie;
	
	if (!pContext || !pContext->m_bInitialized)
		return;
		
	// Stop playback and flush all buffers
	FAudioSourceVoice_Stop(pContext->m_pSourceVoice, 0, FAUDIO_COMMIT_NOW);
	FAudioSourceVoice_FlushSourceBuffers(pContext->m_pSourceVoice);
	
	// Reset buffer states
	EnterCriticalSection(&pContext->m_csBufferLock);
	for (int i = 0; i < CPC_FAUDIO_NUMBEROFBUFFERS; i++)
	{
		pContext->m_bBuffersSubmitted[i] = FALSE;
	}
	pContext->m_iCurrentBuffer = 0;
	LeaveCriticalSection(&pContext->m_csBufferLock);
	
	// Restart playback if not paused
	if (!pContext->m_bPaused)
	{
		FAudioSourceVoice_Start(pContext->m_pSourceVoice, 0, FAUDIO_COMMIT_NOW);
	}
}

void CPP_OMFA_OnEQChanged(CPs_OutputModule* pModule)
{
	// FAudio doesn't need special handling for EQ changes
	// EQ is applied in real-time during buffer filling
}

void CPP_OMFA_SetInternalVolume(CPs_OutputModule* pModule, const int iNewVolume)
{
	CPs_OutputContext_FAudio* pContext = (CPs_OutputContext_FAudio*)pModule->m_pModuleCookie;
	
	if (!pContext || !pContext->m_bInitialized)
		return;
		
	// Convert volume from 0-100 to 0.0-1.0
	pContext->m_fVolume = (float)iNewVolume / 100.0f;
	
	// Apply volume to source voice
	FAudioVoice_SetVolume(pContext->m_pSourceVoice, pContext->m_fVolume, FAUDIO_COMMIT_NOW);
}

// FAudio voice callback - called when a buffer completes
void FAUDIOCALL CPP_OMFA_VoiceCallback(FAudioVoiceCallback* callback, void* pBufferContext)
{
	FAudioVoiceCallbackImpl* pImpl = (FAudioVoiceCallbackImpl*)callback;
	CPs_OutputModule* pModule = pImpl->pModule;
	CPs_OutputContext_FAudio* pContext = (CPs_OutputContext_FAudio*)pModule->m_pModuleCookie;
	
	if (!pContext)
		return;
		
	// Mark the completed buffer as available
	int bufferIndex = (int)(uintptr_t)pBufferContext;
	if (bufferIndex >= 0 && bufferIndex < CPC_FAUDIO_NUMBEROFBUFFERS)
	{
		EnterCriticalSection(&pContext->m_csBufferLock);
		pContext->m_bBuffersSubmitted[bufferIndex] = FALSE;
		LeaveCriticalSection(&pContext->m_csBufferLock);
		
		// Signal that a buffer is available
		SetEvent(pContext->m_evtBufferComplete);
	}
}

#else // !HAVE_FAUDIO

// Stub implementation when FAudio is not available
void CPI_Player_Output_Initialise_FAudio(CPs_OutputModule* pModule)
{
	// This is a one off call to set up the function pointers
	pModule->Initialise = NULL;
	pModule->Uninitialise = NULL;
	pModule->RefillBuffers = NULL;
	pModule->SetPause = NULL;
	pModule->IsOutputComplete = NULL;
	pModule->Flush = NULL;
	pModule->OnEQChanged = NULL;
	pModule->SetInternalVolume = NULL;
	pModule->m_pModuleCookie = NULL;
	pModule->m_pcModuleName = "FAudio Output (unavailable)";
	pModule->m_pCoDec = NULL;
	pModule->m_pEqualiser = NULL;
}

#endif // HAVE_FAUDIO
