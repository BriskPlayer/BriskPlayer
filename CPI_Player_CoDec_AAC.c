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
#include "CPI_Stream.h"
#include "CPI_Player_CoDec.h"

#include <fdk-aac/aacdecoder_lib.h>

////////////////////////////////////////////////////////////////////////////////
//
// This is the AAC CoDec module using fdk-aac
//
////////////////////////////////////////////////////////////////////////////////

typedef struct __CPs_CoDec_AAC
{
	CPs_InStream* m_pInStream;
	
	CPs_FileInfo m_FileInfo;
	
	// fdk-aac decoder handle
	HANDLE_AACDECODER decoder;
	
	// Audio format info
	CStreamInfo* stream_info;
	
	// File buffer for reading
	unsigned char* input_buffer;
	unsigned long input_buffer_size;
	unsigned long input_buffer_pos;
	unsigned long input_buffer_fill;
	
	// PCM output buffer
	INT_PCM* output_buffer;
	unsigned long output_buffer_size;
	unsigned long output_buffer_pos;
	unsigned long output_buffer_fill;
	
	// Track position
	unsigned long current_sample;
	unsigned long total_samples;
	
	// Stream info
	unsigned long sample_rate;
	unsigned char channels;
	
	BOOL eof_reached;
	BOOL decoder_initialized;
	
} CPs_CoDec_AAC;

//
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
//
// Module functions
void CPP_OMAAC_Uninitialise(CPs_CoDecModule* pModule);
BOOL CPP_OMAAC_OpenFile(CPs_CoDecModule* pModule, const char* pcFilename, DWORD dwCookie, HWND hWndOwner);
void CPP_OMAAC_CloseFile(CPs_CoDecModule* pModule);
void CPP_OMAAC_Seek(CPs_CoDecModule* pModule, const int iNumerator, const int iDenominator);
void CPP_OMAAC_GetFileInfo(CPs_CoDecModule* pModule, CPs_FileInfo* pInfo);
//
BOOL CPP_OMAAC_GetPCMBlock(CPs_CoDecModule* pModule, void* pBlock, DWORD* pdwBlockSize);
int CPP_OMAAC_GetCurrentPos_secs(CPs_CoDecModule* pModule);
//
////////////////////////////////////////////////////////////////////////////////

#define AAC_INPUT_BUFFER_SIZE    (8192)
#define AAC_OUTPUT_BUFFER_SIZE   (32768)

////////////////////////////////////////////////////////////////////////////////
//
//
void CPP_OMAAC_Uninitialise(CPs_CoDecModule* pModule)
{
	CPs_CoDec_AAC* pContext;
	
	if (!pModule)
		return;
		
	pContext = (CPs_CoDec_AAC*)pModule->m_pModuleCookie;
	
	if (pContext)
	{
		CPP_OMAAC_CloseFile(pModule);
		free(pContext);
	}
	
	CPFA_EmptyFileAssociations(pModule);
}

