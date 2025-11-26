/*
 * C23 Features Header
 * Copyright (C) 2025 Zach Bacon
 *
 * This header provides C23 features and compatibility for the BriskPlayer project.
 * Assumes C23 compiler support.
 */

#ifndef C23_FEATURES_H
#define C23_FEATURES_H

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>   // For memset, memcpy
#include <stdbool.h>  // For bool, true, false

// Ensure Windows types are available for compatibility
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>  // For BOOL, DWORD, HWND types

// C23 enhanced static_assert (message is optional in C23)
#define STATIC_ASSERT_MSG(cond, msg) static_assert(cond, msg)
#define STATIC_ASSERT(cond) static_assert(cond)

// C23 enhanced attributes
#define NODISCARD [[nodiscard]]
#define DEPRECATED(msg) [[deprecated(msg)]]
#define MAYBE_UNUSED [[maybe_unused]]
#define NORETURN [[noreturn]]
#define FALLTHROUGH [[fallthrough]]
#define UNREACHABLE() [[noreturn]] __builtin_unreachable()

// C23 binary literals helper for bit patterns
#define BIT(n) (1u << (n))

// C23 enhanced enumerations
#define ENUM_TYPED(name, type) enum name : type

// Type-safe allocation macros using typeof
#define MALLOC_TYPE(type) ((type*)malloc(sizeof(type)))
#define CALLOC_TYPE(type, count) ((type*)calloc(count, sizeof(type)))
#define REALLOC_TYPE(ptr, type, count) ((type*)realloc(ptr, (count) * sizeof(type)))

// Safe memory allocation wrappers with error handling
// These check for allocation failures and log errors
static inline void* safe_malloc_impl(size_t size, const char* file, int line)
{
	if (size == 0)
		return NULL;
	
	void* ptr = malloc(size);
	if (!ptr)
	{
		// Critical allocation failure - log it
		char error_msg[256];
		sprintf_s(error_msg, sizeof(error_msg), 
		         "FATAL: malloc(%zu) failed at %s:%d\n", size, file, line);
		OutputDebugStringA(error_msg);
		fprintf(stderr, "%s", error_msg);
	}
	return ptr;
}

static inline void* safe_calloc_impl(size_t count, size_t size, const char* file, int line)
{
	if (count == 0 || size == 0)
		return NULL;
	
	// Check for multiplication overflow
	if (count > SIZE_MAX / size)
	{
		char error_msg[256];
		sprintf_s(error_msg, sizeof(error_msg),
		         "FATAL: calloc(%zu, %zu) would overflow at %s:%d\n", count, size, file, line);
		OutputDebugStringA(error_msg);
		fprintf(stderr, "%s", error_msg);
		return NULL;
	}
	
	void* ptr = calloc(count, size);
	if (!ptr)
	{
		char error_msg[256];
		sprintf_s(error_msg, sizeof(error_msg),
		         "FATAL: calloc(%zu, %zu) failed at %s:%d\n", count, size, file, line);
		OutputDebugStringA(error_msg);
		fprintf(stderr, "%s", error_msg);
	}
	return ptr;
}

static inline void* safe_realloc_impl(void* ptr, size_t size, const char* file, int line)
{
	if (size == 0)
	{
		free(ptr);
		return NULL;
	}
	
	void* new_ptr = realloc(ptr, size);
	if (!new_ptr)
	{
		char error_msg[256];
		sprintf_s(error_msg, sizeof(error_msg),
		         "FATAL: realloc(%p, %zu) failed at %s:%d\n", ptr, size, file, line);
		OutputDebugStringA(error_msg);
		fprintf(stderr, "%s", error_msg);
		// Original pointer is still valid on failure
	}
	return new_ptr;
}

// Safe allocation macros that track location
#define SAFE_MALLOC(size) safe_malloc_impl(size, __FILE__, __LINE__)
#define SAFE_CALLOC(count, size) safe_calloc_impl(count, size, __FILE__, __LINE__)
#define SAFE_REALLOC(ptr, size) safe_realloc_impl(ptr, size, __FILE__, __LINE__)

