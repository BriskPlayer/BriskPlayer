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
#include "CPI_ID3.h"

#define FLAC__NO_DLL
#include "FLAC/stream_decoder.h"

////////////////////////////////////////////////////////////////////////////////
//
// This is the CoDec module - the basic idea is that the file will be opened and
// data will be sucked through the CoDec via calls to CPI_CoDec__GetPCMBlock
//
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//

typedef struct __CPs_CoDec_FLAC
{
	CPs_InStream* m_pInStream;
	
	CPs_FileInfo m_FileInfo;
	
	FLAC__StreamDecoder *decoder;
	
	// PCM buffer for decoded samples
	FLAC__int32 *pcm_buffer;
	unsigned int pcm_buffer_size;
	unsigned int pcm_buffer_pos;
	unsigned int pcm_buffer_length;
	
	// Track position
	FLAC__uint64 total_samples;
	FLAC__uint64 current_sample;
	
	// File format info
	unsigned int sample_rate;
	unsigned int channels;
	unsigned int bits_per_sample;
	
	BOOL eof_reached;
} CPs_CoDec_FLAC;

//
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
//
// Module functions
void CPP_OMFLAC_Uninitialise(CPs_CoDecModule* pModule);
BOOL CPP_OMFLAC_OpenFile(CPs_CoDecModule* pModule, const char* pcFilename, DWORD dwCookie, HWND hWndOwner);
void CPP_OMFLAC_CloseFile(CPs_CoDecModule* pModule);
void CPP_OMFLAC_Seek(CPs_CoDecModule* pModule, const int iNumerator, const int iDenominator);
void CPP_OMFLAC_GetFileInfo(CPs_CoDecModule* pModule, CPs_FileInfo* pInfo);
//
BOOL CPP_OMFLAC_GetPCMBlock(CPs_CoDecModule* pModule, void* pBlock, DWORD* pdwBlockSize);
int CPP_OMFLAC_GetCurrentPos_secs(CPs_CoDecModule* pModule);
//
////////////////////////////////////////////////////////////////////////////////

// Global stream pointer for callbacks (similar to OGG implementation)
CPs_InStream* g_FLACInStream = NULL;

////////////////////////////////////////////////////////////////////////////////
// FLAC decoder callbacks

FLAC__StreamDecoderReadStatus flac_read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
	CPs_CoDec_FLAC *pContext = (CPs_CoDec_FLAC*)client_data;
	size_t bytes_read = 0;
	
	(void)decoder; // Suppress unused parameter warning
	
	if(!pContext || !pContext->m_pInStream || *bytes == 0)
	{
		*bytes = 0;
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}
	
	pContext->m_pInStream->Read(pContext->m_pInStream, buffer, *bytes, &bytes_read);
	*bytes = bytes_read;
	
	if(bytes_read == 0)
	{
		pContext->eof_reached = TRUE;
		return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
	}
	
	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderSeekStatus flac_seek_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	CPs_CoDec_FLAC *pContext = (CPs_CoDec_FLAC*)client_data;
	
	(void)decoder; // Suppress unused parameter warning
	
	if(!pContext->m_pInStream || !pContext->m_pInStream->IsSeekable(pContext->m_pInStream))
		return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;
	
	pContext->m_pInStream->Seek(pContext->m_pInStream, (UINT)absolute_byte_offset);
	
	return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

