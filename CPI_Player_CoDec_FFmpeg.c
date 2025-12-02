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
// C23 Enhanced FFmpeg Decoder
// Universal audio decoder supporting multiple formats through FFmpeg
//
////////////////////////////////////////////////////////////////////////////////

#include "CPI_Player_CoDec_C23.h"
#include "CPI_Stream.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <string.h>
#include <assert.h>

// Threading support with fallback
#if HAVE_C23_THREADING
    #include <threads.h>
#else
    // Use Windows CRITICAL_SECTION as fallback
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    
    // Define mtx_t as CRITICAL_SECTION for Windows fallback
    typedef CRITICAL_SECTION mtx_t;
    #define mtx_plain 0
    
    static inline int mtx_init(mtx_t* mtx, int type) {
        (void)type;  // Unused
        InitializeCriticalSection(mtx);
        return 0;  // Success
    }
    
    static inline int mtx_lock(mtx_t* mtx) {
        EnterCriticalSection(mtx);
        return 0;  // Success
    }
    
    static inline int mtx_unlock(mtx_t* mtx) {
        LeaveCriticalSection(mtx);
        return 0;  // Success
    }
    
    static inline void mtx_destroy(mtx_t* mtx) {
        DeleteCriticalSection(mtx);
    }
    
    // Define thrd_success constant for compatibility
    #define thrd_success 0
#endif

////////////////////////////////////////////////////////////////////////////////
// C23 Enhanced FFmpeg Context with Better Type Safety

// FFmpeg decoder state with C23 enums
typedef enum {
    FFMPEG_STATE_UNINITIALIZED = 0,
    FFMPEG_STATE_READY,
    FFMPEG_STATE_DECODING,
    FFMPEG_STATE_END_OF_STREAM,
    FFMPEG_STATE_ERROR
} FFmpegDecoderState;

// Enhanced FFmpeg context structure with alignment
typedef struct FFmpegContext {
    // FFmpeg components
    alignas(16) AVFormatContext* format_ctx;
    AVCodecContext* codec_ctx;
    const AVCodec* codec;
    SwrContext* swr_ctx;
    
    // Stream interface
    CPs_InStream* stream;
    
    // Audio stream information
    int audio_stream_index;
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint64_t total_samples;
    uint64_t current_sample;
    
    // State management
    FFmpegDecoderState state;
    bool end_of_stream;
    bool mutex_initialized;  // Track if mutex was initialized
    
    // PCM buffer management with enhanced alignment
    alignas(16) int16_t* pcm_buffer;
    size_t buffer_size;
    size_t buffer_position;
    size_t samples_in_buffer;
    
    // Thread synchronization
    mtx_t context_mutex;
    
    // Error handling
    char error_message[256];
    int last_error;
    
    // File path for FFmpeg
    char* file_path;
    
    // Frame and packet for decoding
    AVFrame* frame;
    AVPacket* packet;
} FFmpegContext;

// C23 constexpr configuration
#define FFMPEG_BUFFER_SIZE 8192
#define FFMPEG_MAX_CHANNELS 8
#define FFMPEG_OUTPUT_SAMPLE_RATE 44100
#define FFMPEG_OUTPUT_CHANNELS 2
#define FFMPEG_OUTPUT_FORMAT AV_SAMPLE_FMT_S16

////////////////////////////////////////////////////////////////////////////////
// Helper Functions

static void ffmpeg_log_error(FFmpegContext* context, int error_code, const char* message) {
    if (context && message) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(error_code, err_buf, sizeof(err_buf));
        snprintf(context->error_message, sizeof(context->error_message),
                 "%s: %s", message, err_buf);
    }
}

