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
// Opus Decoder using opusfile
//
////////////////////////////////////////////////////////////////////////////////

#include "CPI_Player_CoDec_C23.h"
#include "CPI_Stream.h"
#include <opus/opusfile.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// Opus decoder state

typedef enum {
    OPUS_DEC_STATE_UNINITIALIZED = 0,
    OPUS_DEC_STATE_READY,
    OPUS_DEC_STATE_END_OF_STREAM,
    OPUS_DEC_STATE_ERROR
} OpusDecoderState;

typedef struct OpusContext {
    OggOpusFile* opus_file;
    CPs_InStream* input_stream;

    // Audio format
    uint32_t channels;
    int64_t total_samples;  // pcm samples (at 48 kHz)
    uint64_t current_sample;

    OpusDecoderState state;
} OpusContext;

////////////////////////////////////////////////////////////////////////////////
// opusfile I/O callbacks (bridging CPs_InStream)

static int opus_read_cb(void* stream, unsigned char* ptr, int nbytes)
{
    OpusContext* ctx = (OpusContext*)stream;
    if (!ctx || !ctx->input_stream || nbytes <= 0) return -1;

    size_t bytes_read = 0;
    ctx->input_stream->Read(ctx->input_stream, ptr, (size_t)nbytes, &bytes_read);
    return (int)bytes_read;
}

static int opus_seek_cb(void* stream, opus_int64 offset, int whence)
{
    OpusContext* ctx = (OpusContext*)stream;
    if (!ctx || !ctx->input_stream) return -1;
    if (!ctx->input_stream->IsSeekable(ctx->input_stream)) return -1;

    uint64_t new_pos;
    switch (whence) {
        case SEEK_SET:
            new_pos = (uint64_t)offset;
            break;
        case SEEK_CUR:
            new_pos = ctx->input_stream->Tell(ctx->input_stream) + offset;
            break;
        case SEEK_END:
            new_pos = ctx->input_stream->GetLength(ctx->input_stream) + offset;
            break;
        default:
            return -1;
    }

    if (new_pos > ctx->input_stream->GetLength(ctx->input_stream))
        return -1;

    ctx->input_stream->Seek(ctx->input_stream, new_pos);
    return 0;
}

static opus_int64 opus_tell_cb(void* stream)
{
    OpusContext* ctx = (OpusContext*)stream;
    if (!ctx || !ctx->input_stream) return -1;
    return (opus_int64)ctx->input_stream->Tell(ctx->input_stream);
}

static int opus_close_cb(void* stream)
{
    (void)stream;
    return 0;  // we handle cleanup elsewhere
}

static const OpusFileCallbacks opus_callbacks = {
    .read  = opus_read_cb,
    .seek  = opus_seek_cb,
    .tell  = opus_tell_cb,
    .close = opus_close_cb
};

////////////////////////////////////////////////////////////////////////////////
// Module functions

static void CPP_OMOPUS_Uninitialise(CPs_CoDecModule* module)
{
    if (!module || !module->m_pModuleCookie) return;

    OpusContext* ctx = (OpusContext*)module->m_pModuleCookie;

    if (ctx->opus_file) {
        op_free(ctx->opus_file);
        ctx->opus_file = NULL;
    }

    if (ctx->input_stream) {
        ctx->input_stream->Uninitialise(ctx->input_stream);
        ctx->input_stream = NULL;
    }

    free(ctx);
    module->m_pModuleCookie = NULL;

    CPFA_EmptyFileAssociations(module);
}

static void CPP_OMOPUS_CloseFile(CPs_CoDecModule* module)
{
    if (!module || !module->m_pModuleCookie) return;

    OpusContext* ctx = (OpusContext*)module->m_pModuleCookie;

    if (ctx->opus_file) {
        op_free(ctx->opus_file);
        ctx->opus_file = NULL;
    }

    if (ctx->input_stream) {
        ctx->input_stream->Uninitialise(ctx->input_stream);
        ctx->input_stream = NULL;
    }

    ctx->state = OPUS_DEC_STATE_UNINITIALIZED;
}

static BOOL CPP_OMOPUS_OpenFile(CPs_CoDecModule* module,
                                const char* filename,
                                DWORD_PTR cookie,
                                HWND owner)
{
    (void)cookie; (void)owner;
    if (!module || !filename) return FALSE;

    OpusContext* ctx = (OpusContext*)module->m_pModuleCookie;
    if (!ctx) return FALSE;

    // If a previous file is still open, close it first
    if (ctx->opus_file) {
        CPP_OMOPUS_CloseFile(module);
    }

    // Open stream
    ctx->input_stream = CP_CreateInStream(filename, NULL);
    if (!ctx->input_stream) return FALSE;

    int error = 0;
    ctx->opus_file = op_open_callbacks(ctx, &opus_callbacks, NULL, 0, &error);
    if (!ctx->opus_file) {
        ctx->input_stream->Uninitialise(ctx->input_stream);
        ctx->input_stream = NULL;
        return FALSE;
    }

    // Opus always decodes to 48 kHz
    // We use op_read_stereo which always outputs 2 channels
    ctx->channels = 2;
    ctx->total_samples = op_pcm_total(ctx->opus_file, -1);
    ctx->current_sample = 0;
    ctx->state = OPUS_DEC_STATE_READY;

    return TRUE;
}