FLAC__StreamDecoderTellStatus flac_tell_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	CPs_CoDec_FLAC *pContext = (CPs_CoDec_FLAC*)client_data;
	
	(void)decoder; // Suppress unused parameter warning
	
	if(!pContext->m_pInStream)
		return FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED;
	
	*absolute_byte_offset = pContext->m_pInStream->Tell(pContext->m_pInStream);
	
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__StreamDecoderLengthStatus flac_length_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
{
	CPs_CoDec_FLAC *pContext = (CPs_CoDec_FLAC*)client_data;
	
	(void)decoder; // Suppress unused parameter warning
	
	if(!pContext->m_pInStream)
		return FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED;
	
	*stream_length = pContext->m_pInStream->GetLength(pContext->m_pInStream);
	
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

FLAC__bool flac_eof_callback(const FLAC__StreamDecoder *decoder, void *client_data)
{
	CPs_CoDec_FLAC *pContext = (CPs_CoDec_FLAC*)client_data;
	
	(void)decoder; // Suppress unused parameter warning
	
	return pContext->eof_reached;
}

FLAC__StreamDecoderWriteStatus flac_write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	CPs_CoDec_FLAC *pContext = (CPs_CoDec_FLAC*)client_data;
	unsigned int i, j;
	unsigned int channels = frame->header.channels;
	unsigned int samples = frame->header.blocksize;
	
	(void)decoder; // Suppress unused parameter warning
	
	// Ensure we have enough buffer space
	unsigned int total_samples_needed = samples * channels;
	if(pContext->pcm_buffer_size < total_samples_needed)
	{
		pContext->pcm_buffer = realloc(pContext->pcm_buffer, total_samples_needed * sizeof(FLAC__int32));
		if(!pContext->pcm_buffer)
		{
			pContext->pcm_buffer_size = 0;
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
		pContext->pcm_buffer_size = total_samples_needed;
	}
	
	// Interleave the samples (left, right, left, right, ...)
	for(i = 0; i < samples; i++)
	{
		for(j = 0; j < channels; j++)
		{
			pContext->pcm_buffer[i * channels + j] = buffer[j][i];
		}
	}
	
	pContext->pcm_buffer_length = total_samples_needed;
	pContext->pcm_buffer_pos = 0;
	pContext->current_sample += samples;
	
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void flac_metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	CPs_CoDec_FLAC *pContext = (CPs_CoDec_FLAC*)client_data;
	
	(void)decoder; // Suppress unused parameter warning
	
	if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
	{
		pContext->total_samples = metadata->data.stream_info.total_samples;
		pContext->sample_rate = metadata->data.stream_info.sample_rate;
		pContext->channels = metadata->data.stream_info.channels;
		pContext->bits_per_sample = metadata->data.stream_info.bits_per_sample;
		
		CP_TRACE3("FLAC stream info: %d Hz, %d channels, %d bits", 
			pContext->sample_rate, pContext->channels, pContext->bits_per_sample);
		
		// Fill in file info
		pContext->m_FileInfo.m_iFreq_Hz = pContext->sample_rate;
		pContext->m_FileInfo.m_bStereo = (pContext->channels == 2);
		pContext->m_FileInfo.m_b16bit = TRUE; // We always output 16-bit PCM
		
		if(pContext->total_samples > 0 && pContext->sample_rate > 0)
		{
			pContext->m_FileInfo.m_iFileLength_Secs = (int)(pContext->total_samples / pContext->sample_rate);
		}
		else
		{
			pContext->m_FileInfo.m_iFileLength_Secs = 0;
		}
		
		// Estimate bitrate (this is approximate for FLAC since it's variable bitrate)
		if(pContext->m_pInStream && pContext->m_FileInfo.m_iFileLength_Secs > 0)
		{
			UINT file_size = pContext->m_pInStream->GetLength(pContext->m_pInStream);
			pContext->m_FileInfo.m_iBitRate_Kbs = (file_size * 8) / (pContext->m_FileInfo.m_iFileLength_Secs * 1000);
		}
		else
		{
			pContext->m_FileInfo.m_iBitRate_Kbs = 0;
		}
	}
}

void flac_error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	(void)decoder; // Suppress unused parameter warning
	(void)status; // Suppress unused parameter warning
	(void)client_data; // Suppress unused parameter warning
	
	CP_TRACE0("FLAC decoder error occurred");
}