static void ffmpeg_cleanup_context(FFmpegContext* context) {
    if (!context) return;
    
    // Only lock if the mutex was initialized
    if (context->mutex_initialized) {
        mtx_lock(&context->context_mutex);
    }
    
    // Free FFmpeg resources
    if (context->frame) {
        av_frame_free(&context->frame);
        context->frame = NULL;
    }
    
    if (context->packet) {
        av_packet_free(&context->packet);
        context->packet = NULL;
    }
    
    if (context->swr_ctx) {
        swr_free(&context->swr_ctx);
        context->swr_ctx = NULL;
    }
    
    if (context->codec_ctx) {
        avcodec_free_context(&context->codec_ctx);
        context->codec_ctx = NULL;
    }
    
    if (context->format_ctx) {
        avformat_close_input(&context->format_ctx);
        context->format_ctx = NULL;
    }
    
    // Free PCM buffer
    if (context->pcm_buffer) {
        free(context->pcm_buffer);
        context->pcm_buffer = NULL;
    }
    
    // Free file path
    if (context->file_path) {
        free(context->file_path);
        context->file_path = NULL;
    }
    
    context->state = FFMPEG_STATE_UNINITIALIZED;
    
    if (context->mutex_initialized) {
        mtx_unlock(&context->context_mutex);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Codec Module Functions

static void ffmpeg_Uninitialise(CP_HCODECMODULE hModule) {
    FFmpegContext* context = (FFmpegContext*)hModule->m_pModuleCookie;
    
    if (context) {
        ffmpeg_cleanup_context(context);
        
        // Destroy mutex only if it was initialized
        if (context->mutex_initialized) {
            mtx_destroy(&context->context_mutex);
        }
        
        free(context);
        hModule->m_pModuleCookie = NULL;
    }
    
    CPFA_EmptyFileAssociations(hModule);
}

static BOOL ffmpeg_OpenFile(CP_HCODECMODULE hModule, const char* pcFilename, 
                           DWORD dwCookie, HWND hWndOwner) {
    (void)dwCookie;
    (void)hWndOwner;
    
    FFmpegContext* context = (FFmpegContext*)hModule->m_pModuleCookie;
    if (!context || !pcFilename) {
        return FALSE;
    }
    
    mtx_lock(&context->context_mutex);
    
    // Clean up any previous file
    if (context->state != FFMPEG_STATE_UNINITIALIZED) {
        ffmpeg_cleanup_context(context);
    }
    
    // Store file path
    context->file_path = _strdup(pcFilename);
    if (!context->file_path) {
        mtx_unlock(&context->context_mutex);
        return FALSE;
    }
    
    // Open input file
    int ret = avformat_open_input(&context->format_ctx, pcFilename, NULL, NULL);
    if (ret < 0) {
        ffmpeg_log_error(context, ret, "Failed to open file");
        free(context->file_path);
        context->file_path = NULL;
        mtx_unlock(&context->context_mutex);
        return FALSE;
    }
    
    // Retrieve stream information
    ret = avformat_find_stream_info(context->format_ctx, NULL);
    if (ret < 0) {
        ffmpeg_log_error(context, ret, "Failed to find stream info");
        avformat_close_input(&context->format_ctx);
        free(context->file_path);
        context->file_path = NULL;
        mtx_unlock(&context->context_mutex);
        return FALSE;
    }
    
    // Find the best audio stream
    context->audio_stream_index = av_find_best_stream(
        context->format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &context->codec, 0);
    
    if (context->audio_stream_index < 0) {
        snprintf(context->error_message, sizeof(context->error_message),
                 "No audio stream found");
        avformat_close_input(&context->format_ctx);
        free(context->file_path);
        context->file_path = NULL;
        mtx_unlock(&context->context_mutex);
        return FALSE;
    }
    
    // Allocate codec context
    context->codec_ctx = avcodec_alloc_context3(context->codec);
    if (!context->codec_ctx) {
        snprintf(context->error_message, sizeof(context->error_message),
                 "Failed to allocate codec context");
        avformat_close_input(&context->format_ctx);
        free(context->file_path);
        context->file_path = NULL;
        mtx_unlock(&context->context_mutex);
        return FALSE;
    }
    
    // Copy codec parameters
    AVStream* audio_stream = context->format_ctx->streams[context->audio_stream_index];
    ret = avcodec_parameters_to_context(context->codec_ctx, audio_stream->codecpar);
    if (ret < 0) {
        ffmpeg_log_error(context, ret, "Failed to copy codec parameters");
        avcodec_free_context(&context->codec_ctx);
        avformat_close_input(&context->format_ctx);
        free(context->file_path);
        context->file_path = NULL;
        mtx_unlock(&context->context_mutex);
        return FALSE;
    }
    
    // Open codec
    ret = avcodec_open2(context->codec_ctx, context->codec, NULL);
    if (ret < 0) {
        ffmpeg_log_error(context, ret, "Failed to open codec");
        avcodec_free_context(&context->codec_ctx);
        avformat_close_input(&context->format_ctx);
        free(context->file_path);
        context->file_path = NULL;
        mtx_unlock(&context->context_mutex);
        return FALSE;
    }
    
    // Store audio format information
    context->sample_rate = context->codec_ctx->sample_rate;
    context->channels = context->codec_ctx->ch_layout.nb_channels;
    context->bits_per_sample = 16; // We'll convert to 16-bit
    
    // Calculate total samples with better fallback handling
    // Try multiple methods to get duration, as some formats (like APE) may not report it correctly
    context->total_samples = 0;
    
    // Method 1: Try stream duration
    if (audio_stream->duration != AV_NOPTS_VALUE && audio_stream->duration > 0) {
        context->total_samples = av_rescale(audio_stream->duration,
                                           audio_stream->time_base.num * context->sample_rate,
                                           audio_stream->time_base.den);
    }
    
    // Method 2: Try format context duration
    if (context->total_samples == 0 && context->format_ctx->duration != AV_NOPTS_VALUE && context->format_ctx->duration > 0) {
        context->total_samples = av_rescale(context->format_ctx->duration,
                                           context->sample_rate,
                                           AV_TIME_BASE);
    }
    
    // Method 3: For APE and other formats, try to estimate from file size and bitrate
    if (context->total_samples == 0 && context->codec_ctx->bit_rate > 0) {
        int64_t file_size = avio_size(context->format_ctx->pb);
        if (file_size > 0) {
            // Estimate duration: (file_size * 8) / bit_rate = seconds
            // Then: seconds * sample_rate = samples
            int64_t duration_secs = (file_size * 8) / context->codec_ctx->bit_rate;
            context->total_samples = duration_secs * context->sample_rate;
        }
    }
    
    // Method 4: If still no duration, set to 0 (unknown) - let it play to the end
    // The player will detect end-of-stream naturally
    if (context->total_samples == 0) {
        printf("Warning: Could not determine file duration for %s, will play until EOF\n", pcFilename);
    }
    
    context->current_sample = 0;
    
    // Initialize resampler if needed
    if (context->codec_ctx->sample_fmt != FFMPEG_OUTPUT_FORMAT ||
        context->sample_rate != FFMPEG_OUTPUT_SAMPLE_RATE ||
        context->channels != FFMPEG_OUTPUT_CHANNELS) {
        
        // Allocate resampler context
        ret = swr_alloc_set_opts2(&context->swr_ctx,
                                  &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO,
                                  FFMPEG_OUTPUT_FORMAT,
                                  FFMPEG_OUTPUT_SAMPLE_RATE,
                                  &context->codec_ctx->ch_layout,
                                  context->codec_ctx->sample_fmt,
                                  context->codec_ctx->sample_rate,
                                  0, NULL);
        
        if (ret < 0) {
            ffmpeg_log_error(context, ret, "Failed to allocate resampler");
            avcodec_free_context(&context->codec_ctx);
            avformat_close_input(&context->format_ctx);
            free(context->file_path);
            context->file_path = NULL;
            mtx_unlock(&context->context_mutex);
            return FALSE;
        }
        
        ret = swr_init(context->swr_ctx);
        if (ret < 0) {
            ffmpeg_log_error(context, ret, "Failed to initialize resampler");
            swr_free(&context->swr_ctx);
            avcodec_free_context(&context->codec_ctx);
            avformat_close_input(&context->format_ctx);
            free(context->file_path);
            context->file_path = NULL;
            mtx_unlock(&context->context_mutex);
            return FALSE;
        }
        
        // Update output parameters
        context->sample_rate = FFMPEG_OUTPUT_SAMPLE_RATE;
        context->channels = FFMPEG_OUTPUT_CHANNELS;
    }
    
    // Allocate frame and packet
    context->frame = av_frame_alloc();
    context->packet = av_packet_alloc();
    
    if (!context->frame || !context->packet) {
        snprintf(context->error_message, sizeof(context->error_message),
                 "Failed to allocate frame/packet");
        if (context->frame) av_frame_free(&context->frame);
        if (context->packet) av_packet_free(&context->packet);
        if (context->swr_ctx) swr_free(&context->swr_ctx);
        avcodec_free_context(&context->codec_ctx);
        avformat_close_input(&context->format_ctx);
        free(context->file_path);
        context->file_path = NULL;
        mtx_unlock(&context->context_mutex);
        return FALSE;
    }
    
    // Allocate PCM buffer
    context->buffer_size = FFMPEG_BUFFER_SIZE * context->channels;
    context->pcm_buffer = CALLOC_TYPE(int16_t, context->buffer_size);
    if (!context->pcm_buffer) {
        av_frame_free(&context->frame);
        av_packet_free(&context->packet);
        if (context->swr_ctx) swr_free(&context->swr_ctx);
        avcodec_free_context(&context->codec_ctx);
        avformat_close_input(&context->format_ctx);
        free(context->file_path);
        context->file_path = NULL;
        mtx_unlock(&context->context_mutex);
        return FALSE;
    }
    
    context->buffer_position = 0;
    context->samples_in_buffer = 0;
    context->end_of_stream = false;
    context->state = FFMPEG_STATE_READY;
    
    mtx_unlock(&context->context_mutex);
    return TRUE;
}

static void ffmpeg_CloseFile(CP_HCODECMODULE hModule) {
    FFmpegContext* context = (FFmpegContext*)hModule->m_pModuleCookie;
    
    if (context) {
        ffmpeg_cleanup_context(context);
    }
}

static void ffmpeg_Seek(CP_HCODECMODULE hModule, const int iNumerator, const int iDenominator) {
    FFmpegContext* context = (FFmpegContext*)hModule->m_pModuleCookie;
    
    if (!context || context->state == FFMPEG_STATE_UNINITIALIZED) {
        return;
    }
    
    mtx_lock(&context->context_mutex);
    
    if (iDenominator == 0 || context->total_samples == 0) {
        mtx_unlock(&context->context_mutex);
        return;
    }
    
    // Calculate target sample
    uint64_t target_sample = (context->total_samples * iNumerator) / iDenominator;
    
    // Convert to timestamp
    AVStream* audio_stream = context->format_ctx->streams[context->audio_stream_index];
    int64_t timestamp = av_rescale(target_sample,
                                   audio_stream->time_base.den,
                                   audio_stream->time_base.num * context->codec_ctx->sample_rate);
    
    // Seek in the file
    int ret = av_seek_frame(context->format_ctx, context->audio_stream_index,
                           timestamp, AVSEEK_FLAG_BACKWARD);
    
    if (ret >= 0) {
        // Flush codec buffers
        avcodec_flush_buffers(context->codec_ctx);
        
        // Reset buffer
        context->buffer_position = 0;
        context->samples_in_buffer = 0;
        context->current_sample = target_sample;
        context->end_of_stream = false;
    }
    
    mtx_unlock(&context->context_mutex);
}

static void ffmpeg_GetFileInfo(CP_HCODECMODULE hModule, CPs_FileInfo* pInfo) {
    FFmpegContext* context = (FFmpegContext*)hModule->m_pModuleCookie;
    
    if (!context || !pInfo) {
        return;
    }
    
    mtx_lock(&context->context_mutex);
    
    if (context->state == FFMPEG_STATE_UNINITIALIZED) {
        mtx_unlock(&context->context_mutex);
        return;
    }
    
    pInfo->m_iFreq_Hz = context->sample_rate;
    pInfo->m_b16bit = (context->bits_per_sample == 16);
    pInfo->m_bStereo = (context->channels == 2);
    
    // Calculate bitrate in Kbps
    if (context->codec_ctx->bit_rate > 0) {
        pInfo->m_iBitRate_Kbs = (UINT)(context->codec_ctx->bit_rate / 1000);
    } else {
        // Try to estimate from file size and duration
        int64_t file_size = avio_size(context->format_ctx->pb);
        if (file_size > 0 && context->total_samples > 0 && context->sample_rate > 0) {
            int duration_secs = (int)(context->total_samples / context->sample_rate);
            if (duration_secs > 0) {
                pInfo->m_iBitRate_Kbs = (UINT)((file_size * 8) / (duration_secs * 1000));
            } else {
                pInfo->m_iBitRate_Kbs = 0;
            }
        } else {
            pInfo->m_iBitRate_Kbs = 0;
        }
    }
    
    if (context->total_samples > 0 && context->sample_rate > 0) {
        pInfo->m_iFileLength_Secs = (int)(context->total_samples / context->sample_rate);
    } else {
        // If we don't know duration, report a very large value so playback continues until EOF
        // This is better than reporting 0 which causes immediate skip
        pInfo->m_iFileLength_Secs = 86400; // 24 hours - essentially unlimited
    }
    
    printf("FFmpeg GetFileInfo: total_samples=%llu, sample_rate=%u, duration_secs=%d\n",
           context->total_samples, context->sample_rate, pInfo->m_iFileLength_Secs);
    
    mtx_unlock(&context->context_mutex);
}

static BOOL ffmpeg_GetPCMBlock(CP_HCODECMODULE hModule, void* pBlock, DWORD* pdwBlockSize) {
    FFmpegContext* context = (FFmpegContext*)hModule->m_pModuleCookie;
    
    if (!context || !pBlock || !pdwBlockSize) {
        if (pdwBlockSize) *pdwBlockSize = 0;
        return FALSE;
    }
    
    mtx_lock(&context->context_mutex);
    
    if (context->state != FFMPEG_STATE_READY && context->state != FFMPEG_STATE_DECODING) {
        *pdwBlockSize = 0;
        mtx_unlock(&context->context_mutex);
        return FALSE;
    }
    
    context->state = FFMPEG_STATE_DECODING;
    
    DWORD bytes_to_copy = *pdwBlockSize;
    DWORD bytes_copied = 0;
    int16_t* output = (int16_t*)pBlock;
    
    while (bytes_copied < bytes_to_copy && !context->end_of_stream) {
        // If buffer has data, copy it
        if (context->samples_in_buffer > 0) {
            size_t samples_to_copy = (bytes_to_copy - bytes_copied) / (sizeof(int16_t) * context->channels);
            if (samples_to_copy > context->samples_in_buffer) {
                samples_to_copy = context->samples_in_buffer;
            }
            
            size_t bytes = samples_to_copy * context->channels * sizeof(int16_t);
            memcpy(output + (bytes_copied / sizeof(int16_t)),
                   context->pcm_buffer + context->buffer_position,
                   bytes);
            
            bytes_copied += (DWORD)bytes;
            context->buffer_position += samples_to_copy * context->channels;
            context->samples_in_buffer -= samples_to_copy;
            context->current_sample += samples_to_copy;
            
            if (context->samples_in_buffer == 0) {
                context->buffer_position = 0;
            }
            
            continue;
        }
        
        // Buffer is empty, decode more data
        int ret = av_read_frame(context->format_ctx, context->packet);
        
        if (ret == AVERROR_EOF) {
            context->end_of_stream = true;
            context->state = FFMPEG_STATE_END_OF_STREAM;
            break;
        } else if (ret < 0) {
            ffmpeg_log_error(context, ret, "Error reading frame");
            context->end_of_stream = true;
            context->state = FFMPEG_STATE_ERROR;
            break;
        }
        
        // Skip non-audio packets
        if (context->packet->stream_index != context->audio_stream_index) {
            av_packet_unref(context->packet);
            continue;
        }
        
        // Send packet to decoder
        ret = avcodec_send_packet(context->codec_ctx, context->packet);
        av_packet_unref(context->packet);
        
        if (ret < 0) {
            ffmpeg_log_error(context, ret, "Error sending packet");
            continue;
        }
        
        // Receive decoded frame
        ret = avcodec_receive_frame(context->codec_ctx, context->frame);
        
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            continue;
        } else if (ret < 0) {
            ffmpeg_log_error(context, ret, "Error receiving frame");
            continue;
        }
        
        // Resample if needed
        int out_samples;
        if (context->swr_ctx) {
            uint8_t* output_buffer = (uint8_t*)context->pcm_buffer;
            out_samples = swr_convert(context->swr_ctx,
                                     &output_buffer,
                                     FFMPEG_BUFFER_SIZE,
                                     (const uint8_t**)context->frame->data,
                                     context->frame->nb_samples);
            
            if (out_samples < 0) {
                av_frame_unref(context->frame);
                continue;
            }
        } else {
            // Direct copy (no resampling needed)
            out_samples = context->frame->nb_samples;
            memcpy(context->pcm_buffer,
                   context->frame->data[0],
                   out_samples * context->channels * sizeof(int16_t));
        }
        
        context->samples_in_buffer = out_samples;
        context->buffer_position = 0;
        
        av_frame_unref(context->frame);
    }
    
    *pdwBlockSize = bytes_copied;
    
    mtx_unlock(&context->context_mutex);
    
    return (bytes_copied > 0 || !context->end_of_stream);
}

static int ffmpeg_GetCurrentPos_secs(CP_HCODECMODULE hModule) {
    FFmpegContext* context = (FFmpegContext*)hModule->m_pModuleCookie;
    
    if (!context || context->state == FFMPEG_STATE_UNINITIALIZED) {
        return 0;
    }
    
    mtx_lock(&context->context_mutex);
    
    int pos_secs = 0;
    if (context->sample_rate > 0) {
        pos_secs = (int)(context->current_sample / context->sample_rate);
    }
    
    mtx_unlock(&context->context_mutex);
    
    return pos_secs;
}

////////////////////////////////////////////////////////////////////////////////
// Module Initialization

void CP_InitialiseCodec_FFmpeg(CPs_CoDecModule* codec) {
    if (!codec) return;
    
    // Allocate context
    FFmpegContext* context = CALLOC_TYPE(FFmpegContext, 1);
    if (!context) return;
    
    // Initialize mutex
    if (mtx_init(&context->context_mutex, mtx_plain) == thrd_success) {
        context->mutex_initialized = true;
    } else {
        free(context);
        return;
    }
    
    context->state = FFMPEG_STATE_UNINITIALIZED;
    
    // Set up codec module
    codec->m_pModuleCookie = context;
    
    codec->Uninitialise = ffmpeg_Uninitialise;
    codec->OpenFile = ffmpeg_OpenFile;
    codec->CloseFile = ffmpeg_CloseFile;
    codec->Seek = ffmpeg_Seek;
    codec->GetFileInfo = ffmpeg_GetFileInfo;
    codec->GetPCMBlock = ffmpeg_GetPCMBlock;
    codec->GetCurrentPos_secs = ffmpeg_GetCurrentPos_secs;
    
    // Initialize file associations
    CPFA_InitialiseFileAssociations(codec);
    
    // Add comprehensive audio format associations supported by FFmpeg
    
    // MPEG Audio Layer formats
    CPFA_AddFileAssociation(codec, "mp3", 0);   // MPEG-1/2 Layer III
    CPFA_AddFileAssociation(codec, "mp2", 0);   // MPEG-1/2 Layer II
    CPFA_AddFileAssociation(codec, "mp1", 0);   // MPEG-1/2 Layer I
    CPFA_AddFileAssociation(codec, "mpa", 0);   // MPEG Audio
    
    // AAC formats
    CPFA_AddFileAssociation(codec, "aac", 0);   // Advanced Audio Coding
    CPFA_AddFileAssociation(codec, "m4a", 0);   // MPEG-4 Audio
    CPFA_AddFileAssociation(codec, "m4b", 0);   // MPEG-4 Audiobook
    CPFA_AddFileAssociation(codec, "m4p", 0);   // MPEG-4 Protected
    CPFA_AddFileAssociation(codec, "m4r", 0);   // MPEG-4 Ringtone
    CPFA_AddFileAssociation(codec, "mp4", 0);   // MPEG-4 Container
    CPFA_AddFileAssociation(codec, "3gp", 0);   // 3GPP Mobile
    CPFA_AddFileAssociation(codec, "3g2", 0);   // 3GPP2 Mobile
    
    // Lossless formats
    CPFA_AddFileAssociation(codec, "flac", 0);  // Free Lossless Audio Codec
    CPFA_AddFileAssociation(codec, "fla", 0);   // FLAC alternate extension
    CPFA_AddFileAssociation(codec, "ape", 0);   // Monkey's Audio - disabled due to duration detection issues
    CPFA_AddFileAssociation(codec, "wv", 0);    // WavPack
    CPFA_AddFileAssociation(codec, "tta", 0);   // True Audio
    CPFA_AddFileAssociation(codec, "tak", 0);   // Tom's lossless Audio Kompressor
    CPFA_AddFileAssociation(codec, "alac", 0);  // Apple Lossless
    
    // Ogg container formats
    CPFA_AddFileAssociation(codec, "ogg", 0);   // Ogg Vorbis
    CPFA_AddFileAssociation(codec, "oga", 0);   // Ogg Audio
    CPFA_AddFileAssociation(codec, "ogx", 0);   // Ogg Multiplex
    CPFA_AddFileAssociation(codec, "ogm", 0);   // Ogg Media
    CPFA_AddFileAssociation(codec, "opus", 0);  // Opus Audio (in Ogg)
    CPFA_AddFileAssociation(codec, "spx", 0);   // Speex (in Ogg)
    
    // Windows Media formats
    CPFA_AddFileAssociation(codec, "wma", 0);   // Windows Media Audio
    CPFA_AddFileAssociation(codec, "asf", 0);   // Advanced Systems Format
    CPFA_AddFileAssociation(codec, "wmv", 0);   // Windows Media Video (audio stream)
    
    // Musepack
    CPFA_AddFileAssociation(codec, "mpc", 0);   // Musepack
    CPFA_AddFileAssociation(codec, "mp+", 0);   // Musepack old extension
    CPFA_AddFileAssociation(codec, "mpp", 0);   // Musepack old extension
    
    // Professional/surround formats
    CPFA_AddFileAssociation(codec, "ac3", 0);   // Dolby Digital
    CPFA_AddFileAssociation(codec, "eac3", 0);  // Dolby Digital Plus
    CPFA_AddFileAssociation(codec, "dts", 0);   // DTS Coherent Acoustics
    CPFA_AddFileAssociation(codec, "dtshd", 0); // DTS-HD
    CPFA_AddFileAssociation(codec, "mlp", 0);   // Meridian Lossless Packing
    CPFA_AddFileAssociation(codec, "thd", 0);   // Dolby TrueHD
    
    // Uncompressed/PCM formats
    CPFA_AddFileAssociation(codec, "wav", 0);   // Waveform Audio
    CPFA_AddFileAssociation(codec, "wave", 0);  // Waveform Audio alternate
    CPFA_AddFileAssociation(codec, "aiff", 0);  // Audio Interchange File Format
    CPFA_AddFileAssociation(codec, "aif", 0);   // AIFF short extension
    CPFA_AddFileAssociation(codec, "aifc", 0);  // AIFF Compressed
    CPFA_AddFileAssociation(codec, "au", 0);    // Sun Audio
    CPFA_AddFileAssociation(codec, "snd", 0);   // Sound file
    CPFA_AddFileAssociation(codec, "pcm", 0);   // Raw PCM
    CPFA_AddFileAssociation(codec, "raw", 0);   // Raw audio data
    
    // Adaptive bitrate formats
    CPFA_AddFileAssociation(codec, "ra", 0);    // RealAudio
    CPFA_AddFileAssociation(codec, "rm", 0);    // RealMedia
    CPFA_AddFileAssociation(codec, "ram", 0);   // RealAudio Metadata
    
    // Tracker/module formats
    CPFA_AddFileAssociation(codec, "mod", 0);   // ProTracker Module
    CPFA_AddFileAssociation(codec, "s3m", 0);   // ScreamTracker 3 Module
    CPFA_AddFileAssociation(codec, "xm", 0);    // FastTracker 2 Extended Module
    CPFA_AddFileAssociation(codec, "it", 0);    // Impulse Tracker
    
    // Other formats
    CPFA_AddFileAssociation(codec, "amr", 0);   // Adaptive Multi-Rate
    CPFA_AddFileAssociation(codec, "awb", 0);   // AMR Wideband
    CPFA_AddFileAssociation(codec, "voc", 0);   // Creative Voice
    CPFA_AddFileAssociation(codec, "vqf", 0);   // TwinVQ
    CPFA_AddFileAssociation(codec, "w64", 0);   // Sony Wave64
    CPFA_AddFileAssociation(codec, "caf", 0);   // Apple Core Audio Format
    CPFA_AddFileAssociation(codec, "dff", 0);   // DSD Storage Facility
    CPFA_AddFileAssociation(codec, "dsf", 0);   // DSD Stream File
    CPFA_AddFileAssociation(codec, "webm", 0);  // WebM Audio
    CPFA_AddFileAssociation(codec, "mka", 0);   // Matroska Audio
    //CPFA_AddFileAssociation(codec, "ofr", 0);   // OptimFROG
    //CPFA_AddFileAssociation(codec, "ofs", 0);   // OptimFROG DualStream
    CPFA_AddFileAssociation(codec, "spx", 0);   // Speex
    CPFA_AddFileAssociation(codec, "gsm", 0);   // GSM Audio
    CPFA_AddFileAssociation(codec, "iff", 0);   // Interchange File Format
    CPFA_AddFileAssociation(codec, "svx", 0);   // 8SVX/16SV Amiga
}
