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
// C23 Enhanced OGG Vorbis Decoder
// Modern Vorbis decoder with enhanced performance and type safety
//
////////////////////////////////////////////////////////////////////////////////

#include "CPI_Player_CoDec_C23.h"
#include "CPI_Stream.h"
#include "ogg/ogg.h"
#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"
#include <string.h>
#include <assert.h>

////////////////////////////////////////////////////////////////////////////////
// C23 Enhanced OGG Context with Better Memory Management

// OGG decoder state with C23 enums
typedef enum {
    OGG_STATE_UNINITIALIZED = 0,
    OGG_STATE_READY,
    OGG_STATE_DECODING,
    OGG_STATE_END_OF_STREAM,
    OGG_STATE_ERROR
} OggDecoderState;

// Enhanced OGG context structure with alignment
typedef struct OggContext {
    // Vorbis file structure
    alignas(16) OggVorbis_File vorbis_file;
    
    // Stream interface (thread-safe, no global variables)
    CPs_InStream* input_stream;
    
    // Audio format information
    uint32_t sample_rate;
    uint8_t channels;
    uint32_t bitrate_nominal;
    uint32_t bitrate_upper;
    uint32_t bitrate_lower;
    uint64_t total_samples;
    uint64_t current_sample;
    
    // State management
    OggDecoderState state;
    int current_section;
    bool end_of_stream;
    
    // Enhanced error handling
    char error_message[256];
    int last_vorbis_error;
    
    // Performance counters
    uint64_t bytes_decoded;
    uint64_t decode_errors;
} OggContext;

// Thread-local storage for stream callbacks (eliminates global variables)
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Thread_local CPs_InStream* tl_current_stream = NULL;
#else
static CPs_InStream* tl_current_stream = NULL;
#endif

////////////////////////////////////////////////////////////////////////////////
// C23 Enhanced Vorbis Callbacks with Better Error Handling

// Enhanced read callback with bounds checking
static size_t ogg_read_callback(void* buffer, size_t size, size_t count, void* stream_ptr)
{
    OggContext* context = (OggContext*)stream_ptr;
    
    if (!context || !context->input_stream || !buffer) {
        return 0;
    }
    
    // Enhanced bounds checking
    const size_t total_bytes = size * count;
    if (total_bytes == 0 || total_bytes > 65536) {  // Reasonable upper limit
        return 0;
    }
    
    size_t bytes_read = 0;
    context->input_stream->Read(context->input_stream, buffer, total_bytes, &bytes_read);
    
    // Update statistics
    context->bytes_decoded += bytes_read;
    
    return bytes_read / size;  // Return number of items read
}

// Enhanced seek callback with better error handling
static int ogg_seek_callback(void* stream_ptr, ogg_int64_t offset, int whence)
{
    OggContext* context = (OggContext*)stream_ptr;
    
    if (!context || !context->input_stream) {
        return -1;
    }
    
    if (!context->input_stream->IsSeekable(context->input_stream)) {
        return -1;
    }
    
    uint64_t new_position;
    
    switch (whence) {
        case SEEK_SET:
            new_position = (uint64_t)offset;
            break;
            
        case SEEK_CUR:
            new_position = context->input_stream->Tell(context->input_stream) + offset;
            break;
            
        case SEEK_END:
            new_position = context->input_stream->GetLength(context->input_stream) + offset;
            break;
            
        default:
            return -1;
    }
    
    // Bounds checking
    if (new_position > context->input_stream->GetLength(context->input_stream)) {
        return -1;
    }
    
    context->input_stream->Seek(context->input_stream, new_position);
    return 0;
}

// Enhanced close callback (no-op for our implementation)
static int ogg_close_callback(void* stream_ptr)
{
    (void)stream_ptr;  // Unused parameter
    return 0;  // Success - we handle cleanup elsewhere
}