////////////////////////////////////////////////////////////////////////////////
//
//
//
void CP_InitialiseCodec_FLAC(CPs_CoDecModule* pCoDec)
{
	CPs_CoDec_FLAC *pContext;
	
	// Setup functions
	pCoDec->Uninitialise = CPP_OMFLAC_Uninitialise;
	pCoDec->OpenFile = CPP_OMFLAC_OpenFile;
	pCoDec->CloseFile = CPP_OMFLAC_CloseFile;
	pCoDec->Seek = CPP_OMFLAC_Seek;
	pCoDec->GetFileInfo = CPP_OMFLAC_GetFileInfo;
	
	pCoDec->GetPCMBlock = CPP_OMFLAC_GetPCMBlock;
	pCoDec->GetCurrentPos_secs = CPP_OMFLAC_GetCurrentPos_secs;
	
	// Setup private data
	pCoDec->m_pModuleCookie = malloc(sizeof(CPs_CoDec_FLAC));
	pContext = (CPs_CoDec_FLAC*)pCoDec->m_pModuleCookie;
	pContext->m_pInStream = NULL;
	pContext->decoder = NULL;
	pContext->pcm_buffer = NULL;
	pContext->pcm_buffer_size = 0;
	pContext->pcm_buffer_pos = 0;
	pContext->pcm_buffer_length = 0;
	pContext->total_samples = 0;
	pContext->current_sample = 0;
	pContext->eof_reached = FALSE;
	
	CPFA_InitialiseFileAssociations(pCoDec);
	CPFA_AddFileAssociation(pCoDec, "FLAC", 0L);
	CPFA_AddFileAssociation(pCoDec, "FLA", 0L);
}

//
//
//
void CPP_OMFLAC_Uninitialise(CPs_CoDecModule* pModule)
{
	CPs_CoDec_FLAC *pContext = (CPs_CoDec_FLAC*)pModule->m_pModuleCookie;
	CP_CHECKOBJECT(pContext);

	CPP_OMFLAC_CloseFile(pModule);
	
	if(pContext->pcm_buffer)
	{
		free(pContext->pcm_buffer);
		pContext->pcm_buffer = NULL;
	}
	
	free(pContext);
	CPFA_EmptyFileAssociations(pModule);
}

//
//
//
BOOL CPP_OMFLAC_OpenFile(CPs_CoDecModule* pModule, const char* pcFilename, DWORD dwCookie, HWND hWndOwner)
{
	CPs_CoDec_FLAC *pContext = (CPs_CoDec_FLAC*)pModule->m_pModuleCookie;
	FLAC__StreamDecoderInitStatus init_status;
	
	(void)dwCookie; // Suppress unused parameter warning
	(void)hWndOwner; // Suppress unused parameter warning
	
	CP_CHECKOBJECT(pContext);
	
	// If we have a stream open - close it
	if (pContext->m_pInStream != NULL)
	{
		CP_TRACE0("Already had a stream open, closing it");
		CPP_OMFLAC_CloseFile(pModule);
	}
	
	// Open our new stream
	CP_TRACE1("Opening FLAC file \"%s\"", pcFilename);
	
	pContext->m_pInStream = CP_CreateInStream(pcFilename, hWndOwner);
	
	if (!pContext->m_pInStream)
	{
		CP_TRACE0("Failed to open FLAC file stream");
		return FALSE;
	}
	
	g_FLACInStream = pContext->m_pInStream;
	
	// Create FLAC decoder
	pContext->decoder = FLAC__stream_decoder_new();
	if(!pContext->decoder)
	{
		CP_TRACE0("Failed to create FLAC decoder");
		pContext->m_pInStream->Uninitialise(pContext->m_pInStream);
		pContext->m_pInStream = NULL;
		g_FLACInStream = NULL;
		return FALSE;
	}
	
	// Set up decoder callbacks
	init_status = FLAC__stream_decoder_init_stream(
		pContext->decoder,
		flac_read_callback,
		flac_seek_callback,
		flac_tell_callback,
		flac_length_callback,
		flac_eof_callback,
		flac_write_callback,
		flac_metadata_callback,
		flac_error_callback,
		pContext
	);
	
	if(init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK)
	{
		CP_TRACE1("Failed to initialize FLAC decoder: %s", FLAC__StreamDecoderInitStatusString[init_status]);
		FLAC__stream_decoder_delete(pContext->decoder);
		pContext->decoder = NULL;
		pContext->m_pInStream->Uninitialise(pContext->m_pInStream);
		pContext->m_pInStream = NULL;
		g_FLACInStream = NULL;
		return FALSE;
	}
	
	// Process metadata
	if(!FLAC__stream_decoder_process_until_end_of_metadata(pContext->decoder))
	{
		FLAC__StreamDecoderState state = FLAC__stream_decoder_get_state(pContext->decoder);
		(void)state; // Suppress unused variable warning
		CP_TRACE0("Failed to process FLAC metadata");
		CPP_OMFLAC_CloseFile(pModule);
		return FALSE;
	}
	
	// Verify we have valid stream info
	if(pContext->channels == 0 || pContext->sample_rate == 0)
	{
		CP_TRACE2("Invalid FLAC stream info: channels=%d, sample_rate=%d", pContext->channels, pContext->sample_rate);
		CPP_OMFLAC_CloseFile(pModule);
		return FALSE;
	}
	
	// Check channel count (we support mono and stereo only)
	if(pContext->channels > 2)
	{
		CP_TRACE1("FLAC file has too many channels: %d", pContext->channels);
		CPP_OMFLAC_CloseFile(pModule);
		return FALSE;
	}
	
	// Reset position
	pContext->current_sample = 0;
	pContext->pcm_buffer_pos = 0;
	pContext->pcm_buffer_length = 0;
	pContext->eof_reached = FALSE;
	
	CP_TRACE3("FLAC file opened successfully: %d Hz, %d channels, %d bits", 
		pContext->sample_rate, pContext->channels, pContext->bits_per_sample);
	
	return TRUE;
}

