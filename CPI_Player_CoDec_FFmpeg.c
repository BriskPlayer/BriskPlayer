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
// Optional FFmpeg decoder, loaded at runtime
//
// BriskPlayer neither links against FFmpeg nor needs its headers to build.
// At engine startup this module looks for a set of FFmpeg shared libraries
// in an "ffmpeg" folder next to the executable:
//
//     briskplayer.exe
//     ffmpeg\avutil-NN.dll
//     ffmpeg\swresample-NN.dll
//     ffmpeg\avcodec-NN.dll
//     ffmpeg\avformat-NN.dll
//     ffmpeg\<any further DLLs those depend on>
//
// ffmpeg\bin\ and ffmpeg\<subfolder>\bin\ are searched too, so an FFmpeg
// release archive can be extracted into the folder as-is.
//
// If a supported set is found, every entry point is resolved through
// GetProcAddress and the codec registers its file associations. If not, the
// module stays zeroed, exactly as if it had never been compiled in, and the
// engine skips it.
//
// ABI: FFmpeg only guarantees struct layouts within a major version, so this
// file must not depend on anything that moves between majors. Values on
// AVCodecContext are read by name through AVOptions ("ar", "b", "ch_layout").
// The handful of fields read directly (FF_* prefix structs below) were checked
// to be byte-identical in FFmpeg 7.x, 8.x and 9.x; any other major version is
// rejected at load time. Before adding a row to ffmpeg_known_sets[], re-check
// those prefixes against that release's headers.
//
////////////////////////////////////////////////////////////////////////////////

#include "CPI_Player_CoDec_C23.h"
#include "CPI_Stream.h"
#include "debug.h"
#include <errno.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// Minimal FFmpeg declarations (see ABI note above)

typedef struct { int num, den; } FF_Rational;

typedef struct {
    int order;          // enum AVChannelOrder
    int nb_channels;
    union { uint64_t mask; void* map; } u;
    void* opaque;
} FF_ChannelLayout;

typedef struct {        // AVIOContext prefix
    const void* av_class;
    unsigned char* buffer;
} FF_IOContext;

typedef struct {        // AVStream prefix
    const void* av_class;
    int index;
    int id;
    void* codecpar;
    void* priv_data;
    FF_Rational time_base;
    int64_t start_time;
    int64_t duration;
} FF_Stream;

typedef struct {        // AVFormatContext prefix
    const void* av_class;
    const void* iformat;
    const void* oformat;
    void* priv_data;
    FF_IOContext* pb;
    int ctx_flags;
    unsigned int nb_streams;
    FF_Stream** streams;
    unsigned int nb_stream_groups;
    void** stream_groups;
    unsigned int nb_chapters;
    void** chapters;
    char* url;
    int64_t start_time;
    int64_t duration;
} FF_FormatContext;

typedef struct {        // AVFrame prefix
    uint8_t* data[8];
    int linesize[8];
    uint8_t** extended_data;
    int width, height;
    int nb_samples;
    int format;
} FF_Frame;

typedef struct {        // AVPacket prefix
    void* buf;
    int64_t pts;
    int64_t dts;
    uint8_t* data;
    int size;
    int stream_index;
} FF_Packet;

typedef struct FF_CodecContext FF_CodecContext;   // opaque, accessed via AVOptions
typedef struct FF_Codec FF_Codec;                 // opaque
typedef struct FF_SwrContext FF_SwrContext;       // opaque

#define FF_MKTAG(a, b, c, d)    ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))
#define FF_AVERROR_EOF          (-(int)FF_MKTAG('E', 'O', 'F', ' '))
#define FF_AVERROR_EAGAIN       (-EAGAIN)
#define FF_NOPTS_VALUE          INT64_MIN
#define FF_TIME_BASE            1000000
#define FF_MEDIA_TYPE_AUDIO     1
#define FF_SAMPLE_FMT_S16       1
#define FF_SEEK_FLAG_BACKWARD   1
#define FF_AVSEEK_SIZE          0x10000
#define FF_AVSEEK_FORCE         0x20000
#define FF_LOG_QUIET            (-8)

////////////////////////////////////////////////////////////////////////////////
// Runtime-resolved entry points