static BOOL CPP_OMOPUS_GetPCMBlock(CPs_CoDecModule* module, void* block, DWORD* block_size)
{
    if (!module || !block || !block_size || !module->m_pModuleCookie)
        return FALSE;

    OpusContext* ctx = (OpusContext*)module->m_pModuleCookie;
    if (!ctx->opus_file || ctx->state == OPUS_DEC_STATE_END_OF_STREAM)
        return FALSE;

    const DWORD requested = *block_size;
    DWORD filled = 0;

    // Temporary float buffer for one Opus frame (max 20ms stereo at 48kHz)
    float float_buf[960 * 2];

    // Read float PCM from opusfile and convert to int16 ourselves.
    // We use op_read_float_stereo instead of op_read_stereo because
    // opusfile's internal float-to-int16 conversion produces near-zero
    // output on some MinGW/GCC toolchains (confirmed with GCC 16).
    while (filled < requested) {
        // How many int16 values fit in remaining space
        int remaining_int16 = (int)((requested - filled) / sizeof(opus_int16));
        if (remaining_int16 < 2) break;

        // Limit read to our float buffer size
        int read_count = remaining_int16;
        if (read_count > 960 * 2) read_count = 960 * 2;

        int ret = op_read_float_stereo(ctx->opus_file, float_buf, read_count);

        if (ret == 0) {
            ctx->state = OPUS_DEC_STATE_END_OF_STREAM;
            break;
        } else if (ret < 0) {
            if (ret == OP_HOLE) continue;
            ctx->state = OPUS_DEC_STATE_ERROR;
            break;
        } else {
            // ret = samples per channel, 2 channels
            int total_values = ret * 2;
            opus_int16* dst = (opus_int16*)((char*)block + filled);

            // Convert float [-1.0, 1.0] to int16 [-32768, 32767]
            for (int i = 0; i < total_values; i++) {
                int v = (int)(float_buf[i] * 32768.0f);
                if (v > 32767) v = 32767;
                if (v < -32768) v = -32768;
                dst[i] = (opus_int16)v;
            }

            DWORD bytes = (DWORD)(total_values * sizeof(opus_int16));
            filled += bytes;
            ctx->current_sample += (uint64_t)ret;
        }
    }

    *block_size = filled;
    return (filled > 0) ? TRUE : FALSE;
}

static void CPP_OMOPUS_Seek(CPs_CoDecModule* module, int numerator, int denominator)
{
    if (!module || !module->m_pModuleCookie || denominator == 0) return;

    OpusContext* ctx = (OpusContext*)module->m_pModuleCookie;
    if (!ctx->opus_file || ctx->total_samples <= 0) return;

    double ratio = (double)numerator / (double)denominator;
    ogg_int64_t target = (ogg_int64_t)(ratio * (double)ctx->total_samples);

    if (op_pcm_seek(ctx->opus_file, target) == 0) {
        ctx->current_sample = (uint64_t)target;
        ctx->state = OPUS_DEC_STATE_READY;
    }
}

static void CPP_OMOPUS_GetFileInfo(CPs_CoDecModule* module, CPs_FileInfo* info)
{
    if (!module || !info || !module->m_pModuleCookie) return;

    OpusContext* ctx = (OpusContext*)module->m_pModuleCookie;

    // Opus always decodes at 48 kHz
    info->m_iFreq_Hz = 48000;
    info->m_bStereo = TRUE;  // op_read_stereo always outputs stereo
    info->m_b16bit = TRUE;

    if (ctx->total_samples > 0) {
        info->m_iFileLength_Secs = (UINT)(ctx->total_samples / 48000);
    } else {
        info->m_iFileLength_Secs = 0;
    }

    // Bitrate from opusfile (instantaneous average, bits/sec)
    if (ctx->opus_file) {
        opus_int32 br = op_bitrate(ctx->opus_file, -1);
        info->m_iBitRate_Kbs = (br > 0) ? (UINT)(br / 1000) : 0;
    } else {
        info->m_iBitRate_Kbs = 0;
    }

}

static int CPP_OMOPUS_GetCurrentPos_secs(CPs_CoDecModule* module)
{
    if (!module || !module->m_pModuleCookie) return 0;

    OpusContext* ctx = (OpusContext*)module->m_pModuleCookie;
    return (int)(ctx->current_sample / 48000);
}

////////////////////////////////////////////////////////////////////////////////
// Module Registration

void CP_InitialiseCodec_OPUS(CPs_CoDecModule* codec)
{
    if (!codec) return;

    // Allocate persistent context
    OpusContext* ctx = (OpusContext*)calloc(1, sizeof(OpusContext));
    if (!ctx) return;

    codec->Uninitialise       = CPP_OMOPUS_Uninitialise;
    codec->OpenFile           = CPP_OMOPUS_OpenFile;
    codec->CloseFile          = CPP_OMOPUS_CloseFile;
    codec->Seek               = CPP_OMOPUS_Seek;
    codec->GetFileInfo        = CPP_OMOPUS_GetFileInfo;
    codec->GetPCMBlock        = CPP_OMOPUS_GetPCMBlock;
    codec->GetCurrentPos_secs = CPP_OMOPUS_GetCurrentPos_secs;

    codec->m_pModuleCookie = ctx;

    CPFA_InitialiseFileAssociations(codec);
    CPFA_AddFileAssociation(codec, "opus", 0L);
}