//
//
//
BOOL CPP_OMAAC_OpenFile(CPs_CoDecModule* pModule, const char* pcFilename, DWORD dwCookie, HWND hWndOwner)
{
	CPs_CoDec_AAC* pContext;
	AAC_DECODER_ERROR error;
	size_t bytes_read;
	UINT valid_bytes;
	UINT buffer_sizes[1];
	
	(void)dwCookie;
	(void)hWndOwner;
	
	CP_CHECKOBJECT(pModule);
	pContext = (CPs_CoDec_AAC*)pModule->m_pModuleCookie;
	CP_CHECKOBJECT(pContext);
	
	// Open input stream
	pContext->m_pInStream = CP_CreateInStream(pcFilename, hWndOwner);
	
	if (!pContext->m_pInStream)
		return FALSE;
		
	// Initialize fdk-aac decoder
	pContext->decoder = aacDecoder_Open(TT_MP4_RAW, 1);
	
	if (!pContext->decoder)
	{
		pContext->m_pInStream->Uninitialise(pContext->m_pInStream);
		pContext->m_pInStream = NULL;
		return FALSE;
	}
	
	// Configure decoder - fdk-aac outputs interleaved PCM by default
	// No specific configuration needed for PCM output format
	
	// Allocate input buffer
	pContext->input_buffer_size = AAC_INPUT_BUFFER_SIZE;
	pContext->input_buffer = (unsigned char*)malloc(pContext->input_buffer_size);
	if (!pContext->input_buffer)
	{
		aacDecoder_Close(pContext->decoder);
		pContext->m_pInStream->Uninitialise(pContext->m_pInStream);
		pContext->m_pInStream = NULL;
		return FALSE;
	}
	
	// Read initial data
	pContext->m_pInStream->Read(pContext->m_pInStream, 
								pContext->input_buffer, 
								pContext->input_buffer_size, 
								&bytes_read);
	pContext->input_buffer_fill = bytes_read;
	pContext->input_buffer_pos = 0;
	
	// Fill decoder with initial data
	valid_bytes = (UINT)pContext->input_buffer_fill;
	buffer_sizes[0] = (UINT)pContext->input_buffer_fill;
	error = aacDecoder_Fill(pContext->decoder, 
							&pContext->input_buffer, 
							buffer_sizes,
							&valid_bytes);
	
	if (error != AAC_DEC_OK)
	{
		free(pContext->input_buffer);
		aacDecoder_Close(pContext->decoder);
		pContext->m_pInStream->Uninitialise(pContext->m_pInStream);
		pContext->m_pInStream = NULL;
		return FALSE;
	}
	
	// Try to decode first frame to get stream info
	error = aacDecoder_DecodeFrame(pContext->decoder, NULL, 0, 0);
	if (error == AAC_DEC_NOT_ENOUGH_BITS)
	{
		// Need more data - this is normal for the first frame
		error = AAC_DEC_OK;
	}
	
	if (error != AAC_DEC_OK)
	{
		free(pContext->input_buffer);
		aacDecoder_Close(pContext->decoder);
		pContext->m_pInStream->Uninitialise(pContext->m_pInStream);
		pContext->m_pInStream = NULL;
		return FALSE;
	}
	
	// Get stream info
	pContext->stream_info = aacDecoder_GetStreamInfo(pContext->decoder);
	if (!pContext->stream_info || pContext->stream_info->sampleRate == 0)
	{
		free(pContext->input_buffer);
		aacDecoder_Close(pContext->decoder);
		pContext->m_pInStream->Uninitialise(pContext->m_pInStream);
		pContext->m_pInStream = NULL;
		return FALSE;
	}
	
	// Store stream info
	pContext->sample_rate = pContext->stream_info->sampleRate;
	pContext->channels = pContext->stream_info->numChannels;
	pContext->decoder_initialized = TRUE;
	
	// Allocate output buffer
	pContext->output_buffer_size = AAC_OUTPUT_BUFFER_SIZE;
	pContext->output_buffer = (INT_PCM*)malloc(pContext->output_buffer_size);
	if (!pContext->output_buffer)
	{
		free(pContext->input_buffer);
		aacDecoder_Close(pContext->decoder);
		pContext->m_pInStream->Uninitialise(pContext->m_pInStream);
		pContext->m_pInStream = NULL;
		return FALSE;
	}
	
	pContext->output_buffer_fill = 0;
	pContext->output_buffer_pos = 0;
	
	// Set up file info
	pContext->m_FileInfo.m_bStereo = (pContext->channels == 2);
	pContext->m_FileInfo.m_iFreq_Hz = pContext->sample_rate;
	pContext->m_FileInfo.m_b16bit = TRUE;
	pContext->m_FileInfo.m_iFileLength_Secs = 0; // Unknown for streams
	pContext->m_FileInfo.m_iBitRate_Kbs = 0; // Will be calculated during playback
	
	// Reset position tracking
	pContext->current_sample = 0;
	pContext->total_samples = 0; // Unknown for streams
	pContext->eof_reached = FALSE;
	
	return TRUE;
}