// Checked type-safe allocations
#define SAFE_MALLOC_TYPE(type) ((type*)SAFE_MALLOC(sizeof(type)))
#define SAFE_CALLOC_TYPE(type, count) ((type*)SAFE_CALLOC(count, sizeof(type)))

// Integer overflow checking helpers
static inline BOOL check_mul_overflow_u32(DWORD a, DWORD b, DWORD* result)
{
	if (a == 0 || b == 0)
	{
		*result = 0;
		return TRUE;
	}
	
	if (a > UINT_MAX / b)
		return FALSE; // Overflow would occur
	
	*result = a * b;
	return TRUE;
}

static inline BOOL check_add_overflow_u32(DWORD a, DWORD b, DWORD* result)
{
	if (a > UINT_MAX - b)
		return FALSE; // Overflow would occur
	
	*result = a + b;
	return TRUE;
}

// Type-safe macros using C23 typeof
#define TYPEOF_MAX(a, b) ({ \
    typeof(a) _a = (a); \
    typeof(b) _b = (b); \
    _a > _b ? _a : _b; \
})

#define TYPEOF_MIN(a, b) ({ \
    typeof(a) _a = (a); \
    typeof(b) _b = (b); \
    _a < _b ? _a : _b; \
})

#define TYPEOF_SWAP(a, b) do { \
    typeof(a) temp = (a); \
    (a) = (b); \
    (b) = temp; \
} while(0)

// Type-safe container_of macro
#define CONTAINER_OF(ptr, type, member) ({ \
    const typeof(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member)); \
})

// Utility macros for cleaner code
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Safe string operations
#define SAFE_FREE(ptr) do { if(ptr) { free(ptr); (ptr) = NULL; } } while(0)

// Windows INI file helper macro for writing integer values
// Creates temporary buffer and converts integer to string in one expression
#define INT_TO_INI_STRING(value) ({ \
    static char _ini_buf[32]; \
    snprintf(_ini_buf, sizeof(_ini_buf), "%d", (int)(value)); \
    _ini_buf; \
})

// Simpler version using compound literal (C99+ compatible)
#define WRITE_INT_TO_INI(section, key, value, filepath) \
    WritePrivateProfileString((section), (key), \
        (char[33]){snprintf((char[33]){}, 33, "%d", (int)(value))}, \
        (filepath))

// C23 decimal floating-point support for improved precision
// For audio calculations where precision matters
#if defined(__STDC_IEC_559_DFP__)
    typedef _Decimal64 audio_precision_t;
    #define AUDIO_DECIMAL(x) x##dd
    #define HAS_DECIMAL_FLOAT 1
#else
    // Fallback to double precision for older compilers
    typedef double audio_precision_t;
    #define AUDIO_DECIMAL(x) x
    #define HAS_DECIMAL_FLOAT 0
#endif

// C23 Generic selections for type-safe audio processing
#define audio_clamp(val, min, max) _Generic((val), \
    float: fmaxf(fminf(val, max), min), \
    double: fmax(fmin(val, max), min), \
    long double: fmaxl(fminl(val, max), min), \
    default: fmax(fmin(val, max), min) \
)

#define audio_abs(val) _Generic((val), \
    float: fabsf(val), \
    double: fabs(val), \
    long double: fabsl(val), \
    int: abs(val), \
    long: labs(val), \
    default: fabs(val) \
)

// Type-generic audio sample conversion
#define convert_sample(sample, from_bits, to_bits) _Generic((sample), \
    int8_t: ((sample) * ((1 << (to_bits)) - 1)) / ((1 << (from_bits)) - 1), \
    int16_t: ((sample) * ((1 << (to_bits)) - 1)) / ((1 << (from_bits)) - 1), \
    int32_t: ((sample) * ((1 << (to_bits)) - 1)) / ((1 << (from_bits)) - 1), \
    default: (sample) \
)