// Enhanced tell callback with error checking
static long ogg_tell_callback(void* stream_ptr)
{
    OggContext* context = (OggContext*)stream_ptr;
    
    if (!context || !context->input_stream) {
        return -1;
    }
    
    return (long)context->input_stream->Tell(context->input_stream);
}

////////////////////////////////////////////////////////////////////////////////
// C23 Enhanced OGG Module Implementation

// Initialize OGG decoder with enhanced cleanup
static void CPP_OMOGG_Uninitialise(CPs_CoDecModule* module)
{
    if (!module || !module->m_pModuleCookie) return;
    
    OggContext* context = (OggContext*)module->m_pModuleCookie;
    
    // Cleanup Vorbis file
    if (context->state != OGG_STATE_UNINITIALIZED) {
        ov_clear(&context->vorbis_file);
    }
    
    // Clear sensitive data
    memset(context, 0, sizeof(OggContext));
    free(context);
    
    module->m_pModuleCookie = NULL;
    
    // Clean up file associations
    CPFA_EmptyFileAssociations(module);
}

// Open file with enhanced initialization
static BOOL CPP_OMOGG_OpenFile(CPs_CoDecModule* module,
                               const char* filename, 
                               DWORD cookie, 
                               HWND owner)
{
    (void)cookie; (void)owner;  // Unused parameters
    
    if (!module || !filename) return FALSE;
    
    // Create enhanced context with proper alignment
    OggContext* context = (OggContext*)_aligned_malloc(sizeof(OggContext), 16);
    if (!context) return FALSE;
    
    // Initialize with designated initializers
    *context = (OggContext){
        .input_stream = NULL,
        .state = OGG_STATE_UNINITIALIZED,
        .current_section = -1,
        .end_of_stream = false,
        .bytes_decoded = 0,
        .decode_errors = 0
    };
    
    // Open stream
    context->input_stream = CP_CreateInStream(filename, NULL);
    if (!context->input_stream) {
        _aligned_free(context);
        return FALSE;
    }
    
    // Setup custom callbacks structure
    ov_callbacks callbacks = {
        .read_func = ogg_read_callback,
        .seek_func = ogg_seek_callback,
        .close_func = ogg_close_callback,
        .tell_func = ogg_tell_callback
    };
    
    // Initialize Vorbis file with enhanced error handling
    int result = ov_open_callbacks(context, &context->vorbis_file, NULL, 0, callbacks);
    
    if (result < 0) {
        // Enhanced error reporting
        switch (result) {
            case OV_EREAD:
                strncpy(context->error_message, "Read error in Vorbis stream", 
                       sizeof(context->error_message) - 1);
                break;
            case OV_ENOTVORBIS:
                strncpy(context->error_message, "Not a valid Vorbis file", 
                       sizeof(context->error_message) - 1);
                break;
            case OV_EVERSION:
                strncpy(context->error_message, "Vorbis version mismatch", 
                       sizeof(context->error_message) - 1);
                break;
            case OV_EBADHEADER:
                strncpy(context->error_message, "Invalid Vorbis header", 
                       sizeof(context->error_message) - 1);
                break;
            default:
                snprintf(context->error_message, sizeof(context->error_message),
                        "Vorbis initialization failed: %d", result);
                break;
        }
        
        context->input_stream->Uninitialise(context->input_stream);
        _aligned_free(context);
        return FALSE;
    }
    
    // Get enhanced audio format information
    vorbis_info* info = ov_info(&context->vorbis_file, -1);
    if (!info) {
        ov_clear(&context->vorbis_file);
        context->input_stream->Uninitialise(context->input_stream);
        _aligned_free(context);
        return FALSE;
    }
    
    // Extract format information with validation
    context->sample_rate = (uint32_t)info->rate;
    context->channels = (uint8_t)info->channels;
    context->bitrate_nominal = (uint32_t)info->bitrate_nominal;
    context->bitrate_upper = (uint32_t)info->bitrate_upper;
    context->bitrate_lower = (uint32_t)info->bitrate_lower;
    context->total_samples = (uint64_t)ov_pcm_total(&context->vorbis_file, -1);
    
    // Validate parameters
    if (context->sample_rate == 0 || context->channels == 0 ||
        context->channels > 8 || context->total_samples == 0) {
        strncpy(context->error_message, "Invalid Vorbis audio parameters", 
               sizeof(context->error_message) - 1);
        ov_clear(&context->vorbis_file);
        context->input_stream->Uninitialise(context->input_stream);
        _aligned_free(context);
        return FALSE;
    }
    
    context->state = OGG_STATE_READY;
    module->m_pModuleCookie = context;
    
    return TRUE;
}