//
//
//
void CPP_OMAAC_CloseFile(CPs_CoDecModule* pModule)
{
	CPs_CoDec_AAC* pContext;
	
	if (!pModule)
		return;
		
	pContext = (CPs_CoDec_AAC*)pModule->m_pModuleCookie;
	
	if (!pContext)
		return;
		
	if (pContext->decoder)
	{
		aacDecoder_Close(pContext->decoder);
		pContext->decoder = NULL;
	}
	
	if (pContext->input_buffer)
	{
		free(pContext->input_buffer);
		pContext->input_buffer = NULL;
	}
	
	if (pContext->output_buffer)
	{
		free(pContext->output_buffer);
		pContext->output_buffer = NULL;
	}
	
	if (pContext->m_pInStream)
	{
		pContext->m_pInStream->Uninitialise(pContext->m_pInStream);
		pContext->m_pInStream = NULL;
	}
	
	pContext->decoder_initialized = FALSE;
	pContext->eof_reached = FALSE;
	pContext->stream_info = NULL;
}

//
//
//
void CPP_OMAAC_Seek(CPs_CoDecModule* pModule, const int iNumerator, const int iDenominator)
{
	// AAC seeking is complex and not supported for streams
	// This is a placeholder implementation
	(void)pModule;
	(void)iNumerator;
	(void)iDenominator;
}

//
//
//
void CPP_OMAAC_GetFileInfo(CPs_CoDecModule* pModule, CPs_FileInfo* pInfo)
{
	CPs_CoDec_AAC* pContext;
	
	CP_CHECKOBJECT(pModule);
	pContext = (CPs_CoDec_AAC*)pModule->m_pModuleCookie;
	CP_CHECKOBJECT(pContext);
	CP_CHECKOBJECT(pInfo);
	
	*pInfo = pContext->m_FileInfo;
}

//
//
//
BOOL CPP_OMAAC_GetPCMBlock(CPs_CoDecModule* pModule, void* pBlock, DWORD* pdwBlockSize)
{
	CPs_CoDec_AAC* pContext;
	DWORD dwBytesRequired;
	unsigned char* pOutputPtr;
	DWORD dwBytesAvailable;
	AAC_DECODER_ERROR error;
	size_t bytes_read;
	DWORD remaining_input;
	UINT valid_bytes;
	unsigned char* buffer_ptr;
	UINT buffer_sizes[1];
	
	CP_CHECKOBJECT(pModule);
	pContext = (CPs_CoDec_AAC*)pModule->m_pModuleCookie;
	CP_CHECKOBJECT(pContext);
	
	if (!pBlock || !pdwBlockSize)
		return FALSE;
	
	if (pContext->eof_reached)
	{
		*pdwBlockSize = 0;
		return FALSE;
	}
	
	dwBytesRequired = *pdwBlockSize;
	pOutputPtr = (unsigned char*)pBlock;
	dwBytesAvailable = 0;
	
	// Fill output buffer from our internal buffer and decode more as needed
	while (dwBytesAvailable < dwBytesRequired && !pContext->eof_reached)
	{
		// If we have data in our output buffer, copy it
		if (pContext->output_buffer_fill > pContext->output_buffer_pos)
		{
			DWORD dwBytesToCopy = pContext->output_buffer_fill - pContext->output_buffer_pos;
			DWORD dwBytesNeeded = dwBytesRequired - dwBytesAvailable;
			
			if (dwBytesToCopy > dwBytesNeeded)
				dwBytesToCopy = dwBytesNeeded;
				
			memcpy(pOutputPtr + dwBytesAvailable, 
				   (unsigned char*)pContext->output_buffer + pContext->output_buffer_pos,
				   dwBytesToCopy);
				   
			pContext->output_buffer_pos += dwBytesToCopy;
			dwBytesAvailable += dwBytesToCopy;
			
			// If we've consumed all output buffer data, reset for next decode
			if (pContext->output_buffer_pos >= pContext->output_buffer_fill)
			{
				pContext->output_buffer_pos = 0;
				pContext->output_buffer_fill = 0;
			}
		}
		else
		{
			// Need to decode more data
			
			// Ensure we have enough input data
			remaining_input = pContext->input_buffer_fill - pContext->input_buffer_pos;
			if (remaining_input < 1024) // Need more input data
			{
				// Move remaining data to start of buffer
				if (remaining_input > 0)
				{
					memmove(pContext->input_buffer, 
							pContext->input_buffer + pContext->input_buffer_pos,
							remaining_input);
				}
				
				pContext->input_buffer_pos = 0;
				pContext->input_buffer_fill = remaining_input;
				
				// Read more data
				pContext->m_pInStream->Read(pContext->m_pInStream,
											pContext->input_buffer + remaining_input,
											pContext->input_buffer_size - remaining_input,
											&bytes_read);
				
				if (bytes_read == 0)
				{
					pContext->eof_reached = TRUE;
					break;
				}
				
				pContext->input_buffer_fill += bytes_read;
			}
			
			// Fill decoder with data
			valid_bytes = (UINT)(pContext->input_buffer_fill - pContext->input_buffer_pos);
			buffer_ptr = pContext->input_buffer + pContext->input_buffer_pos;
			buffer_sizes[0] = valid_bytes;
			
			error = aacDecoder_Fill(pContext->decoder, &buffer_ptr, buffer_sizes, &valid_bytes);
			if (error != AAC_DEC_OK)
			{
				// Fill error - try to skip this data
				pContext->input_buffer_pos += 1;
				continue;
			}
			
			// Update input buffer position based on consumed data
			pContext->input_buffer_pos += (buffer_sizes[0] - valid_bytes);
			
			// Decode one frame
			error = aacDecoder_DecodeFrame(pContext->decoder, 
										   pContext->output_buffer,
										   pContext->output_buffer_size / sizeof(INT_PCM),
										   0);
			
			if (error == AAC_DEC_NOT_ENOUGH_BITS)
			{
				// Need more input data - continue reading
				continue;
			}
			else if (error != AAC_DEC_OK)
			{
				// Decode error - try to skip this data
				pContext->input_buffer_pos += 1;
				continue;
			}
			
			// Get updated stream info
			pContext->stream_info = aacDecoder_GetStreamInfo(pContext->decoder);
			if (pContext->stream_info && pContext->stream_info->frameSize > 0)
			{
				// Successfully decoded a frame
				DWORD frame_bytes = pContext->stream_info->frameSize * pContext->stream_info->numChannels * sizeof(INT_PCM);
				
				// Store frame size in our output buffer tracking
				pContext->output_buffer_fill = frame_bytes;
				pContext->output_buffer_pos = 0;
				
				// Update sample position
				pContext->current_sample += pContext->stream_info->frameSize;
			}
			else
			{
				// No valid frame, advance input position
				pContext->input_buffer_pos += 1;
			}
		}
	}
	
	*pdwBlockSize = dwBytesAvailable;
	return (dwBytesAvailable > 0);
}