// C23 Variable Length Array (VLA) enhancements for dynamic audio processing
#if __STDC_VERSION__ >= 202311L
    // C23 VLA improvements - proper type checking and bounds
    #define VLA_BUFFER(type, name, size) \
        type name[size]; \
        static_assert(sizeof(type) > 0, "Invalid VLA element type"); \
        static_assert((size) > 0, "VLA size must be positive")
    
    // VLA with automatic cleanup (using C23 attributes)
    #define VLA_AUTO_BUFFER(type, name, size, cleanup_func) \
        type name[size] [[nodiscard]]; \
        typeof(cleanup_func) *_cleanup_##name [[cleanup(cleanup_func)]] = &cleanup_func
    
    // Audio buffer VLA with alignment
    #define AUDIO_VLA_BUFFER(type, name, frames, channels) \
        _Alignas(32) type name[frames * channels]; \
        static_assert(_Alignof(type) <= 32, "Type alignment too large for audio buffer")
    
    // Dynamic audio processing buffer with bounds checking
    #define DYNAMIC_AUDIO_BUFFER(name, sample_size, frame_count, channel_count) \
        do { \
            constexpr size_t total_samples = (frame_count) * (channel_count); \
            constexpr size_t buffer_bytes = total_samples * (sample_size); \
            \
            static_assert(buffer_bytes <= (1024 * 1024), "Audio buffer too large"); \
            static_assert((frame_count) > 0 && (channel_count) > 0, "Invalid audio dimensions"); \
            \
            char name[buffer_bytes] _Alignas(32); \
        } while(0)
    
    // VLA-based ring buffer for audio streaming
    #define RING_BUFFER_VLA(type, name, capacity) \
        struct { \
            type buffer[capacity]; \
            size_t head; \
            size_t tail; \
            size_t count; \
            constexpr size_t max_capacity = capacity; \
        } name = { .head = 0, .tail = 0, .count = 0 }
    
    // Automatic VLA sizing based on audio format
    #define AUTO_AUDIO_BUFFER(name, format, duration_ms, sample_rate) \
        do { \
            constexpr size_t bytes_per_sample = ((format) == 8) ? 1 : ((format) == 16) ? 2 : 4; \
            constexpr size_t samples_needed = ((sample_rate) * (duration_ms)) / 1000; \
            constexpr size_t buffer_size = samples_needed * bytes_per_sample; \
            \
            _Alignas(32) char name[buffer_size]; \
        } while(0)
    
    #define HAS_VLA_ENHANCEMENTS 1
#else
    // Fallback implementations for non-C23 compilers
    #define VLA_BUFFER(type, name, size) \
        type name[size]
    
    #define VLA_AUTO_BUFFER(type, name, size, cleanup_func) \
        type name[size]
    
    #define AUDIO_VLA_BUFFER(type, name, frames, channels) \
        type name[(frames) * (channels)]
    
    #define DYNAMIC_AUDIO_BUFFER(name, sample_size, frame_count, channel_count) \
        char name[(frame_count) * (channel_count) * (sample_size)]
    
    #define RING_BUFFER_VLA(type, name, capacity) \
        struct { \
            type buffer[capacity]; \
            size_t head; \
            size_t tail; \
            size_t count; \
        } name = { .head = 0, .tail = 0, .count = 0 }
    
    #define AUTO_AUDIO_BUFFER(name, format, duration_ms, sample_rate) \
        char name[((sample_rate) * (duration_ms) / 1000) * (((format) == 8) ? 1 : ((format) == 16) ? 2 : 4)]
    
    #define HAS_VLA_ENHANCEMENTS 0
#endif

// VLA utility functions for audio processing
static inline void vla_audio_zero(void* buffer, size_t samples, size_t sample_size)
{
    memset(buffer, 0, samples * sample_size);
}

