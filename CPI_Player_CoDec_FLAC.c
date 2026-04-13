/*
 * BriskPlayer - Blazing fast audio player.
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
// C23 Enhanced FLAC Decoder
// Modern lossless audio decoder with enhanced type safety and performance
//
////////////////////////////////////////////////////////////////////////////////

#define FLAC__NO_DLL  // Static linking with FLAC library
#include "CPI_Player_CoDec_C23.h"
#include "threading_compat.h"  // Consolidated threading support
#include <FLAC/stream_decoder.h>
#include <string.h>
#include <assert.h>

////////////////////////////////////////////////////////////////////////////////
// C23 Enhanced FLAC Context with Better Type Safety

// FLAC decoder state with C23 enums
typedef enum {
    FLAC_STATE_UNINITIALIZED = 0,
    FLAC_STATE_READY,
    FLAC_STATE_DECODING,
    FLAC_STATE_END_OF_STREAM,
    FLAC_STATE_ERROR
} FlacDecoderState;

// Enhanced FLAC context structure with alignment
typedef struct FlacContext {
    // FLAC decoder instance
    alignas(16) FLAC__StreamDecoder* decoder;
    
    // File path (libFLAC owns the FILE* when using init_file)
    char file_path[MAX_PATH];
    uint64_t file_size_bytes;  // For bitrate estimation
    
    // Audio format information (enhanced with C23 types)
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint64_t total_samples;
    uint64_t current_sample;
    
    // State management
    FlacDecoderState state;
    bool end_of_stream;
    
    // PCM buffer management with enhanced alignment
    alignas(16) int32_t* pcm_buffer;
    size_t buffer_size;
    size_t buffer_position;
    size_t samples_in_buffer;
    
    // Thread synchronization
    mtx_t context_mutex;
    
    // Error handling
    char error_message[256];
    FLAC__StreamDecoderErrorStatus last_error;
} FlacContext;

// C23 constexpr configuration
#define FLAC_BUFFER_SIZE 4096
#define FLAC_MAX_CHANNELS 8
#define FLAC_MAX_BITS_PER_SAMPLE 32

////////////////////////////////////////////////////////////////////////////////
// C23 Enhanced FLAC Callbacks with Improved Error Handling

// Write callback with C23 enhanced sample processing
static FLAC__StreamDecoderWriteStatus flac_write_callback_c23(
    const FLAC__StreamDecoder* decoder,
    const FLAC__Frame* frame,
    const FLAC__int32* const buffer[],
    void* client_data)
{
    (void)decoder;  // Suppress unused parameter warning
    FlacContext* context = (FlacContext*)client_data;
    
    if (!context || !frame || !buffer) {
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }
    
    // Validate frame parameters with enhanced checking
    if (frame->header.channels > FLAC_MAX_CHANNELS ||
        frame->header.bits_per_sample > FLAC_MAX_BITS_PER_SAMPLE ||
        frame->header.blocksize == 0) {
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }
    
    // Calculate required buffer size
    const size_t samples_needed = frame->header.blocksize * frame->header.channels;
    
    // Ensure buffer capacity with C23 VLA-style allocation
    if (context->buffer_size < samples_needed) {
        // Reallocate with enhanced alignment
        int32_t* new_buffer = (int32_t*)_aligned_malloc(
            samples_needed * sizeof(int32_t), 16);
        if (!new_buffer) {
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        }
        
        _aligned_free(context->pcm_buffer);
        context->pcm_buffer = new_buffer;
        context->buffer_size = samples_needed;
    }
    
    // Enhanced sample conversion with SIMD-friendly loop structure
    size_t sample_index = 0;
    for (size_t i = 0; i < frame->header.blocksize; ++i) {
        for (uint8_t ch = 0; ch < frame->header.channels; ++ch) {
            context->pcm_buffer[sample_index++] = buffer[ch][i];
        }
    }
    
    // Update context state
    context->samples_in_buffer = samples_needed;
    context->buffer_position = 0;
    // FLAC frames carry either a sample number (variable block size) or a frame
    // number (fixed block size, the default for most files).  Reading the wrong
    // union member produces a tiny frame count instead of a real sample position,
    // which breaks position tracking and the Seek range optimisation.
    if (frame->header.number_type == FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER) {
        context->current_sample = frame->header.number.sample_number;
    } else {
        context->current_sample =
            (uint64_t)frame->header.number.frame_number * frame->header.blocksize;
    }
    
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

// Enhanced metadata callback with C23 features
static void flac_metadata_callback_c23(
    const FLAC__StreamDecoder* decoder,
    const FLAC__StreamMetadata* metadata,
    void* client_data)
{
    (void)decoder;  // Suppress unused parameter warning
    FlacContext* context = (FlacContext*)client_data;
    
    if (!context || !metadata) return;
    
    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        // Extract audio format with enhanced validation
        const FLAC__StreamMetadata_StreamInfo* info = &metadata->data.stream_info;
        
        context->sample_rate = info->sample_rate;
        context->channels = (uint8_t)info->channels;
        context->bits_per_sample = (uint8_t)info->bits_per_sample;
        context->total_samples = info->total_samples;
        
        // Validate parameters
        if (context->sample_rate == 0 || context->channels == 0 ||
            context->bits_per_sample == 0) {
            context->state = FLAC_STATE_ERROR;
            strncpy(context->error_message, "Invalid FLAC stream info", 
                   sizeof(context->error_message) - 1);
        } else {
            context->state = FLAC_STATE_READY;
        }
    }
}

// Enhanced error callback
static void flac_error_callback_c23(
    const FLAC__StreamDecoder* decoder,
    FLAC__StreamDecoderErrorStatus status,
    void* client_data)
{
    (void)decoder;
    FlacContext* context = (FlacContext*)client_data;
    
    if (context) {
        // Record the error but do NOT change context->state here.  This callback
        // fires with LOST_SYNC during seek_absolute's binary search, which is
        // completely normal.  Changing state to ERROR here causes GetPCMBlock to
        // think the decoder is broken even after a successful seek.  The actual
        // libFLAC decoder state is queried directly via
        // FLAC__stream_decoder_get_state() wherever recovery decisions are made.
        context->last_error = status;
        snprintf(context->error_message, sizeof(context->error_message),
                "FLAC decode error: %s", FLAC__StreamDecoderErrorStatusString[status]);
    }
}

////////////////////////////////////////////////////////////////////////////////
// C23 Enhanced FLAC Module Implementation

// CloseFile — release the current file but keep the module alive.
// The engine calls this when the stream ends (GetPCMBlock returns FALSE).
static void CPP_OMFLAC_CloseFile(CPs_CoDecModule* module)
{
    if (!module || !module->m_pModuleCookie) return;
    
    FlacContext* context = (FlacContext*)module->m_pModuleCookie;
    
    if (context->decoder) {
        FLAC__stream_decoder_finish(context->decoder);
        FLAC__stream_decoder_delete(context->decoder);
        context->decoder = NULL;
    }
    
    _aligned_free(context->pcm_buffer);
    context->pcm_buffer = NULL;
    context->buffer_size = 0;
    context->samples_in_buffer = 0;
    context->buffer_position = 0;
    context->state = FLAC_STATE_UNINITIALIZED;
}

// Initialize FLAC decoder with C23 enhancements
static void CPP_OMFLAC_Uninitialise(CPs_CoDecModule* module)
{
    if (!module || !module->m_pModuleCookie) return;
    
    FlacContext* context = (FlacContext*)module->m_pModuleCookie;
    
    // Cleanup FLAC decoder (finish closes libFLAC's internal FILE*)
    if (context->decoder) {
        FLAC__stream_decoder_finish(context->decoder);
        FLAC__stream_decoder_delete(context->decoder);
    }
    
    // Free PCM buffer
    _aligned_free(context->pcm_buffer);
    
    // Destroy mutex
    mtx_destroy(&context->context_mutex);
    
    _aligned_free(context);
    
    module->m_pModuleCookie = NULL;
}

// Open file with enhanced error handling
static BOOL CPP_OMFLAC_OpenFile(CPs_CoDecModule* module, 
                                const char* filename,
                                DWORD_PTR cookie,
                                HWND owner)
{
    (void)cookie; (void)owner;  // Unused parameters
    if (!module || !filename) return FALSE;
    
    FlacContext* context = (FlacContext*)_aligned_malloc(sizeof(FlacContext), 16);
    if (!context) return FALSE;
    memset(context, 0, sizeof(FlacContext));
    
    // Store path for error messages; truncate gracefully if too long
    strncpy(context->file_path, filename, MAX_PATH - 1);
    context->file_path[MAX_PATH - 1] = '\0';
    
    // Get file size for bitrate estimation
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (GetFileAttributesExA(filename, GetFileExInfoStandard, &fa)) {
        context->file_size_bytes =
            ((uint64_t)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
    }
    
    context->last_error = FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC;
    
    if (mtx_init(&context->context_mutex, mtx_plain) != thrd_success) {
        _aligned_free(context);
        return FALSE;
    }
    
    context->decoder = FLAC__stream_decoder_new();
    if (!context->decoder) {
        mtx_destroy(&context->context_mutex);
        _aligned_free(context);
        return FALSE;
    }
    
    // Use libFLAC's built-in file I/O (fopen/fseek/fread) so that
    // seek_absolute has complete, consistent control over the FILE*.
    // This avoids all seek/tell/eof callback complexity and is how
    // libFLAC's own example players are written.
    FLAC__StreamDecoderInitStatus init_status =
        FLAC__stream_decoder_init_file(context->decoder,
                                       filename,
                                       flac_write_callback_c23,
                                       flac_metadata_callback_c23,
                                       flac_error_callback_c23,
                                       context);
    
    if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        FLAC__stream_decoder_delete(context->decoder);
        mtx_destroy(&context->context_mutex);
        _aligned_free(context);
        return FALSE;
    }
    
    if (!FLAC__stream_decoder_process_until_end_of_metadata(context->decoder)) {
        FLAC__stream_decoder_finish(context->decoder);
        FLAC__stream_decoder_delete(context->decoder);
        mtx_destroy(&context->context_mutex);
        _aligned_free(context);
        return FALSE;
    }
    
    if (context->state != FLAC_STATE_READY) {
        FLAC__stream_decoder_finish(context->decoder);
        FLAC__stream_decoder_delete(context->decoder);
        mtx_destroy(&context->context_mutex);
        _aligned_free(context);
        return FALSE;
    }
    
    module->m_pModuleCookie = context;
    return TRUE;
}

// Helper: convert int32 FLAC samples to int16 PCM and write to output buffer.
// Returns the number of int16 values written.
static size_t flac_convert_samples(const int32_t* src, int16_t* dst,
                                   size_t count, int bits_per_sample)
{
    int shift = 0;
    if (bits_per_sample > 16)
        shift = bits_per_sample - 16;
    else if (bits_per_sample < 16)
        shift = -(16 - bits_per_sample);
    
    for (size_t i = 0; i < count; ++i) {
        int32_t s = src[i];
        if (shift > 0)      s >>= shift;
        else if (shift < 0) s <<= (-shift);
        dst[i] = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
    }
    return count;
}

// GetPCMBlock — fill loop modelled on the FFmpeg and OGG decoders.
// Keeps decoding frames until the full requested buffer is filled or EOF.
static BOOL CPP_OMFLAC_GetPCMBlock(CPs_CoDecModule* module, 
                                   void* block,
                                   DWORD* block_size)
{
    if (!module || !block || !block_size || !module->m_pModuleCookie) return FALSE;
    
    FlacContext* context = (FlacContext*)module->m_pModuleCookie;
    
    if (mtx_lock(&context->context_mutex) != thrd_success) {
        *block_size = 0;
        return FALSE;
    }
    
    if (context->end_of_stream || context->state == FLAC_STATE_END_OF_STREAM) {
        mtx_unlock(&context->context_mutex);
        *block_size = 0;
        return FALSE;
    }
    
    const DWORD requested = *block_size;            // bytes requested
    DWORD       bytes_written = 0;                  // bytes produced so far
    int16_t*    out = (int16_t*)block;
    
    while (bytes_written < requested) {
        // ---- serve data already sitting in pcm_buffer -----------------------
        if (context->buffer_position < context->samples_in_buffer) {
            size_t avail  = context->samples_in_buffer - context->buffer_position;
            size_t room   = (requested - bytes_written) / sizeof(int16_t);
            size_t to_copy = (avail < room) ? avail : room;
            
            flac_convert_samples(
                &context->pcm_buffer[context->buffer_position],
                out + (bytes_written / sizeof(int16_t)),
                to_copy,
                context->bits_per_sample);
            
            bytes_written += (DWORD)(to_copy * sizeof(int16_t));
            context->buffer_position += to_copy;
            continue;
        }
        
        // ---- pcm_buffer exhausted — decode one more FLAC frame ----------------
        // Check libFLAC state BEFORE calling process_single.
        FLAC__StreamDecoderState ds =
            FLAC__stream_decoder_get_state(context->decoder);
        
        if (ds == FLAC__STREAM_DECODER_END_OF_STREAM) {
            context->end_of_stream = true;
            context->state = FLAC_STATE_END_OF_STREAM;
            break;
        }
        
        if (ds == FLAC__STREAM_DECODER_SEEK_ERROR) {
            if (!FLAC__stream_decoder_reset(context->decoder) ||
                !FLAC__stream_decoder_process_until_end_of_metadata(context->decoder)) {
                context->state = FLAC_STATE_ERROR;
                break;
            }
            context->state = FLAC_STATE_READY;
        } else if (ds == FLAC__STREAM_DECODER_ABORTED ||
                   ds == FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR) {
            if (!FLAC__stream_decoder_reset(context->decoder)) {
                context->state = FLAC_STATE_ERROR;
                break;
            }
            context->state = FLAC_STATE_READY;
        }
        
        // Decode a single FLAC frame → fires write_callback → fills pcm_buffer.
        if (!FLAC__stream_decoder_process_single(context->decoder)) {
            ds = FLAC__stream_decoder_get_state(context->decoder);
            if (ds == FLAC__STREAM_DECODER_END_OF_STREAM) {
                context->end_of_stream = true;
                context->state = FLAC_STATE_END_OF_STREAM;
            }
            break;   // stop filling — whatever we have so far will be returned
        }
    }
    
    *block_size = bytes_written;
    mtx_unlock(&context->context_mutex);
    return (bytes_written > 0) ? TRUE : FALSE;
}

// Seek — modelled on the FFmpeg decoder: seek, flush, clear buffer.
// All subsequent decoding is handled by GetPCMBlock's fill loop.
static void CPP_OMFLAC_Seek(CPs_CoDecModule* module, const int iNumerator, const int iDenominator)
{
    if (!module || !module->m_pModuleCookie || iDenominator == 0) return;
    
    FlacContext* context = (FlacContext*)module->m_pModuleCookie;
    
    if (!context->decoder || context->total_samples == 0) return;
    
    double seek_ratio = (double)iNumerator / (double)iDenominator;
    uint64_t sample_position = (uint64_t)(seek_ratio * (double)context->total_samples);
    
    if (sample_position >= context->total_samples)
        sample_position = context->total_samples - 1;
    
    if (mtx_lock(&context->context_mutex) != thrd_success) return;
    
    // Flush the decoder's internal input buffer (equivalent of avcodec_flush_buffers).
    // This discards any partially-read data and sets state to SEARCH_FOR_FRAME_SYNC.
    FLAC__stream_decoder_flush(context->decoder);
    
    // Perform the sample-accurate seek.
    if (FLAC__stream_decoder_seek_absolute(context->decoder, sample_position)) {
        context->current_sample = sample_position;
        context->state = FLAC_STATE_READY;
    } else {
        // seek_absolute failed — recover via full reset.
        if (FLAC__stream_decoder_reset(context->decoder) &&
            FLAC__stream_decoder_process_until_end_of_metadata(context->decoder)) {
            context->state = FLAC_STATE_READY;
        } else {
            context->state = FLAC_STATE_ERROR;
        }
    }
    
    // Discard any data that write_callback may have deposited during the
    // seek's binary search.  GetPCMBlock's fill loop will decode fresh frames.
    context->buffer_position = 0;
    context->samples_in_buffer = 0;
    context->end_of_stream = false;
    
    mtx_unlock(&context->context_mutex);
}

// Get file information
static void CPP_OMFLAC_GetFileInfo(CPs_CoDecModule* module, CPs_FileInfo* info)
{
    if (!module || !info || !module->m_pModuleCookie) {
        return;
    }
    
    FlacContext* context = (FlacContext*)module->m_pModuleCookie;
    
    // Fill file information from FLAC context
    info->m_iFreq_Hz = (UINT)context->sample_rate;
    info->m_bStereo = (context->channels == 2) ? TRUE : FALSE;
    info->m_b16bit = TRUE;  // FLAC always outputs 16-bit in our implementation
    
    // Calculate duration
    if (context->total_samples > 0 && context->sample_rate > 0) {
        info->m_iFileLength_Secs = (UINT)(context->total_samples / context->sample_rate);
    } else {
        info->m_iFileLength_Secs = 0;
    }
    
    // Estimate bitrate (FLAC is variable bitrate)
    if (info->m_iFileLength_Secs > 0 && context->file_size_bytes > 0) {
        // Try to get file size for bitrate estimation
        if (context->file_size_bytes > 0) {
            info->m_iBitRate_Kbs = (UINT)((context->file_size_bytes * 8) /
                                          (info->m_iFileLength_Secs * 1000));
        } else {
            info->m_iBitRate_Kbs = 0;
        }
    } else {
        info->m_iBitRate_Kbs = 0;
    }
}

// Get current playback position
static int CPP_OMFLAC_GetCurrentPos_secs(CPs_CoDecModule* module)
{
    if (!module || !module->m_pModuleCookie) {
        return 0;
    }
    
    FlacContext* context = (FlacContext*)module->m_pModuleCookie;
    
    if (context->sample_rate > 0) {
        // Thread-safe position calculation
        if (mtx_lock(&context->context_mutex) == thrd_success) {
            uint64_t current_position;
            
            // current_sample represents the start of the current frame
            // Add samples consumed from the current buffer
            if (context->samples_in_buffer > 0 && context->buffer_position > 0) {
                current_position = context->current_sample + (context->buffer_position / context->channels);
            } else {
                current_position = context->current_sample;
            }
            
            mtx_unlock(&context->context_mutex);
            return (int)(current_position / context->sample_rate);
        }
    }
    
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// C23 Enhanced Module Registration

// Create C23 enhanced FLAC codec module
CodecModule* create_flac_codec(void)
{
    CodecModule* codec = create_codec(CODEC_TYPE_FLAC);
    if (!codec) return NULL;
    
    // Enhanced initialization with function pointers
    codec->open_file = (CodecOpenFunc)CPP_OMFLAC_OpenFile;
    codec->close_file = (CodecCloseFunc)CPP_OMFLAC_Uninitialise;
    codec->decode_block = (CodecDecodeFunc)CPP_OMFLAC_GetPCMBlock;
    codec->seek_position = (CodecSeekFunc)CPP_OMFLAC_Seek;
    
    return codec;
}

// Legacy compatibility wrapper
CPs_CoDecModule* CPP_OMFLAC_Create(void)
{
    // Allocate legacy module structure
    CPs_CoDecModule* module = (CPs_CoDecModule*)calloc(1, sizeof(CPs_CoDecModule));
    if (!module) return NULL;
    
    // Enhanced initialization with C23 functions
    module->Uninitialise = CPP_OMFLAC_Uninitialise;
    module->OpenFile = CPP_OMFLAC_OpenFile;
    module->CloseFile = CPP_OMFLAC_CloseFile;
    module->Seek = CPP_OMFLAC_Seek;
    module->GetFileInfo = NULL;  // TODO: Implement enhanced file info
    module->GetPCMBlock = CPP_OMFLAC_GetPCMBlock;
    
    return module;
}

// Standard codec initialization function
void CP_InitialiseCodec_FLAC(CPs_CoDecModule* codec)
{
    if (!codec) return;
    
    // Initialize function pointers
    codec->Uninitialise = CPP_OMFLAC_Uninitialise;
    codec->OpenFile = CPP_OMFLAC_OpenFile;
    codec->CloseFile = CPP_OMFLAC_CloseFile;
    codec->Seek = CPP_OMFLAC_Seek;
    codec->GetFileInfo = CPP_OMFLAC_GetFileInfo;
    codec->GetPCMBlock = CPP_OMFLAC_GetPCMBlock;
    codec->GetCurrentPos_secs = CPP_OMFLAC_GetCurrentPos_secs;
    
    codec->m_pModuleCookie = NULL;
    
    // Initialize file associations for FLAC formats
    CPFA_InitialiseFileAssociations(codec);
    CPFA_AddFileAssociation(codec, "FLAC", 0L);  // Free Lossless Audio Codec
    CPFA_AddFileAssociation(codec, "FLA", 0L);   // Alternative FLAC extension
}