typedef struct FFmpegAPI {
    HMODULE hAvutil, hSwresample, hAvcodec, hAvformat;

    // avutil
    unsigned (*avutil_version)(void);
    void* (*av_malloc)(size_t size);
    void (*av_freep)(void* ptr);
    int (*av_strerror)(int errnum, char* errbuf, size_t errbuf_size);
    void (*av_log_set_level)(int level);
    FF_Frame* (*av_frame_alloc)(void);
    void (*av_frame_free)(FF_Frame** frame);
    void (*av_frame_unref)(FF_Frame* frame);
    int (*av_opt_get_int)(void* obj, const char* name, int search_flags, int64_t* out_val);
    int (*av_opt_get_chlayout)(void* obj, const char* name, int search_flags, FF_ChannelLayout* layout);
    void (*av_channel_layout_default)(FF_ChannelLayout* ch_layout, int nb_channels);
    void (*av_channel_layout_uninit)(FF_ChannelLayout* channel_layout);

    // swresample
    unsigned (*swresample_version)(void);
    int (*swr_alloc_set_opts2)(FF_SwrContext** ps,
                               const FF_ChannelLayout* out_ch_layout, int out_sample_fmt, int out_sample_rate,
                               const FF_ChannelLayout* in_ch_layout, int in_sample_fmt, int in_sample_rate,
                               int log_offset, void* log_ctx);
    int (*swr_init)(FF_SwrContext* s);
    void (*swr_free)(FF_SwrContext** s);
    int (*swr_convert)(FF_SwrContext* s, uint8_t* const* out, int out_count,
                       const uint8_t* const* in, int in_count);
    int (*swr_get_out_samples)(FF_SwrContext* s, int in_samples);

    // avcodec
    unsigned (*avcodec_version)(void);
    FF_CodecContext* (*avcodec_alloc_context3)(const FF_Codec* codec);
    void (*avcodec_free_context)(FF_CodecContext** avctx);
    int (*avcodec_parameters_to_context)(FF_CodecContext* codec, const void* par);
    int (*avcodec_open2)(FF_CodecContext* avctx, const FF_Codec* codec, void** options);
    int (*avcodec_send_packet)(FF_CodecContext* avctx, const FF_Packet* avpkt);
    int (*avcodec_receive_frame)(FF_CodecContext* avctx, FF_Frame* frame);
    void (*avcodec_flush_buffers)(FF_CodecContext* avctx);
    FF_Packet* (*av_packet_alloc)(void);
    void (*av_packet_free)(FF_Packet** pkt);
    void (*av_packet_unref)(FF_Packet* pkt);

    // avformat
    unsigned (*avformat_version)(void);
    FF_FormatContext* (*avformat_alloc_context)(void);
    int (*avformat_open_input)(FF_FormatContext** ps, const char* url, const void* fmt, void** options);
    int (*avformat_find_stream_info)(FF_FormatContext* ic, void** options);
    int (*av_find_best_stream)(FF_FormatContext* ic, int type, int wanted_stream_nb,
                               int related_stream, const FF_Codec** decoder_ret, int flags);
    int (*av_read_frame)(FF_FormatContext* s, FF_Packet* pkt);
    int (*av_seek_frame)(FF_FormatContext* s, int stream_index, int64_t timestamp, int flags);
    void (*avformat_close_input)(FF_FormatContext** s);
    FF_IOContext* (*avio_alloc_context)(unsigned char* buffer, int buffer_size, int write_flag, void* opaque,
                                        int (*read_packet)(void* opaque, uint8_t* buf, int buf_size),
                                        int (*write_packet)(void* opaque, const uint8_t* buf, int buf_size),
                                        int64_t (*seek)(void* opaque, int64_t offset, int whence));
    void (*avio_context_free)(FF_IOContext** s);
} FFmpegAPI;

// One FFmpeg release's library set. The library majors move in lockstep per
// release, so the DLL names and the runtime version checks both come from here.
typedef struct {
    const char* release;
    unsigned avutil, swresample, avcodec, avformat;
} FFmpegLibSet;

static const FFmpegLibSet ffmpeg_known_sets[] = {   // newest first
    { "9.x", 61, 7, 63, 63 },
    { "8.x", 60, 6, 62, 62 },
    { "7.x", 59, 5, 61, 61 },
};

#ifndef LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR
#define LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR 0x00000100
#endif
#ifndef LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
#define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS 0x00001000
#endif

// Loads <dir>\<name>-<major>.dll. Dependencies resolve from the same folder
// first, so a self-contained FFmpeg build (or one with extra runtime DLLs
// such as libwinpthread) works without touching PATH.
static HMODULE ffmpeg_load_dll(const WCHAR* dir, const WCHAR* name, unsigned major)
{
    WCHAR path[MAX_PATH];
    if (_snwprintf(path, MAX_PATH, L"%ls\\%ls-%u.dll", dir, name, major) < 0)
        return NULL;
    path[MAX_PATH - 1] = L'\0';

    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
        return NULL;

    HMODULE h = LoadLibraryExW(path, NULL,
                               LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    // Windows 7 without KB2533623 rejects the LOAD_LIBRARY_SEARCH_* flags.
    if (!h && GetLastError() == ERROR_INVALID_PARAMETER)
        h = LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!h)
        CP_LOG_WARNING("FFmpeg: found %ls but could not load it (error %lu)\n", path, GetLastError());
    return h;
}

static void ffmpeg_unload(FFmpegAPI* api)
{
    if (api->hAvformat)   FreeLibrary(api->hAvformat);
    if (api->hAvcodec)    FreeLibrary(api->hAvcodec);
    if (api->hSwresample) FreeLibrary(api->hSwresample);
    if (api->hAvutil)     FreeLibrary(api->hAvutil);
    memset(api, 0, sizeof(*api));
}

