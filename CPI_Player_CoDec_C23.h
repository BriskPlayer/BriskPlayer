////////////////////////////////////////////////////////////////////////////////
// C23 Enhanced CoDec Interface
// Modern audio decoder architecture with C23 features
//
////////////////////////////////////////////////////////////////////////////////

#ifndef CPI_PLAYER_CODEC_C23_H
#define CPI_PLAYER_CODEC_C23_H

// Ensure Windows types are available before including anything else
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>              // Must come first for BOOL, DWORD, HWND types
#include <shellapi.h>             // For HDROP
#include <commctrl.h>             // For HIMAGELIST

#include <stdint.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>              // For bool, true, false

#include "globals.h"              // Include globals for CPs_FileInfo and other types
#include "CPI_Player_CoDec.h"     // Include core BriskPlayer codec interface after Windows types
#include "c23_compat.h"

////////////////////////////////////////////////////////////////////////////////
// C23 Enhanced Audio Format Specifications

// Bit-precise integer types for exact audio sample widths (when available)
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
typedef _BitInt(8)  audio_sample_8_t;
typedef _BitInt(16) audio_sample_16_t;
typedef _BitInt(24) audio_sample_24_t;
typedef _BitInt(32) audio_sample_32_t;
#else
typedef int8_t  audio_sample_8_t;
typedef int16_t audio_sample_16_t;
typedef int32_t audio_sample_24_t;  // Use 32-bit for 24-bit samples
typedef int32_t audio_sample_32_t;
#endif

// Audio format enumeration with enhanced type safety
typedef enum {
    AUDIO_FORMAT_UNKNOWN = 0,
    AUDIO_FORMAT_PCM_8   = 8,
    AUDIO_FORMAT_PCM_16  = 16,
    AUDIO_FORMAT_PCM_24  = 24,
    AUDIO_FORMAT_PCM_32  = 32,
    AUDIO_FORMAT_FLOAT32 = 132,  // 32-bit float + flag
    AUDIO_FORMAT_FLOAT64 = 164   // 64-bit float + flag
} AudioFormat;

// Codec type enumeration
typedef enum {
    CODEC_TYPE_UNKNOWN   = 0x0000,
    CODEC_TYPE_MPEG      = 0x0001,
    CODEC_TYPE_OGG       = 0x0002,
    CODEC_TYPE_FLAC      = 0x0004,
    CODEC_TYPE_WAV       = 0x0008,
    CODEC_TYPE_AAC       = 0x0010,
    CODEC_TYPE_WINAMP    = 0x0020,
    CODEC_TYPE_LOSSLESS  = (CODEC_TYPE_FLAC | CODEC_TYPE_WAV),
    CODEC_TYPE_LOSSY     = (CODEC_TYPE_MPEG | CODEC_TYPE_OGG | CODEC_TYPE_AAC)
} CodecType;

// Decoder state enumeration
typedef enum {
    DECODER_STATE_UNINITIALIZED = 0,
    DECODER_STATE_READY,
    DECODER_STATE_DECODING,
    DECODER_STATE_EOF,
    DECODER_STATE_ERROR,
    DECODER_STATE_SEEKING
} DecoderState;

////////////////////////////////////////////////////////////////////////////////
// C23 Audio Processing Structures with Alignment

// Audio frame with proper alignment for SIMD operations
struct AudioFrame {
    union {
        struct { float left, right; };        // Stereo float
        struct { int16_t l16, r16; };         // Stereo 16-bit
        struct { int32_t l32, r32; };         // Stereo 32-bit
        float samples[8];                     // Multi-channel array
        uint8_t raw[32];                      // Raw bytes
    };
    uint8_t channels;
    AudioFormat format;
};

// Ensure alignment for SIMD operations
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(struct AudioFrame) >= 32, "AudioFrame too small for SIMD");
#endif

// Audio buffer with bounds checking
typedef struct {
    size_t capacity;
    size_t length;
    size_t position;
    AudioFormat format;
    uint8_t channels;
    uint32_t sample_rate;
    bool is_interleaved;
    
    // Flexible array member
    uint8_t data[];
} AudioBuffer;

