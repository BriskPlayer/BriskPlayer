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

////////////////////////////////////////////////////////////////////////////////
//
// C23 Enhanced Circle Buffer
// Modern thread-safe circular buffer with atomic operations and standard threading
//
////////////////////////////////////////////////////////////////////////////////

// C23 threading and atomic includes with fallback support
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && defined(__STDC_NO_THREADS__) && __STDC_NO_THREADS__ == 0
    // Check if threads.h is actually available
    #if __has_include(<threads.h>)
        #include <threads.h>
        #include <stdatomic.h>
        #define HAVE_C23_THREADING 1
    #else
        // threads.h not available, fall back to Windows threading
        #define HAVE_C23_THREADING 0
    #endif
#else
    // Fallback to Windows threading for older compilers or when threads not supported
    #define HAVE_C23_THREADING 0
#endif

// Forward reference
struct _CPs_CircleBuffer;

////////////////////////////////////////////////////////////////////////////////
// Stream functions

typedef struct _CPs_CircleBuffer* CP_HCIRCLEBUFFER;
typedef void (*pfn_CircleBufferUninitialise)(CP_HCIRCLEBUFFER bBuffer);
//
typedef void (*pfn_CircleBufferWrite)(CP_HCIRCLEBUFFER bBuffer, const void* pSourceBuffer, const unsigned int iNumBytes);
typedef BOOL (*pfn_CircleBufferRead)(CP_HCIRCLEBUFFER bBuffer, void* pDestBuffer, const size_t iBytesToRead, size_t* pbBytesRead);
typedef unsigned int (*pfn_CircleGetUsedSpace)(CP_HCIRCLEBUFFER bBuffer);
typedef unsigned int (*pfn_CircleGetFreeSpace)(CP_HCIRCLEBUFFER bBuffer);
typedef void (*pfn_CircleFlush)(CP_HCIRCLEBUFFER bBuffer);
typedef void (*pfn_CircleSetComplete)(CP_HCIRCLEBUFFER bBuffer);
typedef BOOL (*pfn_CircleIsComplete)(CP_HCIRCLEBUFFER bBuffer);
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// C23 Enhanced Circle Buffer Structure

#if HAVE_C23_THREADING
// C23 version with modern threading primitives
typedef struct alignas(64) _CPs_CircleBuffer  // Cache line alignment
{
	// Public function interface (unchanged for compatibility)
	pfn_CircleBufferUninitialise Uninitialise;
	pfn_CircleBufferWrite Write;
	pfn_CircleBufferRead Read;
	pfn_CircleFlush Flush;
	pfn_CircleGetUsedSpace GetUsedSize;
	pfn_CircleGetFreeSpace GetFreeSize;
	pfn_CircleSetComplete SetComplete;
	pfn_CircleIsComplete IsComplete;
	
	// Enhanced buffer data with alignment
	alignas(16) BYTE* m_pBuffer;  // SIMD-aligned buffer pointer
	size_t m_iBufferSize;
	
	// Atomic cursors for lock-free operations where possible
	_Atomic size_t m_iReadCursor;
	_Atomic size_t m_iWriteCursor;
	_Atomic bool m_bComplete;
	
	// C23 standard threading primitives
	mtx_t m_access_mutex;          // Replaces CRITICAL_SECTION
	cnd_t m_data_available;        // Replaces event for data availability
	cnd_t m_space_available;       // New: notification when space becomes available
	
	// Performance monitoring (thread-safe counters)
	_Atomic uint64_t m_bytes_written;
	_Atomic uint64_t m_bytes_read;
	_Atomic uint32_t m_lock_contentions;
	_Atomic uint32_t m_wait_timeouts;
	
} CPs_CircleBuffer;

#else
// Legacy version for older compilers
typedef struct _CPs_CircleBuffer
{
	// Public functions (same interface)
	pfn_CircleBufferUninitialise Uninitialise;
	pfn_CircleBufferWrite Write;
	pfn_CircleBufferRead Read;
	pfn_CircleFlush Flush;
	pfn_CircleGetUsedSpace GetUsedSize;
	pfn_CircleGetFreeSpace GetFreeSize;
	pfn_CircleSetComplete SetComplete;
	pfn_CircleIsComplete IsComplete;
	
	// Legacy private variables (unchanged)
	BYTE* m_pBuffer;
	unsigned int m_iBufferSize;
	unsigned int m_iReadCursor;
	unsigned int m_iWriteCursor;
	HANDLE m_evtDataAvailable;
	CRITICAL_SECTION m_csCircleBuffer;
	BOOL m_bComplete;
	
} CPs_CircleBuffer;
#endif

//
////////////////////////////////////////////////////////////////////////////////


CPs_CircleBuffer* CP_CreateCircleBuffer(const unsigned int iBufferSize);

// C23 Enhanced Performance Monitoring Functions (only available with C23 support)
#if HAVE_C23_THREADING
void CP_GetCircleBufferStats(CPs_CircleBuffer* pCBuffer, 
                            uint64_t* bytes_written, 
                            uint64_t* bytes_read,
                            uint32_t* lock_contentions,
                            uint32_t* wait_timeouts);
void CP_ResetCircleBufferStats(CPs_CircleBuffer* pCBuffer);
unsigned int CP_GetCircleBufferFillPercentage(CPs_CircleBuffer* pCBuffer);
#endif