//
//
//
int CPP_OMAAC_GetCurrentPos_secs(CPs_CoDecModule* pModule)
{
	CPs_CoDec_AAC* pContext;
	
	CP_CHECKOBJECT(pModule);
	pContext = (CPs_CoDec_AAC*)pModule->m_pModuleCookie;
	CP_CHECKOBJECT(pContext);
	
	if (pContext->sample_rate == 0)
		return 0;
		
	return (int)(pContext->current_sample / pContext->sample_rate);
}

//
//
//
void CP_InitialiseCodec_AAC(CPs_CoDecModule* pCoDec)
{
	CPs_CoDec_AAC* pContext;
	
	CP_CHECKOBJECT(pCoDec);
	
	// Hook module functions
	pCoDec->Uninitialise = CPP_OMAAC_Uninitialise;
	pCoDec->OpenFile = CPP_OMAAC_OpenFile;
	pCoDec->CloseFile = CPP_OMAAC_CloseFile;
	pCoDec->Seek = CPP_OMAAC_Seek;
	pCoDec->GetFileInfo = CPP_OMAAC_GetFileInfo;
	pCoDec->GetPCMBlock = CPP_OMAAC_GetPCMBlock;
	pCoDec->GetCurrentPos_secs = CPP_OMAAC_GetCurrentPos_secs;
	
	// Setup private data
	pContext = (CPs_CoDec_AAC*)malloc(sizeof(CPs_CoDec_AAC));
	memset(pContext, 0, sizeof(CPs_CoDec_AAC));
	pCoDec->m_pModuleCookie = pContext;
	
	// Setup file associations
	CPFA_InitialiseFileAssociations(pCoDec);
	CPFA_AddFileAssociation(pCoDec, "aac", 0L);
	CPFA_AddFileAssociation(pCoDec, "m4a", 0L);
	CPFA_AddFileAssociation(pCoDec, "mp4", 0L);
}