//
//
//
void CPP_OMFLAC_CloseFile(CPs_CoDecModule* pModule)
{
	CPs_CoDec_FLAC *pContext = (CPs_CoDec_FLAC*)pModule->m_pModuleCookie;
	CP_CHECKOBJECT(pContext);
	
	CP_TRACE0("Closing FLAC file");
	
	// Clean up decoder
	if(pContext->decoder)
	{
		FLAC__stream_decoder_finish(pContext->decoder);
		FLAC__stream_decoder_delete(pContext->decoder);
		pContext->decoder = NULL;
	}
	
	// Clean up stream
	if (pContext->m_pInStream != NULL)
	{
		pContext->m_pInStream->Uninitialise(pContext->m_pInStream);
		pContext->m_pInStream = NULL;
	}
	
	// Reset state
	pContext->pcm_buffer_pos = 0;
	pContext->pcm_buffer_length = 0;
	pContext->current_sample = 0;
	pContext->eof_reached = FALSE;
	
	g_FLACInStream = NULL;
}

//
//
//
void CPP_OMFLAC_Seek(CPs_CoDecModule* pModule, const int iNumerator, const int iDenominator)
{
	CPs_CoDec_FLAC *pContext = (CPs_CoDec_FLAC*)pModule->m_pModuleCookie;
	FLAC__uint64 target_sample;
	
	CP_CHECKOBJECT(pContext);
	
	if(!pContext->decoder || pContext->total_samples == 0)
		return;
	
	// Calculate target sample
	target_sample = (FLAC__uint64)(((double)iNumerator / (double)iDenominator) * (double)pContext->total_samples);
	
	// Seek to target sample
	if(FLAC__stream_decoder_seek_absolute(pContext->decoder, target_sample))
	{
		pContext->current_sample = target_sample;
		pContext->pcm_buffer_pos = 0;
		pContext->pcm_buffer_length = 0;
		pContext->eof_reached = FALSE;
	}
}

