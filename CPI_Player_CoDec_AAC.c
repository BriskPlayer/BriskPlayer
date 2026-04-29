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

// minimp4 for MP4/M4A container demuxing
#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"

////////////////////////////////////////////////////////////////////////////////
//
// AAC CoDec module using fdk-aac with MP4/M4A container support via minimp4
//
////////////////////////////////////////////////////////////////////////////////

typedef enum {
	AAC_MODE_ADTS,   // .aac files - ADTS framing
	AAC_MODE_MP4     // .m4a/.mp4 files - MP4 container
} AAC_StreamMode;

typedef struct __CPs_CoDec_AAC
{
	CPs_InStream* m_pInStream;
	CPs_FileInfo m_FileInfo;
	
	AAC_StreamMode stream_mode;
	
	// fdk-aac decoder handle
	HANDLE_AACDECODER decoder;
	CStreamInfo* stream_info;
	
	// ADTS mode: streaming input buffer
	unsigned char* input_buffer;
	unsigned long input_buffer_size;
	unsigned long input_buffer_pos;
	unsigned long input_buffer_fill;
	
	// MP4 mode: minimp4 demuxer
	MP4D_demux_t mp4;
	BOOL mp4_opened;
	int audio_track;
	unsigned int mp4_sample_index;
	unsigned int mp4_sample_count;
	unsigned char* sample_buffer;
	unsigned long sample_buffer_capacity;
	
	// PCM output buffer (both modes)
	INT_PCM* output_buffer;
	unsigned long output_buffer_size;
	unsigned long output_buffer_pos;
	unsigned long output_buffer_fill;
	
	// Position tracking
	unsigned long current_pcm_sample;
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
// Module function declarations
void CPP_OMAAC_Uninitialise(CPs_CoDecModule* pModule);
BOOL CPP_OMAAC_OpenFile(CPs_CoDecModule* pModule, const char* pcFilename, DWORD_PTR dwCookie, HWND hWndOwner);
void CPP_OMAAC_CloseFile(CPs_CoDecModule* pModule);
void CPP_OMAAC_Seek(CPs_CoDecModule* pModule, const int iNumerator, const int iDenominator);
void CPP_OMAAC_GetFileInfo(CPs_CoDecModule* pModule, CPs_FileInfo* pInfo);
BOOL CPP_OMAAC_GetPCMBlock(CPs_CoDecModule* pModule, void* pBlock, DWORD* pdwBlockSize);
int CPP_OMAAC_GetCurrentPos_secs(CPs_CoDecModule* pModule);
//
////////////////////////////////////////////////////////////////////////////////

#define AAC_INPUT_BUFFER_SIZE    (8192)
#define AAC_OUTPUT_BUFFER_SIZE   (32768)
#define AAC_MAX_SAMPLE_SIZE      (16384)

////////////////////////////////////////////////////////////////////////////////
// minimp4 read callback - bridges CPs_InStream to minimp4's I/O interface
//
static int mp4_read_callback(int64_t offset, void *buffer, size_t size, void *token)
{
	CPs_InStream *stream = (CPs_InStream *)token;
	size_t bytes_read = 0;
	
	if (!stream || !buffer || size == 0)
		return -1;
	
	stream->Seek(stream, (size_t)offset);
	stream->Read(stream, buffer, size, &bytes_read);
	
	return (bytes_read == size) ? 0 : -1;
}

////////////////////////////////////////////////////////////////////////////////
// Detect whether the file is an MP4 container by checking for 'ftyp' box
//
static BOOL is_mp4_container(CPs_InStream *stream)
{
	unsigned char header[8];
	size_t bytes_read = 0;
	
	stream->Read(stream, header, 8, &bytes_read);
	stream->Seek(stream, 0);
	
	if (bytes_read < 8)
		return FALSE;
	
	// Check for standard MP4 box types at offset 4-7
	// ftyp = standard MP4 file type box
	// moov, free, skip, wide, mdat = other top-level MP4 boxes
	if ((header[4] == 'f' && header[5] == 't' && header[6] == 'y' && header[7] == 'p') ||
	    (header[4] == 'm' && header[5] == 'o' && header[6] == 'o' && header[7] == 'v') ||
	    (header[4] == 'f' && header[5] == 'r' && header[6] == 'e' && header[7] == 'e') ||
	    (header[4] == 's' && header[5] == 'k' && header[6] == 'i' && header[7] == 'p') ||
	    (header[4] == 'w' && header[5] == 'i' && header[6] == 'd' && header[7] == 'e'))
		return TRUE;
	
	return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// Find the first audio track in the MP4 file that contains AAC audio
//
static int find_aac_audio_track(MP4D_demux_t *mp4)
{
	unsigned int i;
	for (i = 0; i < (unsigned int)mp4->track_count; i++)
	{
		MP4D_track_t *tr = &mp4->track[i];
		
		if (tr->handler_type != MP4D_HANDLER_TYPE_SOUN)
			continue;
		
		// Accept MPEG-4 AAC and MPEG-2 AAC profiles
		if (tr->object_type_indication == 0x40 ||  // MPEG-4 AAC
		    tr->object_type_indication == 0x66 ||  // MPEG-2 AAC Main
		    tr->object_type_indication == 0x67 ||  // MPEG-2 AAC LC
		    tr->object_type_indication == 0x68)    // MPEG-2 AAC SSR
			return (int)i;
	}
	
	return -1;
}

////////////////////////////////////////////////////////////////////////////////
// Open file in MP4 container mode
//
static BOOL CPP_OMAAC_OpenFile_MP4(CPs_CoDec_AAC* pContext, HWND hWndOwner)
{
	MP4D_track_t *tr;
	AAC_DECODER_ERROR error;
	int64_t file_size;
	unsigned int frame_bytes;
	MP4D_file_offset_t offset;
	UINT valid_bytes;
	unsigned char *buffer_ptr;
	UINT buffer_sizes[1];
	
	(void)hWndOwner;
	
	file_size = (int64_t)pContext->m_pInStream->GetLength(pContext->m_pInStream);
	
	// Parse the MP4 container
	if (!MP4D_open(&pContext->mp4, mp4_read_callback, pContext->m_pInStream, file_size))
		return FALSE;
	
	pContext->mp4_opened = TRUE;
	
	// Find an AAC audio track
	pContext->audio_track = find_aac_audio_track(&pContext->mp4);
	if (pContext->audio_track < 0)
	{
		MP4D_close(&pContext->mp4);
		pContext->mp4_opened = FALSE;
		return FALSE;
	}
	
	tr = &pContext->mp4.track[pContext->audio_track];
	pContext->mp4_sample_count = tr->sample_count;
	pContext->mp4_sample_index = 0;
	
	// Open fdk-aac in raw mode for MP4 demuxed data
	pContext->decoder = aacDecoder_Open(TT_MP4_RAW, 1);
	if (!pContext->decoder)
	{
		MP4D_close(&pContext->mp4);
		pContext->mp4_opened = FALSE;
		return FALSE;
	}
	
	// Feed the AudioSpecificConfig (DSI) to the decoder
	if (tr->dsi && tr->dsi_bytes > 0)
	{
		UCHAR *dsi_buf = tr->dsi;
		UINT dsi_size = tr->dsi_bytes;
		
		error = aacDecoder_ConfigRaw(pContext->decoder, &dsi_buf, &dsi_size);
		if (error != AAC_DEC_OK)
		{
			aacDecoder_Close(pContext->decoder);
			pContext->decoder = NULL;
			MP4D_close(&pContext->mp4);
			pContext->mp4_opened = FALSE;
			return FALSE;
		}
	}
	
	// Allocate sample read buffer
	pContext->sample_buffer_capacity = AAC_MAX_SAMPLE_SIZE;
	pContext->sample_buffer = (unsigned char*)malloc(pContext->sample_buffer_capacity);
	if (!pContext->sample_buffer)
	{
		aacDecoder_Close(pContext->decoder);
		pContext->decoder = NULL;
		MP4D_close(&pContext->mp4);
		pContext->mp4_opened = FALSE;
		return FALSE;
	}
	
	// Allocate output buffer
	pContext->output_buffer_size = AAC_OUTPUT_BUFFER_SIZE;
	pContext->output_buffer = (INT_PCM*)malloc(pContext->output_buffer_size);
	if (!pContext->output_buffer)
	{
		free(pContext->sample_buffer);
		pContext->sample_buffer = NULL;
		aacDecoder_Close(pContext->decoder);
		pContext->decoder = NULL;
		MP4D_close(&pContext->mp4);
		pContext->mp4_opened = FALSE;
		return FALSE;
	}
	
	// Decode the first sample to get stream info
	offset = MP4D_frame_offset(&pContext->mp4, pContext->audio_track, 0, &frame_bytes, NULL, NULL);
	if (frame_bytes > 0 && offset > 0)
	{
		size_t bytes_read = 0;
		
		// Grow sample buffer if needed
		if (frame_bytes > pContext->sample_buffer_capacity)
		{
			unsigned char *new_buf = (unsigned char*)realloc(pContext->sample_buffer, frame_bytes);
			if (new_buf)
			{
				pContext->sample_buffer = new_buf;
				pContext->sample_buffer_capacity = frame_bytes;
			}
		}
		
		pContext->m_pInStream->Seek(pContext->m_pInStream, (size_t)offset);
		pContext->m_pInStream->Read(pContext->m_pInStream, pContext->sample_buffer,
		                            frame_bytes < pContext->sample_buffer_capacity ? frame_bytes : pContext->sample_buffer_capacity,
		                            &bytes_read);
		
		if (bytes_read > 0)
		{
			valid_bytes = (UINT)bytes_read;
			buffer_ptr = pContext->sample_buffer;
			buffer_sizes[0] = (UINT)bytes_read;
			
			error = aacDecoder_Fill(pContext->decoder, &buffer_ptr, buffer_sizes, &valid_bytes);
			if (error == AAC_DEC_OK)
			{
				error = aacDecoder_DecodeFrame(pContext->decoder,
				                               pContext->output_buffer,
				                               pContext->output_buffer_size / sizeof(INT_PCM),
				                               0);
			}
		}
		
		pContext->mp4_sample_index = 1;
	}
	
	// Get stream info from decoder
	pContext->stream_info = aacDecoder_GetStreamInfo(pContext->decoder);
	if (!pContext->stream_info || pContext->stream_info->sampleRate == 0)
	{
		// Fall back to MP4 track metadata
		pContext->sample_rate = tr->SampleDescription.audio.samplerate_hz;
		pContext->channels = (unsigned char)tr->SampleDescription.audio.channelcount;
		if (pContext->sample_rate == 0)
			pContext->sample_rate = 44100;
		if (pContext->channels == 0)
			pContext->channels = 2;
	}
	else
	{
		pContext->sample_rate = pContext->stream_info->sampleRate;
		pContext->channels = (unsigned char)pContext->stream_info->numChannels;
	}
	
	pContext->decoder_initialized = TRUE;
	pContext->output_buffer_fill = 0;
	pContext->output_buffer_pos = 0;
	
	// Set up file info
	pContext->m_FileInfo.m_bStereo = (pContext->channels >= 2);
	pContext->m_FileInfo.m_iFreq_Hz = pContext->sample_rate;
	pContext->m_FileInfo.m_b16bit = TRUE;
	
	// Calculate duration from MP4 track metadata
	if (tr->timescale > 0)
	{
		unsigned long long duration = ((unsigned long long)tr->duration_hi << 32) | tr->duration_lo;
		pContext->m_FileInfo.m_iFileLength_Secs = (UINT)(duration / tr->timescale);
	}
	else
	{
		pContext->m_FileInfo.m_iFileLength_Secs = 0;
	}
	
	// Calculate average bitrate
	if (tr->avg_bitrate_bps > 0)
		pContext->m_FileInfo.m_iBitRate_Kbs = tr->avg_bitrate_bps / 1000;
	else
		pContext->m_FileInfo.m_iBitRate_Kbs = 0;
	
	pContext->current_pcm_sample = 0;
	pContext->eof_reached = FALSE;
	
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Open file in ADTS streaming mode
//
static BOOL CPP_OMAAC_OpenFile_ADTS(CPs_CoDec_AAC* pContext, HWND hWndOwner)
{
	AAC_DECODER_ERROR error;
	size_t bytes_read;
	UINT valid_bytes;
	UINT buffer_sizes[1];
	
	(void)hWndOwner;
	
	// Open fdk-aac in ADTS mode
	pContext->decoder = aacDecoder_Open(TT_MP4_ADTS, 1);
	if (!pContext->decoder)
		return FALSE;
	
	// Allocate input buffer
	pContext->input_buffer_size = AAC_INPUT_BUFFER_SIZE;
	pContext->input_buffer = (unsigned char*)malloc(pContext->input_buffer_size);
	if (!pContext->input_buffer)
	{
		aacDecoder_Close(pContext->decoder);
		pContext->decoder = NULL;
		return FALSE;
	}
	
	// Read initial data
	pContext->m_pInStream->Read(pContext->m_pInStream,
	                            pContext->input_buffer,
	                            pContext->input_buffer_size,
	                            &bytes_read);
	pContext->input_buffer_fill = (unsigned long)bytes_read;
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
		pContext->input_buffer = NULL;
		aacDecoder_Close(pContext->decoder);
		pContext->decoder = NULL;
		return FALSE;
	}
	
	// Allocate output buffer
	pContext->output_buffer_size = AAC_OUTPUT_BUFFER_SIZE;
	pContext->output_buffer = (INT_PCM*)malloc(pContext->output_buffer_size);
	if (!pContext->output_buffer)
	{
		free(pContext->input_buffer);
		pContext->input_buffer = NULL;
		aacDecoder_Close(pContext->decoder);
		pContext->decoder = NULL;
		return FALSE;
	}
	
	// Decode first frame to get stream info
	error = aacDecoder_DecodeFrame(pContext->decoder,
	                               pContext->output_buffer,
	                               pContext->output_buffer_size / sizeof(INT_PCM),
	                               0);
	if (error == AAC_DEC_NOT_ENOUGH_BITS)
		error = AAC_DEC_OK;
	
	if (error != AAC_DEC_OK)
	{
		free(pContext->output_buffer);
		pContext->output_buffer = NULL;
		free(pContext->input_buffer);
		pContext->input_buffer = NULL;
		aacDecoder_Close(pContext->decoder);
		pContext->decoder = NULL;
		return FALSE;
	}
	
	// Get stream info
	pContext->stream_info = aacDecoder_GetStreamInfo(pContext->decoder);
	if (!pContext->stream_info || pContext->stream_info->sampleRate == 0)
	{
		free(pContext->output_buffer);
		pContext->output_buffer = NULL;
		free(pContext->input_buffer);
		pContext->input_buffer = NULL;
		aacDecoder_Close(pContext->decoder);
		pContext->decoder = NULL;
		return FALSE;
	}
	
	pContext->sample_rate = pContext->stream_info->sampleRate;
	pContext->channels = (unsigned char)pContext->stream_info->numChannels;
	pContext->decoder_initialized = TRUE;
	pContext->output_buffer_fill = 0;
	pContext->output_buffer_pos = 0;
	
	// Set up file info
	pContext->m_FileInfo.m_bStereo = (pContext->channels >= 2);
	pContext->m_FileInfo.m_iFreq_Hz = pContext->sample_rate;
	pContext->m_FileInfo.m_b16bit = TRUE;
	pContext->m_FileInfo.m_iFileLength_Secs = 0;
	pContext->m_FileInfo.m_iBitRate_Kbs = 0;
	
	pContext->current_pcm_sample = 0;
	pContext->eof_reached = FALSE;
	
	return TRUE;
}

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
BOOL CPP_OMAAC_OpenFile(CPs_CoDecModule* pModule, const char* pcFilename, DWORD_PTR dwCookie, HWND hWndOwner)
{
	CPs_CoDec_AAC* pContext;
	BOOL result;
	
	(void)dwCookie;
	
	CP_CHECKOBJECT(pModule);
	pContext = (CPs_CoDec_AAC*)pModule->m_pModuleCookie;
	CP_CHECKOBJECT(pContext);
	
	// Open input stream
	pContext->m_pInStream = CP_CreateInStream(pcFilename, hWndOwner);
	if (!pContext->m_pInStream)
		return FALSE;
	
	// Detect file format and open accordingly
	if (is_mp4_container(pContext->m_pInStream))
	{
		pContext->stream_mode = AAC_MODE_MP4;
		result = CPP_OMAAC_OpenFile_MP4(pContext, hWndOwner);
	}
	else
	{
		pContext->stream_mode = AAC_MODE_ADTS;
		result = CPP_OMAAC_OpenFile_ADTS(pContext, hWndOwner);
	}
	
	if (!result)
	{
		pContext->m_pInStream->Uninitialise(pContext->m_pInStream);
		pContext->m_pInStream = NULL;
	}
	
	return result;
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
	
	if (pContext->mp4_opened)
	{
		MP4D_close(&pContext->mp4);
		pContext->mp4_opened = FALSE;
	}
	
	if (pContext->input_buffer)
	{
		free(pContext->input_buffer);
		pContext->input_buffer = NULL;
	}
	
	if (pContext->sample_buffer)
	{
		free(pContext->sample_buffer);
		pContext->sample_buffer = NULL;
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
	pContext->sample_buffer_capacity = 0;
}

//
//
//
void CPP_OMAAC_Seek(CPs_CoDecModule* pModule, const int iNumerator, const int iDenominator)
{
	CPs_CoDec_AAC* pContext;
	
	if (!pModule || iDenominator == 0)
		return;
	
	pContext = (CPs_CoDec_AAC*)pModule->m_pModuleCookie;
	if (!pContext)
		return;
	
	if (pContext->stream_mode == AAC_MODE_MP4 && pContext->mp4_sample_count > 0)
	{
		// Calculate target sample index from seek ratio
		double ratio = (double)iNumerator / (double)iDenominator;
		unsigned int target = (unsigned int)(ratio * pContext->mp4_sample_count);
		
		if (target >= pContext->mp4_sample_count)
			target = pContext->mp4_sample_count - 1;
		
		pContext->mp4_sample_index = target;
		pContext->output_buffer_fill = 0;
		pContext->output_buffer_pos = 0;
		pContext->eof_reached = FALSE;
		
		// Estimate PCM position
		if (pContext->m_FileInfo.m_iFileLength_Secs > 0 && pContext->sample_rate > 0)
			pContext->current_pcm_sample = (unsigned long)(ratio * pContext->m_FileInfo.m_iFileLength_Secs * pContext->sample_rate);
	}
	// ADTS seek not supported (no index)
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

////////////////////////////////////////////////////////////////////////////////
// GetPCMBlock for MP4 mode - read individual demuxed samples
//
static BOOL CPP_OMAAC_GetPCMBlock_MP4(CPs_CoDec_AAC* pContext, void* pBlock, DWORD* pdwBlockSize)
{
	DWORD dwBytesRequired = *pdwBlockSize;
	unsigned char* pOutputPtr = (unsigned char*)pBlock;
	DWORD dwBytesAvailable = 0;
	
	while (dwBytesAvailable < dwBytesRequired && !pContext->eof_reached)
	{
		// Copy from output buffer first
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
			
			if (pContext->output_buffer_pos >= pContext->output_buffer_fill)
			{
				pContext->output_buffer_pos = 0;
				pContext->output_buffer_fill = 0;
			}
		}
		else
		{
			// Read and decode next MP4 sample
			unsigned int frame_bytes = 0;
			MP4D_file_offset_t offset;
			size_t bytes_read = 0;
			UINT valid_bytes;
			unsigned char *buffer_ptr;
			UINT buffer_sizes[1];
			AAC_DECODER_ERROR error;
			
			if (pContext->mp4_sample_index >= pContext->mp4_sample_count)
			{
				pContext->eof_reached = TRUE;
				break;
			}
			
			offset = MP4D_frame_offset(&pContext->mp4, pContext->audio_track,
			                            pContext->mp4_sample_index, &frame_bytes, NULL, NULL);
			
			if (frame_bytes == 0 || offset == 0)
			{
				pContext->mp4_sample_index++;
				continue;
			}
			
			// Grow sample buffer if needed
			if (frame_bytes > pContext->sample_buffer_capacity)
			{
				unsigned char *new_buf = (unsigned char*)realloc(pContext->sample_buffer, frame_bytes);
				if (!new_buf)
				{
					pContext->eof_reached = TRUE;
					break;
				}
				pContext->sample_buffer = new_buf;
				pContext->sample_buffer_capacity = frame_bytes;
			}
			
			// Read sample data from file
			pContext->m_pInStream->Seek(pContext->m_pInStream, (size_t)offset);
			pContext->m_pInStream->Read(pContext->m_pInStream, pContext->sample_buffer,
			                            frame_bytes, &bytes_read);
			
			if (bytes_read == 0)
			{
				pContext->eof_reached = TRUE;
				break;
			}
			
			// Feed sample to fdk-aac
			valid_bytes = (UINT)bytes_read;
			buffer_ptr = pContext->sample_buffer;
			buffer_sizes[0] = (UINT)bytes_read;
			
			error = aacDecoder_Fill(pContext->decoder, &buffer_ptr, buffer_sizes, &valid_bytes);
			if (error != AAC_DEC_OK)
			{
				pContext->mp4_sample_index++;
				continue;
			}
			
			// Decode
			error = aacDecoder_DecodeFrame(pContext->decoder,
			                               pContext->output_buffer,
			                               pContext->output_buffer_size / sizeof(INT_PCM),
			                               0);
			
			pContext->mp4_sample_index++;
			
			if (error == AAC_DEC_NOT_ENOUGH_BITS)
				continue;
			
			if (error != AAC_DEC_OK)
				continue;
			
			// Get decoded frame info
			pContext->stream_info = aacDecoder_GetStreamInfo(pContext->decoder);
			if (pContext->stream_info && pContext->stream_info->frameSize > 0)
			{
				DWORD frame_pcm_bytes = pContext->stream_info->frameSize
				                        * pContext->stream_info->numChannels
				                        * sizeof(INT_PCM);
				
				pContext->output_buffer_fill = frame_pcm_bytes;
				pContext->output_buffer_pos = 0;
				pContext->current_pcm_sample += pContext->stream_info->frameSize;
			}
		}
	}
	
	*pdwBlockSize = dwBytesAvailable;
	return (dwBytesAvailable > 0);
}

////////////////////////////////////////////////////////////////////////////////
// GetPCMBlock for ADTS mode - streaming decode
//
static BOOL CPP_OMAAC_GetPCMBlock_ADTS(CPs_CoDec_AAC* pContext, void* pBlock, DWORD* pdwBlockSize)
{
	DWORD dwBytesRequired = *pdwBlockSize;
	unsigned char* pOutputPtr = (unsigned char*)pBlock;
	DWORD dwBytesAvailable = 0;
	
	while (dwBytesAvailable < dwBytesRequired && !pContext->eof_reached)
	{
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
			
			if (pContext->output_buffer_pos >= pContext->output_buffer_fill)
			{
				pContext->output_buffer_pos = 0;
				pContext->output_buffer_fill = 0;
			}
		}
		else
		{
			size_t bytes_read;
			DWORD remaining_input;
			UINT valid_bytes;
			unsigned char* buffer_ptr;
			UINT buffer_sizes[1];
			AAC_DECODER_ERROR error;
			
			// Ensure we have enough input data
			remaining_input = pContext->input_buffer_fill - pContext->input_buffer_pos;
			if (remaining_input < 1024)
			{
				if (remaining_input > 0)
				{
					memmove(pContext->input_buffer,
					        pContext->input_buffer + pContext->input_buffer_pos,
					        remaining_input);
				}
				
				pContext->input_buffer_pos = 0;
				pContext->input_buffer_fill = remaining_input;
				
				pContext->m_pInStream->Read(pContext->m_pInStream,
				                            pContext->input_buffer + remaining_input,
				                            pContext->input_buffer_size - remaining_input,
				                            &bytes_read);
				
				if (bytes_read == 0)
				{
					pContext->eof_reached = TRUE;
					break;
				}
				
				pContext->input_buffer_fill += (unsigned long)bytes_read;
			}
			
			// Fill decoder with data
			valid_bytes = (UINT)(pContext->input_buffer_fill - pContext->input_buffer_pos);
			buffer_ptr = pContext->input_buffer + pContext->input_buffer_pos;
			buffer_sizes[0] = valid_bytes;
			
			error = aacDecoder_Fill(pContext->decoder, &buffer_ptr, buffer_sizes, &valid_bytes);
			if (error != AAC_DEC_OK)
			{
				pContext->input_buffer_pos += 1;
				continue;
			}
			
			pContext->input_buffer_pos += (buffer_sizes[0] - valid_bytes);
			
			// Decode one frame
			error = aacDecoder_DecodeFrame(pContext->decoder,
			                               pContext->output_buffer,
			                               pContext->output_buffer_size / sizeof(INT_PCM),
			                               0);
			
			if (error == AAC_DEC_NOT_ENOUGH_BITS)
				continue;
			
			if (error != AAC_DEC_OK)
			{
				pContext->input_buffer_pos += 1;
				continue;
			}
			
			pContext->stream_info = aacDecoder_GetStreamInfo(pContext->decoder);
			if (pContext->stream_info && pContext->stream_info->frameSize > 0)
			{
				DWORD frame_bytes = pContext->stream_info->frameSize
				                    * pContext->stream_info->numChannels
				                    * sizeof(INT_PCM);
				
				pContext->output_buffer_fill = frame_bytes;
				pContext->output_buffer_pos = 0;
				pContext->current_pcm_sample += pContext->stream_info->frameSize;
			}
			else
			{
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
BOOL CPP_OMAAC_GetPCMBlock(CPs_CoDecModule* pModule, void* pBlock, DWORD* pdwBlockSize)
{
	CPs_CoDec_AAC* pContext;
	
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
	
	if (pContext->stream_mode == AAC_MODE_MP4)
		return CPP_OMAAC_GetPCMBlock_MP4(pContext, pBlock, pdwBlockSize);
	else
		return CPP_OMAAC_GetPCMBlock_ADTS(pContext, pBlock, pdwBlockSize);
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
	
	return (int)(pContext->current_pcm_sample / pContext->sample_rate);
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
	pContext = CALLOC_TYPE(CPs_CoDec_AAC, 1);
	if (!pContext)
	{
		CP_TRACE0("Failed to allocate AAC codec context");
		return;
	}
	pCoDec->m_pModuleCookie = pContext;
	
	// Setup file associations
	CPFA_InitialiseFileAssociations(pCoDec);
	CPFA_AddFileAssociation(pCoDec, "aac", 0L);
	CPFA_AddFileAssociation(pCoDec, "m4a", 0L);
}