// memcpy rather than a cast: converting FARPROC to an unrelated function
// pointer type draws -Wcast-function-type on GCC/Clang.
#define FF_SYM(lib, name) \
    do { \
        FARPROC proc_ = GetProcAddress(api->lib, #name); \
        if (!proc_) { missing = #name; goto fail; } \
        static_assert(sizeof(api->name) == sizeof(proc_), "function pointer size"); \
        memcpy(&api->name, &proc_, sizeof(proc_)); \
    } while (0)

// Tries every known release's DLL set in one directory.
static const FFmpegLibSet* ffmpeg_load_from_dir(FFmpegAPI* api, const WCHAR* dir)
{
    for (size_t i = 0; i < sizeof(ffmpeg_known_sets) / sizeof(ffmpeg_known_sets[0]); i++)
    {
        const FFmpegLibSet* s = &ffmpeg_known_sets[i];
        // Load in dependency order so each library finds the previous ones.
        api->hAvutil     = ffmpeg_load_dll(dir, L"avutil", s->avutil);
        api->hSwresample = api->hAvutil     ? ffmpeg_load_dll(dir, L"swresample", s->swresample) : NULL;
        api->hAvcodec    = api->hSwresample ? ffmpeg_load_dll(dir, L"avcodec", s->avcodec)       : NULL;
        api->hAvformat   = api->hAvcodec    ? ffmpeg_load_dll(dir, L"avformat", s->avformat)     : NULL;
        if (api->hAvformat)
        {
            CP_LOG_INFO("FFmpeg: using DLLs from %ls\n", dir);
            return s;
        }
        ffmpeg_unload(api);
    }
    return NULL;
}

// Looks for the DLLs in ffmpeg\, ffmpeg\bin\, and ffmpeg\<subfolder>\bin\ or
// ffmpeg\<subfolder>\ - release archives (e.g. gyan.dev, BtbN) unpack as
// <name>\bin\*.dll, so extracting one into the ffmpeg folder just works.
static const FFmpegLibSet* ffmpeg_find_and_load(FFmpegAPI* api, const WCHAR* base)
{
    WCHAR dir[MAX_PATH];
    const FFmpegLibSet* set = ffmpeg_load_from_dir(api, base);
    if (set)
        return set;

    if (_snwprintf(dir, MAX_PATH, L"%ls\\bin", base) > 0 && (set = ffmpeg_load_from_dir(api, dir)) != NULL)
        return set;

    WCHAR pattern[MAX_PATH];
    if (_snwprintf(pattern, MAX_PATH, L"%ls\\*", base) < 0)
        return NULL;
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return NULL;
    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0 ||
            _wcsicmp(fd.cFileName, L"bin") == 0)
            continue;
        if (_snwprintf(dir, MAX_PATH, L"%ls\\%ls\\bin", base, fd.cFileName) > 0 &&
            (set = ffmpeg_load_from_dir(api, dir)) != NULL)
            break;
        if (_snwprintf(dir, MAX_PATH, L"%ls\\%ls", base, fd.cFileName) > 0 &&
            (set = ffmpeg_load_from_dir(api, dir)) != NULL)
            break;
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return set;
}

static BOOL ffmpeg_load(FFmpegAPI* api)
{
    memset(api, 0, sizeof(*api));

    WCHAR dir[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, dir, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
        return FALSE;
    WCHAR* slash = wcsrchr(dir, L'\\');
    if (!slash)
        return FALSE;
    *slash = L'\0';
    if (wcslen(dir) + 8 >= MAX_PATH)
        return FALSE;
    wcscat(dir, L"\\ffmpeg");

    if (GetFileAttributesW(dir) == INVALID_FILE_ATTRIBUTES)
    {
        CP_LOG_VERBOSE("FFmpeg: no ffmpeg folder next to the executable, codec disabled\n");
        return FALSE;
    }

    const FFmpegLibSet* set = ffmpeg_find_and_load(api, dir);
    if (!set)
    {
        CP_LOG_INFO("FFmpeg: ffmpeg folder present but no supported DLL set (FFmpeg 7.x-9.x) found\n");
        return FALSE;
    }

    const char* missing = NULL;
    FF_SYM(hAvutil, avutil_version);
    FF_SYM(hAvutil, av_malloc);
    FF_SYM(hAvutil, av_freep);
    FF_SYM(hAvutil, av_strerror);
    FF_SYM(hAvutil, av_log_set_level);
    FF_SYM(hAvutil, av_frame_alloc);
    FF_SYM(hAvutil, av_frame_free);
    FF_SYM(hAvutil, av_frame_unref);
    FF_SYM(hAvutil, av_opt_get_int);
    FF_SYM(hAvutil, av_opt_get_chlayout);
    FF_SYM(hAvutil, av_channel_layout_default);
    FF_SYM(hAvutil, av_channel_layout_uninit);
    FF_SYM(hSwresample, swresample_version);
    FF_SYM(hSwresample, swr_alloc_set_opts2);
    FF_SYM(hSwresample, swr_init);
    FF_SYM(hSwresample, swr_free);
    FF_SYM(hSwresample, swr_convert);
    FF_SYM(hSwresample, swr_get_out_samples);
    FF_SYM(hAvcodec, avcodec_version);
    FF_SYM(hAvcodec, avcodec_alloc_context3);
    FF_SYM(hAvcodec, avcodec_free_context);
    FF_SYM(hAvcodec, avcodec_parameters_to_context);
    FF_SYM(hAvcodec, avcodec_open2);
    FF_SYM(hAvcodec, avcodec_send_packet);
    FF_SYM(hAvcodec, avcodec_receive_frame);
    FF_SYM(hAvcodec, avcodec_flush_buffers);
    FF_SYM(hAvcodec, av_packet_alloc);
    FF_SYM(hAvcodec, av_packet_free);
    FF_SYM(hAvcodec, av_packet_unref);
    FF_SYM(hAvformat, avformat_version);
    FF_SYM(hAvformat, avformat_alloc_context);
    FF_SYM(hAvformat, avformat_open_input);
    FF_SYM(hAvformat, avformat_find_stream_info);
    FF_SYM(hAvformat, av_find_best_stream);
    FF_SYM(hAvformat, av_read_frame);
    FF_SYM(hAvformat, av_seek_frame);
    FF_SYM(hAvformat, avformat_close_input);
    FF_SYM(hAvformat, avio_alloc_context);
    FF_SYM(hAvformat, avio_context_free);

    // The file names matched; make sure the binaries agree (a renamed DLL
    // from another release would otherwise be trusted with the wrong ABI).
    if ((api->avutil_version() >> 16)     != set->avutil ||
        (api->swresample_version() >> 16) != set->swresample ||
        (api->avcodec_version() >> 16)    != set->avcodec ||
        (api->avformat_version() >> 16)   != set->avformat)
    {
        CP_LOG_WARNING("FFmpeg: DLL file names say %s but the library versions disagree, codec disabled\n",
                       set->release);
        ffmpeg_unload(api);
        return FALSE;
    }

    api->av_log_set_level(FF_LOG_QUIET);
    CP_LOG_INFO("FFmpeg: loaded FFmpeg %s runtime (avformat %u.%u)\n", set->release,
                api->avformat_version() >> 16, (api->avformat_version() >> 8) & 0xFF);
    return TRUE;

fail:
    CP_LOG_WARNING("FFmpeg: %s not exported by the FFmpeg %s DLLs, codec disabled\n", missing, set->release);
    ffmpeg_unload(api);
    return FALSE;
}

#undef FF_SYM

////////////////////////////////////////////////////////////////////////////////
// Decoder state

#define FFMPEG_IO_BUFFER_SIZE 65536
#define FFMPEG_OUT_CHANNELS   2

typedef struct FFmpegContext {
    FFmpegAPI api;

    CPs_InStream* stream;
    FF_FormatContext* format_ctx;
    FF_CodecContext* codec_ctx;
    FF_SwrContext* swr_ctx;
    FF_Frame* frame;
    FF_Packet* packet;
    int audio_stream_index;
    bool draining;
    bool end_of_stream;

    // Resampler input config, so a mid-stream format change rebuilds it
    int swr_in_format;
    int swr_in_rate;
    int swr_in_channels;

    // Output is always 16-bit stereo at the source's opening sample rate
    int out_rate;
    int64_t duration_us;    // 0 when unknown
    int64_t bit_rate;
    uint64_t current_sample;

    // Demuxers seek to the nearest seek point at or before the target, which
    // for some formats (TTA, WMA) is up to a second early. After a seek the
    // first packet's pts says where decoding really resumed; output up to
    // seek_target_sample is then dropped.
    FF_Stream* audio_stream;
    bool seek_pts_pending;
    uint64_t seek_target_sample;

    // Converted PCM not yet handed to the engine
    int16_t* pcm;
    int pcm_capacity;       // in sample frames
    int pcm_frames;
    int pcm_pos;
} FFmpegContext;

////////////////////////////////////////////////////////////////////////////////
// AVIO callbacks (bridging CPs_InStream)

static int ffmpeg_io_read(void* opaque, uint8_t* buf, int buf_size)
{
    FFmpegContext* ctx = (FFmpegContext*)opaque;
    size_t bytes_read = 0;
    if (buf_size <= 0)
        return 0;
    ctx->stream->Read(ctx->stream, buf, (size_t)buf_size, &bytes_read);
    return bytes_read > 0 ? (int)bytes_read : FF_AVERROR_EOF;
}

static int64_t ffmpeg_io_seek(void* opaque, int64_t offset, int whence)
{
    FFmpegContext* ctx = (FFmpegContext*)opaque;
    if (whence & FF_AVSEEK_SIZE)
        return (int64_t)ctx->stream->GetLength(ctx->stream);
    if (CP_StreamSeek(ctx->stream, (long long)offset, whence & ~FF_AVSEEK_FORCE) != 0)
        return -1;
    return (int64_t)CP_StreamTell(ctx->stream);
}

////////////////////////////////////////////////////////////////////////////////
// Helpers

static void ffmpeg_log_error(FFmpegContext* ctx, int err, const char* what)
{
    char buf[128];
    if (ctx->api.av_strerror(err, buf, sizeof(buf)) < 0)
        snprintf(buf, sizeof(buf), "error %d", err);
    CP_LOG_WARNING("FFmpeg: %s: %s\n", what, buf);
}

static void ffmpeg_CloseFile(CPs_CoDecModule* module)
{
    if (!module || !module->m_pModuleCookie) return;
    FFmpegContext* ctx = (FFmpegContext*)module->m_pModuleCookie;
    FFmpegAPI* api = &ctx->api;

    if (ctx->frame)     api->av_frame_free(&ctx->frame);
    if (ctx->packet)    api->av_packet_free(&ctx->packet);
    if (ctx->swr_ctx)   api->swr_free(&ctx->swr_ctx);
    if (ctx->codec_ctx) api->avcodec_free_context(&ctx->codec_ctx);

    if (ctx->format_ctx)
    {
        // A caller-supplied pb is not freed by avformat_close_input; its
        // buffer may have been reallocated by FFmpeg, so free it via pb.
        FF_IOContext* pb = ctx->format_ctx->pb;
        api->avformat_close_input(&ctx->format_ctx);
        if (pb)
        {
            api->av_freep(&pb->buffer);
            api->avio_context_free(&pb);
        }
    }

    if (ctx->stream)
    {
        ctx->stream->Uninitialise(ctx->stream);
        ctx->stream = NULL;
    }

    free(ctx->pcm);

    // Keep the loaded API, reset everything else
    FFmpegAPI saved = ctx->api;
    memset(ctx, 0, sizeof(*ctx));
    ctx->api = saved;
}

static void ffmpeg_Uninitialise(CPs_CoDecModule* module)
{
    if (!module || !module->m_pModuleCookie) return;

    ffmpeg_CloseFile(module);

    FFmpegContext* ctx = (FFmpegContext*)module->m_pModuleCookie;
    ffmpeg_unload(&ctx->api);
    free(ctx);
    module->m_pModuleCookie = NULL;

    CPFA_EmptyFileAssociations(module);
}

// Returns 1 with a frame in ctx->frame, 0 at end of stream.
static int ffmpeg_decode_frame(FFmpegContext* ctx)
{
    FFmpegAPI* api = &ctx->api;

    for (;;)
    {
        int ret = api->avcodec_receive_frame(ctx->codec_ctx, ctx->frame);
        if (ret == 0)
            return 1;
        if (ret == FF_AVERROR_EOF)
            return 0;
        if (ret != FF_AVERROR_EAGAIN)
        {
            ffmpeg_log_error(ctx, ret, "decode failed");
            return 0;
        }

        // Decoder wants input
        if (ctx->draining)
            return 0;

        ret = api->av_read_frame(ctx->format_ctx, ctx->packet);
        if (ret < 0)
        {
            if (ret != FF_AVERROR_EOF)
                ffmpeg_log_error(ctx, ret, "read failed");
            // Flush whatever the decoder still holds
            api->avcodec_send_packet(ctx->codec_ctx, NULL);
            ctx->draining = true;
            continue;
        }

        if (ctx->packet->stream_index == ctx->audio_stream_index)
        {
            if (ctx->seek_pts_pending && ctx->packet->pts != FF_NOPTS_VALUE)
            {
                const FF_Stream* st = ctx->audio_stream;
                int64_t pts = ctx->packet->pts;
                if (st->start_time != FF_NOPTS_VALUE)
                    pts -= st->start_time;
                if (pts < 0)
                    pts = 0;
                ctx->current_sample = (uint64_t)((double)pts * st->time_base.num * ctx->out_rate / st->time_base.den);
                ctx->seek_pts_pending = false;
            }
            ret = api->avcodec_send_packet(ctx->codec_ctx, ctx->packet);
            // A corrupt packet is skipped rather than ending playback
            if (ret < 0 && ret != FF_AVERROR_EAGAIN)
                ffmpeg_log_error(ctx, ret, "bad packet skipped");
        }
        api->av_packet_unref(ctx->packet);
    }
}

// Converts ctx->frame into ctx->pcm. Returns FALSE on a fatal error.
static BOOL ffmpeg_convert_frame(FFmpegContext* ctx)
{
    FFmpegAPI* api = &ctx->api;
    FF_Frame* frame = ctx->frame;

    // The decoder keeps the codec context in sync with the frame it just
    // returned, so it's the ABI-safe place to read the input layout from.
    int64_t in_rate = 0;
    FF_ChannelLayout in_layout;
    memset(&in_layout, 0, sizeof(in_layout));
    api->av_opt_get_int(ctx->codec_ctx, "ar", 0, &in_rate);
    if (api->av_opt_get_chlayout(ctx->codec_ctx, "ch_layout", 0, &in_layout) < 0 || in_rate <= 0)
        return FALSE;

    if (!ctx->swr_ctx ||
        ctx->swr_in_format != frame->format ||
        ctx->swr_in_rate != (int)in_rate ||
        ctx->swr_in_channels != in_layout.nb_channels)
    {
        FF_ChannelLayout out_layout;
        memset(&out_layout, 0, sizeof(out_layout));
        api->av_channel_layout_default(&out_layout, FFMPEG_OUT_CHANNELS);

        if (ctx->swr_ctx)
            api->swr_free(&ctx->swr_ctx);
        int ret = api->swr_alloc_set_opts2(&ctx->swr_ctx,
                                           &out_layout, FF_SAMPLE_FMT_S16, ctx->out_rate,
                                           &in_layout, frame->format, (int)in_rate,
                                           0, NULL);
        if (ret >= 0)
            ret = api->swr_init(ctx->swr_ctx);
        api->av_channel_layout_uninit(&out_layout);
        if (ret < 0)
        {
            ffmpeg_log_error(ctx, ret, "resampler setup failed");
            api->av_channel_layout_uninit(&in_layout);
            if (ctx->swr_ctx)
                api->swr_free(&ctx->swr_ctx);
            return FALSE;
        }
        ctx->swr_in_format = frame->format;
        ctx->swr_in_rate = (int)in_rate;
        ctx->swr_in_channels = in_layout.nb_channels;
    }
    api->av_channel_layout_uninit(&in_layout);

    int needed = api->swr_get_out_samples(ctx->swr_ctx, frame->nb_samples);
    if (needed < 0)
        return FALSE;
    if (needed > ctx->pcm_capacity)
    {
        int16_t* grown = (int16_t*)realloc(ctx->pcm, (size_t)needed * FFMPEG_OUT_CHANNELS * sizeof(int16_t));
        if (!grown)
            return FALSE;
        ctx->pcm = grown;
        ctx->pcm_capacity = needed;
    }

    uint8_t* out = (uint8_t*)ctx->pcm;
    int got = api->swr_convert(ctx->swr_ctx, &out, ctx->pcm_capacity,
                               (const uint8_t* const*)frame->extended_data, frame->nb_samples);
    if (got < 0)
    {
        ffmpeg_log_error(ctx, got, "resample failed");
        got = 0;
    }
    ctx->pcm_frames = got;
    ctx->pcm_pos = 0;
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Module functions

static BOOL ffmpeg_OpenFile(CPs_CoDecModule* module, const char* filename, DWORD_PTR cookie, HWND owner)
{
    (void)cookie; (void)owner;
    if (!module || !filename || !module->m_pModuleCookie) return FALSE;

    FFmpegContext* ctx = (FFmpegContext*)module->m_pModuleCookie;
    FFmpegAPI* api = &ctx->api;
    unsigned char* io_buffer = NULL;
    FF_IOContext* pb = NULL;
    int ret;

    ffmpeg_CloseFile(module);

    // Go through the player's own stream layer (Unicode paths, internet
    // streams) rather than letting FFmpeg open the name itself.
    ctx->stream = CP_CreateInStream(filename, NULL);
    if (!ctx->stream)
        return FALSE;

    io_buffer = (unsigned char*)api->av_malloc(FFMPEG_IO_BUFFER_SIZE);
    if (!io_buffer)
        goto fail;
    pb = api->avio_alloc_context(io_buffer, FFMPEG_IO_BUFFER_SIZE, 0, ctx, ffmpeg_io_read, NULL,
                                 ctx->stream->IsSeekable(ctx->stream) ? ffmpeg_io_seek : NULL);
    if (!pb)
        goto fail;
    io_buffer = NULL;   // owned by pb now

    ctx->format_ctx = api->avformat_alloc_context();
    if (!ctx->format_ctx)
        goto fail;
    ctx->format_ctx->pb = pb;
    pb = NULL;          // freed through format_ctx->pb from here on

    // The name is only a probing hint (extension) since I/O is ours.
    // On failure avformat_open_input frees format_ctx but not the custom pb.
    {
        FF_IOContext* custom_pb = ctx->format_ctx->pb;
        ret = api->avformat_open_input(&ctx->format_ctx, filename, NULL, NULL);
        if (ret < 0)
        {
            ffmpeg_log_error(ctx, ret, "could not open input");
            api->av_freep(&custom_pb->buffer);
            api->avio_context_free(&custom_pb);
            goto fail;
        }
    }

    ret = api->avformat_find_stream_info(ctx->format_ctx, NULL);
    if (ret < 0)
    {
        ffmpeg_log_error(ctx, ret, "could not read stream info");
        goto fail;
    }

    const FF_Codec* codec = NULL;
    ctx->audio_stream_index = api->av_find_best_stream(ctx->format_ctx, FF_MEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (ctx->audio_stream_index < 0 || !codec ||
        (unsigned)ctx->audio_stream_index >= ctx->format_ctx->nb_streams)
    {
        CP_LOG_VERBOSE("FFmpeg: no decodable audio stream in %s\n", filename);
        goto fail;
    }
    FF_Stream* st = ctx->format_ctx->streams[ctx->audio_stream_index];
    ctx->audio_stream = st;

    ctx->codec_ctx = api->avcodec_alloc_context3(codec);
    if (!ctx->codec_ctx)
        goto fail;
    ret = api->avcodec_parameters_to_context(ctx->codec_ctx, st->codecpar);
    if (ret >= 0)
        ret = api->avcodec_open2(ctx->codec_ctx, codec, NULL);
    if (ret < 0)
    {
        ffmpeg_log_error(ctx, ret, "could not open decoder");
        goto fail;
    }

    int64_t rate = 0;
    api->av_opt_get_int(ctx->codec_ctx, "ar", 0, &rate);
    if (rate <= 0 || rate > 768000)
    {
        CP_LOG_WARNING("FFmpeg: unusable sample rate %lld in %s\n", (long long)rate, filename);
        goto fail;
    }
    ctx->out_rate = (int)rate;
    api->av_opt_get_int(ctx->codec_ctx, "b", 0, &ctx->bit_rate);

    // Prefer the audio stream's own duration; the container's may include
    // other streams.
    if (st->duration != FF_NOPTS_VALUE && st->duration > 0 && st->time_base.den > 0)
        ctx->duration_us = (int64_t)((double)st->duration * st->time_base.num * FF_TIME_BASE / st->time_base.den);
    else if (ctx->format_ctx->duration != FF_NOPTS_VALUE && ctx->format_ctx->duration > 0)
        ctx->duration_us = ctx->format_ctx->duration;

    if (ctx->bit_rate <= 0 && ctx->duration_us > 0)
    {
        UINT length = ctx->stream->GetLength(ctx->stream);
        if (length > 0)
            ctx->bit_rate = (int64_t)((double)length * 8.0 * FF_TIME_BASE / (double)ctx->duration_us);
    }

    ctx->frame = api->av_frame_alloc();
    ctx->packet = api->av_packet_alloc();
    if (!ctx->frame || !ctx->packet)
        goto fail;

    return TRUE;

fail:
    if (io_buffer)
        api->av_freep(&io_buffer);
    if (pb)
    {
        api->av_freep(&pb->buffer);
        api->avio_context_free(&pb);
    }
    ffmpeg_CloseFile(module);
    return FALSE;
}

static BOOL ffmpeg_GetPCMBlock(CPs_CoDecModule* module, void* block, DWORD* block_size)
{
    if (!module || !block || !block_size || !module->m_pModuleCookie)
        return FALSE;

    FFmpegContext* ctx = (FFmpegContext*)module->m_pModuleCookie;
    if (!ctx->codec_ctx)
    {
        *block_size = 0;
        return FALSE;
    }

    const size_t frame_bytes = FFMPEG_OUT_CHANNELS * sizeof(int16_t);
    const DWORD requested = *block_size;
    DWORD filled = 0;

    while (requested - filled >= frame_bytes)
    {
        if (ctx->pcm_pos < ctx->pcm_frames && ctx->current_sample < ctx->seek_target_sample)
        {
            // Pre-roll from an early seek point: discard up to the target
            uint64_t skip = ctx->seek_target_sample - ctx->current_sample;
            int frames = ctx->pcm_frames - ctx->pcm_pos;
            if ((uint64_t)frames > skip)
                frames = (int)skip;
            ctx->pcm_pos += frames;
            ctx->current_sample += (uint64_t)frames;
            continue;
        }

        if (ctx->pcm_pos < ctx->pcm_frames)
        {
            int frames = (int)((requested - filled) / frame_bytes);
            if (frames > ctx->pcm_frames - ctx->pcm_pos)
                frames = ctx->pcm_frames - ctx->pcm_pos;

            memcpy((char*)block + filled, ctx->pcm + (size_t)ctx->pcm_pos * FFMPEG_OUT_CHANNELS,
                   (size_t)frames * frame_bytes);
            filled += (DWORD)(frames * frame_bytes);
            ctx->pcm_pos += frames;
            ctx->current_sample += (uint64_t)frames;
            continue;
        }

        if (ctx->end_of_stream)
            break;

        if (!ffmpeg_decode_frame(ctx))
        {
            ctx->end_of_stream = true;
            break;
        }
        BOOL converted = ffmpeg_convert_frame(ctx);
        ctx->api.av_frame_unref(ctx->frame);
        if (!converted)
        {
            ctx->end_of_stream = true;
            break;
        }
    }

    *block_size = filled;
    return filled > 0 ? TRUE : FALSE;
}

static void ffmpeg_Seek(CPs_CoDecModule* module, const int numerator, const int denominator)
{
    if (!module || !module->m_pModuleCookie || denominator == 0) return;

    FFmpegContext* ctx = (FFmpegContext*)module->m_pModuleCookie;
    if (!ctx->codec_ctx || ctx->duration_us <= 0)
        return;

    int64_t target_us = (int64_t)((double)ctx->duration_us * numerator / denominator);
    int64_t ts = target_us;
    if (ctx->format_ctx->start_time != FF_NOPTS_VALUE)
        ts += ctx->format_ctx->start_time;

    int ret = ctx->api.av_seek_frame(ctx->format_ctx, -1, ts, FF_SEEK_FLAG_BACKWARD);
    if (ret < 0)
    {
        ffmpeg_log_error(ctx, ret, "seek failed");
        return;
    }

    ctx->api.avcodec_flush_buffers(ctx->codec_ctx);
    if (ctx->swr_ctx)
        ctx->api.swr_free(&ctx->swr_ctx);   // drop buffered pre-seek samples
    ctx->pcm_frames = ctx->pcm_pos = 0;
    ctx->draining = false;
    ctx->end_of_stream = false;
    ctx->current_sample = (uint64_t)((double)target_us * ctx->out_rate / FF_TIME_BASE);
    ctx->seek_target_sample = ctx->current_sample;
    ctx->seek_pts_pending = true;
}

static void ffmpeg_GetFileInfo(CPs_CoDecModule* module, CPs_FileInfo* info)
{
    if (!module || !info || !module->m_pModuleCookie) return;

    FFmpegContext* ctx = (FFmpegContext*)module->m_pModuleCookie;
    if (!ctx->codec_ctx) return;

    info->m_iFreq_Hz = (UINT)ctx->out_rate;
    info->m_bStereo = TRUE;
    info->m_b16bit = TRUE;
    info->m_iBitRate_Kbs = ctx->bit_rate > 0 ? (UINT)(ctx->bit_rate / 1000) : 0;
    // Unknown length (e.g. a live stream): report a day rather than 0, which
    // the engine treats as an instantly finished track.
    info->m_iFileLength_Secs = ctx->duration_us > 0 ? (UINT)(ctx->duration_us / FF_TIME_BASE) : 86400;
}

static int ffmpeg_GetCurrentPos_secs(CPs_CoDecModule* module)
{
    if (!module || !module->m_pModuleCookie) return 0;

    FFmpegContext* ctx = (FFmpegContext*)module->m_pModuleCookie;
    return ctx->out_rate > 0 ? (int)(ctx->current_sample / (uint64_t)ctx->out_rate) : 0;
}

////////////////////////////////////////////////////////////////////////////////
// Module Registration

void CP_InitialiseCodec_FFmpeg(CPs_CoDecModule* codec)
{
    if (!codec) return;
    memset(codec, 0, sizeof(*codec));

    FFmpegContext* ctx = (FFmpegContext*)calloc(1, sizeof(FFmpegContext));
    if (!ctx) return;

    if (!ffmpeg_load(&ctx->api))
    {
        free(ctx);
        return;   // codec stays zeroed: no associations, engine skips it
    }

    codec->Uninitialise       = ffmpeg_Uninitialise;
    codec->OpenFile           = ffmpeg_OpenFile;
    codec->CloseFile          = ffmpeg_CloseFile;
    codec->Seek               = ffmpeg_Seek;
    codec->GetFileInfo        = ffmpeg_GetFileInfo;
    codec->GetPCMBlock        = ffmpeg_GetPCMBlock;
    codec->GetCurrentPos_secs = ffmpeg_GetCurrentPos_secs;

    codec->m_pModuleCookie = ctx;

    CPFA_InitialiseFileAssociations(codec);

    // Native codecs sit earlier in the engine's codec order, so for formats
    // they handle (wav, mp3, ogg, flac, aac/m4a, opus) FFmpeg only acts as
    // the fallback when they fail to open a file.
    static const char* const extensions[] = {
        // MPEG audio
        "mp3", "mp2", "mp1", "mpa",
        // AAC / MPEG-4
        "aac", "m4a", "m4b", "m4p", "m4r", "mp4", "3gp", "3g2",
        // Lossless
        "flac", "fla", "ape", "wv", "tta", "tak", "alac",
        // Ogg family
        "ogg", "oga", "ogx", "ogm", "opus", "spx",
        // Windows Media
        "wma", "asf", "wmv",
        // Musepack
        "mpc", "mpp",
        // Dolby / DTS / surround
        "ac3", "eac3", "dts", "dtshd", "mlp", "thd",
        // PCM containers
        "wav", "wave", "aiff", "aif", "aifc", "au", "snd", "w64", "caf",
        // RealMedia
        "ra", "rm",
        // Tracker modules (only if the FFmpeg build includes libopenmpt)
        "mod", "s3m", "xm", "it",
        // Other
        "amr", "awb", "voc", "vqf", "dff", "dsf", "webm", "mka", "gsm", "iff", "svx",
    };
    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++)
        CPFA_AddFileAssociation(codec, extensions[i], 0);
}