//
//
//
BOOL CPP_OMFLAC_GetPCMBlock(CPs_CoDecModule* pModule, void* pBlock, DWORD* pdwBlockSize)
{
	CPs_CoDec_FLAC *pContext = (CPs_CoDec_FLAC*)pModule->m_pModuleCookie;
	unsigned char* output_buffer = (unsigned char*)pBlock;
	DWORD bytes_written = 0;
	DWORD bytes_requested = *pdwBlockSize;
	
	CP_CHECKOBJECT(pContext);
	
	if(!pContext->decoder)
	{
		CP_TRACE0("FLAC: No decoder available");
		*pdwBlockSize = 0;
		return FALSE;
	}
	
	// Check decoder state before proceeding
	FLAC__StreamDecoderState decoder_state = FLAC__stream_decoder_get_state(pContext->decoder);
	if(decoder_state == FLAC__STREAM_DECODER_ABORTED || 
	   decoder_state == FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR ||
	   decoder_state == FLAC__STREAM_DECODER_UNINITIALIZED)
	{
		CP_TRACE1("FLAC decoder in error state: %s", FLAC__StreamDecoderStateString[decoder_state]);
		*pdwBlockSize = 0;
		return FALSE;
	}
	
	while(bytes_written < bytes_requested)
	{
		// If we have data in our PCM buffer, copy it out
		if(pContext->pcm_buffer_pos < pContext->pcm_buffer_length)
		{
			while(pContext->pcm_buffer_pos < pContext->pcm_buffer_length && bytes_written + 1 < bytes_requested)
			{
				FLAC__int32 sample = pContext->pcm_buffer[pContext->pcm_buffer_pos++];
				
				// Convert to 16-bit signed PCM
				short pcm_sample;
				
				if(pContext->bits_per_sample == 16)
				{
					// Already 16-bit, just cast
					pcm_sample = (short)sample;
				}
				else if(pContext->bits_per_sample < 16)
				{
					// Scale up to 16-bit
					pcm_sample = (short)(sample << (16 - pContext->bits_per_sample));
				}
				else
				{
					// Scale down to 16-bit (common for 24-bit FLAC)
					pcm_sample = (short)(sample >> (pContext->bits_per_sample - 16));
				}
				
				// Store as little-endian 16-bit signed
				output_buffer[bytes_written++] = (unsigned char)(pcm_sample & 0xFF);
				output_buffer[bytes_written++] = (unsigned char)((pcm_sample >> 8) & 0xFF);
			}
		}
		else
		{
			// Need to decode more data
			BOOL decode_result = FLAC__stream_decoder_process_single(pContext->decoder);
			
			if(!decode_result)
			{
				// Check what kind of error occurred
				decoder_state = FLAC__stream_decoder_get_state(pContext->decoder);
				if(decoder_state == FLAC__STREAM_DECODER_END_OF_STREAM)
				{
					pContext->eof_reached = TRUE;
					break;
				}
				else
				{
					CP_TRACE1("FLAC decode error, state: %s", FLAC__StreamDecoderStateString[decoder_state]);
					pContext->eof_reached = TRUE;
					break;
				}
			}
			
			// Check if we reached end of stream
			decoder_state = FLAC__stream_decoder_get_state(pContext->decoder);
			if(decoder_state == FLAC__STREAM_DECODER_END_OF_STREAM)
			{
				pContext->eof_reached = TRUE;
				break;
			}
			
			// If no new data was produced and we're at the end, we're done
			if(pContext->pcm_buffer_pos >= pContext->pcm_buffer_length && pContext->eof_reached)
			{
				break;
			}
		}
	}
	
	*pdwBlockSize = bytes_written;
	
	// Return FALSE when we've reached EOF and have no more data to return
	// This is crucial for the player to know when to advance to the next track
	if(pContext->eof_reached && bytes_written == 0)
	{
		CP_TRACE0("FLAC: End of stream reached");
		return FALSE;
	}
	
	return TRUE;
}

//
//
//
void CPP_OMFLAC_GetFileInfo(CPs_CoDecModule* pModule, CPs_FileInfo* pInfo)
{
	CPs_CoDec_FLAC *pContext = (CPs_CoDec_FLAC*)pModule->m_pModuleCookie;
	CP_CHECKOBJECT(pContext);
	
	if(!pInfo)
		return;
	
	memcpy(pInfo, &pContext->m_FileInfo, sizeof(*pInfo));
}

//
//
//
int CPP_OMFLAC_GetCurrentPos_secs(CPs_CoDecModule* pModule)
{
	CPs_CoDec_FLAC *pContext = (CPs_CoDec_FLAC*)pModule->m_pModuleCookie;
	CP_CHECKOBJECT(pContext);
	
	if(pContext->sample_rate == 0)
		return 0;
	
	return (int)(pContext->current_sample / pContext->sample_rate);
}