// Enhanced PCM block processing with better error handling
static BOOL CPP_OMOGG_GetPCMBlock(CPs_CoDecModule* module, void* block, DWORD* block_size)
{
    if (!module || !block || !block_size || !module->m_pModuleCookie) {
        return FALSE;
    }
    
    OggContext* context = (OggContext*)module->m_pModuleCookie;
    
    // Check for end of stream
    if (context->end_of_stream || context->state == OGG_STATE_END_OF_STREAM) {
        *block_size = 0;
        return FALSE;
    }
    
    DWORD bytes_read = 0;
    const DWORD requested_size = *block_size;
    
    // Enhanced reading loop with better error handling
    while (bytes_read < requested_size) {
        const long result = ov_read(&context->vorbis_file,
                                   (char*)block + bytes_read,
                                   requested_size - bytes_read,
                                   0,    // little endian
                                   2,    // 16-bit samples
                                   1,    // signed
                                   &context->current_section);
        
        if (result == 0) {
            // End of stream reached
            context->end_of_stream = true;
            context->state = OGG_STATE_END_OF_STREAM;
            break;
        } else if (result < 0) {
            // Enhanced error handling
            context->decode_errors++;
            
            switch (result) {
                case OV_HOLE:
                    // Hole in data stream - recoverable
                    continue;
                    
                case OV_EBADLINK:
                    strncpy(context->error_message, "Invalid stream section", 
                           sizeof(context->error_message) - 1);
                    context->state = OGG_STATE_ERROR;
                    return FALSE;
                    
                case OV_EINVAL:
                    strncpy(context->error_message, "Invalid decode parameters", 
                           sizeof(context->error_message) - 1);
                    context->state = OGG_STATE_ERROR;
                    return FALSE;
                    
                default:
                    snprintf(context->error_message, sizeof(context->error_message),
                            "Vorbis decode error: %ld", result);
                    context->state = OGG_STATE_ERROR;
                    return FALSE;
            }
        } else {
            // Successful read
            bytes_read += (DWORD)result;
            context->current_sample += result / (context->channels * 2);  // 16-bit samples
        }
    }
    
    *block_size = bytes_read;
    return (bytes_read > 0) ? TRUE : FALSE;
}

// Enhanced seek operation with better precision
static void CPP_OMOGG_Seek(CPs_CoDecModule* module, int numerator, int denominator)
{
    if (!module || !module->m_pModuleCookie || denominator == 0) {
        return;
    }
    
    OggContext* context = (OggContext*)module->m_pModuleCookie;
    
    if (context->total_samples == 0) {
        return;
    }
    
    // Calculate target sample with enhanced precision
    const double seek_ratio = (double)numerator / (double)denominator;
    const ogg_int64_t target_sample = (ogg_int64_t)(seek_ratio * context->total_samples);
    
    // Perform seek with error checking
    const int result = ov_pcm_seek(&context->vorbis_file, target_sample);
    
    if (result == 0) {
        // Successful seek
        context->current_sample = (uint64_t)target_sample;
        context->end_of_stream = false;
        context->state = OGG_STATE_READY;
    } else {
        // Seek failed - enhanced error reporting
        switch (result) {
            case OV_ENOSEEK:
                strncpy(context->error_message, "Stream not seekable", 
                       sizeof(context->error_message) - 1);
                break;
            case OV_EINVAL:
                strncpy(context->error_message, "Invalid seek position", 
                       sizeof(context->error_message) - 1);
                break;
            case OV_EREAD:
                strncpy(context->error_message, "Read error during seek", 
                       sizeof(context->error_message) - 1);
                break;
            default:
                snprintf(context->error_message, sizeof(context->error_message),
                        "Seek failed: %d", result);
                break;
        }
    }
}