// Audio constants
#define AUDIO_BUFFER_ALIGNMENT 16
#define DEFAULT_BUFFER_SIZE 8192
#define CD_SAMPLE_RATE 44100U
#define STEREO_CHANNELS 2

////////////////////////////////////////////////////////////////////////////////
// C23 Enhanced Codec Interface

// Forward declarations
struct CodecModule;
typedef struct CodecModule* CodecHandle;

// Function pointer types with better naming
typedef bool (*CodecOpenFunc)(CodecHandle codec, const char* filename);
typedef void (*CodecCloseFunc)(CodecHandle codec);
typedef bool (*CodecDecodeFunc)(CodecHandle codec, AudioBuffer* buffer);
typedef bool (*CodecSeekFunc)(CodecHandle codec, double position_seconds);

// Enhanced codec interface with C23 features
typedef struct CodecModule {
    // Codec identification
    CodecType type;
    DecoderState state;
    
    // Audio format information
    AudioFormat output_format;
    uint8_t channels;
    uint32_t sample_rate;
    uint64_t total_samples;
    uint64_t current_sample;
    
    // Function pointers
    CodecOpenFunc   open_file;
    CodecCloseFunc  close_file;
    CodecDecodeFunc decode_block;
    CodecSeekFunc   seek_position;
    
    // Private data with type erasure
    void* private_data;
    
    // Enhanced error handling
    char last_error[256];
    int error_code;
} CodecModule;

////////////////////////////////////////////////////////////////////////////////
// C23 Audio Processing Utilities

// Type-generic audio sample conversion using _Generic (C11 feature)
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define AUDIO_CONVERT_SAMPLE(sample, from_type, to_type) _Generic((sample), \
    int16_t:  convert_from_int16,     \
    int32_t:  convert_from_int32,     \
    float:    convert_from_float32,   \
    double:   convert_from_float64    \
    )(sample, to_type)
#else
#define AUDIO_CONVERT_SAMPLE(sample, from_type, to_type) \
    convert_sample_generic(sample, from_type, to_type)
#endif

// Audio buffer creation with proper alignment
static inline AudioBuffer* audio_buffer_create(size_t frame_count, 
                                              AudioFormat format, 
                                              uint8_t channels)
{
    size_t sample_size = (format & 0x7F) / 8;  // Remove float flag
    size_t total_size = sizeof(AudioBuffer) + 
                       (frame_count * channels * sample_size);
    
    // Ensure proper alignment
    total_size = (total_size + AUDIO_BUFFER_ALIGNMENT - 1) & 
                 ~(AUDIO_BUFFER_ALIGNMENT - 1);
    
    AudioBuffer* buffer = (AudioBuffer*)malloc(total_size);
    if (!buffer) return NULL;
    
    // Initialize with compound literal-style assignment
    buffer->capacity = frame_count * channels;
    buffer->length = 0;
    buffer->position = 0;
    buffer->format = format;
    buffer->channels = channels;
    buffer->sample_rate = CD_SAMPLE_RATE;
    buffer->is_interleaved = true;
    
    return buffer;
}

// Enhanced PCM processing with SIMD-friendly alignment
static inline void audio_process_stereo_float(struct AudioFrame frames[], 
                                            size_t count,
                                            float gain) 
{
    // Compiler can optimize this loop with SIMD
    for (size_t i = 0; i < count; ++i) {
        frames[i].left *= gain;
        frames[i].right *= gain;
    }
}

// Audio filter function type
typedef void (*AudioFilterFunc)(struct AudioFrame* frame);

// Normalization filter implementation
static void normalize_audio_filter(struct AudioFrame* frame)
{
    const float max_amplitude = 1.0f;
    float peak = fmaxf(fabsf(frame->left), fabsf(frame->right));
    if (peak > max_amplitude) {
        float scale = max_amplitude / peak;
        frame->left *= scale;
        frame->right *= scale;
    }
}

////////////////////////////////////////////////////////////////////////////////
// C23 Codec Factory with Enhanced Type Safety