static inline void vla_audio_copy(void* dest, const void* src, size_t samples, size_t sample_size)
{
    memcpy(dest, src, samples * sample_size);
}

// VLA-based audio mixing function
#define MIX_AUDIO_VLA(dest_type, dest, src1, src2, samples) \
    do { \
        VLA_BUFFER(dest_type, temp_mix, samples); \
        for (size_t i = 0; i < (samples); i++) { \
            temp_mix[i] = audio_clamp((src1)[i] + (src2)[i], \
                                     -(1 << (sizeof(dest_type) * 8 - 1)), \
                                     (1 << (sizeof(dest_type) * 8 - 1)) - 1); \
        } \
        vla_audio_copy(dest, temp_mix, samples, sizeof(dest_type)); \
    } while(0)

// C23 Functional Programming Patterns (lambda-like using function pointers)
// Audio callback function types for functional composition
typedef void (*audio_processor_fn)(void* samples, size_t count, void* user_data);
typedef BOOL (*audio_filter_fn)(const void* sample, void* user_data);  
typedef void (*audio_event_fn)(int event_type, void* event_data);

// Functional audio processing macros for lambda-like behavior
#define AUDIO_FOREACH(samples, count, processor, user_data) \
    do { \
        for (size_t _i = 0; _i < (count); _i++) { \
            (processor)(&(samples)[_i], _i, (user_data)); \
        } \
    } while(0)

#define AUDIO_FILTER(samples, count, filter, user_data, output) \
    do { \
        size_t _out_idx = 0; \
        for (size_t _i = 0; _i < (count); _i++) { \
            if ((filter)(&(samples)[_i], (user_data))) { \
                (output)[_out_idx++] = (samples)[_i]; \
            } \
        } \
    } while(0)

// Event-driven audio callback system
typedef struct {
    audio_event_fn callback;
    void* user_data;
    int priority;
} audio_event_handler_t;

// Functional composition helpers
#define COMPOSE_AUDIO_FN(fn1, fn2) \
    ({ \
        typeof(fn1) _f1 = (fn1); \
        typeof(fn2) _f2 = (fn2); \
        void _composed(void* data, size_t size, void* user) { \
            _f1(data, size, user); \
            _f2(data, size, user); \
        } \
        _composed; \
    })

// C23 Compound literals for audio configuration
#define AUDIO_CONFIG(freq, channels, bits) \
    (CPs_FileInfo){ \
        .m_iFreq_Hz = (freq), \
        .m_bStereo = ((channels) > 1), \
        .m_b16bit = ((bits) >= 16), \
        .m_iFileLength_Secs = 0, \
        .m_iBitRate_Kbs = 0 \
    }

#define WAVE_FORMAT(freq, channels, bits) \
    (FAudioWaveFormatEx){ \
        .wFormatTag = FAUDIO_FORMAT_PCM, \
        .nChannels = (channels), \
        .nSamplesPerSec = (freq), \
        .wBitsPerSample = (bits), \
        .nBlockAlign = ((channels) * (bits)) / 8, \
        .nAvgBytesPerSec = (freq) * (((channels) * (bits)) / 8), \
        .cbSize = 0 \
    }

// Audio event compound literals  
#define PLAYER_EVENT(state) \
    (struct { CPe_PlayerState state; }){ .state = (state) }

#define AUDIO_BUFFER_DESC(size, align) \
    (struct { size_t size; size_t alignment; }){ \
        .size = (size), \
        .alignment = (align) \
    }

// C23 Thread-local storage for audio contexts
// Per-thread audio processing state to reduce global dependencies
typedef struct {
    float volume_scale;
    int sample_rate;
    int channels;
    BOOL eq_enabled;
    float eq_bands[8];
} thread_audio_context_t;

// Thread-local audio context - each audio processing thread gets its own
extern _Thread_local thread_audio_context_t tl_audio_context;