// Enhanced file info with detailed format information
static void CPP_OMOGG_GetFileInfo(CPs_CoDecModule* module, CPs_FileInfo* info)
{
    if (!module || !info || !module->m_pModuleCookie) {
        return;
    }
    
    OggContext* context = (OggContext*)module->m_pModuleCookie;
    
    // Enhanced file information
    info->m_iFreq_Hz = (UINT)context->sample_rate;
    info->m_bStereo = (context->channels == 2) ? TRUE : FALSE;
    info->m_b16bit = TRUE;  // OGG always outputs 16-bit
    
    // Calculate duration with better precision
    if (context->total_samples > 0 && context->sample_rate > 0) {
        info->m_iFileLength_Secs = (UINT)(context->total_samples / context->sample_rate);
    } else {
        info->m_iFileLength_Secs = 0;
    }
    
    // Enhanced bitrate information
    if (context->bitrate_nominal > 0) {
        info->m_iBitRate_Kbs = (UINT)(context->bitrate_nominal / 1000);
    } else if (info->m_iFileLength_Secs > 0) {
        // Estimate from file size and duration
        const uint64_t file_size = context->input_stream->GetLength(context->input_stream);
        info->m_iBitRate_Kbs = (UINT)((file_size * 8) / (info->m_iFileLength_Secs * 1000));
    } else {
        info->m_iBitRate_Kbs = 0;
    }
}

// Get current playback position
static int CPP_OMOGG_GetCurrentPos_secs(CPs_CoDecModule* module)
{
    if (!module || !module->m_pModuleCookie) {
        return 0;
    }
    
    OggContext* context = (OggContext*)module->m_pModuleCookie;
    
    if (context->sample_rate == 0) {
        return 0;
    }
    
    return (int)(context->current_sample / context->sample_rate);
}

////////////////////////////////////////////////////////////////////////////////
// C23 Enhanced Module Registration

// Create enhanced OGG codec module
void CP_InitialiseCodec_OGG(CPs_CoDecModule* codec)
{
    if (!codec) return;
    
    // Enhanced initialization with function pointers
    codec->Uninitialise = CPP_OMOGG_Uninitialise;
    codec->OpenFile = CPP_OMOGG_OpenFile;
    codec->CloseFile = NULL;  // Handled by Uninitialise
    codec->Seek = CPP_OMOGG_Seek;
    codec->GetFileInfo = CPP_OMOGG_GetFileInfo;
    codec->GetPCMBlock = CPP_OMOGG_GetPCMBlock;
    codec->GetCurrentPos_secs = CPP_OMOGG_GetCurrentPos_secs;
    
    codec->m_pModuleCookie = NULL;
    
    // Initialize file associations with enhanced extensions
    CPFA_InitialiseFileAssociations(codec);
    CPFA_AddFileAssociation(codec, "OGG", 0L);  // Standard OGG Vorbis
    CPFA_AddFileAssociation(codec, "OGA", 0L);  // OGG Audio
    CPFA_AddFileAssociation(codec, "OGV", 0L);  // OGG Video (audio track)
    CPFA_AddFileAssociation(codec, "OGX", 0L);  // OGG Multiplex
}

// Legacy compatibility wrapper
CPs_CoDecModule* CPP_OMOGG_Create(void)
{
    CPs_CoDecModule* module = (CPs_CoDecModule*)calloc(1, sizeof(CPs_CoDecModule));
    if (!module) return NULL;
    
    CP_InitialiseCodec_OGG(module);
    return module;
}