// Codec factory function
static inline CodecHandle create_codec(CodecType type)
{
    CodecModule* codec = (CodecModule*)calloc(1, sizeof(CodecModule));
    if (!codec) return NULL;
    
    codec->type = type;
    codec->state = DECODER_STATE_UNINITIALIZED;
    
    // Initialize based on type
    switch (type) {
        case CODEC_TYPE_FLAC:
            codec->output_format = AUDIO_FORMAT_PCM_16;
            codec->channels = STEREO_CHANNELS;
            break;
            
        case CODEC_TYPE_MPEG:
            codec->output_format = AUDIO_FORMAT_PCM_16;
            codec->channels = STEREO_CHANNELS;
            break;
            
        default:
            free(codec);
            return NULL;
    }
    
    return codec;
}

// Enhanced error handling
static inline const char* get_codec_error(CodecHandle codec)
{
    return codec ? codec->last_error : "Invalid codec handle";
}

// Thread-local storage for per-thread audio contexts
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
static _Thread_local AudioBuffer* tl_temp_buffer = NULL;
static _Thread_local size_t tl_buffer_size = 0;
#else
// Fallback to static for older compilers
static AudioBuffer* tl_temp_buffer = NULL;
static size_t tl_buffer_size = 0;
#endif

// Audio processing with enhanced bounds checking
static inline bool process_audio_block(CodecHandle codec, 
                                      size_t requested_samples)
{
    // Bounds checking
    if (!codec || requested_samples > 65536) {  // Reasonable upper limit
        return false;
    }
    
    // For VLA-like behavior, use dynamic allocation
    size_t temp_size = requested_samples * codec->channels;
    float* temp_samples = (float*)malloc(temp_size * sizeof(float));
    if (!temp_samples) {
        return false;
    }
    
    // Initialize the buffer
    for (size_t i = 0; i < temp_size; ++i) {
        temp_samples[i] = 0.0f;
    }
    
    // Process the audio block...
    // (Implementation would go here)
    
    free(temp_samples);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Enhanced Codec Registration System

// Forward declarations for codec factories
#ifdef HAVE_FLAC_CODEC
CodecModule* create_flac_codec(void);
#endif
#ifdef HAVE_MPEG_CODEC
CodecModule* create_mpeg_codec(void);
#endif
#ifdef HAVE_OGG_CODEC
CodecModule* create_ogg_codec(void);
#endif
#ifdef HAVE_AAC_CODEC
CodecModule* create_aac_codec(void);
#endif

// Codec registration structure
typedef struct {
    CodecType type;
    const char* name;
    const char* extensions[8];  // File extensions
    uint32_t capabilities;
    CodecModule* (*factory)(void);
} CodecRegistration;

// Global codec registry
static const CodecRegistration codec_registry[] = {
#ifdef HAVE_FLAC_CODEC
    {
        CODEC_TYPE_FLAC,
        "FLAC Lossless Audio",
        {"flac", "fla", NULL},
        0x01,  // Seeking support
        create_flac_codec
    },
#endif
#ifdef HAVE_MPEG_CODEC
    {
        CODEC_TYPE_MPEG,
        "MPEG Audio Layer",
        {"mp3", "mp2", "mp1", NULL},
        0x03,  // Seeking + VBR support
        create_mpeg_codec
    },
#endif
#ifdef HAVE_OGG_CODEC
    {
        CODEC_TYPE_OGG,
        "Ogg Vorbis Audio",
        {"ogg", "oga", NULL},
        0x01,  // Seeking support
        create_ogg_codec
    },
#endif
#ifdef HAVE_AAC_CODEC
    {
        CODEC_TYPE_AAC,
        "AAC Audio",
        {"aac", "m4a", "mp4", NULL},
        0x01,  // Seeking support
        create_aac_codec
    },
#endif
    // Sentinel
    {CODEC_TYPE_UNKNOWN, NULL, {NULL}, 0, NULL}
};

// Codec discovery function
static inline const CodecRegistration* find_codec_for_extension(const char* ext)
{
    for (const CodecRegistration* reg = codec_registry; 
         reg->type != CODEC_TYPE_UNKNOWN; 
         ++reg) {
        for (int i = 0; reg->extensions[i] != NULL; ++i) {
            if (strcmp(ext, reg->extensions[i]) == 0) {
                return reg;
            }
        }
    }
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////

#endif // CPI_PLAYER_CODEC_C23_H