// Macros for thread-local audio context access
#define TL_AUDIO_VOLUME() (tl_audio_context.volume_scale)
#define TL_AUDIO_SAMPLE_RATE() (tl_audio_context.sample_rate)
#define TL_AUDIO_CHANNELS() (tl_audio_context.channels)
#define TL_AUDIO_EQ_ENABLED() (tl_audio_context.eq_enabled)
#define TL_AUDIO_EQ_BAND(n) (tl_audio_context.eq_bands[(n)])

// Initialize thread-local audio context
#define INIT_TL_AUDIO_CONTEXT(vol, rate, ch) \
    do { \
        tl_audio_context = (thread_audio_context_t){ \
            .volume_scale = (vol), \
            .sample_rate = (rate), \
            .channels = (ch), \
            .eq_enabled = FALSE \
        }; \
    } while(0)

// C23 Anonymous structures and unions for cleaner audio format definitions
// Flexible audio sample format that can represent different sample types
typedef union {
    struct {  // Anonymous struct for individual channel access
        short left;
        short right;
    };
    struct {  // Anonymous struct for mono access
        int mono;
    };
    struct {  // Anonymous struct for byte access
        unsigned char bytes[4];
    };
    int raw;  // Raw 32-bit access
} audio_sample_t;
static_assert(sizeof(audio_sample_t) == 4, "Audio sample should be 4 bytes");

// Flexible audio format descriptor using anonymous unions
typedef struct {
    int sample_rate;
    int channels;
    
    union {  // Anonymous union for different bit depth representations
        struct {
            unsigned char bits_per_sample;
            unsigned char is_signed;
            unsigned char is_float;
        };
        unsigned int format_flags;
    };
    
    union {  // Anonymous union for different buffer size representations
        struct {
            size_t buffer_size_bytes;
            size_t buffer_size_samples;
        };
        struct {
            int buffer_ms;
            int buffers_count;
        };
    };
} flexible_audio_format_t;

// C23 Bit-precise integer types for exact audio sample width control
// These provide exact bit-width control for audio samples
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    // C23 bit-precise integer types for exact audio sample control
    typedef _BitInt(24) audio_sample_24bit_t;  // Exact 24-bit audio samples
    typedef _BitInt(20) audio_sample_20bit_t;  // Exact 20-bit audio samples  
    typedef _BitInt(18) audio_sample_18bit_t;  // Exact 18-bit audio samples
    
    // Bit-precise audio processing macros
    #define AUDIO_SAMPLE_24BIT_MAX ((audio_sample_24bit_t)((1 << 23) - 1))
    #define AUDIO_SAMPLE_24BIT_MIN ((audio_sample_24bit_t)(-(1 << 23)))
    
    #define CONVERT_TO_24BIT(sample, from_bits) \
        ((audio_sample_24bit_t)((sample) * AUDIO_SAMPLE_24BIT_MAX / ((1 << ((from_bits) - 1)) - 1)))
        
    #define HAS_BIT_PRECISE_TYPES 1
#else
    // Fallback for non-C23 compilers
    typedef int32_t audio_sample_24bit_t;
    typedef int32_t audio_sample_20bit_t;
    typedef int32_t audio_sample_18bit_t;
    
    #define AUDIO_SAMPLE_24BIT_MAX ((int32_t)((1 << 23) - 1))
    #define AUDIO_SAMPLE_24BIT_MIN ((int32_t)(-(1 << 23)))
    
    #define CONVERT_TO_24BIT(sample, from_bits) \
        ((int32_t)((sample) * AUDIO_SAMPLE_24BIT_MAX / ((1 << ((from_bits) - 1)) - 1)))
        
    #define HAS_BIT_PRECISE_TYPES 0
#endif

// Audio sample type selection macro using bit-precise types
#define AUDIO_SAMPLE_TYPE(bits) \
    _Generic((bits), \
        8: int8_t, \
        16: int16_t, \
        24: audio_sample_24bit_t, \
        32: int32_t, \
        default: int16_t \
    )

#endif // C23_FEATURES_H